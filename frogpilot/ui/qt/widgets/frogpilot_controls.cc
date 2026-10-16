#include "frogpilot/ui/qt/widgets/frogpilot_controls.h"

#include <QCheckBox>
#include <QRandomGenerator>

#include "selfdrive/ui/qt/widgets/scrollview.h"
#include "selfdrive/ui/ui.h"

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
    background-color: #178644;
  }
  QPushButton:checked:disabled {
    background-color: #99178644;
  }
  QPushButton:disabled {
    color: #33E4E4E4;
  }
)";

void FrogPilotConfirmationDialog::softReboot(QWidget *parent) {
  if (isOpenpilotSteering()) {
    ConfirmationDialog::alert(tr("The device can't reboot while openpilot is steering. Disengage, then reboot from the Device panel."), parent);
    return;
  }

  Params().putBool("DoSoftReboot", true);
}

bool FrogPilotConfirmationDialog::toggleReboot(QWidget *parent) {
  ConfirmationDialog d(tr("Reboot required to take effect."), tr("Reboot Now"), tr("Reboot Later"), false, parent);
  return d.exec();
}

bool FrogPilotConfirmationDialog::yesorno(const QString &prompt_text, QWidget *parent) {
  ConfirmationDialog d(prompt_text, tr("Yes"), tr("No"), false, parent);
  return d.exec();
}

bool isFrogsGoMoo() {
  static bool is_frogsgomoo = QFile::exists("/persist/frogsgomoo.py");
  return is_frogsgomoo;
}

bool isOpenpilotSteering() {
  return uiState()->engaged() || (uiState()->scene.started && frogpilotUIState()->frogpilot_scene.always_on_lateral_active);
}

bool useKonikServer() {
  static bool use_konik = QFile::exists("/cache/use_konik");
  return use_konik;
}

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

class TitleFitter : public QObject {
public:
  using QObject::QObject;

protected:
  bool eventFilter(QObject *object, QEvent *event) override {
    if (event->type() != QEvent::Paint) {
      return false;
    }

    QPushButton *title = static_cast<QPushButton*>(object);

    QFont font = fitInterFont(title->font().pixelSize(), static_cast<QFont::Weight>(title->font().weight()), title->width(), {title->text()});
    QString text = title->text();
    if (font.pixelSize() < 30) {
      font.setPixelSize(30);
      text = QFontMetrics(font).elidedText(text, Qt::ElideRight, title->width());
    }

    QPainter painter(title);
    painter.setFont(font);
    title->style()->drawItemText(&painter, title->rect(), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic, title->palette(), title->isEnabled(), text,
                                 title->foregroundRole());
    return true;
  }
};

void fitTitle(QPushButton *title) {
  static TitleFitter *titleFitter = new TitleFitter(qApp);

  title->parentWidget()->findChild<ElidedLabel*>()->hide();
  title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  title->installEventFilter(titleFitter);
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
  loop = framesParent.toElement().attribute("data-loop") != "false";
  for (const QDomElement &group : frames) {
    framesParent.removeChild(group);
  }

  pixmaps.resize(frames.size());

  timer.setSingleShot(true);
  QObject::connect(&timer, &QTimer::timeout, this, [this]() {
    frame = (frame + 1) % frames.size();
    if (!loop && frame == frames.size() - 1) {
      played = true;
    } else {
      timer.start(delays[frame]);
    }
    emit frameChanged();
  });
}

void FrogPilotMovie::start() {
  if (gif) {
    gif->start();
  } else {
    if (!loop && frame == frames.size() - 1) {
      frame = 0;
    }
    played = false;
    timer.start(delays[frame]);
  }
}

void FrogPilotMovie::stop() {
  if (gif) {
    gif->stop();
  } else {
    played = false;
    timer.stop();
  }
}

bool FrogPilotMovie::isRunning() const {
  if (gif) {
    return gif->state() == QMovie::Running;
  }
  return timer.isActive() || played;
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

QSize FrogPilotMovie::scaledSize() const {
  return size;
}

QString FrogPilotMovie::fileName() const {
  return path;
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

  if (!movie || movie->fileName() != sourcePath || movie->scaledSize() != size) {
    movie = QSharedPointer<FrogPilotMovie>::create(sourcePath, size);

    if (repaintOnFrame) {
      QObject::connect(movie.data(), &FrogPilotMovie::frameChanged, parent, [parent]() {
        parent->update();
      });
    }
  }

  movie->start();
}

const QMap<QString, QString> &randomEventNames() {
  static const QMap<QString, QString> names = {
    {"accel30", QCoreApplication::translate("FrogPilotDataPanel", "UwUs")},
    {"accel35", QCoreApplication::translate("FrogPilotDataPanel", "Loch Ness Encounters")},
    {"accel40", QCoreApplication::translate("FrogPilotDataPanel", "Visits to 1955")},
    {"dejaVuCurve", QCoreApplication::translate("FrogPilotDataPanel", "Deja Vu Moments")},
    {"firefoxSteerSaturated", QCoreApplication::translate("FrogPilotDataPanel", "Internet Explorer Weeeeeeees")},
    {"goatSteerSaturated", QCoreApplication::translate("FrogPilotDataPanel", "Goat Screams")},
    {"hal9000", QCoreApplication::translate("FrogPilotDataPanel", "HAL 9000 Denials")},
    {"openpilotCrashedRandomEvent", QCoreApplication::translate("FrogPilotDataPanel", "openpilot Crashes")},
    {"thisIsFineSteerSaturated", QCoreApplication::translate("FrogPilotDataPanel", "This Is Fine Moments")},
    {"toBeContinued", QCoreApplication::translate("FrogPilotDataPanel", "To Be Continued Moments")},
    {"vCruise69", QCoreApplication::translate("FrogPilotDataPanel", "Noices")},
    {"yourFrogTriedToKillMe", QCoreApplication::translate("FrogPilotDataPanel", "Attempted Frog Murders")},
    {"youveGotMail", QCoreApplication::translate("FrogPilotDataPanel", "Total Mail Received")}
  };
  return names;
}

QString cleanModelName(QString modelName) {
  return modelName.remove("_default").remove("(Default)");
}

QString formatDistance(double meters, bool isMetric) {
  int value;
  QString unit;
  if (isMetric) {
    value = qRound(meters / 1000.0);
    unit = (value == 1) ? QCoreApplication::translate("FrogPilotDataPanel", " kilometer") : QCoreApplication::translate("FrogPilotDataPanel", " kilometers");
  } else {
    value = qRound(meters * METER_TO_MILE);
    unit = (value == 1) ? QCoreApplication::translate("FrogPilotDataPanel", " mile") : QCoreApplication::translate("FrogPilotDataPanel", " miles");
  }
  return QLocale().toString(value) + unit;
}

QString formatDownloadProgress(const std::string &progress, const QString &updaterState) {
  QJsonObject progressObject = QJsonDocument::fromJson(QByteArray::fromStdString(progress)).object();
  if (!progressObject.contains("percent")) {
    return updaterState;
  }

  QString result = QString("%1%").arg(progressObject.value("percent").toInt());

  QJsonValue eta = progressObject.value("eta");
  if (eta.isDouble()) {
    int seconds = eta.toInt();
    if (seconds < 60) {
      result += QCoreApplication::translate("SoftwarePanel", " · less than a minute left");
    } else {
      result += QCoreApplication::translate("SoftwarePanel", " · about %1 min left").arg(qRound(seconds / 60.0));
    }
  }
  return result;
}

QString formatDuration(int seconds) {
  const int secondsInDay = 60 * 60 * 24;
  const int secondsInHour = 60 * 60;

  int days = seconds / secondsInDay;
  int hours = (seconds % secondsInDay) / secondsInHour;
  int minutes = (seconds % secondsInHour) / 60;

  QString result;
  if (days > 0) {
    result += QLocale().toString(days) + (days == 1 ? QCoreApplication::translate("FrogPilotDataPanel", " day ") : QCoreApplication::translate("FrogPilotDataPanel", " days "));
  }
  if (hours > 0 || days > 0) {
    result += QLocale().toString(hours) + (hours == 1 ? QCoreApplication::translate("FrogPilotDataPanel", " hour ") : QCoreApplication::translate("FrogPilotDataPanel", " hours "));
  }
  result += QLocale().toString(minutes) + (minutes == 1 ? QCoreApplication::translate("FrogPilotDataPanel", " minute") : QCoreApplication::translate("FrogPilotDataPanel", " minutes"));
  return result.trimmed();
}

QString formatShortTime(const QTime &time) {
  QLocale uiLocale(uiState()->language.mid(5));
  if (uiLocale.language() == QLocale::C) {
    uiLocale = QLocale(QLocale::English, QLocale::UnitedStates);
  }
  return uiLocale.toString(time, QLocale::ShortFormat);
}

QString newPairingCode() {
  return QString::number(QRandomGenerator::system()->bounded(1000000)).rightJustified(6, '0');
}

FrogPilotMultiOptionDialog::FrogPilotMultiOptionDialog(const QString &prompt_text, const QStringList &l, const QString &confirm_text, QWidget *parent) : DialogBase(parent) {
  QFrame *container = new QFrame(this);
  container->setStyleSheet(R"(
    QFrame { background-color: #1B1B1B; }
    #confirm_btn[enabled="false"] { background-color: #2B2B2B; }
    #confirm_btn:enabled { background-color: #465BEA; }
    #confirm_btn:enabled:pressed { background-color: #3049F4; }
  )");

  QVBoxLayout *main_layout = new QVBoxLayout(container);
  main_layout->setContentsMargins(55, 50, 55, 50);

  QLabel *title = new QLabel(prompt_text, this);
  title->setStyleSheet("font-size: 70px; font-weight: 500;");
  main_layout->addWidget(title, 0, Qt::AlignLeft | Qt::AlignTop);
  main_layout->addSpacing(25);

  QWidget *listWidget = new QWidget(this);
  QVBoxLayout *listLayout = new QVBoxLayout(listWidget);
  listLayout->setSpacing(20);
  listWidget->setStyleSheet(R"(
    QPushButton {
      height: 135;
      padding: 0px 160px 0px 50px;
      text-align: left;
      font-size: 55px;
      font-weight: 300;
      border-radius: 10px;
      background-color: #4F4F4F;
    }
    QPushButton:checked { background-color: #465BEA; }
    QCheckBox::indicator {
      width: 60px;
      height: 60px;
      border: 5px solid #E4E4E4;
      border-radius: 10px;
    }
    QCheckBox::indicator:checked { image: url(:/icons/checkmark.svg); }
  )");

  QButtonGroup *group = new QButtonGroup(listWidget);
  group->setExclusive(false);

  QPushButton *confirm_btn = new QPushButton(confirm_text);
  confirm_btn->setObjectName("confirm_btn");
  confirm_btn->setEnabled(false);

  for (const QString &s : l) {
    QPushButton *selectionLabel = new QPushButton(s);
    selectionLabel->setCheckable(true);
    selectionLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

    QCheckBox *checkbox = new QCheckBox(selectionLabel);
    checkbox->setAttribute(Qt::WA_TransparentForMouseEvents);
    QObject::connect(selectionLabel, &QPushButton::toggled, checkbox, &QCheckBox::setChecked);

    QHBoxLayout *selectionLayout = new QHBoxLayout(selectionLabel);
    selectionLayout->setContentsMargins(0, 0, 50, 0);
    selectionLayout->addWidget(checkbox, 0, Qt::AlignRight);

    QObject::connect(selectionLabel, &QPushButton::toggled, [=](bool checked) {
      if (checked) {
        selections.append(s);
      } else {
        selections.removeOne(s);
      }
      confirm_btn->setEnabled(!selections.isEmpty());
    });

    group->addButton(selectionLabel);
    listLayout->addWidget(selectionLabel);
  }
  // add stretch to keep buttons spaced correctly
  listLayout->addStretch(1);

  ScrollView *scroll_view = new ScrollView(listWidget, this);
  scroll_view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  main_layout->addWidget(scroll_view);
  main_layout->addSpacing(35);

  // cancel + confirm buttons
  QHBoxLayout *blayout = new QHBoxLayout;
  main_layout->addLayout(blayout);
  blayout->setSpacing(50);

  QPushButton *cancel_btn = new QPushButton(tr("Cancel"));
  QObject::connect(cancel_btn, &QPushButton::clicked, this, &ConfirmationDialog::reject);
  QObject::connect(confirm_btn, &QPushButton::clicked, this, &ConfirmationDialog::accept);
  blayout->addWidget(cancel_btn);
  blayout->addWidget(confirm_btn);

  QVBoxLayout *outer_layout = new QVBoxLayout(this);
  outer_layout->setContentsMargins(50, 50, 50, 50);
  outer_layout->addWidget(container);
}

QStringList FrogPilotMultiOptionDialog::getSelections(const QString &prompt_text, const QStringList &l, const QString &confirm_text, QWidget *parent) {
  FrogPilotMultiOptionDialog d(prompt_text, l, confirm_text, parent);
  if (d.exec()) {
    return d.selections;
  }
  return {};
}
