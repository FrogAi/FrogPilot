#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "selfdrive/frogpilot/navigation/ui/navigation_functions.h"

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

  loadSelectedMaps();
}

void MapSelectionControl::loadSelectedMaps() {
  QJsonObject existingMaps = QJsonDocument::fromJson(QByteArray::fromStdString(params.get("MapsSelected"))).object();

  QSet<QString> selectedMaps;
  for (const QJsonValue &value : existingMaps.value(selectionType).toArray()) {
    selectedMaps.insert(value.toString());
  }

  for (QAbstractButton *button : mapButtons->buttons()) {
    button->setChecked(selectedMaps.contains(button->property("mapKey").toString()));
  }
}

void MapSelectionControl::updateSelectedMaps() {
  QJsonObject existingMaps = QJsonDocument::fromJson(QByteArray::fromStdString(params.get("MapsSelected"))).object();

  QSet<QString> selectedMaps;
  for (const QJsonValue &value : existingMaps.value(selectionType).toArray()) {
    selectedMaps.insert(value.toString());
  }

  for (QAbstractButton *button : mapButtons->buttons()) {
    QString mapKey = button->property("mapKey").toString();
    if (button->isChecked()) {
      selectedMaps.insert(mapKey);
    } else {
      selectedMaps.remove(mapKey);
    }
  }

  existingMaps[selectionType] = QJsonArray::fromStringList(selectedMaps.values());
  params.putNonBlocking("MapsSelected", QJsonDocument(existingMaps).toJson(QJsonDocument::Compact).toStdString());
}
