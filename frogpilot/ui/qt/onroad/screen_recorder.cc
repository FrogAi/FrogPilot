#include "frogpilot/ui/qt/onroad/screen_recorder.h"

#ifdef QCOM2

#include <cstdio>
#include <cstring>
#include <ctime>
#include <elf.h>
#include <fcntl.h>
#include <filesystem>
#include <link.h>
#include <poll.h>
#include <stdexcept>
#include <sys/file.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

// The Qualcomm controls must be included before the system V4L2 header.
#include "third_party/linux/include/v4l2-controls.h"
#include <linux/videodev2.h>

#include "common/swaglog.h"
#include "common/timing.h"
#include "common/util.h"
#include "third_party/c2d2/c2d2.h"
#include "third_party/linux/include/msm_media_info.h"

// These driver functions are exported by libC2D2.so but absent from its header.
extern "C" C2D_STATUS c2dDriverInit(C2D_DRIVER_SETUP_INFO *setup);
extern "C" C2D_STATUS c2dDriverDeInit(void);

#endif

ScreenRecorder::ScreenRecorder(QObject *parent) : QObject(parent) {}

ScreenRecorder::~ScreenRecorder() {
  stop();
}

ScreenRecorder *screenRecorder() {
  static ScreenRecorder recorder;
  return &recorder;
}

#ifdef QCOM2

bool ScreenRecorder::active() const {
  return recording;
}

void ScreenRecorder::attach() {
  int cleanupLock = open(LOCK_PATH, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0664);
  if (cleanupLock >= 0) {
    if (flock(cleanupLock, LOCK_EX | LOCK_NB) == 0) {
      std::error_code error;
      std::filesystem::remove_all(IN_PROGRESS_DIR, error);
    }
    close(cleanupLock);
  }

  dl_iterate_phdr(patchDriverImports, nullptr);
}

void ScreenRecorder::start() {
  if (recording) {
    return;
  }

  try {
    {
      std::lock_guard lock(recorderMutex);
      for (const SwapchainBuffer &buffer : swapchain) {
        sources.push_back({.buffer = buffer});
      }
    }
    if (sources.empty()) {
      return;
    }

    height = sources[0].buffer.height;
    width = sources[0].buffer.width;

    lockFd = open(LOCK_PATH, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0664);
    if (flock(lockFd, LOCK_EX | LOCK_NB) != 0) {
      stop();
      return;
    }

    std::filesystem::remove_all(IN_PROGRESS_DIR);
    util::create_directories(RECORDINGS_DIR, 0775);
    util::create_directories(IN_PROGRESS_DIR, 0775);

    char name[32];
    time_t now = time(nullptr);
    strftime(name, sizeof(name), "%Y-%m-%d_%H-%M-%S", localtime(&now));
    recordingPath = util::string_format("%s/%s.mp4.partial", IN_PROGRESS_DIR, name);
    finalPath = util::string_format("%s/%s.mp4", RECORDINGS_DIR, name);
    for (int suffix = 1; std::filesystem::exists(finalPath); ++suffix) {
      finalPath = util::string_format("%s/%s_%d.mp4", RECORDINGS_DIR, name, suffix);
    }

    openRecording();
    encoderFd = HANDLE_EINTR(open(ENCODER_DEVICE, O_RDWR | O_NONBLOCK | O_CLOEXEC));
    checkResult(encoderFd >= 0, "cannot open screen encoder");
    configureEncoder();
    createSurfaces();

    {
      std::lock_guard lock(recorderMutex);
      recording = true;
    }
    emit stateChanged();
  } catch (const std::exception &error) {
    LOGE("screen recorder: %s", error.what());
    failed = true;
    stop();
  }
}

void ScreenRecorder::stop() {
  bool wasRecording;
  {
    std::lock_guard lock(recorderMutex);
    wasRecording = recording;
    recording = false;
  }

  if (encoderThread.joinable()) {
    stopping = true;
    v4l2_encoder_cmd stopCommand = {.cmd = V4L2_ENC_CMD_STOP};
    if (util::safe_ioctl(encoderFd, VIDIOC_ENCODER_CMD, &stopCommand) < 0) {
      failed = true;
      LOGE("screen recorder: cannot stop encoder");
    }
    encoderThread.join();
  }
  if (encoderFd >= 0) {
    for (v4l2_buf_type type : {V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE}) {
      util::safe_ioctl(encoderFd, VIDIOC_STREAMOFF, &type);
    }
    close(encoderFd);
    encoderFd = -1;
  }

  if (mp4) {
    bool complete = headerWritten && !failed;
    if (headerWritten && av_write_trailer(mp4) < 0) {
      complete = false;
    }
    if (mp4->pb && avio_closep(&mp4->pb) < 0) {
      complete = false;
    }
    avformat_free_context(mp4);
    mp4 = nullptr;
    stream = nullptr;
    if (complete) {
      if (rename(recordingPath.c_str(), finalPath.c_str()) != 0) {
        LOGE("screen recorder: cannot save %s", finalPath.c_str());
      }
    } else {
      unlink(recordingPath.c_str());
    }
  }

  for (Source &source : sources) {
    if (source.surface) {
      c2dDestroySurface(source.surface);
    }
    if (source.gpu) {
      c2dUnMapAddr(source.gpu);
    }
    if (source.map && source.map != MAP_FAILED) {
      munmap(source.map, source.buffer.size);
    }
  }
  for (int i = 0; i < ENCODER_INPUT_BUFFERS; i++) {
    if (targets[i].surface) {
      c2dDestroySurface(targets[i].surface);
    }
    if (targets[i].gpu) {
      c2dUnMapAddr(targets[i].gpu);
    }
    targets[i] = {};
    if (inputs[i].addr) {
      inputs[i].free();
      inputs[i] = {};
    }
  }
  for (int i = 0; i < ENCODER_OUTPUT_BUFFERS; i++) {
    if (outputs[i].addr) {
      outputs[i].free();
      outputs[i] = {};
    }
  }
  if (c2dInitialized) {
    c2dDriverDeInit();
    c2dInitialized = false;
  }
  if (lockFd >= 0) {
    close(lockFd);
    lockFd = -1;
  }

  freeInputs.clear();
  sources.clear();

  failed = false;
  headerWritten = false;
  height = 0;
  lastCaptureUs = 0;
  stopping = false;
  width = 0;

  if (wasRecording) {
    emit stateChanged();
  }
}

// Capture before the compositor can release the completed frame for reuse.
void ScreenRecorder::captureFrame(wl_proxy *waylandBuffer) {
  std::lock_guard lock(recorderMutex);
  if (!recording || failed) {
    return;
  }

  try {
    const Source *source = nullptr;
    for (const Source &candidate : sources) {
      if (candidate.buffer.waylandBuffer == waylandBuffer) {
        source = &candidate;
      }
    }

    int64_t timestampUs = nanos_since_boot() / 1000;
    if (!source || timestampUs - lastCaptureUs < 1000000 / MAX_FRAME_RATE || freeInputs.empty()) {
      return;
    }
    unsigned int input = freeInputs.back();
    freeInputs.pop_back();

    C2D_OBJECT blit = {};
    blit.surface_id = source->surface;
    blit.config_mask = C2D_SOURCE_RECT_BIT | C2D_TARGET_RECT_BIT | C2D_NO_BILINEAR_BIT | C2D_NO_ANTIALIASING_BIT | C2D_ALPHA_BLEND_NONE;
    blit.source_rect = {0, 0, static_cast<int32>(width << 16), static_cast<int32>(height << 16)};
    blit.target_rect = blit.source_rect;
    checkResult(c2dDraw(targets[input].surface, 0, nullptr, 0, 0, &blit, 1) == C2D_STATUS_OK, "cannot convert recording frame");
    checkResult(c2dFinish(targets[input].surface) == C2D_STATUS_OK, "cannot finish recording frame");

    lastCaptureUs = timestampUs;
    queueBuffer(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, input, inputs[input],
                {static_cast<time_t>(timestampUs / 1000000), static_cast<suseconds_t>(timestampUs % 1000000)});
    if (!encoderThread.joinable()) {
      encoderThread = std::thread(&ScreenRecorder::dequeueFrames, this);  // the encoder only reports end of stream once it has had a frame
    }
  } catch (const std::exception &error) {
    recordingFailed(error);
  }
}

void ScreenRecorder::checkResult(bool success, const char *operation) {
  if (!success) {
    throw std::runtime_error(operation);
  }
}

void ScreenRecorder::configureEncoder() {
  v4l2_format encoded = {
    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
    .fmt = {.pix_mp = {.width = width, .height = height, .pixelformat = V4L2_PIX_FMT_H264, .field = V4L2_FIELD_ANY,
                       .colorspace = V4L2_COLORSPACE_DEFAULT}},
  };
  util::safe_ioctl(encoderFd, VIDIOC_S_FMT, &encoded, "VIDIOC_S_FMT failed");
  v4l2_streamparm frameRate = {.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, .parm = {.output = {.timeperframe = {1, MAX_FRAME_RATE}}}};
  util::safe_ioctl(encoderFd, VIDIOC_S_PARM, &frameRate, "VIDIOC_S_PARM failed");
  v4l2_format raw = {
    .type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
    .fmt = {.pix_mp = {.width = width, .height = height, .pixelformat = V4L2_PIX_FMT_NV12, .field = V4L2_FIELD_ANY,
                       .colorspace = V4L2_COLORSPACE_470_SYSTEM_BG}},
  };
  util::safe_ioctl(encoderFd, VIDIOC_S_FMT, &raw, "VIDIOC_S_FMT failed");

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
    util::safe_ioctl(encoderFd, VIDIOC_S_CTRL, &control, "VIDIOC_S_CTRL failed");
  }

  requestBuffers(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, ENCODER_OUTPUT_BUFFERS);
  requestBuffers(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, ENCODER_INPUT_BUFFERS);
  for (v4l2_buf_type type : {V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE}) {
    util::safe_ioctl(encoderFd, VIDIOC_STREAMON, &type, "VIDIOC_STREAMON failed");
  }

  // the ION buffers are zeroed by the CPU on allocation; clean those cache lines before the hardware owns them
  for (int i = 0; i < ENCODER_OUTPUT_BUFFERS; i++) {
    outputs[i].allocate(encoded.fmt.pix_mp.plane_fmt[0].sizeimage);
    outputs[i].sync(VISIONBUF_SYNC_TO_DEVICE);
    queueBuffer(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, i, outputs[i]);
  }

  for (int i = 0; i < ENCODER_INPUT_BUFFERS; i++) {
    inputs[i].allocate(raw.fmt.pix_mp.plane_fmt[0].sizeimage);
    inputs[i].sync(VISIONBUF_SYNC_TO_DEVICE);
    freeInputs.push_back(i);
  }
}

void ScreenRecorder::createSurfaces() {
  C2D_DRIVER_SETUP_INFO setup = {.max_surface_template_needed = 8};
  checkResult(c2dDriverInit(&setup) == C2D_STATUS_OK, "cannot initialize C2D");
  c2dInitialized = true;

  for (Source &source : sources) {
    source.map = mmap(nullptr, source.buffer.size, PROT_READ | PROT_WRITE, MAP_SHARED, source.buffer.fd, 0);
    checkResult(source.map != MAP_FAILED, "cannot map recording source");
    checkResult(c2dMapAddr(source.buffer.fd, source.map, source.buffer.size, 0, KGSL_USER_MEM_TYPE_ION, &source.gpu) == C2D_STATUS_OK,
          "cannot map recording source to GPU");
    C2D_RGB_SURFACE_DEF rgba = {
      .format = C2D_COLOR_FORMAT_8888_ARGB | C2D_FORMAT_SWAP_RB | C2D_FORMAT_UBWC_COMPRESSED,  // R, G, B, A bytes
      .width = source.buffer.width,
      .height = source.buffer.height,
      .buffer = source.map,
      .phys = source.gpu,
      .stride = static_cast<int32>(source.buffer.stride),
    };
    checkResult(c2dCreateSurface(&source.surface, C2D_SOURCE,
                                static_cast<C2D_SURFACE_TYPE>(C2D_SURFACE_RGB_HOST | C2D_SURFACE_WITH_PHYS), &rgba) == C2D_STATUS_OK,
          "cannot create recording source");
  }

  uint32_t uvOffset = VENUS_Y_STRIDE(COLOR_FMT_NV12, width) * VENUS_Y_SCANLINES(COLOR_FMT_NV12, height);
  for (int i = 0; i < ENCODER_INPUT_BUFFERS; i++) {
    VisionBuf &input = inputs[i];
    Target &target = targets[i];
    checkResult(c2dMapAddr(input.fd, input.addr, input.len, 0, KGSL_USER_MEM_TYPE_ION, &target.gpu) == C2D_STATUS_OK, "cannot map encoder input");
    C2D_YUV_SURFACE_DEF nv12 = {
      .format = C2D_COLOR_FORMAT_420_NV12,
      .width = width,
      .height = height,
      .plane0 = input.addr,
      .phys0 = target.gpu,
      .stride0 = static_cast<int32>(VENUS_Y_STRIDE(COLOR_FMT_NV12, width)),
      .plane1 = static_cast<uint8_t *>(input.addr) + uvOffset,
      .phys1 = static_cast<uint8_t *>(target.gpu) + uvOffset,
      .stride1 = static_cast<int32>(VENUS_UV_STRIDE(COLOR_FMT_NV12, width)),
    };
    checkResult(c2dCreateSurface(&target.surface, C2D_TARGET,
                                static_cast<C2D_SURFACE_TYPE>(C2D_SURFACE_YUV_HOST | C2D_SURFACE_WITH_PHYS), &nv12) == C2D_STATUS_OK,
          "cannot create encoder input");
  }
}

v4l2_buffer ScreenRecorder::dequeueBuffer(uint32_t type, v4l2_plane *plane) {
  v4l2_buffer buffer = {.type = type, .memory = V4L2_MEMORY_USERPTR, .m = {.planes = plane}, .length = 1};
  util::safe_ioctl(encoderFd, VIDIOC_DQBUF, &buffer, "VIDIOC_DQBUF failed");
  return buffer;
}

void ScreenRecorder::dequeueFrames() {
  util::set_thread_name("screen_recorder");

  try {
    pollfd encoderPoll = {.fd = encoderFd, .events = POLLIN | POLLOUT};
    while (!failed) {
      int ready = HANDLE_EINTR(poll(&encoderPoll, 1, 1000));
      checkResult(ready >= 0 && !(encoderPoll.revents & (POLLERR | POLLHUP | POLLNVAL)), "encoder poll failed");
      checkResult(ready || !stopping, "encoder did not finish");
      if (encoderPoll.revents & POLLOUT) {
        v4l2_plane plane = {};
        v4l2_buffer frame = dequeueBuffer(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, &plane);
        std::lock_guard lock(recorderMutex);
        freeInputs.push_back(frame.index);
      }
      if (encoderPoll.revents & POLLIN) {
        v4l2_plane plane = {};
        v4l2_buffer packet = dequeueBuffer(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, &plane);
        if (packet.flags & V4L2_QCOM_BUF_FLAG_EOS) {
          return;
        }
        VisionBuf &data = outputs[packet.index];
        data.sync(VISIONBUF_SYNC_FROM_DEVICE);
        writePacket(static_cast<uint8_t *>(data.addr), plane.bytesused, packet.timestamp.tv_sec * 1000000LL + packet.timestamp.tv_usec, packet.flags);
        queueBuffer(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, packet.index, data);
      }
    }
  } catch (const std::exception &error) {
    recordingFailed(error);
  }
}

void ScreenRecorder::openRecording() {
  checkResult(avformat_alloc_output_context2(&mp4, nullptr, "mp4", recordingPath.c_str()) >= 0, "cannot allocate MP4 context");
  stream = avformat_new_stream(mp4, nullptr);
  checkResult(stream, "cannot allocate MP4 stream");
  stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
  stream->codecpar->codec_id = AV_CODEC_ID_H264;
  stream->codecpar->width = width;
  stream->codecpar->height = height;
  stream->codecpar->format = AV_PIX_FMT_YUV420P;
  stream->time_base = {1, 1000000};
  mp4->avoid_negative_ts = AVFMT_AVOID_NEG_TS_MAKE_ZERO;

  checkResult(avio_open(&mp4->pb, recordingPath.c_str(), AVIO_FLAG_WRITE) >= 0, "cannot open recording file");
}

// Adreno uses private Wayland requests and RTLD_DEEPBIND, so its imports must be patched directly.
int ScreenRecorder::patchDriverImports(dl_phdr_info *object, size_t, void *) {
  if (!strstr(object->dlpi_name, "libeglSubDriverWayland")) {
    return 0;
  }

  const ElfW(Sym) *symbols = nullptr;
  const char *strings = nullptr;
  const ElfW(Rela) *relocations = nullptr;
  size_t relocationsSize = 0;
  for (int i = 0; i < object->dlpi_phnum; i++) {
    if (object->dlpi_phdr[i].p_type != PT_DYNAMIC) {
      continue;
    }
    for (const ElfW(Dyn) *dynamicEntry = reinterpret_cast<const ElfW(Dyn) *>(object->dlpi_addr + object->dlpi_phdr[i].p_vaddr);
         dynamicEntry->d_tag != DT_NULL; dynamicEntry++) {
      if (dynamicEntry->d_tag == DT_SYMTAB) {
        symbols = reinterpret_cast<const ElfW(Sym) *>(dynamicEntry->d_un.d_ptr);
      } else if (dynamicEntry->d_tag == DT_STRTAB) {
        strings = reinterpret_cast<const char *>(dynamicEntry->d_un.d_ptr);
      } else if (dynamicEntry->d_tag == DT_JMPREL) {
        relocations = reinterpret_cast<const ElfW(Rela) *>(dynamicEntry->d_un.d_ptr);
      } else if (dynamicEntry->d_tag == DT_PLTRELSZ) {
        relocationsSize = dynamicEntry->d_un.d_val;
      }
    }
  }

  for (size_t i = 0; i < relocationsSize / sizeof(ElfW(Rela)); i++) {
    const char *symbol = strings + symbols[ELF64_R_SYM(relocations[i].r_info)].st_name;
    void *hook = nullptr;
    if (strcmp(symbol, "wl_proxy_marshal") == 0) {
      hook = reinterpret_cast<void *>(marshalRequest);
    } else if (strcmp(symbol, "wl_proxy_marshal_constructor") == 0) {
      hook = reinterpret_cast<void *>(marshalConstructor);
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

const wl_interface *ScreenRecorder::proxyInterface(wl_proxy *proxy) {
  return *reinterpret_cast<const wl_interface *const *>(proxy);  // struct wl_proxy starts with struct wl_object { const wl_interface *interface; ... }
}

void ScreenRecorder::queueBuffer(uint32_t type, unsigned int index, VisionBuf &visionBuffer, timeval timestamp) {
  v4l2_plane plane = {};
  plane.bytesused = static_cast<uint32_t>(visionBuffer.len);
  plane.length = static_cast<uint32_t>(visionBuffer.len);
  plane.m.userptr = reinterpret_cast<unsigned long>(visionBuffer.addr);
  plane.reserved[0] = static_cast<unsigned int>(visionBuffer.fd);

  v4l2_buffer buffer = {};
  buffer.index = index;
  buffer.type = type;
  buffer.flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
  buffer.timestamp = timestamp;
  buffer.memory = V4L2_MEMORY_USERPTR;
  buffer.m.planes = &plane;
  buffer.length = 1;

  util::safe_ioctl(encoderFd, VIDIOC_QBUF, &buffer, "VIDIOC_QBUF failed");
}

// Optional markers and version digits in Wayland signatures do not consume an argument.
void ScreenRecorder::readArguments(const char *signature, wl_argument *arguments, va_list argumentList) {
  int count = 0;
  for (const char *type = signature; *type; type++) {
    if (*type == 'i') {
      arguments[count++].i = va_arg(argumentList, int32_t);
    } else if (*type == 'u') {
      arguments[count++].u = va_arg(argumentList, uint32_t);
    } else if (*type == 'f') {
      arguments[count++].f = va_arg(argumentList, wl_fixed_t);
    } else if (*type == 's') {
      arguments[count++].s = va_arg(argumentList, const char *);
    } else if (*type == 'o' || *type == 'n') {
      arguments[count++].o = va_arg(argumentList, wl_object *);
    } else if (*type == 'a') {
      arguments[count++].a = va_arg(argumentList, wl_array *);
    } else if (*type == 'h') {
      arguments[count++].h = va_arg(argumentList, int32_t);
    }
  }
}

void ScreenRecorder::recordingFailed(const std::exception &error) {
  LOGE("screen recorder: %s", error.what());
  failed = true;

  QMetaObject::invokeMethod(this, [this] {
    if (recording && failed) {
      stop();
    }
  }, Qt::QueuedConnection);
}

void ScreenRecorder::requestBuffers(uint32_t type, unsigned int count) {
  v4l2_requestbuffers request = {.count = count, .type = type, .memory = V4L2_MEMORY_USERPTR};
  util::safe_ioctl(encoderFd, VIDIOC_REQBUFS, &request, "VIDIOC_REQBUFS failed");
}

void ScreenRecorder::marshalRequest(wl_proxy *proxy, uint32_t opcode, ...) {
  const wl_interface *interface = proxyInterface(proxy);
  wl_argument arguments[MAX_REQUEST_ARGUMENTS];
  va_list argumentList;
  va_start(argumentList, opcode);
  readArguments(interface->methods[opcode].signature, arguments, argumentList);
  va_end(argumentList);

  if (strcmp(interface->name, "wl_surface") == 0) {
    ScreenRecorder *recorder = screenRecorder();
    if (opcode == WL_SURFACE_ATTACH) {
      recorder->attachedBuffer = reinterpret_cast<wl_proxy *>(arguments[0].o);
    } else if (opcode == WL_SURFACE_COMMIT) {
      recorder->captureFrame(recorder->attachedBuffer);  // before the compositor can release the buffer for reuse
      recorder->attachedBuffer = nullptr;
    }
  }
  wl_proxy_marshal_array(proxy, opcode, arguments);
}

wl_proxy *ScreenRecorder::marshalConstructor(wl_proxy *proxy, uint32_t opcode, const wl_interface *interface, ...) {
  const wl_message &request = proxyInterface(proxy)->methods[opcode];
  wl_argument arguments[MAX_REQUEST_ARGUMENTS];
  va_list argumentList;
  va_start(argumentList, interface);
  readArguments(request.signature, arguments, argumentList);
  va_end(argumentList);

  wl_proxy *created = wl_proxy_marshal_array_constructor(proxy, opcode, arguments, interface);
  if (strcmp(proxyInterface(proxy)->name, "wayland_buffer_backend") == 0 && strcmp(request.name, "create_buffer") == 0) {
    SwapchainBuffer buffer;
    buffer.fd = fcntl(arguments[1].h, F_DUPFD_CLOEXEC, 0);
    buffer.height = arguments[4].u;
    buffer.size = static_cast<size_t>(lseek(buffer.fd, 0, SEEK_END));
    buffer.stride = arguments[6].u;
    buffer.width = arguments[3].u;
    buffer.waylandBuffer = created;

    ScreenRecorder *recorder = screenRecorder();
    std::lock_guard lock(recorder->recorderMutex);
    recorder->swapchain.push_back(buffer);
  }
  return created;
}

void ScreenRecorder::writePacket(uint8_t *data, size_t size, int64_t timestampUs, uint32_t flags) {
  if (flags & V4L2_QCOM_BUF_FLAG_CODECCONFIG) {
    stream->codecpar->extradata = static_cast<uint8_t *>(av_mallocz(size + AV_INPUT_BUFFER_PADDING_SIZE));
    checkResult(stream->codecpar->extradata, "cannot allocate H264 header");
    memcpy(stream->codecpar->extradata, data, size);
    stream->codecpar->extradata_size = size;
    checkResult(avformat_write_header(mp4, nullptr) >= 0, "cannot write MP4 header");
    headerWritten = true;
    return;
  }

  if (!headerWritten) {
    return;
  }

  AVPacket packet = {};
  packet.data = data;
  packet.size = size;
  packet.stream_index = stream->index;
  packet.pts = packet.dts = timestampUs;
  packet.duration = 1000000 / MAX_FRAME_RATE;  // only the last frame keeps this; the others get the real gap
  packet.flags = (flags & V4L2_BUF_FLAG_KEYFRAME) ? AV_PKT_FLAG_KEY : 0;
  checkResult(av_write_frame(mp4, &packet) >= 0 && mp4->pb->error >= 0, "cannot write recording frame");
}

#else

bool ScreenRecorder::active() const {
  return false;
}

void ScreenRecorder::attach() {}
void ScreenRecorder::start() {}
void ScreenRecorder::stop() {}

#endif
