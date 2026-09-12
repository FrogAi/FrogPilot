#pragma once

#include <map>

#include <QColor>
#include <QMetaObject>
#include <QMovie>
#include <QPixmap>
#include <QSharedPointer>
#include <QPointer>
#include <QSize>
#include <QString>
#include <QWidget>

bool useKonikServer();

void clearMovie(QSharedPointer<QMovie> &movie, QWidget *parent);
void loadGif(const QString &gifPath, QSharedPointer<QMovie> &movie, const QSize &size, QWidget *parent, bool repaintOnFrame = true);
void loadImage(const QString &basePath, QPixmap &pixmap, QSharedPointer<QMovie> &movie, const QSize &size, QWidget *parent);
void updateFrogPilotToggles();

QColor loadThemeColors(const QString &colorKey, bool clearCache = false);

QString processModelName(const QString &modelName);

template <typename Function>
void runOnUIThread(QObject *context, Function &&function) {
  QMetaObject::invokeMethod(context, std::forward<Function>(function), Qt::QueuedConnection);
}
