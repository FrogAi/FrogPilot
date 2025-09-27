#include "libyuv.h"

#include "selfdrive/ui/qt/util.h"

#include "frogpilot/ui/screenrecorder/screenrecorder.h"

constexpr int MAX_DURATION = 1000 * 60 * 5;

constexpr int SCREEN_WIDTH = 2160;
constexpr int SCREEN_HEIGHT = 1080;

const QDir RECORDINGS_FOLDER("/data/media/screen_recordings");

ScreenRecorder::ScreenRecorder(QWidget *parent) : QPushButton(parent) {
  setFixedSize(btn_size, btn_size);

  rgbScaleBuffer.resize(SCREEN_WIDTH * SCREEN_HEIGHT * 4);

  rootWidget = topWidget(this);

  QObject::connect(this, &QPushButton::clicked, this, &ScreenRecorder::toggleRecording);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &ScreenRecorder::stopRecording);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &ScreenRecorder::updateState);
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
  encoder = std::make_unique<OmxEncoder>(RECORDINGS_FOLDER.path().toStdString().c_str(), SCREEN_WIDTH, SCREEN_HEIGHT, UI_FREQ * 2, 12 * 1024 * 1024);
  encoder->encoder_open((QDateTime::currentDateTime().toString("MMMM_dd_yyyy-hh-mmAP").toStdString() + ".mp4").c_str());

  if (!encoder->is_open) {
    encoder.reset();
    return;
  }

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

  if (encoder) {
    encoder->encoder_close();
    encoder.reset();
  }
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
  QPainter p(this);
  p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

  if (recording) {
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - startedTime;

    qreal phase = (elapsed % 2000) / 2000.0 * 2 * M_PI;
    qreal alphaFactor = 0.5 + 0.5 * sin(phase);

    QColor glowColor = redColor();
    glowColor.setAlphaF(0.3 + 0.7 * alphaFactor);

    int glowWidth = 8 + static_cast<int>(2 * alphaFactor);

    p.setBrush(blackColor(166));
    p.setFont(InterFont(25, QFont::Bold));
    p.setPen(QPen(glowColor, glowWidth));
  } else {
    p.setBrush(blackColor(166));
    p.setFont(InterFont(25, QFont::DemiBold));
    p.setPen(QPen(redColor(), 8));
  }

  int centeringOffset = 10;

  QRect buttonRect(centeringOffset, btn_size / 3, btn_size - centeringOffset * 2, btn_size / 3);
  p.drawRoundedRect(buttonRect, 24, 24);

  QRect textRect = buttonRect.adjusted(centeringOffset, 0, -centeringOffset, 0);
  p.setPen(QPen(whiteColor(), 6));
  p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, recording ? tr("RECORDING") : tr("RECORD"));

  if (!recording) {
    p.setBrush(redColor(166));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPoint(buttonRect.right() - btn_size / 10 - centeringOffset, buttonRect.center().y()), btn_size / 10, btn_size / 10);
  }
}
