#pragma once

#include <cmath>
#include <set>

#include <QMovie>
#include <QNetworkReply>
#include <QStyle>
#include <QTimer>

#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/qt/widgets/controls.h"

bool isFrogsGoMoo();
bool isOpenpilotSteering();

QFont fitInterFont(int pixelSize, QFont::Weight weight, int width, const QStringList &texts);

QString cleanModelName(QString modelName);
QString formatShortTime(const QTime &time);

void fitTitle(QPushButton *title);
void loadGif(const QString &gifPath, QSharedPointer<QMovie> &movie, const QSize &size, QWidget *parent, bool repaintOnFrame = true);
void loadImage(const QString &basePath, QPixmap &pixmap, QSharedPointer<QMovie> &movie, const QSize &size, QWidget *parent, bool repaintOnFrame = true);

template <typename T>
void openDescriptions(bool forceOpenDescriptions, const std::map<QString, T> &toggles) {
  if (!forceOpenDescriptions) {
    return;
  }

  for (const auto &[key, toggle] : toggles) {
    if (AbstractControl *control = qobject_cast<AbstractControl*>(toggle)) {
      control->showDescription();
    }
  }
}

template <typename Function>
void runOnUIThread(QObject *context, Function &&function) {
  QMetaObject::invokeMethod(context, std::forward<Function>(function), Qt::QueuedConnection);
}

extern const QString buttonStyle;

class FrogPilotConfirmationDialog : public ConfirmationDialog {
  Q_OBJECT

public:
  static bool toggleReboot(QWidget *parent);
  static bool yesorno(const QString &prompt_text, QWidget *parent);
};

class FrogPilotMultiOptionDialog : public DialogBase {
  Q_OBJECT

public:
  explicit FrogPilotMultiOptionDialog(const QString &prompt_text, const QStringList &l, const QString &confirm_text, QWidget *parent);
  static QStringList getSelections(const QString &prompt_text, const QStringList &l, const QString &confirm_text, QWidget *parent);
  QStringList selections;
};

class FrogPilotListWidget : public ListWidget {
  Q_OBJECT
 public:
  explicit FrogPilotListWidget(QWidget *parent = 0) : ListWidget(parent) {
    outer_layout.setStretch(1, 0);
  }
  inline void addItem(QWidget *w, bool expanding = false) {
    w->setSizePolicy(QSizePolicy::Preferred, expanding ? QSizePolicy::Expanding : QSizePolicy::Maximum);
    inner_layout.addWidget(w);
  }
  inline void addItem(QLayout *layout) { ListWidget::addItem(layout); }
  inline void insertItem(int index, QWidget *w) {
    w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    inner_layout.insertWidget(index, w);
  }
  void clear() {
    while (QLayoutItem *child = inner_layout.takeAt(0)) {
      if (child->widget()) {
        child->widget()->hide();
        child->widget()->deleteLater();
      }
      delete child;
    }
    update();
  }

private:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setPen(Qt::gray);
    int last = inner_layout.count() - 1;
    while (last > 0 && inner_layout.itemAt(last)->widget() != nullptr && !inner_layout.itemAt(last)->widget()->isVisible()) {
      last--;
    }
    for (int i = 0; i < last; ++i) {
      QWidget *widget = inner_layout.itemAt(i)->widget();
      if (widget == nullptr || widget->isVisible()) {
        QRect r = inner_layout.itemAt(i)->geometry();
        int bottom = r.bottom() + inner_layout.spacing() / 2;
        p.drawLine(r.left() + 40, bottom, r.right() - 40, bottom);
      }
    }
  }
};

class FrogPilotButtonsControl : public AbstractControl {
  Q_OBJECT
public:
  FrogPilotButtonsControl(const QString &title, const QString &desc, const QString &icon,
                          const std::vector<QString> &button_texts, bool checkable = false, bool exclusive = true,
                          const int minimum_button_width = 225) : AbstractControl(title, desc, icon) {
    button_group = new QButtonGroup(this);
    button_group->setExclusive(exclusive);
    for (int i = 0; i < button_texts.size(); i++) {
      QPushButton *button = new QPushButton(button_texts[i], this);
      button->setCheckable(checkable);
      button->setStyleSheet(buttonStyle);
      button->setMinimumWidth(minimum_button_width);
      hlayout->addWidget(button);
      button_group->addButton(button, i);
    }

    QObject::connect(button_group, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &FrogPilotButtonsControl::buttonClicked);
  }

  void setEnabled(bool enable) {
    for (auto btn : button_group->buttons()) {
      btn->setEnabled(enable);
    }
  }

  void setCheckedButton(int id) {
    if (QAbstractButton *button = button_group->button(id)) {
      button->setChecked(true);
    }
  }

  void clearCheckedButtons() {
    bool original_exclusive = button_group->exclusive();

    button_group->setExclusive(false);

    for (QAbstractButton *button : button_group->buttons()) {
      button->setChecked(false);
    }

    button_group->setExclusive(original_exclusive);
  }

  void setEnabledButtons(int id, bool enable) {
    if (QAbstractButton *button = button_group->button(id)) {
      button->setEnabled(enable);
    }
  }

  void setText(int id, const QString &text) {
    if (QAbstractButton *button = button_group->button(id)) {
      button->setText(text);
    }
  }

  void setVisibleButton(int id, bool visible) {
    if (QAbstractButton *button = button_group->button(id)) {
      button->setVisible(visible);
    }
  }

signals:
  void buttonClicked(int id);

protected:
  QButtonGroup *button_group;
};

class FrogPilotButtonToggleControl : public ParamControl {
  Q_OBJECT
public:
  FrogPilotButtonToggleControl(const QString &param, const QString &title, const QString &desc, const QString &icon,
                               const std::vector<QString> &button_params, const std::vector<QString> &button_texts,
                               bool exclusive = false, int minimum_button_width = 225)
    : ParamControl(param, title, desc, icon), button_params(button_params) {
    fitTitle(title_label);

    button_group = new QButtonGroup(this);
    button_group->setExclusive(exclusive);
    for (int i = 0; i < button_texts.size(); i++) {
      QPushButton *button = new QPushButton(button_texts[i], this);
      button->setCheckable(true);
      button->setStyleSheet(buttonStyle);
      button->setMinimumWidth(minimum_button_width);
      hlayout->addWidget(button);
      button_group->addButton(button, i);
    }

    hlayout->removeWidget(&toggle);
    hlayout->addWidget(&toggle);

    QObject::connect(button_group, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &FrogPilotButtonToggleControl::onButtonClicked);

    QObject::connect(this, &ToggleControl::toggleFlipped, this, &FrogPilotButtonToggleControl::refresh);
  }

  void refresh() override {
    ParamControl::refresh();

    for (QAbstractButton *button : button_group->buttons()) {
      button->setEnabled(toggle.on);
    }

    bool original_exclusive = button_group->exclusive();

    button_group->setExclusive(false);

    for (int i = 0; i < button_params.size(); ++i) {
      button_group->button(i)->setChecked(params.getBool(button_params[i].toStdString()));
    }

    button_group->setExclusive(original_exclusive);
  }

  void clearCheckedButtons() {
    bool original_exclusive = button_group->exclusive();

    button_group->setExclusive(false);

    for (QAbstractButton *button : button_group->buttons()) {
      button->setChecked(false);
    }

    button_group->setExclusive(original_exclusive);
  }

  void setCheckedButton(int id) {
    if (QAbstractButton *button = button_group->button(id)) {
      button->setChecked(true);
    }
  }

  void setVisibleButton(int id, bool visible) {
    if (QAbstractButton *button = button_group->button(id)) {
      button->setVisible(visible);
    }
  }

signals:
  void buttonClicked(int id);

private:
  void onButtonClicked(int id) {
    if (!button_params.empty()) {
      params.putBool(button_params[id].toStdString(), button_group->button(id)->isChecked());
    }

    emit buttonClicked(id);
  }

  std::vector<QString> button_params;

  QButtonGroup *button_group;
};

class FrogPilotManageControl : public ParamControl {
  Q_OBJECT
public:
  FrogPilotManageControl(const QString &param, const QString &title, const QString &desc, const QString &icon) : ParamControl(param, title, desc, icon) {
    fitTitle(title_label);

    manage_button = new QPushButton(tr("MANAGE"), this);
    manage_button->setMinimumWidth(250);
    manage_button->setStyleSheet(buttonStyle);

    hlayout->insertWidget(hlayout->indexOf(&toggle), manage_button);

    QObject::connect(manage_button, &QPushButton::clicked, this, &FrogPilotManageControl::manageButtonClicked);
    QObject::connect(this, &ToggleControl::toggleFlipped, this, &FrogPilotManageControl::refresh);
  }

  void refresh() override {
    ParamControl::refresh();
    manage_button->setEnabled(toggle.on);
  }

  void setManageVisible(bool visible) {
    manage_button->setVisible(visible);
  }

signals:
  void manageButtonClicked();

private:
  QPushButton *manage_button;
};

class FrogPilotParamValueControl : public AbstractControl {
  Q_OBJECT
public:
  FrogPilotParamValueControl(const QString &param, const QString &title, const QString &desc, const QString &icon,
                             float min_value, float max_value, const QString &label, const std::map<float, QString> &value_labels = {},
                             float interval = 1.0f, bool fast_increase = false)
                             : AbstractControl(title, desc, icon),
                               fast_increase(fast_increase), interval(interval), max_value(max_value), min_value(min_value),
                               value_labels(value_labels), label(label) {
    decimals = std::ceil(-std::log10(interval));
    factor = std::pow(10, decimals);
    key = param.toStdString();
    key_type = params.getKeyType(key);

    fitTitle(title_label);

    setupButton(decrement_button, "-");
    setupButton(increment_button, "+");

    value_label = new QLabel(this);
    value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    value_label->setStyleSheet("QLabel {color: #E0E879;}");
    updateLabelWidth();

    hlayout->addWidget(value_label);
    hlayout->addWidget(&decrement_button);
    hlayout->addWidget(&increment_button);

    QObject::connect(&decrement_button, &QPushButton::pressed, this, [this]() { changeValue(-1); });
    QObject::connect(&increment_button, &QPushButton::pressed, this, [this]() { changeValue(1); });
    QObject::connect(&decrement_button, &QPushButton::released, this, [this]() { finishPress(decrement_button); });
    QObject::connect(&increment_button, &QPushButton::released, this, [this]() { finishPress(increment_button); });

    save_timer.setSingleShot(true);
    save_timer.setInterval(150);
    QObject::connect(&save_timer, &QTimer::timeout, this, &FrogPilotParamValueControl::updateParam);
  }

  void hideEvent(QHideEvent *event) override {
    AbstractControl::hideEvent(event);
    decrement_button.setDown(false);
    increment_button.setDown(false);
    warning_shown = false;
    updateParam();
  }

  virtual void refresh() {
    save_timer.stop();
    float stored = key_type == ParamKeyType::INT ? params.getInt(key) : params.getFloat(key);
    stored = std::round(stored * factor) / factor;

    value = std::clamp(stored, min_value, max_value);
    previous_value = stored;
    hold_accelerated = false;
    hold_start_value = value;

    updateDisplay();
  }

  void setPrecision(int decimalPlaces) {
    decimals = decimalPlaces;
    factor = std::pow(10, decimals);

    updateLabelWidth();
  }

  void setWarning(const QString &newWarning) {
    warning = newWarning;
  }

  void showEvent(QShowEvent *event) override {
    refresh();
  }

  void updateControl(float newMinValue, float newMaxValue, const std::map<float, QString> &newValueLabels = {}, const QString &newLabel = QString()) {
    min_value = newMinValue;
    max_value = newMaxValue;

    value_labels = newValueLabels;

    label = newLabel;

    updateLabelWidth();

    refresh();
  }

  void updateParam() {
    save_timer.stop();
    if (value == previous_value) {
      return;
    }

    if (key_type == ParamKeyType::INT) {
      params.putInt(key, value);
    } else {
      params.putFloat(key, value);
    }
    previous_value = value;
  }

signals:
  void valueChanged(float value);

protected:
  QLabel *value_label;

  Params params;

private:
  void changeValue(int direction) {
    save_timer.stop();

    if (!warning.isEmpty() && !warning_shown) {
      showWarning();
      return;
    }

    float delta = interval;
    if (fast_increase && hold_accelerated) {
      delta *= 5;
    }
    value = std::clamp(value + direction * delta, min_value, max_value);

    updateValue();

    if (std::abs(value - hold_start_value) > 5 * interval && std::lround(value / interval) % 5 == 0) {
      hold_accelerated = true;
    }
  }

  void finishPress(const QPushButton &button) {
    if (button.isDown()) {
      return;
    }

    hold_accelerated = false;
    hold_start_value = value;
    if (!decrement_button.isDown() && !increment_button.isDown()) {
      save_timer.start();
    }
  }

  QString numberText(float number) {
    return QString::number(number, 'f', std::max(decimals, 0)) + label;
  }

  void setupButton(QPushButton &button, const QString &text) {
    static const QString valueButtonStyle = buttonStyle + " QPushButton { font-size: 50px; }";

    button.setAutoRepeat(true);
    button.setAutoRepeatDelay(500);
    button.setAutoRepeatInterval(150);
    button.setFixedSize(150, 100);
    button.setStyleSheet(valueButtonStyle);
    button.setText(text);
  }

  void showWarning() {
    warning_shown = true;

    decrement_button.setDown(false);
    increment_button.setDown(false);

    ConfirmationDialog::alert(warning, this);
  }

  void updateDisplay() {
    QString displayText = numberText(value);

    long roundedValue = std::lround(value * factor);
    for (const auto &[labelValue, labelText] : value_labels) {
      if (std::lround(labelValue * factor) == roundedValue) {
        displayText = labelText;
        break;
      }
    }

    decrement_button.setEnabled(value > min_value);
    increment_button.setEnabled(value < max_value);

    value_label->setText(displayText);
  }

  void updateLabelWidth() {
    InterFont unkernedFont(50);
    unkernedFont.setKerning(false);

    QFontMetricsF metrics(InterFont(50));
    QFontMetricsF unkernedMetrics(unkernedFont);

    QChar widestDigit = '0';
    for (char digit = '1'; digit <= '9'; ++digit) {
      if (metrics.horizontalAdvance(digit) > metrics.horizontalAdvance(widestDigit)) {
        widestDigit = digit;
      }
    }

    QStringList texts{numberText(min_value), numberText(max_value)};

    long roundedMinimum = std::lround(min_value * factor);
    long roundedMaximum = std::lround(max_value * factor);
    for (const auto &[labelValue, labelText] : value_labels) {
      long roundedValue = std::lround(labelValue * factor);
      if (roundedValue >= roundedMinimum && roundedValue <= roundedMaximum) {
        texts.append(labelText);
      }
    }

    for (QString &text : texts) {
      for (QChar &character : text) {
        if (character >= '0' && character <= '9') {
          character = widestDigit;
        }
      }
    }
    texts.removeDuplicates();

    qreal width = 0;
    for (const QString &text : texts) {
      width = std::max({width, metrics.horizontalAdvance(text), unkernedMetrics.horizontalAdvance(text)});
    }
    value_label->setFixedSize(std::ceil(width), 100);
  }

  void updateValue() {
    value = std::clamp(std::round(value * factor) / factor, min_value, max_value);

    emit valueChanged(value);

    updateDisplay();
  }

  bool fast_increase;
  bool hold_accelerated = false;
  bool warning_shown = false;

  float factor;
  float hold_start_value = 0.0f;
  float interval;
  float max_value;
  float min_value;
  float previous_value = 0.0f;
  float value = 0.0f;

  int decimals;

  std::map<float, QString> value_labels;

  std::string key;

  ParamKeyType key_type;

  QPushButton decrement_button;
  QPushButton increment_button;

  QString label;
  QString warning;

  QTimer save_timer;
};

class FrogPilotParamValueButtonControl : public FrogPilotParamValueControl {
  Q_OBJECT
public:
  FrogPilotParamValueButtonControl(const QString &param, const QString &title, const QString &desc, const QString &icon,
                                   float min_value, float max_value, const QString &label, const std::map<float, QString> &value_labels,
                                   float interval, bool fast_increase, const std::vector<QString> &button_params, const std::vector<QString> &button_texts,
                                   bool left_button = false)
                                   : FrogPilotParamValueControl(param, title, desc, icon, min_value, max_value, label, value_labels, interval, fast_increase),
                                     button_params(button_params) {
    button_group = new QButtonGroup(this);
    button_group->setExclusive(false);
    for (int i = 0; i < button_texts.size(); i++) {
      QPushButton *button = new QPushButton(button_texts[i], this);
      button->setCheckable(!button_params.empty());
      button->setStyleSheet(buttonStyle);
      button->setMinimumWidth(225);
      if (left_button) {
        hlayout->insertWidget(hlayout->indexOf(value_label) - 1, button);
      } else {
        hlayout->addWidget(button);
      }
      button_group->addButton(button, i);
    }

    QObject::connect(button_group, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &FrogPilotParamValueButtonControl::onButtonClicked);
  }

  void refresh() override {
    for (int i = 0; i < button_params.size(); ++i) {
      button_group->button(i)->setChecked(params.getBool(button_params[i].toStdString()));
    }
    FrogPilotParamValueControl::refresh();
  }

signals:
  void buttonClicked(int id);

private:
  void onButtonClicked(int id) {
    if (!button_params.empty()) {
      params.putBool(button_params[id].toStdString(), button_group->button(id)->isChecked());
    }

    emit buttonClicked(id);
  }

  std::vector<QString> button_params;

  QButtonGroup *button_group;
};
