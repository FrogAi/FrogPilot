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

void loadGif(const QString &gifPath, QSharedPointer<QMovie> &movie, const QSize &size, QWidget *parent, bool repaintOnFrame) {
  const QString sourcePath = QFileInfo(gifPath).canonicalFilePath();
  if (sourcePath.isEmpty()) {
    movie.reset();
    return;
  }

  if (movie && movie->fileName() == sourcePath) {
    if (movie->scaledSize() != size) {
      movie->setScaledSize(size);
    }
    movie->start();
    return;
  }

  movie = QSharedPointer<QMovie>::create(sourcePath);
  movie->setCacheMode(QMovie::CacheAll);
  movie->setScaledSize(size);

  if (repaintOnFrame) {
    QObject::connect(movie.data(), &QMovie::frameChanged, parent, [parent]() {
      parent->update();
    });
  }

  movie->start();
}

void loadImage(const QString &basePath, QPixmap &pixmap, QSharedPointer<QMovie> &movie, const QSize &size, QWidget *parent, bool repaintOnFrame) {
  const QString gifPath = basePath + ".gif";
  if (QFileInfo::exists(gifPath)) {
    pixmap = QPixmap();
    loadGif(gifPath, movie, size, parent, repaintOnFrame);
  } else {
    movie.reset();

    QPixmap loadedPixmap(QFileInfo(basePath + ".png").canonicalFilePath());
    pixmap = loadedPixmap.isNull() ? QPixmap() : loadedPixmap.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  }

  parent->update();
}

QString cleanModelName(QString modelName) {
  return modelName.remove("_default").remove("(Default)");
}
