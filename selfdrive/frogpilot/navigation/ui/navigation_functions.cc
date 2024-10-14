#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "selfdrive/frogpilot/navigation/ui/navigation_functions.h"

MapSelectionControl::MapSelectionControl(const QMap<QString, QString> &map, bool isCountry, QWidget *parent)
  : QWidget(parent), buttonGroup(new QButtonGroup(this)), gridLayout(new QGridLayout(this)), mapData(map), isCountry(isCountry) {

  buttonGroup->setExclusive(false);

  const QList<QString> keys = mapData.keys();
  for (int i = 0; i < keys.size(); ++i) {
    QPushButton *button = new QPushButton(mapData[keys[i]], this);
    button->setCheckable(true);
    button->setStyleSheet(buttonStyle);
    button->setMinimumWidth(225);
    gridLayout->addWidget(button, i / 3, i % 3);
    buttonGroup->addButton(button, i);
    connect(button, &QPushButton::toggled, this, &MapSelectionControl::updateSelectedMaps);
  }

  loadSelectedMaps();
}

void MapSelectionControl::loadSelectedMaps() {
  QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(params.get("MapsSelected")));
  if (!doc.isObject()) {
    return;
  }

  QJsonArray selectedArray = doc.object().value(isCountry ? "nations" : "states").toArray();
  QList<QAbstractButton *> buttons = buttonGroup->buttons();

  for (int i = 0; i < selectedArray.size(); ++i) {
    QString buttonLabel = mapData.value(selectedArray[i].toString());
    if (!buttonLabel.isEmpty()) {
      for (int j = 0; j < buttons.size(); ++j) {
        if (buttons[j]->text() == buttonLabel) {
          buttons[j]->setChecked(true);
          break;
        }
      }
    }
  }
}

void MapSelectionControl::updateSelectedMaps() {
  QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(params.get("MapsSelected")));
  QJsonObject selectionJson = doc.isObject() ? doc.object() : QJsonObject();

  QString selectionType = isCountry ? "nations" : "states";
  QJsonArray existingSelections = selectionJson.value(selectionType).toArray();

  QList<QAbstractButton *> buttons = buttonGroup->buttons();
  for (int i = 0; i < buttons.size(); ++i) {
    QAbstractButton *button = buttons[i];
    QString code = mapData.key(button->text());

    if (code.isEmpty()) {
      continue;
    }

    bool alreadySelected = false;
    for (int j = 0; j < existingSelections.size(); ++j) {
      if (existingSelections[j].toString() == code) {
        alreadySelected = true;
        break;
      }
    }

    if (button->isChecked() && !alreadySelected) {
      existingSelections.append(code);
    } else if (!button->isChecked() && alreadySelected) {
      for (int j = 0; j < existingSelections.size(); ++j) {
        if (existingSelections[j].toString() == code) {
          existingSelections.removeAt(j);
          break;
        }
      }
    }
  }

  selectionJson[selectionType] = existingSelections;
  params.put("MapsSelected", QString::fromUtf8(QJsonDocument(selectionJson).toJson(QJsonDocument::Compact)).toStdString());
}
