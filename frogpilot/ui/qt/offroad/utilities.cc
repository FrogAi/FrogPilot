#include "frogpilot/ui/qt/offroad/utilities.h"

FrogPilotUtilitiesPanel::FrogPilotUtilitiesPanel(FrogPilotSettingsWindow *parent) : FrogPilotListWidget(parent), parent(parent) {
  networkManager = new QNetworkAccessManager(this);
  pairingPollTimer = new QTimer(this);

  QJsonObject shownDescriptions = QJsonDocument::fromJson(QString::fromStdString(params.get("ShownToggleDescriptions")).toUtf8()).object();
  QString className = this->metaObject()->className();

  bool forceOpenDescriptions = false;
  if (!shownDescriptions.value(className).toBool(false)) {
    forceOpenDescriptions = true;
    shownDescriptions.insert(className, true);
    params.put("ShownToggleDescriptions", QJsonDocument(shownDescriptions).toJson(QJsonDocument::Compact).toStdString());
  }

  ParamControl *debugModeToggle = new ParamControl("DebugMode", tr("Debug Mode"), tr("<b>Use FrogPilot's developer metrics on your next drive</b> to diagnose issues and improve bug reports."), "");
  if (forceOpenDescriptions) {
    debugModeToggle->showDescription();
  }
  addItem(debugModeToggle);

  ButtonControl *flashPandaButton = new ButtonControl(tr("Flash Panda"), tr("FLASH"), tr("<b>Reinstall the Panda firmware</b> to fix connection or reliability issues."));
  QObject::connect(flashPandaButton, &ButtonControl::clicked, [parent, flashPandaButton, this]() {
    if (ConfirmationDialog::confirm(tr("Are you sure you want to flash the Panda firmware?"), tr("Flash"), this)) {
      std::thread([parent, flashPandaButton, this]() {
        parent->keepScreenOn = true;

        flashPandaButton->setEnabled(false);
        flashPandaButton->setValue(tr("Flashing..."));

        params_memory.putBool("FlashPanda", true);
        while (params_memory.getBool("FlashPanda")) {
          util::sleep_for(UI_FREQ);
        }

        flashPandaButton->setValue(tr("Flashed!"));

        util::sleep_for(2500);

        flashPandaButton->setValue(tr("Rebooting..."));

        util::sleep_for(2500);

        Hardware::reboot();
      }).detach();
    }
  });
  if (forceOpenDescriptions) {
    flashPandaButton->showDescription();
  }
  addItem(flashPandaButton);

  FrogPilotButtonsControl *forceStartedButton = new FrogPilotButtonsControl(tr("Force Drive State"), tr("<b>Manually set openpilot to be offroad or onroad.</b>"), "", {tr("OFFROAD"), tr("ONROAD"), tr("OFF")}, true);
  QObject::connect(forceStartedButton, &FrogPilotButtonsControl::buttonClicked, [this](int id) {
    if (id == 0) {
      params_memory.putBool("ForceOffroad", true);
      params_memory.putBool("ForceOnroad", false);

      updateFrogPilotToggles();
    } else if (id == 1) {
      params.put("CarParams", params.get("CarParamsPersistent"));
      params.put("FrogPilotCarParams", params.get("FrogPilotCarParamsPersistent"));

      params_memory.putBool("ForceOffroad", false);
      params_memory.putBool("ForceOnroad", true);

      updateFrogPilotToggles();
    } else if (id == 2) {
      params_memory.putBool("ForceOffroad", false);
      params_memory.putBool("ForceOnroad", false);

      updateFrogPilotToggles();
    }
  });
  forceStartedButton->setCheckedButton(2);
  if (forceOpenDescriptions) {
    forceStartedButton->showDescription();
  }
  addItem(forceStartedButton);

  bool paired = params.getBool("PondPaired");
  pondButton = new ButtonControl(
    tr("Pair to \"The Pond\""),
    paired ? tr("UNPAIR") : tr("PAIR"),
    tr("<b>Pair this device with your frogpilot.com account</b> to remotely manage settings from anywhere.")
  );
  QObject::connect(pondButton, &ButtonControl::clicked, [this]() {
    if (!frogpilotUIState()->frogpilot_scene.online) {
      ConfirmationDialog::alert(tr("Please connect to the internet first!"), this);
      return;
    }

    bool isPaired = params.getBool("PondPaired");

    if (isPaired && !ConfirmationDialog::confirm(tr("Are you sure you want to unpair from \"The Pond\"?"), tr("Unpair"), this)) {
      return;
    }

    pondButton->setEnabled(false);
    pondButton->setValue(isPaired ? tr("Unpairing...") : tr("Requesting code..."));

    QJsonObject payload;
    payload["api_token"] = QString::fromStdString(params.get("FrogPilotApiToken"));
    payload["device"] = QString::fromStdString(Hardware::get_name()).trimmed().remove(QChar('\0'));
    payload["frogpilot_dongle_id"] = QString::fromStdString(params.get("FrogPilotDongleId"));

    QString buildMetadataStr = QString::fromStdString(params.get("BuildMetadata"));
    if (!buildMetadataStr.isEmpty()) {
      QJsonDocument bmDoc = QJsonDocument::fromJson(buildMetadataStr.toUtf8());
      if (!bmDoc.isNull()) {
        payload["build_metadata"] = bmDoc.object();
      }
    }

    QNetworkRequest request(QUrl(QString("https://www.frogpilot.com/api/pond/pair/") + (isPaired ? "unpair" : "request")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "frogpilot-api/1.0");

    QByteArray postData = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QNetworkReply *reply = networkManager->post(request, postData);
    QObject::connect(reply, &QNetworkReply::finished, [this, reply, isPaired, payload]() {
      QByteArray responseBody = reply->readAll();

      reply->deleteLater();

      pondButton->setEnabled(true);
      pondButton->setValue("");

      if (reply->error() != QNetworkReply::NoError) {
        QString errorDetail;
        QString serverError = QJsonDocument::fromJson(responseBody).object().value("error").toString();

        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode == 401) {
          errorDetail = tr("Authentication failed. Please restart your device.");
        } else if (statusCode == 403) {
          errorDetail = serverError.contains("build", Qt::CaseInsensitive)
            ? tr("Unofficial or modified build detected.")
            : tr("Access denied: %1").arg(serverError);
        } else if (statusCode == 429) {
          errorDetail = tr("Too many attempts. Please wait and try again.");
        } else if (statusCode == 503) {
          errorDetail = tr("Server is temporarily unavailable. Please try again later.");
        } else if (statusCode > 0) {
          errorDetail = tr("Server error (%1): %2").arg(statusCode).arg(serverError.isEmpty() ? tr("Unknown error") : serverError);
        } else {
          errorDetail = tr("Network error: %1").arg(reply->errorString());
        }

        if (isPaired) {
          pondButton->setValue(tr("Failed to unpair"));

          QTimer::singleShot(2500, [this]() {
            pondButton->setValue("");
          });
        } else {
          ConfirmationDialog::alert(tr("Failed to get pairing code.\n\n%1").arg(errorDetail), this);
        }
        return;
      }

      if (isPaired) {
        params.putBool("PondPaired", false);

        pondButton->setText(tr("PAIR"));
        pondButton->setValue(tr("Unpaired!"));

        QTimer::singleShot(2500, [this]() {
          pondButton->setValue("");
        });
        return;
      }

      QString code = QJsonDocument::fromJson(responseBody).object().value("code").toString();
      if (code.isEmpty()) {
        ConfirmationDialog::alert(tr("Failed to get pairing code. Please try again."), this);
        return;
      }

      pondButton->setValue(tr("Code: %1").arg(code));

      ConfirmationDialog::alert(tr("Go to \"frogpilot.com/the_pond\" and enter this code: %1").arg(code), this);

      QString dongleId = payload["frogpilot_dongle_id"].toString();
      QString apiToken = payload["api_token"].toString();

      pairingPollTimer->disconnect();

      int pollCount = 0;
      QObject::connect(pairingPollTimer, &QTimer::timeout, [this, code, dongleId, apiToken, pollCount]() mutable {
        if (++pollCount > 200) {
          pairingPollTimer->stop();

          pondButton->setValue(tr("Code expired"));

          QTimer::singleShot(2500, [this]() {
            pondButton->setValue("");
          });
          return;
        }

        QNetworkRequest statusRequest{QUrl(QString("https://www.frogpilot.com/api/pond/pair/status?code=%1&dongle_id=%2&api_token=%3").arg(code, dongleId, apiToken))};
        statusRequest.setRawHeader("User-Agent", "frogpilot-api/1.0");

        QNetworkReply *statusReply = networkManager->get(statusRequest);
        QObject::connect(statusReply, &QNetworkReply::finished, [this, statusReply]() {
          statusReply->deleteLater();
          if (statusReply->error() != QNetworkReply::NoError) {
            return;
          }

          QString status = QJsonDocument::fromJson(statusReply->readAll()).object().value("status").toString();
          if (status == "paired") {
            pairingPollTimer->stop();

            params.putBool("PondPaired", true);

            pondButton->setText(tr("UNPAIR"));
            pondButton->setValue(tr("Paired!"));

            ConfirmationDialog::alert(tr("Device successfully paired to \"The Pond\"!"), this);

            QTimer::singleShot(2500, [this]() {
              pondButton->setValue("");
            });
          } else if (status == "expired") {
            pairingPollTimer->stop();

            pondButton->setValue(tr("Code expired"));

            QTimer::singleShot(2500, [this]() {
              pondButton->setValue("");
            });
          }
        });
      });

      pairingPollTimer->start(3000);
    });
  });
  if (forceOpenDescriptions) {
    pondButton->showDescription();
  }
  addItem(pondButton);
  pondButton->setVisible(QString::fromStdString(params.get("GitRemote")).toLower() == "https://github.com/frogai/openpilot.git");

  ButtonControl *reportIssueButton = new ButtonControl(tr("Report a Bug or an Issue"), tr("REPORT"), tr("<b>Send a bug report</b> so we can help fix the problem!"));
  QObject::connect(reportIssueButton, &ButtonControl::clicked, [this]() {
    if (!frogpilotUIState()->frogpilot_scene.online) {
      ConfirmationDialog::alert(tr("Please connect to the internet before sending a report!"), this);
      return;
    }

    QStringList report_messages;
    QString crash_report = tr("I saw an alert that said \"openpilot crashed\"");
    if (QFile::exists("/data/error_logs/error.txt")) {
      report_messages << crash_report;
    }
    QStringList additional_issues = {
      tr("Acceleration feels harsh or jerky"),
      tr("An alert was unclear and I didn't know what it meant"),
      tr("Braking is too sudden or uncomfortable"),
      tr("I'm not sure if this is normal or a bug:"),
      tr("My screen froze or is stuck loading something"),
      tr("My steering wheel buttons aren't working"),
      tr("openpilot disengages when I don't expect it"),
      tr("openpilot doesn't react to stopped vehicles ahead"),
      tr("openpilot doesn't resume from a stop"),
      tr("openpilot feels sluggish or slow to respond"),
      tr("Steering feels twitchy or unnatural"),
      tr("The car doesn't follow curves well"),
      tr("The car isn't staying centered in its lane"),
      tr("Something else (please describe)")
    };
    report_messages.append(additional_issues);

    QMap<QString, bool> needs_extra_input;
    for (const QString &issue : report_messages) {
      if (issue.contains("confused") ||
          issue.contains("crashed") ||
          issue.contains("not sure") ||
          issue.contains("Something else")) {
        needs_extra_input[issue] = true;
      }
    }

    QString selected_issue = MultiOptionDialog::getSelection(tr("What's going on?"), report_messages, "", this);
    if (selected_issue.isEmpty()) {
      return;
    }

    if (needs_extra_input.value(selected_issue, false)) {
      QString extra_input = InputDialog::getText(tr("Please describe what's happening"), this, tr("Send Report"), false, 10, "", 300).trimmed();
      if (extra_input.isEmpty()) {
        return;
      }
      selected_issue += " — " + extra_input;
    }

    QJsonObject reportData;
    reportData["Issue"] = selected_issue;
    reportData["DiscordUser"] = InputDialog::getText(tr("What's your Discord username?"), this, tr("Send Report"), false, -1, QString::fromStdString(params.get("DiscordUsername"))).trimmed();

    params.putNonBlocking("DiscordUsername", reportData["DiscordUser"].toString().toStdString());
    params_memory.put("IssueReported", QJsonDocument(reportData).toJson(QJsonDocument::Compact).toStdString());

    ConfirmationDialog::alert(tr("Report Sent! Thanks for letting us know!"), this);
  });
  if (forceOpenDescriptions) {
    reportIssueButton->showDescription();
  }
  addItem(reportIssueButton);
  reportIssueButton->setVisible(QString::fromStdString(params.get("GitRemote")).toLower() == "https://github.com/frogai/openpilot.git");

  ButtonControl *resetTogglesButton = new ButtonControl(tr("Reset Toggles to Default"), tr("RESET"), tr("<b>Reset all toggles to their default values.</b>"));
  QObject::connect(resetTogglesButton, &ButtonControl::clicked, [parent, resetTogglesButton, this]() {
    if (ConfirmationDialog::confirm(tr("Are you sure you want to reset all toggles to their default values?"), tr("Reset"), this)) {
      std::thread([parent, resetTogglesButton, this]() mutable {
        parent->keepScreenOn = true;

        resetTogglesButton->setEnabled(false);
        resetTogglesButton->setValue(tr("Resetting..."));

        params.putBool("DoToggleReset", true);

        resetTogglesButton->setValue(tr("Reset!"));

        util::sleep_for(2500);

        resetTogglesButton->setValue(tr("Rebooting..."));

        util::sleep_for(2500);

        Hardware::reboot();
      }).detach();
    }
  });
  if (forceOpenDescriptions) {
    resetTogglesButton->showDescription();
  }
  addItem(resetTogglesButton);

  ButtonControl *resetTogglesButtonStock = new ButtonControl(tr("Reset Toggles to Stock openpilot"), tr("RESET"), tr("<b>Reset all toggles to match stock openpilot.</b>"));
  QObject::connect(resetTogglesButtonStock, &ButtonControl::clicked, [parent, resetTogglesButtonStock, this]() {
    if (ConfirmationDialog::confirm(tr("Are you sure you want to reset all toggles to match stock openpilot?"), tr("Reset"), this)) {
      std::thread([parent, resetTogglesButtonStock, this]() mutable {
        parent->keepScreenOn = true;

        resetTogglesButtonStock->setEnabled(false);
        resetTogglesButtonStock->setValue(tr("Resetting..."));

        params.putBool("DoToggleResetStock", true);

        resetTogglesButtonStock->setValue(tr("Reset!"));

        util::sleep_for(2500);

        resetTogglesButtonStock->setValue(tr("Rebooting..."));

        util::sleep_for(2500);

        Hardware::reboot();
      }).detach();
    }
  });
  if (forceOpenDescriptions) {
    resetTogglesButtonStock->showDescription();
  }
  addItem(resetTogglesButtonStock);
}

void FrogPilotUtilitiesPanel::showEvent(QShowEvent *event) {
  FrogPilotListWidget::showEvent(event);

  bool isPaired = params.getBool("PondPaired");
  pondButton->setText(isPaired ? tr("UNPAIR") : tr("PAIR"));
}
