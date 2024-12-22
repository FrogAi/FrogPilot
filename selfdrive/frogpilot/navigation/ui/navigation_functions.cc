#include <QJsonDocument>
#include <QJsonObject>

#include "selfdrive/frogpilot/navigation/ui/navigation_functions.h"

MapSelectionControl::MapSelectionControl(const QMap<QString, QString> &map, bool isCountry)
  : buttonGroup(new QButtonGroup(this)), gridLayout(new QGridLayout(this)), mapData(map), isCountry(isCountry), selectionType(isCountry ? "nations" : "states") {
  buttonGroup->setExclusive(false);

  QList<QString> keys = mapData.keys();
  for (int i = 0; i < keys.size(); ++i) {
    QPushButton *button = new QPushButton(mapData[keys[i]], this);

    button->setCheckable(true);
    button->setStyleSheet(buttonStyle);
    button->setMinimumWidth(225);

    gridLayout->addWidget(button, i / 3, i % 3);
    buttonGroup->addButton(button, i);

    connect(button, &QPushButton::toggled, this, &MapSelectionControl::updateSelectedMaps);
  }

  buttons = buttonGroup->buttons();

  mapSelections = QJsonDocument::fromJson(QByteArray::fromStdString(params.get("MapsSelected"))).object()[selectionType].toArray();

  loadSelectedMaps();
}

void MapSelectionControl::loadSelectedMaps() {
  for (int i = 0; i < mapSelections.size(); ++i) {
    QString selectedKey = mapSelections[i].toString();
    for (int j = 0; j < buttons.size(); ++j) {
      QAbstractButton *button = buttons[j];
      if (button->text() == mapData.value(selectedKey)) {
        button->setChecked(true);
        break;
      }
    }
  }
}

void MapSelectionControl::updateSelectedMaps() {
  for (QAbstractButton *button : buttons) {
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
