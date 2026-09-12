#include "frogpilot/ui/qt/widgets/frogpilot_controls.h"

#include <QFile>
#include <QFileInfo>
#include <QPointer>

const QString buttonStyle = R"(
  QPushButton {
    padding: 0px 25px 0px 25px;
    border-radius: 50px;
    font-size: 35px;
    font-weight: 500;
    height: 100px;
    color: #E4E4E4;
    background-color: #393939;
  }
  QPushButton:pressed {
    background-color: #4a4a4a;
  }
  QPushButton:checked:enabled {
    background-color: #33Ab4C;
  }
  QPushButton:disabled {
    color: #33E4E4E4;
  }
)";

bool FrogPilotConfirmationDialog::toggleReboot(QWidget *parent) {
  ConfirmationDialog d(tr("Reboot required to take effect."), tr("Reboot Now"), tr("Reboot Later"), false, parent);
  bool reboot = d.exec();
  if (reboot) {
    for (FrogPilotParamValueControl *control : parent->findChildren<FrogPilotParamValueControl*>()) {
      if (control->isVisible()) {
        control->updateParam();
      }
    }
  }
  return reboot;
}

bool FrogPilotConfirmationDialog::yesorno(const QString &prompt_text, QWidget *parent) {
  ConfirmationDialog d(prompt_text, tr("Yes"), tr("No"), false, parent);
  return d.exec();
}

bool useKonikServer() {
  static bool use_konik = QFile::exists("/cache/use_konik");
  return use_konik;
}

void clearMovie(QSharedPointer<QMovie> &movie, QWidget *parent) {
  if (!movie) {
    return;
  }

  QObject::disconnect(movie.data(), nullptr, parent, nullptr);
  movie->stop();
  movie.reset();
}

void loadGif(const QString &gifPath, QSharedPointer<QMovie> &movie, const QSize &size, QWidget *parent, bool repaintOnFrame) {
  if (!parent) {
    return;
  }

  if (gifPath.isEmpty()) {
    clearMovie(movie, parent);
    return;
  }

  QFileInfo gifInfo(gifPath);
  if (!gifInfo.exists()) {
    clearMovie(movie, parent);
    return;
  }

  QString sourcePath = gifInfo.canonicalFilePath();
  if (sourcePath.isEmpty()) {
    sourcePath = gifInfo.absoluteFilePath();
  }

  if (movie && movie->property("sourcePath").toString() == sourcePath && movie->state() == QMovie::Running) {
    if (movie->scaledSize() != size) {
      movie->setScaledSize(size);
    }
    return;
  }

  clearMovie(movie, parent);

  movie = QSharedPointer<QMovie>::create(gifPath);
  movie->setProperty("sourcePath", sourcePath);
  movie->setCacheMode(QMovie::CacheAll);
  movie->setScaledSize(size);

  if (repaintOnFrame) {
    QPointer<QWidget> safeParent(parent);
    QObject::connect(movie.data(), &QMovie::frameChanged, parent, [safeParent]() {
      if (safeParent && safeParent->isVisible()) {
        safeParent->update();
      }
    });
  }

  movie->start();
}

void loadImage(const QString &basePath, QPixmap &pixmap, QSharedPointer<QMovie> &movie, const QSize &size, QWidget *parent) {
  if (!parent) {
    return;
  }

  const QString gifPath = basePath + ".gif";
  if (QFileInfo::exists(gifPath)) {
    pixmap = QPixmap();
    loadGif(gifPath, movie, size, parent);
  } else {
    clearMovie(movie, parent);

    QPixmap loadedPixmap(basePath + ".png");
    pixmap = loadedPixmap.isNull() ? QPixmap() : loadedPixmap.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  }

  parent->update();
}

void updateFrogPilotToggles() {
  static Params params_memory{"/dev/shm/params"};
  params_memory.putBool("FrogPilotTogglesUpdated", true);
}

QColor loadThemeColors(const QString &colorKey, bool clearCache) {
  static QJsonObject cachedColorData;

  if (clearCache) {
    QFile file("../../frogpilot/assets/active_theme/colors/colors.json");
    if (file.open(QIODevice::ReadOnly)) {
      cachedColorData = QJsonDocument::fromJson(file.readAll()).object();
    } else {
      cachedColorData = QJsonObject();
      return QColor();
    }

  }

  if (cachedColorData.isEmpty()) {
    return QColor();
  }

  if (colorKey.isEmpty()) {
    return QColor(255, 255, 255);
  }

  const QJsonObject colorObj = cachedColorData[colorKey].toObject();
  return QColor(
    colorObj.value("red").toInt(255),
    colorObj.value("green").toInt(255),
    colorObj.value("blue").toInt(255),
    colorObj.value("alpha").toInt(255)
  );
}

QString processModelName(const QString &modelName) {
  QString modelCleaned = modelName;
  modelCleaned = modelCleaned.remove(QRegularExpression("[🗺️👀📡]")).replace("(Default)", "").simplified();
  return modelCleaned;
}
