(() => {
"use strict";

const P = window.Pond;

const EXACT_TITLE_BONUS = 2;
const FIELDS = ["context", "description", "title", "words"];
const FOCUSABLE = "a[href], button, input, select, textarea, [tabindex]";
const HIDDEN_PENALTY = 0.8;
const HIGHLIGHT_TIME = 2000;
const MAX_RESULTS = 8;
const MIN_SCORE = 0.6;
const POND_BOOST = 1.2;
const SECOND_FIELD_SHARE = 0.25;
const TITLE_COVERAGE_BONUS = 1.5;
const TITLE_PHRASE_BONUS = 2;
const WEIGHTS = { context: 1.2, description: 0.8, title: 3, words: 2.5 };
const WORDS_PHRASE_BONUS = 1;

const POND = [
  { page: "backups", words: "backup restore save copy export import snapshot undo" },
  { page: "backups", title: "Back up FrogPilot", target: '[data-act="frogbackups-new"]', words: "save copy install version" },
  { page: "backups", title: "Back up settings now", target: '[data-act="backups-now"]', words: "save copy create backup" },
  { page: "backups", title: "FrogPilot backups", target: "section:has(#fb-h)", words: "install version rollback downgrade previous older go back" },
  { page: "backups", title: "Import a backup", target: "label:has([data-import])", words: "import upload backup file zip" },
  { page: "backups", title: "Reset settings to defaults", target: "section:has(#reset-h)", words: "reset defaults factory start over stock openpilot erase" },
  { page: "backups", title: "Restore a backup", target: "section:has(#bk-h)", words: "restore undo go back previous settings" },
  { page: "backups", title: "Settings backups", target: "section:has(#bk-h)", words: "settings backup restore download delete" },
  { page: "device", words: "power hardware temperature heat cpu gpu storage disk space battery voltage network wifi" },
  { page: "device", title: "Alerts on the car", target: "#device-alerts", words: "warnings notifications problems messages" },
  { page: "device", title: "Device status", target: "section:has([data-now])", words: "temperature heat hot cpu gpu car battery voltage network signal wifi" },
  { page: "device", title: "Power off", target: '.danger-zone:has([data-kind="off"])', words: "shutdown shut down turn off" },
  { page: "device", title: "Reboot", target: '.danger-zone:has([data-kind="reboot"])', words: "restart reset reboot" },
  { page: "device", title: "Soft reboot", target: '.danger-zone:has([data-kind="soft"])', words: "restart openpilot reboot" },
  { page: "device", title: "Storage breakdown", target: "#device-storage", words: "disk space free full storage used" },
  { page: "drives", words: "dashcam video footage clips camera recordings routes trips history replay watch" },
  { page: "drives", title: "Delete drives", target: '[data-act="clips-select-toggle"]', words: "delete remove erase clear free space select" },
  { page: "drives", title: "Favorite a drive", target: '[data-act="drives-favorite"]', words: "favorite star bookmark keep save preserve" },
  { page: "drives", title: "Search drives", target: "[data-list-q]", words: "find filter search date day" },
  { page: "home", words: "dashboard overview summary start" },
  { page: "home", title: "Driving stats", target: "section:has(#stats-h)", words: "statistics miles kilometers distance hours drives driven" },
  { page: "home", title: "FrogPilot stats", target: "section:has(#fpstats-h)", words: "statistics records most used favorite" },
  { page: "home", title: "Last drive", target: "section:has(#last-h)", words: "recent latest previous drive" },
  { page: "home", title: "Reset FrogPilot stats", target: '[data-act="home-reset-stats"]', words: "clear zero erase statistics" },
  { page: "keys", words: "api key token mapbox weather openweathermap" },
  { page: "keys", title: "Mapbox keys", target: 'section:has([data-kind="mapbox_public"])', words: "mapbox public secret key navigation", settings: ["MapboxPublicKey", "MapboxSecretKey"] },
  { page: "keys", title: "Weather key", target: "section:has([data-weather])", words: "weather key openweathermap token api", settings: ["WeatherToken"] },
  { page: "logs", words: "errors crash crashes bug bugs debug console terminal shell command tmux" },
  { page: "logs", title: "Error logs", target: '[data-files="error"]', words: "errors crash crashes bug bugs logs" },
  { page: "logs", title: "Live console", target: "section:has([data-console])", words: "console terminal tmux live output shell" },
  { page: "logs", title: "Run a command", target: "#console-in", words: "command terminal type run shell" },
  { page: "logs", title: "Saved console logs", target: '[data-files="tmux"]', words: "saved console logs tmux" },
  { page: "maps", words: "map offline speed limit data regions states countries osm download" },
  { page: "maps", title: "Choose map regions", target: "section:has([data-region-search])", words: "regions states countries select pick", settings: ["MapsSelected"] },
  { page: "maps", title: "Download maps", target: '[data-act="maps-download"]', words: "download offline speed limit maps get" },
  { page: "maps", title: "Map update schedule", target: ".field:has([data-schedule])", words: "update maps automatically weekly monthly schedule", settings: ["PreferredSchedule"] },
  { page: "maps", title: "Remove maps", target: '[data-act="maps-remove"]', words: "delete remove maps free space" },
  { page: "models", words: "model ai neural network brain driving model" },
  { page: "models", title: "Delete a model", target: ".models-list", words: "delete remove free space" },
  { page: "models", title: "Download a model", target: '[data-act="models-download-picked"]', words: "download get new model" },
  { page: "models", title: "Model blacklist", target: ".models-list", words: "blacklist exclude block random picks", settings: ["BlacklistedModels"] },
  { page: "models", title: "Model randomizer", target: "label:has([data-random])", words: "random shuffle", settings: ["ModelRandomizer"] },
  { page: "models", title: "Reset model ratings", target: '[data-act="models-ratings"]', words: "ratings scores reset" },
  { page: "models", title: "Switch driving model", target: ".models-list", words: "use select change pick switch model", settings: ["DrivingModel"] },
  { page: "recordings", words: "screen capture recording video footage clips instant replay dashcam", settings: ["InstantReplay", "ScreenRecorder"] },
  { page: "recordings", title: "Delete screen recordings", target: '[data-act="clips-select-toggle"]', words: "delete remove erase clear free space select" },
  { page: "report", words: "bug bugs problem issue crash feedback support help discord broken" },
  { page: "software", words: "update upgrade install version branch release download new" },
  { page: "software", title: "Automatic updates", target: "section:has([data-auto])", words: "auto automatically update background", settings: ["AutomaticUpdates"] },
  { page: "software", title: "Check for updates", target: '[data-act="check-update"]', words: "check update new version" },
  { page: "software", title: "Install the update", target: '[data-act="install-update"], [data-act="download-update"]', words: "install download update upgrade" },
  { page: "software", title: "Switch branch", target: "section:has([data-branch])", words: "branch channel release staging testing development switch" },
  { page: "startup", words: "startup alert boot message greeting welcome text", settings: ["StartupMessageBottom", "StartupMessageTop"] },
  { page: "tailscale", words: "vpn remote access away from home outside internet tunnel network" },
  { page: "tailscale", title: "Connect a phone or computer", target: '[data-act="tailscale-guide"], section:has(.ts-guide-body)', words: "phone computer laptop connect remote guide how" },
  { page: "tailscale", title: "Install Tailscale", target: '[data-act="tailscale-install"], section:has([data-act="tailscale-uninstall"])', words: "install vpn remote access" },
  { page: "tailscale", title: "Sign in to Tailscale", target: '[data-act="tailscale-login"], [data-act="tailscale-install"], section:has([data-act="tailscale-uninstall"]):not(:has([data-act="tailscale-login"]))', words: "login log in sign in connect account" },
  { page: "tailscale", title: "Tailscale device name", target: "[data-ts-name]", words: "hostname name address rename" },
  { page: "tailscale", title: "Uninstall Tailscale", target: '[data-act="tailscale-uninstall"]', words: "remove uninstall delete" },
  { page: "theme", words: "theme maker make create design build customize colors colours icons sounds skin" },
  { page: "theme", title: "Apply a theme to the car", target: '[data-act="tm-apply"]', words: "apply use set theme car" },
  { page: "theme", title: "Make a new theme", target: '.menu-wrap:has([data-act="tm-new"]) > [data-act="menu"]', words: "new create import export rename duplicate delete theme" },
  { page: "theme", title: "Submit a theme", target: '[data-act="tm-submit"]', words: "share submit upload theme" },
  { page: "themes", words: "themes theme packs pick select switch change colors colours icons sounds steering wheel turn signal personality skin appearance look style",
    settings: ["ColorScheme", "CustomThemes", "DistanceIconPack", "IconPack", "SignalAnimation", "SoundPack", "WheelIcon"] },
  { page: "themes", title: "Delete a theme", target: ".two-col:not([hidden])", words: "delete remove theme pack free space" },
  { page: "themes", title: "Download themes", target: '[data-act="themes-download-picked"]', words: "download get new more theme pack community" },
  { page: "vehicle", words: "car vehicle fingerprint calibration doors" },
  { page: "vehicle", title: "Calibration", target: 'section:has([data-act="vehicle-reset-calibration"])', words: "pitch yaw calibrate curve learning lateral grip" },
  { page: "vehicle", title: "Car details", target: "#vehicle-car", words: "fingerprint model make capabilities radar blind spot longitudinal stop and go" },
  { page: "vehicle", title: "Lock or unlock the doors", target: "[data-doors]", words: "lock car lock doors unlock doors locks" },
  { page: "vehicle", title: "Reset calibration", target: '.danger-zone:has([data-act="vehicle-reset-calibration"])', words: "reset calibration recalibrate" },
  { page: "vehicle", title: "Reset curve data", target: '.danger-zone:has([data-act="vehicle-reset-curves"])', words: "reset curves cornering learning", settings: ["ResetCurveData"] },
  { page: "vehicle", title: "Toyota security keys", target: 'section:has([data-act="secoc-add"])', words: "secoc key toyota lexus security" }
];

const SETTING_WORDS = {
  AccelerationProfile: "eco sport acceleration pickup speed up faster gentle",
  AdbEnabled: "adb usb debugging android",
  AlertVolumeControl: "volume loud quiet sound chime mute",
  AlwaysOnDM: "driver monitoring camera attention eyes face",
  AlwaysOnLateral: "aol steering always on lane keep mads",
  AutomaticUpdates: "auto update upgrade",
  CarMake: "car brand vehicle select",
  CarModel: "car vehicle select model",
  CEStopLights: "stop sign red light traffic light",
  ClusterOffset: "speedometer dashboard speed mismatch",
  ConditionalExperimental: "cem automatic experimental auto switch",
  CurveSpeedController: "curves corners turns bends slow down csc",
  CustomAlerts: "alerts chime notification sounds",
  CustomCruise: "set speed buttons increments step",
  CustomCruiseLong: "set speed buttons increments step hold",
  CustomPersonalities: "follow distance gap following aggressive relaxed personality",
  CustomThemes: "theme skin colors icons sounds",
  DecelerationProfile: "braking coast eco slow down",
  DeveloperUI: "developer debug metrics overlay numbers",
  DeviceShutdown: "turn off sleep timer battery drain power off shutdown",
  DisableOpenpilotLongitudinal: "stock acc cruise control longitudinal",
  DisengageOnAccelerator: "gas pedal accelerator disengage",
  DistanceButtonControl: "steering wheel button remap shortcut",
  DriverCamera: "reverse backup camera",
  DrivingModel: "model ai brain neural network",
  ExperimentalMode: "e2e end to end experimental chill",
  ForceFingerprint: "fingerprint car detection",
  ForceStops: "stop sign red light traffic light",
  GoatScream: "goat funny",
  GreenLightAlert: "traffic light green go chime",
  GsmApn: "cellular sim data mobile lte",
  GsmMetered: "cellular sim data mobile lte",
  GsmRoaming: "cellular sim data mobile lte",
  HigherBitrate: "video quality dashcam resolution",
  HolidayThemes: "christmas halloween holiday seasonal",
  IncreasedStoppedDistance: "gap stopped distance traffic light space",
  IncreaseThermalLimits: "overheat hot temperature thermal heat",
  InstantReplay: "dashcam replay save clip footage",
  IsMetric: "kilometers km kph units miles imperial metric",
  LaneChanges: "lane change nudge signal blinker merge",
  LanguageSetting: "language translate spanish french german",
  LateralTune: "steering tune smoother",
  LeadDepartingAlert: "car ahead moving lead departing go",
  LKASButtonControl: "lkas button steering wheel remap",
  LockDoorsTimer: "lock car after park walk away",
  LongitudinalPersonality: "follow distance aggressive relaxed standard gap",
  LoudBlindspotAlert: "blind spot bsm warning louder",
  LowVoltageShutdown: "battery drain voltage protect car battery",
  MapboxPublicKey: "mapbox navigation key",
  MapboxSecretKey: "mapbox navigation key",
  MapGears: "eco sport gear mode drive mode",
  MapsSelected: "offline speed limit maps regions states download",
  ModelRandomizer: "random model shuffle",
  NNFF: "steering smoother neural feedforward torque",
  NoLogging: "privacy record logs",
  NoUploads: "privacy data upload connect cellular data",
  NudgelessLaneChange: "nudgeless automatic lane change no nudge",
  OpenpilotEnabledToggle: "enable disable openpilot turn off",
  PauseLateralSpeed: "pause steering low speed parking lot",
  RandomEvents: "easter eggs fun jokes",
  RecordAudio: "microphone audio sound record",
  RecordFront: "driver camera inside cabin record",
  ReverseCruise: "cruise button increment set speed",
  ScreenBrightness: "dim bright display light dark night",
  ScreenBrightnessOnroad: "dim bright display light dark night",
  ScreenRecorder: "record screen capture video",
  ScreenTimeout: "screen off sleep display timeout",
  ScreenTimeoutOnroad: "screen off sleep display timeout",
  SNGHack: "stop and go traffic resume standstill",
  SpeedLimitController: "speed limit slc posted signs follow speed limits",
  SpeedLimitFiller: "speed limit learn missing wrong map signs camera fix correct",
  StandbyMode: "screen off display dark",
  StartupMessageTop: "welcome greeting boot",
  StoppedTimer: "red light timer stopped",
  SubaruSNG: "stop and go traffic resume standstill",
  TetheringEnabled: "hotspot tether wifi share internet",
  ToyotaDoors: "lock car auto lock doors unlock",
  TuningLevel: "advanced developer minimal standard more settings hidden settings unlock",
  UseKonikServer: "konik connect server upload",
  VoltSNG: "stop and go traffic resume standstill",
  WeatherPresets: "rain snow fog weather bad weather slippery",
  WeatherToken: "openweathermap weather key api"
};

const STOP_WORDS = new Set(["a", "an", "and", "are", "can", "do", "does", "for", "how", "i", "in", "is", "it", "me", "my", "of", "on", "or", "the", "to", "what", "where", "with"]);

const pages = Object.fromEntries(P.NAV.flatMap(group => group.items).map(item => [item.id, item]));
const documents = POND.map(pondDocument);
let levelNames = [];
let open = null;
let settingsLoad = null;

function normalize(text) {
  return text.toLowerCase().replace(/[^a-z0-9]+/g, " ").trim();
}

function stem(word) {
  if (word.length > 5 && word.endsWith("ing")) {
    return word.slice(0, -3);
  }
  if (word.length > 4 && word.endsWith("ies")) {
    return `${word.slice(0, -3)}y`;
  }
  if (word.length > 3 && word.endsWith("s") && !word.endsWith("ss")) {
    return word.slice(0, -1);
  }
  return word;
}

function terms(text) {
  return [...new Set(normalize(text).split(" ").filter(word => word && !STOP_WORDS.has(word)).map(stem))];
}

function indexed(doc, fields) {
  const index = {};
  for (const field of FIELDS) {
    index[field] = terms(fields[field] || "");
  }
  return { ...doc, index, phrases: { title: normalize(fields.title), words: normalize(fields.words || "") } };
}

function pondDocument(entry) {
  const page = pages[entry.page];
  return indexed({
    href: page.id === "home" ? "/" : `/${page.id}`,
    icon: page.icon,
    kind: "pond",
    page: page.label,
    settings: entry.settings || [],
    target: entry.target || "",
    title: entry.title || page.label
  }, { context: entry.title ? page.label : "", title: entry.title || page.label, words: entry.words });
}

function settingDocument(setting) {
  return indexed({
    description: setting.description,
    key: setting.key,
    kind: "setting",
    level: setting.level,
    path: setting.path,
    title: setting.name,
    toggle: setting.toggle
  }, { context: setting.path.join(" "), description: setting.description, title: setting.name, words: SETTING_WORDS[setting.key] });
}

function editDistance(a, b) {
  const rows = [];
  for (let i = 0; i <= a.length; i++) {
    rows.push([i]);
    for (let j = 1; j <= b.length; j++) {
      if (i === 0) {
        rows[i].push(j);
        continue;
      }
      const cost = a[i - 1] === b[j - 1] ? 0 : 1;
      let distance = Math.min(rows[i - 1][j] + 1, rows[i][j - 1] + 1, rows[i - 1][j - 1] + cost);
      if (i > 1 && j > 1 && a[i - 1] === b[j - 2] && a[i - 2] === b[j - 1]) {
        distance = Math.min(distance, rows[i - 2][j - 2] + 1);
      }
      rows[i].push(distance);
    }
  }
  return rows[a.length][b.length];
}

function termScore(term, word) {
  if (word === term) {
    return 1;
  }
  if (word.startsWith(term)) {
    return term.length > 1 ? 0.85 : 0.4;
  }
  if (term.length < 4) {
    return 0;
  }
  if (word.includes(term)) {
    return 0.55;
  }
  const allowed = term.length >= 8 ? 2 : 1;
  const distance = Math.min(editDistance(term, word), editDistance(term, word.slice(0, term.length)));
  if (distance > allowed) {
    return 0;
  }
  return distance === 1 ? 0.7 : 0.5;
}

function search(query) {
  const queryTerms = terms(query);
  if (!queryTerms.length) {
    return [];
  }

  const memo = queryTerms.map(() => new Map());
  const score = (index, word) => {
    if (!memo[index].has(word)) {
      memo[index].set(word, termScore(queryTerms[index], word));
    }
    return memo[index].get(word);
  };
  const phrase = normalize(query);

  const results = [];
  for (const doc of documents) {
    let matched = 0;
    let total = 0;
    for (let index = 0; index < queryTerms.length; index++) {
      const fieldScores = FIELDS.map(field => Math.max(0, ...doc.index[field].map(word => score(index, word) * WEIGHTS[field])));
      fieldScores.sort((a, b) => b - a);
      if (fieldScores[0] > 0) {
        matched += 1;
        total += fieldScores[0] + fieldScores[1] * SECOND_FIELD_SHARE;
      }
    }
    if (!matched) {
      continue;
    }

    const titleTerms = doc.index.title;
    const covered = titleTerms.filter(word => queryTerms.some((_, index) => score(index, word) > 0)).length;
    total += TITLE_COVERAGE_BONUS * covered / titleTerms.length;
    if (doc.phrases.title === phrase) {
      total += EXACT_TITLE_BONUS;
    }
    if (phrase.length >= 3 && doc.phrases.title.includes(phrase)) {
      total += TITLE_PHRASE_BONUS;
    }
    if (phrase.length >= 3 && doc.phrases.words.includes(phrase)) {
      total += WORDS_PHRASE_BONUS;
    }

    let final = total * (matched / queryTerms.length) ** 2;
    if (doc.kind === "pond") {
      final *= POND_BOOST;
    }
    if (hiddenLevel(doc)) {
      final *= HIDDEN_PENALTY;
    }
    if (final >= MIN_SCORE) {
      results.push({ doc, score: final });
    }
  }
  return results.sort((a, b) => b.score - a.score).slice(0, MAX_RESULTS).map(result => result.doc);
}

function hiddenLevel(doc) {
  return doc.level > P.state.tuning_level ? levelNames[doc.level] || "" : "";
}

function optionHTML(doc, index, place) {
  const attributes = `class="search-hit" role="option" id="search-${place}-${index}" data-index="${index}" tabindex="-1" aria-selected="false"`;
  if (doc.kind === "pond") {
    return `<a ${attributes} href="${doc.href}">${P.ic(doc.icon)}<b>${P.esc(doc.title)}</b><span>${doc.target ? P.esc(doc.page) : "Page"}</span></a>`;
  }
  const level = hiddenLevel(doc);
  return `<div ${attributes}>${P.ic("settings")}<b>${P.esc(doc.title)}${level ? ` <span class="badge">Needs ${P.esc(level)}</span>` : ""}</b><span>${P.esc(doc.path.join(" › "))}</span></div>`;
}

function setActive(index) {
  const options = open.box.querySelectorAll(".search-hit");
  options.forEach((option, i) => option.setAttribute("aria-selected", String(i === index)));
  open.active = index;
  const input = open.box.querySelector("[data-search]");
  if (index < 0) {
    input.removeAttribute("aria-activedescendant");
    return;
  }
  input.setAttribute("aria-activedescendant", options[index].id);
  options[index].scrollIntoView({ block: "nearest" });
}

function closeResults(box) {
  const list = box.querySelector(".search-results");
  list.hidden = true;
  list.innerHTML = "";
  box.querySelector("[data-search]").setAttribute("aria-expanded", "false");
  box.querySelector("[data-search]").removeAttribute("aria-activedescendant");
  const sheetNav = box.closest(".sheet-dlg")?.querySelector(".sheet-nav");
  if (sheetNav) {
    sheetNav.hidden = false;
  }
  if (open?.box === box) {
    open = null;
  }
}

function showResults(box) {
  const input = box.querySelector("[data-search]");
  const query = input.value.trim();
  if (!query) {
    closeResults(box);
    return;
  }

  const results = search(query);
  const list = box.querySelector(".search-results");
  const place = box.dataset.searchBox;
  list.innerHTML = results.length ? results.map((doc, index) => optionHTML(doc, index, place)).join("")
    : `<p class="search-empty">Nothing matches “${P.esc(query)}”. Try another word.</p>`;
  list.hidden = false;
  input.setAttribute("aria-expanded", "true");
  const sheetNav = box.closest(".sheet-dlg")?.querySelector(".sheet-nav");
  if (sheetNav) {
    sheetNav.hidden = true;
  }
  open = { active: -1, box, results };
  setActive(results.length ? 0 : -1);
}

function loadSettings() {
  if (settingsLoad) {
    return;
  }
  settingsLoad = P.get("/api/search").then(index => {
    levelNames = index.levels;
    documents.push(...index.settings.map(settingDocument));
    if (open) {
      showResults(open.box);
    }
  }).catch(error => {
    settingsLoad = null;
    P.fail(error, "Couldn't load the device settings for search");
  });
}

function reveal(selector) {
  const content = document.getElementById("content");
  const target = selector ? document.querySelector(".main").querySelector(selector) : null;
  if (!target) {
    content.focus({ preventScroll: true });
    return;
  }
  target.scrollIntoView({ behavior: P.reducedMotion.matches ? "auto" : "smooth", block: "center" });
  target.classList.add("search-flash");
  setTimeout(() => target.classList.remove("search-flash"), HIGHLIGHT_TIME);
  (target.matches(FOCUSABLE) ? target : content).focus({ preventScroll: true });
}

function clearSearch() {
  document.querySelectorAll("[data-search-box]").forEach(box => {
    box.querySelector("[data-search]").value = "";
    closeResults(box);
  });
  document.getElementById("more-sheet").close();
}

async function openPond(doc) {
  clearSearch();
  await P.go(doc.href);
  reveal(doc.target);
}

async function openSetting(doc) {
  if (doc.toggle) {
    clearSearch();
    P.go(`/toggles?key=${encodeURIComponent(doc.key)}`);
    return;
  }
  const link = documents.find(other => other.kind === "pond" && other.settings.includes(doc.key));
  const level = hiddenLevel(doc);
  const ok = await P.dialog({
    icon: "settings",
    title: P.esc(doc.title),
    body: `<p class="field-label">On the device</p>
      <p class="search-path">${P.esc(doc.path.join(" › "))}</p>
      ${doc.description ? `<p>${P.esc(doc.description)}</p>` : ""}
      ${level ? `<p class="muted">Hidden at your tuning level. Set Tuning Level, at the top of Settings › FrogPilot, to ${P.esc(level)} or higher to see it.</p>` : ""}`,
    confirm: link ? `Open ${P.esc(link.page)}` : "Done",
    cancel: link ? "Close" : ""
  });
  if (ok && link) {
    openPond(link);
  }
}

function choose(index) {
  const doc = open.results[index];
  if (doc.kind === "pond") {
    openPond(doc);
  } else {
    openSetting(doc);
  }
}

function searchBox(place) {
  return `<div class="search-box" data-search-box="${place}">
    ${place === "rail" ? `<button type="button" class="icon-btn search-open" data-act="rail-toggle" aria-label="Search">${P.ic("search")}</button>` : ""}
    <label class="search"><span class="sr">Search The Pond and the device's settings</span>${P.ic("search")}<input class="input" type="search" data-search role="combobox" aria-autocomplete="list" aria-expanded="false" aria-controls="search-results-${place}" placeholder="Search" autocomplete="off" spellcheck="false">${place === "rail" ? '<kbd class="search-key" aria-hidden="true">/</kbd>' : ""}</label>
    <div class="search-results" id="search-results-${place}" role="listbox" aria-label="Search results" hidden></div>
  </div>`;
}

document.addEventListener("input", e => {
  if (e.target.matches("[data-search]")) {
    showResults(e.target.closest("[data-search-box]"));
  }
});

document.addEventListener("focusin", e => {
  if (!e.target.matches("[data-search]")) {
    return;
  }
  loadSettings();
  if (e.target.value.trim()) {
    showResults(e.target.closest("[data-search-box]"));
  }
});

document.addEventListener("focusout", e => {
  const box = e.target.closest("[data-search-box]");
  if (box && !box.contains(e.relatedTarget)) {
    closeResults(box);
  }
});

document.addEventListener("keydown", e => {
  if (!e.target.matches("[data-search]")) {
    return;
  }
  const box = e.target.closest("[data-search-box]");
  if (e.key === "Escape" && e.target.value) {
    e.preventDefault();
    e.stopPropagation();
    e.target.value = "";
    closeResults(box);
  } else if (e.key === "ArrowDown" || e.key === "ArrowUp") {
    e.preventDefault();
    if (open?.box !== box) {
      showResults(box);
      return;
    }
    if (!open.results.length) {
      return;
    }
    const step = e.key === "ArrowDown" ? 1 : -1;
    setActive((open.active + step + open.results.length) % open.results.length);
  } else if (e.key === "Enter" && open?.box === box && open.active >= 0) {
    e.preventDefault();
    choose(open.active);
  }
}, true);

document.addEventListener("click", e => {
  const option = e.target.closest(".search-hit");
  if (option && open && !e.metaKey && !e.ctrlKey && !e.shiftKey && e.button === 0) {
    e.preventDefault();
    e.stopPropagation();
    choose(Number(option.dataset.index));
  }
}, true);

document.addEventListener("click", e => {
  if (e.target.closest(".search-open")) {
    document.querySelector("#rail [data-search]").focus();
  }
});

document.addEventListener("keydown", e => {
  if (e.key !== "/" || e.ctrlKey || e.metaKey || e.altKey || e.target.closest("input, textarea, select, [contenteditable]") || document.querySelector("dialog[open]")) {
    return;
  }
  const rail = document.getElementById("rail");
  if (getComputedStyle(rail).display === "none") {
    return;
  }
  e.preventDefault();
  const input = rail.querySelector("[data-search]");
  if (input.getClientRects().length) {
    input.focus();
  } else {
    rail.querySelector(".search-open").click();
  }
});

P.searchBox = searchBox;
})();
