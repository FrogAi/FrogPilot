#include "frogpilot/ui/qt/onroad/screen_recorder.h"

#ifdef QCOM2

#include <link.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <wayland-client.h>

extern "C" {
#include <libavformat/avformat.h>
}

// has to be in this order
#include "third_party/linux/include/v4l2-controls.h"
#include <linux/videodev2.h>

#include "common/swaglog.h"
#include "msgq/visionipc/visionbuf.h"
#include "selfdrive/ui/ui.h"
#include "third_party/c2d2/c2d2.h"
#include "third_party/linux/include/msm_media_info.h"

extern "C" C2D_STATUS c2dDriverInit(C2D_DRIVER_SETUP_INFO *setup);
extern "C" C2D_STATUS c2dDriverDeInit(void);

namespace {
const char ENCODER_DEVICE[] = "/dev/v4l/by-path/platform-aa00000.qcom_vidc-video-index1";
const char IN_PROGRESS_DIR[] = "/data/media/screen_recordings.in_progress";
const char RECORDINGS_DIR[] = "/data/media/screen_recordings";

const int BITRATE = 6000000;
const int COLOR_SPACE_BT601_625 = 5;
const int ENCODER_BUFFERS = 3;
const int MAX_REQUEST_ARGUMENTS = 20;

const int64_t FRAME_INTERVAL_US = 1000000 / UI_FREQ;
const int64_t MAX_REPLAY_GAP_US = 1000000;

const size_t MAX_REPLAY_BYTES_PER_SECOND = BITRATE / 8 * 5 / 4;
const size_t SAVE_FRAMES_PER_STEP = UI_FREQ;

const uint32_t KGSL_USER_MEM_TYPE_ION = 3;
const uint32_t V4L2_EVENT_MSM_VIDC_SYS_ERROR = V4L2_EVENT_PRIVATE_START + 0x1000 + 5;
const uint32_t V4L2_QCOM_BUF_DATA_CORRUPT = 0x00400000;
const uint32_t V4L2_QCOM_BUF_FLAG_CODECCONFIG = 0x00020000;

struct SwapchainBuffer {
  int fd;

  size_t size;

  uint32_t height;
  uint32_t stride;
  uint32_t width;

  wl_proxy *wl_buffer;
};

struct Source {
  size_t size = 0;

  uint32_t surface = 0;

  void *gpu = nullptr;
  void *map = MAP_FAILED;

  wl_proxy *wl_buffer = nullptr;
};

struct Session {
  bool c2d_initialized = false;

  int fd = -1;

  int64_t next_capture_us = 0;

  uint32_t height = 0;
  uint32_t targets[ENCODER_BUFFERS] = {};
  uint32_t width = 0;

  void *input_gpu[ENCODER_BUFFERS] = {};

  C2D_STATUS capture_status = C2D_STATUS_OK;

  std::vector<std::pair<int, int64_t>> captured;
  std::vector<uint8_t> codec_config;
  std::vector<int> free_inputs;
  std::vector<Source> sources;

  VisionBuf inputs[ENCODER_BUFFERS];
  VisionBuf outputs[ENCODER_BUFFERS];
};

int wake_fd = -1;

Session *capture_session = nullptr;

std::atomic<bool> manual_request = false;
std::atomic<bool> quit_request = false;
std::atomic<bool> replay_ready = false;
std::atomic<bool> replay_saving = false;
std::atomic<bool> save_request = false;

std::atomic<int> replay_request = 0;

std::atomic<wl_event_queue *> driver_queue = nullptr;

std::mutex capture_mutex;

std::thread recorder_thread;

std::vector<SwapchainBuffer> swapchain;

void *(*proxyCreateWrapper)(void *) = nullptr;
void (*proxyWrapperDestroy)(void *) = nullptr;

wl_proxy *attached_buffer = nullptr;

void wake() {
  eventfd_write(wake_fd, 1);
}

std::string error(const std::string &what) {
  return what + ": " + strerror(errno);
}

const wl_interface *proxyInterface(wl_proxy *proxy) {
  return *reinterpret_cast<const wl_interface *const *>(proxy);
}

void argumentsFromVaList(const char *signature, wl_argument *args, va_list ap) {
  int count = 0;
  for (const char *type = signature; *type; type++) {
    if (*type == 'i') {
      args[count++].i = va_arg(ap, int32_t);
    } else if (*type == 'u') {
      args[count++].u = va_arg(ap, uint32_t);
    } else if (*type == 'f') {
      args[count++].f = va_arg(ap, wl_fixed_t);
    } else if (*type == 's') {
      args[count++].s = va_arg(ap, const char *);
    } else if (*type == 'o' || *type == 'n') {
      args[count++].o = va_arg(ap, wl_object *);
    } else if (*type == 'a') {
      args[count++].a = va_arg(ap, wl_array *);
    } else if (*type == 'h') {
      args[count++].h = va_arg(ap, int32_t);
    }
  }
}

void capture(wl_proxy *wl_buffer) {
  std::lock_guard lock(capture_mutex);

  Session *s = capture_session;
  if (!s || s->capture_status != C2D_STATUS_OK || s->free_inputs.empty()) {
    return;
  }

  const Source *source = nullptr;
  for (const Source &candidate : s->sources) {
    if (candidate.wl_buffer == wl_buffer) {
      source = &candidate;
    }
  }

  int64_t timestamp_us = nanos_since_boot() / 1000;
  if (!source || timestamp_us < s->next_capture_us - FRAME_INTERVAL_US) {
    return;
  }

  int input = s->free_inputs.back();

  C2D_OBJECT blit = {};
  blit.surface_id = source->surface;
  blit.config_mask = C2D_SOURCE_RECT_BIT | C2D_TARGET_RECT_BIT | C2D_NO_BILINEAR_BIT | C2D_NO_ANTIALIASING_BIT | C2D_ALPHA_BLEND_NONE;
  blit.source_rect = {0, 0, static_cast<int32>(s->width << 16), static_cast<int32>(s->height << 16)};
  blit.target_rect = blit.source_rect;

  s->capture_status = c2dDraw(s->targets[input], 0, nullptr, 0, 0, &blit, 1);
  if (s->capture_status == C2D_STATUS_OK) {
    s->capture_status = c2dFinish(s->targets[input]);
  }

  if (s->capture_status == C2D_STATUS_OK) {
    s->next_capture_us = std::max(s->next_capture_us, timestamp_us) + FRAME_INTERVAL_US;
    s->free_inputs.pop_back();
    s->captured.push_back({input, timestamp_us});
  }

  wake();
}

void hookedProxyMarshal(wl_proxy *proxy, uint32_t opcode, ...) {
  const wl_interface *interface = proxyInterface(proxy);

  wl_argument args[MAX_REQUEST_ARGUMENTS];
  va_list ap;
  va_start(ap, opcode);
  argumentsFromVaList(interface->methods[opcode].signature, args, ap);
  va_end(ap);

  if (strcmp(interface->name, "wl_surface") == 0) {
    if (opcode == WL_SURFACE_ATTACH) {
      attached_buffer = reinterpret_cast<wl_proxy *>(args[0].o);
    } else if (opcode == WL_SURFACE_COMMIT) {
      capture(attached_buffer);
      attached_buffer = nullptr;
    }
  }

  wl_proxy_marshal_array(proxy, opcode, args);
}

wl_proxy *hookedProxyMarshalConstructor(wl_proxy *proxy, uint32_t opcode, const wl_interface *interface, ...) {
  const wl_message &request = proxyInterface(proxy)->methods[opcode];

  wl_argument args[MAX_REQUEST_ARGUMENTS];
  va_list ap;
  va_start(ap, interface);
  argumentsFromVaList(request.signature, args, ap);
  va_end(ap);

  wl_event_queue *queue = driver_queue;
  if (queue && proxyCreateWrapper && strcmp(interface->name, "wl_callback") == 0) {
    wl_proxy *wrapper = static_cast<wl_proxy *>(proxyCreateWrapper(proxy));
    wl_proxy_set_queue(wrapper, queue);
    wl_proxy *callback = wl_proxy_marshal_array_constructor(wrapper, opcode, args, interface);
    proxyWrapperDestroy(wrapper);
    return callback;
  }

  wl_proxy *created = wl_proxy_marshal_array_constructor(proxy, opcode, args, interface);
  if (strcmp(proxyInterface(proxy)->name, "wayland_buffer_backend") == 0 && strcmp(request.name, "create_buffer") == 0) {
    int fd = fcntl(args[1].h, F_DUPFD_CLOEXEC, 0);  // create_buffer(new wl_buffer, ion_fd, ion_metadata_fd, width, height, format, stride)
    {
      std::lock_guard lock(capture_mutex);
      swapchain.push_back({.fd = fd, .size = static_cast<size_t>(lseek(fd, 0, SEEK_END)), .height = args[4].u, .stride = args[6].u, .width = args[3].u, .wl_buffer = created});
    }
    wake();
  }
  return created;
}

void hookedProxySetQueue(wl_proxy *proxy, wl_event_queue *queue) {
  driver_queue = queue;
  wl_proxy_set_queue(proxy, queue);
}

int patchDriverImports(dl_phdr_info *object, size_t, void *) {
  if (!strstr(object->dlpi_name, "libeglSubDriverWayland")) {
    return 0;
  }

  const ElfW(Sym) *symbols = nullptr;
  const char *strings = nullptr;
  const ElfW(Rela) *relocations = nullptr;
  size_t relocations_size = 0;

  for (int i = 0; i < object->dlpi_phnum; i++) {
    if (object->dlpi_phdr[i].p_type != PT_DYNAMIC) {
      continue;
    }

    for (const ElfW(Dyn) *dyn = reinterpret_cast<const ElfW(Dyn) *>(object->dlpi_addr + object->dlpi_phdr[i].p_vaddr); dyn->d_tag != DT_NULL; dyn++) {
      if (dyn->d_tag == DT_SYMTAB) {
        symbols = reinterpret_cast<const ElfW(Sym) *>(dyn->d_un.d_ptr);
      } else if (dyn->d_tag == DT_STRTAB) {
        strings = reinterpret_cast<const char *>(dyn->d_un.d_ptr);
      } else if (dyn->d_tag == DT_JMPREL) {
        relocations = reinterpret_cast<const ElfW(Rela) *>(dyn->d_un.d_ptr);
      } else if (dyn->d_tag == DT_PLTRELSZ) {
        relocations_size = dyn->d_un.d_val;
      }
    }
  }

  for (size_t i = 0; i < relocations_size / sizeof(ElfW(Rela)); i++) {
    const char *symbol = strings + symbols[ELF64_R_SYM(relocations[i].r_info)].st_name;

    void *hook = nullptr;
    if (strcmp(symbol, "wl_proxy_marshal") == 0) {
      hook = reinterpret_cast<void *>(hookedProxyMarshal);
    } else if (strcmp(symbol, "wl_proxy_marshal_constructor") == 0) {
      hook = reinterpret_cast<void *>(hookedProxyMarshalConstructor);
    } else if (strcmp(symbol, "wl_proxy_set_queue") == 0) {
      hook = reinterpret_cast<void *>(hookedProxySetQueue);
    }

    if (hook) {
      void **slot = reinterpret_cast<void **>(object->dlpi_addr + relocations[i].r_offset);
      void *page = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(slot) & ~static_cast<uintptr_t>(getpagesize() - 1));
      mprotect(page, getpagesize(), PROT_READ | PROT_WRITE);
      *slot = hook;
    }
  }
  return 1;
}

bool queueBuffer(int fd, v4l2_buf_type type, int index, VisionBuf &buf, int64_t timestamp_us) {
  v4l2_plane plane = {
    .bytesused = static_cast<uint32_t>(buf.len),
    .length = static_cast<uint32_t>(buf.len),
    .m = {.userptr = reinterpret_cast<unsigned long>(buf.addr)},
    .reserved = {static_cast<unsigned int>(buf.fd)},
  };
  v4l2_buffer buffer = {
    .index = static_cast<uint32_t>(index),
    .type = type,
    .flags = V4L2_BUF_FLAG_TIMESTAMP_COPY,
    .timestamp = {static_cast<time_t>(timestamp_us / 1000000), static_cast<suseconds_t>(timestamp_us % 1000000)},
    .memory = V4L2_MEMORY_USERPTR,
    .m = {.planes = &plane},
    .length = 1,
  };
  return util::safe_ioctl(fd, VIDIOC_QBUF, &buffer) == 0;
}

bool dequeueBuffer(int fd, v4l2_buf_type type, v4l2_buffer &buffer, v4l2_plane &plane) {
  buffer = {.type = type, .memory = V4L2_MEMORY_USERPTR, .m = {.planes = &plane}, .length = 1};
  return util::safe_ioctl(fd, VIDIOC_DQBUF, &buffer) == 0;
}

std::string openEncoder(Session &s) {
  s.fd = HANDLE_EINTR(open(ENCODER_DEVICE, O_RDWR | O_NONBLOCK | O_CLOEXEC));
  if (s.fd < 0) {
    return error("open encoder");
  }

  v4l2_format encoded = {
    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
    .fmt = {.pix_mp = {.width = s.width, .height = s.height, .pixelformat = V4L2_PIX_FMT_H264, .field = V4L2_FIELD_ANY, .colorspace = V4L2_COLORSPACE_DEFAULT}},
  };
  if (util::safe_ioctl(s.fd, VIDIOC_S_FMT, &encoded) != 0) {
    return error("VIDIOC_S_FMT");
  }

  v4l2_streamparm frame_rate = {.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, .parm = {.output = {.timeperframe = {1, UI_FREQ}}}};
  if (util::safe_ioctl(s.fd, VIDIOC_S_PARM, &frame_rate) != 0) {
    return error("VIDIOC_S_PARM");
  }

  v4l2_format raw = {
    .type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
    .fmt = {.pix_mp = {.width = s.width, .height = s.height, .pixelformat = V4L2_PIX_FMT_NV12, .field = V4L2_FIELD_ANY,
                       .colorspace = V4L2_COLORSPACE_470_SYSTEM_BG}},
  };
  if (util::safe_ioctl(s.fd, VIDIOC_S_FMT, &raw) != 0) {
    return error("VIDIOC_S_FMT");
  }

  v4l2_control controls[] = {
    {.id = V4L2_CID_MPEG_VIDEO_BITRATE, .value = BITRATE},
    {.id = V4L2_CID_MPEG_VIDC_VIDEO_NUM_P_FRAMES, .value = UI_FREQ - 1},
    {.id = V4L2_CID_MPEG_VIDC_VIDEO_NUM_B_FRAMES, .value = 0},
    {.id = V4L2_CID_MPEG_VIDEO_HEADER_MODE, .value = V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE},
    {.id = V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL, .value = V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_VBR_CFR},
    {.id = V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY, .value = V4L2_MPEG_VIDC_VIDEO_PRIORITY_REALTIME_DISABLE},
    {.id = V4L2_CID_MPEG_VIDC_VIDEO_IDR_PERIOD, .value = 1},
    {.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE, .value = V4L2_MPEG_VIDEO_H264_PROFILE_HIGH},
    {.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL, .value = V4L2_MPEG_VIDEO_H264_LEVEL_UNKNOWN},
    {.id = V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE, .value = V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC},
    {.id = V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL, .value = V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL_0},
    {.id = V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE, .value = 0},
    {.id = V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA, .value = 0},
    {.id = V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA, .value = 0},
    {.id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE, .value = 0},
    {.id = V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE, .value = COLOR_SPACE_BT601_625},
    {.id = V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE, .value = V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE_DISABLE},
  };
  for (v4l2_control control : controls) {
    if (util::safe_ioctl(s.fd, VIDIOC_S_CTRL, &control) != 0) {
      return error(util::string_format("VIDIOC_S_CTRL %#x", control.id));
    }
  }

  v4l2_event_subscription error_event = {.type = V4L2_EVENT_MSM_VIDC_SYS_ERROR};
  if (util::safe_ioctl(s.fd, VIDIOC_SUBSCRIBE_EVENT, &error_event) != 0) {
    return error("VIDIOC_SUBSCRIBE_EVENT");
  }

  for (v4l2_buf_type type : {V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE}) {
    v4l2_requestbuffers request = {.count = ENCODER_BUFFERS, .type = type, .memory = V4L2_MEMORY_USERPTR};
    if (util::safe_ioctl(s.fd, VIDIOC_REQBUFS, &request) != 0) {
      return error("VIDIOC_REQBUFS");
    }
  }

  for (v4l2_buf_type type : {V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE}) {
    if (util::safe_ioctl(s.fd, VIDIOC_STREAMON, &type) != 0) {
      return error("VIDIOC_STREAMON");
    }
  }

  for (int i = 0; i < ENCODER_BUFFERS; i++) {
    s.outputs[i].allocate(encoded.fmt.pix_mp.plane_fmt[0].sizeimage);
    s.inputs[i].allocate(raw.fmt.pix_mp.plane_fmt[0].sizeimage);
    if (s.outputs[i].sync(VISIONBUF_SYNC_TO_DEVICE) != 0 || s.inputs[i].sync(VISIONBUF_SYNC_TO_DEVICE) != 0) {
      return error("ION cache clean");
    }

    if (!queueBuffer(s.fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, i, s.outputs[i], 0)) {
      return error("VIDIOC_QBUF");
    }

    s.free_inputs.push_back(i);
  }
  return "";
}

std::string createTargets(Session &s) {
  C2D_DRIVER_SETUP_INFO setup = {.max_surface_template_needed = 8};
  C2D_STATUS status = c2dDriverInit(&setup);
  if (status != C2D_STATUS_OK) {
    return util::string_format("c2dDriverInit returned %d", status);
  }
  s.c2d_initialized = true;

  uint32_t uv_offset = VENUS_Y_STRIDE(COLOR_FMT_NV12, s.width) * VENUS_Y_SCANLINES(COLOR_FMT_NV12, s.height);
  for (int i = 0; i < ENCODER_BUFFERS; i++) {
    VisionBuf &input = s.inputs[i];
    status = c2dMapAddr(input.fd, input.addr, input.len, 0, KGSL_USER_MEM_TYPE_ION, &s.input_gpu[i]);
    if (status != C2D_STATUS_OK) {
      return util::string_format("c2dMapAddr returned %d", status);
    }

    C2D_YUV_SURFACE_DEF nv12 = {
      .format = C2D_COLOR_FORMAT_420_NV12,
      .width = s.width,
      .height = s.height,
      .plane0 = input.addr,
      .phys0 = s.input_gpu[i],
      .stride0 = static_cast<int32>(VENUS_Y_STRIDE(COLOR_FMT_NV12, s.width)),
      .plane1 = static_cast<uint8_t *>(input.addr) + uv_offset,
      .phys1 = static_cast<uint8_t *>(s.input_gpu[i]) + uv_offset,
      .stride1 = static_cast<int32>(VENUS_UV_STRIDE(COLOR_FMT_NV12, s.width)),
    };
    status = c2dCreateSurface(&s.targets[i], C2D_TARGET, static_cast<C2D_SURFACE_TYPE>(C2D_SURFACE_YUV_HOST | C2D_SURFACE_WITH_PHYS), &nv12);
    if (status != C2D_STATUS_OK) {
      return util::string_format("c2dCreateSurface returned %d", status);
    }
  }
  return "";
}

std::string mapSource(Session &s, const SwapchainBuffer &buffer) {
  Source &source = s.sources.emplace_back();
  source.wl_buffer = buffer.wl_buffer;
  source.size = buffer.size;
  source.map = mmap(nullptr, buffer.size, PROT_READ | PROT_WRITE, MAP_SHARED, buffer.fd, 0);
  if (source.map == MAP_FAILED) {
    return error("mmap");
  }

  C2D_STATUS status = c2dMapAddr(buffer.fd, source.map, buffer.size, 0, KGSL_USER_MEM_TYPE_ION, &source.gpu);
  if (status != C2D_STATUS_OK) {
    return util::string_format("c2dMapAddr returned %d", status);
  }

  C2D_RGB_SURFACE_DEF rgba = {
    .format = C2D_COLOR_FORMAT_8888_ARGB | C2D_FORMAT_SWAP_RB | C2D_FORMAT_UBWC_COMPRESSED,
    .width = buffer.width,
    .height = buffer.height,
    .buffer = source.map,
    .phys = source.gpu,
    .stride = static_cast<int32>(buffer.stride),
  };
  status = c2dCreateSurface(&source.surface, C2D_SOURCE, static_cast<C2D_SURFACE_TYPE>(C2D_SURFACE_RGB_HOST | C2D_SURFACE_WITH_PHYS), &rgba);
  if (status != C2D_STATUS_OK) {
    return util::string_format("c2dCreateSurface returned %d", status);
  }
  return "";
}

class Mp4File {
public:
  ~Mp4File() {
    if (mp4) {
      if (mp4->pb) {
        avio_closep(&mp4->pb);
      }
      avformat_free_context(mp4);
    }

    if (!path.empty()) {
      unlink(path.c_str());
    }
  }

  bool open(uint32_t width, uint32_t height, const std::vector<uint8_t> &codec_config, bool replay) {
    char name[32];
    time_t now = time(nullptr);
    tm local = {};
    localtime_r(&now, &local);
    strftime(name, sizeof(name), "%Y-%m-%d_%H-%M-%S", &local);

    std::string filename = util::string_format("%s_%s%llu.mp4", name, replay ? "replay_" : "", static_cast<unsigned long long>(nanos_since_boot()));
    util::create_directories(IN_PROGRESS_DIR, 0775);
    path = std::string(IN_PROGRESS_DIR) + "/" + filename;
    final_path = std::string(RECORDINGS_DIR) + "/" + filename;

    if (avformat_alloc_output_context2(&mp4, nullptr, "mp4", path.c_str()) < 0) {
      return false;
    }

    stream = avformat_new_stream(mp4, nullptr);
    stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    stream->codecpar->codec_id = AV_CODEC_ID_H264;
    stream->codecpar->width = width;
    stream->codecpar->height = height;
    stream->codecpar->extradata = static_cast<uint8_t *>(av_mallocz(codec_config.size() + AV_INPUT_BUFFER_PADDING_SIZE));
    memcpy(stream->codecpar->extradata, codec_config.data(), codec_config.size());
    stream->codecpar->extradata_size = codec_config.size();
    stream->time_base = {1, 1000000};
    return avio_open(&mp4->pb, path.c_str(), AVIO_FLAG_WRITE) >= 0 && avformat_write_header(mp4, nullptr) >= 0;
  }

  bool write(const uint8_t *data, size_t size, int64_t timestamp_us, bool keyframe) {
    if (start_us < 0 && !keyframe) {
      return true;
    }

    if (start_us < 0) {
      start_us = timestamp_us;
    }

    AVPacket packet = {};
    packet.data = const_cast<uint8_t *>(data);
    packet.size = size;
    packet.stream_index = stream->index;
    packet.pts = packet.dts = av_rescale_q(timestamp_us - start_us, {1, 1000000}, stream->time_base);
    packet.duration = av_rescale_q(FRAME_INTERVAL_US, {1, 1000000}, stream->time_base);
    packet.flags = keyframe ? AV_PKT_FLAG_KEY : 0;
    return av_write_frame(mp4, &packet) >= 0;
  }

  bool finish() {
    if (start_us < 0 || av_write_trailer(mp4) < 0) {
      return false;
    }

    int write_error = mp4->pb->error;
    if (avio_closep(&mp4->pb) < 0 || write_error < 0 || rename(path.c_str(), final_path.c_str()) != 0) {
      return false;
    }

    path.clear();
    return true;
  }

private:
  int64_t start_us = -1;

  AVFormatContext *mp4 = nullptr;

  AVStream *stream = nullptr;

  std::string final_path;
  std::string path;
};

struct ReplayFrame {
  bool keyframe;

  int64_t timestamp_us;

  std::vector<uint8_t> data;
};

class ReplayBuffer {
public:
  using Frame = std::shared_ptr<const ReplayFrame>;

  void setDuration(int seconds) {
    duration_us = seconds * 1000000LL;
    max_bytes = seconds * MAX_REPLAY_BYTES_PER_SECOND;

    if (seconds == 0) {
      clear();
    }
    trim();
  }

  void clear() {
    groups.clear();
    bytes = 0;
  }

  void append(const uint8_t *data, size_t size, int64_t timestamp_us, bool keyframe) {
    replay_time_us += std::min(timestamp_us - last_timestamp_us, MAX_REPLAY_GAP_US);
    last_timestamp_us = timestamp_us;

    if (keyframe) {
      groups.emplace_back();
    } else if (groups.empty()) {
      return;
    }

    groups.back().push_back(std::make_shared<ReplayFrame>(ReplayFrame{keyframe, replay_time_us, {data, data + size}}));
    bytes += size;
    trim();
  }

  bool ready() const {
    return !groups.empty() && groups.back().back()->timestamp_us - groups.front().front()->timestamp_us >= 1000000;
  }

  std::vector<Frame> snapshot() const {
    std::vector<Frame> frames;
    for (const std::vector<Frame> &group : groups) {
      frames.insert(frames.end(), group.begin(), group.end());
    }
    return frames;
  }

private:
  void trim() {
    while (groups.size() > 1) {
      int64_t window_start_us = groups.back().back()->timestamp_us - duration_us;
      if (groups[1].front()->timestamp_us > window_start_us && bytes <= max_bytes) {
        break;
      }

      for (const Frame &frame : groups.front()) {
        bytes -= frame->data.size();
      }
      groups.pop_front();
    }
  }

  int64_t duration_us = 0;
  int64_t last_timestamp_us = 0;
  int64_t replay_time_us = 0;

  size_t bytes = 0;
  size_t max_bytes = 0;

  std::deque<std::vector<Frame>> groups;
};

struct ReplaySave {
  size_t next = 0;

  Mp4File file;

  std::vector<ReplayBuffer::Frame> frames;
};

class Recorder {
public:
  void run() {
    util::set_thread_name("screen_recorder");

    std::error_code error_code;
    std::filesystem::remove_all(IN_PROGRESS_DIR, error_code);
    if (error_code) {
      LOGE("screen recorder: could not clear %s: %s", IN_PROGRESS_DIR, error_code.message().c_str());
    }

    while (true) {
      handleRequests();
      if (quit_request && !session && !save) {
        break;
      }
      waitForEvents();
    }
  }

private:
  void handleRequests() {
    bool quit = quit_request;
    bool manual = !quit && manual_request;

    int seconds = quit ? 0 : replay_request.load();

    if (seconds != replay_seconds) {
      replay_seconds = seconds;
      replay.setDuration(seconds);
      failed = false;
    }

    if (manual && !recording) {
      failed = false;
    }

    bool capturing = !failed && (replay_seconds > 0 || manual);
    if (capturing && !session) {
      startSession();
    }

    if (session) {
      queueCaptured();
    }

    if (session) {
      updateCapture(capturing);
    }

    if (session && manual && !recording && !session->codec_config.empty()) {
      startRecording();
    }

    if (recording && !manual) {
      finishRecording(true);
    }

    if (save_request) {
      if (!save) {
        startSave();
      }
      save_request = false;
    }

    if (session && !capturing && !recording) {
      stopSession();
    }

    replay_ready = replay.ready();
  }

  void waitForEvents() {
    pollfd fds[2] = {
      {.fd = wake_fd, .events = POLLIN},
      {.fd = session ? session->fd : -1, .events = POLLIN | POLLOUT | POLLPRI},
    };
    if (HANDLE_EINTR(poll(fds, 2, save ? 0 : -1)) < 0) {
      fail(error("poll"));
      return;
    }

    if (fds[0].revents & POLLIN) {
      eventfd_t count;
      eventfd_read(wake_fd, &count);
    }

    if (session && (fds[1].revents & POLLPRI)) {
      fail("encoder reported a session error");
    }

    if (session && (fds[1].revents & POLLOUT)) {
      returnInput();
    }

    if (session && (fds[1].revents & POLLIN)) {
      receivePacket();
    }

    if (save) {
      continueSave();
    }
  }

  void startSession() {
    {
      std::lock_guard lock(capture_mutex);
      if (swapchain.empty()) {
        return;
      }

      session = std::make_unique<Session>();
      session->width = swapchain[0].width;
      session->height = swapchain[0].height;
    }

    std::string failure = openEncoder(*session);
    if (failure.empty()) {
      failure = createTargets(*session);
    }

    if (!failure.empty()) {
      fail(failure);
    }
  }

  void updateCapture(bool capturing) {
    std::vector<SwapchainBuffer> added;
    {
      std::lock_guard lock(capture_mutex);
      if (capturing) {
        added.assign(swapchain.begin() + session->sources.size(), swapchain.end());
      }

      if (capturing && added.empty()) {
        capture_session = session.get();
      } else {
        capture_session = nullptr;
      }
    }

    if (added.empty()) {
      return;
    }

    for (const SwapchainBuffer &buffer : added) {
      std::string failure = mapSource(*session, buffer);
      if (!failure.empty()) {
        fail(failure);
        return;
      }
    }

    std::lock_guard lock(capture_mutex);
    capture_session = session.get();
  }

  void stopSession() {
    {
      std::lock_guard lock(capture_mutex);
      capture_session = nullptr;
    }

    Session &s = *session;

    bool released = true;
    if (s.fd >= 0) {
      for (v4l2_buf_type type : {V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE}) {
        v4l2_requestbuffers request = {.count = 0, .type = type, .memory = V4L2_MEMORY_USERPTR};
        released &= util::safe_ioctl(s.fd, VIDIOC_STREAMOFF, &type) == 0;
        released &= util::safe_ioctl(s.fd, VIDIOC_REQBUFS, &request) == 0;
      }
      released &= close(s.fd) == 0;
    }

    for (Source &source : s.sources) {
      if (source.surface) {
        released &= c2dDestroySurface(source.surface) == C2D_STATUS_OK;
      }

      if (source.gpu) {
        released &= c2dUnMapAddr(source.gpu) == C2D_STATUS_OK;
      }

      if (source.map != MAP_FAILED) {
        released &= munmap(source.map, source.size) == 0;
      }
    }

    for (int i = 0; i < ENCODER_BUFFERS; i++) {
      if (s.targets[i]) {
        released &= c2dDestroySurface(s.targets[i]) == C2D_STATUS_OK;
      }

      if (s.input_gpu[i]) {
        released &= c2dUnMapAddr(s.input_gpu[i]) == C2D_STATUS_OK;
      }

      if (s.inputs[i].addr) {
        released &= s.inputs[i].free() == 0;
      }

      if (s.outputs[i].addr) {
        released &= s.outputs[i].free() == 0;
      }
    }

    if (s.c2d_initialized) {
      released &= c2dDriverDeInit() == C2D_STATUS_OK;
    }

    if (!released) {
      LOGE("screen recorder: could not release every encoder resource");
    }

    session.reset();
    replay.clear();
  }

  void fail(const std::string &reason) {
    LOGE("screen recorder: %s, stopping", reason.c_str());

    failed = true;

    if (recording) {
      finishRecording(true);
    } else if (manual_request) {
      notify(cereal::FrogPilotOnroadEvent::EventName::RECORDING_START_FAILED);
    }
    manual_request = false;

    if (session) {
      stopSession();
    }
  }

  void queueCaptured() {
    C2D_STATUS capture_status;

    std::vector<std::pair<int, int64_t>> frames;
    {
      std::lock_guard lock(capture_mutex);
      frames.swap(session->captured);
      capture_status = session->capture_status;
    }

    if (capture_status != C2D_STATUS_OK) {
      fail(util::string_format("C2D blit returned %d", capture_status));
      return;
    }

    for (const auto &[input, timestamp_us] : frames) {
      if (!queueBuffer(session->fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, input, session->inputs[input], timestamp_us)) {
        fail(error("VIDIOC_QBUF"));
        return;
      }
    }
  }

  void returnInput() {
    v4l2_buffer buffer;
    v4l2_plane plane = {};
    if (!dequeueBuffer(session->fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, buffer, plane)) {
      fail(error("VIDIOC_DQBUF"));
      return;
    }

    std::lock_guard lock(capture_mutex);
    session->free_inputs.push_back(buffer.index);
  }

  void receivePacket() {
    v4l2_buffer buffer;
    v4l2_plane plane = {};
    if (!dequeueBuffer(session->fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, buffer, plane)) {
      fail(error("VIDIOC_DQBUF"));
      return;
    }

    if (buffer.flags & (V4L2_BUF_FLAG_ERROR | V4L2_QCOM_BUF_DATA_CORRUPT)) {
      fail(util::string_format("encoder returned a corrupt packet (flags %#x)", buffer.flags));
      return;
    }

    const uint8_t *data = static_cast<uint8_t *>(session->outputs[buffer.index].addr) + plane.data_offset;
    if (buffer.flags & V4L2_QCOM_BUF_FLAG_CODECCONFIG) {
      session->codec_config.assign(data, data + plane.bytesused);
    } else {
      handlePacket(data, plane.bytesused, buffer.timestamp.tv_sec * 1000000LL + buffer.timestamp.tv_usec, buffer.flags & V4L2_BUF_FLAG_KEYFRAME);
    }

    if (!queueBuffer(session->fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, buffer.index, session->outputs[buffer.index], 0)) {
      fail(error("VIDIOC_QBUF"));
    }
  }

  void handlePacket(const uint8_t *data, size_t size, int64_t timestamp_us, bool keyframe) {
    if (size > 0 && replay_seconds > 0) {
      replay.append(data, size, timestamp_us, keyframe);
    }

    if (recording && size > 0 && !recording->write(data, size, timestamp_us, keyframe)) {
      finishRecording(false);
      manual_request = false;
    }
  }

  void startRecording() {
    std::unique_ptr<Mp4File> file = std::make_unique<Mp4File>();
    if (!file->open(session->width, session->height, session->codec_config, false)) {
      notify(cereal::FrogPilotOnroadEvent::EventName::RECORDING_START_FAILED);
      manual_request = false;
      return;
    }

    v4l2_control keyframe = {.id = V4L2_CID_MPEG_VIDC_VIDEO_REQUEST_IFRAME};
    if (util::safe_ioctl(session->fd, VIDIOC_S_CTRL, &keyframe) != 0) {
      LOGE("screen recorder: keyframe request failed: %s", strerror(errno));
    }

    recording = std::move(file);
    notify(cereal::FrogPilotOnroadEvent::EventName::RECORDING_STARTED);
  }

  void finishRecording(bool keep) {
    bool saved = keep && recording->finish();
    notify(saved ? cereal::FrogPilotOnroadEvent::EventName::RECORDING_SAVED : cereal::FrogPilotOnroadEvent::EventName::RECORDING_FAILED);
    recording.reset();
  }

  void startSave() {
    if (!replay.ready()) {
      notify(cereal::FrogPilotOnroadEvent::EventName::REPLAY_FAILED);
      return;
    }

    save = std::make_unique<ReplaySave>();
    if (!save->file.open(session->width, session->height, session->codec_config, true)) {
      notify(cereal::FrogPilotOnroadEvent::EventName::REPLAY_FAILED);
      save.reset();
      return;
    }

    save->frames = replay.snapshot();
    replay_saving = true;
  }

  void continueSave() {
    for (size_t i = 0; i < SAVE_FRAMES_PER_STEP && save->next < save->frames.size(); i++) {
      ReplayBuffer::Frame &frame = save->frames[save->next++];
      if (!save->file.write(frame->data.data(), frame->data.size(), frame->timestamp_us, frame->keyframe)) {
        finishSave(false);
        return;
      }
      frame.reset();
    }

    if (save->next == save->frames.size()) {
      finishSave(save->file.finish());
    }
  }

  void finishSave(bool saved) {
    notify(saved ? cereal::FrogPilotOnroadEvent::EventName::REPLAY_SAVED : cereal::FrogPilotOnroadEvent::EventName::REPLAY_FAILED);
    save.reset();
    replay_saving = false;
  }

  void notify(cereal::FrogPilotOnroadEvent::EventName event) {
    runOnUIThread(frogpilotUIState(), [event]() {
      frogpilotUIState()->screenRecorderEvent(event);
    });
  }

  bool failed = false;

  int replay_seconds = 0;

  ReplayBuffer replay;

  std::unique_ptr<Mp4File> recording;
  std::unique_ptr<ReplaySave> save;
  std::unique_ptr<Session> session;
};

void shutdownRecorder() {
  quit_request = true;
  wake();
  recorder_thread.join();
}
}

void ScreenRecorder::attach() {
  wake_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (wake_fd < 0) {
    LOGE("screen recorder: eventfd failed: %s", strerror(errno));
    return;
  }

  recorder_thread = std::thread([] {
    Recorder().run();
  });
  proxyCreateWrapper = reinterpret_cast<void *(*)(void *)>(dlsym(RTLD_DEFAULT, "wl_proxy_create_wrapper"));
  proxyWrapperDestroy = reinterpret_cast<void (*)(void *)>(dlsym(RTLD_DEFAULT, "wl_proxy_wrapper_destroy"));
  dl_iterate_phdr(patchDriverImports, nullptr);

  QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, shutdownRecorder);
}

void ScreenRecorder::start() {
  if (!manual_request.exchange(true)) {
    wake();
  }
}

void ScreenRecorder::stop() {
  if (manual_request.exchange(false)) {
    wake();
  }
}

bool ScreenRecorder::active() {
  return manual_request;
}

void ScreenRecorder::setReplayDuration(int seconds) {
  if (replay_request.exchange(seconds) != seconds) {
    wake();
  }
}

bool ScreenRecorder::replayReady() {
  return replay_ready;
}

bool ScreenRecorder::replaySaving() {
  return replay_saving || save_request;
}

void ScreenRecorder::saveReplay() {
  if (!replay_saving && !save_request.exchange(true)) {
    wake();
  }
}

#else

void ScreenRecorder::attach() {}
void ScreenRecorder::start() {}
void ScreenRecorder::stop() {}
bool ScreenRecorder::active() { return false; }
void ScreenRecorder::setReplayDuration(int) {}
bool ScreenRecorder::replayReady() { return false; }
bool ScreenRecorder::replaySaving() { return false; }
void ScreenRecorder::saveReplay() {}

#endif
