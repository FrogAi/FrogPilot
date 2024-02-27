#pragma once

#include <deque>
#include <filesystem>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "selfdrive/ui/qt/offroad/settings.h"
#include "selfdrive/ui/qt/widgets/controls.h"
#include "selfdrive/ui/ui.h"

QMap<QString, QString> northeastMap = {
  {"CT", "Connecticut"}, {"DE", "Delaware"}, {"MA", "Massachusetts"},
  {"MD", "Maryland"}, {"ME", "Maine"}, {"NH", "New Hampshire"},
  {"NJ", "New Jersey"}, {"NY", "New York"}, {"PA", "Pennsylvania"},
  {"RI", "Rhode Island"}, {"VT", "Vermont"}
};

QMap<QString, QString> midwestMap = {
  {"IA", "Iowa"}, {"IL", "Illinois"}, {"IN", "Indiana"},
  {"KS", "Kansas"}, {"MI", "Michigan"}, {"MN", "Minnesota"},
  {"MO", "Missouri"}, {"ND", "North Dakota"}, {"NE", "Nebraska"},
  {"OH", "Ohio"}, {"SD", "South Dakota"}, {"WI", "Wisconsin"}
};

QMap<QString, QString> southMap = {
  {"AL", "Alabama"}, {"AR", "Arkansas"}, {"FL", "Florida"},
  {"GA", "Georgia"}, {"KY", "Kentucky"}, {"LA", "Louisiana"},
  {"MS", "Mississippi"}, {"NC", "North Carolina"}, {"OK", "Oklahoma"},
  {"SC", "South Carolina"}, {"TN", "Tennessee"}, {"TX", "Texas"},
  {"VA", "Virginia"}, {"WV", "West Virginia"}
};

QMap<QString, QString> westMap = {
  {"AK", "Alaska"}, {"AZ", "Arizona"}, {"CA", "California"},
  {"CO", "Colorado"}, {"HI", "Hawaii"}, {"ID", "Idaho"},
  {"MT", "Montana"}, {"NM", "New Mexico"}, {"NV", "Nevada"},
  {"OR", "Oregon"}, {"UT", "Utah"}, {"WA", "Washington"},
  {"WY", "Wyoming"}
};

QMap<QString, QString> territoriesMap = {
  {"AS", "American Samoa"}, {"DC", "District of Columbia"},
  {"GM", "Guam"}, {"MP", "North Mariana Islands"},
  {"PR", "Puerto Rico"}, {"VI", "Virgin Islands"}
};

QMap<QString, QString> africaMap = {
  {"DZ", "Algeria"}, {"AO", "Angola"}, {"BJ", "Benin"}, {"BW", "Botswana"},
  {"BF", "Burkina Faso"}, {"BI", "Burundi"}, {"CM", "Cameroon"}, {"CV", "Cape Verde"},
  {"CF", "Central African Republic"}, {"TD", "Chad"}, {"KM", "Comoros"}, {"CG", "Congo (Brazzaville)"},
  {"CD", "Congo (Kinshasa)"}, {"CI", "Ivory Coast"}, {"DJ", "Djibouti"}, {"EG", "Egypt"},
  {"GQ", "Equatorial Guinea"}, {"ER", "Eritrea"}, {"SZ", "Eswatini"}, {"ET", "Ethiopia"},
  {"GA", "Gabon"}, {"GM", "Gambia"}, {"GH", "Ghana"}, {"GN", "Guinea"},
  {"GW", "Guinea-Bissau"}, {"KE", "Kenya"}, {"LS", "Lesotho"}, {"LR", "Liberia"},
  {"LY", "Libya"}, {"MG", "Madagascar"}, {"MW", "Malawi"}, {"ML", "Mali"},
  {"MR", "Mauritania"}, {"MU", "Mauritius"}, {"MA", "Morocco"}, {"MZ", "Mozambique"},
  {"NA", "Namibia"}, {"NE", "Niger"}, {"NG", "Nigeria"}, {"RW", "Rwanda"},
  {"ST", "Sao Tome and Principe"}, {"SN", "Senegal"}, {"SC", "Seychelles"}, {"SL", "Sierra Leone"},
  {"SO", "Somalia"}, {"ZA", "South Africa"}, {"SS", "South Sudan"}, {"SD", "Sudan"},
  {"TZ", "Tanzania"}, {"TG", "Togo"}, {"TN", "Tunisia"}, {"UG", "Uganda"},
  {"EH", "Western Sahara"}, {"ZM", "Zambia"}, {"ZW", "Zimbabwe"}
};

QMap<QString, QString> antarcticaMap = {
  {"AQ", "Antarctica"}
};

QMap<QString, QString> asiaMap = {
  {"AF", "Afghanistan"}, {"AM", "Armenia"}, {"AZ", "Azerbaijan"}, {"BH", "Bahrain"},
  {"BD", "Bangladesh"}, {"BT", "Bhutan"}, {"BN", "Brunei"}, {"KH", "Cambodia"},
  {"CN", "China"}, {"CY", "Cyprus"}, {"GE", "Georgia"}, {"IN", "India"},
  {"ID", "Indonesia"}, {"IR", "Iran"}, {"IQ", "Iraq"}, {"IL", "Israel"},
  {"JP", "Japan"}, {"JO", "Jordan"}, {"KZ", "Kazakhstan"}, {"KP", "North Korea"},
  {"KR", "South Korea"}, {"KW", "Kuwait"}, {"KG", "Kyrgyzstan"}, {"LA", "Laos"},
  {"LB", "Lebanon"}, {"MY", "Malaysia"}, {"MV", "Maldives"}, {"MN", "Mongolia"},
  {"MM", "Myanmar"}, {"NP", "Nepal"}, {"OM", "Oman"}, {"PK", "Pakistan"},
  {"PS", "Palestine"}, {"PH", "Philippines"}, {"QA", "Qatar"}, {"SA", "Saudi Arabia"},
  {"SG", "Singapore"}, {"LK", "Sri Lanka"}, {"SY", "Syria"}, {"TW", "Taiwan"},
  {"TJ", "Tajikistan"}, {"TH", "Thailand"}, {"TL", "Timor-Leste"}, {"TR", "Turkey"},
  {"TM", "Turkmenistan"}, {"AE", "United Arab Emirates"}, {"UZ", "Uzbekistan"}, {"VN", "Vietnam"},
  {"YE", "Yemen"}
};

QMap<QString, QString> europeMap = {
  {"AL", "Albania"}, {"AD", "Andorra"}, {"AT", "Austria"}, {"BY", "Belarus"},
  {"BE", "Belgium"}, {"BA", "Bosnia and Herzegovina"}, {"BG", "Bulgaria"}, {"HR", "Croatia"},
  {"CY", "Cyprus"}, {"CZ", "Czech Republic"}, {"DK", "Denmark"}, {"EE", "Estonia"},
  {"FI", "Finland"}, {"FR", "France"}, {"DE", "Germany"}, {"GR", "Greece"},
  {"HU", "Hungary"}, {"IS", "Iceland"}, {"IE", "Ireland"}, {"IT", "Italy"},
  {"LV", "Latvia"}, {"LI", "Liechtenstein"}, {"LT", "Lithuania"}, {"LU", "Luxembourg"},
  {"MT", "Malta"}, {"MD", "Moldova"}, {"MC", "Monaco"}, {"ME", "Montenegro"},
  {"NL", "Netherlands"}, {"MK", "North Macedonia"}, {"NO", "Norway"}, {"PL", "Poland"},
  {"PT", "Portugal"}, {"RO", "Romania"}, {"RU", "Russia"}, {"SM", "San Marino"},
  {"RS", "Serbia"}, {"SK", "Slovakia"}, {"SI", "Slovenia"}, {"ES", "Spain"},
  {"SE", "Sweden"}, {"CH", "Switzerland"}, {"TR", "Turkey"}, {"UA", "Ukraine"},
  {"GB", "United Kingdom"}, {"VA", "Vatican City"}
};

QMap<QString, QString> northAmericaMap = {
  {"AG", "Antigua and Barbuda"}, {"BS", "Bahamas"}, {"BB", "Barbados"}, {"BZ", "Belize"},
  {"CA", "Canada"}, {"CR", "Costa Rica"}, {"CU", "Cuba"}, {"DM", "Dominica"},
  {"DO", "Dominican Republic"}, {"SV", "El Salvador"}, {"GD", "Grenada"}, {"GT", "Guatemala"},
  {"HT", "Haiti"}, {"HN", "Honduras"}, {"JM", "Jamaica"}, {"MX", "Mexico"},
  {"NI", "Nicaragua"}, {"PA", "Panama"}, {"KN", "Saint Kitts and Nevis"}, {"LC", "Saint Lucia"},
  {"VC", "Saint Vincent and the Grenadines"}, {"TT", "Trinidad and Tobago"}, {"US", "United States"}
};

QMap<QString, QString> oceaniaMap = {
  {"AU", "Australia"}, {"FJ", "Fiji"}, {"KI", "Kiribati"}, {"MH", "Marshall Islands"},
  {"FM", "Micronesia"}, {"NR", "Nauru"}, {"NZ", "New Zealand"}, {"PW", "Palau"},
  {"PG", "Papua New Guinea"}, {"WS", "Samoa"}, {"SB", "Solomon Islands"}, {"TO", "Tonga"},
  {"TV", "Tuvalu"}, {"VU", "Vanuatu"}
};

QMap<QString, QString> southAmericaMap = {
  {"AR", "Argentina"}, {"BO", "Bolivia"}, {"BR", "Brazil"}, {"CL", "Chile"},
  {"CO", "Colombia"}, {"EC", "Ecuador"}, {"GY", "Guyana"}, {"PY", "Paraguay"},
  {"PE", "Peru"}, {"SR", "Suriname"}, {"TT", "Trinidad and Tobago"}, {"UY", "Uruguay"},
  {"VE", "Venezuela"}
};

class ButtonSelectionControl : public QWidget {
public:
  static QString selectedStates;
  static QString selectedCountries;

  explicit ButtonSelectionControl(const QString &id, const QString &title, const QString &description,
                                  const QMap<QString, QString> &map, bool isCountry, QWidget *parent = nullptr)
      : QWidget(parent), country(isCountry) {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignTop);
    layout->setSpacing(10);

    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    buttonsLayout->setSpacing(10);
    layout->addLayout(buttonsLayout);

    int count = 0;
    int max = country ? 3 : 4;

    QJsonObject mapsSelected = QJsonDocument::fromJson(QString::fromStdString(Params().get("MapsSelected")).toUtf8()).object();

    for (const QString &stateCode : map.keys()) {
      if (count % max == 0 && count != 0) {
        buttonsLayout = new QHBoxLayout();
        buttonsLayout->setSpacing(10);
        layout->addLayout(buttonsLayout);
      }

      QPushButton *button = createButton(buttonsLayout, map[stateCode], stateCode);

      QString key = country ? "nations" : "states";
      if (mapsSelected.contains(key)) {
        QJsonArray selectedItems = mapsSelected.value(key).toArray();
        button->setChecked(selectedItems.contains(stateCode));
      }

      count++;
    }

    adjustButtonWidths(buttonsLayout);
  }

private:
  bool country;

  const QString buttonStyle = R"(
    QPushButton {
      border-radius: 50px; font-size: 40px; font-weight: 500;
      height: 100px; padding: 0 25 0 25; color: #E4E4E4;
      background-color: #393939;
    }
    QPushButton:pressed, QPushButton:checked {
      background-color: #4a4a4a;
    }
    QPushButton:checked:enabled {
      background-color: #33Ab4C;
    }
    QPushButton:disabled {
      color: #33E4E4E4;
    }
  )";

  QPushButton *createButton(QHBoxLayout *layout, const QString &label, const QString &stateCode) {
    QPushButton *button = new QPushButton(label, this);
    button->setCheckable(true);
    button->setStyleSheet(buttonStyle);
    connect(button, &QPushButton::clicked, this, [this, button, stateCode] { updateState(stateCode, button); });
    layout->addWidget(button);
    return button;
  }

  void adjustButtonWidths(QHBoxLayout *layout) {
    if (!layout || layout->count() == (country ? 3 : 4)) return;

    for (int i = 0; i < layout->count(); ++i) {
      QWidget *widget = layout->itemAt(i)->widget();
      QPushButton *button = qobject_cast<QPushButton *>(widget);
      if (button) {
        button->setMinimumWidth(button->sizeHint().width());
      }
    }
  }

  void updateState(const QString &newState, QPushButton *button) {
    QString &selectedList = country ? selectedCountries : selectedStates;
    QStringList tempList = selectedList.split(',');

    if (button->isChecked()) {
      if (!selectedList.isEmpty()) selectedList += ",";
      selectedList += newState;
    } else {
      tempList.removeAll(newState);
      selectedList = tempList.join(',');
    }

    Params("/dev/shm/params").remove("OSMDownloadLocations");
  }
};

QString ButtonSelectionControl::selectedStates = "";
QString ButtonSelectionControl::selectedCountries = "";

namespace {
  template <typename T>
  T extractFromJson(const std::string &jsonData, const std::string &key, T defaultValue = 0) {
    std::string::size_type pos = jsonData.find(key);
    return pos != std::string::npos ? std::stol(jsonData.substr(pos + key.length())) : defaultValue;
  }
}

QString formatTime(long timeInSeconds) {
  long minutes = timeInSeconds / 60;
  long seconds = timeInSeconds % 60;
  QString formattedTime = (minutes > 0) ? QString::number(minutes) + "m " : "";
  formattedTime += QString::number(seconds) + "s";
  return formattedTime;
}

QString formatDateTime(const std::chrono::time_point<std::chrono::system_clock> &timePoint) {
  return QDateTime::fromTime_t(std::chrono::system_clock::to_time_t(timePoint)).toString("h:mm ap");
}

QString calculateElapsedTime(int totalFiles, int downloadedFiles, const std::chrono::steady_clock::time_point &startTime) {
  using namespace std::chrono;
  if (totalFiles <= 0 || downloadedFiles >= totalFiles) return "Calculating...";

  long elapsed = duration_cast<seconds>(steady_clock::now() - startTime).count();
  return formatTime(elapsed);
}

QString calculateETA(int totalFiles, int downloadedFiles, const std::chrono::steady_clock::time_point &startTime) {
  using namespace std::chrono;
  if (totalFiles <= 0 || downloadedFiles >= totalFiles) return "Calculating...";

  long elapsed = duration_cast<seconds>(steady_clock::now() - startTime).count();

  if (downloadedFiles == 0 || elapsed <= 0) {
    return "Calculating...";
  }

  double averageTimePerFile = static_cast<double>(elapsed) / downloadedFiles;
  int remainingFiles = totalFiles - downloadedFiles;
  long estimatedTimeRemaining = static_cast<long>(averageTimePerFile * remainingFiles);

  std::chrono::time_point<std::chrono::system_clock> estimatedCompletionTime = system_clock::now() + seconds(estimatedTimeRemaining);
  QString estimatedTimeStr = formatDateTime(estimatedCompletionTime);

  return formatTime(estimatedTimeRemaining) + " (" + estimatedTimeStr + ")";

}

QString formatDownloadStatus(int totalFiles, int downloadedFiles) {
  if (totalFiles <= 0) return "Calculating...";
  if (downloadedFiles >= totalFiles) return "Downloaded";

  int percentage = static_cast<int>(100 * downloadedFiles / totalFiles);
  return QString::asprintf("Downloading: %d/%d (%d%%)", downloadedFiles, totalFiles, percentage);
}

quint64 calculateDirectorySize(const QString &path) {
  quint64 totalSize = 0;
  QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    it.next();
    QFileInfo fileInfo(it.filePath());
    if (fileInfo.isFile()) {
      totalSize += fileInfo.size();
    }
  }
  return totalSize;
}

QString formatSize(qint64 size) {
  const qint64 kb = 1024;
  const qint64 mb = 1024 * kb;
  const qint64 gb = 1024 * mb;

  if (size < gb) {
    double sizeMB = size / static_cast<double>(mb);
    return QString::number(sizeMB, 'f', 2) + " MB";
  } else {
    double sizeGB = size / static_cast<double>(gb);
    return QString::number(sizeGB, 'f', 2) + " GB";
  }
}
