#pragma once

#include <memory>

#include "frogpilot/ui/screenrecorder/recorder_engine.h"
#include "selfdrive/ui/qt/onroad/buttons.h"

class ScreenRecorder : public QPushButton {
  Q_OBJECT

public:
  explicit ScreenRecorder(QWidget *parent = nullptr);
  ~ScreenRecorder();

  void startRecording();
  void stopRecording();

protected:
  void paintEvent(QPaintEvent *event) override;

private slots:
  void toggleRecording();

private:
  void updateState();

  QColor blackColor(int alpha = 255) { return QColor(0, 0, 0, alpha); }
  QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }
  QColor whiteColor(int alpha = 255) { return QColor(255, 255, 255, alpha); }

  bool recording = false;

  int frameCount = 0;

  qint64 startedTime = 0;

  std::unique_ptr<RecorderEngine> engine;

  QWidget *rootWidget;
};
