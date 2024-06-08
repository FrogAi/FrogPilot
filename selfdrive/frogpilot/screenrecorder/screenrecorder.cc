#include "libyuv.h"

#include "selfdrive/frogpilot/screenrecorder/screenrecorder.h"
#include "selfdrive/ui/qt/util.h"

namespace {
  inline long long milliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  }
}

ScreenRecorder::ScreenRecorder(QWidget *parent) : QPushButton(parent), recording(false), frame(0), started(0) {
  screenHeight = 1080;
  screenWidth = 2160;

  recorderIcon = loadPixmap("../frogpilot/assets/other_images/img_recorder.png", {img_size, img_size});
  recordingIcon = loadPixmap("../frogpilot/assets/other_images/img_recording.png", {img_size, img_size});
  encoder = std::make_unique<OmxEncoder>("/data/media/0/videos", screenWidth, screenHeight, 60, 8 * 1024 * 1024, false, false);
  rgbScaleBuffer = std::make_unique<uint8_t[]>(screenWidth * screenHeight * 4);

  setFixedSize(btn_size, btn_size);
  connect(this, &QPushButton::clicked, this, &ScreenRecorder::toggleRecording);
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
  frame = 0;
  rootWidget = this;

  char filename[64];
  QDateTime currentTime = QDateTime::currentDateTime();
  QString formattedTime = currentTime.toString("MMMM_dd_yyyy-hh:mmAP");
  snprintf(filename, sizeof(filename), "%s.mp4", formattedTime.toUtf8().constData());

  while (rootWidget->parentWidget() != nullptr) {
    rootWidget = rootWidget->parentWidget();
  }

  try {
    openEncoder(filename);
    encodingThread = std::thread(&ScreenRecorder::encodingThreadFunction, this);
  } catch (const std::exception &e) {
    std::cerr << "Error starting encoder: " << e.what() << std::endl;
    recording = false;
  }

  update();
  started = milliseconds();
}

void ScreenRecorder::stop() {
  if (!recording) {
    return;
  }

  recording = false;
  update();

  if (encodingThread.joinable()) {
    encodingThread.join();
  }

  try {
    closeEncoder();
  } catch (const std::exception &e) {
    std::cerr << "Error stopping encoder: " << e.what() << std::endl;
  }

  imageQueue.clear();
}

void ScreenRecorder::openEncoder(const std::string &filename) {
  encoder->encoder_open(filename.c_str());
}

void ScreenRecorder::closeEncoder() {
  if (encoder) {
    encoder->encoder_close();
  }
}

void ScreenRecorder::encodingThreadFunction() {
  const uint64_t start_time = nanos_since_boot();

  while (recording && encoder) {
    QImage popImage;
    if (imageQueue.pop_wait_for(popImage, std::chrono::milliseconds(10))) {
      const QImage image = popImage.convertToFormat(QImage::Format_RGBA8888);
      libyuv::ARGBScale(image.bits(), image.width() * 4,
                        image.width(), image.height(),
                        rgbScaleBuffer.get(), screenWidth * 4,
                        screenWidth, screenHeight,
                        libyuv::kFilterBilinear);
      encoder->encode_frame_rgba(rgbScaleBuffer.get(), screenWidth, screenHeight,
                                 nanos_since_boot() - start_time);
    }
  }
}

void ScreenRecorder::updateScreen() {
  if (!uiState()->scene.started) {
    if (recording) {
      stop();
    }
    return;
  }

  if (!recording) {
    return;
  }

  if (milliseconds() - started > 1000 * 60 * 3) {
    stop();
    start();
    return;
  }

  if (rootWidget) {
    imageQueue.push(rootWidget->grab().toImage());
  }
  frame++;
}

void ScreenRecorder::paintEvent(QPaintEvent *event) {
  bool flicker = ((milliseconds() - started) / 1000) % 2 == 0;

  QPainter p(this);
  drawIcon(p, QPoint(btn_size / 2, btn_size / 2), recording && flicker ? recordingIcon : recorderIcon, QColor(0, 0, 0, 0), isDown() ? 0.6 : 1.0);
}
