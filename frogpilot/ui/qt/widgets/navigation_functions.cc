#include "frogpilot/ui/qt/widgets/navigation_functions.h"

#include <QSignalBlocker>

MapSelectionControl::MapSelectionControl(const QMap<QString, QString> &map, bool isCountry) : selectionType(isCountry ? "nations" : "states") {
  mapButtons = new QButtonGroup(this);
  mapButtons->setExclusive(false);

  QGridLayout *mapLayout = new QGridLayout(this);

  QList<QString> keys = map.keys();
  for (int i = 0; i < keys.size(); ++i) {
    QPushButton *button = new QPushButton(map[keys[i]], this);
    button->setCheckable(true);
    button->setProperty("mapKey", keys[i]);
    button->setStyleSheet(buttonStyle);

    mapButtons->addButton(button, i);

    mapLayout->addWidget(button, i / 3, i % 3);

    QObject::connect(button, &QPushButton::toggled, this, &MapSelectionControl::updateSelectedMaps);
  }

  reloadSelectedMaps();
}

void MapSelectionControl::reloadSelectedMaps() {
  QString mapsSelected = QString::fromStdString(params.get("MapsSelected"));
  QStringList mapList = mapsSelected.split(",", QString::SkipEmptyParts);
  QString prefix = (selectionType == "nations") ? "nation." : "us_state.";

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
  QString prefix = (selectionType == "nations") ? "nation." : "us_state.";

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
  params.put("MapsSelected", newMapList.join(",").toStdString());
}
