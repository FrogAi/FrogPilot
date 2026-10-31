#pragma once

#include <QFile>
#include <QMetaObject>
#include <QMovie>
#include <QSharedPointer>
#include <QSize>
#include <QString>
#include <QWidget>

bool frogsGoMoo();

inline bool useKonikServer() {
  static const bool use_konik = QFile::exists("/cache/use_konik");
  return use_konik;
}

QString processModelName(const QString &modelName);

template <typename Function>
void runOnUIThread(QObject *context, Function &&function) {
  QMetaObject::invokeMethod(context, std::forward<Function>(function), Qt::QueuedConnection);
}
