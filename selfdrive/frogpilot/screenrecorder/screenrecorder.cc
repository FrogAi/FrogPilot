#include "libyuv.h"

#include "selfdrive/ui/qt/util.h"

#include "selfdrive/frogpilot/screenrecorder/screenrecorder.h"

namespace {
  long long currentMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  }

  uint64_t nanosSinceBoot() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
  }
}

ScreenRecorder::ScreenRecorder(QWidget *parent) : QPushButton(parent) {
  setFixedSize(btn_size, btn_size);

  std::thread encoderInitThread([this]() {
    encoder = std::make_unique<OmxEncoder>("/data/media/screen_recordings", screenWidth, screenHeight, UI_FREQ, 8 * 1024 * 1024);
    encoderReady = true;
  });
  encoderInitThread.detach();

  rgbScaleBuffer = std::make_unique<uint8_t[]>(screenWidth * screenHeight * 4);

  QObject::connect(this, &QPushButton::clicked, this, &ScreenRecorder::toggleRecording);
}

ScreenRecorder::~ScreenRecorder() {
  stopRecording();
}

void ScreenRecorder::toggleRecording() {
  if (recording) {
    stopRecording();
  } else {
    startRecording();
  }
}

void ScreenRecorder::startRecording() {
  if (recording || !encoderReady) {
    std::cerr << "Recording already in progress or encoder not ready." << std::endl;
    return;
  }

  recording = true;

  rootWidget = this;
  while (rootWidget->parentWidget()) {
    rootWidget = rootWidget->parentWidget();
  }

  QString filename = QDateTime::currentDateTime().toString("MMMM_dd_yyyy-hh:mmAP") + ".mp4";
  openEncoder(filename.toStdString());
  encodingThread = std::thread(&ScreenRecorder::encodingThreadFunction, this);
  startedTime = currentMilliseconds();
}

void ScreenRecorder::stopRecording() {
  if (!recording) {
    return;
  }

  recording = false;

  if (encodingThread.joinable()) {
    encodingThread.join();
  }

  closeEncoder();
  imageQueue.clear();
  rgbScaleBuffer = std::make_unique<uint8_t[]>(screenWidth * screenHeight * 4);
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
  uint64_t startTime = nanosSinceBoot();

  while (recording) {
    QImage image;
    if (imageQueue.pop_wait_for(image, std::chrono::milliseconds(10))) {
      QImage convertedImage = image.convertToFormat(QImage::Format_RGBA8888);
      libyuv::ARGBScale(
        convertedImage.bits(),
        convertedImage.width() * 4,
        convertedImage.width(),
        convertedImage.height(),
        rgbScaleBuffer.get(),
        screenWidth * 4,
        screenWidth,
        screenHeight,
        libyuv::kFilterBilinear
      );
      encoder->encode_frame_rgba(
        rgbScaleBuffer.get(),
        screenWidth,
        screenHeight,
        nanosSinceBoot() - startTime
      );
    }
  }
}

void ScreenRecorder::updateScreen(double fps, bool started) {
  if (!recording) {
    return;
  }

  if (!started) {
    stopRecording();
    return;
  }

  static long long recordingDurationLimit = 1000 * 60 * 3;
  if (currentMilliseconds() - startedTime > recordingDurationLimit) {
    stopRecording();
    startRecording();
    return;
  }

  if (rootWidget) {
    imageQueue.push(rootWidget->grab().toImage());
  }
}

void ScreenRecorder::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  if (recording) {
    painter.setPen(QPen(redColor(), 6));
    painter.setBrush(redColor(166));
    painter.setFont(InterFont(25, QFont::Bold));
  } else {
    painter.setPen(QPen(redColor(), 6));
    painter.setBrush(blackColor(166));
    painter.setFont(InterFont(25, QFont::DemiBold));
  }

  int centeringOffset = 10;
  QRect buttonRect(centeringOffset, btn_size / 3, btn_size - centeringOffset * 2, btn_size / 3);
  painter.drawRoundedRect(buttonRect, 24, 24);

  QRect textRect = buttonRect.adjusted(centeringOffset, 0, -centeringOffset, 0);
  painter.setPen(QPen(whiteColor(), 6));
  painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, tr("RECORD"));

  if (recording && ((currentMilliseconds() - startedTime) / 1000) % 2 == 0) {
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPoint(buttonRect.right() - btn_size / 10 - centeringOffset, buttonRect.center().y()), btn_size / 10, btn_size / 10);
  }
}
