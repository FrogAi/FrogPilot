#include "frogpilot/ui/screenrecorder/recorder_engine.h"

#include <chrono>

#include <sys/statvfs.h>

#include <QDateTime>
#include <QDir>

#include "common/swaglog.h"
#include "common/timing.h"
#include "common/util.h"

#include "third_party/libyuv/include/libyuv.h"

namespace {
const QString RECORDINGS_DIR = "/data/media/screen_recordings";
constexpr uint64_t MAX_SEGMENT_NS = 5ULL * 60 * 1000000000ULL;
constexpr uint64_t MIN_FREE_SPACE_BYTES = 1ULL << 30;
}

RecorderEngine::RecorderEngine(int width, int height, int fps, int bitrate) : bitrate(bitrate), fps(fps), height(height), width(width) {}

RecorderEngine::~RecorderEngine() {
  stop();
}

const QImage &RecorderEngine::blend_frames(const QImage &a, const QImage &b) {
  if (blend_buf.size() != a.size() || blend_buf.format() != a.format()) {
    blend_buf = QImage(a.size(), a.format());
  }

  libyuv::ARGBInterpolate(a.constBits(), a.bytesPerLine(),
                          b.constBits(), b.bytesPerLine(),
                          blend_buf.bits(), blend_buf.bytesPerLine(),
                          a.width(), a.height(), 128);

  return blend_buf;
}

bool RecorderEngine::open_segment() {
  struct statvfs vfs;
  if (statvfs(RECORDINGS_DIR.toStdString().c_str(), &vfs) == 0) {
    uint64_t free_bytes = (uint64_t)vfs.f_bavail * vfs.f_frsize;
    if (free_bytes < MIN_FREE_SPACE_BYTES) {
      LOGE("screenrecorder: not enough free space to record (%llu MB free)",
           (unsigned long long)(free_bytes >> 20));
      return false;
    }
  }

  encoder = std::make_unique<ScreenEncoder>(width, height, fps, bitrate);

  if (!encoder->open(segment_path())) {
    encoder.reset();
    return false;
  }

  segment_start_ns = nanos_since_boot();

  return true;
}

std::string RecorderEngine::segment_path() const {
  QString name = QDateTime::currentDateTime().toString("MMMM_dd_yyyy-hh-mm-ssAP") + ".mp4";
  return (RECORDINGS_DIR + "/" + name).toStdString();
}

bool RecorderEngine::start() {
  if (recording) {
    return true;
  }

  if (worker.joinable()) {
    worker.join();
  }

  QDir().mkpath(RECORDINGS_DIR);
  if (!open_segment()) {
    LOGE("screenrecorder: failed to start encoder");
    return false;
  }

  recording = true;

  worker = std::thread(&RecorderEngine::worker_loop, this);

  return true;
}

void RecorderEngine::stop() {
  recording = false;

  q_cv.notify_all();

  if (worker.joinable()) {
    worker.join();
  }

  {
    std::lock_guard<std::mutex> lk(q_mutex);
    queue.clear();
  }
  encoder.reset();
}

void RecorderEngine::submit_frame(QImage &&frame, uint64_t ts_ns) {
  if (!recording) {
    return;
  }

  {
    std::lock_guard<std::mutex> lk(q_mutex);
    if (queue.size() >= MAX_QUEUE) {
      queue.pop_front();
    }
    queue.push_back({std::move(frame), ts_ns});
  }
  q_cv.notify_one();
}

void RecorderEngine::worker_loop() {
  util::set_thread_name("sr-capture");

  QImage prev_image;

  uint64_t prev_ts = 0;

  while (recording) {
    CapturedFrame cf;
    {
      std::unique_lock<std::mutex> lk(q_mutex);
      q_cv.wait_for(lk, std::chrono::milliseconds(100),
                    [this] { return !queue.empty() || !recording; });

      if (!recording) {
        break;
      }

      if (queue.empty()) {
        continue;
      }

      cf = std::move(queue.front());
      queue.pop_front();
    }

    if (nanos_since_boot() - segment_start_ns > MAX_SEGMENT_NS) {
      encoder.reset();

      if (!open_segment()) {
        LOGE("screenrecorder: segment rotation failed, stopping");
        recording = false;
        break;
      }

      prev_image = QImage();

      prev_ts = 0;
    }

    if (!encoder || !encoder->ok()) {
      recording = false;
      break;
    }

    if (cf.image.format() != QImage::Format_RGB32) {
      cf.image = cf.image.convertToFormat(QImage::Format_RGB32);
    }

    if (cf.image.width() != width || cf.image.height() != height) {
      if (!size_warned) {
        LOGW("screenrecorder: grabbed frame %dx%d != configured %dx%d, rescaling",
             cf.image.width(), cf.image.height(), width, height);
        size_warned = true;
      }
      cf.image = cf.image.scaled(width, height, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }

    if (!prev_image.isNull() && prev_image.size() == cf.image.size()) {
      uint64_t mid_ts = prev_ts + (cf.ts_ns - prev_ts) / 2;
      const QImage &mid = blend_frames(prev_image, cf.image);
      encoder->encode_frame(mid.constBits(), mid.bytesPerLine(), mid_ts);
    }
    encoder->encode_frame(cf.image.constBits(), cf.image.bytesPerLine(), cf.ts_ns);

    prev_image = cf.image;
    prev_ts = cf.ts_ns;
  }
}
