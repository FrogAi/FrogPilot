#pragma once

#include "omx_encoder.h"
#include "blocking_queue.h"

#include "selfdrive/ui/qt/onroad/buttons.h"

class ScreenRecorder : public QPushButton {
  Q_OBJECT

public:
  explicit ScreenRecorder(QWidget *parent = nullptr);

  void startRecording();
  void stopRecording();

protected:
  void paintEvent(QPaintEvent *event) override;

private slots:
  void toggleRecording();

private:
  void encodeImage();
  void updateState();

  bool recording;

  int frameCount;

  qint64 startedTime;

  std::thread encodingThread;

  std::unique_ptr<OmxEncoder> encoder;

  std::vector<uint8_t> rgbScaleBuffer;

  BlockingQueue<QImage> imageQueue{UI_FREQ};

  QColor blackColor(int alpha = 255) { return QColor(0, 0, 0, alpha); }
  QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }
  QColor whiteColor(int alpha = 255) { return QColor(255, 255, 255, alpha); }

  QImage synthesizeFrame(const QImage &frame1, const QImage &frame2, double alpha);

  QWidget *rootWidget;
};
