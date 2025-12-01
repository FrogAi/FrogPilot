#pragma once

#include <QDirIterator>

#include "frogpilot/ui/qt/widgets/frogpilot_controls.h"

inline const QMap<QString, QString> midwestMap = {
  {"IL", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Illinois")},
  {"IN", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Indiana")},
  {"IA", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Iowa")},
  {"KS", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Kansas")},
  {"MI", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Michigan")},
  {"MN", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Minnesota")},
  {"MO", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Missouri")},
  {"NE", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Nebraska")},
  {"ND", QT_TRANSLATE_NOOP("MapSelectionControlStates", "North Dakota")},
  {"OH", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Ohio")},
  {"SD", QT_TRANSLATE_NOOP("MapSelectionControlStates", "South Dakota")},
  {"WI", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Wisconsin")}
};

inline const QMap<QString, QString> northeastMap = {
  {"CT", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Connecticut")},
  {"ME", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Maine")},
  {"MA", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Massachusetts")},
  {"NH", QT_TRANSLATE_NOOP("MapSelectionControlStates", "New Hampshire")},
  {"NJ", QT_TRANSLATE_NOOP("MapSelectionControlStates", "New Jersey")},
  {"NY", QT_TRANSLATE_NOOP("MapSelectionControlStates", "New York")},
  {"PA", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Pennsylvania")},
  {"RI", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Rhode Island")},
  {"VT", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Vermont")}
};

inline const QMap<QString, QString> southMap = {
  {"AL", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Alabama")},
  {"AR", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Arkansas")},
  {"DE", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Delaware")},
  {"DC", QT_TRANSLATE_NOOP("MapSelectionControlStates", "District of Columbia")},
  {"FL", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Florida")},
  {"GA", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Georgia")},
  {"KY", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Kentucky")},
  {"LA", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Louisiana")},
  {"MD", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Maryland")},
  {"MS", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Mississippi")},
  {"NC", QT_TRANSLATE_NOOP("MapSelectionControlStates", "North Carolina")},
  {"OK", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Oklahoma")},
  {"SC", QT_TRANSLATE_NOOP("MapSelectionControlStates", "South Carolina")},
  {"TN", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Tennessee")},
  {"TX", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Texas")},
  {"VA", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Virginia")},
  {"WV", QT_TRANSLATE_NOOP("MapSelectionControlStates", "West Virginia")}
};

inline const QMap<QString, QString> westMap = {
  {"AK", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Alaska")},
  {"AZ", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Arizona")},
  {"CA", QT_TRANSLATE_NOOP("MapSelectionControlStates", "California")},
  {"CO", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Colorado")},
  {"HI", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Hawaii")},
  {"ID", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Idaho")},
  {"MT", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Montana")},
  {"NV", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Nevada")},
  {"NM", QT_TRANSLATE_NOOP("MapSelectionControlStates", "New Mexico")},
  {"OR", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Oregon")},
  {"UT", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Utah")},
  {"WA", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Washington")},
  {"WY", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Wyoming")}
};

inline const QMap<QString, QString> territoriesMap = {
  {"AS", QT_TRANSLATE_NOOP("MapSelectionControlStates", "American Samoa")},
  {"GM", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Guam")},
  {"MP", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Northern Mariana Islands")},
  {"PR", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Puerto Rico")},
  {"VI", QT_TRANSLATE_NOOP("MapSelectionControlStates", "Virgin Islands")}
};

inline const QMap<QString, QString> africaMap = {
  {"DZ", QT_TRANSLATE_NOOP("MapSelectionControl", "Algeria")},
  {"AO", QT_TRANSLATE_NOOP("MapSelectionControl", "Angola")},
  {"BJ", QT_TRANSLATE_NOOP("MapSelectionControl", "Benin")},
  {"BW", QT_TRANSLATE_NOOP("MapSelectionControl", "Botswana")},
  {"BF", QT_TRANSLATE_NOOP("MapSelectionControl", "Burkina Faso")},
  {"BI", QT_TRANSLATE_NOOP("MapSelectionControl", "Burundi")},
  {"CM", QT_TRANSLATE_NOOP("MapSelectionControl", "Cameroon")},
  {"CF", QT_TRANSLATE_NOOP("MapSelectionControl", "Central African Republic")},
  {"TD", QT_TRANSLATE_NOOP("MapSelectionControl", "Chad")},
  {"CG", QT_TRANSLATE_NOOP("MapSelectionControl", "Congo (Brazzaville)")},
  {"CD", QT_TRANSLATE_NOOP("MapSelectionControl", "Congo (Kinshasa)")},
  {"DJ", QT_TRANSLATE_NOOP("MapSelectionControl", "Djibouti")},
  {"EG", QT_TRANSLATE_NOOP("MapSelectionControl", "Egypt")},
  {"GQ", QT_TRANSLATE_NOOP("MapSelectionControl", "Equatorial Guinea")},
  {"ER", QT_TRANSLATE_NOOP("MapSelectionControl", "Eritrea")},
  {"SZ", QT_TRANSLATE_NOOP("MapSelectionControl", "Eswatini")},
  {"ET", QT_TRANSLATE_NOOP("MapSelectionControl", "Ethiopia")},
  {"GA", QT_TRANSLATE_NOOP("MapSelectionControl", "Gabon")},
  {"GM", QT_TRANSLATE_NOOP("MapSelectionControl", "Gambia")},
  {"GH", QT_TRANSLATE_NOOP("MapSelectionControl", "Ghana")},
  {"GN", QT_TRANSLATE_NOOP("MapSelectionControl", "Guinea")},
  {"GW", QT_TRANSLATE_NOOP("MapSelectionControl", "Guinea-Bissau")},
  {"CI", QT_TRANSLATE_NOOP("MapSelectionControl", "Ivory Coast")},
  {"KE", QT_TRANSLATE_NOOP("MapSelectionControl", "Kenya")},
  {"LS", QT_TRANSLATE_NOOP("MapSelectionControl", "Lesotho")},
  {"LR", QT_TRANSLATE_NOOP("MapSelectionControl", "Liberia")},
  {"LY", QT_TRANSLATE_NOOP("MapSelectionControl", "Libya")},
  {"MG", QT_TRANSLATE_NOOP("MapSelectionControl", "Madagascar")},
  {"MW", QT_TRANSLATE_NOOP("MapSelectionControl", "Malawi")},
  {"ML", QT_TRANSLATE_NOOP("MapSelectionControl", "Mali")},
  {"MR", QT_TRANSLATE_NOOP("MapSelectionControl", "Mauritania")},
  {"MA", QT_TRANSLATE_NOOP("MapSelectionControl", "Morocco")},
  {"MZ", QT_TRANSLATE_NOOP("MapSelectionControl", "Mozambique")},
  {"NA", QT_TRANSLATE_NOOP("MapSelectionControl", "Namibia")},
  {"NE", QT_TRANSLATE_NOOP("MapSelectionControl", "Niger")},
  {"NG", QT_TRANSLATE_NOOP("MapSelectionControl", "Nigeria")},
  {"RW", QT_TRANSLATE_NOOP("MapSelectionControl", "Rwanda")},
  {"SN", QT_TRANSLATE_NOOP("MapSelectionControl", "Senegal")},
  {"SL", QT_TRANSLATE_NOOP("MapSelectionControl", "Sierra Leone")},
  {"SO", QT_TRANSLATE_NOOP("MapSelectionControl", "Somalia")},
  {"ZA", QT_TRANSLATE_NOOP("MapSelectionControl", "South Africa")},
  {"SS", QT_TRANSLATE_NOOP("MapSelectionControl", "South Sudan")},
  {"SD", QT_TRANSLATE_NOOP("MapSelectionControl", "Sudan")},
  {"TZ", QT_TRANSLATE_NOOP("MapSelectionControl", "Tanzania")},
  {"TG", QT_TRANSLATE_NOOP("MapSelectionControl", "Togo")},
  {"TN", QT_TRANSLATE_NOOP("MapSelectionControl", "Tunisia")},
  {"UG", QT_TRANSLATE_NOOP("MapSelectionControl", "Uganda")},
  {"ZM", QT_TRANSLATE_NOOP("MapSelectionControl", "Zambia")},
  {"ZW", QT_TRANSLATE_NOOP("MapSelectionControl", "Zimbabwe")}
};

inline const QMap<QString, QString> antarcticaMap = {
  {"AQ", QT_TRANSLATE_NOOP("MapSelectionControl", "Antarctica")}
};

inline const QMap<QString, QString> asiaMap = {
  {"AF", QT_TRANSLATE_NOOP("MapSelectionControl", "Afghanistan")},
  {"AM", QT_TRANSLATE_NOOP("MapSelectionControl", "Armenia")},
  {"AZ", QT_TRANSLATE_NOOP("MapSelectionControl", "Azerbaijan")},
  {"BD", QT_TRANSLATE_NOOP("MapSelectionControl", "Bangladesh")},
  {"BT", QT_TRANSLATE_NOOP("MapSelectionControl", "Bhutan")},
  {"BN", QT_TRANSLATE_NOOP("MapSelectionControl", "Brunei")},
  {"KH", QT_TRANSLATE_NOOP("MapSelectionControl", "Cambodia")},
  {"CN", QT_TRANSLATE_NOOP("MapSelectionControl", "China")},
  {"CY", QT_TRANSLATE_NOOP("MapSelectionControl", "Cyprus")},
  {"TL", QT_TRANSLATE_NOOP("MapSelectionControl", "East Timor")},
  {"IN", QT_TRANSLATE_NOOP("MapSelectionControl", "India")},
  {"ID", QT_TRANSLATE_NOOP("MapSelectionControl", "Indonesia")},
  {"IR", QT_TRANSLATE_NOOP("MapSelectionControl", "Iran")},
  {"IQ", QT_TRANSLATE_NOOP("MapSelectionControl", "Iraq")},
  {"IL", QT_TRANSLATE_NOOP("MapSelectionControl", "Israel")},
  {"JP", QT_TRANSLATE_NOOP("MapSelectionControl", "Japan")},
  {"JO", QT_TRANSLATE_NOOP("MapSelectionControl", "Jordan")},
  {"KZ", QT_TRANSLATE_NOOP("MapSelectionControl", "Kazakhstan")},
  {"KW", QT_TRANSLATE_NOOP("MapSelectionControl", "Kuwait")},
  {"KG", QT_TRANSLATE_NOOP("MapSelectionControl", "Kyrgyzstan")},
  {"LA", QT_TRANSLATE_NOOP("MapSelectionControl", "Laos")},
  {"LB", QT_TRANSLATE_NOOP("MapSelectionControl", "Lebanon")},
  {"MY", QT_TRANSLATE_NOOP("MapSelectionControl", "Malaysia")},
  {"MN", QT_TRANSLATE_NOOP("MapSelectionControl", "Mongolia")},
  {"MM", QT_TRANSLATE_NOOP("MapSelectionControl", "Myanmar")},
  {"NP", QT_TRANSLATE_NOOP("MapSelectionControl", "Nepal")},
  {"KP", QT_TRANSLATE_NOOP("MapSelectionControl", "North Korea")},
  {"OM", QT_TRANSLATE_NOOP("MapSelectionControl", "Oman")},
  {"PK", QT_TRANSLATE_NOOP("MapSelectionControl", "Pakistan")},
  {"PS", QT_TRANSLATE_NOOP("MapSelectionControl", "Palestine")},
  {"PH", QT_TRANSLATE_NOOP("MapSelectionControl", "Philippines")},
  {"QA", QT_TRANSLATE_NOOP("MapSelectionControl", "Qatar")},
  {"RU", QT_TRANSLATE_NOOP("MapSelectionControl", "Russia")},
  {"SA", QT_TRANSLATE_NOOP("MapSelectionControl", "Saudi Arabia")},
  {"KR", QT_TRANSLATE_NOOP("MapSelectionControl", "South Korea")},
  {"LK", QT_TRANSLATE_NOOP("MapSelectionControl", "Sri Lanka")},
  {"SY", QT_TRANSLATE_NOOP("MapSelectionControl", "Syria")},
  {"TW", QT_TRANSLATE_NOOP("MapSelectionControl", "Taiwan")},
  {"TJ", QT_TRANSLATE_NOOP("MapSelectionControl", "Tajikistan")},
  {"TH", QT_TRANSLATE_NOOP("MapSelectionControl", "Thailand")},
  {"TR", QT_TRANSLATE_NOOP("MapSelectionControl", "Turkey")},
  {"TM", QT_TRANSLATE_NOOP("MapSelectionControl", "Turkmenistan")},
  {"AE", QT_TRANSLATE_NOOP("MapSelectionControl", "United Arab Emirates")},
  {"UZ", QT_TRANSLATE_NOOP("MapSelectionControl", "Uzbekistan")},
  {"VN", QT_TRANSLATE_NOOP("MapSelectionControl", "Vietnam")},
  {"YE", QT_TRANSLATE_NOOP("MapSelectionControl", "Yemen")}
};

inline const QMap<QString, QString> europeMap = {
  {"AL", QT_TRANSLATE_NOOP("MapSelectionControl", "Albania")},
  {"AT", QT_TRANSLATE_NOOP("MapSelectionControl", "Austria")},
  {"BY", QT_TRANSLATE_NOOP("MapSelectionControl", "Belarus")},
  {"BE", QT_TRANSLATE_NOOP("MapSelectionControl", "Belgium")},
  {"BA", QT_TRANSLATE_NOOP("MapSelectionControl", "Bosnia and Herzegovina")},
  {"BG", QT_TRANSLATE_NOOP("MapSelectionControl", "Bulgaria")},
  {"HR", QT_TRANSLATE_NOOP("MapSelectionControl", "Croatia")},
  {"CZ", QT_TRANSLATE_NOOP("MapSelectionControl", "Czech Republic")},
  {"DK", QT_TRANSLATE_NOOP("MapSelectionControl", "Denmark")},
  {"EE", QT_TRANSLATE_NOOP("MapSelectionControl", "Estonia")},
  {"FI", QT_TRANSLATE_NOOP("MapSelectionControl", "Finland")},
  {"FR", QT_TRANSLATE_NOOP("MapSelectionControl", "France")},
  {"GE", QT_TRANSLATE_NOOP("MapSelectionControl", "Georgia")},
  {"DE", QT_TRANSLATE_NOOP("MapSelectionControl", "Germany")},
  {"GR", QT_TRANSLATE_NOOP("MapSelectionControl", "Greece")},
  {"HU", QT_TRANSLATE_NOOP("MapSelectionControl", "Hungary")},
  {"IS", QT_TRANSLATE_NOOP("MapSelectionControl", "Iceland")},
  {"IE", QT_TRANSLATE_NOOP("MapSelectionControl", "Ireland")},
  {"IT", QT_TRANSLATE_NOOP("MapSelectionControl", "Italy")},
  {"LV", QT_TRANSLATE_NOOP("MapSelectionControl", "Latvia")},
  {"LT", QT_TRANSLATE_NOOP("MapSelectionControl", "Lithuania")},
  {"LU", QT_TRANSLATE_NOOP("MapSelectionControl", "Luxembourg")},
  {"MD", QT_TRANSLATE_NOOP("MapSelectionControl", "Moldova")},
  {"ME", QT_TRANSLATE_NOOP("MapSelectionControl", "Montenegro")},
  {"NL", QT_TRANSLATE_NOOP("MapSelectionControl", "Netherlands")},
  {"MK", QT_TRANSLATE_NOOP("MapSelectionControl", "North Macedonia")},
  {"NO", QT_TRANSLATE_NOOP("MapSelectionControl", "Norway")},
  {"PL", QT_TRANSLATE_NOOP("MapSelectionControl", "Poland")},
  {"PT", QT_TRANSLATE_NOOP("MapSelectionControl", "Portugal")},
  {"RO", QT_TRANSLATE_NOOP("MapSelectionControl", "Romania")},
  {"RS", QT_TRANSLATE_NOOP("MapSelectionControl", "Serbia")},
  {"SK", QT_TRANSLATE_NOOP("MapSelectionControl", "Slovakia")},
  {"SI", QT_TRANSLATE_NOOP("MapSelectionControl", "Slovenia")},
  {"ES", QT_TRANSLATE_NOOP("MapSelectionControl", "Spain")},
  {"SE", QT_TRANSLATE_NOOP("MapSelectionControl", "Sweden")},
  {"CH", QT_TRANSLATE_NOOP("MapSelectionControl", "Switzerland")},
  {"UA", QT_TRANSLATE_NOOP("MapSelectionControl", "Ukraine")},
  {"GB", QT_TRANSLATE_NOOP("MapSelectionControl", "United Kingdom")}
};

inline const QMap<QString, QString> northAmericaMap = {
  {"BS", QT_TRANSLATE_NOOP("MapSelectionControl", "Bahamas")},
  {"BZ", QT_TRANSLATE_NOOP("MapSelectionControl", "Belize")},
  {"CA", QT_TRANSLATE_NOOP("MapSelectionControl", "Canada")},
  {"CR", QT_TRANSLATE_NOOP("MapSelectionControl", "Costa Rica")},
  {"CU", QT_TRANSLATE_NOOP("MapSelectionControl", "Cuba")},
  {"DO", QT_TRANSLATE_NOOP("MapSelectionControl", "Dominican Republic")},
  {"SV", QT_TRANSLATE_NOOP("MapSelectionControl", "El Salvador")},
  {"GL", QT_TRANSLATE_NOOP("MapSelectionControl", "Greenland")},
  {"GT", QT_TRANSLATE_NOOP("MapSelectionControl", "Guatemala")},
  {"HT", QT_TRANSLATE_NOOP("MapSelectionControl", "Haiti")},
  {"HN", QT_TRANSLATE_NOOP("MapSelectionControl", "Honduras")},
  {"JM", QT_TRANSLATE_NOOP("MapSelectionControl", "Jamaica")},
  {"MX", QT_TRANSLATE_NOOP("MapSelectionControl", "Mexico")},
  {"NI", QT_TRANSLATE_NOOP("MapSelectionControl", "Nicaragua")},
  {"PA", QT_TRANSLATE_NOOP("MapSelectionControl", "Panama")},
  {"TT", QT_TRANSLATE_NOOP("MapSelectionControl", "Trinidad and Tobago")},
  {"US", QT_TRANSLATE_NOOP("MapSelectionControl", "United States")}
};

inline const QMap<QString, QString> oceaniaMap = {
  {"AU", QT_TRANSLATE_NOOP("MapSelectionControl", "Australia")},
  {"FJ", QT_TRANSLATE_NOOP("MapSelectionControl", "Fiji")},
  {"TF", QT_TRANSLATE_NOOP("MapSelectionControl", "French Southern Territories")},
  {"NC", QT_TRANSLATE_NOOP("MapSelectionControl", "New Caledonia")},
  {"NZ", QT_TRANSLATE_NOOP("MapSelectionControl", "New Zealand")},
  {"PG", QT_TRANSLATE_NOOP("MapSelectionControl", "Papua New Guinea")},
  {"SB", QT_TRANSLATE_NOOP("MapSelectionControl", "Solomon Islands")},
  {"VU", QT_TRANSLATE_NOOP("MapSelectionControl", "Vanuatu")}
};

inline const QMap<QString, QString> southAmericaMap = {
  {"AR", QT_TRANSLATE_NOOP("MapSelectionControl", "Argentina")},
  {"BO", QT_TRANSLATE_NOOP("MapSelectionControl", "Bolivia")},
  {"BR", QT_TRANSLATE_NOOP("MapSelectionControl", "Brazil")},
  {"CL", QT_TRANSLATE_NOOP("MapSelectionControl", "Chile")},
  {"CO", QT_TRANSLATE_NOOP("MapSelectionControl", "Colombia")},
  {"EC", QT_TRANSLATE_NOOP("MapSelectionControl", "Ecuador")},
  {"FK", QT_TRANSLATE_NOOP("MapSelectionControl", "Falkland Islands")},
  {"GY", QT_TRANSLATE_NOOP("MapSelectionControl", "Guyana")},
  {"PY", QT_TRANSLATE_NOOP("MapSelectionControl", "Paraguay")},
  {"PE", QT_TRANSLATE_NOOP("MapSelectionControl", "Peru")},
  {"SR", QT_TRANSLATE_NOOP("MapSelectionControl", "Suriname")},
  {"UY", QT_TRANSLATE_NOOP("MapSelectionControl", "Uruguay")},
  {"VE", QT_TRANSLATE_NOOP("MapSelectionControl", "Venezuela")}
};

inline QString calculateDirectorySize(const QDir &directory) {
  constexpr double MB = 1024.0 * 1024.0;
  constexpr double GB = 1024.0 * MB;

  if (!directory.exists()) {
    return QObject::tr("0 MB");
  }

  double totalSize = 0;
  QDirIterator it(directory.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    it.next();
    totalSize += it.fileInfo().size();
  }

  if (totalSize >= GB) {
    return QString::number(totalSize / GB, 'f', 2) + QObject::tr(" GB");
  }
  return QString::number(totalSize / MB, 'f', 2) + QObject::tr(" MB");
}

inline QString formatElapsedTime(float elapsedMilliseconds) {
  int totalSeconds = elapsedMilliseconds / 1000;
  int hours = totalSeconds / 3600;
  int minutes = (totalSeconds % 3600) / 60;
  int seconds = totalSeconds % 60;

  QString formattedTime;
  if (hours > 0) {
    formattedTime += QString::number(hours) + (hours == 1 ? QObject::tr(" hour ") : QObject::tr(" hours "));
  }
  if (minutes > 0) {
    formattedTime += QString::number(minutes) + (minutes == 1 ? QObject::tr(" minute ") : QObject::tr(" minutes "));
  }
  formattedTime += QString::number(seconds) + (seconds == 1 ? QObject::tr(" second") : QObject::tr(" seconds"));

  return formattedTime.trimmed();
}

inline QString formatETA(float elapsedTime, int downloadedFiles, int previousDownloadedFiles, int totalFiles, const QDateTime &startTime) {
  static QDateTime estimatedFinishTime;

  if (downloadedFiles != previousDownloadedFiles) {
    estimatedFinishTime = startTime.addMSecs((elapsedTime * totalFiles) / downloadedFiles);
  }

  int remainingTime = qMax<qint64>(0, QDateTime::currentDateTime().secsTo(estimatedFinishTime));

  QString estimatedFinishTimeStr = formatShortTime(estimatedFinishTime.time());
  QString remainingTimeStr = formatElapsedTime(remainingTime * 1000);

  return QString("%1 (%2)").arg(remainingTimeStr).arg(estimatedFinishTimeStr);
}

class MapSelectionControl : public QWidget {
  Q_OBJECT

public:
  MapSelectionControl(const QMap<QString, QString> &map, bool isCountry = false);
  void reloadSelectedMaps();

private:
  void updateSelectedMaps();

  QButtonGroup *mapButtons;

  QString prefix;
};
