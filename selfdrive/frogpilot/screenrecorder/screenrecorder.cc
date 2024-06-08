#include "libyuv.h"

#include "selfdrive/frogpilot/screenrecorder/screenrecorder.h"
#include "selfdrive/ui/qt/util.h"

static long long milliseconds() {
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  return static_cast<long long>(t.tv_sec * 1000.0 + t.tv_nsec * 1e-6);
}

ScreenRecorder::ScreenRecorder(QWidget *parent) : QPushButton(parent), image_queue(30), recording(false), started(0), frame(0) {
  setFixedSize(192 / 2 + 25, 192 / 2);
  setFocusPolicy(Qt::NoFocus);

  screen_height = 1080;
  screen_width = 2160;
  recording_height = 720;
  recording_width = (screen_width * recording_height) / screen_height + (recording_width % 2);

  rgb_scale_buffer = std::make_unique<uint8_t[]>(recording_width * recording_height * 4);

  connect(this, &QPushButton::released, this, &ScreenRecorder::toggle);

  initializeEncoder();
}

void ScreenRecorder::initializeEncoder() {
  const std::string path = "/data/media/0/videos";
  encoder = std::make_unique<OmxEncoder>(path.c_str(), recording_width, recording_height, 60, 2 * 1024 * 1024, false, false);
}

ScreenRecorder::~ScreenRecorder() {
  stop();
}

void ScreenRecorder::applyColor() {
  if (frame % (UI_FREQ / 2) == 0) {
    recording_color = (frame % UI_FREQ < (UI_FREQ / 2)) ? QColor::fromRgbF(1, 1, 1, 1) : QColor::fromRgbF(0, 0, 0, 1);
    update();
  }
}

void ScreenRecorder::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  QRect fullRect(0, 0, 192 / 2, 192 / 2);
  QColor outerColor = recording ? recording_color : QColor::fromRgbF(1, 1, 1, 1);
  int outerRedWidth = fullRect.width() * 0.05;

  QRect outerRect = fullRect;
  p.setBrush(outerColor);
  p.setPen(Qt::NoPen);
  p.drawEllipse(outerRect);

  QRect middleRect = fullRect.marginsRemoved(QMargins(outerRedWidth, outerRedWidth, outerRedWidth, outerRedWidth));
  p.setBrush(QColor::fromRgbF(1, 0, 0, 1));
  p.drawEllipse(middleRect);
}

void ScreenRecorder::openEncoder(const char *filename) {
  encoder->encoder_open(filename);
}

void ScreenRecorder::closeEncoder() {
  if (encoder) {
    encoder->encoder_close();
  }
}

void ScreenRecorder::toggle() {
  if (!recording) {
    start();
  } else {
    stop();
  }
}

void ScreenRecorder::start() {
  if (recording) return;

  char filename[64];
  const time_t t = time(NULL);
  const struct tm tm = *localtime(&t);
  snprintf(filename, sizeof(filename), "%04d%02d%02d-%02d%02d%02d.mp4",
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
           tm.tm_hour, tm.tm_min, tm.tm_sec);

  recording = true;
  frame = 0;
  QWidget *widget = this;
  while (widget->parentWidget() != nullptr) {
    widget = widget->parentWidget();
  }
  rootWidget = widget;
  openEncoder(filename);
  encoding_thread = std::thread(&ScreenRecorder::encoding_thread_func, this);

  update();
  started = milliseconds();
}

void ScreenRecorder::encoding_thread_func() {
  const uint64_t start_time = nanos_since_boot();
  while (recording && encoder) {
    QImage popImage;
    if (image_queue.pop_wait_for(popImage, std::chrono::milliseconds(10))) {
      const QImage image = popImage.convertToFormat(QImage::Format_RGBA8888);
      libyuv::ARGBScale(image.bits(), image.width() * 4,
                        image.width(), image.height(),
                        rgb_scale_buffer.get(), recording_width * 4,
                        recording_width, recording_height,
                        libyuv::kFilterLinear);
      encoder->encode_frame_rgba(rgb_scale_buffer.get(), recording_width, recording_height,
                                 static_cast<uint64_t>(nanos_since_boot() - start_time));
    }
  }
}

void ScreenRecorder::stop() {
  if (!recording) return;

  recording = false;
  update();

  if (encoding_thread.joinable()) {
    encoding_thread.join();
  }
  closeEncoder();
  image_queue.clear();
}

void ScreenRecorder::update_screen() {
  if (!uiState()->scene.started) {
    if (recording) {
      stop();
    }
    return;
  }
  if (!recording) return;

  if (milliseconds() - started > 1000 * 60 * 3) {
    stop();
    start();
    return;
  }

  applyColor();
  if (rootWidget != nullptr) {
    const QPixmap pixmap = rootWidget->grab();
    image_queue.push(pixmap.toImage());
  }
  frame++;
}
