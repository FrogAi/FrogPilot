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
  {"CT", "Connecticut"}, {"ME", "Maine"}, {"MA", "Massachusetts"},
  {"NH", "New Hampshire"}, {"NJ", "New Jersey"}, {"NY", "New York"},
  {"PA", "Pennsylvania"}, {"RI", "Rhode Island"}, {"VT", "Vermont"}
};

QMap<QString, QString> midwestMap = {
  {"IL", "Illinois"}, {"IN", "Indiana"}, {"IA", "Iowa"},
  {"KS", "Kansas"}, {"MI", "Michigan"}, {"MN", "Minnesota"},
  {"MO", "Missouri"}, {"NE", "Nebraska"}, {"ND", "North Dakota"},
  {"OH", "Ohio"}, {"SD", "South Dakota"}, {"WI", "Wisconsin"}
};

QMap<QString, QString> southMap = {
  {"AL", "Alabama"}, {"AR", "Arkansas"}, {"DE", "Delaware"},
  {"DC", "District of Columbia"}, {"FL", "Florida"}, {"GA", "Georgia"},
  {"KY", "Kentucky"}, {"LA", "Louisiana"}, {"MS", "Mississippi"},
  {"NC", "North Carolina"}, {"OK", "Oklahoma"}, {"SC", "South Carolina"},
  {"TN", "Tennessee"}, {"TX", "Texas"}, {"VA", "Virginia"},
  {"WV", "West Virginia"}
};

QMap<QString, QString> westMap = {
  {"AK", "Alaska"}, {"AZ", "Arizona"}, {"CA", "California"},
  {"CO", "Colorado"}, {"HI", "Hawaii"}, {"ID", "Idaho"},
  {"MT", "Montana"}, {"NV", "Nevada"}, {"NM", "New Mexico"},
  {"OR", "Oregon"}, {"UT", "Utah"}, {"WA", "Washington"},
  {"WY", "Wyoming"}
};

QMap<QString, QString> territoriesMap = {
  {"AS", "American Samoa"}, {"GU", "Guam"}, {"MP", "Northern Mariana Islands"},
  {"PR", "Puerto Rico"}, {"VI", "Virgin Islands"}
};

QMap<QString, QString> africaMap = {
  {"DZ", "Algeria"}, {"AO", "Angola"}, {"BJ", "Benin"}, {"BW", "Botswana"},
  {"BF", "Burkina Faso"}, {"BI", "Burundi"}, {"CM", "Cameroon"}, {"CV", "Cape Verde"},
  {"CF", "Central African Republic"}, {"CE", "Ceuta"}, {"TD", "Chad"},
  {"KM", "Comoros"}, {"CG", "Congo-Brazzaville"}, {"CD", "Congo-Kinshasa"},
  {"CI", "Côte d'Ivoire"}, {"DJ", "Djibouti"}, {"EG", "Egypt"},
  {"GQ", "Equatorial Guinea"}, {"ER", "Eritrea"}, {"SZ", "Eswatini"},
  {"ET", "Ethiopia"}, {"GA", "Gabon"}, {"GM", "Gambia"}, {"GH", "Ghana"},
  {"GN", "Guinea"}, {"GW", "Guinea-Bissau"}, {"KE", "Kenya"},
  {"LS", "Lesotho"}, {"LR", "Liberia"}, {"LY", "Libya"},
  {"MG", "Madagascar"}, {"MW", "Malawi"}, {"ML", "Mali"}, {"MR", "Mauritania"},
  {"MU", "Mauritius"}, {"YT", "Mayotte"}, {"ML", "Melilla"},
  {"MA", "Morocco"}, {"MZ", "Mozambique"}, {"NA", "Namibia"}, {"NE", "Niger"}, {"NG", "Nigeria"},
  {"RE", "La Réunion"}, {"RW", "Rwanda"},
  {"SH", "Saint Helena, Ascension and Tristan da Cunha"}, {"ST", "São Tomé and Príncipe"},
  {"SN", "Senegal"}, {"SC", "Seychelles"}, {"SL", "Sierra Leone"},
  {"SO", "Somalia"}, {"ZA", "South Africa"}, {"SS", "South Sudan"},
  {"SD", "Sudan"}, {"TZ", "Tanzania"}, {"TG", "Togo"}, {"TN", "Tunisia"},
  {"UG", "Uganda"}, {"EH", "Western Sahara"}, {"ZM", "Zambia"},
  {"ZW", "Zimbabwe"}
};

QMap<QString, QString> antarcticaMap = {
  {"AQ", "Antarctica"}
};

QMap<QString, QString> asiaMap = {
  {"AF", "Afghanistan"}, {"AM", "Armenia"}, {"AZ", "Azerbaijan"},
  {"BH", "Bahrain"}, {"BD", "Bangladesh"}, {"BT", "Bhutan"},
  {"BN", "Brunei"}, {"KH", "Cambodia"}, {"CN", "China"},
  {"CY", "Cyprus"}, {"TL", "Timor-Leste"}, {"GE", "Georgia"},
  {"HK", "Hong Kong"}, {"IN", "India"}, {"ID", "Indonesia"},
  {"IR", "Iran"}, {"IQ", "Iraq"}, {"IL", "Israel"}, {"JP", "Japan"},
  {"JO", "Jordan"}, {"KZ", "Kazakhstan"}, {"KW", "Kuwait"},
  {"KG", "Kyrgyzstan"}, {"LA", "Laos"}, {"LB", "Lebanon"},
  {"MO", "Macau"}, {"MY", "Malaysia"}, {"MV", "Maldives"},
  {"MN", "Mongolia"}, {"MM", "Myanmar"}, {"NP", "Nepal"},
  {"KP", "North Korea"}, {"OM", "Oman"}, {"PK", "Pakistan"},
  {"PS", "Palestine"}, {"PH", "Philippines"}, {"QA", "Qatar"},
  {"RU", "Russia"}, {"SA", "Saudi Arabia"}, {"SG", "Singapore"},
  {"KR", "South Korea"}, {"LK", "Sri Lanka"}, {"SY", "Syria"},
  {"TW", "Taiwan"}, {"TJ", "Tajikistan"}, {"TH", "Thailand"},
  {"TR", "Turkey"}, {"TM", "Turkmenistan"}, {"AE", "United Arab Emirates"},
  {"UZ", "Uzbekistan"}, {"VN", "Vietnam"}, {"YE", "Yemen"}
};

QMap<QString, QString> europeMap = {
  {"AL", "Albania"}, {"AD", "Andorra"}, {"AM", "Armenia"},
  {"AT", "Austria"}, {"AZ", "Azerbaijan"}, {"BY", "Belarus"},
  {"BE", "Belgium"}, {"BA", "Bosnia and Herzegovina"}, {"BG", "Bulgaria"},
  {"HR", "Croatia"}, {"CY", "Cyprus"}, {"CZ", "Czech Republic"},
  {"DK", "Denmark"}, {"EE", "Estonia"}, {"FI", "Finland"},
  {"FR", "France"}, {"GE", "Georgia"}, {"DE", "Germany"},
  {"GI", "Gibraltar"}, {"GR", "Greece"}, {"GG", "Guernsey"},
  {"HU", "Hungary"}, {"IS", "Iceland"}, {"IE", "Ireland"},
  {"IM", "Isle of Man"}, {"IT", "Italy"}, {"JE", "Jersey"},
  {"LV", "Latvia"}, {"LI", "Liechtenstein"}, {"LT", "Lithuania"},
  {"LU", "Luxembourg"}, {"MT", "Malta"}, {"MD", "Moldova"},
  {"MC", "Monaco"}, {"ME", "Montenegro"}, {"NL", "Netherlands"},
  {"MK", "North Macedonia"}, {"NO", "Norway"}, {"PL", "Poland"},
  {"PT", "Portugal"}, {"RO", "Romania"}, {"RU", "Russia"},
  {"SM", "San Marino"}, {"RS", "Serbia"}, {"SK", "Slovakia"},
  {"SI", "Slovenia"}, {"ES", "Spain"}, {"SJ", "Svalbard and Jan Mayen"},
  {"SE", "Sweden"}, {"CH", "Switzerland"}, {"UA", "Ukraine"},
  {"GB", "United Kingdom"}, {"VA", "Vatican City"}
};

QMap<QString, QString> northAmericaMap = {
  {"AG", "Antigua and Barbuda"}, {"AI", "Anguilla"}, {"AW", "Aruba"},
  {"BS", "Bahamas"}, {"BB", "Barbados"}, {"BZ", "Belize"},
  {"BM", "Bermuda"}, {"CA", "Canada"}, {"KY", "Cayman Islands"},
  {"CR", "Costa Rica"}, {"CU", "Cuba"}, {"CW", "Curaçao"},
  {"DM", "Dominica"}, {"DO", "Dominican Republic"}, {"SV", "El Salvador"},
  {"GL", "Greenland"}, {"GD", "Grenada"}, {"GP", "Guadeloupe"},
  {"GT", "Guatemala"}, {"HT", "Haiti"}, {"HN", "Honduras"},
  {"JM", "Jamaica"}, {"MX", "Mexico"}, {"MS", "Montserrat"},
  {"NI", "Nicaragua"}, {"PA", "Panama"}, {"KN", "Saint Kitts and Nevis"},
  {"LC", "Saint Lucia"}, {"VC", "Saint Vincent and the Grenadines"},
  {"SX", "Sint Maarten"}, {"TT", "Trinidad and Tobago"},
  {"TC", "Turks and Caicos Islands"}, {"US", "United States"},
  {"VG", "British Virgin Islands"}, {"VI", "United States Virgin Islands"}
};

QMap<QString, QString> oceaniaMap = {
  {"AS", "American Samoa"}, {"AU", "Australia"}, {"CK", "Cook Islands"},
  {"FJ", "Fiji"}, {"PF", "French Polynesia"}, {"GU", "Guam"},
  {"KI", "Kiribati"}, {"MH", "Marshall Islands"}, {"FM", "Micronesia"},
  {"NR", "Nauru"}, {"NC", "New Caledonia"}, {"NZ", "New Zealand"},
  {"NU", "Niue"}, {"NF", "Norfolk Island"}, {"MP", "Northern Mariana Islands"},
  {"PW", "Palau"}, {"PG", "Papua New Guinea"}, {"PN", "Pitcairn Islands"},
  {"WS", "Samoa"}, {"SB", "Solomon Islands"}, {"TK", "Tokelau"},
  {"TO", "Tonga"}, {"TV", "Tuvalu"}, {"VU", "Vanuatu"},
  {"WF", "Wallis and Futuna"}
};

QMap<QString, QString> southAmericaMap = {
  {"AR", "Argentina"}, {"BO", "Bolivia"}, {"BR", "Brazil"},
  {"CL", "Chile"}, {"CO", "Colombia"}, {"EC", "Ecuador"},
  {"FK", "Falkland Islands"}, {"GF", "French Guiana"},
  {"GY", "Guyana"}, {"PY", "Paraguay"}, {"PE", "Peru"},
  {"SR", "Suriname"}, {"GS", "South Georgia and the South Sandwich Islands"},
  {"UY", "Uruguay"}, {"VE", "Venezuela"}
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

    QList<QString> sortedKeys = map.keys();
    std::sort(sortedKeys.begin(), sortedKeys.end(), [&](const QString &a, const QString &b) {
      return map[a] < map[b];
    });

    for (const QString &stateCode : sortedKeys) {
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
    QObject::connect(button, &QPushButton::clicked, this, [this, button, stateCode] { updateState(stateCode, button); });
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
