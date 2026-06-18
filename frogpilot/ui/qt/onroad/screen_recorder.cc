#include "frogpilot/ui/qt/onroad/screen_recorder.h"

#ifdef QCOM2

#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

// has to be in this order
#include "third_party/linux/include/v4l2-controls.h"
#include <linux/videodev2.h>

#include "common/swaglog.h"
#include "common/timing.h"
#include "common/util.h"
#include "msgq/visionipc/visionbuf.h"
#include "third_party/c2d2/c2d2.h"
#include "third_party/linux/include/msm_media_info.h"

// exported by libC2D2.so but missing from its header; c2dMapAddr fails until the driver is initialized
extern "C" C2D_STATUS c2dDriverInit(C2D_DRIVER_SETUP_INFO *setup);
extern "C" C2D_STATUS c2dDriverDeInit(void);

namespace {
const char RECORDINGS_DIR[] = "/data/media/screen_recordings";
const char IN_PROGRESS_DIR[] = "/data/media/screen_recordings.in_progress";
const char LOCK_PATH[] = "/data/media/screen_recordings.lock";
const char ENCODER_DEVICE[] = "/dev/v4l/by-path/platform-aa00000.qcom_vidc-video-index1";

const int ENCODER_BUFFERS = 3;
const int MAX_FRAME_RATE = 30;  // the UI commits in bursts of up to ~60 per second
const int FRAME_QP = 22;
const int COLOR_SPACE_BT601_625 = 5;  // MSM_VIDC_BT601_6_625 (kernel uapi msm_vidc.h): what C2D's limited-range conversion produces
const int MAX_REQUEST_ARGUMENTS = 20;  // the driver's longest request (create_planar_buffer) has 12
const uint32_t KGSL_USER_MEM_TYPE_ION = 3;  // third_party/linux/include/msm_kgsl.h
const uint32_t V4L2_QCOM_BUF_FLAG_CODECCONFIG = 0x00020000;
const uint32_t V4L2_QCOM_BUF_FLAG_EOS = 0x02000000;

// The Adreno EGL Wayland sub-driver (libeglSubDriverWayland.so) creates the window's four RGBA8888 UBWC swapchain buffers with its
// private wayland_buffer_backend.create_buffer request and attaches and commits them from its own updater thread once the GPU has
// finished rendering a frame. It is loaded RTLD_DEEPBIND, so its libwayland-client imports are redirected here through its GOT.
struct SwapchainBuffer {
  wl_proxy *wl_buffer;
  int fd;
  size_t size;
  uint32_t width, height, stride;
};

std::mutex recorder_mutex;
std::vector<SwapchainBuffer> swapchain;
wl_proxy *attached_buffer = nullptr;
std::atomic<bool> recording = false;  // read every paint by the button, so it must not take recorder_mutex

void capture(wl_proxy *wl_buffer);

const wl_interface *proxyInterface(wl_proxy *proxy) {
  return *reinterpret_cast<const wl_interface *const *>(proxy);  // struct wl_proxy starts with struct wl_object { const wl_interface *interface; ... }
}

// libwayland's wl_argument_from_va_list is not exported; '?' and version digits carry no argument
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
      capture(attached_buffer);  // before the compositor can release the buffer for reuse
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

  wl_proxy *created = wl_proxy_marshal_array_constructor(proxy, opcode, args, interface);
  if (strcmp(proxyInterface(proxy)->name, "wayland_buffer_backend") == 0 && strcmp(request.name, "create_buffer") == 0) {
    int fd = fcntl(args[1].h, F_DUPFD_CLOEXEC, 0);  // create_buffer(new wl_buffer, ion_fd, ion_metadata_fd, width, height, format, stride)
    std::lock_guard lock(recorder_mutex);
    swapchain.push_back({created, fd, static_cast<size_t>(lseek(fd, 0, SEEK_END)), args[3].u, args[4].u, args[6].u});
  }
  return created;
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
    }

    if (hook) {
      void **slot = reinterpret_cast<void **>(object->dlpi_addr + relocations[i].r_offset);
      void *page = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(slot) & ~static_cast<uintptr_t>(getpagesize() - 1));
      mprotect(page, getpagesize(), PROT_READ | PROT_WRITE);  // the driver is linked BIND_NOW + RELRO
      *slot = hook;
    }
  }
  return 1;
}

struct Session {
  int lock_fd;
  std::string path;
  std::string final_path;
  uint32_t width, height;

  struct Source {
    SwapchainBuffer buffer;
    void *map;
    void *gpu;
    uint32_t surface;
  };
  std::vector<Source> sources;
  struct Target {
    void *gpu;
    uint32_t surface;
  } targets[ENCODER_BUFFERS];

  int encoder_fd;
  VisionBuf inputs[ENCODER_BUFFERS];
  VisionBuf outputs[ENCODER_BUFFERS];
  std::vector<unsigned int> free_inputs;
  std::thread dequeue_thread;

  AVFormatContext *mp4 = nullptr;
  AVStream *stream = nullptr;
  bool header_written = false;
  int64_t last_capture_us = 0;
};

Session *session = nullptr;

bool openMp4(Session &s) {
  avformat_alloc_output_context2(&s.mp4, nullptr, nullptr, s.path.c_str());
  s.stream = avformat_new_stream(s.mp4, nullptr);
  s.stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
  s.stream->codecpar->codec_id = AV_CODEC_ID_H264;
  s.stream->codecpar->width = s.width;
  s.stream->codecpar->height = s.height;
  s.stream->codecpar->format = AV_PIX_FMT_YUV420P;
  s.stream->time_base = {1, 1000000};
  s.mp4->avoid_negative_ts = AVFMT_AVOID_NEG_TS_MAKE_ZERO;

  if (avio_open(&s.mp4->pb, s.path.c_str(), AVIO_FLAG_WRITE) < 0) {
    LOGE("screen recorder: cannot create %s", s.path.c_str());
    avformat_free_context(s.mp4);
    return false;
  }
  return true;
}

void writePacket(Session &s, uint8_t *data, size_t size, int64_t timestamp_us, uint32_t flags) {
  if (flags & V4L2_QCOM_BUF_FLAG_CODECCONFIG) {
    s.stream->codecpar->extradata = static_cast<uint8_t *>(av_mallocz(size + AV_INPUT_BUFFER_PADDING_SIZE));
    memcpy(s.stream->codecpar->extradata, data, size);
    s.stream->codecpar->extradata_size = size;
    if (avformat_write_header(s.mp4, nullptr) < 0) {
      return;
    }
    s.header_written = true;
    return;
  }
  if (!s.header_written) {
    return;
  }
  AVPacket packet = {};
  packet.data = data;
  packet.size = size;
  packet.stream_index = s.stream->index;
  packet.pts = packet.dts = timestamp_us;
  packet.duration = 1000000 / MAX_FRAME_RATE;  // only the last frame keeps this; the others get the real gap
  packet.flags = (flags & V4L2_BUF_FLAG_KEYFRAME) ? AV_PKT_FLAG_KEY : 0;
  av_write_frame(s.mp4, &packet);
}

void queueBuffer(int fd, v4l2_buf_type type, unsigned int index, VisionBuf &buf, timeval timestamp = {}) {
  v4l2_plane plane = {
    .bytesused = static_cast<uint32_t>(buf.len),
    .length = static_cast<uint32_t>(buf.len),
    .m = {.userptr = reinterpret_cast<unsigned long>(buf.addr)},
    .reserved = {static_cast<unsigned int>(buf.fd)},
  };
  v4l2_buffer buffer = {
    .index = index,
    .type = type,
    .flags = V4L2_BUF_FLAG_TIMESTAMP_COPY,
    .timestamp = timestamp,
    .memory = V4L2_MEMORY_USERPTR,
    .m = {.planes = &plane},
    .length = 1,
  };
  util::safe_ioctl(fd, VIDIOC_QBUF, &buffer, "VIDIOC_QBUF failed");
}

v4l2_buffer dequeueBuffer(int fd, v4l2_buf_type type, v4l2_plane *plane) {
  v4l2_buffer buffer = {.type = type, .memory = V4L2_MEMORY_USERPTR, .m = {.planes = plane}, .length = 1};
  util::safe_ioctl(fd, VIDIOC_DQBUF, &buffer, "VIDIOC_DQBUF failed");
  return buffer;
}

void requestBuffers(int fd, v4l2_buf_type type, unsigned int count) {
  v4l2_requestbuffers request = {.count = count, .type = type, .memory = V4L2_MEMORY_USERPTR};
  util::safe_ioctl(fd, VIDIOC_REQBUFS, &request, "VIDIOC_REQBUFS failed");
}

void configureEncoder(Session &s) {
  v4l2_format encoded = {
    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
    .fmt = {.pix_mp = {.width = s.width, .height = s.height, .pixelformat = V4L2_PIX_FMT_H264, .field = V4L2_FIELD_ANY, .colorspace = V4L2_COLORSPACE_DEFAULT}},
  };
  util::safe_ioctl(s.encoder_fd, VIDIOC_S_FMT, &encoded, "VIDIOC_S_FMT failed");
  v4l2_streamparm frame_rate = {.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, .parm = {.output = {.timeperframe = {1, MAX_FRAME_RATE}}}};
  util::safe_ioctl(s.encoder_fd, VIDIOC_S_PARM, &frame_rate, "VIDIOC_S_PARM failed");
  v4l2_format raw = {
    .type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
    .fmt = {.pix_mp = {.width = s.width, .height = s.height, .pixelformat = V4L2_PIX_FMT_NV12, .field = V4L2_FIELD_ANY,
                       .colorspace = V4L2_COLORSPACE_470_SYSTEM_BG}},
  };
  util::safe_ioctl(s.encoder_fd, VIDIOC_S_FMT, &raw, "VIDIOC_S_FMT failed");

  v4l2_control controls[] = {
    {.id = V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL, .value = V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_OFF},
    {.id = V4L2_CID_MPEG_VIDC_VIDEO_I_FRAME_QP, .value = FRAME_QP},
    {.id = V4L2_CID_MPEG_VIDC_VIDEO_P_FRAME_QP, .value = FRAME_QP},
    {.id = V4L2_CID_MPEG_VIDC_VIDEO_B_FRAME_QP, .value = FRAME_QP},
    {.id = V4L2_CID_MPEG_VIDC_VIDEO_QP_MASK, .value = 7},
    {.id = V4L2_CID_MPEG_VIDC_VIDEO_NUM_P_FRAMES, .value = MAX_FRAME_RATE - 1},
    {.id = V4L2_CID_MPEG_VIDC_VIDEO_NUM_B_FRAMES, .value = 0},
    {.id = V4L2_CID_MPEG_VIDC_VIDEO_IDR_PERIOD, .value = 1},
    {.id = V4L2_CID_MPEG_VIDEO_HEADER_MODE, .value = V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE},
    {.id = V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY, .value = V4L2_MPEG_VIDC_VIDEO_PRIORITY_REALTIME_DISABLE},
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
    util::safe_ioctl(s.encoder_fd, VIDIOC_S_CTRL, &control, "VIDIOC_S_CTRL failed");
  }

  requestBuffers(s.encoder_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, ENCODER_BUFFERS);
  requestBuffers(s.encoder_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, ENCODER_BUFFERS);
  for (v4l2_buf_type type : {V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE}) {
    util::safe_ioctl(s.encoder_fd, VIDIOC_STREAMON, &type, "VIDIOC_STREAMON failed");
  }

  // the ION buffers are zeroed by the CPU on allocation; clean those cache lines before the hardware owns them
  for (int i = 0; i < ENCODER_BUFFERS; i++) {
    s.outputs[i].allocate(encoded.fmt.pix_mp.plane_fmt[0].sizeimage);
    s.outputs[i].sync(VISIONBUF_SYNC_TO_DEVICE);
    queueBuffer(s.encoder_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, i, s.outputs[i]);
    s.inputs[i].allocate(raw.fmt.pix_mp.plane_fmt[0].sizeimage);
    s.inputs[i].sync(VISIONBUF_SYNC_TO_DEVICE);
    s.free_inputs.push_back(i);
  }
}

void closeEncoder(Session &s) {
  for (v4l2_buf_type type : {V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE}) {
    util::safe_ioctl(s.encoder_fd, VIDIOC_STREAMOFF, &type, "VIDIOC_STREAMOFF failed");
    requestBuffers(s.encoder_fd, type, 0);
  }
  close(s.encoder_fd);
}

void dequeueLoop(Session *s) {
  util::set_thread_name("screen_recorder");
  pollfd pfd = {.fd = s->encoder_fd, .events = POLLIN | POLLOUT};
  while (true) {
    if (poll(&pfd, 1, -1) < 0) {
      continue;
    }
    if (pfd.revents & POLLOUT) {
      v4l2_plane plane = {};
      v4l2_buffer frame = dequeueBuffer(s->encoder_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, &plane);
      std::lock_guard lock(recorder_mutex);
      s->free_inputs.push_back(frame.index);
    }
    if (pfd.revents & POLLIN) {
      v4l2_plane plane = {};
      v4l2_buffer packet = dequeueBuffer(s->encoder_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, &plane);
      if (packet.flags & V4L2_QCOM_BUF_FLAG_EOS) {
        return;
      }
      VisionBuf &data = s->outputs[packet.index];
      data.sync(VISIONBUF_SYNC_FROM_DEVICE);
      writePacket(*s, static_cast<uint8_t *>(data.addr), plane.bytesused, packet.timestamp.tv_sec * 1000000LL + packet.timestamp.tv_usec, packet.flags);
      queueBuffer(s->encoder_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, packet.index, data);
    }
  }
}

void createSurfaces(Session &s) {
  C2D_DRIVER_SETUP_INFO setup = {.max_surface_template_needed = 8};
  c2dDriverInit(&setup);

  for (Session::Source &source : s.sources) {
    source.map = mmap(nullptr, source.buffer.size, PROT_READ | PROT_WRITE, MAP_SHARED, source.buffer.fd, 0);
    c2dMapAddr(source.buffer.fd, source.map, source.buffer.size, 0, KGSL_USER_MEM_TYPE_ION, &source.gpu);
    C2D_RGB_SURFACE_DEF rgba = {
      .format = C2D_COLOR_FORMAT_8888_ARGB | C2D_FORMAT_SWAP_RB | C2D_FORMAT_UBWC_COMPRESSED,  // R, G, B, A bytes
      .width = source.buffer.width,
      .height = source.buffer.height,
      .buffer = source.map,
      .phys = source.gpu,
      .stride = static_cast<int32>(source.buffer.stride),
    };
    c2dCreateSurface(&source.surface, C2D_SOURCE, static_cast<C2D_SURFACE_TYPE>(C2D_SURFACE_RGB_HOST | C2D_SURFACE_WITH_PHYS), &rgba);
  }

  uint32_t uv_offset = VENUS_Y_STRIDE(COLOR_FMT_NV12, s.width) * VENUS_Y_SCANLINES(COLOR_FMT_NV12, s.height);
  for (int i = 0; i < ENCODER_BUFFERS; i++) {
    VisionBuf &input = s.inputs[i];
    Session::Target &target = s.targets[i];
    c2dMapAddr(input.fd, input.addr, input.len, 0, KGSL_USER_MEM_TYPE_ION, &target.gpu);
    C2D_YUV_SURFACE_DEF nv12 = {
      .format = C2D_COLOR_FORMAT_420_NV12,
      .width = s.width,
      .height = s.height,
      .plane0 = input.addr,
      .phys0 = target.gpu,
      .stride0 = static_cast<int32>(VENUS_Y_STRIDE(COLOR_FMT_NV12, s.width)),
      .plane1 = static_cast<uint8_t *>(input.addr) + uv_offset,
      .phys1 = static_cast<uint8_t *>(target.gpu) + uv_offset,
      .stride1 = static_cast<int32>(VENUS_UV_STRIDE(COLOR_FMT_NV12, s.width)),
    };
    c2dCreateSurface(&target.surface, C2D_TARGET, static_cast<C2D_SURFACE_TYPE>(C2D_SURFACE_YUV_HOST | C2D_SURFACE_WITH_PHYS), &nv12);
  }
}

void destroySurfaces(Session &s) {
  for (Session::Source &source : s.sources) {
    c2dDestroySurface(source.surface);
    c2dUnMapAddr(source.gpu);
    munmap(source.map, source.buffer.size);
  }
  for (Session::Target &target : s.targets) {
    c2dDestroySurface(target.surface);
    c2dUnMapAddr(target.gpu);
  }
  c2dDriverDeInit();
}

// driver thread, at every wl_surface.commit: the frame is complete and the compositor cannot reuse the buffer yet
void capture(wl_proxy *wl_buffer) {
  std::lock_guard lock(recorder_mutex);
  if (!session) {
    return;
  }
  const Session::Source *source = nullptr;
  for (const Session::Source &candidate : session->sources) {
    if (candidate.buffer.wl_buffer == wl_buffer) {
      source = &candidate;
    }
  }

  int64_t timestamp_us = nanos_since_boot() / 1000;
  if (!source || timestamp_us - session->last_capture_us < 1000000 / MAX_FRAME_RATE || session->free_inputs.empty()) {
    return;
  }
  unsigned int input = session->free_inputs.back();
  session->free_inputs.pop_back();

  C2D_OBJECT blit = {};
  blit.surface_id = source->surface;
  blit.config_mask = C2D_SOURCE_RECT_BIT | C2D_TARGET_RECT_BIT | C2D_NO_BILINEAR_BIT | C2D_NO_ANTIALIASING_BIT | C2D_ALPHA_BLEND_NONE;
  blit.source_rect = {0, 0, static_cast<int32>(session->width << 16), static_cast<int32>(session->height << 16)};
  blit.target_rect = blit.source_rect;
  c2dDraw(session->targets[input].surface, 0, nullptr, 0, 0, &blit, 1);
  c2dFinish(session->targets[input].surface);

  session->last_capture_us = timestamp_us;
  if (!session->dequeue_thread.joinable()) {
    session->dequeue_thread = std::thread(dequeueLoop, session);  // the encoder only reports end of stream once it has had a frame
  }
  queueBuffer(session->encoder_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, input, session->inputs[input],
              {static_cast<time_t>(timestamp_us / 1000000), static_cast<suseconds_t>(timestamp_us % 1000000)});
}
}

void ScreenRecorder::attach() {
  std::filesystem::remove_all(IN_PROGRESS_DIR);  // a recording cut short by a ui crash has no MP4 index
  dl_iterate_phdr(patchDriverImports, nullptr);
}

void ScreenRecorder::start() {
  Session *s = new Session();
  {
    std::lock_guard lock(recorder_mutex);
    for (const SwapchainBuffer &buffer : swapchain) {
      s->sources.push_back({.buffer = buffer});
    }
  }
  s->width = s->sources[0].buffer.width;
  s->height = s->sources[0].buffer.height;

  s->lock_fd = open(LOCK_PATH, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0664);
  if (flock(s->lock_fd, LOCK_EX | LOCK_NB) != 0) {  // the DATA panel is deleting or renaming recordings
    close(s->lock_fd);
    delete s;
    return;
  }

  char name[32];
  time_t now = time(nullptr);
  strftime(name, sizeof(name), "%Y-%m-%d_%H-%M-%S.mp4", localtime(&now));
  util::create_directories(RECORDINGS_DIR, 0775);
  util::create_directories(IN_PROGRESS_DIR, 0775);
  s->path = util::string_format("%s/%s", IN_PROGRESS_DIR, name);
  s->final_path = util::string_format("%s/%s", RECORDINGS_DIR, name);
  if (!openMp4(*s)) {
    close(s->lock_fd);
    delete s;
    return;
  }

  s->encoder_fd = HANDLE_EINTR(open(ENCODER_DEVICE, O_RDWR | O_NONBLOCK | O_CLOEXEC));
  configureEncoder(*s);
  createSurfaces(*s);

  std::lock_guard lock(recorder_mutex);
  session = s;
  recording = true;
}

void ScreenRecorder::stop() {
  recording = false;
  Session *s;
  {
    std::lock_guard lock(recorder_mutex);
    s = session;
    session = nullptr;
  }
  if (!s) {
    return;
  }

  if (s->dequeue_thread.joinable()) {
    v4l2_encoder_cmd stop_command = {.cmd = V4L2_ENC_CMD_STOP};
    util::safe_ioctl(s->encoder_fd, VIDIOC_ENCODER_CMD, &stop_command, "VIDIOC_ENCODER_CMD failed");
    s->dequeue_thread.join();
  }

  if (s->header_written) {
    av_write_trailer(s->mp4);
  }
  avio_closep(&s->mp4->pb);
  avformat_free_context(s->mp4);
  if (s->header_written) {
    rename(s->path.c_str(), s->final_path.c_str());
  } else {
    unlink(s->path.c_str());
  }

  destroySurfaces(*s);
  closeEncoder(*s);
  for (int i = 0; i < ENCODER_BUFFERS; i++) {
    s->inputs[i].free();
    s->outputs[i].free();
  }
  close(s->lock_fd);
  delete s;
}

bool ScreenRecorder::active() {
  return recording;
}

#else

void ScreenRecorder::attach() {}
void ScreenRecorder::start() {}
void ScreenRecorder::stop() {}
bool ScreenRecorder::active() { return false; }

#endif
