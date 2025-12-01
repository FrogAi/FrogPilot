#include "frogpilot/ui/qt/widgets/navigation_functions.h"

MapSelectionControl::MapSelectionControl(const QMap<QString, QString> &map, bool isCountry) : prefix(isCountry ? "nation." : "us_state.") {
  setStyleSheet(buttonStyle);

  mapButtons = new QButtonGroup(this);
  mapButtons->setExclusive(false);

  QGridLayout *mapLayout = new QGridLayout(this);

  QList<QPair<QString, QString>> mapNames;
  for (QMap<QString, QString>::const_iterator it = map.constBegin(); it != map.constEnd(); ++it) {
    mapNames.append(qMakePair(QCoreApplication::translate(isCountry ? "MapSelectionControl" : "MapSelectionControlStates", it.value().toUtf8().constData()), it.key()));
  }
  std::sort(mapNames.begin(), mapNames.end(), [](const QPair<QString, QString> &a, const QPair<QString, QString> &b) {
    return a.first.localeAwareCompare(b.first) < 0;
  });

  for (int i = 0; i < mapNames.size(); ++i) {
    QPushButton *button = new QPushButton(mapNames[i].first, this);
    button->setCheckable(true);
    button->setProperty("mapKey", mapNames[i].second);

    mapButtons->addButton(button);

    mapLayout->addWidget(button, i / 3, i % 3);

    QObject::connect(button, &QPushButton::toggled, this, &MapSelectionControl::updateSelectedMaps);
  }
}

void MapSelectionControl::reloadSelectedMaps() {
  QString mapsSelected = QString::fromStdString(params.get("MapsSelected"));
  QStringList mapList = mapsSelected.split(",", QString::SkipEmptyParts);

  QSet<QString> selectedMaps;
  for (const QString &map : mapList) {
    if (map.startsWith(prefix)) {
      selectedMaps.insert(map.mid(prefix.length()));
    }
  }

  for (QAbstractButton *button : mapButtons->buttons()) {
    const QSignalBlocker blocker(button);
    button->setChecked(selectedMaps.contains(button->property("mapKey").toString()));
  }
}

void MapSelectionControl::updateSelectedMaps() {
  QString mapsSelected = QString::fromStdString(params.get("MapsSelected"));
  QStringList mapList = mapsSelected.split(",", QString::SkipEmptyParts);

  QSet<QString> controlMaps;
  for (QAbstractButton *button : mapButtons->buttons()) {
    controlMaps.insert(button->property("mapKey").toString());
  }

  QStringList newMapList;
  for (const QString &map : mapList) {
    if (!map.startsWith(prefix) || !controlMaps.contains(map.mid(prefix.length()))) {
      newMapList.append(map);
    }
  }

  for (QAbstractButton *button : mapButtons->buttons()) {
    if (button->isChecked()) {
      newMapList.append(prefix + button->property("mapKey").toString());
    }
  }

  newMapList.sort();
  if (newMapList.isEmpty()) {
    params.remove("MapsSelected");
  } else {
    params.put("MapsSelected", newMapList.join(",").toStdString());
  }
}
