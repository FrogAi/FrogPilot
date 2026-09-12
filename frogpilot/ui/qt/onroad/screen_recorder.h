#pragma once

#include <QObject>

#ifdef QCOM2
#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <exception>
#include <mutex>
#include <string>
#include <sys/time.h>
#include <thread>
#include <vector>

#include "msgq/visionipc/visionbuf.h"

struct AVFormatContext;
struct AVStream;
struct dl_phdr_info;
struct v4l2_buffer;
struct v4l2_plane;
struct wl_interface;
struct wl_proxy;
union wl_argument;
#endif

class ScreenRecorder : public QObject {
  Q_OBJECT

public:
  explicit ScreenRecorder(QObject *parent = nullptr);
  ~ScreenRecorder();

  bool active() const;

  void attach();
  void start();
  void stop();

signals:
  void stateChanged();

private:
#ifdef QCOM2
  void captureFrame(wl_proxy *buffer);
  void configureEncoder();
  void createSurfaces();
  void dequeueFrames();
  void openRecording();
  void queueBuffer(uint32_t type, unsigned int index, VisionBuf &buffer, timeval timestamp = {});
  void recordingFailed(const std::exception &error);
  void requestBuffers(uint32_t type, unsigned int count);
  void writePacket(uint8_t *data, size_t size, int64_t timestampUs, uint32_t flags);

  v4l2_buffer dequeueBuffer(uint32_t type, v4l2_plane *plane);

  static int patchDriverImports(dl_phdr_info *object, size_t size, void *data);

  static void checkResult(bool success, const char *operation);
  static void marshalRequest(wl_proxy *proxy, uint32_t opcode, ...);
  static void readArguments(const char *signature, wl_argument *arguments, va_list argumentList);

  static const wl_interface *proxyInterface(wl_proxy *proxy);

  static wl_proxy *marshalConstructor(wl_proxy *proxy, uint32_t opcode, const wl_interface *interface, ...);

  static constexpr char ENCODER_DEVICE[] = "/dev/v4l/by-path/platform-aa00000.qcom_vidc-video-index1";
  static constexpr char IN_PROGRESS_DIR[] = "/data/media/screen_recordings.in_progress";
  static constexpr char LOCK_PATH[] = "/data/media/screen_recordings.lock";
  static constexpr char RECORDINGS_DIR[] = "/data/media/screen_recordings";

  static constexpr int COLOR_SPACE_BT601_625 = 5;  // C2D produces limited-range BT.601.
  static constexpr int ENCODER_INPUT_BUFFERS = 3;
  static constexpr int ENCODER_OUTPUT_BUFFERS = 2;
  static constexpr int FRAME_QP = 22;
  static constexpr int MAX_FRAME_RATE = 30;
  static constexpr int MAX_REQUEST_ARGUMENTS = 20;  // The driver's longest request has 12 arguments.

  static constexpr uint32_t KGSL_USER_MEM_TYPE_ION = 3;
  static constexpr uint32_t V4L2_QCOM_BUF_FLAG_CODECCONFIG = 0x00020000;
  static constexpr uint32_t V4L2_QCOM_BUF_FLAG_EOS = 0x02000000;

  struct SwapchainBuffer {
    int fd;

    size_t size;

    uint32_t height;
    uint32_t stride;
    uint32_t width;

    wl_proxy *waylandBuffer;
  };

  struct Source {
    SwapchainBuffer buffer;

    uint32_t surface = 0;

    void *gpu = nullptr;
    void *map = nullptr;
  };

  struct Target {
    uint32_t surface = 0;

    void *gpu = nullptr;
  };

  bool c2dInitialized = false;
  bool headerWritten = false;

  int encoderFd = -1;
  int lockFd = -1;

  int64_t lastCaptureUs = 0;

  std::atomic<bool> failed = false;
  std::atomic<bool> recording = false;
  std::atomic<bool> stopping = false;

  std::mutex recorderMutex;

  std::string finalPath;
  std::string recordingPath;

  std::thread encoderThread;

  std::vector<unsigned int> freeInputs;
  std::vector<Source> sources;
  std::vector<SwapchainBuffer> swapchain;

  uint32_t height = 0;
  uint32_t width = 0;

  AVFormatContext *mp4 = nullptr;
  AVStream *stream = nullptr;

  Target targets[ENCODER_INPUT_BUFFERS] = {};

  VisionBuf inputs[ENCODER_INPUT_BUFFERS] = {};
  VisionBuf outputs[ENCODER_OUTPUT_BUFFERS] = {};

  wl_proxy *attachedBuffer = nullptr;
#endif
};

ScreenRecorder *screenRecorder();
