#pragma once

#include <cmath>
#include <set>

#include <QMovie>
#include <QNetworkReply>
#include <QStyle>
#include <QTimer>

#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/qt/widgets/controls.h"

QFont fitInterFont(int pixelSize, QFont::Weight weight, int width, const QStringList &texts);

QString cleanModelName(QString modelName);

void loadGif(const QString &gifPath, QSharedPointer<QMovie> &movie, const QSize &size, QWidget *parent, bool repaintOnFrame = true);
void loadImage(const QString &basePath, QPixmap &pixmap, QSharedPointer<QMovie> &movie, const QSize &size, QWidget *parent, bool repaintOnFrame = true);

template <typename Function>
void runOnUIThread(QObject *context, Function &&function) {
  QMetaObject::invokeMethod(context, std::forward<Function>(function), Qt::QueuedConnection);
}
