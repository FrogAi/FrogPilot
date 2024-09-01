#pragma once

#include <cmath>

#include <QRegularExpression>
#include <QTimer>

#include "selfdrive/ui/qt/widgets/controls.h"

QColor loadThemeColors(const QString &colorKey, const bool clearCache = false);

void updateFrogPilotToggles();

inline QString processModelName(const QString &modelName) {
  QString modelCleaned = modelName;
  modelCleaned = modelCleaned.remove(QRegularExpression("[🗺️👀📡]")).simplified();
  modelCleaned = modelCleaned.remove(QRegularExpression("[^a-zA-Z0-9()-]"));
  modelCleaned = modelCleaned.replace(" ", "").replace("(Default)", "").replace("-", "");
  return modelCleaned;
}

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
  static bool toggle(const QString &prompt_text, const QString &confirm_text, QWidget *parent);
  static bool toggleAlert(const QString &prompt_text, const QString &button_text, QWidget *parent);
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
  inline void addItem(QWidget *w) {
    w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    inner_layout.addWidget(w);
  }
  inline void addItem(QLayout *layout) { inner_layout.addLayout(layout); }
  inline void setSpacing(int spacing) { inner_layout.setSpacing(spacing); }

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

      if ((widget == nullptr || widget->isVisible()) && nextWidget != nullptr && nextWidget->isVisible()) {
        QRect r = inner_layout.itemAt(i)->geometry();
        int bottom = r.bottom() + inner_layout.spacing() / 2;
        p.drawLine(r.left() + 40, bottom, r.right() - 40, bottom);
      }
    }
  }
  QVBoxLayout outer_layout;
  QVBoxLayout inner_layout;
};

class FrogPilotButtonsControl : public AbstractControl {
  Q_OBJECT

public:
  FrogPilotButtonsControl(const QString &title, const QString &desc,
                          const std::vector<QString> &buttonLabels,
                          const bool checkable = false, const bool exclusive = true, const QString &icon = "",
                          const int minimumButtonWidth = 225)
    : AbstractControl(title, desc, icon), buttonGroup(new QButtonGroup(this)) {
    buttonGroup->setExclusive(exclusive);
    for (int i = 0; i < buttonLabels.size(); ++i) {
      QPushButton *button = new QPushButton(buttonLabels[i], this);
      button->setCheckable(checkable);
      button->setStyleSheet(buttonStyle);
      button->setMinimumWidth(minimumButtonWidth);
      hlayout->addWidget(button);
      buttonGroup->addButton(button, i);
    }

    QObject::connect(buttonGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), [=](int id) {
      emit buttonClicked(id);
    });
  }

  void setEnabled(bool enable) {
    for (QAbstractButton *btn : buttonGroup->buttons()) {
      btn->setEnabled(enable);
    }
  }

  void setCheckedButton(int id, bool status = true) {
    if (QAbstractButton *button = buttonGroup->button(id)) {
      button->setChecked(status);
    }
  }

  void setEnabledButtons(int id, bool enable) {
    if (QAbstractButton *button = buttonGroup->button(id)) {
      button->setEnabled(enable);
    }
  }

  void setVisibleButton(int id, bool visible) {
    if (QAbstractButton *button = buttonGroup->button(id)) {
      button->setVisible(visible);
    }
  }

  void setText(int id, const QString &text) {
    if (QAbstractButton *button = buttonGroup->button(id)) {
      button->setText(text);
    }
  }

signals:
  void buttonClicked(int id);

private:
  QButtonGroup *buttonGroup;
};

class FrogPilotButtonToggleControl : public ParamControl {
  Q_OBJECT

public:
  FrogPilotButtonToggleControl(const QString &param, const QString &title, const QString &desc,
                               const std::vector<QString> &buttonParams, const std::vector<QString> &buttonLabels,
                               const bool exclusive = false, const int minimumButtonWidth = 225, QWidget *parent = nullptr)
  : ParamControl(param, title, desc, "", parent),
  key(param.toStdString()), buttonParams(buttonParams), buttonGroup(new QButtonGroup(this)) {
    buttonGroup->setExclusive(exclusive);

    for (int i = 0; i < buttonLabels.size(); ++i) {
      QPushButton *button = new QPushButton(buttonLabels[i], this);
      button->setCheckable(true);
      button->setStyleSheet(buttonStyle);
      button->setMinimumWidth(minimumButtonWidth);
      hlayout->addWidget(button);
      buttonGroup->addButton(button, i);
    }

    hlayout->addWidget(&toggle);

    QObject::connect(buttonGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), [=](int id) {
      bool checked = buttonGroup->button(id)->isChecked();
      params.putBool(buttonParams[id].toStdString(), checked);
      emit buttonClicked(id);
    });

    QObject::connect(this, &ToggleControl::toggleFlipped, this, &FrogPilotButtonToggleControl::refresh);
  }

  void refresh() {
    bool state = params.getBool(key);
    if (state != toggle.on) {
      toggle.togglePosition();
    }

    const QList<QAbstractButton *> buttons = buttonGroup->buttons();
    for (int i = 0; i < buttons.size(); ++i) {
      QAbstractButton *button = buttons[i];
      if (button) {
        button->setVisible(state);
        button->setChecked(params.getBool(buttonParams[i].toStdString()));
      }
    }
  }

  void setEnabledButtons(int id, bool enable) {
    if (QAbstractButton *button = buttonGroup->button(id)) {
      button->setEnabled(enable);
    }
  }

  void setVisibleButton(int id, bool visible) {
    if (QAbstractButton *button = buttonGroup->button(id)) {
      button->setVisible(visible);
    }
  }

signals:
  void buttonClicked(int id);

protected:
  void showEvent(QShowEvent *event) override {
    refresh();
    QObject::connect(this, &ToggleControl::toggleFlipped, this, &FrogPilotButtonToggleControl::refresh);
  }

private:
  Params params;

  QButtonGroup *buttonGroup;

  std::string key;
  std::vector<QString> buttonParams;
};

class FrogPilotParamManageControl : public ParamControl {
  Q_OBJECT

public:
  FrogPilotParamManageControl(const QString &param, const QString &title, const QString &desc, const QString &icon, QWidget *parent = nullptr)
    : ParamControl(param, title, desc, icon, parent),
      key(param.toStdString()),
      manageButton(new ButtonControl("", tr("MANAGE"))) {

    hlayout->insertWidget(hlayout->indexOf(&toggle) - 1, manageButton);

    QObject::connect(manageButton, &ButtonControl::clicked, this, &FrogPilotParamManageControl::manageButtonClicked);
    QObject::connect(this, &ToggleControl::toggleFlipped, this, &FrogPilotParamManageControl::refresh);
  }

  void setEnabled(bool enabled) {
    manageButton->setEnabled(enabled && params.getBool(key));

    toggle.setEnabled(enabled);
    toggle.update();
  }

  void refresh() {
    manageButton->setEnabled(params.getBool(key));
  }

signals:
  void manageButtonClicked();

protected:
  void showEvent(QShowEvent *event) override {
    ParamControl::showEvent(event);
    refresh();
  }

private:
  Params params;

  ButtonControl *manageButton;

  std::string key;
};

class FrogPilotParamValueControl : public AbstractControl {
  Q_OBJECT

public:
  FrogPilotParamValueControl(const QString &param, const QString &title, const QString &desc, const QString &icon,
                             const float minValue, const float maxValue, const QString &label = "", const std::map<int, QString> &valueLabels = {},
                             const float interval = 1.0f, const bool compactSize = false, const bool instantUpdate = false)
    : AbstractControl(title, desc, icon), key(param.toStdString()), minValue(minValue), maxValue(maxValue),
      labelText(label), interval(interval), valueLabels(valueLabels),
      decimalPlaces(std::ceil(-std::log10(interval))), factor(std::pow(10.0f, decimalPlaces)),
      instantUpdate(instantUpdate) {

    setupButton(decrementButton, "-");
    setupButton(incrementButton, "+");

    valueLabel = new QLabel(this);
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    valueLabel->setStyleSheet("QLabel { color: #E0E879; }");
    if (compactSize) {
      valueLabel->setFixedSize(175, 100);
    } else {
      valueLabel->setFixedSize(350, 100);
    }

    hlayout->addWidget(valueLabel);
    hlayout->addWidget(&decrementButton);
    hlayout->addWidget(&incrementButton);

    QObject::connect(&decrementButton, &QPushButton::pressed, this, &FrogPilotParamValueControl::onDecrementPressed);
    QObject::connect(&incrementButton, &QPushButton::pressed, this, &FrogPilotParamValueControl::onIncrementPressed);
    QObject::connect(&decrementButton, &QPushButton::released, this, &FrogPilotParamValueControl::onButtonReleased);
    QObject::connect(&incrementButton, &QPushButton::released, this, &FrogPilotParamValueControl::onButtonReleased);
  }

  void updateControl(float newMinValue, float newMaxValue, const QString &newLabel = "") {
    minValue = newMinValue;
    maxValue = newMaxValue;
    labelText = newLabel;
    refresh();
  }

  void refresh() {
    value = params.getFloat(key);
    updateValueDisplay();
  }

signals:
  void valueChanged(float value);

protected:
  void showEvent(QShowEvent *event) override {
    refresh();
  }

private slots:
  void onIncrementPressed() {
    adjustValue(interval);
  }

  void onDecrementPressed() {
    adjustValue(-interval);
  }

  void onButtonReleased() {
    if (instantUpdate) {
      params.putFloatNonBlocking(key, value);
    }

    float lastValue = value;
    QTimer::singleShot(50, this, [this, lastValue]() {
      if (lastValue != value) {
        return;
      }

      previousDelta = false;
      if (!instantUpdate) {
        params.putFloat(key, value);
        emit valueChanged(value);
      }
    });
  }

private:
  void adjustValue(float delta) {
    float modResult = fmod(value, 5.0f * interval);
    if (modResult < interval) {
      if (previousDelta) {
        delta *= 5;
      }
      previousDelta = true;
    }

    value = qBound(minValue, value + delta, maxValue);
    value = std::round(value * factor) / factor;

    updateValueDisplay();

    if (instantUpdate) {
      emit valueChanged(value);
    }
  }

  void updateValueDisplay() {
    int intValue = static_cast<int>(value);
    if (valueLabels.count(intValue)) {
      valueLabel->setText(valueLabels.at(intValue));
    } else {
      valueLabel->setText(QString::number(value, 'f', decimalPlaces) + labelText);
    }
  }

  void setupButton(QPushButton &button, const QString &text) {
    button.setFixedSize(150, 100);
    button.setText(text);
    button.setAutoRepeat(true);
    button.setAutoRepeatInterval(150);
    button.setAutoRepeatDelay(500);
    button.setStyleSheet(R"(
      QPushButton {
        border-radius: 50px;
        font-size: 50px;
        font-weight: 500;
        height: 100px;
        padding: 0 25px;
        color: #E4E4E4;
        background-color: #393939;
      }
      QPushButton:pressed {
        background-color: #4a4a4a;
      }
    )");
  }

  Params params;

  QLabel *valueLabel;

  QPushButton decrementButton;
  QPushButton incrementButton;

  QString labelText;

  bool instantUpdate;
  bool previousDelta;

  int decimalPlaces;

  float factor;
  float interval;
  float minValue;
  float maxValue;
  float value;

  std::map<int, QString> valueLabels;

  std::string key;
};

class FrogPilotParamValueButtonControl : public FrogPilotParamValueControl {
  Q_OBJECT

public:
  FrogPilotParamValueButtonControl(const QString &param, const QString &title, const QString &desc, const QString &icon,
                                   const float minValue, const float maxValue, const QString &label = "", const std::map<int, QString> &valueLabels = {},
                                   const float interval = 1.0f,
                                   const std::vector<QString> &buttonParams = {}, const std::vector<QString> &buttonLabels = {},
                                   const bool checkable = true, const int minimumButtonWidth = 225)
    : FrogPilotParamValueControl(param, title, desc, icon, minValue, maxValue, label, valueLabels, interval, true),
      buttonParams(buttonParams),
      buttonGroup(new QButtonGroup(this)),
      checkable(checkable) {

    buttonGroup->setExclusive(false);

    for (int i = 0; i < buttonLabels.size(); ++i) {
      QPushButton *button = new QPushButton(buttonLabels[i], this);
      button->setCheckable(checkable);
      button->setStyleSheet(buttonStyle);
      button->setMinimumWidth(minimumButtonWidth);

      hlayout->addWidget(button);
      buttonGroup->addButton(button, i);
    }

    QObject::connect(buttonGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), [=](int id) {
      if (checkable) {
        bool checked = buttonGroup->button(id)->isChecked();
        params.putBool(buttonParams[id].toStdString(), checked);
      }
      emit buttonClicked(id);
    });

    QObject::connect(this, &FrogPilotParamValueControl::valueChanged, this, &FrogPilotParamValueButtonControl::refresh);
  }

  void refresh() {
    if (checkable) {
      const QList<QAbstractButton *> buttons = buttonGroup->buttons();
      for (int i = 0; i < buttons.size(); ++i) {
        QAbstractButton *button = buttons[i];
        if (button) {
          button->setChecked(params.getBool(buttonParams[i].toStdString()));
        }
      }
    }
    FrogPilotParamValueControl::refresh();
  }

signals:
  void buttonClicked(int id);

protected:
  void showEvent(QShowEvent *event) override {
    FrogPilotParamValueControl::showEvent(event);
    refresh();
  }

private:
  Params params;

  QButtonGroup *buttonGroup;

  bool checkable;

  std::vector<QString> buttonParams;
};

class FrogPilotDualParamControl : public QFrame {
  Q_OBJECT

public:
  FrogPilotDualParamControl(FrogPilotParamValueControl *control1, FrogPilotParamValueControl *control2, QWidget *parent = nullptr)
    : QFrame(parent), control1(control1), control2(control2) {
    QHBoxLayout *hlayout = new QHBoxLayout(this);
    hlayout->addWidget(control1);
    hlayout->addWidget(control2);

    control1->setObjectName("control1");
    control2->setObjectName("control2");
  }

  void updateControl(float newMinValue, float newMaxValue, const QString &newLabel = "") {
    control1->updateControl(newMinValue, newMaxValue, newLabel);
    control2->updateControl(newMinValue, newMaxValue, newLabel);
  }

  void refresh() {
    control1->refresh();
    control2->refresh();
  }

private:
  FrogPilotParamValueControl *control1;
  FrogPilotParamValueControl *control2;
};

inline void makeConnections(QObject *controlToggle, std::function<void()> slot = updateFrogPilotToggles) {
  if (!controlToggle) {
    return;
  }

  FrogPilotButtonsControl *frogpilotButtonsControl = qobject_cast<FrogPilotButtonsControl*>(controlToggle);
  if (frogpilotButtonsControl) {
    QObject::connect(frogpilotButtonsControl, &FrogPilotButtonsControl::buttonClicked, [slot]() {
      slot();
    });
    return;
  }

  FrogPilotButtonToggleControl *frogpilotToggleButtonControl = qobject_cast<FrogPilotButtonToggleControl*>(controlToggle);
  if (frogpilotToggleButtonControl) {
    QObject::connect(frogpilotToggleButtonControl, &FrogPilotButtonToggleControl::buttonClicked, [slot]() {
      slot();
    });
    return;
  }

  FrogPilotParamValueButtonControl *paramValueButtonControl = qobject_cast<FrogPilotParamValueButtonControl*>(controlToggle);
  if (paramValueButtonControl) {
    QObject::connect(paramValueButtonControl, &FrogPilotParamValueButtonControl::buttonClicked, [slot]() {
      slot();
    });
    QObject::connect(paramValueButtonControl, &FrogPilotParamValueButtonControl::valueChanged, [slot]() {
      slot();
    });
    return;
  }

  FrogPilotParamValueControl *paramValueControl = qobject_cast<FrogPilotParamValueControl*>(controlToggle);
  if (paramValueControl) {
    QObject::connect(paramValueControl, &FrogPilotParamValueControl::valueChanged, [slot]() {
      slot();
    });
    return;
  }

  FrogPilotDualParamControl *dualParamControl = qobject_cast<FrogPilotDualParamControl*>(controlToggle);
  if (dualParamControl) {
    QObject *control1 = dualParamControl->findChild<QObject*>("control1");
    if (control1) {
      makeConnections(control1, slot);
    }

    QObject *control2 = dualParamControl->findChild<QObject*>("control2");
    if (control2) {
      makeConnections(control2, slot);
    }

    return;
  }

  ButtonParamControl *buttonParamControl = qobject_cast<ButtonParamControl*>(controlToggle);
  if (buttonParamControl) {
    QObject::connect(buttonParamControl, &ButtonParamControl::buttonClicked, [slot]() {
      slot();
    });
    return;
  }

  ParamControl *paramControl = qobject_cast<ParamControl*>(controlToggle);
  if (paramControl) {
    QObject::connect(paramControl, &ToggleControl::toggleFlipped, [slot]() {
      slot();
    });
    return;
  }

  ToggleControl *toggleControl = qobject_cast<ToggleControl*>(controlToggle);
  if (toggleControl) {
    QObject::connect(toggleControl, &ToggleControl::toggleFlipped, [slot]() {
      slot();
    });
  }
}
