#include "frogpilot/ui/qt/offroad/data_settings.h"

FrogPilotDataPanel::FrogPilotDataPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent) {
  QStackedLayout *dataLayout = new QStackedLayout();
  addItem(dataLayout);

  FrogPilotListWidget *dataMainList = new FrogPilotListWidget(this);
  ScrollView *dataMainPanel = new ScrollView(dataMainList, this);
  dataLayout->addWidget(dataMainPanel);

  FrogPilotListWidget *statsLabelsList = new FrogPilotListWidget(this);
  ScrollView *statsLabelsPanel = new ScrollView(statsLabelsList, this);
  dataLayout->addWidget(statsLabelsPanel);

  FrogPilotButtonsControl *viewStatsButton = new FrogPilotButtonsControl(tr("FrogPilot Stats"), tr("<b>See everything FrogPilot has tracked about your driving, or reset the numbers and start over.</b><br><br>Stats can only be reset while the car is off."), "", {tr("RESET"), tr("VIEW")});
  QObject::connect(viewStatsButton, &FrogPilotButtonsControl::buttonClicked, [dataLayout, statsLabelsList, statsLabelsPanel, this](int id) {
    if (id == 0) {
      if (uiState()->scene.started) {
        ConfirmationDialog::alert(tr("Stats can't be reset while the car is on. Turn the car off and try again."), this);
        return;
      }

      if (ConfirmationDialog::confirm(tr("Are you sure you want to reset all of your FrogPilot stats?"), tr("Reset"), this)) {
        params.remove("FrogPilotStats");
      }
    } else if (id == 1) {
      updateStatsLabels(statsLabelsList);

      emit openSubPanel();
      dataLayout->setCurrentWidget(statsLabelsPanel);
    }
  });
  dataMainList->addItem(viewStatsButton);

  accountButton = new FrogPilotButtonsControl(tr("FrogPilot.com Account"), tr("<b>Link this device to your FrogPilot.com account, or delete the data FrogPilot keeps about it.</b><br><br>A linked device's stats and reports show in your account. Both show a code to scan with your phone."), "", {tr("LINK"), tr("DELETE"), tr("UNLINK")});
  accountButton->setVisible(false);
  dataMainList->addItem(accountButton);

  QSizePolicy accountButtonPolicy = accountButton->sizePolicy();
  accountButtonPolicy.setRetainSizeWhenHidden(true);
  accountButton->setSizePolicy(accountButtonPolicy);

  if (forceOpen) {
    for (AbstractControl *control : std::initializer_list<AbstractControl*>{viewStatsButton, accountButton}) {
      control->showDescription();
    }
  }

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [dataLayout, dataMainPanel] {
    dataLayout->setCurrentWidget(dataMainPanel);
  });
}

void FrogPilotDataPanel::updateStatsLabels(FrogPilotListWidget *labelsList) {
  labelsList->clear();

  bool isMetric = params.getBool("IsMetric");
  QJsonObject stats = QJsonDocument::fromJson(QByteArray::fromStdString(params.get("FrogPilotStats"))).object();

  static QMap<QString, QPair<QString, QString>> keyMap = {
    {"AEBEvents", {tr("Total Collision Alerts"), "count"}},
    {"AOLTime", {tr("Time Using \"Always On Lateral\""), "timePercent"}},
    {"CruiseSpeedTimes", {tr("Favorite Set Speed"), "speed"}},
    {"CurrentMonthsMeters", {tr("Distance Driven This Month"), "distance"}},
    {"DayTime", {tr("Time Driving (Daytime)"), "timePercent"}},
    {"Disengages", {tr("Total Disengagements"), "count"}},
    {"Engages", {tr("Total Engagements"), "count"}},
    {"ExperimentalModeTime", {tr("Time Using \"Experimental Mode\""), "timePercent"}},
    {"FrogChirps", {tr("Total Frog Chirps"), "count"}},
    {"FrogHops", {tr("Total Frog Hops"), "count"}},
    {"FrogPilotDrives", {tr("Total Drives"), "count"}},
    {"FrogPilotMeters", {tr("Total Distance Driven"), "distance"}},
    {"FrogPilotSeconds", {tr("Total Driving Time"), "time"}},
    {"FrogSqueaks", {tr("Total Frog Squeaks"), "count"}},
    {"GoatScreams", {tr("Total Goat Screams"), "count"}},
    {"LateralTime", {tr("Time openpilot Was Steering"), "timePercent"}},
    {"LongestDistanceWithoutOverride", {tr("Longest Distance Without an Override"), "distance"}},
    {"LongitudinalTime", {tr("Time openpilot Controlled the Speed"), "timePercent"}},
    {"MaxAcceleration", {tr("Highest openpilot Acceleration"), "accel"}},
    {"ModelTimes", {tr("Driving Models:"), "parent"}},
    {"NightTime", {tr("Time Driving (Nighttime)"), "timePercent"}},
    {"Overrides", {tr("Total Overrides"), "count"}},
    {"OverrideTime", {tr("Time Driving Manually"), "timePercent"}},
    {"PersonalityTimes", {tr("Driving Personalities:"), "parent"}},
    {"RandomEvents", {tr("Random Events:"), "parent"}},
    {"StandstillTime", {tr("Time Stopped"), "timePercent"}},
    {"StopLightTime", {tr("Time Spent at Stoplights"), "timePercent"}},
    {"TrackedTime", {tr("Total Time Tracked"), "time"}},
    {"WeatherTimes", {tr("Time Driven (Weather):"), "parent"}}
  };

  static QMap<QString, QString> countNames = {
    {"AEBEvents", tr("Collision Alerts")},
    {"Disengages", tr("Disengagements")},
    {"Engages", tr("Engagements")},
    {"FrogChirps", tr("Frog Chirps")},
    {"FrogHops", tr("Frog Hops")},
    {"FrogPilotDrives", tr("Drives")},
    {"FrogSqueaks", tr("Frog Squeaks")},
    {"GoatScreams", tr("Goat Screams")},
    {"Overrides", tr("Overrides")}
  };

  static QMap<QString, QString> personalityNames = {
    {"Aggressive", tr("Aggressive")},
    {"Relaxed", tr("Relaxed")},
    {"Standard", tr("Standard")}
  };

  static QMap<QString, QString> weatherNames = {
    {"clear", tr("Clear")},
    {"low_visibility", tr("Low Visibility")},
    {"rain", tr("Rain")},
    {"rain_storm", tr("Rain Storm")},
    {"snow", tr("Snow")}
  };

  QStringList keys = keyMap.keys();
  std::sort(keys.begin(), keys.end(), [&](const QString &a, const QString &b) {
    return keyMap.value(a).first.toLower() < keyMap.value(b).first.toLower();
  });

  std::function<QString(double)> formatNumber = [&](double number) {
    return QLocale().toString(number);
  };

  double trackedTime = stats.value("TrackedTime").toDouble();

  for (const QString &key : keys) {
    QJsonValue value = stats.value(key);
    QString labelText = keyMap.value(key).first;
    QString type = keyMap.value(key).second;

    if ((key == "DayTime" || key == "NightTime") && value.toDouble() <= 0.0) {
      continue;
    }

    if (key == "CurrentMonthsMeters" && stats.value("Month").toInt() != QDateTime::currentDateTimeUtc().date().month()) {
      value = QJsonValue(0);
    }

    if (key == "AEBEvents") {
      QJsonObject totalEvents = stats.value("TotalEvents").toObject();

      QString displayValue = formatNumber(totalEvents.value("stockAeb").toInt(0) + totalEvents.value("fcw").toInt(0)) + " " + countNames.value(key);

      labelsList->addItem(new LabelControl(labelText, displayValue, "", this));
    } else if (key == "CruiseSpeedTimes" && value.isObject() && !value.toObject().isEmpty()) {
      QJsonObject speeds = value.toObject();

      double maxTime = -1.0;
      QString bestSpeed;
      for (const QString &speedKey : speeds.keys()) {
        double time = speeds.value(speedKey).toDouble();
        if (time > maxTime) {
          bestSpeed = speedKey;
          maxTime = time;
        }
      }

      QString displaySpeed;
      if (isMetric) {
        displaySpeed = QString::number(qRound(bestSpeed.toDouble() * MS_TO_KPH)) + " " + tr("km/h");
      } else {
        displaySpeed = QString::number(qRound(bestSpeed.toDouble() * MS_TO_MPH)) + " " + tr("mph");
      }

      labelsList->addItem(new LabelControl(labelText, displaySpeed + " (" + formatDuration(maxTime) + ")", "", this));
    } else if (type == "parent") {
      labelsList->addItem(new LabelControl(labelText, "", "", this));

      QJsonObject subObject = value.toObject();
      QStringList subKeys;

      if (key == "RandomEvents") {
        subKeys = randomEventNames().keys();
      } else {
        subKeys = subObject.keys();
      }

      std::sort(subKeys.begin(), subKeys.end(), [&](const QString &a, const QString &b) {
        return randomEventNames().value(a, a).toLower() < randomEventNames().value(b, b).toLower();
      });

      for (const QString &subkey : subKeys) {
        if (subkey.compare("unknown", Qt::CaseInsensitive) == 0) {
          continue;
        }

        QString displaySubKey = subkey;
        if (key == "ModelTimes") {
          displaySubKey = cleanModelName(subkey);
        } else if (key == "RandomEvents") {
          displaySubKey = randomEventNames().value(subkey, subkey);
        } else if (key == "PersonalityTimes") {
          displaySubKey = personalityNames.value(subkey, subkey);
        } else if (key == "WeatherTimes") {
          displaySubKey = weatherNames.value(subkey, subkey);
        }

        QString subvalue;
        if (key.endsWith("Times")) {
          subvalue = formatDuration(subObject.value(subkey).toDouble());
        } else {
          subvalue = formatNumber(subObject.value(subkey).toInt(0));
        }

        labelsList->addItem(new LabelControl("     " + displaySubKey, subvalue, "", this));
      }
    } else {
      QString displayValue;
      if (type == "accel") {
        displayValue = QString::number(value.toDouble(), 'f', 2) + " " + tr("m/s²");
      } else if (type == "count") {
        displayValue = formatNumber(value.toInt()) + " " + countNames.value(key);
      } else if (type == "distance") {
        displayValue = formatDistance(value.toDouble(), isMetric);
      } else if (type == "speed") {
        displayValue = "--";
      } else if (type == "time" || type == "timePercent") {
        displayValue = formatDuration(value.toDouble());
      }

      labelsList->addItem(new LabelControl(labelText, displayValue, "", this));

      if (type == "timePercent") {
        int percent = 0;
        if (trackedTime > 0.0) {
          percent = (value.toDouble() * 100.0) / trackedTime;
        }

        labelsList->addItem(new LabelControl(tr("% of %1").arg(labelText), formatNumber(percent) + "%", "", this));
      }
    }
  }
}
