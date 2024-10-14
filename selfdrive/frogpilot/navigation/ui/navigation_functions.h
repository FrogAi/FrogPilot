#pragma once

#include <filesystem>
#include <string>

#include <QDateTime>

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
  {"GM", "Gambia"},  {"GH", "Ghana"}, {"GN", "Guinea"},
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

namespace fs = std::filesystem;

inline bool isMapdRunning() {
  return std::system("pgrep mapd > /dev/null 2>&1") == 0;
}

inline QString calculateDirectorySize(const QString &directoryPath) {
  constexpr uintmax_t oneGB = 1024 * 1024 * 1024;
  constexpr uintmax_t oneMB = 1024 * 1024;

  uintmax_t totalSize = 0;
  fs::path path(directoryPath.toStdString());

  if (!fs::exists(path) || !fs::is_directory(path)) {
    return "0 MB";
  }

  for (fs::recursive_directory_iterator iter(path, fs::directory_options::skip_permission_denied), end; iter != end; ++iter) {
    const fs::directory_entry &entry = *iter;
    if (entry.is_regular_file()) {
      totalSize += entry.file_size();
    }
  }

  if (totalSize >= oneGB) {
    return QString::number(static_cast<double>(totalSize) / oneGB, 'f', 2) + " GB";
  } else {
    return QString::number(static_cast<double>(totalSize) / oneMB, 'f', 2) + " MB";
  }
}

inline QString formatCurrentDate() {
  QDate currentDate = QDate::currentDate();
  QString suffix;
  int day = currentDate.day();

  if (day % 10 == 1 && day != 11) {
    suffix = "st";
  } else if (day % 10 == 2 && day != 12) {
    suffix = "nd";
  } else if (day % 10 == 3 && day != 13) {
    suffix = "rd";
  } else {
    suffix = "th";
  }

  return currentDate.toString("MMMM d'") + suffix + QString(", %1").arg(currentDate.year());
}

inline QString formatElapsedTime(qint64 elapsedMilliseconds) {
  qint64 totalSeconds = elapsedMilliseconds / 1000;
  qint64 hours = totalSeconds / 3600;
  qint64 minutes = (totalSeconds % 3600) / 60;
  qint64 seconds = totalSeconds % 60;

  QString formattedTime;
  if (hours > 0) {
    formattedTime += QString::number(hours) + (hours == 1 ? " hour " : " hours ");
  }
  if (minutes > 0) {
    formattedTime += QString::number(minutes) + (minutes == 1 ? " minute " : " minutes ");
  }
  formattedTime += QString::number(seconds) + (seconds == 1 ? " second" : " seconds");

  return formattedTime;
}

class MapSelectionControl : public QWidget {
  Q_OBJECT

public:
  MapSelectionControl(const QMap<QString, QString> &map, bool isCountry = false, QWidget *parent = nullptr);

private:
  void loadSelectedMaps();
  void updateSelectedMaps();

  Params params;

  QButtonGroup *buttonGroup;

  QGridLayout *gridLayout;

  QMap<QString, QString> mapData;

  bool isCountry;
};
