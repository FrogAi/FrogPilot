#include "frogpilot/ui/screenrecorder/screen_encoder.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include "common/swaglog.h"
#include "common/timing.h"
#include "common/util.h"

#include "third_party/libyuv/include/libyuv.h"
#include "third_party/linux/include/msm_media_info.h"

#include "third_party/linux/include/v4l2-controls.h"
#include <linux/videodev2.h>

#define V4L2_QCOM_BUF_FLAG_CODECCONFIG 0x00020000
#define V4L2_QCOM_BUF_FLAG_EOS         0x02000000
#define V4L2_BUF_FLAG_KEYFRAME         0x00000008

namespace {

const char *ENCODER_DEV = "/dev/v4l/by-path/platform-aa00000.qcom_vidc-video-index1";

constexpr int MUX_TIMEBASE = 90000;

constexpr uint64_t DRAIN_TIMEOUT_NS = 3ULL * 1000000000ULL;

constexpr uint64_t DRAIN_INPUT_TIMEOUT_NS = 1ULL * 1000000000ULL;

constexpr int ENQUEUE_TIMEOUT_MS = 250;

unsigned int request_buffers(int fd, v4l2_buf_type buf_type, unsigned int count) {
  v4l2_requestbuffers reqbuf = {
    .count = count,
    .type = buf_type,
    .memory = V4L2_MEMORY_USERPTR,
  };
  if (util::safe_ioctl(fd, VIDIOC_REQBUFS, &reqbuf) != 0) {
    return 0;
  }
  return reqbuf.count;
}

bool queue_buffer(int fd, v4l2_buf_type buf_type, unsigned int index, VisionBuf *buf, struct timeval timestamp = {}) {
  v4l2_plane plane = {
    .bytesused = (uint32_t)buf->len,
    .length = (unsigned int)buf->len,
    .m = { .userptr = (unsigned long)buf->addr },
    .reserved = {(unsigned int)buf->fd},
  };
  v4l2_buffer v4l_buf = {
    .index = index,
    .type = buf_type,
    .flags = V4L2_BUF_FLAG_TIMESTAMP_COPY,
    .timestamp = timestamp,
    .memory = V4L2_MEMORY_USERPTR,
    .m = { .planes = &plane },
    .length = 1,
  };
  return util::safe_ioctl(fd, VIDIOC_QBUF, &v4l_buf) == 0;
}

bool dequeue_buffer(int fd, v4l2_buf_type buf_type, unsigned int *index = nullptr,
                    unsigned int *bytesused = nullptr, unsigned int *flags = nullptr,
                    struct timeval *timestamp = nullptr, unsigned int *data_offset = nullptr) {
  v4l2_plane plane = {0};
  v4l2_buffer v4l_buf = {
    .type = buf_type,
    .memory = V4L2_MEMORY_USERPTR,
    .m = { .planes = &plane },
    .length = 1,
  };
  if (util::safe_ioctl(fd, VIDIOC_DQBUF, &v4l_buf) != 0) {
    return false;
  }

  if (index) {
    *index = v4l_buf.index;
  }

  if (bytesused) {
    *bytesused = plane.bytesused;
  }

  if (flags) {
    *flags = v4l_buf.flags;
  }

  if (timestamp) {
    *timestamp = v4l_buf.timestamp;
  }

  if (data_offset) {
    *data_offset = plane.data_offset;
  }

  return true;
}

}

ScreenEncoder::ScreenEncoder(int width, int height, int fps, int bitrate) : bitrate(bitrate), fps(fps), height(height), width(width) {}

ScreenEncoder::~ScreenEncoder() {
  close();
}

bool ScreenEncoder::open(const std::string &path) {
  if (is_open) {
    return true;
  }

  base_set = false;
  header_written = false;
  warned_no_header = false;
  stream_error = false;

  base_ts_us = 0;
  last_dts = -1;
  drain_deadline_ns = 0;

  if (!setup_muxer(path)) {
    teardown_muxer();
    return false;
  }

  if (!setup_device()) {
    teardown_device();
    teardown_muxer();
    return false;
  }

  is_open = true;

  dequeue_thread = std::thread(&ScreenEncoder::dequeue_loop, this);

  return true;
}

void ScreenEncoder::close() {
  if (!is_open) {
    return;
  }

  is_open = false;

  if (fd >= 0 && !stream_error) {
    uint64_t drain_budget_end = nanos_since_boot() + DRAIN_INPUT_TIMEOUT_NS;
    for (int i = 0; i < BUF_IN_COUNT; i++) {
      int remaining_ms = (int)(((int64_t)drain_budget_end - (int64_t)nanos_since_boot()) / 1000000);
      unsigned int idx;
      if (remaining_ms <= 0 || !free_buf_in.try_pop(idx, remaining_ms)) {
        LOGW("screenrecorder: encoder did not fully drain before stop");
        break;
      }
    }

    v4l2_encoder_cmd cmd = { .cmd = V4L2_ENC_CMD_STOP };
    util::safe_ioctl(fd, VIDIOC_ENCODER_CMD, &cmd);
  }

  drain_deadline_ns = nanos_since_boot() + DRAIN_TIMEOUT_NS;

  if (dequeue_thread.joinable()) {
    dequeue_thread.join();
  }

  teardown_device();
  teardown_muxer();
}

bool ScreenEncoder::setup_device() {
  fd = ::open(ENCODER_DEV, O_RDWR | O_NONBLOCK);
  if (fd < 0) {
    LOGE("screenrecorder: cannot open encoder %s (errno %d)", ENCODER_DEV, errno);
    return false;
  }

  v4l2_capability cap = {};
  if (util::safe_ioctl(fd, VIDIOC_QUERYCAP, &cap) != 0) {
    LOGE("screenrecorder: VIDIOC_QUERYCAP failed");
    return false;
  }

  if (strcmp((const char *)cap.card, "msm_vidc_venc") != 0) {
    LOGE("screenrecorder: unexpected encoder card '%s'", cap.card);
    return false;
  }

  v4l2_format fmt_out = {
    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
    .fmt = { .pix_mp = {
      .width = (unsigned int)width,
      .height = (unsigned int)height,
      .pixelformat = V4L2_PIX_FMT_H264,
      .field = V4L2_FIELD_ANY,
      .colorspace = V4L2_COLORSPACE_DEFAULT,
    }},
  };
  if (util::safe_ioctl(fd, VIDIOC_S_FMT, &fmt_out) != 0) {
    LOGE("screenrecorder: S_FMT (output/H264) failed");
    return false;
  }

  int out_buf_size = fmt_out.fmt.pix_mp.plane_fmt[0].sizeimage;

  v4l2_streamparm streamparm = {
    .type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
    .parm = { .output = { .timeperframe = { .numerator = 1, .denominator = (unsigned int)fps } } },
  };
  util::safe_ioctl(fd, VIDIOC_S_PARM, &streamparm);

  v4l2_format fmt_in = {
    .type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
    .fmt = { .pix_mp = {
      .width = (unsigned int)width,
      .height = (unsigned int)height,
      .pixelformat = V4L2_PIX_FMT_NV12,
      .field = V4L2_FIELD_ANY,
      .colorspace = V4L2_COLORSPACE_470_SYSTEM_BG,
    }},
  };
  if (util::safe_ioctl(fd, VIDIOC_S_FMT, &fmt_in) != 0) {
    LOGE("screenrecorder: S_FMT (input/NV12) failed");
    return false;
  }

  in_buf_size = std::max<int>(fmt_in.fmt.pix_mp.plane_fmt[0].sizeimage,
                              VENUS_BUFFER_SIZE(COLOR_FMT_NV12, width, height));

  v4l2_control shared_ctrls[] = {
    { .id = V4L2_CID_MPEG_VIDEO_HEADER_MODE, .value = V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE },
    { .id = V4L2_CID_MPEG_VIDEO_BITRATE, .value = bitrate },
    { .id = V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL, .value = V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_VBR_CFR },
    { .id = V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY, .value = V4L2_MPEG_VIDC_VIDEO_PRIORITY_REALTIME_DISABLE },
    { .id = V4L2_CID_MPEG_VIDC_VIDEO_IDR_PERIOD, .value = 1 },
  };
  for (auto &c : shared_ctrls) {
    util::safe_ioctl(fd, VIDIOC_S_CTRL, &c);
  }

  v4l2_control h264_ctrls[] = {
    { .id = V4L2_CID_MPEG_VIDEO_H264_PROFILE, .value = V4L2_MPEG_VIDEO_H264_PROFILE_HIGH },
    { .id = V4L2_CID_MPEG_VIDEO_H264_LEVEL, .value = V4L2_MPEG_VIDEO_H264_LEVEL_UNKNOWN },
    { .id = V4L2_CID_MPEG_VIDC_VIDEO_NUM_P_FRAMES, .value = fps * 2 - 1 },
    { .id = V4L2_CID_MPEG_VIDC_VIDEO_NUM_B_FRAMES, .value = 0 },
    { .id = V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE, .value = V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC },
    { .id = V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL, .value = V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL_0 },
    { .id = V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE, .value = 0 },
    { .id = V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA, .value = 0 },
    { .id = V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA, .value = 0 },
    { .id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE, .value = 0 },
  };
  for (auto &c : h264_ctrls) {
    util::safe_ioctl(fd, VIDIOC_S_CTRL, &c);
  }

  unsigned int n_capture = request_buffers(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, BUF_OUT_COUNT);
  unsigned int n_output = request_buffers(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, BUF_IN_COUNT);
  if (n_capture < BUF_OUT_COUNT || n_output < BUF_IN_COUNT) {
    LOGE("screenrecorder: REQBUFS granted too few buffers (capture %u/%d, output %u/%d)",
         n_capture, BUF_OUT_COUNT, n_output, BUF_IN_COUNT);
    return false;
  }

  v4l2_buf_type bt = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  if (util::safe_ioctl(fd, VIDIOC_STREAMON, &bt) != 0) {
    LOGE("screenrecorder: STREAMON (capture) failed");
    return false;
  }

  bt = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  if (util::safe_ioctl(fd, VIDIOC_STREAMON, &bt) != 0) {
    LOGE("screenrecorder: STREAMON (output) failed");
    return false;
  }

  for (int i = 0; i < BUF_OUT_COUNT; i++) {
    buf_out[i].allocate(out_buf_size);
    if (!queue_buffer(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, i, &buf_out[i])) {
      LOGE("screenrecorder: failed to queue initial capture buffer %d", i);
      return false;
    }
  }

  for (int i = 0; i < BUF_IN_COUNT; i++) {
    buf_in[i].allocate(in_buf_size);
    free_buf_in.push(i);
  }

  y_scanlines = VENUS_Y_SCANLINES(COLOR_FMT_NV12, height);
  y_stride = VENUS_Y_STRIDE(COLOR_FMT_NV12, width);
  uv_stride = VENUS_UV_STRIDE(COLOR_FMT_NV12, width);

  return true;
}

void ScreenEncoder::teardown_device() {
  if (fd >= 0) {
    v4l2_buf_type bt = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    util::safe_ioctl(fd, VIDIOC_STREAMOFF, &bt);
    request_buffers(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, 0);
    bt = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    util::safe_ioctl(fd, VIDIOC_STREAMOFF, &bt);
    request_buffers(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, 0);
    ::close(fd);
    fd = -1;
  }

  for (int i = 0; i < BUF_OUT_COUNT; i++) {
    if (buf_out[i].addr) {
      buf_out[i].free();
    }
  }

  for (int i = 0; i < BUF_IN_COUNT; i++) {
    if (buf_in[i].addr) {
      buf_in[i].free();
    }
  }

  unsigned int idx;
  while (free_buf_in.try_pop(idx, 0)) {}
}

void ScreenEncoder::encode_frame(const uint8_t *rgb32, int stride, uint64_t ts_ns) {
  if (!is_open || stream_error) {
    return;
  }

  unsigned int idx;
  if (!free_buf_in.try_pop(idx, ENQUEUE_TIMEOUT_MS)) {
    static uint64_t last_drop_warn_ns = 0;
    uint64_t now = nanos_since_boot();
    if (now - last_drop_warn_ns > 1000000000ULL) {
      LOGW("screenrecorder: encoder backpressure, dropping frame");
      last_drop_warn_ns = now;
    }
    return;
  }

  VisionBuf *buf = &buf_in[idx];
  uint8_t *dst_y = (uint8_t *)buf->addr;
  uint8_t *dst_uv = dst_y + (size_t)y_stride * y_scanlines;
  libyuv::ARGBToNV12(rgb32, stride, dst_y, y_stride, dst_uv, uv_stride, width, height);
  buf->sync(VISIONBUF_SYNC_TO_DEVICE);

  struct timeval ts = {
    .tv_sec = (long)(ts_ns / 1000000000),
    .tv_usec = (long)((ts_ns / 1000) % 1000000),
  };
  if (!queue_buffer(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, idx, buf, ts)) {
    LOGE("screenrecorder: failed to queue input frame");
    free_buf_in.push(idx);
    stream_error = true;
  }
}

void ScreenEncoder::dequeue_loop() {
  util::set_thread_name("sr-encoder");

  struct pollfd pfd = { .fd = fd, .events = POLLIN | POLLOUT };
  bool exit = false;

  while (!exit) {
    if (stream_error) {
      break;
    }

    uint64_t deadline = drain_deadline_ns.load();
    if (deadline != 0 && nanos_since_boot() > deadline) {
      LOGE("screenrecorder: timed out waiting for encoder EOS, forcing teardown");
      stream_error = true;
      break;
    }

    int rc = poll(&pfd, 1, 1000);
    if (rc < 0) {
      if (errno == EINTR) {
        continue;
      }

      LOGE("screenrecorder: encoder poll failed (%d)", errno);
      stream_error = true;
      break;
    } else if (rc == 0) {
      continue;
    }

    if (pfd.revents & POLLIN) {
      unsigned int index = 0, bytesused = 0, flags = 0, data_offset = 0;
      struct timeval ts = {};
      if (!dequeue_buffer(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, &index, &bytesused, &flags, &ts, &data_offset)) {
        LOGE("screenrecorder: capture DQBUF failed");
        stream_error = true;
        break;
      }

      if (index >= BUF_OUT_COUNT) {
        LOGE("screenrecorder: capture DQBUF returned out-of-range index %u", index);
        stream_error = true;
        break;
      }

      buf_out[index].sync(VISIONBUF_SYNC_FROM_DEVICE);
      uint8_t *data = (uint8_t *)buf_out[index].addr + data_offset;

      if (flags & V4L2_QCOM_BUF_FLAG_EOS) {
        exit = true;
      } else if (flags & V4L2_QCOM_BUF_FLAG_CODECCONFIG) {
        mux_write_header(data, bytesused);
      } else if (bytesused > 0) {
        uint64_t ts_us = (uint64_t)ts.tv_sec * 1000000 + ts.tv_usec;
        mux_write_packet(data, bytesused, ts_us, flags & V4L2_BUF_FLAG_KEYFRAME);
      }

      if (!exit && !queue_buffer(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, index, &buf_out[index])) {
        LOGE("screenrecorder: failed to requeue capture buffer %u", index);
        stream_error = true;
        break;
      }
    }

    if (pfd.revents & POLLOUT) {
      unsigned int index = 0;
      if (dequeue_buffer(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, &index) && index < BUF_IN_COUNT) {
        free_buf_in.push(index);
      }
    }
  }
}

bool ScreenEncoder::setup_muxer(const std::string &path) {
  vid_path = path;
  lock_path = path + ".lock";

  int lock_fd = ::open(lock_path.c_str(), O_RDWR | O_CREAT, 0664);
  if (lock_fd >= 0) {
    ::close(lock_fd);
  }

  avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, vid_path.c_str());
  if (!ofmt_ctx) {
    LOGE("screenrecorder: avformat_alloc_output_context2 failed for %s", vid_path.c_str());
    return false;
  }

  const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
  if (!codec) {
    LOGE("screenrecorder: H264 codec not found for muxing");
    return false;
  }

  codec_ctx = avcodec_alloc_context3(codec);
  if (!codec_ctx) {
    return false;
  }

  codec_ctx->width = width;
  codec_ctx->height = height;
  codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  codec_ctx->time_base = (AVRational){ 1, MUX_TIMEBASE };

  out_stream = avformat_new_stream(ofmt_ctx, NULL);
  if (!out_stream) {
    return false;
  }

  out_stream->time_base = codec_ctx->time_base;

  if (avio_open(&ofmt_ctx->pb, vid_path.c_str(), AVIO_FLAG_WRITE) < 0) {
    LOGE("screenrecorder: avio_open failed for %s", vid_path.c_str());
    return false;
  }

  return true;
}

void ScreenEncoder::teardown_muxer() {
  if (ofmt_ctx) {
    if (header_written) {
      av_write_trailer(ofmt_ctx);
    }

    if (ofmt_ctx->pb) {
      avio_closep(&ofmt_ctx->pb);
    }

    avformat_free_context(ofmt_ctx);
    ofmt_ctx = nullptr;
  }

  if (codec_ctx) {
    avcodec_free_context(&codec_ctx);
  }

  out_stream = nullptr;

  if (!header_written && !vid_path.empty()) {
    unlink(vid_path.c_str());
  }

  if (!lock_path.empty()) {
    unlink(lock_path.c_str());
    lock_path.clear();
  }

  vid_path.clear();
  base_set = false;
  base_ts_us = 0;
  header_written = false;
}

void ScreenEncoder::mux_write_header(const uint8_t *data, int len) {
  if (header_written || !ofmt_ctx) {
    return;
  }

  if (len > 0) {
    codec_ctx->extradata = (uint8_t *)av_mallocz(len + AV_INPUT_BUFFER_PADDING_SIZE);
    codec_ctx->extradata_size = len;
    memcpy(codec_ctx->extradata, data, len);
  }

  if (avcodec_parameters_from_context(out_stream->codecpar, codec_ctx) < 0) {
    LOGE("screenrecorder: avcodec_parameters_from_context failed");
    stream_error = true;
    return;
  }

  AVDictionary *opts = nullptr;
  av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov+default_base_moof", 0);
  int rc = avformat_write_header(ofmt_ctx, &opts);
  av_dict_free(&opts);
  if (rc < 0) {
    LOGE("screenrecorder: avformat_write_header failed");
    stream_error = true;
    return;
  }

  header_written = true;
}

void ScreenEncoder::mux_write_packet(const uint8_t *data, int len, uint64_t ts_us, bool keyframe) {
  if (!header_written) {
    if (!warned_no_header) {
      LOGE("screenrecorder: dropping encoded frame, no codec header written yet");
      warned_no_header = true;
    }
    return;
  }

  if (!base_set) {
    base_ts_us = ts_us;
    base_set = true;
  }

  int64_t rel = (int64_t)ts_us - (int64_t)base_ts_us;
  if (rel < 0) {
    rel = 0;
  }

  AVRational in_tb = { 1, 1000000 };
  AVPacket *pkt = av_packet_alloc();
  if (!pkt) {
    LOGE("screenrecorder: av_packet_alloc failed");
    return;
  }
  pkt->data = (uint8_t *)data;
  pkt->size = len;
  pkt->stream_index = out_stream->index;

  enum AVRounding rnd = (enum AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
  int64_t dts = av_rescale_q_rnd(rel, in_tb, out_stream->time_base, rnd);
  if (dts <= last_dts) {
    dts = last_dts + 1;
  }
  last_dts = dts;
  pkt->pts = pkt->dts = dts;
  pkt->duration = av_rescale_q(1000000 / fps, in_tb, out_stream->time_base);
  if (keyframe) {
    pkt->flags |= AV_PKT_FLAG_KEY;
  }

  if (av_interleaved_write_frame(ofmt_ctx, pkt) < 0) {
    LOGE("screenrecorder: av_interleaved_write_frame failed (len %d), stopping", len);
    stream_error = true;
  }

  av_packet_free(&pkt);
}
