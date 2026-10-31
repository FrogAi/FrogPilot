#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalBlocker>

#include "frogpilot/ui/qt/widgets/navigation_functions.h"

MapSelectionControl::MapSelectionControl(const QMap<QString, QString> &map, bool isCountry) : selectionType(isCountry ? "nations" : "states") {
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
    button->setStyleSheet(buttonStyle);

    mapButtons->addButton(button, i);

    mapLayout->addWidget(button, i / 3, i % 3);

    QObject::connect(button, &QPushButton::toggled, this, &MapSelectionControl::updateSelectedMaps);
  }

  reloadSelectedMaps();
}

void MapSelectionControl::reloadSelectedMaps() {
  QJsonObject existingMaps = QJsonDocument::fromJson(QByteArray::fromStdString(params.get("MapsSelected"))).object();

  QSet<QString> selectedMaps;
  for (const QJsonValue &value : existingMaps.value(selectionType).toArray()) {
    selectedMaps.insert(value.toString());
  }

  for (QAbstractButton *button : mapButtons->buttons()) {
    const QSignalBlocker blocker(button);
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
  params.put("MapsSelected", QJsonDocument(existingMaps).toJson(QJsonDocument::Compact).toStdString());
}
