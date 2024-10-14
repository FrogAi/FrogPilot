#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#include "selfdrive/frogpilot/screenrecorder/omx_encoder.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cstdlib>
#include <cstdio>

#include <OMX_Component.h>
#include <OMX_IndexExt.h>
#include <OMX_QCOMExtns.h>
#include <OMX_VideoExt.h>
#include "libyuv.h"
#include "msm_media_info.h"
#include "common/swaglog.h"
#include "common/util.h"

ExitHandler do_exit;

using namespace libyuv;

LIBYUV_API
int ABGRToNV12(const uint8_t* src_abgr,
               int src_stride_abgr,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_uv,
               int dst_stride_uv,
               int width,
               int height) {
  int y;
  int halfwidth = (width + 1) >> 1;

  void (*ABGRToUVRow)(const uint8_t* src_abgr0, int src_stride_abgr,
                      uint8_t* dst_u, uint8_t* dst_v, int width) = ABGRToUVRow_NEON;
  void (*ABGRToYRow)(const uint8_t* src_abgr, uint8_t* dst_y, int width) = ABGRToYRow_NEON;
  void (*MergeUVRow_)(const uint8_t* src_u, const uint8_t* src_v,
                      uint8_t* dst_uv, int width) = MergeUVRow_NEON;

  if (!src_abgr || !dst_y || !dst_uv || width <= 0 || height == 0) {
    return -1;
  }

  if (height < 0) {
    height = -height;
    src_abgr = src_abgr + (height - 1) * src_stride_abgr;
    src_stride_abgr = -src_stride_abgr;
  }

  align_buffer_64(row_u, ((halfwidth + 31) & ~31) * 2);
  uint8_t* row_v = row_u + ((halfwidth + 31) & ~31);

  for (y = 0; y < height - 1; y += 2) {
    ABGRToUVRow(src_abgr, src_stride_abgr, row_u, row_v, width);
    MergeUVRow_(row_u, row_v, dst_uv, halfwidth);
    ABGRToYRow(src_abgr, dst_y, width);
    ABGRToYRow(src_abgr + src_stride_abgr, dst_y + dst_stride_y, width);
    src_abgr += src_stride_abgr * 2;
    dst_y += dst_stride_y * 2;
    dst_uv += dst_stride_uv;
  }

  if (height & 1) {
    ABGRToUVRow(src_abgr, 0, row_u, row_v, width);
    MergeUVRow_(row_u, row_v, dst_uv, halfwidth);
    ABGRToYRow(src_abgr, dst_y, width);
  }

  free_aligned_buffer_64(row_u);
  return 0;
}

// Check the OMX error code and assert if an error occurred.
#define OMX_CHECK(_expr)              \
  do {                                \
    assert(OMX_ErrorNone == (_expr)); \
  } while (0)

// ***** OMX callback functions *****

void OmxEncoder::wait_for_state(OMX_STATETYPE state_) {
  std::unique_lock lk(state_lock);
  while (state != state_) {
    state_cv.wait(lk);
  }
}

static OMX_CALLBACKTYPE omx_callbacks = {
  .EventHandler = OmxEncoder::event_handler,
  .EmptyBufferDone = OmxEncoder::empty_buffer_done,
  .FillBufferDone = OmxEncoder::fill_buffer_done,
};

OMX_ERRORTYPE OmxEncoder::event_handler(OMX_HANDLETYPE component, OMX_PTR app_data, OMX_EVENTTYPE event,
                                   OMX_U32 data1, OMX_U32 data2, OMX_PTR event_data) {
  OmxEncoder *e = (OmxEncoder*)app_data;
  if (event == OMX_EventCmdComplete) {
    assert(data1 == OMX_CommandStateSet);
    LOG("set state event 0x%x", data2);
    {
      std::unique_lock lk(e->state_lock);
      e->state = (OMX_STATETYPE)data2;
    }
    e->state_cv.notify_all();
  } else if (event == OMX_EventError) {
    LOGE("OMX error 0x%08x", data1);
  } else {
    LOGE("OMX unhandled event %d", event);
    assert(false);
  }

  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxEncoder::empty_buffer_done(OMX_HANDLETYPE component, OMX_PTR app_data,
                                                   OMX_BUFFERHEADERTYPE *buffer) {
  OmxEncoder *e = (OmxEncoder*)app_data;
  e->free_in.push(buffer);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxEncoder::fill_buffer_done(OMX_HANDLETYPE component, OMX_PTR app_data,
                                                  OMX_BUFFERHEADERTYPE *buffer) {
  OmxEncoder *e = (OmxEncoder*)app_data;
  e->done_out.push(buffer);
  return OMX_ErrorNone;
}

#define PORT_INDEX_IN 0
#define PORT_INDEX_OUT 1

static const char* omx_color_fomat_name(uint32_t format) __attribute__((unused));
static const char* omx_color_fomat_name(uint32_t format) {
  switch (format) {
  case OMX_COLOR_FormatUnused: return "OMX_COLOR_FormatUnused";
  case OMX_COLOR_FormatMonochrome: return "OMX_COLOR_FormatMonochrome";
  case OMX_COLOR_Format8bitRGB332: return "OMX_COLOR_Format8bitRGB332";
  case OMX_COLOR_Format12bitRGB444: return "OMX_COLOR_Format12bitRGB444";
  case OMX_COLOR_Format16bitARGB4444: return "OMX_COLOR_Format16bitARGB4444";
  case OMX_COLOR_Format16bitARGB1555: return "OMX_COLOR_Format16bitARGB1555";
  case OMX_COLOR_Format16bitRGB565: return "OMX_COLOR_Format16bitRGB565";
  case OMX_COLOR_Format16bitBGR565: return "OMX_COLOR_Format16bitBGR565";
  case OMX_COLOR_Format18bitRGB666: return "OMX_COLOR_Format18bitRGB666";
  case OMX_COLOR_Format18bitARGB1665: return "OMX_COLOR_Format18bitARGB1665";
  case OMX_COLOR_Format19bitARGB1666: return "OMX_COLOR_Format19bitARGB1666";
  case OMX_COLOR_Format24bitRGB888: return "OMX_COLOR_Format24bitRGB888";
  case OMX_COLOR_Format24bitBGR888: return "OMX_COLOR_Format24bitBGR888";
  case OMX_COLOR_Format24bitARGB1887: return "OMX_COLOR_Format24bitARGB1887";
  case OMX_COLOR_Format25bitARGB1888: return "OMX_COLOR_Format25bitARGB1888";
  case OMX_COLOR_Format32bitBGRA8888: return "OMX_COLOR_Format32bitBGRA8888";
  case OMX_COLOR_Format32bitARGB8888: return "OMX_COLOR_Format32bitARGB8888";
  case OMX_COLOR_FormatYUV411Planar: return "OMX_COLOR_FormatYUV411Planar";
  case OMX_COLOR_FormatYUV411PackedPlanar: return "OMX_COLOR_FormatYUV411PackedPlanar";
  case OMX_COLOR_FormatYUV420Planar: return "OMX_COLOR_FormatYUV420Planar";
  case OMX_COLOR_FormatYUV420PackedPlanar: return "OMX_COLOR_FormatYUV420PackedPlanar";
  case OMX_COLOR_FormatYUV420SemiPlanar: return "OMX_COLOR_FormatYUV420SemiPlanar";
  case OMX_COLOR_FormatYUV422Planar: return "OMX_COLOR_FormatYUV422Planar";
  case OMX_COLOR_FormatYUV422PackedPlanar: return "OMX_COLOR_FormatYUV422PackedPlanar";
  case OMX_COLOR_FormatYUV422SemiPlanar: return "OMX_COLOR_FormatYUV422SemiPlanar";
  case OMX_COLOR_FormatYCbYCr: return "OMX_COLOR_FormatYCbYCr";
  case OMX_COLOR_FormatYCrYCb: return "OMX_COLOR_FormatYCrYCb";
  case OMX_COLOR_FormatCbYCrY: return "OMX_COLOR_FormatCbYCrY";
  case OMX_COLOR_FormatCrYCbY: return "OMX_COLOR_FormatCrYCbY";
  case OMX_COLOR_FormatYUV444Interleaved: return "OMX_COLOR_FormatYUV444Interleaved";
  case OMX_COLOR_FormatRawBayer8bit: return "OMX_COLOR_FormatRawBayer8bit";
  case OMX_COLOR_FormatRawBayer10bit: return "OMX_COLOR_FormatRawBayer10bit";
  case OMX_COLOR_FormatRawBayer8bitcompressed: return "OMX_COLOR_FormatRawBayer8bitcompressed";
  case OMX_COLOR_FormatL2: return "OMX_COLOR_FormatL2";
  case OMX_COLOR_FormatL4: return "OMX_COLOR_FormatL4";
  case OMX_COLOR_FormatL8: return "OMX_COLOR_FormatL8";
  case OMX_COLOR_FormatL16: return "OMX_COLOR_FormatL16";
  case OMX_COLOR_FormatL24: return "OMX_COLOR_FormatL24";
  case OMX_COLOR_FormatL32: return "OMX_COLOR_FormatL32";
  case OMX_COLOR_FormatYUV420PackedSemiPlanar: return "OMX_COLOR_FormatYUV420PackedSemiPlanar";
  case OMX_COLOR_FormatYUV422PackedSemiPlanar: return "OMX_COLOR_FormatYUV422PackedSemiPlanar";
  case OMX_COLOR_Format18BitBGR666: return "OMX_COLOR_Format18BitBGR666";
  case OMX_COLOR_Format24BitARGB6666: return "OMX_COLOR_Format24BitARGB6666";
  case OMX_COLOR_Format24BitABGR6666: return "OMX_COLOR_Format24BitABGR6666";

  case OMX_COLOR_FormatAndroidOpaque: return "OMX_COLOR_FormatAndroidOpaque";
  case OMX_TI_COLOR_FormatYUV420PackedSemiPlanar: return "OMX_TI_COLOR_FormatYUV420PackedSemiPlanar";
  case OMX_QCOM_COLOR_FormatYVU420SemiPlanar: return "OMX_QCOM_COLOR_FormatYVU420SemiPlanar";
  case OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka: return "OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka";
  case OMX_SEC_COLOR_FormatNV12Tiled: return "OMX_SEC_COLOR_FormatNV12Tiled";
  case OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m: return "OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m";

  case QOMX_COLOR_FormatYVU420PackedSemiPlanar32m4ka: return "QOMX_COLOR_FormatYVU420PackedSemiPlanar32m4ka";
  case QOMX_COLOR_FormatYUV420PackedSemiPlanar16m2ka: return "QOMX_COLOR_FormatYUV420PackedSemiPlanar16m2ka";
  case QOMX_COLOR_FORMATYUV420PackedSemiPlanar32mMultiView: return "QOMX_COLOR_FORMATYUV420PackedSemiPlanar32mMultiView";
  case QOMX_COLOR_FORMATYUV420PackedSemiPlanar32mCompressed: return "QOMX_COLOR_FORMATYUV420PackedSemiPlanar32mCompressed";
  case QOMX_COLOR_Format32bitRGBA8888: return "QOMX_COLOR_Format32bitRGBA8888";
  case QOMX_COLOR_Format32bitRGBA8888Compressed: return "QOMX_COLOR_Format32bitRGBA8888Compressed";

  default:
    return "unkn";
  }
}


// ***** encoder functions *****

OmxEncoder::OmxEncoder(const char* path, int width, int height, int fps, int bitrate) {
  this->path = path;
  this->width = width;
  this->height = height;
  this->fps = fps;

  OMX_STRING component = (OMX_STRING)("OMX.qcom.video.encoder.avc");
  int err = OMX_GetHandle(&handle, component, this, &omx_callbacks);
  if (err != OMX_ErrorNone) {
    LOGE("error getting codec: %x", err);
  }

  // setup input port
  OMX_PARAM_PORTDEFINITIONTYPE in_port = {0};
  in_port.nSize = sizeof(in_port);
  in_port.nPortIndex = (OMX_U32) PORT_INDEX_IN;
  OMX_CHECK(OMX_GetParameter(handle, OMX_IndexParamPortDefinition, (OMX_PTR) &in_port));

  in_port.format.video.nFrameWidth = width;
  in_port.format.video.nFrameHeight = height;
  in_port.format.video.nStride = VENUS_Y_STRIDE(COLOR_FMT_NV12, width);
  in_port.format.video.nSliceHeight = height;
  in_port.nBufferSize = VENUS_BUFFER_SIZE(COLOR_FMT_NV12, width, height);
  in_port.format.video.xFramerate = (fps * 65536);
  in_port.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
  in_port.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)QOMX_COLOR_FORMATYUV420PackedSemiPlanar32m;

  OMX_CHECK(OMX_SetParameter(handle, OMX_IndexParamPortDefinition, (OMX_PTR) &in_port));
  OMX_CHECK(OMX_GetParameter(handle, OMX_IndexParamPortDefinition, (OMX_PTR) &in_port));
  in_buf_headers.resize(in_port.nBufferCountActual);

  // setup output port
  OMX_PARAM_PORTDEFINITIONTYPE out_port;
  memset(&out_port, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  out_port.nSize = sizeof(out_port);
  out_port.nVersion.s.nVersionMajor = 1;
  out_port.nVersion.s.nVersionMinor = 0;
  out_port.nVersion.s.nRevision = 0;
  out_port.nVersion.s.nStep = 0;
  out_port.nPortIndex = (OMX_U32) PORT_INDEX_OUT;

  OMX_ERRORTYPE error = OMX_GetParameter(handle, OMX_IndexParamPortDefinition, (OMX_PTR)&out_port);
  if (error != OMX_ErrorNone) {
    LOGE("Error getting output port parameters: 0x%08x", error);
    return;
  }

  out_port.format.video.nFrameWidth = width;
  out_port.format.video.nFrameHeight = height;
  out_port.format.video.xFramerate = 0;
  out_port.format.video.nBitrate = bitrate;
  out_port.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
  out_port.format.video.eColorFormat = OMX_COLOR_FormatUnused;

  error = OMX_SetParameter(handle, OMX_IndexParamPortDefinition, (OMX_PTR) &out_port);
  if (error != OMX_ErrorNone) {
    LOGE("Error setting output port parameters: 0x%08x", error);
    return;
  }

  error = OMX_GetParameter(handle, OMX_IndexParamPortDefinition, (OMX_PTR) &out_port);
  if (error != OMX_ErrorNone) {
    LOGE("Error getting updated output port parameters: 0x%08x", error);
    return;
  }

  out_buf_headers.resize(out_port.nBufferCountActual);

  OMX_VIDEO_PARAM_BITRATETYPE bitrate_type = {0};
  bitrate_type.nSize = sizeof(bitrate_type);
  bitrate_type.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
  OMX_CHECK(OMX_GetParameter(handle, OMX_IndexParamVideoBitrate, (OMX_PTR) &bitrate_type));
  bitrate_type.eControlRate = OMX_Video_ControlRateVariable;
  bitrate_type.nTargetBitrate = bitrate;

  OMX_CHECK(OMX_SetParameter(handle, OMX_IndexParamVideoBitrate, (OMX_PTR) &bitrate_type));

  // setup h264
  OMX_VIDEO_PARAM_AVCTYPE avc = {0};
  avc.nSize = sizeof(avc);
  avc.nPortIndex = (OMX_U32) PORT_INDEX_OUT;
  OMX_CHECK(OMX_GetParameter(handle, OMX_IndexParamVideoAvc, &avc));

  avc.nBFrames = 0;
  avc.nPFrames = 15;

  avc.eProfile = OMX_VIDEO_AVCProfileHigh;
  avc.eLevel = OMX_VIDEO_AVCLevel31;

  avc.nAllowedPictureTypes |= OMX_VIDEO_PictureTypeB;
  avc.eLoopFilterMode = OMX_VIDEO_AVCLoopFilterEnable;

  avc.nRefFrames = 1;
  avc.bUseHadamard = OMX_TRUE;
  avc.bEntropyCodingCABAC = OMX_TRUE;
  avc.bWeightedPPrediction = OMX_TRUE;
  avc.bconstIpred = OMX_TRUE;

  OMX_CHECK(OMX_SetParameter(handle, OMX_IndexParamVideoAvc, &avc));
  OMX_CHECK(OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateIdle, NULL));

  for (OMX_BUFFERHEADERTYPE* &buf : in_buf_headers) {
    OMX_CHECK(OMX_AllocateBuffer(handle, &buf, PORT_INDEX_IN, this, in_port.nBufferSize));
  }

  for (OMX_BUFFERHEADERTYPE* &buf : out_buf_headers) {
    OMX_CHECK(OMX_AllocateBuffer(handle, &buf, PORT_INDEX_OUT, this, out_port.nBufferSize));
  }

  wait_for_state(OMX_StateIdle);

  OMX_CHECK(OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateExecuting, NULL));

  wait_for_state(OMX_StateExecuting);

  // give omx all the output buffers
  for (OMX_BUFFERHEADERTYPE* &buf : out_buf_headers) {
    OMX_CHECK(OMX_FillThisBuffer(handle, buf));
  }

  // fill the input free queue
  for (OMX_BUFFERHEADERTYPE* &buf : in_buf_headers) {
    free_in.push(buf);
  }
}

void OmxEncoder::handle_out_buf(OmxEncoder *encoder, OMX_BUFFERHEADERTYPE *out_buf) {
  int err;
  uint8_t *buf_data = out_buf->pBuffer + out_buf->nOffset;

  if (out_buf->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
    if (encoder->codec_config_len < out_buf->nFilledLen) {
      encoder->codec_config = (uint8_t *)realloc(encoder->codec_config, out_buf->nFilledLen);
    }
    encoder->codec_config_len = out_buf->nFilledLen;
    memcpy(encoder->codec_config, buf_data, out_buf->nFilledLen);
#ifdef QCOM2
    out_buf->nTimeStamp = 0;
#endif
  }

  if (encoder->of) {
    fwrite(buf_data, out_buf->nFilledLen, 1, encoder->of);
  }

  if (!encoder->wrote_codec_config && encoder->codec_config_len > 0) {
    // extradata will be freed by av_free() in avcodec_free_context()
    encoder->codec_ctx->extradata = (uint8_t*)av_mallocz(encoder->codec_config_len + AV_INPUT_BUFFER_PADDING_SIZE);
    encoder->codec_ctx->extradata_size = encoder->codec_config_len;
    memcpy(encoder->codec_ctx->extradata, encoder->codec_config, encoder->codec_config_len);

    err = avcodec_parameters_from_context(encoder->out_stream->codecpar, encoder->codec_ctx);
    assert(err >= 0);
    err = avformat_write_header(encoder->ofmt_ctx, NULL);
    assert(err >= 0);

    encoder->wrote_codec_config = true;
  }

  if (out_buf->nTimeStamp > 0) {
    // input timestamps are in microseconds
    AVRational in_timebase = {1, 1000000};

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = buf_data;
    pkt.size = out_buf->nFilledLen;

    enum AVRounding rnd = static_cast<enum AVRounding>(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
    pkt.pts = pkt.dts = av_rescale_q_rnd(out_buf->nTimeStamp, in_timebase, encoder->ofmt_ctx->streams[0]->time_base, rnd);
    pkt.duration = av_rescale_q(50 * 1000, in_timebase, encoder->ofmt_ctx->streams[0]->time_base);

    if (out_buf->nFlags & OMX_BUFFERFLAG_SYNCFRAME) {
      pkt.flags |= AV_PKT_FLAG_KEY;
    }

    err = av_write_frame(encoder->ofmt_ctx, &pkt);
    if (err < 0) {
      LOGW("ts encoder write issue");
    }

    av_free_packet(&pkt);
  }

  // give omx back the buffer
#ifdef QCOM2
  if (out_buf->nFlags & OMX_BUFFERFLAG_EOS) {
    out_buf->nTimeStamp = 0;
  }
#endif
  OMX_CHECK(OMX_FillThisBuffer(encoder->handle, out_buf));
}

int OmxEncoder::encode_frame_rgba(const uint8_t *ptr, int in_width, int in_height, uint64_t ts) {
  if (!is_open) {
    return -1;
  }

  // this sometimes freezes... put it outside the encoder lock so we can still trigger rotates...
  // THIS IS A REALLY BAD IDEA, but apparently the race has to happen 30 times to trigger this
  OMX_BUFFERHEADERTYPE* in_buf = nullptr;
  while (!free_in.try_pop(in_buf, 20)) {
    if (do_exit) {
      return -1;
    }
  }

  int ret = counter;

  uint8_t *in_buf_ptr = in_buf->pBuffer;

  uint8_t *in_y_ptr = in_buf_ptr;
  int in_y_stride = VENUS_Y_STRIDE(COLOR_FMT_NV12, width);
  int in_uv_stride = VENUS_UV_STRIDE(COLOR_FMT_NV12, width);
  uint8_t *in_uv_ptr = in_buf_ptr + (in_y_stride * VENUS_Y_SCANLINES(COLOR_FMT_NV12, height));

  int err = ABGRToNV12(ptr, width * 4, in_y_ptr, in_y_stride, in_uv_ptr, in_uv_stride, width, height);
  assert(err == 0);

  in_buf->nFilledLen = VENUS_BUFFER_SIZE(COLOR_FMT_NV12, width, height);
  in_buf->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;
  in_buf->nOffset = 0;
  in_buf->nTimeStamp = ts / 1000LL;  // OMX_TICKS, in microseconds
  last_t = in_buf->nTimeStamp;

  OMX_CHECK(OMX_EmptyThisBuffer(handle, in_buf));

  // pump output
  while (true) {
    OMX_BUFFERHEADERTYPE *out_buf;
    if (!done_out.try_pop(out_buf)) {
      break;
    }
    handle_out_buf(this, out_buf);
  }

  dirty = true;

  counter++;

  return ret;
}

void OmxEncoder::encoder_open(const char* filename) {
  struct stat st = {0};
  if (stat(path.c_str(), &st) == -1) {
    mkdir(path.c_str(), 0755);
  }

  snprintf(vid_path, sizeof(vid_path), "%s/%s", path.c_str(), filename);

  avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, vid_path);
  assert(ofmt_ctx);

  out_stream = avformat_new_stream(ofmt_ctx, NULL);
  assert(out_stream);

  // set codec correctly
  av_register_all();

  AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
  assert(codec);

  codec_ctx = avcodec_alloc_context3(codec);
  assert(codec_ctx);
  codec_ctx->width = width;
  codec_ctx->height = height;
  codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  codec_ctx->time_base = {1, fps};

  int err = avio_open(&ofmt_ctx->pb, vid_path, AVIO_FLAG_WRITE);
  assert(err >= 0);

  wrote_codec_config = false;

  // create camera lock file
  snprintf(lock_path, sizeof(lock_path), "%s/%s.lock", path.c_str(), filename);
  int lock_fd = HANDLE_EINTR(open(lock_path, O_RDWR | O_CREAT, 0664));
  assert(lock_fd >= 0);
  close(lock_fd);

  is_open = true;
  counter = 0;
}

void OmxEncoder::encoder_close() {
  if (is_open) {
    if (dirty) {
      // drain output only if there could be frames in the encoder

      OMX_BUFFERHEADERTYPE* in_buf = free_in.pop();
      in_buf->nFilledLen = 0;
      in_buf->nOffset = 0;
      in_buf->nFlags = OMX_BUFFERFLAG_EOS;
      in_buf->nTimeStamp = last_t + 1000000LL / fps;

      OMX_CHECK(OMX_EmptyThisBuffer(handle, in_buf));

      while (true) {
        OMX_BUFFERHEADERTYPE *out_buf = done_out.pop();

        handle_out_buf(this, out_buf);

        if (out_buf->nFlags & OMX_BUFFERFLAG_EOS) {
          break;
        }
      }
      dirty = false;
    }

    av_write_trailer(ofmt_ctx);
    avcodec_free_context(&codec_ctx);
    avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    unlink(lock_path);
  }
  is_open = false;
}

OmxEncoder::~OmxEncoder() {
  assert(!is_open);

  OMX_CHECK(OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateIdle, NULL));

  wait_for_state(OMX_StateIdle);

  OMX_CHECK(OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateLoaded, NULL));

  for (OMX_BUFFERHEADERTYPE* buf : in_buf_headers) {
    OMX_CHECK(OMX_FreeBuffer(handle, PORT_INDEX_IN, buf));
  }

  for (OMX_BUFFERHEADERTYPE* buf : out_buf_headers) {
    OMX_CHECK(OMX_FreeBuffer(handle, PORT_INDEX_OUT, buf));
  }

  wait_for_state(OMX_StateLoaded);

  OMX_CHECK(OMX_FreeHandle(handle));

  OMX_BUFFERHEADERTYPE *out_buf;
  while (free_in.try_pop(out_buf));
  while (done_out.try_pop(out_buf));

  if (codec_config) {
    free(codec_config);
  }
}
