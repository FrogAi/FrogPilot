#pragma once

#include <cmath>
#include <utility>

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QMovie>
#include <QRegularExpression>
#include <QTimer>

#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/qt/widgets/controls.h"

#include "frogpilot/ui/qt/widgets/frogpilot_helpers.h"

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

extern const QString buttonStyle;

class FrogPilotConfirmationDialog : public ConfirmationDialog {
  Q_OBJECT

public:
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
  void clear() {
    while (QLayoutItem *child = inner_layout.takeAt(0)) {
      if (child->widget()) {
        child->widget()->deleteLater();
      }
      delete child;
    }
  }

private:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setPen(Qt::gray);
    const int itemCount = inner_layout.count();
    int visibleWidgetsAhead = 0;
    for (int i = 0; i < itemCount; ++i) {
      QWidget *widget = inner_layout.itemAt(i)->widget();
      if (widget != nullptr && widget->isVisible()) {
        visibleWidgetsAhead++;
      }
    }

    for (int i = 0; i < itemCount - 1; ++i) {
      QWidget *widget = inner_layout.itemAt(i)->widget();
      const bool widgetVisible = widget != nullptr && widget->isVisible();
      visibleWidgetsAhead -= widgetVisible;

      if (widget == nullptr || (widgetVisible && visibleWidgetsAhead > 0)) {
        QRect r = inner_layout.itemAt(i)->geometry();
        int bottom = r.bottom() + inner_layout.spacing() / 2;
        p.drawLine(r.left() + 40, bottom, r.right() - 40, bottom);
      }
    }
  }
  QVBoxLayout outer_layout;
  QVBoxLayout inner_layout;
};

class FrogPilotButtonGroup {
public:
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

  void setButtonsEnabled(bool enable) {
    for (QAbstractButton *button : button_group->buttons()) {
      button->setEnabled(enable);
    }
  }

  void setCheckedButton(int id) {
    if (QAbstractButton *button = button_group->button(id)) {
      button->setChecked(true);
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

protected:
  void buildButtons(QWidget *owner, QBoxLayout *layout, const std::vector<QString> &button_texts,
                    bool checkable, bool exclusive, int minimum_button_width, const QString &style) {
    button_group = new QButtonGroup(owner);
    button_group->setExclusive(exclusive);

    for (int i = 0; i < button_texts.size(); i++) {
      QPushButton *button = new QPushButton(button_texts[i], owner);
      button->setCheckable(checkable);
      button->setStyleSheet(style);
      button->setMinimumWidth(minimum_button_width);
      layout->addWidget(button);
      button_group->addButton(button, i);
    }
  }

  QButtonGroup *button_group = nullptr;
};

class FrogPilotButtonControl : public ParamControl, public FrogPilotButtonGroup {
  Q_OBJECT
public:
  FrogPilotButtonControl(const QString &param, const QString &title, const QString &desc, const QString &icon,
                         const std::vector<QString> &button_texts, bool checkable = false,
                         bool exclusive = false, int minimum_button_width = 225) : ParamControl(param, title, desc, icon) {
    buildButtons(this, hlayout, button_texts, checkable, exclusive, minimum_button_width, buttonStyle);

    hlayout->removeWidget(&toggle);
    hlayout->addWidget(&toggle);

    QObject::connect(button_group, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &FrogPilotButtonControl::onButtonClicked);

    QObject::connect(this, &ToggleControl::toggleFlipped, this, &FrogPilotButtonControl::refresh);
  }

  void refresh() override {
    ParamControl::refresh();

    for (QAbstractButton *button : button_group->buttons()) {
      button->setEnabled(toggle.on);
    }
  }

  void showEvent(QShowEvent *event) override {
    refresh();
  }

signals:
  void buttonClicked(int id);

protected:
  virtual void onButtonClicked(int id) {
    emit buttonClicked(id);
  }

};

class FrogPilotButtonsControl : public AbstractControl, public FrogPilotButtonGroup {
  Q_OBJECT
public:
  FrogPilotButtonsControl(const QString &title, const QString &desc, const QString &icon,
                          const std::vector<QString> &button_texts, const bool &checkable = false, const bool &exclusive = true,
                          const int minimum_button_width = 225) : AbstractControl(title, desc, icon) {
    buildButtons(this, hlayout, button_texts, checkable, exclusive, minimum_button_width, buttonStyle);

    QObject::connect(button_group, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &FrogPilotButtonsControl::buttonClicked);
  }

signals:
  void buttonClicked(int id);
};

class FrogPilotButtonToggleControl : public FrogPilotButtonControl {
public:
  FrogPilotButtonToggleControl(const QString &param, const QString &title, const QString &desc, const QString &icon,
                               const std::vector<QString> &button_params, const std::vector<QString> &button_texts,
                               bool exclusive = false, int minimum_button_width = 225)
    : FrogPilotButtonControl(param, title, desc, icon, button_texts, true, exclusive, minimum_button_width), button_params(button_params) {

    for (int i = 0; i < button_params.size(); ++i) {
      button_group->button(i)->setChecked(params.getBool(button_params[i].toStdString()));
    }

  }

  void onButtonClicked(int id) override {
    params.putBool(button_params[id].toStdString(), button_group->button(id)->isChecked());

    emit buttonClicked(id);
  }

  void refresh() override {
    FrogPilotButtonControl::refresh();

    for (int i = 0; i < button_params.size(); ++i) {
      button_group->button(i)->setChecked(params.getBool(button_params[i].toStdString()));
    }
  }

private:
  std::vector<QString> button_params;
};

class FrogPilotManageControl : public ParamControl {
  Q_OBJECT
public:
  FrogPilotManageControl(const QString &param, const QString &title, const QString &desc, const QString &icon) : ParamControl(param, title, desc, icon) {
    manageButton = new QPushButton(tr("MANAGE"), this);
    manageButton->setFixedSize(250, 100);
    manageButton->setStyleSheet(buttonStyle);

    hlayout->insertWidget(hlayout->indexOf(&toggle), manageButton);

    QObject::connect(manageButton, &QPushButton::clicked, this, &FrogPilotManageControl::manageButtonClicked);
    QObject::connect(this, &ToggleControl::toggleFlipped, this, &FrogPilotManageControl::refresh);
  }

  void refresh() override {
    ParamControl::refresh();
    manageButton->setEnabled(toggle.on);
  }

signals:
  void manageButtonClicked();

private:
  QPushButton *manageButton;
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

    decrement_repeating_timer.setSingleShot(true);
    increment_repeating_timer.setSingleShot(true);

    QObject::connect(&decrement_button, &QPushButton::released, [this]() {
      decrement_repeating_timer.start(decrement_button.autoRepeatInterval());
    });
    QObject::connect(&increment_button, &QPushButton::released, [this]() {
      increment_repeating_timer.start(increment_button.autoRepeatInterval());
    });

    QObject::connect(&decrement_repeating_timer, &QTimer::timeout, [this]() {
      decrement_repeating = false;
      previous_value = value;
    });
    QObject::connect(&increment_repeating_timer, &QTimer::timeout, [this]() {
      increment_repeating = false;
      previous_value = value;
    });
  }

  void decrementPressed() {
    if (display_warning && !warning_shown) {
      showWarning();
      return;
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

    if (value_dirty) {
      updateParam();

      value_dirty = false;
    }
  }

  void incrementPressed() {
    if (display_warning && !warning_shown) {
      showWarning();
      return;
    }

    float delta = increment_repeating && fast_increase ? interval * 5 : interval;
    value = std::min(value + delta, max_value);

    updateValue();

    increment_repeating |= std::lround(value / interval) % 5 == 0;
    increment_repeating &= std::abs(value - previous_value) > 5 * interval;
    increment_repeating |= delta == interval * 5;
  }

  virtual void refresh() {
    float stored = std::round(params.getFloat(key) * factor) / factor;

    value = std::clamp(stored, min_value, max_value);
    previous_value = value;

    value_dirty |= value != stored;

    updateDisplay();
  }

  void setWarning(const QString &newWarning) {
    display_warning = true;

    warning = newWarning;
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

  void showEvent(QShowEvent *event) override {
    refresh();
  }

  void showWarning() {
    warning_shown = true;

    decrement_button.setDown(false);
    increment_button.setDown(false);

    ConfirmationDialog::alert(warning, this);
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
    previous_value = value;
  }

  void updateValue() {
    value = std::clamp(std::round(value * factor) / factor, min_value, max_value);

    value_dirty = true;

    emit valueChanged(value);

    updateDisplay();
  }

signals:
  void valueChanged(float value);

protected:
  QLabel *value_label;

  Params params;

private:
  bool decrement_repeating = false;
  bool display_warning = false;
  bool fast_increase;
  bool increment_repeating = false;
  bool value_dirty = false;
  bool warning_shown = false;

  float interval;
  float factor;
  float max_value;
  float min_value;
  float previous_value = 0.0f;
  float value = 0.0f;

  std::map<float, QString> value_labels;

  std::string key;

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

  void refresh() override {
    if (checkable) {
      for (int i = 0; i < button_params.size(); ++i) {
        button_group->button(i)->setChecked(params.getBool(button_params[i].toStdString()));
      }
    }
    FrogPilotParamValueControl::refresh();
  }

signals:
  void buttonClicked(int id);

private:
  bool checkable;

  std::vector<QString> button_params;

  QButtonGroup *button_group;
};

class FrogPilotDualParamValueControl : public QFrame {
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

private:
  FrogPilotParamValueControl *control1;
  FrogPilotParamValueControl *control2;
};
