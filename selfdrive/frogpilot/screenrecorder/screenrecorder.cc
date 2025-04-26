#include "libyuv.h"

#include "selfdrive/ui/qt/util.h"

#include "selfdrive/frogpilot/screenrecorder/screenrecorder.h"

constexpr int MAX_DURATION = 1000 * 60 * 5;

constexpr int SCREEN_WIDTH = 2160;
constexpr int SCREEN_HEIGHT = 1080;

const QDir RECORDINGS_FOLDER("/data/media/screen_recordings");

ScreenRecorder::ScreenRecorder(QWidget *parent) : QPushButton(parent) {
  setFixedSize(btn_size, btn_size);

  encoder = std::make_unique<OmxEncoder>(RECORDINGS_FOLDER.path().toStdString().c_str(), SCREEN_WIDTH, SCREEN_HEIGHT, UI_FREQ * 2, 12 * 1024 * 1024);

  rgbScaleBuffer.resize(SCREEN_WIDTH * SCREEN_HEIGHT * 4);

  rootWidget = topWidget(this);

  QObject::connect(this, &QPushButton::clicked, this, &ScreenRecorder::toggleRecording);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &ScreenRecorder::stopRecording);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &ScreenRecorder::updateState);
}

ScreenRecorder::~ScreenRecorder() {
  stopRecording();
}

void ScreenRecorder::updateState() {
  if (!recording) {
    return;
  }

  if (QDateTime::currentMSecsSinceEpoch() - startedTime > MAX_DURATION) {
    stopRecording();
    startRecording();
    return;
  }

  if (frameCount % 2 == 0) {
    imageQueue.push(rootWidget->grab().toImage());
  }

  frameCount += 1;
}

void ScreenRecorder::toggleRecording() {
  recording ? stopRecording() : startRecording();
}

void ScreenRecorder::startRecording() {
  encoder->encoder_open((QDateTime::currentDateTime().toString("MMMM_dd_yyyy-hh-mmAP").toStdString() + ".mp4").c_str());

  recording = true;

  frameCount = 0;

  startedTime = QDateTime::currentMSecsSinceEpoch();

  encodingThread = std::thread(&ScreenRecorder::encodeImage, this);
}

void ScreenRecorder::stopRecording() {
  recording = false;

  if (encodingThread.joinable()) {
    encodingThread.join();
  }

  encoder->encoder_close();
}

QImage ScreenRecorder::synthesizeFrame(const QImage &frame1, const QImage &frame2, double alpha) {
  QImage blended(frame1.size(), frame1.format());

  const uint8_t *bits1 = frame1.constBits();
  const uint8_t *bits2 = frame2.constBits();

  uint8_t *blendedBits = blended.bits();

  int numPixels = frame1.width() * frame1.height();

  for (int i = 0; i < numPixels * 4; ++i) {
    blendedBits[i] = bits1[i] * (1.0 - alpha) + bits2[i] * alpha;
  }

  return blended;
}

void ScreenRecorder::encodeImage() {
  uint64_t previousTimestamp = 0;

  QImage previousImage;

  while (recording) {
    uint64_t currentTimestamp = nanos_since_boot();

    QImage image;

    if (imageQueue.pop_wait_for(image, std::chrono::milliseconds(1000 / UI_FREQ))) {
      image = image.convertToFormat(QImage::Format_RGBA8888);

      if (!previousImage.isNull()) {
        double alpha = std::clamp((currentTimestamp - previousTimestamp) / (1000.0 / UI_FREQ), 0.0, 1.0);

        QImage syntheticImage = synthesizeFrame(previousImage, image, alpha);

        std::copy(syntheticImage.bits(), syntheticImage.bits() + SCREEN_WIDTH * SCREEN_HEIGHT * 4, rgbScaleBuffer.data());
        encoder->encode_frame_rgba(rgbScaleBuffer.data(), SCREEN_WIDTH, SCREEN_HEIGHT, (previousTimestamp + currentTimestamp) / 2);
      }

      std::copy(image.bits(), image.bits() + SCREEN_WIDTH * SCREEN_HEIGHT * 4, rgbScaleBuffer.data());
      encoder->encode_frame_rgba(rgbScaleBuffer.data(), SCREEN_WIDTH, SCREEN_HEIGHT, currentTimestamp);

      previousImage = image;
      previousTimestamp = currentTimestamp;
    }

    std::this_thread::yield();
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

  if (recording && ((QDateTime::currentMSecsSinceEpoch() - startedTime) / 1000) % 2 == 0) {
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPoint(buttonRect.right() - btn_size / 10 - centeringOffset, buttonRect.center().y()), btn_size / 10, btn_size / 10);
  }
}
