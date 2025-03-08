#include <QJsonDocument>
#include <QJsonObject>

#include "selfdrive/frogpilot/navigation/ui/navigation_functions.h"

MapSelectionControl::MapSelectionControl(const QMap<QString, QString> &map, bool isCountry) : mapData(map), isCountry(isCountry), selectionType(isCountry ? "nations" : "states") {
  mapButtons = new QButtonGroup(this);
  mapButtons->setExclusive(false);

  mapLayout = new QGridLayout(this);

  QList<QString> keys = mapData.keys();
  for (int i = 0; i < keys.size(); ++i) {
    QPushButton *button = new QPushButton(mapData[keys[i]], this);
    button->setCheckable(true);
    button->setStyleSheet(buttonStyle);

    mapButtons->addButton(button, i);

    mapLayout->addWidget(button, i / 3, i % 3);

    QObject::connect(button, &QPushButton::toggled, this, &MapSelectionControl::updateSelectedMaps);
  }

  maps = mapButtons->buttons();
  mapSelections = QJsonDocument::fromJson(QByteArray::fromStdString(params.get("MapsSelected"))).object().value(selectionType).toArray();

  loadSelectedMaps();
}

void MapSelectionControl::loadSelectedMaps() {
  for (int i = 0; i < mapSelections.size(); ++i) {
    QString selectedKey = mapSelections[i].toString();
    for (int j = 0; j < maps.size(); ++j) {
      QAbstractButton *button = maps[j];
      if (button->text() == mapData.value(selectedKey)) {
        button->setChecked(true);
        break;
      }
    }
  }
}

void MapSelectionControl::updateSelectedMaps() {
  for (QAbstractButton *button : maps) {
    QString key = mapData.key(button->text());
    if (button->isChecked() && !mapSelections.contains(key)) {
      mapSelections.append(key);
    } else if (!button->isChecked()) {
      for (int i = 0; i < mapSelections.size(); ++i) {
        if (mapSelections[i].toString() == key) {
          mapSelections.removeAt(i);
          --i;
        }
      }
    }
  }

  params.put("MapsSelected", QString::fromUtf8(QJsonDocument(QJsonObject{{selectionType, mapSelections}}).toJson(QJsonDocument::Compact)).toStdString());
}
