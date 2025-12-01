#include "frogpilot/ui/qt/widgets/frogpilot_controls.h"

#include "selfdrive/ui/ui.h"

QFont fitInterFont(int pixelSize, QFont::Weight weight, int width, const QStringList &texts) {
  static QHash<QString, int> fittedSizes;

  QString key = QString("%1 %2 %3 %4").arg(pixelSize).arg(weight).arg(width).arg(texts.join('|'));
  QHash<QString, int>::const_iterator fitted = fittedSizes.constFind(key);
  if (fitted != fittedSizes.constEnd()) {
    return InterFont(fitted.value(), weight);
  }

  InterFont font(pixelSize, weight);
  QFontMetrics metrics(font);

  int textWidth = 0;
  for (const QString &text : texts) {
    textWidth = std::max(textWidth, metrics.horizontalAdvance(text));
  }
  if (textWidth > width) {
    font.setPixelSize(std::max(1, pixelSize * width / textWidth));
  }
  fittedSizes.insert(key, font.pixelSize());
  return font;
}

FrogPilotMovie::FrogPilotMovie(const QString &fileName, const QSize &size) : size(size), path(fileName) {
  if (!fileName.endsWith(".svg")) {
    gif = new QMovie(fileName, QByteArray(), this);
    gif->setCacheMode(QMovie::CacheAll);
    gif->setScaledSize(size);
    QObject::connect(gif, &QMovie::frameChanged, this, &FrogPilotMovie::frameChanged);
    return;
  }

  QFile file(fileName);
  file.open(QIODevice::ReadOnly);
  svg.setContent(&file);

  const QDomNodeList groups = svg.elementsByTagName("g");
  for (int i = 0; i < groups.size(); i++) {
    const QDomElement group = groups.at(i).toElement();
    if (group.hasAttribute("data-ms")) {
      delays.append(group.attribute("data-ms").toInt());
      frames.append(group);
    }
  }

  framesParent = frames.first().parentNode();
  for (const QDomElement &group : frames) {
    framesParent.removeChild(group);
  }

  pixmaps.resize(frames.size());

  timer.setSingleShot(true);
  QObject::connect(&timer, &QTimer::timeout, this, [this]() {
    frame = (frame + 1) % frames.size();
    timer.start(delays[frame]);
    emit frameChanged();
  });
}

void FrogPilotMovie::start() {
  if (gif) {
    gif->start();
  } else {
    timer.start(delays[frame]);
  }
}

void FrogPilotMovie::stop() {
  if (gif) {
    gif->stop();
  } else {
    timer.stop();
  }
}

bool FrogPilotMovie::isRunning() const {
  if (gif) {
    return gif->state() == QMovie::Running;
  }
  return timer.isActive();
}

int FrogPilotMovie::currentFrameNumber() const {
  if (gif) {
    return gif->currentFrameNumber();
  }
  return frame;
}

QPixmap FrogPilotMovie::currentPixmap() {
  if (gif) {
    return gif->currentPixmap();
  }

  QPixmap &pixmap = pixmaps[frame];
  if (pixmap.isNull()) {
    framesParent.appendChild(frames[frame]);
    QByteArray document = svg.toByteArray();
    framesParent.removeChild(frames[frame]);

    QBuffer buffer(&document);
    QImageReader reader(&buffer, "svg");
    reader.setScaledSize(size);
    pixmap = QPixmap::fromImageReader(&reader);
  }
  return pixmap;
}

void loadImage(const QString &basePath, QPixmap &pixmap, QSharedPointer<FrogPilotMovie> &movie, const QSize &size, QWidget *parent, bool repaintOnFrame) {
  const QString gifPath = basePath + ".gif";
  if (QFileInfo::exists(gifPath)) {
    pixmap = QPixmap();
    loadMovie(gifPath, movie, size, parent, repaintOnFrame);
  } else {
    QString imagePath = basePath + ".svg";
    if (!QFileInfo::exists(imagePath)) {
      imagePath = basePath + ".png";
    }

    QFile file(imagePath);
    file.open(QIODevice::ReadOnly);
    if (imagePath.endsWith(".svg") && file.readAll().contains("data-ms=")) {
      pixmap = QPixmap();
      loadMovie(imagePath, movie, size, parent, repaintOnFrame);
    } else {
      movie.reset();

      QPixmap loadedPixmap(QFileInfo(imagePath).canonicalFilePath());
      pixmap = loadedPixmap.isNull() ? QPixmap() : loadedPixmap.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
  }

  parent->update();
}

void loadMovie(const QString &moviePath, QSharedPointer<FrogPilotMovie> &movie, const QSize &size, QWidget *parent, bool repaintOnFrame) {
  const QString sourcePath = QFileInfo(moviePath).canonicalFilePath();
  if (sourcePath.isEmpty()) {
    movie.reset();
    return;
  }

  if (!movie || movie->path != sourcePath || movie->size != size) {
    movie = QSharedPointer<FrogPilotMovie>::create(sourcePath, size);

    if (repaintOnFrame) {
      QObject::connect(movie.data(), &FrogPilotMovie::frameChanged, parent, [parent]() {
        parent->update();
      });
    }
  }

  movie->start();
}

QString cleanModelName(QString modelName) {
  return modelName.remove("_default").remove("(Default)");
}
