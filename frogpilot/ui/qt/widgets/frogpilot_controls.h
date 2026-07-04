#pragma once

#include <cmath>

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMovie>
#include <QRegularExpression>
#include <QTimer>

#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/qt/widgets/controls.h"

bool useKonikServer();

void loadGif(const QString &gifPath, QSharedPointer<QMovie> &movie, const QSize &size, QWidget *parent);
void loadImage(const QString &basePath, QPixmap &pixmap, QSharedPointer<QMovie> &movie, const QSize &size, QWidget *parent);
void openDescriptions(bool forceOpenDescriptions, std::map<QString, AbstractControl*> toggles);
void updateFrogPilotToggles();

QColor loadThemeColors(const QString &colorKey, bool clearCache = false);

QString processModelName(const QString &modelName);

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

class FrogPilotConfirmationDialog : public ConfirmationDialog {
  Q_OBJECT

public:
  explicit FrogPilotConfirmationDialog(const QString &prompt_text, const QString &confirm_text,
                                       const QString &cancel_text, const bool rich, QWidget *parent);
  static bool toggleReboot(QWidget *parent);
  static bool yesorno(const QString &prompt_text, QWidget *parent);
};

class FrogPilotListWidget : public QWidget {
  Q_OBJECT
 public:
  explicit FrogPilotListWidget(QWidget *parent = 0) : QWidget(parent), outer_layout(this) {
    outer_layout.setMargin(0);
    outer_layout.setSpacing(0);
    outer_layout.addLayout(&inner_layout);
    inner_layout.setMargin(0);
    inner_layout.setSpacing(25); // default spacing is 25
    outer_layout.addStretch();
  }
  inline void addItem(QWidget *w, bool expanding = false) {
    w->setSizePolicy(QSizePolicy::Preferred, expanding ? QSizePolicy::Expanding : QSizePolicy::Maximum);
    inner_layout.addWidget(w);
  }
  inline void addItem(QLayout *layout) { inner_layout.addLayout(layout); }
  inline void setSpacing(int spacing) { inner_layout.setSpacing(spacing); }

  void clear() {
    while (QLayoutItem *child = inner_layout.takeAt(0)) {
      if (child->widget()) {
        child->widget()->deleteLater();
      }
      delete child;
    }
    outer_layout.addStretch();
  }

private:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setPen(Qt::gray);
    for (int i = 0; i < inner_layout.count() - 1; ++i) {
      QWidget *widget = inner_layout.itemAt(i)->widget();

      QWidget *nextWidget = nullptr;
      for (int j = i + 1; j < inner_layout.count(); ++j) {
        nextWidget = inner_layout.itemAt(j)->widget();
        if (nextWidget != nullptr && nextWidget->isVisible()) {
          break;
        }
      }

      if (widget == nullptr || (widget->isVisible() && nextWidget->isVisible())) {
        QRect r = inner_layout.itemAt(i)->geometry();
        int bottom = r.bottom() + inner_layout.spacing() / 2;
        p.drawLine(r.left() + 40, bottom, r.right() - 40, bottom);
      }
    }
  }
  QVBoxLayout outer_layout;
  QVBoxLayout inner_layout;
};

class FrogPilotButtonControl : public ParamControl {
  Q_OBJECT
public:
  FrogPilotButtonControl(const QString &param, const QString &title, const QString &desc, const QString &icon,
                         const std::vector<QString> &button_texts, bool checkable = false,
                         bool exclusive = false, int minimum_button_width = 225) : ParamControl(param, title, desc, icon) {
    key = param.toStdString();

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

    hlayout->addWidget(&toggle);

    QObject::connect(button_group, QOverload<int>::of(&QButtonGroup::buttonClicked), [=](int id) {
      emit buttonClicked(id);
    });

    QObject::connect(this, &ToggleControl::toggleFlipped, this, &FrogPilotButtonControl::refresh);
  }

  virtual void refresh() {
    bool state = params.getBool(key);
    if (state != toggle.on) {
      toggle.togglePosition();
    }

    for (QAbstractButton *button : button_group->buttons()) {
      button->setEnabled(state);
    }
  }

  void clearCheckedButtons(bool clear_exclusivity = false) {
    if (clear_exclusivity) {
      button_group->setExclusive(false);
    }

    for (QAbstractButton *button : button_group->buttons()) {
      button->setChecked(false);
    }

    if (clear_exclusivity) {
      button_group->setExclusive(true);
    }
  }

  void setCheckedButton(int id) {
    if (QAbstractButton *button = button_group->button(id)) {
      button->setChecked(true);
    }
  }

  void setEnabledButton(int id, bool enable) {
    if (QAbstractButton *button = button_group->button(id)) {
      button->setEnabled(enable);
    }
  }

  void setVisibleButton(int id, bool visible) {
    if (QAbstractButton *button = button_group->button(id)) {
      button->setVisible(visible);
    }
  }

  void showEvent(QShowEvent *event) override {
    refresh();
  }

signals:
  void buttonClicked(int id);

protected:
  std::string key;

  Params params;

  QButtonGroup *button_group;
};

class FrogPilotButtonsControl : public AbstractControl {
  Q_OBJECT
public:
  FrogPilotButtonsControl(const QString &title, const QString &desc, const QString &icon,
                          const std::vector<QString> &button_texts, const bool &checkable = false, const bool &exclusive = true,
                          const int minimum_button_width = 225) : AbstractControl(title, desc, icon) {
    button_group = new QButtonGroup(this);
    button_group->setExclusive(exclusive);
    for (int i = 0; i < button_texts.size(); i++) {
      QPushButton *button = new QPushButton(button_texts[i], this);
      button->installEventFilter(this);
      button->setCheckable(checkable);
      button->setStyleSheet(buttonStyle);
      button->setMinimumWidth(minimum_button_width);
      hlayout->addWidget(button);
      button_group->addButton(button, i);
    }

    QObject::connect(button_group, QOverload<int>::of(&QButtonGroup::buttonClicked), [=](int id) {
      emit buttonClicked(id);
    });
  }

  void clearCheckedButtons(bool clear_exclusivity = false) {
    if (clear_exclusivity) {
      button_group->setExclusive(false);
    }

    for (QAbstractButton *button : button_group->buttons()) {
      button->setChecked(false);
    }

    if (clear_exclusivity) {
      button_group->setExclusive(true);
    }
  }

  void setCheckedButton(int id) {
    if (QAbstractButton *button = button_group->button(id)) {
      button->setChecked(true);
    }
  }

  void setEnabled(bool enable) {
    for (QAbstractButton *button : button_group->buttons()) {
      button->setEnabled(enable);
    }
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
  void disabledButtonClicked(int id);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override {
    if (event->type() == QEvent::MouseButtonPress) {
      QPushButton *button = qobject_cast<QPushButton *>(obj);
      if (button && !button->isEnabled()) {
        emit disabledButtonClicked(button_group->id(button));
      }
    }
    return AbstractControl::eventFilter(obj, event);
  }

private:
  QButtonGroup *button_group;
};

class FrogPilotButtonToggleControl : public FrogPilotButtonControl {
  Q_OBJECT
public:
  FrogPilotButtonToggleControl(const QString &param, const QString &title, const QString &desc, const QString &icon,
                               const std::vector<QString> &button_params, const std::vector<QString> &button_texts,
                               bool exclusive = false, int minimum_button_width = 225)
    : FrogPilotButtonControl(param, title, desc, icon, button_texts, true, exclusive, minimum_button_width), button_params(button_params) {

    for (int i = 0; i < button_texts.size(); i++) {
      button_group->buttons()[i]->setChecked(params.getBool(button_params[i].toStdString()));
    }

    QObject::connect(button_group, QOverload<int>::of(&QButtonGroup::buttonClicked), [=](int id) {
      params.putBool(button_params[id].toStdString(), button_group->button(id)->isChecked());
      emit buttonClicked(id);
    });
  }

  void refresh() override {
    FrogPilotButtonControl::refresh();

    for (int i = 0; i < button_group->buttons().size(); i++) {
      QAbstractButton *button = button_group->button(i);
      button->setChecked(params.getBool(button_params[i].toStdString()));
    }
  }

private:
  std::vector<QString> button_params;
};

class FrogPilotManageControl : public ParamControl {
  Q_OBJECT
public:
  FrogPilotManageControl(const QString &param, const QString &title, const QString &desc, const QString &icon) : ParamControl(param, title, desc, icon) {
    key = param.toStdString();

    manageButton = new ButtonControl("", tr("MANAGE"), "", this);

    hlayout->insertWidget(hlayout->indexOf(&toggle) - 1, manageButton);

    QObject::connect(manageButton, &ButtonControl::clicked, this, &FrogPilotManageControl::manageButtonClicked);
    QObject::connect(this, &ToggleControl::toggleFlipped, this, &FrogPilotManageControl::refresh);
  }

  void refresh() {
    manageButton->setEnabled(params.getBool(key));
  }

  void setManageVisibility(bool visible) {
    manageButton->setVisible(visible);
  }

  void showEvent(QShowEvent *event) override {
    refresh();
    ParamControl::showEvent(event);
  }

signals:
  void manageButtonClicked();

private:
  std::string key;

  ButtonControl *manageButton;

  Params params;
};

class FrogPilotParamValueControl : public AbstractControl {
  Q_OBJECT
public:
  FrogPilotParamValueControl(const QString &param, const QString &title, const QString &desc, const QString &icon,
                             float min_value, float max_value, const QString &label, const std::map<float, QString> &value_labels = {},
                             float interval = 1.0f, bool fast_increase = false, int label_width = 350)
                             : AbstractControl(title, desc, icon),
                               fast_increase(fast_increase), interval(interval), label(label), min_value(min_value), max_value(max_value), value_labels(value_labels) {
    factor = std::pow(10, std::ceil(-std::log10(interval)));
    key = param.toStdString();

    setupButton(decrement_button, "-");
    setupButton(increment_button, "+");

    value_label = new QLabel(this);
    value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    value_label->setFixedSize(QSize(label_width, 100));
    value_label->setStyleSheet("QLabel {color: #E0E879;}");

    hlayout->addWidget(value_label);
    hlayout->addWidget(&decrement_button);
    hlayout->addWidget(&increment_button);

    QObject::connect(&decrement_button, &QPushButton::pressed, this, &FrogPilotParamValueControl::decrementPressed);
    QObject::connect(&increment_button, &QPushButton::pressed, this, &FrogPilotParamValueControl::incrementPressed);

    QObject::connect(&decrement_button, &QPushButton::released, [this]() {
      decrement_repeating_timer.start(decrement_button.autoRepeatInterval());
    });
    QObject::connect(&increment_button, &QPushButton::released, [this]() {
      increment_repeating_timer.start(increment_button.autoRepeatInterval());
    });

    QObject::connect(&decrement_repeating_timer, &QTimer::timeout, [this]() {
      decrement_repeating = false;
    });
    QObject::connect(&increment_repeating_timer, &QTimer::timeout, [this]() {
      increment_repeating = false;
    });
  }

  void decrementPressed() {
    if (display_warning && !warning_shown) {
      showWarning();
    }

    float delta = decrement_repeating && fast_increase ? interval * 5 : interval;
    value = std::max(value - delta, min_value);

    updateValue();

    decrement_repeating |= std::lround(value / interval) % 5 == 0;
    decrement_repeating &= std::abs(value - previous_value) > 5 * interval;
    decrement_repeating |= delta == interval * 5;
  }

  void hideEvent(QHideEvent *event) override {
    AbstractControl::hideEvent(event);

    warning_shown = false;

    updateParam();
  }

  void incrementPressed() {
    if (display_warning && !warning_shown) {
      showWarning();
    }

    float delta = increment_repeating && fast_increase ? interval * 5 : interval;
    value = std::min(value + delta, max_value);

    updateValue();

    increment_repeating |= std::lround(value / interval) % 5 == 0;
    increment_repeating &= std::abs(value - previous_value) > 5 * interval;
    increment_repeating |= delta == interval * 5;
  }

  void refresh() {
    value = std::clamp(std::round(params.getFloat(key) * factor) / factor, min_value, max_value);
    previous_value = value;

    updateDisplay();
    updateParam();
  }

  void setWarning(const QString &newWarning) {
    display_warning = true;

    warning = newWarning;
  }

  void setupButton(QPushButton &button, const QString &text) {
    button.setAutoRepeat(true);
    button.setAutoRepeatDelay(500);
    button.setAutoRepeatInterval(150);
    button.setFixedSize(150, 100);
    button.setStyleSheet(buttonStyle);
    if (text == "+" || text == "-") {
      button.setStyleSheet(button.styleSheet() + " QPushButton { font-size: 50px; }");
    }
    button.setText(text);
  }

  void showEvent(QShowEvent *event) override {
    refresh();
  }

  void showWarning() {
    ConfirmationDialog::alert(warning, this);

    warning_shown = true;
  }

  void updateControl(const float &newMinValue, const float &newMaxValue, const std::map<float, QString> &newValueLabels = {}) {
    min_value = newMinValue;
    max_value = newMaxValue;

    value_labels = newValueLabels;

    refresh();
  }

  void updateDisplay() {
    QString displayText = QString::number(value) + label;

    for (const std::pair<const float, QString> &entry : value_labels) {
      if (std::lround(entry.first * factor) == std::lround(value * factor)) {
        displayText = entry.second;
        break;
      }
    }

    value_label->setText(displayText);
  }

  void updateParam() {
    params.putFloat(key, value);
  }

  void updateValue() {
    value = std::round(value * factor) / factor;

    emit valueChanged(value);

    updateDisplay();
  }

signals:
  void valueChanged(float value);

protected:
  QLabel *value_label;

private:
  bool decrement_repeating;
  bool display_warning;
  bool fast_increase;
  bool increment_repeating;
  bool warning_shown;

  float interval;
  float factor;
  float max_value;
  float min_value;
  float previous_value;
  float value;

  std::map<float, QString> value_labels;

  std::string key;

  Params params;

  QPushButton decrement_button;
  QPushButton increment_button;

  QString label;
  QString warning;

  QTimer decrement_repeating_timer;
  QTimer increment_repeating_timer;
};

class FrogPilotParamValueButtonControl : public FrogPilotParamValueControl {
  Q_OBJECT
public:
  FrogPilotParamValueButtonControl(const QString &param, const QString &title, const QString &desc, const QString &icon,
                                   float min_value, float max_value, const QString &label, const std::map<float, QString> &value_labels,
                                   float interval, bool fast_increase, const std::vector<QString> &button_params, const std::vector<QString> &button_texts,
                                   bool left_button = false, bool checkable = true, int minimum_button_width = 225)
                                   : FrogPilotParamValueControl(param, title, desc, icon, min_value, max_value, label, value_labels, interval, fast_increase, 200),
                                     button_params(button_params), checkable(checkable) {
    button_group = new QButtonGroup(this);
    button_group->setExclusive(false);
    for (int i = 0; i < button_texts.size(); i++) {
      QPushButton *button = new QPushButton(button_texts[i], this);
      button->setCheckable(checkable);
      button->setChecked(checkable && params.getBool(button_params[i].toStdString()));
      button->setStyleSheet(buttonStyle);
      button->setMinimumWidth(minimum_button_width);
      if (left_button) {
        hlayout->insertWidget(hlayout->indexOf(value_label) - 1, button);
      } else {
        hlayout->addWidget(button);
      }
      button_group->addButton(button, i);
    }

    QObject::connect(button_group, QOverload<int>::of(&QButtonGroup::buttonClicked), [=](int id) {
      if (checkable) {
        params.putBool(button_params[id].toStdString(), button_group->button(id)->isChecked());
      }
      emit buttonClicked(id);
    });
  }

  void refresh() {
    if (checkable) {
      for (int i = 0; i < button_group->buttons().size(); i++) {
        QAbstractButton *button = button_group->button(i);
        button->setChecked(params.getBool(button_params[i].toStdString()));
      }
    }
    FrogPilotParamValueControl::refresh();
  }

  void showEvent(QShowEvent *event) override {
    refresh();
    FrogPilotParamValueControl::showEvent(event);
  }

signals:
  void buttonClicked(int id);

private:
  bool checkable;

  std::vector<QString> button_params;

  Params params;

  QButtonGroup *button_group;
};

class FrogPilotDualParamValueControl : public QFrame {
  Q_OBJECT
public:
  FrogPilotDualParamValueControl(FrogPilotParamValueControl *control1, FrogPilotParamValueControl *control2, QWidget *parent = nullptr) : QFrame(parent), control1(control1), control2(control2) {
    QHBoxLayout *hlayout = new QHBoxLayout(this);
    hlayout->addWidget(control1);
    hlayout->addWidget(control2);
  }

  void updateControl(const float &newMinValue, const float &newMaxValue, const std::map<float, QString> &newValueLabels = {}) {
    control1->updateControl(newMinValue, newMaxValue, newValueLabels);
    control2->updateControl(newMinValue, newMaxValue, newValueLabels);
  }

  void refresh() {
    control1->refresh();
    control2->refresh();
  }

private:
  FrogPilotParamValueControl *control1;
  FrogPilotParamValueControl *control2;
};
