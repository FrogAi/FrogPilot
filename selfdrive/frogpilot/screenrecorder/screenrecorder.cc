#include "libyuv.h"

#include "selfdrive/ui/qt/util.h"

#include "selfdrive/frogpilot/screenrecorder/screenrecorder.h"

namespace {
  inline long long milliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  }
}

ScreenRecorder::ScreenRecorder(QWidget *parent) : QPushButton(parent), recording(false) {
  setFixedSize(btn_size, btn_size);

  encoder = std::make_unique<OmxEncoder>("/data/media/screen_recordings", screenWidth, screenHeight, UI_FREQ, 8 * 1024 * 1024);
  rgbScaleBuffer = std::make_unique<uint8_t[]>(screenWidth * screenHeight * 4);

  QObject::connect(this, &QPushButton::clicked, this, &ScreenRecorder::toggleRecording);
}

ScreenRecorder::~ScreenRecorder() {
  stop();
}

void ScreenRecorder::toggleRecording() {
  recording ? stop() : start();
}

void ScreenRecorder::start() {
  if (recording) {
    return;
  }

  recording = true;

  rootWidget = this;
  while (rootWidget->parentWidget()) {
    rootWidget = rootWidget->parentWidget();
  }

  QString filename = QDateTime::currentDateTime().toString("MMMM_dd_yyyy-hh:mmAP") + ".mp4";
  try {
    openEncoder(filename.toStdString());
    encodingThread = std::thread(&ScreenRecorder::encodingThreadFunction, this);
  } catch (const std::exception &e) {
    std::cerr << "Error starting encoder: " << e.what() << std::endl;
    recording = false;
  }

  started_time = milliseconds();
}

void ScreenRecorder::stop() {
  if (!recording) {
    return;
  }

  recording = false;

  if (encodingThread.joinable()) {
    encodingThread.join();
  }

  try {
    closeEncoder();
  } catch (const std::exception &e) {
    std::cerr << "Error stopping encoder: " << e.what() << std::endl;
  }

  imageQueue.clear();
  rgbScaleBuffer.reset(new uint8_t[screenWidth * screenHeight * 4]);
}

void ScreenRecorder::openEncoder(const std::string &filename) {
  if (encoder) {
    encoder->encoder_open(filename.c_str());
  }
}

void ScreenRecorder::closeEncoder() {
  if (encoder) {
    encoder->encoder_close();
  }
}

void ScreenRecorder::encodingThreadFunction() {
  uint64_t start_time = nanos_since_boot();

  while (recording) {
    if (!encoder) {
      break;
    }

    QImage popImage;
    if (imageQueue.pop_wait_for(popImage, std::chrono::milliseconds(10))) {
      QImage image = popImage.convertToFormat(QImage::Format_RGBA8888);
      libyuv::ARGBScale(image.bits(), image.width() * 4, image.width(), image.height(),
                        rgbScaleBuffer.get(), screenWidth * 4, screenWidth, screenHeight,
                        libyuv::kFilterBilinear);
      encoder->encode_frame_rgba(rgbScaleBuffer.get(), screenWidth, screenHeight, nanos_since_boot() - start_time);
    }
  }
}

void ScreenRecorder::updateScreen(double fps, bool started) {
  if (!recording) {
    return;
  }

  if (!started) {
    stop();
    return;
  }

  if (milliseconds() - started_time > 1000 * 60 * 3) {
    stop();
    start();
    return;
  }

  static bool previousFrameSkipped = false;
  if (fps < UI_FREQ && !previousFrameSkipped) {
    previousFrameSkipped = true;
    return;
  } else {
    previousFrameSkipped = false;
  }

  if (rootWidget) {
    imageQueue.push(rootWidget->grab().toImage());
  }
}

void ScreenRecorder::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  int centeringOffset = 10;

  QRect buttonRect(centeringOffset, btn_size / 3, btn_size - centeringOffset * 2, btn_size / 3);
  painter.setPen(QPen(Qt::red, 6));
  painter.drawRoundedRect(buttonRect, 15, 15);

  QRect textRect = buttonRect.adjusted(centeringOffset, 0, -centeringOffset, 0);
  painter.setFont(InterFont(25, QFont::Bold));
  painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, tr("RECORD"));

  if (recording && ((milliseconds() - started_time) / 1000) % 2 == 0) {
    painter.setBrush(QColor(255, 0, 0));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPoint(buttonRect.right() - btn_size / 10 - centeringOffset, buttonRect.center().y()), btn_size / 10, btn_size / 10);
  }
}
