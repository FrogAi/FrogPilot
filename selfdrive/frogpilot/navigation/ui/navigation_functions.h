#pragma once

#include <filesystem>

#include <QDateTime>
#include <QJsonArray>

#include "selfdrive/frogpilot/ui/qt/widgets/frogpilot_controls.h"

inline QMap<QString, QString> northeastMap = {
  {"CT", "Connecticut"}, {"ME", "Maine"}, {"MA", "Massachusetts"},
  {"NH", "New Hampshire"}, {"NJ", "New Jersey"}, {"NY", "New York"},
  {"PA", "Pennsylvania"}, {"RI", "Rhode Island"}, {"VT", "Vermont"}
};

inline QMap<QString, QString> midwestMap = {
  {"IL", "Illinois"}, {"IN", "Indiana"}, {"IA", "Iowa"},
  {"KS", "Kansas"}, {"MI", "Michigan"}, {"MN", "Minnesota"},
  {"MO", "Missouri"}, {"NE", "Nebraska"}, {"ND", "North Dakota"},
  {"OH", "Ohio"}, {"SD", "South Dakota"}, {"WI", "Wisconsin"}
};

inline QMap<QString, QString> southMap = {
  {"AL", "Alabama"}, {"AR", "Arkansas"}, {"DE", "Delaware"},
  {"DC", "District of Columbia"}, {"FL", "Florida"}, {"GA", "Georgia"},
  {"KY", "Kentucky"}, {"LA", "Louisiana"}, {"MD", "Maryland"},
  {"MS", "Mississippi"}, {"NC", "North Carolina"}, {"OK", "Oklahoma"},
  {"SC", "South Carolina"}, {"TN", "Tennessee"}, {"TX", "Texas"},
  {"VA", "Virginia"}, {"WV", "West Virginia"}
};

inline QMap<QString, QString> westMap = {
  {"AK", "Alaska"}, {"AZ", "Arizona"}, {"CA", "California"},
  {"CO", "Colorado"}, {"HI", "Hawaii"}, {"ID", "Idaho"},
  {"MT", "Montana"}, {"NV", "Nevada"}, {"NM", "New Mexico"},
  {"OR", "Oregon"}, {"UT", "Utah"}, {"WA", "Washington"},
  {"WY", "Wyoming"}
};

inline QMap<QString, QString> territoriesMap = {
  {"AS", "American Samoa"}, {"GU", "Guam"}, {"MP", "Northern Mariana Islands"},
  {"PR", "Puerto Rico"}, {"VI", "Virgin Islands"}
};

inline QMap<QString, QString> africaMap = {
  {"DZ", "Algeria"}, {"AO", "Angola"}, {"BJ", "Benin"},
  {"BW", "Botswana"}, {"BF", "Burkina Faso"}, {"BI", "Burundi"},
  {"CM", "Cameroon"}, {"CF", "Central African Republic"}, {"TD", "Chad"},
  {"KM", "Comoros"}, {"CG", "Congo (Brazzaville)"}, {"CD", "Congo (Kinshasa)"},
  {"DJ", "Djibouti"}, {"EG", "Egypt"}, {"GQ", "Equatorial Guinea"},
  {"ER", "Eritrea"}, {"ET", "Ethiopia"}, {"GA", "Gabon"},
  {"GM", "Gambia"}, {"GH", "Ghana"}, {"GN", "Guinea"},
  {"GW", "Guinea-Bissau"}, {"CI", "Ivory Coast"}, {"KE", "Kenya"},
  {"LS", "Lesotho"}, {"LR", "Liberia"}, {"LY", "Libya"},
  {"MG", "Madagascar"}, {"MW", "Malawi"}, {"ML", "Mali"},
  {"MR", "Mauritania"}, {"MA", "Morocco"}, {"MZ", "Mozambique"},
  {"NA", "Namibia"}, {"NE", "Niger"}, {"NG", "Nigeria"},
  {"RW", "Rwanda"}, {"SN", "Senegal"}, {"SL", "Sierra Leone"},
  {"SO", "Somalia"}, {"ZA", "South Africa"}, {"SS", "South Sudan"},
  {"SD", "Sudan"}, {"SZ", "Swaziland"}, {"TZ", "Tanzania"},
  {"TG", "Togo"}, {"TN", "Tunisia"}, {"UG", "Uganda"},
  {"ZM", "Zambia"}, {"ZW", "Zimbabwe"}
};

inline QMap<QString, QString> antarcticaMap = {
  {"AQ", "Antarctica"}
};

inline QMap<QString, QString> asiaMap = {
  {"AF", "Afghanistan"}, {"AM", "Armenia"}, {"AZ", "Azerbaijan"},
  {"BH", "Bahrain"}, {"BD", "Bangladesh"}, {"BT", "Bhutan"},
  {"BN", "Brunei"}, {"KH", "Cambodia"}, {"CN", "China"},
  {"CY", "Cyprus"}, {"TL", "East Timor"}, {"HK", "Hong Kong"},
  {"IN", "India"}, {"ID", "Indonesia"}, {"IR", "Iran"},
  {"IQ", "Iraq"}, {"IL", "Israel"}, {"JP", "Japan"},
  {"JO", "Jordan"}, {"KZ", "Kazakhstan"}, {"KW", "Kuwait"},
  {"KG", "Kyrgyzstan"}, {"LA", "Laos"}, {"LB", "Lebanon"},
  {"MY", "Malaysia"}, {"MV", "Maldives"}, {"MO", "Macao"},
  {"MN", "Mongolia"}, {"MM", "Myanmar"}, {"NP", "Nepal"},
  {"KP", "North Korea"}, {"OM", "Oman"}, {"PK", "Pakistan"},
  {"PS", "Palestine"}, {"PH", "Philippines"}, {"QA", "Qatar"},
  {"RU", "Russia"}, {"SA", "Saudi Arabia"}, {"SG", "Singapore"},
  {"KR", "South Korea"}, {"LK", "Sri Lanka"}, {"SY", "Syria"},
  {"TW", "Taiwan"}, {"TJ", "Tajikistan"}, {"TH", "Thailand"},
  {"TR", "Turkey"}, {"TM", "Turkmenistan"}, {"AE", "United Arab Emirates"},
  {"UZ", "Uzbekistan"}, {"VN", "Vietnam"}, {"YE", "Yemen"}
};

inline QMap<QString, QString> europeMap = {
  {"AL", "Albania"}, {"AT", "Austria"}, {"BY", "Belarus"},
  {"BE", "Belgium"}, {"BA", "Bosnia and Herzegovina"}, {"BG", "Bulgaria"},
  {"HR", "Croatia"}, {"CZ", "Czech Republic"}, {"DK", "Denmark"},
  {"EE", "Estonia"}, {"FI", "Finland"}, {"FR", "France"},
  {"GE", "Georgia"}, {"DE", "Germany"}, {"GR", "Greece"},
  {"HU", "Hungary"}, {"IS", "Iceland"}, {"IE", "Ireland"},
  {"IT", "Italy"}, {"KZ", "Kazakhstan"}, {"LV", "Latvia"},
  {"LT", "Lithuania"}, {"LU", "Luxembourg"}, {"MK", "Macedonia"},
  {"MD", "Moldova"}, {"ME", "Montenegro"}, {"NL", "Netherlands"},
  {"NO", "Norway"}, {"PL", "Poland"}, {"PT", "Portugal"},
  {"RO", "Romania"}, {"RS", "Serbia"}, {"SK", "Slovakia"},
  {"SI", "Slovenia"}, {"ES", "Spain"}, {"SE", "Sweden"},
  {"CH", "Switzerland"}, {"TR", "Turkey"}, {"UA", "Ukraine"},
  {"GB", "United Kingdom"}
};

inline QMap<QString, QString> northAmericaMap = {
  {"BS", "Bahamas"}, {"BZ", "Belize"}, {"CA", "Canada"},
  {"CR", "Costa Rica"}, {"CU", "Cuba"}, {"DO", "Dominican Republic"},
  {"SV", "El Salvador"}, {"GL", "Greenland"}, {"GD", "Grenada"},
  {"GT", "Guatemala"}, {"HT", "Haiti"}, {"HN", "Honduras"},
  {"JM", "Jamaica"}, {"MX", "Mexico"}, {"NI", "Nicaragua"},
  {"PA", "Panama"}, {"TT", "Trinidad and Tobago"}, {"US", "United States"}
};

inline QMap<QString, QString> oceaniaMap = {
  {"AU", "Australia"}, {"FJ", "Fiji"}, {"TF", "French Southern Territories"},
  {"NC", "New Caledonia"}, {"NZ", "New Zealand"}, {"PG", "Papua New Guinea"},
  {"SB", "Solomon Islands"}, {"VU", "Vanuatu"}
};

inline QMap<QString, QString> southAmericaMap = {
  {"AR", "Argentina"}, {"BO", "Bolivia"}, {"BR", "Brazil"},
  {"CL", "Chile"}, {"CO", "Colombia"}, {"EC", "Ecuador"},
  {"FK", "Falkland Islands"}, {"GY", "Guyana"}, {"PY", "Paraguay"},
  {"PE", "Peru"}, {"SR", "Suriname"}, {"UY", "Uruguay"},
  {"VE", "Venezuela"}
};

inline QString calculateDirectorySize(const QString &directoryPath) {
  namespace fs = std::filesystem;

  constexpr double oneMB = 1024 * 1024;
  constexpr double oneGB = 1024 * 1024 * 1024;

  fs::path path(directoryPath.toStdString());
  if (!fs::exists(path) || !fs::is_directory(path)) {
    return "0 MB";
  }

  double totalSize = 0;
  for (fs::recursive_directory_iterator iter(path, fs::directory_options::skip_permission_denied), end; iter != end; ++iter) {
    const fs::directory_entry &entry = *iter;
    if (entry.is_regular_file()) {
      totalSize += entry.file_size();
    }
  }

  if (totalSize >= oneGB) {
    return QString::number(totalSize / oneGB, 'f', 2) + " GB";
  }
  return QString::number(totalSize / oneMB, 'f', 2) + " MB";
}

inline QString daySuffix(int day) {
  if (day % 10 == 1 && day != 11) return "st";
  if (day % 10 == 2 && day != 12) return "nd";
  if (day % 10 == 3 && day != 13) return "rd";
  return "th";
}

inline QString formatCurrentDate() {
  QDate currentDate = QDate::currentDate();
  return currentDate.toString("MMMM d'") + daySuffix(currentDate.day()) + QString(", %1").arg(currentDate.year());
}

inline QString formatElapsedTime(float elapsedMilliseconds) {
  int totalSeconds = elapsedMilliseconds / 1000;
  int hours = totalSeconds / 3600;
  int minutes = (totalSeconds % 3600) / 60;
  int seconds = totalSeconds % 60;

  QString formattedTime;
  if (hours > 0) {
    formattedTime += QString::number(hours) + (hours == 1 ? " hour " : " hours ");
  }
  if (minutes > 0) {
    formattedTime += QString::number(minutes) + (minutes == 1 ? " minute " : " minutes ");
  }
  formattedTime += QString::number(seconds) + (seconds == 1 ? " second" : " seconds");

  return formattedTime.trimmed();
}

inline QString formatETA(float elapsedTime, int downloadedFiles, int previousDownloadedFiles, int totalFiles, QDateTime &startTime) {
  static QDateTime estimatedFinishTime;

  static float previousElapsedTime;

  if (downloadedFiles != previousDownloadedFiles) {
    estimatedFinishTime = startTime.addMSecs((elapsedTime * totalFiles) / downloadedFiles);
  } else {
    estimatedFinishTime = estimatedFinishTime.addSecs((previousElapsedTime - elapsedTime) / 1000);
  }
  previousElapsedTime = elapsedTime;

  int remainingTime = QDateTime::currentDateTime().secsTo(estimatedFinishTime);

  QString estimatedFinishTimeStr = estimatedFinishTime.toString("h:mm AP");
  QString remainingTimeStr = formatElapsedTime(remainingTime * 1000);

  return QString("%1 (%2)").arg(remainingTimeStr).arg(estimatedFinishTimeStr);
}

class MapSelectionControl : public QWidget {
  Q_OBJECT

public:
  MapSelectionControl(const QMap<QString, QString> &map, bool isCountry = false);

private:
  void loadSelectedMaps();
  void updateSelectedMaps();

  bool isCountry;

  Params params;

  QButtonGroup *mapButtons;

  QGridLayout *mapLayout;

  QJsonArray mapSelections;

  QList<QAbstractButton *> maps;

  QMap<QString, QString> mapData;

  QString selectionType;
};
