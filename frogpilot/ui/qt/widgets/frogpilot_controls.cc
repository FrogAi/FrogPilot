#include "frogpilot/ui/qt/widgets/frogpilot_controls.h"

#include <QCheckBox>

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

bool FrogPilotConfirmationDialog::toggleReboot(QWidget *parent) {
  ConfirmationDialog d(tr("Reboot required to take effect."), tr("Reboot Now"), tr("Reboot Later"), false, parent);
  return d.exec();
}

bool FrogPilotConfirmationDialog::yesorno(const QString &prompt_text, QWidget *parent) {
  ConfirmationDialog d(prompt_text, tr("Yes"), tr("No"), false, parent);
  return d.exec();
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

QString formatShortTime(const QTime &time) {
  QLocale uiLocale(uiState()->language.mid(5));
  if (uiLocale.language() == QLocale::C) {
    uiLocale = QLocale(QLocale::English, QLocale::UnitedStates);
  }
  return uiLocale.toString(time, QLocale::ShortFormat);
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
