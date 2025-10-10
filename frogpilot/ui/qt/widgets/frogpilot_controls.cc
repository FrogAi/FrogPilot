#include "selfdrive/ui/ui.h"

#include "frogpilot/ui/frogpilot_ui.h"

bool FrogPilotConfirmationDialog::toggleReboot(QWidget *parent) {
  ConfirmationDialog d(tr("Reboot required to take effect."), tr("Reboot Now"), tr("Reboot Later"), false, parent);
  return d.exec();
}

bool FrogPilotConfirmationDialog::yesorno(const QString &prompt_text, QWidget *parent) {
  ConfirmationDialog d(prompt_text, tr("Yes"), tr("No"), false, parent);
  return d.exec();
}

void openDescriptions(bool forceOpenDescriptions, std::map<QString, AbstractControl*> toggles) {
  if (forceOpenDescriptions) {
    for (auto &[key, toggle] : toggles) {
      if (key != "CESpeed") {
        toggle->showDescription();
      }
    }
  }
}

void loadGif(const QString &gifPath, QSharedPointer<QMovie> &movie, const QSize &size, QWidget *parent) {
  if (!movie.isNull()) {
    QObject::disconnect(movie.data(), nullptr, parent, nullptr);

    movie->stop();
    movie.clear();
  }

  if (QFileInfo::exists(gifPath)) {
    QSharedPointer<QMovie> gif(QSharedPointer<QMovie>::create(gifPath, QByteArray(), parent));
    gif->setCacheMode(QMovie::CacheAll);
    gif->setScaledSize(size);

    QObject::connect(gif.data(), &QMovie::frameChanged, parent, [parent](int) { parent->update(); }, Qt::UniqueConnection);

    gif->start();

    movie = gif;
  }

  parent->update();
}

void loadImage(const QString &basePath, QPixmap &pixmap, QSharedPointer<QMovie> &movie, const QSize &size, QWidget *parent, Qt::AspectRatioMode aspectRatioMode) {
  QString gifPath = basePath + ".gif";
  if (QFileInfo::exists(gifPath)) {
    loadGif(gifPath, movie, size, parent);

    parent->update();
  } else {
    if (!movie.isNull()) {
      QObject::disconnect(movie.data(), nullptr, parent, nullptr);

      movie->stop();
      movie.clear();
    }

    pixmap = QPixmap(basePath + ".png").scaled(size, aspectRatioMode, Qt::SmoothTransformation);

    parent->update();
  }
}

void updateFrogPilotToggles() {
  static Params params_memory{"/dev/shm/params"};
  params_memory.putBool("FrogPilotTogglesUpdated", true);
}

QString processModelName(const QString &modelName) {
  QString modelCleaned = modelName;
  modelCleaned = modelCleaned.remove(QRegularExpression("[🗺️👀📡]")).simplified();
  modelCleaned = modelCleaned.replace("(Default)", "");
  return modelCleaned;
}
