#pragma once

#include <cmath>
#include <set>

#include <QDomDocument>
#include <QMovie>
#include <QNetworkReply>
#include <QStyle>
#include <QTimer>

#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/qt/widgets/controls.h"

inline QColor blackColor(int alpha = 255) { return QColor(0, 0, 0, alpha); }

QFont fitInterFont(int pixelSize, QFont::Weight weight, int width, const QStringList &texts);

QString cleanModelName(QString modelName);

class FrogPilotMovie : public QObject {
  Q_OBJECT

public:
  FrogPilotMovie(const QString &fileName, const QSize &size);

  void start();
  void stop();

  bool isRunning() const;

  int currentFrameNumber() const;

  QPixmap currentPixmap();

  const QSize size;

  const QString path;

signals:
  void frameChanged();

private:
  int frame = 0;

  QDomDocument svg;

  QDomNode framesParent;

  QMovie *gif = nullptr;

  QTimer timer;

  QVector<int> delays;
  QVector<QDomElement> frames;
  QVector<QPixmap> pixmaps;
};

void loadImage(const QString &basePath, QPixmap &pixmap, QSharedPointer<FrogPilotMovie> &movie, const QSize &size, QWidget *parent, bool repaintOnFrame = true);
void loadMovie(const QString &moviePath, QSharedPointer<FrogPilotMovie> &movie, const QSize &size, QWidget *parent, bool repaintOnFrame = true);

template <typename Function>
void runOnUIThread(QObject *context, Function &&function) {
  QMetaObject::invokeMethod(context, std::forward<Function>(function), Qt::QueuedConnection);
}
