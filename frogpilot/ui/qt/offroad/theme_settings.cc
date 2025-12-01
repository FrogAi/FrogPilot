#include "frogpilot/ui/qt/offroad/theme_settings.h"

FrogPilotThemesPanel::FrogPilotThemesPanel(FrogPilotSettingsWindow *parent, bool forceOpen) : FrogPilotListWidget(parent), parent(parent) {
  forceOpenDescriptions = forceOpen;

  QStackedLayout *themesLayout = new QStackedLayout();
  addItem(themesLayout);

  FrogPilotListWidget *themesList = new FrogPilotListWidget(this);

  ScrollView *themesPanel = new ScrollView(themesList, this);

  themesLayout->addWidget(themesPanel);

  FrogPilotListWidget *customThemesList = new FrogPilotListWidget(this);

  ScrollView *customThemesPanel = new ScrollView(customThemesList, this);

  themesLayout->addWidget(customThemesPanel);

  const std::vector<std::tuple<QString, QString, QString, QString>> themeToggles {
    {"CustomThemes", tr("Custom Themes"), tr("<b>Swap openpilot's colors, icons, sounds, turn signal animations, steering wheel picture, and personality button for a theme pack you download.</b><br><br>You mix and match freely, so one theme's colors can run alongside another's sounds. Packs are made by other drivers, and you can build your own with the \"Theme Maker\" in \"The Pond\"."), "../../frogpilot/assets/toggle_icons/icon_frog.svg"},
    {"ColorScheme", tr("Color Scheme"), tr("<b>Change the colors openpilot draws on the driving screen, mainly the path ahead of you and the lane lines.</b><br><br>\"Stock\" is openpilot's normal green path with white lane lines. A scheme also recolors the marker on the car ahead and the sidebar boxes, but the road edges are always red and never change. Holiday options match the holiday they are named after, and a downloaded pack brings its own set of colors."), ""},
    {"IconPack", tr("Icon Pack"), tr("<b>Change the settings, home, and flag buttons on openpilot's sidebar.</b><br><br>\"Stock\" puts the normal three back. A pack replaces all three at once and nothing else, so every other icon openpilot draws stays stock."), ""},
    {"DistanceIconPack", tr("Personality Button"), tr("<b>Change the icons on the driving personality button, the one you tap on the driving screen to switch between Aggressive, Standard, and Relaxed.</b><br><br>Each pack draws four icons, one each for Aggressive, Standard, and Relaxed, plus one that takes over while Traffic Mode is on. This row only appears while that button is switched on under \"Driving Personality Button\"."), ""},
    {"SoundPack", tr("Sound Pack"), tr("<b>Change the chimes openpilot plays for its alerts, like the sound when it starts driving or warns you about something.</b><br><br>\"Stock\" uses openpilot's normal chimes. A pack only replaces the sound files it actually ships and anything it leaves out stays stock, so the holiday packs mostly bring just their own engage and disengage chimes. How loud each one plays is set separately under \"Alert Volumes\" in \"Alerts and Sounds\"."), ""},
    {"WheelIcon", tr("Steering Wheel"), tr("<b>Change the steering wheel picture in the top right corner of the driving screen, which spins as openpilot steers.</b><br><br>\"Stock\" uses openpilot's normal wheel and \"None\" hides it completely. Some downloaded wheels are animated."), ""},
    {"SignalAnimation", tr("Turn Signal"), tr("<b>Play an animation across the driving screen for as long as your turn signal is on.</b><br><br>The animation runs toward whichever side you signaled. \"None\" turns it off, and each downloaded pack brings its own animation."), ""},
    {"DownloadStatusLabel", tr("Download Status"), "", ""},

    {"HolidayThemes", tr("Holiday Themes"), tr("<b>Dress openpilot up for thirteen holidays through the year, swapping the colors, icons, sounds, turn signals, steering wheel, and personality button all at once.</b><br><br>Smaller ones like April Fools or Cinco de Mayo run on the day itself. Easter, Halloween, Thanksgiving, and Christmas start on the Monday of that week and finish on the day, so they last anywhere from one day to a full week depending on where the date falls.<br><br>While a holiday is running it replaces the themes you picked, and your own choices come back the next day."), "../../frogpilot/assets/toggle_icons/icon_calendar.svg"},
    {"RainbowPath", tr("Rainbow Path"), tr("<b>Paint the driving path in shifting rainbow colors that scroll faster the quicker you go, like the Rainbow Road track from Mario Kart.</b><br><br>The rainbow replaces whatever color the path normally uses, including one that came with a theme you downloaded. With \"Acceleration Path\" also on, the green and red speed colors take over whenever openpilot speeds up or slows down, so the rainbow only shows while you hold a steady speed."), "../../frogpilot/assets/toggle_icons/icon_rainbow.svg"},
    {"RandomEvents", tr("Random Events"), tr("<b>Play a rare joke alert, with its own sound and sometimes its own steering wheel picture, when something unusual happens on a drive.</b><br><br>Taking off hard, a corner sharper than openpilot can steer through, or a collision warning can each set one off. Every alert can only happen once per drive, a swapped steering wheel goes back to normal after about five seconds, and none of them change how openpilot drives."), "../../frogpilot/assets/toggle_icons/icon_random.svg"},
    {"RandomThemes", tr("Random Themes"), tr("<b>Start every drive with a theme picked at random from the built-in Frog theme and the packs you have already downloaded.</b><br><br>A downloaded pack only joins the mix once you have its colors, icons, and sounds, and the same theme can come up twice in a row. With \"Include Holiday Themes\" on, the holiday themes are in the mix too. While this is on, the rows inside \"Custom Themes\" stop offering \"SELECT\", and turning it back off gives you your own picks again."), "../../frogpilot/assets/toggle_icons/icon_random_themes.svg"},
    {"StartupAlert", tr("Startup Alert"), tr("<b>Change the two lines of text openpilot shows on screen at the start of every drive.</b><br><br>\"STOCK\" is openpilot's usual safety reminder and \"FROGPILOT\" is the frog version. \"CUSTOM\" lets you write your own, up to 35 characters on the top line and 45 on the bottom."), "../../frogpilot/assets/toggle_icons/icon_message.svg"}
  };

  for (const auto &[param, title, desc, icon] : themeToggles) {
    AbstractControl *themeToggle;

    if (param == "CustomThemes") {
      FrogPilotManageControl *customThemesToggle = new FrogPilotManageControl(param, title, desc, icon);
      QObject::connect(customThemesToggle, &FrogPilotManageControl::manageButtonClicked, [customThemesPanel, themesLayout]() {
        themesLayout->setCurrentWidget(customThemesPanel);
      });
      themeToggle = customThemesToggle;
    } else if (param == "ColorScheme" || param == "DistanceIconPack" || param == "IconPack" || param == "SignalAnimation" || param == "SoundPack" || param == "WheelIcon") {
      themeToggle = new FrogPilotButtonsControl(title, desc, icon, {tr("DELETE"), tr("DOWNLOAD"), tr("SELECT")});
    } else if (param == "DownloadStatusLabel") {
      themeToggle = new LabelControl(title, tr("Idle"));

    } else if (param == "RandomThemes") {
      std::vector<QString> randomThemesToggles{"RandomThemesHolidays"};
      std::vector<QString> randomThemesToggleNames{tr("Include Holiday Themes")};
      themeToggle = new FrogPilotButtonToggleControl(param, title, desc, icon, randomThemesToggles, randomThemesToggleNames);

    } else {
      themeToggle = new ParamControl(param, title, desc, icon);
    }

    toggles[param] = themeToggle;

    if (customThemeKeys.contains(param)) {
      customThemesList->addItem(themeToggle);
    } else {
      themesList->addItem(themeToggle);

      if (param == "CustomThemes") {
        parentKeys.insert(param);
      }
    }

    if (FrogPilotManageControl *frogPilotManageToggle = qobject_cast<FrogPilotManageControl*>(themeToggle)) {
      QObject::connect(frogPilotManageToggle, &FrogPilotManageControl::manageButtonClicked, [this]() {
        emit openSubPanel();
        openDescriptions(forceOpenDescriptions, toggles);
      });
    }
  }

  QObject::connect(static_cast<ToggleControl*>(toggles["CustomThemes"]), &ToggleControl::toggleFlipped, this, &FrogPilotThemesPanel::updateToggles);

  QObject::connect(parent, &FrogPilotSettingsWindow::closeSubPanel, [themesLayout, themesPanel, this] {
    openDescriptions(forceOpenDescriptions, toggles);
    themesLayout->setCurrentWidget(themesPanel);
  });
}

void FrogPilotThemesPanel::showEvent(QShowEvent *event) {
  updateToggles();
}

void FrogPilotThemesPanel::updateToggles() {
  QSet<QString> visibleParents;

  for (auto &[key, toggle] : toggles) {
    if (parentKeys.contains(key)) {
      continue;
    }

    bool setVisible = parent->tuningLevel >= parent->frogpilotToggleLevels.value(key).toDouble();

    if (key == "DistanceIconPack") {
      setVisible &= parent->hasOpenpilotLongitudinal && params.getBool("CustomUI") && params.getBool("OnroadDistanceButton");
    }

    else if (key == "RandomThemes") {
      setVisible &= params.getBool("CustomThemes");
    }

    toggle->setVisible(setVisible);

    if (setVisible) {
      if (customThemeKeys.contains(key)) {
        visibleParents.insert("CustomThemes");
      }
    }
  }

  for (const QString &key : parentKeys) {
    toggles[key]->setVisible(visibleParents.contains(key));
  }

  openDescriptions(forceOpenDescriptions, toggles);

  update();
}
