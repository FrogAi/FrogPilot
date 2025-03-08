#pragma once

#include "omx_encoder.h"
#include "blocking_queue.h"

#include "selfdrive/ui/qt/onroad/buttons.h"

class ScreenRecorder : public QPushButton {
  Q_OBJECT

public:
  explicit ScreenRecorder(QWidget *parent = nullptr);
  ~ScreenRecorder() override;

  void updateScreen(double fps, bool started);

protected:
  void paintEvent(QPaintEvent *event) override;

private slots:
  void toggleRecording();

private:
  void closeEncoder();
  void encodingThreadFunction();
  void openEncoder(const std::string &filename);
  void startRecording();
  void stopRecording();

  BlockingQueue<QImage> imageQueue{UI_FREQ};

  inline QColor blackColor(int alpha = 255) { return QColor(0, 0, 0, alpha); }
  inline QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }
  inline QColor whiteColor(int alpha = 255) { return QColor(255, 255, 255, alpha); }

  QWidget *rootWidget = nullptr;

  int screenHeight = 1080;
  int screenWidth = 2160;

  long long startedTime = 0;

  std::atomic<bool> encoderReady{false};
  std::atomic<bool> recording{false};

  std::unique_ptr<OmxEncoder> encoder;

  std::unique_ptr<uint8_t[]> rgbScaleBuffer;

  std::thread encodingThread;
};
