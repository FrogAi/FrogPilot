(() => {
"use strict";

const BUSY_DONE_TIME = 1500;
const BUSY_FAILED_TIME = 2000;
const NB = "\u00a0";
const POLL_INTERVAL = 1000;
const RESTART_TIMEOUT = 300000;
const SKELETON_DELAY = 400;
const VIEW_CACHE_AGE = 600000;
const reducedMotion = matchMedia("(prefers-reduced-motion: reduce)");

const ICONS = {
  home: '<path d="M3.5 10.5 12 4l8.5 6.5V20a1 1 0 0 1-1 1h-5v-6h-5v6h-5a1 1 0 0 1-1-1z"/>',
  drives: '<path d="M8.5 3 4 21M15.5 3 20 21M12 4v2.5M12 10.5V13M12 17v3"/>',
  recordings: '<rect x="3" y="4.5" width="18" height="12.5" rx="2"/><path d="M8.5 21h7"/><circle cx="12" cy="10.75" r="2.6" class="f"/>',
  device: '<rect x="6.5" y="6.5" width="11" height="11" rx="1.5"/><path d="M10 3v3.5M14 3v3.5M10 17.5V21M14 17.5V21M3 10h3.5M3 14h3.5M17.5 10H21M17.5 14H21"/>',
  vehicle: '<path d="M4 16.5V12l2.1-4.6A2.2 2.2 0 0 1 8.1 6h7.8a2.2 2.2 0 0 1 2 1.4L20 12v4.5z"/><path d="M4 12h16M6.5 16.5v2a1 1 0 0 1-1 1h-.5a1 1 0 0 1-1-1v-2M19.5 16.5v2a1 1 0 0 1-1 1H18a1 1 0 0 1-1-1v-2M7.5 14.2h1.4M15.1 14.2h1.4"/>',
  software: '<path d="M12 3.5v11m0 0-4.2-4.2M12 14.5l4.2-4.2M4.5 16.5v2a2 2 0 0 0 2 2h11a2 2 0 0 0 2-2v-2"/>',
  theme: '<path d="M12 3a9 9 0 1 0 0 18c1.1 0 1.8-.8 1.8-1.7 0-.5-.2-.9-.5-1.2s-.5-.7-.5-1.2c0-.9.8-1.7 1.7-1.7H17a4 4 0 0 0 4-4C21 6.6 17 3 12 3z"/><circle cx="7.5" cy="11.5" r="1.2" class="f"/><circle cx="10" cy="7.3" r="1.2" class="f"/><circle cx="15" cy="7.3" r="1.2" class="f"/>',
  backups: '<path d="M3.8 12a8.2 8.2 0 1 0 2.4-5.8"/><path d="M3.5 4v4.3h4.3M12 7.8V12l2.9 1.9"/>',
  models: '<path d="M12 3.5 3.5 8 12 12.5 20.5 8z"/><path d="M3.5 12.2 12 16.7l8.5-4.5M3.5 16.4 12 20.9l8.5-4.5"/>',
  maps: '<path d="M9 4.5 3.5 6.8v12.7L9 17.2l6 2.3 5.5-2.3V4.5L15 6.8z"/><path d="M9 4.5v12.7M15 6.8v12.7"/>',
  keys: '<circle cx="8" cy="15.5" r="4.2"/><path d="m11 12.5 8.5-8.5M16.2 7.3l2.6 2.6M13.8 9.7l2 2"/>',
  startup: '<path d="M4 5h16v11H9.5L5 19.8V16H4z" /><path d="M8 9.3h8M8 12.3h5"/>',
  frogbackups: '<rect x="3.5" y="4" width="17" height="4.2" rx="1"/><path d="M5 8.2V19a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1V8.2M10 12h4"/>',
  tailscale: '<circle cx="6" cy="6" r="1.7" class="f"/><circle cx="12" cy="6" r="1.7" class="f"/><circle cx="18" cy="6" r="1.7" class="f"/><circle cx="6" cy="12" r="1.7" class="f"/><circle cx="12" cy="12" r="1.7" class="f"/><circle cx="18" cy="12" r="1.7" class="f"/><circle cx="12" cy="18" r="1.7" class="f"/><circle cx="6" cy="18" r="1.7" opacity=".35" class="f"/><circle cx="18" cy="18" r="1.7" opacity=".35" class="f"/>',
  logs: '<rect x="3" y="4.5" width="18" height="15" rx="2"/><path d="m7 9.5 3 2.5-3 2.5M12.5 15H17"/>',
  report: '<path d="M8.2 9.5a3.8 3.8 0 0 1 7.6 0v4.6a3.8 3.8 0 0 1-7.6 0z"/><path d="M12 13v5M4 13h4.2M15.8 13H20M5 8.2l3.2 2M19 8.2l-3.2 2M5 19l3.2-2M19 19l-3.2-2M9.7 5.8 8.3 4.3M14.3 5.8l1.4-1.5"/>',
  search: '<circle cx="11" cy="11" r="6.5"/><path d="m20 20-4.3-4.3"/>',
  bookmark: '<path d="M6.5 21V4.8c0-.4.4-.8.8-.8h9.4c.4 0 .8.4.8.8V21L12 16.8z"/>',
  lock: '<rect x="5.5" y="10.5" width="13" height="10" rx="2"/><path d="M8.5 10.5V7.8a3.5 3.5 0 0 1 7 0v2.7"/>',
  unlock: '<rect x="5.5" y="10.5" width="13" height="10" rx="2"/><path d="M8.5 10.5V7.8a3.5 3.5 0 0 1 6.8-1.2"/>',
  pencil: '<path d="M4 20h4.2L19.3 8.9a2.6 2.6 0 0 0-3.7-3.7L4.5 16.3z"/><path d="m13.8 7 3.6 3.6"/>',
  trash: '<path d="M4 7h16M9.5 7V4.5h5V7M6.5 7l.9 12.6a1 1 0 0 0 1 .9h7.2a1 1 0 0 0 1-.9L17.5 7M10 11v5.5M14 11v5.5"/>',
  download: '<path d="M12 4v11m0 0-4.2-4.2M12 15l4.2-4.2M5 20h14"/>',
  upload: '<path d="M12 16V5m0 0L7.8 9.2M12 5l4.2 4.2M5 20h14"/>',
  play: '<path d="M7.5 4.8v14.4a.6.6 0 0 0 .9.5l11.2-7.2a.6.6 0 0 0 0-1L8.4 4.3a.6.6 0 0 0-.9.5z" class="f"/>',
  pause: '<rect x="6.5" y="4.5" width="4" height="15" rx="1" class="f"/><rect x="13.5" y="4.5" width="4" height="15" rx="1" class="f"/>',
  back: '<path d="m14.5 5.5-6.5 6.5 6.5 6.5"/>',
  next: '<path d="m9.5 5.5 6.5 6.5-6.5 6.5"/>',
  down: '<path d="m6 9.5 6 6 6-6"/>',
  x: '<path d="M6.5 6.5 17.5 17.5M17.5 6.5 6.5 17.5"/>',
  check: '<path d="m4.5 12.5 4.8 4.8L19.5 7"/>',
  copy: '<rect x="8.5" y="8.5" width="11.5" height="11.5" rx="2"/><path d="M15.5 8.5V5.5a1.5 1.5 0 0 0-1.5-1.5H5.5A1.5 1.5 0 0 0 4 5.5V14a1.5 1.5 0 0 0 1.5 1.5h3"/>',
  send: '<path d="M20.5 3.5 3.5 10.6l6.9 3 3 6.9z"/><path d="m10.4 13.6 10.1-10.1"/>',
  sound: '<path d="M4 9.5h3.8L12.5 5.5v13l-4.7-4H4z"/><path d="M15.8 9.2a4 4 0 0 1 0 5.6M18.3 6.7a7.5 7.5 0 0 1 0 10.6"/>',
  power: '<path d="M12 3.5v8M6.6 6.6a7.6 7.6 0 1 0 10.8 0"/>',
  refresh: '<path d="M19.8 11a7.9 7.9 0 0 0-14.3-4.2L4 8.5M4 4v4.5h4.5M4.2 13a7.9 7.9 0 0 0 14.3 4.2L20 15.5M20 20v-4.5h-4.5"/>',
  menu: '<path d="M4 7h16M4 12h16M4 17h16"/>',
  more: '<rect x="4" y="4" width="6.5" height="6.5" rx="1.5"/><rect x="13.5" y="4" width="6.5" height="6.5" rx="1.5"/><rect x="4" y="13.5" width="6.5" height="6.5" rx="1.5"/><rect x="13.5" y="13.5" width="6.5" height="6.5" rx="1.5"/>',
  dots: '<circle cx="5.5" cy="12" r="1.6" class="f"/><circle cx="12" cy="12" r="1.6" class="f"/><circle cx="18.5" cy="12" r="1.6" class="f"/>',
  rew: '<path d="M11.5 6.5 6 12l5.5 5.5M18 6.5 12.5 12l5.5 5.5"/>',
  ffw: '<path d="M12.5 6.5 18 12l-5.5 5.5M6 6.5l5.5 5.5L6 17.5"/>',
  full: '<path d="M4 9V4h5M20 9V4h-5M4 15v5h5M20 15v5h-5"/>',
  info: '<circle cx="12" cy="12" r="8.5"/><path d="M12 11v5.2"/><circle cx="12" cy="7.9" r="1.1" class="f"/>',
  alert: '<path d="M10.4 4.2 2.9 17.4a1.8 1.8 0 0 0 1.6 2.7h15a1.8 1.8 0 0 0 1.6-2.7L13.6 4.2a1.8 1.8 0 0 0-3.2 0z"/><path d="M12 9.5v4.5"/><circle cx="12" cy="17" r="1.1" class="f"/>',
  plus: '<path d="M12 5v14M5 12h14"/>',
  minus: '<path d="M5 12h14"/>',
  file: '<path d="M14 3.5H7a1.5 1.5 0 0 0-1.5 1.5v14A1.5 1.5 0 0 0 7 20.5h10a1.5 1.5 0 0 0 1.5-1.5V8z"/><path d="M14 3.5V8h4.5M12 11.5v6M9.2 14.7 12 17.5l2.8-2.8"/>',
  wheel: '<circle cx="12" cy="12" r="8.5"/><circle cx="12" cy="12" r="2.2"/><path d="M3.6 11h6.2M14.2 11h6.2M12 14.2v6.2"/>',
  signal: '<path d="M14 5.5 20 11.5l-6 6M20 11.5H9.5A5.5 5.5 0 0 0 4 17v2"/>',
  link: '<path d="M14 4h6v6M20 4l-8.5 8.5M18 14v5a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V7a1 1 0 0 1 1-1h5"/>',
  flag: '<path d="M5.5 21V4M5.5 4.5h11.5l-2.2 4 2.2 4H5.5"/>',
  curve: '<path d="M6 20v-5.5A6.5 6.5 0 0 1 12.5 8H19M15.5 4.5 19 8l-3.5 3.5"/>',
  flask: '<path d="M9.5 3.5h5M10.5 3.5v5.8l-5.3 9.3a1.3 1.3 0 0 0 1.1 1.9h11.4a1.3 1.3 0 0 0 1.1-1.9l-5.3-9.3V3.5M7.4 15h9.2"/>',
  lane: '<path d="M5 20 7.5 4M19 20 16.5 4M12 19v-8m0 0-3 3m3-3 3 3"/>',
  mute: '<path d="M4 9.5h3.8L12.5 5.5v13l-4.7-4H4z"/><path d="m16 9.5 5 5M21 9.5l-5 5"/>',
  star: '<path d="m12 3.8 2.5 5.1 5.6.8-4 4 .9 5.6-5-2.7-5 2.7.9-5.6-4-4 5.6-.8z"/>',
  stop: '<path d="M8.4 3.5h7.2l4.9 4.9v7.2l-4.9 4.9H8.4l-4.9-4.9V8.4z"/>',
  theater: '<rect x="2.5" y="6" width="19" height="12" rx="2"/>',
  settings: '<path d="M4 7.5h9M17 7.5h3M4 16.5h3M11 16.5h9"/><circle cx="15" cy="7.5" r="2"/><circle cx="9" cy="16.5" r="2"/>',
  brake: '<circle cx="12" cy="12" r="6"/><path d="M12 9v3.5"/><circle cx="12" cy="15" r="0.9" class="f"/><path d="M4.5 7.5a8.5 8.5 0 0 0 0 9M19.5 7.5a8.5 8.5 0 0 1 0 9"/>',
  gauge: '<path d="M3.8 16.5a8.5 8.5 0 1 1 16.4 0"/><path d="m12 13.5 4.2-4.7"/><circle cx="12" cy="13.5" r="1.3" class="f"/>',
  person: '<circle cx="12" cy="8" r="3.5"/><path d="M5 20a7 7 0 0 1 14 0"/>',
  signpost: '<path d="M12 21V3M12 5h6.5l2 2.5-2 2.5H12M12 12H5.5l-2 2.5 2 2.5H12"/>'
};

function ic(name, cls = "") {
  return `<svg class="ic ${cls}" viewBox="0 0 24 24" aria-hidden="true">${ICONS[name] || ""}</svg>`;
}

function esc(value) {
  return String(value ?? "").replace(/[&<>"']/g, ch => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[ch]);
}

function now() {
  return Date.now() / 1000;
}

function fmt(t, opts) {
  return new Intl.DateTimeFormat("en-US", { hour12: true, ...opts }).format(new Date(t * 1000));
}

function dayIndex(t) {
  const date = new Date(t * 1000);
  return Math.round(Date.UTC(date.getFullYear(), date.getMonth(), date.getDate()) / 86400000);
}

function dayLabel(t) {
  const diff = dayIndex(now()) - dayIndex(t);
  if (diff === 0) {
    return "Today";
  }
  if (diff === 1) {
    return "Yesterday";
  }
  if (diff < 7) {
    return fmt(t, { weekday: "long" });
  }
  return fmt(t, { weekday: "short", month: "short", day: "numeric" });
}

function timeLabel(t) {
  return fmt(t, { hour: "numeric", minute: "2-digit" });
}

function whenLabel(t) {
  const diff = dayIndex(now()) - dayIndex(t);
  if (diff <= 1) {
    return `${dayLabel(t)}, ${timeLabel(t)}`;
  }
  return `${fmt(t, { month: "short", day: "numeric" })}, ${timeLabel(t)}`;
}

function duration(seconds) {
  const total = Math.round(seconds / 60);
  const h = Math.floor(total / 60);
  const m = total % 60;
  if (h === 0) {
    return `${m}${NB}min`;
  }
  return m ? `${h}${NB}h ${m}${NB}min` : `${h}${NB}h`;
}

function clock(seconds) {
  const s = Math.max(0, Math.floor(seconds));
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  const sec = String(s % 60).padStart(2, "0");
  return h ? `${h}:${String(m).padStart(2, "0")}:${sec}` : `${m}:${sec}`;
}

function size(bytes) {
  if (bytes >= 1e9) {
    return `${(bytes / 1e9).toFixed(1)}${NB}GB`;
  }
  if (bytes >= 1e6) {
    return `${Math.round(bytes / 1e6)}${NB}MB`;
  }
  return `${Math.max(1, Math.round(bytes / 1e3))}${NB}KB`;
}

function distance(meters) {
  if (state.is_metric) {
    return `${(meters / 1000).toFixed(1)}${NB}km`;
  }
  return `${(meters / 1609.344).toFixed(1)}${NB}mi`;
}

function speed(metersPerSecond) {
  if (state.is_metric) {
    return `${Math.round(metersPerSecond * 3.6)}${NB}km/h`;
  }
  return `${Math.round(metersPerSecond * 2.23694)}${NB}mph`;
}

function plural(n, word, many = word + "s") {
  return `${n.toLocaleString("en-US")} ${n === 1 ? word : many}`;
}

const PIN_WARNINGS_KEY = "thePondPinWarnings";
const state = { is_metric: false, language: "", locked: false, model: "", paired: false, pairing: false, tuning_level: 0 };
const shell = { device: null, software: null, vehicle: null };
let shellPromise = null;

const ERRORS = {
  all_downloaded: "Everything is already downloaded",
  busy: "The device is busy with another job. Try again when it finishes",
  car_on: "Turn the car off first",
  connection_lost: "The Pond couldn't reach the device",
  in_use: "It's in use right now",
  name_in_use: "That name is already taken",
  name_taken: "That name is already taken",
  no_update: "There's no update ready to install",
  not_found: "It isn't on the device anymore",
  offline: "The device isn't connected to the internet",
  openpilot_steering: "openpilot is steering. Park the car first",
  pending: "The last request is still in progress",
  recording: "It's still recording",
  updater_busy: "The updater is busy. Try again in a moment"
};

class PondError extends Error {
  constructor(status, code) {
    super(ERRORS[code] || code || `Error ${status}`);
    this.code = code;
    this.handled = status === 401 || status === 423;
    this.status = status;
  }
}

async function send(path, init) {
  let response;
  try {
    response = await fetch(path, init);
  } catch (error) {
    if (init.signal?.aborted) {
      throw error;
    }
    checkState();
    throw new PondError(0, "connection_lost");
  }
  if (!response.ok) {
    if (response.status === 401 || response.status === 423) {
      checkState();
    }
    throw new PondError(response.status, (await response.text()).trim());
  }
  return response;
}

async function request(method, path, body) {
  if (method !== "GET") {
    viewCache.clear();
  }
  const init = { method, headers: {} };
  if (body instanceof FormData) {
    init.body = body;
  } else if (body !== undefined) {
    init.headers["Content-Type"] = "application/json";
    init.body = JSON.stringify(body);
  }
  const response = await send(path, init);
  if (response.status === 202 || response.status === 204) {
    return null;
  }
  if ((response.headers.get("Content-Type") || "").includes("application/json")) {
    return response.json();
  }
  return response.text();
}

function get(path) {
  return request("GET", path);
}

function fail(error, title = "That didn't work", messages = {}) {
  if (!(error instanceof PondError)) {
    throw error;
  }
  if (error.handled) {
    return;
  }
  return toast(title, esc(messages[error.code] || error.message), "alert");
}

function poll(check, interval = POLL_INTERVAL) {
  return new Promise((resolve, reject) => {
    async function step() {
      try {
        const result = await check();
        if (result !== undefined) {
          resolve(result);
          return;
        }
      } catch (error) {
        if (!(error instanceof PondError) || error.code !== "connection_lost") {
          reject(error);
          return;
        }
      }
      setTimeout(step, interval);
    }
    step();
  });
}

const percentFrames = new WeakMap();

function showPercent(node, html) {
  cancelAnimationFrame(percentFrames.get(node));
  const match = /(\d+)%/.exec(html);
  const target = match ? Number(match[1]) : null;
  const from = node.dataset.percent ? Number(node.dataset.percent) : null;
  node.dataset.percent = target ?? "";

  if (target === null || from === null || target <= from || reducedMotion.matches) {
    node.innerHTML = html;
    return;
  }

  const start = performance.now();
  const step = now => {
    const shown = Math.round(from + (target - from) * Math.min(1, (now - start) / POLL_INTERVAL));
    node.innerHTML = html.replace(match[0], `${shown}%`);
    if (shown < target) {
      percentFrames.set(node, requestAnimationFrame(step));
    }
  };
  step(start);
}

function saveBlob(blob, name) {
  const link = document.createElement("a");
  link.href = URL.createObjectURL(blob);
  link.download = name;
  document.body.append(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(link.href), 1000);
}

function saveLink(url) {
  const link = document.createElement("a");
  link.href = url;
  link.download = "";
  document.body.append(link);
  link.click();
  link.remove();
}

async function download(url, name) {
  const controller = new AbortController();
  const handle = job({ label: `Downloading ${esc(name)}`, progress: 0, cancel: () => controller.abort() });
  try {
    const response = await send(url, { signal: controller.signal });
    const total = Number(response.headers.get("Content-Length")) || 0;
    const reader = response.body.getReader();
    const chunks = [];
    let received = 0;
    for (;;) {
      const { done, value } = await reader.read();
      if (done) {
        break;
      }
      chunks.push(value);
      received += value.length;
      handle.update(total ? `${size(received)} of ${size(total)}` : size(received), total ? received / total : null);
    }
    const disposition = response.headers.get("Content-Disposition") || "";
    saveBlob(new Blob(chunks, { type: response.headers.get("Content-Type") || "" }), disposition.match(/filename="([^"]+)"/)?.[1] || name);
    handle.done(`${esc(name)} saved to your downloads`);
  } catch (error) {
    if (controller.signal.aborted) {
      handle.done(`Download of ${esc(name)} canceled`);
      return;
    }
    handle.fail(error, `Couldn't download ${esc(name)}`);
  }
}

const NAV = [
  { group: null, items: [{ id: "home", label: "Home", icon: "home" }] },
  { group: "Drives", items: [
    { id: "drives", label: "Drives", icon: "drives" },
    { id: "recordings", label: "Screen Recordings", icon: "recordings" }
  ] },
  { group: "Car", items: [
    { id: "device", label: "Device and Power", icon: "device" },
    { id: "software", label: "Software Updates", icon: "software", pip: () => hasUpdate() },
    { id: "vehicle", label: "Vehicle and Doors", icon: "vehicle" }
  ] },
  { group: "Settings", items: [
    { id: "models", label: "Driving Models", icon: "models" },
    { id: "keys", label: "Keys", icon: "keys" },
    { id: "maps", label: "Speed Limit Maps", icon: "maps" },
    { id: "startup", label: "Startup Alert", icon: "startup" },
    { id: "theme", label: "Theme Maker", icon: "pencil" },
    { id: "themes", label: "Themes", icon: "theme" },
    { id: "toggles", label: "Toggles", icon: "settings" }
  ] },
  { group: "Tools", items: [
    { id: "backups", label: "Backups", icon: "backups" },
    { id: "logs", label: "Error Logs and Console", icon: "logs" },
    { id: "report", label: "Report an Issue", icon: "report" },
    { id: "tailscale", label: "Tailscale", icon: "tailscale" }
  ] }
];
const TABS = [
  { id: "home", label: "Home", icon: "home" },
  { id: "device", label: "Car", icon: "vehicle", also: ["vehicle"] },
  { id: "drives", label: "Drives", icon: "drives", also: ["drive"] },
  { id: "software", label: "Updates", icon: "software" }
];
const NETWORK_TYPES = { cell2G: "2G", cell3G: "3G", cell4G: "LTE", cell5G: "5G", ethernet: "Ethernet", none: "Offline", wifi: "Wi-Fi" };

const PAGES = {};
const REDIRECTS = { frogbackups: "backups" };
let currentView = "home";

function hasUpdate() {
  return Boolean(shell.software && (shell.software.update_available || shell.software.fetch_available));
}

function href(id) {
  return id === "home" ? "/" : `/${id}`;
}

function navActive(id) {
  return id === currentView || id === PAGES[currentView]?.parent;
}

function navLinks(sheet = false) {
  return NAV.map(group => `
    <div class="nav-group">
      ${group.group ? `<h2>${group.group}</h2>` : ""}
      ${group.items.map(item => {
        const pip = item.pip ? item.pip() : false;
        return `<a class="nav-link ${pip ? "has-pip" : ""}" href="${href(item.id)}" ${navActive(item.id) ? 'aria-current="page"' : ""}>
          ${ic(item.icon)}<span class="lbl">${item.label}</span>${!sheet ? `<span class="tip" aria-hidden="true">${item.label}</span>` : ""}
          ${pip ? `<span class="pip"><span class="sr">Update ready</span></span>` : ""}
        </a>`;
      }).join("")}
    </div>`).join("");
}

function renderRail() {
  const rail = document.getElementById("rail");
  if (!rail.firstElementChild) {
    rail.innerHTML = `
      <div class="rail-head">
        <a class="brand" href="/" aria-label="The Pond home">
          <img src="/assets/img/frogpilot-icon.svg" alt="" width="38" height="38">
          <span class="brand-word"><b>FrogPilot</b><span>The Pond</span></span>
        </a>
        <button class="icon-btn rail-expand" data-act="rail-toggle" aria-label="Show all page names">${ic("menu")}</button>
      </div>
      ${window.Pond.searchBox("rail")}
      <nav class="rail-nav" aria-label="Pages"></nav>
      <div class="rail-foot"></div>`;
  }
  const network = shell.device ? NETWORK_TYPES[shell.device.network.type] || shell.device.network.type : "";
  rail.querySelector(".rail-expand").setAttribute("aria-expanded", String(document.getElementById("app").classList.contains("rail-open")));
  rail.querySelector(".rail-nav").innerHTML = navLinks();
  rail.querySelector(".rail-foot").outerHTML = `<div class="rail-foot" aria-live="polite">
      <span class="chain"><span class="dot" title="Connected"></span><span class="txt">${esc(network)}</span></span>
    </div>`;
}

function renderTabbar() {
  const tabActive = t => t.id === currentView || (t.also || []).includes(currentView);
  const moreActive = !TABS.some(tabActive);
  document.getElementById("tabbar").innerHTML = TABS.map(t => {
    const pip = t.id === "software" && hasUpdate();
    return `<a href="${href(t.id)}" class="${pip ? "has-pip" : ""}" ${tabActive(t) ? 'aria-current="page"' : ""}>${ic(t.icon)}<span>${t.label}</span></a>`;
  }).join("") + `<button type="button" data-act="more" ${moreActive ? 'aria-current="page"' : ""} aria-haspopup="dialog">${ic("more")}<span>More</span></button>`;
}

function renderFooter() {
  const d = shell.device;
  document.getElementById("site-footer").innerHTML = `<p>FrogPilot ${esc(d.version)} · ${esc(d.branch)} · <span class="mono">${esc((d.commit || "").slice(0, 7))}</span></p><p>${esc(shell.vehicle.model)}</p>`;
}

function openMore() {
  const sheet = document.getElementById("more-sheet");
  sheet.innerHTML = `<div class="sheet-grab"></div>
    <div class="sheet-head"><h2 id="more-title">Everything in The Pond</h2><button class="icon-btn" data-act="close-more" aria-label="Close">${ic("x")}</button></div>
    ${window.Pond.searchBox("sheet")}
    <nav class="sheet-nav" aria-label="All pages">${navLinks(true)}</nav>`;
  sheet.showModal();
}

function loadShell() {
  shellPromise = Promise.all([get("/api/device"), get("/api/vehicle"), get("/api/software")]).then(([device, vehicle, software]) => {
    Object.assign(shell, { device, software, vehicle });
    renderFooter();
    renderRail();
    renderTabbar();
    return shell;
  });
  shellPromise.catch(error => fail(error, "Couldn't load the device details"));
  return shellPromise;
}

function setSoftware(software) {
  shell.software = software;
  renderRail();
  renderTabbar();
}

function widths(root) {
  root.querySelectorAll("[data-w]").forEach(el => {
    el.style.width = `${el.dataset.w}%`;
  });
}

let dlg = null;
let dlgResolve = null;

function openDialog({ icon = "info", tone = "", title, body = "", confirm = "OK", confirmTone = "primary", cancel = "Cancel", wide = false, requireCheck = "", onOpen }) {
  dlg.className = `dlg ${wide ? "wide" : ""}`;
  dlg.innerHTML = `<form method="dialog">
    <div class="dlg-body">
      <h2 id="dlg-title"><span class="ic-wrap ${tone}">${ic(icon)}</span>${title}</h2>
      ${body}
      ${requireCheck ? `<label class="check"><input type="checkbox" data-require> <span>${requireCheck}</span></label>` : ""}
    </div>
    <div class="dlg-foot">
      ${cancel ? `<button class="btn btn-quiet" value="cancel" type="button" data-dialog-cancel>${cancel}</button>` : ""}
      <button class="btn ${confirmTone === "danger" ? "btn-danger" : "btn-primary"}" value="ok" type="submit" data-confirm ${requireCheck ? "disabled" : ""}>${confirm}</button>
    </div>
  </form>`;
  dlg.setAttribute("aria-labelledby", "dlg-title");
  widths(dlg);
  const cancelButton = dlg.querySelector("[data-dialog-cancel]");
  if (cancelButton) {
    cancelButton.addEventListener("click", () => dlg.close("cancel"));
  }
  const check = dlg.querySelector("[data-require]");
  if (check) {
    check.addEventListener("change", () => {
      dlg.querySelector("[data-confirm]").disabled = !check.checked;
    });
  }
  return new Promise(resolve => {
    dlgResolve = resolve;
    dlg.returnValue = "";
    dlg.showModal();
    if (onOpen) {
      onOpen(dlg);
    }
    const focusTarget = cancel ? dlg.querySelector('[value="cancel"]') : dlg.querySelector("[data-confirm]");
    focusTarget.focus({ preventScroll: true });
  });
}

async function pinWarning(feature, text) {
  if (state.pairing) {
    return true;
  }

  let warned = [];
  try {
    warned = JSON.parse(localStorage.getItem(PIN_WARNINGS_KEY)) || [];
  } catch {
    warned = [];
  }

  if (warned.includes(feature)) {
    return true;
  }

  const risk = "Because “Pairing Code” is off, anyone on the same Wi-Fi or Tailscale network can do this too.";
  const advice = "Set a Pairing Code so only the browsers you pair can.";

  let settingUp = false;

  const ok = await openDialog({
    icon: "alert",
    tone: "danger",
    title: "No Pairing Code is set",
    body: `<p>${text}</p><p>${risk} ${advice}</p>`,
    cancel: "Set a Pairing Code",
    confirm: "Continue anyway",
    onOpen: dialog => {
      dialog.querySelector("[data-dialog-cancel]").addEventListener("click", () => {
        settingUp = true;
      });
    }
  });
  if (!ok) {
    if (settingUp) {
      go("/toggles?key=ThePondPairing");
    }

    return false;
  }

  try {
    localStorage.setItem(PIN_WARNINGS_KEY, JSON.stringify([...warned, feature]));
  } catch {
  }

  return true;
}

function lossList(lost = [], kept = []) {
  return `${lost.length ? `<div class="loses lost"><h3>What goes</h3><ul>${lost.map(t => `<li>${ic("x")}<span>${t}</span></li>`).join("")}</ul></div>` : ""}
    ${kept.length ? `<div class="loses kept"><h3>What stays</h3><ul>${kept.map(t => `<li>${ic("check")}<span>${t}</span></li>`).join("")}</ul></div>` : ""}`;
}

function toast(text, detail = "", icon = "check") {
  const el = document.createElement("div");
  el.className = "toast";
  el.innerHTML = `<div class="row">${ic(icon)}<div class="toast-text"><b>${text}</b>${detail ? `<span>${detail}</span>` : ""}</div><button type="button" aria-label="Dismiss">${ic("x", "ic-s")}</button></div>`;
  el.querySelector("button").onclick = () => el.remove();
  document.getElementById("toasts").append(el);
  if (icon !== "alert") {
    setTimeout(() => el.remove(), 5200);
  }
  return el;
}

function showResult(button, mark, disabled, icon, time) {
  mark.innerHTML = ic(icon);
  button.removeAttribute("aria-busy");
  setTimeout(() => {
    mark.remove();
    button.classList.remove("is-busy");
    button.style.removeProperty("--busy-color");
    button.disabled = disabled;
  }, time);
}

async function busy(button, promise) {
  const disabled = button.disabled;
  const mark = document.createElement("span");
  mark.className = "busy-mark";
  mark.setAttribute("aria-hidden", "true");
  mark.innerHTML = `<span class="spinner"></span>`;
  button.style.setProperty("--busy-color", getComputedStyle(button).color);
  button.classList.add("is-busy");
  button.setAttribute("aria-busy", "true");
  button.disabled = true;
  button.append(mark);
  let result;
  try {
    result = await promise;
  } catch (error) {
    showResult(button, mark, disabled, "alert", BUSY_FAILED_TIME);
    throw error;
  }
  showResult(button, mark, disabled, "check", BUSY_DONE_TIME);
  return result;
}

function skeleton(rows = 3) {
  return `<div class="sk-rows" aria-hidden="true">${'<div class="sk-row"><span class="sk sk-line"></span><span class="sk sk-line sk-short"></span></div>'.repeat(rows)}</div>`;
}

function skeletonPanel(rows) {
  return `<div class="sk-panel"><span class="sk sk-head" aria-hidden="true"></span>${skeleton(rows)}</div>`;
}

const jobs = new Set();
let baseTitle = document.title;

function updateTabTitle() {
  const measured = [...jobs].filter(j => j.progress !== null);
  if (!measured.length) {
    document.title = baseTitle;
    return;
  }
  const pct = Math.round(measured.reduce((sum, j) => sum + j.progress, 0) / measured.length * 100);
  document.title = `(${pct}%) ${baseTitle}`;
}

function job({ label, detail = "", progress = null, cancel = null }) {
  const el = document.createElement("div");
  el.className = "toast";
  el.setAttribute("role", "status");
  el.innerHTML = `<div class="row"><span class="spinner" aria-hidden="true"></span><b>${label}</b><span data-detail>${detail}</span>${cancel ? `<button type="button" data-cancel>Cancel</button>` : ""}</div><div class="progress" ${progress === null ? "hidden" : ""}><i></i></div>`;
  document.getElementById("toasts").append(el);
  const bar = el.querySelector(".progress");
  const handle = {
    progress,
    label(text) {
      el.querySelector(".row > b").innerHTML = text;
    },
    update(text, value = null) {
      showPercent(el.querySelector("[data-detail]"), text);
      handle.progress = value;
      bar.hidden = value === null;
      bar.firstChild.style.width = `${(value || 0) * 100}%`;
      updateTabTitle();
    },
    finish() {
      jobs.delete(handle);
      el.remove();
      updateTabTitle();
    },
    done(text, more = "") {
      handle.finish();
      if (text) {
        toast(text, more);
      }
    },
    fail(error, title, messages) {
      handle.finish();
      return fail(error, title, messages);
    }
  };
  bar.firstChild.style.width = `${(progress || 0) * 100}%`;
  if (cancel) {
    el.querySelector("[data-cancel]").onclick = event => {
      event.currentTarget.disabled = true;
      cancel();
    };
  }
  jobs.add(handle);
  updateTabTitle();
  return handle;
}

let dropdownCount = 0;

function dropdown({ name = "", value, options, label, attrs = "" }) {
  const current = options.find(([v]) => v === value) || options[0];
  const id = `dd-${++dropdownCount}`;
  return `<div class="menu-wrap dd" data-dd data-value="${esc(current[0])}" ${attrs}>
    <button type="button" class="select dd-btn" data-act="dd" role="combobox" aria-controls="${id}" aria-haspopup="listbox" aria-expanded="false" aria-label="${esc(label)}">${esc(current[1])}</button>
    <div class="menu dd-menu" id="${id}" role="listbox" aria-label="${esc(label)}" hidden>${options.map(([v, l]) => `<button type="button" class="menu-item" role="option" data-act="dd-pick" data-value="${esc(v)}" aria-selected="${v === current[0]}">${ic("check")}<span>${esc(l)}</span></button>`).join("")}</div>
    ${name ? `<input type="hidden" name="${esc(name)}" value="${esc(current[0])}">` : ""}
  </div>`;
}

function setDropdown(wrap, value) {
  wrap.dataset.value = value;
  wrap.querySelectorAll("[role='option']").forEach(option => {
    const selected = option.dataset.value === value;
    option.setAttribute("aria-selected", String(selected));
    if (selected) {
      wrap.querySelector(".dd-btn").textContent = option.textContent;
    }
  });
  const input = wrap.querySelector("input[type='hidden']");
  if (input) {
    input.value = value;
  }
}

function closeMenus(refocus = false) {
  document.querySelectorAll(".menu:not([hidden])").forEach(menu => {
    menu.hidden = true;
    const btn = menu.parentElement.querySelector("[aria-expanded]");
    if (btn) {
      btn.setAttribute("aria-expanded", "false");
      if (refocus) {
        btn.focus();
      }
    }
  });
}

function openMenu(btn) {
  const menu = btn.parentElement.querySelector(".menu");
  const open = menu.hidden;
  closeMenus();
  menu.hidden = !open;
  btn.setAttribute("aria-expanded", String(open));
  if (open) {
    (menu.querySelector("[aria-selected='true']") || menu.querySelector("button, a, input"))?.focus();
  }
}

const ACTIONS = {
  "rail-toggle": () => {
    const app = document.getElementById("app");
    const open = !app.classList.contains("rail-open");
    app.classList.toggle("rail-open", open);
    document.querySelector(".rail-expand")?.setAttribute("aria-expanded", String(open));
    document.querySelector(".rail-scrim")?.remove();
    if (open) {
      const scrim = document.createElement("div");
      scrim.className = "rail-scrim";
      scrim.onclick = () => ACTIONS["rail-toggle"]();
      app.append(scrim);
    }
  },
  more: () => openMore(),
  "close-more": () => document.getElementById("more-sheet").close(),
  menu: btn => openMenu(btn),
  dd: btn => openMenu(btn),
  "dd-pick": option => {
    const wrap = option.closest("[data-dd]");
    const changed = wrap.dataset.value !== option.dataset.value;
    setDropdown(wrap, option.dataset.value);
    closeMenus(true);
    if (changed) {
      wrap.dispatchEvent(new Event("change", { bubbles: true }));
    }
  },
  retry: btn => busy(btn, render(false)),
  reconnect: async btn => {
    try {
      await busy(btn, checkState().then(() => {
        if (overlay === "offline") {
          throw new PondError(0, "connection_lost");
        }
      }));
    } catch (error) {
      if (!(error instanceof PondError)) {
        throw error;
      }
    }
  }
};

function go(target, { replace = false } = {}) {
  const url = new URL(target, location.href);
  if (replace) {
    history.replaceState(null, "", url);
  } else {
    history.pushState(null, "", url);
  }
  return render(true);
}

function setParam(key, value) {
  const url = new URL(location.href);
  if (value === "" || value == null) {
    url.searchParams.delete(key);
  } else {
    url.searchParams.set(key, value);
  }
  history.replaceState(null, "", url);
}

function stripTags(s) {
  return String(s).replace(/<[^>]+>/g, "");
}

function renderTop(out) {
  const top = document.getElementById("topbar");
  top.className = `topbar ${window.scrollY > 4 ? "scrolled" : ""}`;
  top.innerHTML = `
    ${out.crumb ? `<nav class="crumb" aria-label="Back"><a href="${out.crumb.href}">${ic("back")}<span class="crumb-lbl">${out.crumb.label}</span></a></nav>` : `<a class="brand m-brand" href="/" aria-label="The Pond home"><img src="/assets/img/frogpilot-icon.svg" alt="" width="34" height="34"></a>`}
    <h1 class="page-title" id="page-title">${out.title}</h1>
    ${out.actions ? `<div class="top-actions">${out.actions}</div>` : ""}`;
  baseTitle = `${out.docTitle || stripTags(out.title)} · The Pond`;
  updateTabTitle();
}

const viewCache = new Map();
let viewCleanup = null;
let renderToken = 0;
let skeletonTimer = 0;

function viewFromPath() {
  let view = location.pathname.replace(/^\/+|\/+$/g, "") || "home";
  if (REDIRECTS[view]) {
    view = REDIRECTS[view];
    history.replaceState(null, "", href(view) + location.search);
  }
  if (PAGES[view]) {
    return view;
  }
  history.replaceState(null, "", "/");
  return "home";
}

async function render(resetScroll = true) {
  const view = viewFromPath();
  const page = PAGES[view];
  const sameView = view === currentView;
  currentView = view;
  if (viewCleanup) {
    viewCleanup();
    viewCleanup = null;
  }
  document.getElementById("app").classList.remove("rail-open");
  document.querySelector(".rail-scrim")?.remove();
  renderRail();
  renderTabbar();
  const content = document.getElementById("content");
  const cacheKey = location.pathname + location.search;
  const cached = viewCache.get(cacheKey);
  let fadeIn = false;
  clearTimeout(skeletonTimer);
  if (!sameView && cached && Date.now() - cached.time < VIEW_CACHE_AGE) {
    renderTop({ title: page.title, ...cached.out });
    document.getElementById("topbar").inert = true;
    content.innerHTML = cached.out.body;
    content.inert = true;
    widths(content);
  } else if (!sameView) {
    renderTop({ title: page.title });
    content.innerHTML = "";
    skeletonTimer = setTimeout(() => {
      content.innerHTML = `<div class="page-skeleton" role="status"><span class="sr">Loading</span>${skeletonPanel(4)}${skeletonPanel(3)}</div>`;
    }, SKELETON_DELAY);
    fadeIn = true;
  }
  if (resetScroll) {
    window.scrollTo(0, 0);
  }

  const token = ++renderToken;
  content.setAttribute("aria-busy", "true");
  let out;
  try {
    out = await page.render(new URLSearchParams(location.search));
    viewCache.set(cacheKey, { out, time: Date.now() });
  } catch (error) {
    if (!(error instanceof PondError)) {
      throw error;
    }
    out = {
      title: page.title,
      body: error.handled ? "" : `<div class="empty"><h3>This page didn't load</h3><p>${esc(error.message)}</p><button class="btn btn-quiet btn-small" data-act="retry">Try again</button></div>`
    };
  }
  if (token !== renderToken) {
    return;
  }
  clearTimeout(skeletonTimer);
  content.removeAttribute("aria-busy");
  content.inert = false;
  document.getElementById("topbar").inert = false;
  renderTop({ title: page.title, ...out });
  content.innerHTML = out.body;
  content.setAttribute("aria-labelledby", "page-title");
  widths(content);
  if (fadeIn && !reducedMotion.matches) {
    content.animate([{ opacity: 0 }, { opacity: 1 }], { duration: 150, easing: "ease-out" });
  }
  if (out.mount) {
    viewCleanup = out.mount(content) || null;
  }
}

let overlay = "";
let started = false;
let restartDone = null;
let restartTimer = 0;
let restarting = false;
let sawFailure = false;
let nextBeat = 0;

function stateBrand() {
  return `<div class="brand state-brand"><img src="/assets/img/frogpilot-icon.svg" alt="" width="38" height="38"><span class="brand-word"><b>FrogPilot</b><span>The Pond</span></span></div>`;
}

function showLocked(holder) {
  holder.innerHTML = `<div class="overlay full state-page" role="alertdialog" aria-modal="true" aria-labelledby="lk-h" aria-describedby="lk-p">
    ${stateBrand()}
    <div class="state-body">
      <img class="mascot" src="/assets/img/frogpilot-mascot.svg" alt="" width="168" height="168">
      <div class="state-words">
        <h1 id="lk-h">The car is driving</h1>
        <p class="standfirst" id="lk-p">The Pond is resting while you're on the road. It comes back on its own when you park.</p>
        <p class="state-status"><span class="state-line"><span class="dot"></span>Still connected to the ${esc(state.model || "device")}</span></p>
      </div>
    </div>
    </div>`;
}

const PAIR_ERRORS = {
  pairing_off: "Pairing is turned off on the car. Turn on “Pairing Code” in the car's Navigation settings to pair this browser.",
  too_many_attempts: "Too many wrong codes. Wait 10 minutes, then try again.",
  wrong_code: "That code doesn't match the one in the car's Navigation settings."
};

function showPairing(holder) {
  holder.innerHTML = `<div class="overlay full state-page" role="dialog" aria-modal="true" aria-labelledby="pr-h">
    ${stateBrand()}
    <div class="state-body">
      <form class="state-words state-form" data-pair>
        <h1 id="pr-h">Pair this browser</h1>
        <p class="standfirst">Enter the 6-digit code shown next to “Manage Your Settings At” in the car's Navigation settings.</p>
        <div class="code-boxes" role="group" aria-label="Pairing code">${[0, 1, 2, 3, 4, 5].map(i => `${i === 3 ? '<span class="gap"></span>' : ""}<input inputmode="numeric" maxlength="1" pattern="[0-9]" aria-label="Digit ${i + 1}" autocomplete="one-time-code">`).join("")}</div>
        <p class="muted pair-msg" data-pair-msg aria-live="polite">Turning pairing off and back on makes a new one.</p>
        <button class="btn btn-primary" type="submit">Pair</button>
      </form>
    </div>
    </div>`;
  const boxes = [...holder.querySelectorAll(".code-boxes input")];
  boxes[0].focus();
  boxes.forEach((b, i) => {
    b.addEventListener("input", () => {
      b.value = b.value.replace(/\D/g, "").slice(0, 1);
      if (b.value && boxes[i + 1]) {
        boxes[i + 1].focus();
      }
    });
    b.addEventListener("keydown", e => {
      if (e.key === "Backspace" && !b.value && boxes[i - 1]) {
        boxes[i - 1].focus();
      }
    });
    b.addEventListener("paste", e => {
      const digits = (e.clipboardData.getData("text").match(/\d/g) || []).slice(0, 6);
      if (!digits.length) {
        return;
      }
      e.preventDefault();
      digits.forEach((d, j) => {
        if (boxes[j]) {
          boxes[j].value = d;
        }
      });
      boxes[Math.min(5, digits.length)].focus();
    });
  });
  holder.querySelector("[data-pair]").addEventListener("submit", async e => {
    e.preventDefault();
    const code = boxes.map(b => b.value).join("");
    const msg = holder.querySelector("[data-pair-msg]");
    if (code.length < 6) {
      msg.textContent = "Enter all 6 digits.";
      return;
    }
    try {
      await busy(e.target.querySelector("button[type='submit']"), request("POST", "/api/pair", { code }));
    } catch (error) {
      if (!(error instanceof PondError)) {
        throw error;
      }
      msg.textContent = PAIR_ERRORS[error.code] || error.message;
      return;
    }
    msg.textContent = "Paired. Welcome to The Pond.";
    checkState();
  });
}

function offlineCount() {
  return `, next try in ${Math.max(1, Math.ceil((nextBeat - Date.now()) / 1000))}${NB}s`;
}

function showOffline(holder, label, detail) {
  holder.innerHTML = `<div class="overlay dim" role="alertdialog" aria-modal="true" aria-labelledby="off-h">
    <div class="state-card panel">
      <span class="spinner offline-spinner" aria-hidden="true"></span>
      <h1 id="off-h" class="offline-title">${label || "Connection lost"}</h1>
      ${detail ? `<p>${detail}</p>` : `<p>Reconnecting to the ${esc(state.model || "device")}<span data-count>${offlineCount()}</span>. Make sure your phone is on the same Wi-Fi as the car, or on Tailscale.</p>
      <button class="btn btn-quiet btn-small" data-act="reconnect">Try now</button>`}
    </div></div>`;
}

function setOverlay(kind, label = "", detail = "") {
  if (kind === overlay && (kind !== "" || started) && !label) {
    return;
  }
  const blocked = overlay !== "" || !started;
  const holder = document.getElementById("overlays");
  const app = document.getElementById("app");
  overlay = kind;
  if (!kind) {
    holder.innerHTML = "";
    app.inert = false;
    if (blocked) {
      started = true;
      loadShell();
      render(false);
    }
    if (restartDone) {
      restartDone();
      restartDone = null;
    }
    return;
  }
  app.inert = true;
  if (kind === "locked") {
    showLocked(holder);
  } else if (kind === "pairing") {
    showPairing(holder);
  } else {
    showOffline(holder, label, detail);
  }
}

async function checkState() {
  let next = "offline";
  try {
    const response = await fetch("/api/state");
    if (response.ok) {
      Object.assign(state, await response.json());
      next = state.locked ? "locked" : state.pairing && !state.paired ? "pairing" : "";
    }
  } catch {
    next = "offline";
  }
  if (restarting) {
    if (next === "offline") {
      sawFailure = true;
    }
    if (!sawFailure || next === "offline") {
      return;
    }
    restarting = false;
  }
  setOverlay(next);
}

function restart(label, detail = "") {
  restarting = true;
  sawFailure = false;
  clearTimeout(restartTimer);
  if (detail) {
    setOverlay("restart", label, detail);
    restartTimer = setTimeout(() => {
      if (!restarting) {
        return;
      }
      restarting = false;
      checkState();
    }, RESTART_TIMEOUT);
  } else {
    setOverlay("offline", label);
  }
  return new Promise(resolve => {
    restartDone = resolve;
  });
}

async function beat() {
  if (restarting || Date.now() >= nextBeat) {
    if (!document.hidden) {
      await checkState();
    }
    nextBeat = Date.now() + (restarting ? POLL_INTERVAL : 5000);
  }
  setTimeout(beat, POLL_INTERVAL);
}

setInterval(() => {
  const count = document.querySelector("#overlays [data-count]");
  if (count) {
    count.textContent = offlineCount();
  }
}, 1000);

window.addEventListener("popstate", () => render());
window.addEventListener("scroll", () => {
  document.getElementById("topbar").classList.toggle("scrolled", window.scrollY > 4);
}, { passive: true });
document.addEventListener("visibilitychange", () => {
  if (!document.hidden) {
    checkState();
  }
});

document.addEventListener("click", e => {
  const link = e.target.closest("a[href]");
  if (link && !link.hasAttribute("download") && !link.target && !e.metaKey && !e.ctrlKey && !e.shiftKey && e.button === 0) {
    const url = new URL(link.href);
    if (url.origin === location.origin && !url.pathname.startsWith("/api/") && !url.pathname.startsWith("/assets/")) {
      e.preventDefault();
      document.getElementById("more-sheet").close();
      go(url.pathname + url.search);
      document.getElementById("content").focus({ preventScroll: true });
      return;
    }
  }
  const act = e.target.closest("[data-act]");
  if (act && ACTIONS[act.dataset.act]) {
    ACTIONS[act.dataset.act](act, e);
  }
  if (!e.target.closest(".menu-wrap") || e.target.closest("button.menu-item")) {
    closeMenus();
  }
});

document.addEventListener("keydown", e => {
  if (e.key === "Escape") {
    if (document.querySelector(".menu:not([hidden])")) {
      e.preventDefault();
    }
    closeMenus(true);
    if (document.getElementById("app").classList.contains("rail-open")) {
      ACTIONS["rail-toggle"]();
    }
    return;
  }
  const wrap = e.target.closest("[data-dd]");
  if (!wrap) {
    return;
  }
  const menu = wrap.querySelector(".dd-menu");
  const options = [...menu.querySelectorAll("[role='option']")];
  if (e.target.classList.contains("dd-btn") && ["ArrowDown", "ArrowUp"].includes(e.key)) {
    e.preventDefault();
    if (menu.hidden) {
      openMenu(e.target);
    }
    return;
  }
  const index = options.indexOf(e.target);
  if (index < 0) {
    return;
  }
  const moves = { ArrowDown: index + 1, ArrowUp: index - 1, End: options.length - 1, Home: 0 };
  if (e.key in moves) {
    e.preventDefault();
    options[Math.max(0, Math.min(options.length - 1, moves[e.key]))].focus();
  } else if (e.key === "Tab") {
    closeMenus();
  }
});

window.Pond = {
  NAV,
  NB,
  NETWORK_TYPES,
  PondError,
  act: (name, fn) => {
    ACTIONS[name] = fn;
  },
  busy,
  clock,
  dayIndex,
  dayLabel,
  dialog: openDialog,
  distance,
  download,
  dropdown,
  duration,
  esc,
  fail,
  fmt,
  get,
  go,
  ic,
  job,
  lossList,
  now,
  page: (id, page) => {
    PAGES[id] = page;
  },
  params: () => new URLSearchParams(location.search),
  pinWarning,
  plural,
  poll,
  reducedMotion,
  refresh: () => render(false),
  request,
  restart,
  saveLink,
  setDropdown,
  setParam,
  setSoftware,
  shell: () => shellPromise,
  showPercent,
  size,
  speed,
  state,
  timeLabel,
  toast,
  whenLabel,
  widths
};

window.addEventListener("DOMContentLoaded", () => {
  dlg = document.getElementById("dlg");
  dlg.addEventListener("close", () => {
    if (dlgResolve) {
      const ok = dlg.returnValue === "ok";
      const resolve = dlgResolve;
      dlgResolve = null;
      resolve(ok ? dlg : null);
    }
  });
  document.getElementById("more-sheet").addEventListener("click", e => {
    if (e.target === e.currentTarget) {
      e.currentTarget.close();
    }
  });
  currentView = viewFromPath();
  renderTop({ title: PAGES[currentView].title });
  renderRail();
  renderTabbar();
  beat();
});
})();
