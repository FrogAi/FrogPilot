(() => {
"use strict";

const P = window.Pond;
const { esc, ic } = P;

const ALPHA_USED = ["LaneLines", "Sidebar1", "Sidebar2", "Sidebar3"];
const BORDER_ENGAGED = "#178644";
const COLOR_KEYS = ["LaneLines", "LeadMarker", "Path", "PathEdge", "Sidebar1", "Sidebar2", "Sidebar3"];
const COLOR_LABELS = { LaneLines: "Lane lines", LeadMarker: "Lead marker", Path: "Path", PathEdge: "Path edges", Sidebar1: "Temperature bar", Sidebar2: "Vehicle bar", Sidebar3: "Connection bar" };
const COMPONENTS = ["colors", "distance_icons", "icons", "signals", "sounds", "steering_wheels"];
const EXPERIMENTAL = "#DA6F25";
const IMAGE_TYPES = ".png,.svg,.gif,image/png,image/svg+xml,image/gif";
const IMG = { src: "/assets/img/theme_maker/road.jpg", w: 482, h: 302, vpx: 240.5, vpy: 123.5, k: 469, camH: 1.4 };
const IMG_DIR = "/assets/img/theme_maker/";
const LEAD_D = 18;
const MB = 1024 * 1024;
const PART_COMPONENT = { distance: "distance_icons", icons: "icons", signals: "signals", sounds: "sounds", wheel: "steering_wheels" };
const PERSONALITIES = ["aggressive", "standard", "relaxed", "traffic"];
const PRESETS = ["#178644", "#2E9D57", "#126B36", "#17823B", "#C21111", "#3685C0", "#862FE0", "#A138FF", "#F27503", "#E8B11C", "#30FF9C", "#FFFFFF"];
const SOUND_SLOTS = [
  { id: "engage", label: "Engage", when: "openpilot engages" },
  { id: "disengage", label: "Disengage", when: "You brake, cancel, or take over" },
  { id: "prompt", label: "Prompt", when: "Heads-ups like a blocked lane change or a green light" },
  { id: "prompt_distracted", label: "Distracted", when: "Eyes off the road, repeats" },
  { id: "refuse", label: "Refuse", when: "openpilot can't engage, like with a door open" },
  { id: "warning_soft", label: "Soft warning", when: "Take control soon, repeats" },
  { id: "warning_immediate", label: "Urgent warning", when: "Take control now, repeats" },
  { id: "startup", label: "Startup", when: "Holiday greeting, only while a holiday theme is on" }
];
const TIZI_SLOTS = ["disengage", "engage"];
const USER_SUFFIX = "-user_created";

const MESSAGES = {
  empty_theme: "This theme has nothing in it to submit yet",
  in_use: "The car is using this theme. Apply another one first",
  invalid_colors: "The device didn't accept those colors",
  invalid_file: "That isn't a theme file the device can read",
  invalid_image: "Use a PNG, SVG, GIF, or JPEG image",
  invalid_signals: "Turn signals need 1 to 30 image frames",
  invalid_sound: "The device couldn't read that sound file",
  invalid_source: "The device doesn't have that theme anymore",
  invalid_theme: "The device doesn't have that theme anymore",
  invalid_username: "Enter your Discord name",
  random_themes: "Random Themes is on. Turn it off on the car first",
  submission_failed: "The FrogPilot server didn't take the theme. Try again later"
};

const META = {
  path: { icon: "drives", title: "Path", where: "The ribbon the car plans to drive" },
  lanes: { icon: "drives", title: "Lane lines", where: "The lines the model sees on the road" },
  edges: { icon: "drives", title: "Path edges", where: "The rails along both sides of the path" },
  lead: { icon: "vehicle", title: "Lead marker", where: "The chevron under the car ahead" },
  sidebar: { icon: "menu", title: "Sidebar bars", where: "The color strip on each status box" },
  wheel: { icon: "wheel", title: "Steering wheel", where: "Top right, turns with the wheel" },
  signals: { icon: "signal", title: "Turn signals", where: "Hops across the screen while a blinker is on" },
  icons: { icon: "home", title: "Sidebar buttons", where: "Settings at the top, flag or home at the bottom" },
  distance: { icon: "vehicle", title: "Personality icons", where: "The driving personality button, bottom left" },
  sounds: { icon: "sound", title: "Sounds", where: "What the car plays, and when" }
};

const PART_COLORS = { edges: ["PathEdge"], lanes: ["LaneLines"], lead: ["LeadMarker"], path: ["Path"], sidebar: ["Sidebar1", "Sidebar2", "Sidebar3"] };

const T = {
  blind: false,
  button: "flag",
  current: "",
  drive: "engaged",
  made: new Set(),
  part: "path",
  personality: "standard",
  showPersonality: false,
  sidebar: false,
  signal: null,
  soundFocus: "engage"
};

let data = null;
let themes = [];
let userThemes = null;
let look = null;
const lists = new Map();
const loadedAt = Date.now();
const versions = new Map();

const enc = encodeURIComponent;
const cur = () => themes.find(t => t.key === T.current) || themes[0];

async function loadData() {
  [data, userThemes] = await Promise.all([P.get("/api/themes"), P.get("/api/user_themes")]);
  const limits = userThemes.limits;
  Object.assign(MESSAGES, {
    file_too_large: `That file is over ${limits.file_size / MB}${P.NB}MB`,
    invalid_name: `Use 1 to ${limits.name_length} letters, numbers, and spaces`,
    theme_too_large: `The theme is over ${limits.submission_size / MB}${P.NB}MB`
  });
  themes = buildThemes();
}

function nameFromKey(key) {
  return key.replace(USER_SUFFIX, "").split("_").filter(Boolean).map(word => word[0].toUpperCase() + word.slice(1).toLowerCase()).join(" ");
}

function keyFromName(name) {
  return name.trim().toLowerCase().split(/\s+/).join("_") + USER_SUFFIX;
}

function fallback(component) {
  return component === "signals" ? "none" : "stock";
}

function installed(component, key) {
  return data.components[component].installed.some(entry => entry.key === key);
}

function packName(component, key) {
  if (data.builtins[key]) {
    return data.builtins[key];
  }
  const entry = data.components[component].installed.find(item => item.key === key);
  return entry ? entry.name : key;
}

function buildThemes() {
  const user = new Map();
  const packs = new Map();
  for (const component of COMPONENTS) {
    for (const entry of data.components[component].installed) {
      if (entry.user_created) {
        user.set(entry.key, entry.name);
        continue;
      }
      const base = entry.key.replace(/-animated$/, "");
      const parts = packs.get(base) || {};
      if (!parts[component] || entry.key.endsWith("-animated")) {
        parts[component] = entry.key;
      }
      packs.set(base, parts);
    }
  }
  for (const key of [...data.user_themes, ...T.made]) {
    if (!user.has(key)) {
      user.set(key, nameFromKey(key));
    }
  }

  const parts = pick => Object.fromEntries(COMPONENTS.map(component => [component, pick(component)]));
  const list = [];
  for (const [key, name] of user) {
    list.push({ key, name, user: true, parts: parts(() => key) });
  }
  for (const [key, name] of Object.entries(data.builtins)) {
    if (key !== "none") {
      list.push({ key, name, user: false, parts: parts(component => data.components[component].excluded.includes(key) ? fallback(component) : key) });
    }
  }
  for (const [base, found] of packs) {
    if (found.colors && !data.builtins[base]) {
      list.push({ key: base, name: packName("colors", found.colors).replace(/ \(Animated\)$/, ""), user: false, parts: parts(component => found[component] || fallback(component)) });
    }
  }
  return list.sort((a, b) => a.name.localeCompare(b.name));
}

function usingName() {
  if (data.holiday_themes && data.holiday && data.holiday !== "stock") {
    const holiday = data.holiday.replace(/_week$/, "");
    return data.builtins[holiday] || nameFromKey(holiday);
  }
  if (!data.custom_themes) {
    return "Stock";
  }
  const selected = data.components.colors.selected;
  const match = themes.find(t => t.parts.colors === selected);
  return match ? match.name : packName("colors", selected);
}

async function fileList(component, key) {
  const id = `${component}/${key}`;
  if (!lists.has(id)) {
    try {
      lists.set(id, await P.get(`/api/theme_files/${component}/${enc(key)}`));
    } catch (error) {
      if (!(error instanceof P.PondError) || error.status !== 404) {
        throw error;
      }
      lists.set(id, []);
    }
  }
  return lists.get(id);
}

function listed(component, key) {
  return lists.get(`${component}/${key}`) || [];
}

function forget(key, component, version = Date.now()) {
  lists.delete(`${component}/${key}`);
  versions.set(`${component}/${key}`, version);
}

function fileUrl(component, key, name) {
  return `/api/theme_files/${component}/${enc(key)}/${enc(name)}${key.endsWith(USER_SUFFIX) ? `?v=${versions.get(`${component}/${key}`) || loadedAt}` : ""}`;
}

function pickSlot(list, slot) {
  return ["gif", "svg", "png"].map(extension => `${slot}.${extension}`).find(name => list.includes(name));
}

function slotUrl(component, key, slot) {
  const name = pickSlot(listed(component, key), slot);
  return name ? fileUrl(component, key, name) : null;
}

function signalInfo(key) {
  const list = listed("signals", key);
  const images = list.filter(name => /\.(gif|png|svg)$/i.test(name));
  const frames = images.filter(name => !/blindspot/i.test(name));
  const blind = images.find(name => /blindspot/i.test(name));
  const marker = list.filter(name => !images.includes(name)).map(name => name.split("_")).find(parts => parts.length === 2 && /^\d+$/.test(parts[1]) && Number(parts[1]) > 0);
  return {
    blind: blind ? fileUrl("signals", key, blind) : null,
    blindName: blind || null,
    files: frames,
    frames: frames.map(name => fileUrl("signals", key, name)),
    ms: marker ? Number(marker[1]) : 125,
    style: marker ? marker[0] : null
  };
}

function gifFrameCount(bytes) {
  const table = flags => (flags & 0x80 ? 3 << ((flags & 0x07) + 1) : 0);
  const skip = at => {
    while (bytes[at] !== 0) {
      at += bytes[at] + 1;
    }
    return at + 1;
  };
  let at = 13 + table(bytes[10]);
  let count = 0;
  while (at < bytes.length && bytes[at] !== 0x3B) {
    if (bytes[at] === 0x2C) {
      count += 1;
      at = skip(at + 11 + table(bytes[at + 9]));
    } else {
      at = skip(at + 2);
    }
  }
  return count;
}

function toColor(value) {
  const part = name => (value[name] ?? 255).toString(16).padStart(2, "0");
  return { a: value.alpha ?? 255, hex: `#${part("red")}${part("green")}${part("blue")}`.toUpperCase() };
}

function rgb(hex) {
  const n = parseInt(hex.slice(1), 16);
  return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
}

function rgba(hex, a) {
  const [r, g, b] = rgb(hex);
  return `rgba(${r},${g},${b},${a})`;
}

function colorCss(c) {
  return rgba(c.hex, c.a / 255);
}

function effective(theme, component) {
  if (!theme.user || installed(component, theme.key)) {
    return theme.parts[component];
  }
  if (component === "steering_wheels" && userThemes.no_wheel.includes(theme.key)) {
    return "none";
  }
  return fallback(component);
}

function own(component) {
  const theme = cur();
  return theme.user && installed(component, theme.key);
}

async function loadLook(theme) {
  const keys = Object.fromEntries(COMPONENTS.map(component => [component, effective(theme, component)]));
  await Promise.all([...COMPONENTS.map(component => fileList(component, keys[component])), fileList("colors", "stock"), fileList("icons", "stock")]);

  const stockColors = await P.get(fileUrl("colors", "stock", "colors.json"));
  const themed = keys.colors !== "stock" && listed("colors", keys.colors).includes("colors.json");
  const json = themed ? await P.get(fileUrl("colors", keys.colors, "colors.json")) : {};
  const colors = {};
  for (const key of COLOR_KEYS) {
    colors[key] = toColor(json[key] || stockColors[key] || {});
  }

  const signals = signalInfo(keys.signals);
  signals.sequence = [];
  for (const [index, name] of signals.files.entries()) {
    let count = 1;
    if (/\.gif$/i.test(name)) {
      count = gifFrameCount(new Uint8Array(await (await fetchBlob(signals.frames[index])).arrayBuffer()));
    }
    for (let i = 0; i < count; i++) {
      signals.sequence.push(signals.frames[index]);
    }
  }
  signals.gif = signals.files.some(name => /\.gif$/i.test(name));

  const wheelList = listed("steering_wheels", keys.steering_wheels);
  const sounds = {};
  for (const slot of SOUND_SLOTS) {
    sounds[slot.id] = soundFor(slot.id, keys.sounds, theme);
  }

  return {
    colors,
    distance: Object.fromEntries(PERSONALITIES.map(name => [name, slotUrl("distance_icons", keys.distance_icons, name)])),
    icons: Object.fromEntries(["flag", "home", "settings"].map(kind => [kind, slotUrl("icons", keys.icons, `button_${kind}`)])),
    keys,
    signals,
    sounds,
    stockColors,
    stockScheme: !themed,
    wheel: keys.steering_wheels === "stock" ? "stock" : wheelList.length ? fileUrl("steering_wheels", keys.steering_wheels, wheelList[0]) : null
  };
}

function soundFor(slot, key, theme) {
  const list = listed("sounds", key);
  const tizi = TIZI_SLOTS.includes(slot) && list.includes(`${slot}_tizi.wav`) ? `${slot}_tizi.wav` : null;
  const name = tizi || (list.includes(`${slot}.wav`) ? `${slot}.wav` : null);
  if (name && key !== "stock") {
    return { from: theme.user ? theme.name : packName("sounds", key), own: theme.user, src: fileUrl("sounds", key, name) };
  }
  if (slot === "startup") {
    return { from: "Stock engage, as a fallback", own: false, src: fileUrl("sounds", "stock", "engage_tizi.wav") };
  }
  return { from: key === "stock" ? "Stock" : "Stock, as a fallback", own: false, src: fileUrl("sounds", "stock", TIZI_SLOTS.includes(slot) ? `${slot}_tizi.wav` : `${slot}.wav`) };
}

function wheelSrc() {
  if (look.wheel === "stock") {
    return IMG_DIR + (T.drive === "experimental" ? "experimental.svg" : "chffr_wheel.png");
  }
  return look.wheel;
}

function packKeys(component) {
  const info = data.components[component];
  const keys = Object.keys(data.builtins).filter(key => !info.excluded.includes(key));
  for (const entry of info.installed) {
    if (entry.key !== T.current && !keys.includes(entry.key)) {
      keys.push(entry.key);
    }
  }
  return keys.sort((a, b) => packName(component, a).localeCompare(packName(component, b)));
}

async function loadPacks(component) {
  if (component) {
    await Promise.all(packKeys(component).map(key => fileList(component, key)));
  }
}

const camImg = new Image();
camImg.src = IMG.src;
camImg.addEventListener("load", () => draw());

function surfaceGeom() {
  const open = T.sidebar;
  const ox = open ? 330 : 30;
  const W = open ? 1800 : 2100;
  const H = 1020;
  const s = Math.max(W / IMG.w, H / IMG.h);
  return { ox, W, H, s, ix: (W - IMG.w * s) / 2, iy: (H - IMG.h * s) / 2 };
}

function proj(g, x, y) {
  return [g.ix + (IMG.vpx + (IMG.k * y) / x) * g.s, g.iy + (IMG.vpy + (IMG.k * IMG.camH) / x) * g.s];
}

function ribbon(g, center, half, x0, x1, steps = 40) {
  const left = [];
  const right = [];
  for (let i = 0; i <= steps; i++) {
    const x = x0 * Math.pow(x1 / x0, i / steps);
    left.push(proj(g, x, center - half));
    right.push(proj(g, x, center + half));
  }
  const path = new Path2D();
  [...left, ...right.reverse()].forEach(([px, py], i) => (i ? path.lineTo(px, py) : path.moveTo(px, py)));
  path.closePath();
  return { path, left, right };
}

const LANES = [{ y: -5.55, p: .5, end: 55 }, { y: -1.85, p: .92, end: 80 }, { y: 1.85, p: .9, end: 80 }, { y: 5.55, p: .55, end: 55 }];
const EDGES = [{ y: -7.6, std: .45 }, { y: 7.3, std: .5 }];
let shapes = {};
let hover = null;
let canvas = null;
let ctx = null;

function draw() {
  if (!canvas || !canvas.isConnected) {
    return;
  }
  const g = surfaceGeom();
  const exp = T.drive === "experimental";
  const colors = look.colors;
  ctx.setTransform(1, 0, 0, 1, 0, 0);
  ctx.clearRect(0, 0, 2160, 1080);
  if (T.sidebar) {
    ctx.fillStyle = "#393939";
    ctx.fillRect(0, 0, 300, 1080);
  }
  ctx.fillStyle = BORDER_ENGAGED;
  ctx.fillRect(g.ox - 30, 0, g.W + 60, 1080);
  ctx.save();
  ctx.translate(g.ox, 30);
  ctx.beginPath();
  ctx.rect(0, 0, g.W, g.H);
  ctx.clip();
  ctx.fillStyle = "#111";
  ctx.fillRect(0, 0, g.W, g.H);
  if (camImg.complete && camImg.naturalWidth) {
    ctx.drawImage(camImg, g.ix, g.iy, IMG.w * g.s, IMG.h * g.s);
  }

  const shade = ctx.createLinearGradient(0, 252, 0, 420);
  shade.addColorStop(0, "rgba(0,0,0,.45)");
  shade.addColorStop(1, "rgba(0,0,0,0)");
  ctx.fillStyle = "rgba(0,0,0,.45)";
  ctx.fillRect(0, 0, g.W, 252);
  ctx.fillStyle = shade;
  ctx.fillRect(0, 252, g.W, 168);

  shapes = {};
  const lanePaths = [];
  const laneHit = new Path2D();
  const lc = colors.LaneLines;
  for (const lane of LANES) {
    const rib = ribbon(g, lane.y, (0.1016 / 2) * lane.p, 2.2, lane.end);
    const alpha = Math.min(lane.p, .7);
    ctx.fillStyle = look.stockScheme ? `rgba(255,255,255,${alpha})` : rgba(lc.hex, (lc.a / 255) * alpha);
    ctx.fill(rib.path);
    lanePaths.push(rib.path);
    laneHit.addPath(ribbon(g, lane.y, .32, 2.2, lane.end).path);
  }
  shapes.lanes = { draw: lanePaths, hit: laneHit };
  for (const edge of EDGES) {
    const rib = ribbon(g, edge.y, .0254, 2.2, 45);
    ctx.fillStyle = `rgba(255,0,0,${Math.max(0, Math.min(1, 1 - edge.std))})`;
    ctx.fill(rib.path);
  }

  const maxDist = Math.max(0, 2 * LEAD_D - Math.min(0.7 * LEAD_D, 10));
  const outerHalf = (6.1 * 0.3048) / 2;
  const innerHalf = outerHalf * (1 - 20 / 100);
  const outer = ribbon(g, 0, outerHalf, 2.2, maxDist);
  const inner = ribbon(g, 0, innerHalf, 2.2, maxDist);
  const pg = ctx.createLinearGradient(0, g.H, 0, 0);
  if (look.stockScheme) {
    pg.addColorStop(0, "hsla(148,94%,51%,.4)");
    pg.addColorStop(.5, "hsla(112,100%,68%,.35)");
    pg.addColorStop(1, "hsla(112,100%,68%,0)");
  } else {
    pg.addColorStop(0, rgba(colors.Path.hex, 1));
    pg.addColorStop(1, rgba(colors.Path.hex, .1));
  }
  ctx.fillStyle = pg;
  ctx.fill(inner.path);
  shapes.path = { draw: [inner.path], hit: inner.path };

  const ring = new Path2D();
  ring.addPath(outer.path);
  ring.addPath(inner.path);
  const eg = ctx.createLinearGradient(0, g.H, 0, 0);
  if (look.stockScheme && !exp) {
    eg.addColorStop(0, "hsla(148,94%,41%,.4)");
    eg.addColorStop(.5, "hsla(112,100%,54%,.35)");
    eg.addColorStop(1, "hsla(112,100%,54%,0)");
  } else {
    const ec = exp ? EXPERIMENTAL : colors.PathEdge.hex;
    eg.addColorStop(0, rgba(ec, .4));
    eg.addColorStop(.5, rgba(ec, .35));
    eg.addColorStop(1, rgba(ec, 0));
  }
  ctx.fillStyle = eg;
  ctx.fill(ring, "evenodd");
  const ringHit = new Path2D();
  ringHit.addPath(ribbon(g, -(outerHalf + innerHalf) / 2, (outerHalf - innerHalf) / 2 + .22, 2.2, maxDist).path);
  ringHit.addPath(ribbon(g, (outerHalf + innerHalf) / 2, (outerHalf - innerHalf) / 2 + .22, 2.2, maxDist).path);
  shapes.edges = { draw: [outer.path], hit: ringHit };

  const sz = Math.max(15, Math.min(30, 750 / (LEAD_D / 3 + 30))) * 2.35;
  let [lx, ly] = proj(g, LEAD_D, 0);
  lx = Math.max(0, Math.min(g.W - sz / 2, lx));
  ly = Math.min(ly, g.H - sz * .6);
  const glow = new Path2D();
  glow.moveTo(lx + sz * 1.35 + sz / 5, ly + sz + sz / 10);
  glow.lineTo(lx, ly - sz / 10);
  glow.lineTo(lx - sz * 1.35 - sz / 5, ly + sz + sz / 10);
  glow.closePath();
  ctx.fillStyle = "rgb(218,202,37)";
  ctx.fill(glow);
  const chev = new Path2D();
  chev.moveTo(lx + sz * 1.25, ly + sz);
  chev.lineTo(lx, ly);
  chev.lineTo(lx - sz * 1.25, ly + sz);
  chev.closePath();
  const fillAlpha = Math.min(255, Math.max(0, 255 * (1 - LEAD_D / 40))) / 255;
  ctx.fillStyle = rgba(colors.LeadMarker.hex, fillAlpha);
  ctx.fill(chev);
  const leadBox = [lx - sz * 2, ly - sz * .8, sz * 4, sz * 2.4];
  const leadHit = new Path2D();
  leadHit.rect(...leadBox);
  shapes.lead = { draw: [glow], hit: leadHit, box: leadBox };

  const outlineFor = (part, strong) => {
    const s = shapes[part];
    if (!s) {
      return;
    }
    ctx.save();
    ctx.lineJoin = "round";
    for (const p of s.draw) {
      ctx.setLineDash(strong ? [] : [22, 14]);
      ctx.lineWidth = strong ? 13 : 9;
      ctx.strokeStyle = "rgba(0,0,0,.55)";
      ctx.stroke(p);
      ctx.lineWidth = strong ? 7 : 5;
      ctx.strokeStyle = strong ? lily() : "rgba(255,255,255,.9)";
      ctx.stroke(p);
    }
    ctx.restore();
  };
  if (hover && hover !== T.part && shapes[hover]) {
    outlineFor(hover, false);
  }
  if (shapes[T.part]) {
    outlineFor(T.part, true);
  }
  ctx.restore();
  placeKbdHits(g);
}

let lilyCache = "";
function lily() {
  if (!lilyCache) {
    lilyCache = getComputedStyle(document.documentElement).getPropertyValue("--lily").trim();
  }
  return lilyCache;
}

function placeKbdHits(g) {
  const holder = document.getElementById("kbd-hits");
  if (!holder) {
    return;
  }
  const boxes = {
    path: [proj(g, 2.6, -1.2), proj(g, 20, 1.2)],
    lanes: [proj(g, 3, -1.85), proj(g, 40, 1.85)],
    edges: [proj(g, 3, -1.1), proj(g, 20, 1.1)],
    lead: [shapes.lead.box.slice(0, 2), [shapes.lead.box[0] + shapes.lead.box[2], shapes.lead.box[1] + shapes.lead.box[3]]]
  };
  holder.querySelectorAll("[data-kbd]").forEach(b => {
    const [[x1, y1], [x2, y2]] = boxes[b.dataset.kbd];
    const left = Math.max(0, Math.min(x1, x2));
    const top = Math.max(0, Math.min(y1, y2));
    b.style.left = `${left}px`;
    b.style.top = `${top}px`;
    b.style.width = `${Math.min(g.W, Math.max(x1, x2)) - left}px`;
    b.style.height = `${Math.min(g.H, Math.max(y1, y2)) - top}px`;
  });
}

function stageHTML() {
  const exp = T.drive === "experimental";
  const hit = (part, cls, inner, label) => `<button type="button" class="hit ${cls} ${T.part === part ? "sel" : ""}" data-part="${part}" aria-label="${label}">${inner}</button>`;
  const img = src => (src ? `<img src="${src}" alt="">` : "");
  return `
    <canvas id="tm-canvas" width="2160" height="1080" aria-hidden="true"></canvas>
    ${T.sidebar ? `<div class="sb">
      ${hit("icons", "set-btn", img(look.icons.settings), "Settings button icon")}
      <div class="net" aria-hidden="true">${"<i></i>".repeat(5)}</div>
      <div class="net-type" aria-hidden="true">Wi-Fi</div>
      ${[["TEMP", "GOOD", "Sidebar1"], ["VEHICLE", "ONLINE", "Sidebar2"], ["CONNECT", "ONLINE", "Sidebar3"]].map(([a, b, key]) => hit("sidebar", `slot slot-${key.slice(-1)}`, `<span class="bar" data-swatch="${key}"></span><span class="txt">${a}<br>${b}</span>`, `Sidebar ${COLOR_LABELS[key]} color`)).join("")}
      ${hit("icons", "home-btn", img(look.icons[T.button]), T.button === "flag" ? "Flag button icon, shown while driving" : "Home button icon, shown while parked")}
    </div>` : ""}
    <div class="surface" id="surface" data-surface>
      <div class="hud set-box"><b>MAX</b><span>68</span><div class="sl-sign"><b>LIMIT</b><span>65</span><i>+3</i></div></div>
      <div class="hud rec-pill"><span>RECORD</span><i></i></div>
      <div class="hud replay-pill">CAPTURE</div>
      <div class="hud cur-speed"><b>67</b><span>mph</span></div>
      ${look.wheel ? hit("wheel", `wheel-btn ${exp ? "exp" : ""}`, img(wheelSrc()), "Steering wheel icon") : hit("wheel", "wheel-btn none", "", "Steering wheel icon, set to none")}
      ${T.showPersonality ? hit("distance", "pers-btn", img(look.distance[T.personality]), "Personality button icon") : ""}
      <div class="hud dm ${T.showPersonality ? "shift" : ""}" data-dm><img src="${IMG_DIR}driver_face.png" alt=""></div>
      <img class="sig-img" data-sig alt="" hidden>
      <div id="kbd-hits">${[["path", "Path color"], ["lanes", "Lane line color"], ["edges", "Path edge color"], ["lead", "Lead marker color"]].map(([k, l]) => `<button type="button" class="kbd-hit" data-kbd="${k}" data-part="${k}" aria-label="${l}"></button>`).join("")}</div>
    </div>`;
}

function paint(root) {
  root.querySelectorAll("[data-swatch]").forEach(el => {
    el.style.background = colorCss(look.colors[el.dataset.swatch]);
  });
  root.querySelectorAll("[data-preset]").forEach(el => {
    el.style.background = el.dataset.preset;
  });
  root.querySelectorAll("[data-color]").forEach(ed => {
    ed.querySelector("[data-sw]").style.background = colorCss(look.colors[ed.dataset.color]);
  });
}

function buildStage() {
  const stage = document.getElementById("stage");
  if (!stage) {
    return;
  }
  stage.classList.toggle("sb-open", T.sidebar);
  stage.innerHTML = stageHTML();
  paint(stage);
  canvas = stage.querySelector("#tm-canvas");
  ctx = canvas.getContext("2d");
  lilyCache = "";
  draw();
  updateSignal(true);
  placeTapTargets();
}

function placeTapTargets() {
  const layer = document.getElementById("tap-layer");
  const replica = document.getElementById("replica");
  if (!layer || !replica || getComputedStyle(layer).display === "none") {
    return;
  }
  const box = replica.getBoundingClientRect();
  const groups = [];
  const slots = [...document.querySelectorAll("#stage .sb .slot")];
  if (slots.length) {
    groups.push({ part: "sidebar", rects: slots.map(s => s.getBoundingClientRect()) });
  }
  for (const el of document.querySelectorAll("#stage .hit:not(.slot), #stage .kbd-hit[data-kbd='lead']")) {
    groups.push({ part: el.dataset.part, rects: [el.getBoundingClientRect()] });
  }
  layer.replaceChildren(...groups.map(g => {
    const left = Math.min(...g.rects.map(r => r.left)) - box.left;
    const top = Math.min(...g.rects.map(r => r.top)) - box.top;
    const right = Math.max(...g.rects.map(r => r.right)) - box.left;
    const bottom = Math.max(...g.rects.map(r => r.bottom)) - box.top;
    const w = Math.max(44, right - left);
    const h = Math.max(44, bottom - top);
    const button = document.createElement("button");
    button.type = "button";
    button.tabIndex = -1;
    button.dataset.tap = g.part;
    button.style.left = `${Math.max(0, Math.min(box.width - w, (left + right) / 2 - w / 2))}px`;
    button.style.top = `${Math.max(0, Math.min(box.height - h, (top + bottom) / 2 - h / 2))}px`;
    button.style.width = `${w}px`;
    button.style.height = `${h}px`;
    return button;
  }));
}

let sigTimer = null;
let sigStart = 0;
let sigToken = 0;
let decoded = new Map();
let replicaWaits = 0;

function decodeImage(url) {
  const image = new Image();
  image.src = url;
  return image.decode().then(() => image);
}

function signalFrames(s) {
  if (!s.loading) {
    const urls = [...new Set([...s.sequence, s.blind].filter(Boolean))];
    decoded = new Map(urls.map(url => [url, decoded.get(url) || decodeImage(url)]));
    s.loading = Promise.all(decoded.values()).then(([first]) => {
      s.size = { h: first.naturalHeight, w: first.naturalWidth };
    }, error => {
      decoded = new Map();
      s.loading = null;
      throw error;
    });
  }
  return s.loading;
}

async function replicaBusy(promise) {
  replicaWaits += 1;
  document.getElementById("replica")?.classList.add("tm-loading");
  try {
    return await promise;
  } finally {
    replicaWaits -= 1;
    if (!replicaWaits) {
      document.getElementById("replica")?.classList.remove("tm-loading");
    }
  }
}

function updateSignal(reset) {
  const img = document.querySelector("[data-sig]");
  const dm = document.querySelector("[data-dm]");
  const pers = document.querySelector(".pers-btn");
  const token = ++sigToken;
  clearInterval(sigTimer);
  sigTimer = null;
  renderControlsState();
  if (!img) {
    return;
  }
  const s = look.signals;
  const side = T.signal;
  const traditional = s.style !== "static";
  if (!side || !s.sequence.length || !s.style || !s.size) {
    img.hidden = true;
    if (dm) {
      dm.style.visibility = "";
    }
    if (pers) {
      pers.style.visibility = "";
    }
    if (side && s.sequence.length && s.style) {
      replicaBusy(signalFrames(s)).then(() => {
        if (s.size && token === sigToken) {
          updateSignal(reset);
        }
      }, () => {
        if (token === sigToken && !replicaWaits) {
          P.toast("Couldn't show the turn signal frames", "", "alert");
        }
      });
    }
    return;
  }
  const g = surfaceGeom();
  const { w, h } = s.size;
  img.style.width = `${w}px`;
  img.style.height = `${h}px`;
  if (reset) {
    sigStart = performance.now();
  }
  if (dm) {
    dm.style.visibility = traditional ? "hidden" : "";
  }
  if (pers) {
    pers.style.visibility = traditional ? "hidden" : "";
  }
  const step = () => {
    const n = s.sequence.length;
    let i = Math.floor((performance.now() - sigStart) / s.ms) % n;
    if (P.reducedMotion.matches) {
      i = 0;
    }
    const blind = T.blind && s.blind;
    let x;
    let y = g.H - h;
    if (!traditional) {
      const cx = Math.floor((g.W - 1) / 2);
      x = side === "left" ? Math.floor(cx * .75) - w : Math.floor(cx * 1.25);
      y = Math.floor(h / 2);
    } else if (blind) {
      x = side === "left" ? g.W - w : 0;
    } else if (s.gif) {
      const m = Math.floor((g.W + w * 2) / n);
      x = side === "left" ? g.W - i * m + w : i * m - w;
    } else {
      x = side === "left" ? g.W - (i + 1) * w : i * w;
    }
    const src = blind ? s.blind : s.sequence[i];
    if (img.getAttribute("src") !== src) {
      img.src = src;
    }
    img.style.left = `${x}px`;
    img.style.top = `${y}px`;
    img.classList.toggle("mirror", side === "right");
    img.hidden = false;
  };
  step();
  if (!P.reducedMotion.matches) {
    sigTimer = setInterval(step, Math.max(30, s.ms / 2));
  }
}

let audio = null;
let audioSlot = null;

function stopSound() {
  if (audio) {
    audio.pause();
  }
  audio = null;
  audioSlot = null;
  markPlaying();
}

function playSound(slotId) {
  const was = audioSlot;
  stopSound();
  if (was === slotId) {
    return;
  }
  const player = new Audio(look.sounds[slotId].src);
  audio = player;
  audioSlot = slotId;
  player.addEventListener("ended", () => {
    if (audio === player) {
      stopSound();
    }
  });
  player.play().catch(error => {
    if (audio === player) {
      stopSound();
    }
    P.toast("Couldn't play the sound", esc(error.message), "alert");
  });
  markPlaying();
}

function markPlaying() {
  document.querySelectorAll("[data-play-sound]").forEach(b => {
    const on = b.dataset.playSound === audioSlot;
    b.classList.toggle("on", on);
    b.setAttribute("aria-label", `${on ? "Stop" : "Play"} ${SOUND_SLOTS.find(s => s.id === b.dataset.playSound).label} sound`);
    b.innerHTML = ic(on ? "pause" : "play");
  });
}

function partLabel(component, ownLabel) {
  if (own(component)) {
    return ownLabel;
  }
  return packName(component, effective(cur(), component));
}

function stripHTML() {
  const s = look.signals;
  const btn = (part, prev, label, sub, extra = "", pressed = T.part === part) => `<button type="button" class="slot-btn" data-part="${part}" ${extra} aria-pressed="${pressed}">${prev}<span class="lbl">${label}${sub ? `<small>${sub}</small>` : ""}</span></button>`;
  const swatch = key => `<span class="slot-prev sw-prev"><i data-swatch="${key}"></i></span>`;
  const preview = src => `<span class="slot-prev">${src ? `<img src="${src}" alt="">` : ""}</span>`;
  const signalLabel = s.style && s.frames.length ? `${esc(partLabel("signals", "Your frames"))}, ${s.ms}${P.NB}ms` : "None";
  return `
    <div class="strip-group"><h3>Colors</h3><div class="strip-row">
      ${btn("path", swatch("Path"), "Path", look.colors.Path.hex)}
      ${btn("lanes", swatch("LaneLines"), "Lane lines", look.colors.LaneLines.hex)}
      ${btn("edges", swatch("PathEdge"), "Path edges", look.colors.PathEdge.hex)}
      ${btn("lead", swatch("LeadMarker"), "Lead marker", look.colors.LeadMarker.hex)}
      ${btn("sidebar", `<span class="slot-prev sw-prev sw-bars">${["Sidebar1", "Sidebar2", "Sidebar3"].map(k => `<i data-swatch="${k}"></i>`).join("")}</span>`, "Sidebar bars", "3 colors")}
    </div></div>
    <div class="strip-group"><h3>Icons and animation</h3><div class="strip-row">
      ${btn("wheel", preview(look.wheel ? wheelSrc() : null), "Steering wheel", esc(partLabel("steering_wheels", "Your image")))}
      ${btn("signals", preview(s.frames[2] || s.frames[0]), "Turn signals", signalLabel)}
      ${btn("icons", preview(look.icons.home), "Sidebar buttons", "Settings, flag, home")}
      ${btn("distance", preview(look.distance.standard), "Personality icons", esc(partLabel("distance_icons", "Your images")))}
    </div></div>
    <div class="strip-group"><h3>Sounds</h3><div class="strip-row">
      ${SOUND_SLOTS.map(s2 => `<span class="snd-wrap">${btn("sounds", `<span class="slot-prev snd">${ic("sound")}</span>`, s2.label, "", `data-sound="${s2.id}"`, T.part === "sounds" && T.soundFocus === s2.id)}<button type="button" class="icon-btn" data-play-sound="${s2.id}" aria-label="Play ${s2.label} sound">${ic("play")}</button></span>`).join("")}
    </div></div>`;
}

function colorEditor(key, note) {
  const c = look.colors[key];
  return `<div class="color-ed full" data-color="${key}">
    <div class="color-top">
      <label class="big-sw"><i data-sw></i><input type="color" value="${c.hex.toLowerCase()}" data-in="picker" aria-label="${COLOR_LABELS[key]} color picker"></label>
      <div class="tm-color-name"><b>${COLOR_LABELS[key]}</b><div class="hex-row"><label class="sr" for="hex-${key}">${COLOR_LABELS[key]} hex</label><input class="input" id="hex-${key}" value="${c.hex}" maxlength="7" spellcheck="false" autocomplete="off" data-in="hex"></div></div>
    </div>
    ${ALPHA_USED.includes(key) ? `<div class="alpha-row"><label for="a-${key}">Opacity</label><input type="range" id="a-${key}" min="0" max="255" value="${c.a}" data-in="alpha"><output data-out>${Math.round((c.a / 255) * 100)}%</output></div>` : ""}
    <div class="presets" role="group" aria-label="Quick colors">${PRESETS.map(h => `<button type="button" data-preset="${h}" aria-label="Use ${h}"></button>`).join("")}</div>
    ${note ? `<p class="how">${ic("info")}<span>${note}</span></p>` : ""}
  </div>`;
}

function choiceGrid(kind, current, items) {
  return `<div class="choice-grid" role="group">${items.map(it => `<button type="button" class="choice" data-choice="${kind}" data-value="${esc(it.value)}" aria-pressed="${current === it.value}"><span class="pv ${it.sq ? "sq" : ""}">${it.src ? `<img src="${it.src}" alt="">` : ""}</span>${esc(it.label)}</button>`).join("")}</div>`;
}

function packChoices(component, src, sq = false) {
  return packKeys(component).map(key => ({ value: key, label: packName(component, key), src: src(key), sq }));
}

function currentChoice(component) {
  if (own(component)) {
    return "own";
  }
  return effective(cur(), component);
}

function uploadButton(kind, label, accept, cls = "btn btn-quiet btn-small file-btn full tm-start", multiple = false) {
  return `<label class="${cls}">${ic("upload")}${label}<input type="file" ${multiple ? "multiple" : ""} accept="${accept}" data-upload="${kind}"></label>`;
}

function inspectorHTML() {
  const t = cur();
  const part = T.part;
  const m = META[part];
  const s = look.signals;
  const stockNote = t.user && look.stockScheme ? " This theme uses openpilot's stock colors until you change one." : "";
  let body = "";
  if (part === "path") {
    body = colorEditor("Path", `The car always fades the path from solid at the bottom to 10% at the top, so only the color is used.${stockNote}`);
  } else if (part === "lanes") {
    body = colorEditor("LaneLines", `Opacity is used. The car also fades each line by how sure it is the line is there, to at most 70%.${stockNote}`);
  } else if (part === "edges") {
    body = colorEditor("PathEdge", `The car always draws the edges at 40%, fading out up the screen, so only the color is used. In Experimental Mode the car uses its own orange instead${T.drive === "experimental" ? ", which is what you're seeing now" : ""}.${stockNote}`);
  } else if (part === "lead") {
    body = colorEditor("LeadMarker", "The yellow glow always shows. The car sets how solid your color is from the distance to the car ahead: clearer as it gets farther, gone past 40 m. This truck is about 18 m ahead.");
  } else if (part === "sidebar") {
    body = ["Sidebar1", "Sidebar2", "Sidebar3"].map(k => colorEditor(k, k === "Sidebar3" ? "Opacity is used. Each bar turns yellow or red on its own when something needs attention." : "")).join("");
  } else if (part === "wheel") {
    const items = packChoices("steering_wheels", key => (key === "stock" ? `${IMG_DIR}chffr_wheel.png` : listed("steering_wheels", key)[0] ? fileUrl("steering_wheels", key, listed("steering_wheels", key)[0]) : ""));
    if (own("steering_wheels")) {
      items.unshift({ value: "own", label: "Your image", src: look.wheel });
    }
    body = `<div class="full">${choiceGrid("wheel", currentChoice("steering_wheels"), items)}</div>
      ${uploadButton("wheel", "Upload an image", IMAGE_TYPES)}
      <p class="how full">${ic("info")}<span>PNG and SVG are fitted into 144${P.NB}px. A GIF plays at exactly 144 by 144. None hides the button's circle too.</span></p>`;
  } else if (part === "signals") {
    const items = packChoices("signals", key => {
      const info = signalInfo(key);
      return info.frames[2] || info.frames[0] || "";
    }, true);
    if (own("signals")) {
      items.unshift({ value: "own", label: "Your frames", src: s.frames[2] || s.frames[0], sq: true });
    }
    const styleOff = s.frames.length ? "" : "disabled";
    body = `<div class="full">${choiceGrid("signals", currentChoice("signals"), items)}</div>
      <div class="field"><span>Test it</span><div class="seg" role="group" aria-label="Blinker">
        <button type="button" data-act="tm-signal" data-side="left" aria-pressed="${T.signal === "left"}">${ic("back", "ic-s")} Left</button>
        <button type="button" data-act="tm-signal" data-side="right" aria-pressed="${T.signal === "right"}">Right ${ic("next", "ic-s")}</button>
      </div></div>
      <label class="switch"><input type="checkbox" data-blind ${T.blind ? "checked" : ""}><span class="track"></span><span>Car in the blind spot</span></label>
      <div class="field"><span>Style</span><div class="seg" role="group" aria-label="Style">
        <button type="button" data-style="traditional" aria-pressed="${s.style === "traditional"}" ${styleOff}>Hops across</button>
        <button type="button" data-style="static" aria-pressed="${s.style === "static"}" ${styleOff}>Stays put</button>
      </div></div>
      <label class="field"><span>Time per frame</span><span class="alpha-row tm-ms-row"><input type="range" min="50" max="400" step="5" value="${s.ms}" data-ms aria-label="Milliseconds per frame" ${styleOff}><output data-ms-out>${s.ms}${P.NB}ms</output></span></label>
      ${uploadButton("signals", "Upload frames", IMAGE_TYPES, undefined, true)}
      <p class="how full">${ic("info")}<span>Frames play in name order. 350 by 500 matches the built-in frogs. Name one with “blindspot” to use it when a car is beside you.</span></p>`;
  } else if (part === "icons") {
    const rows = [["settings", "Settings button", "Top of the sidebar"], ["flag", "Flag button", "Bottom of the sidebar while driving. Tap it to flag a moment."], ["home", "Home button", "Bottom of the sidebar while parked"]];
    body = rows.map(([kind, label, where]) => {
      const slot = `button_${kind}`;
      const ownSlot = own("icons") && pickSlot(listed("icons", t.key), slot);
      const items = packChoices("icons", key => slotUrl("icons", key, slot) || "", kind === "settings");
      if (ownSlot) {
        items.unshift({ value: "own", label: "Yours", src: look.icons[kind], sq: kind === "settings" });
      }
      const current = t.user ? (ownSlot ? "own" : own("icons") ? "" : "stock") : t.parts.icons;
      return `<div class="field full"><span>${label}</span><span class="muted tm-where">${where}</span>
        ${choiceGrid(`icon-${kind}`, current, items)}
        <span class="tm-file-row">${uploadButton(`icon-${kind}`, "Upload", IMAGE_TYPES, "btn btn-text btn-small file-btn tm-flush")}${ownSlot ? `<button type="button" class="btn btn-text btn-small" data-remove-slot="icons/${slot}">${ic("trash")}Remove</button>` : ""}</span></div>`;
    }).join("");
  } else if (part === "distance") {
    const items = packChoices("distance_icons", key => slotUrl("distance_icons", key, T.personality) || "");
    if (own("distance_icons")) {
      items.unshift({ value: "own", label: "Yours", src: look.distance[T.personality] });
    }
    const ownSlot = own("distance_icons") && pickSlot(listed("distance_icons", t.key), T.personality);
    const name = T.personality[0].toUpperCase() + T.personality.slice(1);
    body = `<div class="full">${choiceGrid("distance", currentChoice("distance_icons"), items)}</div>
      <div class="field"><span>Personality</span><div class="seg tm-wrap" role="group" aria-label="Personality">${PERSONALITIES.map(p => `<button type="button" data-pers="${p}" aria-pressed="${T.personality === p}">${p[0].toUpperCase() + p.slice(1)}</button>`).join("")}</div></div>
      <label class="switch"><input type="checkbox" data-show-pers ${T.showPersonality ? "checked" : ""}><span class="track"></span><span>Show the button on the screen</span></label>
      <span class="tm-file-row full">${uploadButton("distance", `Upload ${name}`, IMAGE_TYPES, "btn btn-quiet btn-small file-btn")}${ownSlot ? `<button type="button" class="btn btn-text btn-small" data-remove-slot="distance_icons/${T.personality}">${ic("trash")}Remove ${name}</button>` : ""}</span>
      <p class="how full">${ic("info")}<span>The car shows these only when the onroad personality button is turned on in FrogPilot's settings.</span></p>`;
  } else if (part === "sounds") {
    const options = [["", "Choose sounds to copy"], ...packKeys("sounds").map(key => [key, `${packName("sounds", key)} sounds`])];
    body = `<label class="field full"><span>Start from</span>${P.dropdown({ value: "", options, label: "Start from", attrs: "data-sound-pack" })}</label>
      <div class="snd-list full">${SOUND_SLOTS.map(slot => {
        const info = look.sounds[slot.id];
        return `<div class="snd-row ${T.soundFocus === slot.id ? "focus" : ""}" data-snd-row="${slot.id}">
          <button type="button" class="icon-btn play" data-play-sound="${slot.id}" aria-label="Play ${slot.label} sound">${ic("play")}</button>
          <span><b>${slot.label}</b><span>${slot.when}</span><span class="tm-from">${esc(info.from)}</span></span>
          <span class="tm-snd-acts">${info.own ? `<button type="button" class="icon-btn" data-remove-slot="sounds/${slot.id}" title="Use stock" aria-label="Use the stock ${slot.label} sound">${ic("refresh")}</button>` : ""}<label class="icon-btn file-btn" title="Replace" aria-label="Replace ${slot.label} sound">${ic("upload")}<input type="file" accept="audio/*,.wav" data-upload="sound-${slot.id}"></label></span>
        </div>`;
      }).join("")}</div>
      <p class="how full">${ic("info")}<span>The device turns what you upload into the mono, 16-bit, 48${P.NB}kHz WAV the car plays. Slots a theme leaves empty use openpilot's stock sound.</span></p>`;
  }
  return `
    <div class="insp-head"><span class="ic-wrap">${ic(m.icon)}</span><div><h2 id="insp-h">${m.title}</h2><p>${m.where}</p></div></div>
    ${t.user ? "" : `<div class="tm-notice-wrap"><div class="notice">${ic("lock")}<span><b>${esc(t.name)} is built in.</b> Change anything and you get your own copy to edit; the original stays as it is.</span></div></div>`}
    <fieldset class="insp-body tm-fieldset">${body}</fieldset>
    ${t.user ? `<div class="insp-foot"><button class="btn btn-text btn-small tm-flush" data-act="tm-reset-part">${ic("refresh")}Reset to stock</button></div>` : ""}`;
}

function controlsHTML() {
  return `
    <span class="ctl"><label class="switch"><input type="checkbox" data-sidebar ${T.sidebar ? "checked" : ""}><span class="track"></span><span class="field-label">Sidebar</span></label></span>
    <span class="ctl"><span class="field-label" id="btn-lbl">Bottom button</span><span class="seg" role="group" aria-labelledby="btn-lbl">
      <button type="button" data-btn="flag" aria-pressed="${T.button === "flag"}">${ic("flag")} Flag<span class="lbl-long">, driving</span></button>
      <button type="button" data-btn="home" aria-pressed="${T.button === "home"}">${ic("home")} Home<span class="lbl-long">, parked</span></button>
    </span></span>
    <span class="ctl"><span class="field-label" id="sig-lbl">Blinker</span><span class="seg" role="group" aria-labelledby="sig-lbl">
      <button type="button" data-act="tm-signal" data-side="left" aria-pressed="${T.signal === "left"}">${ic("back")} Left</button>
      <button type="button" data-act="tm-signal" data-side="right" aria-pressed="${T.signal === "right"}">Right ${ic("next")}</button>
    </span></span>
    <span class="ctl"><span class="field-label" id="dr-lbl">Mode</span><span class="seg" role="group" aria-labelledby="dr-lbl">
      <button type="button" data-drive="engaged" aria-pressed="${T.drive === "engaged"}">Engaged</button>
      <button type="button" data-drive="experimental" aria-pressed="${T.drive === "experimental"}">Experimental</button>
    </span></span>
    <p class="muted tm-hint">Click anything on the screen to change it. Tap the road to open or close the sidebar, like on the car.</p>`;
}

function renderControlsState() {
  document.querySelectorAll("[data-act='tm-signal']").forEach(b => b.setAttribute("aria-pressed", String(T.signal === b.dataset.side)));
}

let inspectorToken = 0;

async function renderInspector() {
  const insp = document.getElementById("inspector");
  if (!insp) {
    return;
  }
  const token = ++inspectorToken;
  try {
    await loadPacks(PART_COMPONENT[T.part]);
  } catch (error) {
    P.fail(error, "Couldn't load the choices");
  }
  if (token !== inspectorToken || !insp.isConnected) {
    return;
  }
  insp.innerHTML = inspectorHTML();
  paint(insp);
  markPlaying();
}

function refreshStrip() {
  const strip = document.getElementById("strip");
  if (!strip) {
    return;
  }
  strip.innerHTML = stripHTML();
  paint(strip);
  markPlaying();
}

async function rebuild() {
  buildStage();
  refreshStrip();
  const using = document.getElementById("tm-using");
  if (using) {
    using.innerHTML = `The car is using <b>${esc(usingName())}</b>`;
  }
  await renderInspector();
}

function setSidebar(open) {
  T.sidebar = open;
  const box = document.querySelector("[data-sidebar]");
  if (box) {
    box.checked = open;
  }
  P.setParam("sidebar", open ? "open" : "");
  buildStage();
}

async function selectPart(part, sound, control) {
  T.part = part;
  if (sound) {
    T.soundFocus = sound;
  }
  if ((part === "sidebar" || part === "icons") && !T.sidebar) {
    setSidebar(true);
  }
  if (part === "distance" && !T.showPersonality) {
    T.showPersonality = true;
    buildStage();
  }
  if (part === "signals" && !T.signal) {
    T.signal = "left";
  }
  P.setParam("part", part === "path" ? "" : part);
  document.querySelectorAll("#stage .hit").forEach(h => h.classList.toggle("sel", h.dataset.part === part));
  document.querySelectorAll(".slot-btn").forEach(b => {
    b.setAttribute("aria-pressed", String(b.dataset.part === part && (part !== "sounds" || b.dataset.sound === T.soundFocus)));
  });
  draw();
  updateSignal(part === "signals");
  const component = PART_COMPONENT[part];
  const cached = !component || packKeys(component).every(key => lists.has(`${component}/${key}`));
  const loading = renderInspector();
  if (!cached) {
    if (control) {
      P.busy(control, loading);
    } else {
      replicaBusy(loading);
    }
  }
  await loading;
  if (matchMedia("(max-width: 1099px)").matches) {
    const insp = document.getElementById("inspector");
    const replica = document.getElementById("replica");
    if (!insp || !replica) {
      return;
    }
    const stickyHeight = matchMedia("(max-width: 719px)").matches ? replica.offsetHeight : 0;
    insp.style.scrollMarginTop = `${document.getElementById("topbar").offsetHeight + stickyHeight + 12}px`;
    insp.scrollIntoView({ behavior: P.reducedMotion.matches ? "auto" : "smooth", block: "start" });
  }
}

let queue = Promise.resolve();

function enqueue(work) {
  const run = queue.then(work);
  queue = run.catch(() => {});
  return run;
}

async function reloadCurrent(key, component, version) {
  if (component) {
    forget(key, component, version);
  }
  try {
    await loadData();
    if (T.current !== key || !document.getElementById("stage")) {
      return;
    }
    look = await loadLook(cur());
  } catch (error) {
    P.fail(error, "Couldn't reload the theme");
    return;
  }
  await rebuild();
}

function mutate(change) {
  const saving = save(change);
  return change.component === "sounds" ? saving : replicaBusy(saving);
}

async function save({ component, control, done = "", doneDetail = "", failTitle, label, version, work }) {
  const key = T.current;
  await flushColors();
  const target = document.querySelector(control);
  const handle = P.job({ label });
  const saving = enqueue(() => work(key));
  let saved = true;
  try {
    await (target ? P.busy(target, saving) : saving);
    handle.done(done, doneDetail);
  } catch (error) {
    saved = false;
    handle.fail(error, failTitle, MESSAGES);
  }
  await reloadCurrent(key, component, saved ? version : undefined);
  const replaced = saved && target && !target.isConnected && document.querySelector(control);
  if (replaced) {
    P.busy(replaced, Promise.resolve());
  }
}

let colorTimer = 0;
let colorsKey = "";

function colorsBody() {
  const colors = {};
  for (const key of COLOR_KEYS) {
    const [red, green, blue] = rgb(look.colors[key].hex);
    colors[key] = { alpha: look.colors[key].a, blue, green, red };
  }
  return { colors };
}

function flushColors() {
  clearTimeout(colorTimer);
  if (!colorsKey) {
    return queue;
  }
  const key = colorsKey;
  const body = colorsBody();
  colorsKey = "";
  return enqueue(async () => {
    await P.request("PUT", `/api/user_themes/${enc(key)}/colors`, body);
    forget(key, "colors");
    if (!installed("colors", key)) {
      data.components.colors.installed.push({ key, name: nameFromKey(key), user_created: true });
    }
  }).catch(error => {
    P.fail(error, "Couldn't save the colors", MESSAGES);
    return reloadCurrent(key);
  });
}

function editColor(key, patch, source) {
  Object.assign(look.colors[key], patch);
  look.stockScheme = false;
  const c = look.colors[key];
  const ed = document.querySelector(`[data-color="${key}"]`);
  if (ed) {
    ed.querySelector("[data-sw]").style.background = colorCss(c);
    if (source !== "picker") {
      ed.querySelector("[data-in='picker']").value = c.hex.toLowerCase();
    }
    if (source !== "hex") {
      ed.querySelector("[data-in='hex']").value = c.hex;
    }
    const alpha = ed.querySelector("[data-in='alpha']");
    if (alpha) {
      if (source !== "alpha") {
        alpha.value = c.a;
      }
      ed.querySelector("[data-out]").textContent = `${Math.round((c.a / 255) * 100)}%`;
    }
  }
  document.querySelectorAll(`#stage [data-swatch="${key}"]`).forEach(bar => {
    bar.style.background = colorCss(c);
  });
  draw();
  colorsKey = T.current;
  clearTimeout(colorTimer);
  colorTimer = setTimeout(() => {
    flushColors();
    refreshStrip();
  }, 400);
}

async function fetchBlob(url) {
  const response = await fetch(url);
  if (!response.ok) {
    throw new P.PondError(response.status, (await response.text()).trim());
  }
  return response.blob();
}

function fileForm(fields) {
  const form = new FormData();
  for (const [name, value, fileName] of fields) {
    if (fileName) {
      form.append(name, value, fileName);
    } else {
      form.append(name, value);
    }
  }
  return form;
}

function putFile(key, path, blob, name) {
  return P.request("PUT", `/api/user_themes/${enc(key)}/${path}`, fileForm([["file", blob, name]]));
}

async function putSignals(key, style, interval, frames, blind) {
  const fields = [["style", style], ["interval", String(interval)]];
  for (const [blob, name] of frames) {
    fields.push(["frame", blob, name]);
  }
  if (blind) {
    fields.push(["blindspot", blind[0], blind[1]]);
  }
  await P.request("PUT", `/api/user_themes/${enc(key)}/signals`, fileForm(fields));
}

async function copyComponent(key, component, source) {
  if (component === "steering_wheels" && source === "none") {
    await P.request("PUT", `/api/user_themes/${enc(key)}/steering_wheels/none`);
    return;
  }
  if (source === "stock" || source === "none") {
    await P.request("DELETE", `/api/user_themes/${enc(key)}/${component}`);
    return;
  }
  if (component === "steering_wheels") {
    const list = await fileList(component, source);
    await putFile(key, "steering_wheels", await fetchBlob(fileUrl(component, source, list[0])), list[0]);
    return;
  }
  await P.request("POST", `/api/user_themes/${enc(key)}/${component}/copy`, { source });
}

async function copySlot(key, component, slot, source) {
  const name = pickSlot(await fileList(component, source), slot);
  if (name) {
    await putFile(key, `${component}/${slot}`, await fetchBlob(fileUrl(component, source, name)), name);
  } else {
    await P.request("DELETE", `/api/user_themes/${enc(key)}/${component}/${slot}`);
  }
}

function cleanName(name) {
  return name.replace(/'/g, "").replace(/[^A-Za-z0-9 ]+/g, " ").replace(/\s+/g, " ").trim();
}

function uniqueName(base) {
  const taken = new Set(themes.filter(t => t.user).map(t => t.key));
  const root = cleanName(base) || "My Theme";
  const fit = suffix => `${root.slice(0, userThemes.limits.name_length - suffix.length).trim()}${suffix}`;
  let name = fit(" Copy");
  for (let n = 2; taken.has(keyFromName(name)); n++) {
    name = fit(` Copy ${n}`);
  }
  return name;
}

async function createFrom(name, source) {
  if (source === "active") {
    return (await P.request("POST", "/api/user_themes", { name, source })).key;
  }
  if (source.user) {
    return (await P.request("POST", `/api/user_themes/${enc(source.key)}/duplicate`, { name })).key;
  }
  const { key } = await P.request("POST", "/api/user_themes", { name, source: source.key });
  for (const component of COMPONENTS) {
    const part = source.parts[component];
    if (part !== source.key && part !== fallback(component)) {
      await copyComponent(key, component, part);
    }
  }
  return key;
}

function openTheme(key) {
  T.made.add(key);
  T.current = key;
  P.setParam("theme", key);
  return P.refresh();
}

function nameDialog({ icon, title, confirm, value = "", extra = "" }) {
  return P.dialog({
    icon, title, confirm,
    body: `${extra}<label class="field"><span>Name</span><input class="input" data-name value="${esc(value)}" maxlength="${userThemes.limits.name_length}" autocomplete="off"></label>`,
    onOpen: dl => {
      const input = dl.querySelector("[data-name]");
      setTimeout(() => {
        input.focus();
        input.select();
      }, 0);
    }
  });
}

let pendingEdit = null;

async function edit(apply) {
  if (cur().user) {
    apply(false);
    return;
  }
  const offered = pendingEdit;
  pendingEdit = apply;
  if (offered) {
    return;
  }
  const copied = await copyBuiltIn();
  const latest = pendingEdit;
  pendingEdit = null;
  if (copied) {
    latest(true);
  } else {
    rebuild();
  }
}

async function copyBuiltIn(title = `Make your own copy of ${esc(cur().name)}?`, why = "Built-in themes can't be changed, so your change goes on a copy of your own.") {
  const t = cur();
  const d = await nameDialog({
    icon: "copy", title, confirm: "Make a copy", value: uniqueName(t.name),
    extra: `<p>${why} ${esc(t.name)} stays as it is.</p>`
  });
  if (!d) {
    return false;
  }
  const name = d.querySelector("[data-name]").value.trim();
  const handle = P.job({ label: `Copying ${esc(t.name)}` });
  let key;
  try {
    key = await replicaBusy(enqueue(() => createFrom(name, t)));
  } catch (error) {
    handle.fail(error, `Couldn't copy ${esc(t.name)}`, MESSAGES);
    return false;
  }
  handle.done(`Made “${esc(name)}”`, "Edit away; the original is untouched");
  await replicaBusy(openTheme(key));
  return true;
}

function near(button) {
  return button.closest(".menu-wrap")?.querySelector("[data-act='menu']") || button;
}

function showButton(button) {
  T.button = button;
  P.setParam("button", button);
  document.querySelectorAll("[data-btn]").forEach(x => x.setAttribute("aria-pressed", String(x.dataset.btn === button)));
}

function copyVersion(source) {
  return source.endsWith(USER_SUFFIX) ? undefined : source;
}

function signalsVersion() {
  return versions.get(`signals/${T.current}`) || loadedAt;
}

function chooseOption(kind, value) {
  if (kind === "signals" && value !== "none" && !T.signal) {
    T.signal = "left";
  }
  const control = `.choice[data-choice="${kind}"][data-value="${CSS.escape(value)}"]`;
  if (kind === "wheel") {
    mutate({ component: "steering_wheels", control, failTitle: "Couldn't change the steering wheel", label: "Changing the steering wheel", work: key => copyComponent(key, "steering_wheels", value) });
  } else if (kind === "signals") {
    mutate({ component: "signals", control, failTitle: "Couldn't change the turn signals", label: "Changing the turn signals", version: copyVersion(value), work: key => copyComponent(key, "signals", value) });
  } else if (kind === "distance") {
    mutate({ component: "distance_icons", control, failTitle: "Couldn't change the personality icons", label: "Changing the personality icons", version: copyVersion(value), work: key => copyComponent(key, "distance_icons", value) });
  } else {
    mutate({ component: "icons", control, failTitle: "Couldn't change the button", label: "Changing the button", work: key => copySlot(key, "icons", `button_${kind.slice(5)}`, value) });
  }
}

let msTimer = 0;

function setSignalMs(ms) {
  look.signals.ms = ms;
  const slider = document.querySelector("[data-ms]");
  if (slider) {
    slider.value = ms;
  }
  const out = document.querySelector("[data-ms-out]");
  if (out) {
    out.textContent = `${ms}${P.NB}ms`;
  }
  if (!T.signal) {
    T.signal = "left";
  }
  updateSignal(true);
  clearTimeout(msTimer);
  msTimer = setTimeout(() => {
    mutate({ component: "signals", control: "[data-ms-out]", failTitle: "Couldn't save the turn signals", label: "Saving the turn signal timing", version: signalsVersion(), work: key => putSignals(key, look.signals.style, ms, [], null) });
  }, 500);
}

function setSignalStyle(style) {
  if (!T.signal) {
    T.signal = "left";
  }
  look.signals.style = style;
  document.querySelectorAll("[data-style]").forEach(x => x.setAttribute("aria-pressed", String(x.dataset.style === style)));
  updateSignal(true);
  mutate({ component: "signals", control: `[data-style="${style}"]`, failTitle: "Couldn't save the turn signals", label: "Saving the turn signal style", version: signalsVersion(), work: key => putSignals(key, style, look.signals.ms, [], null) });
}

function copySoundPack(source) {
  const name = esc(packName("sounds", source));
  mutate({ component: "sounds", control: "[data-sound-pack] .dd-btn", done: `Sounds now start from ${name}`, failTitle: "Couldn't copy the sounds", label: `Copying ${name} sounds`, version: copyVersion(source), work: key => copyComponent(key, "sounds", source) });
}

function uploadFiles(kind, files) {
  const label = files.length > 1 ? `${files.length} files` : esc(files[0].name);
  let component;
  let work;
  if (kind === "wheel") {
    component = "steering_wheels";
    work = key => putFile(key, "steering_wheels", files[0], files[0].name);
  } else if (kind === "signals") {
    const blindFile = files.find(f => /blindspot/i.test(f.name));
    const frames = files.filter(f => f !== blindFile).map(f => [f, f.name]);
    component = "signals";
    work = key => putSignals(key, look.signals.style || "traditional", look.signals.ms, frames, blindFile ? [blindFile, blindFile.name] : null);
    T.signal = T.signal || "left";
  } else if (kind.startsWith("icon-")) {
    component = "icons";
    work = key => putFile(key, `icons/button_${kind.slice(5)}`, files[0], files[0].name);
  } else if (kind === "distance") {
    component = "distance_icons";
    work = key => putFile(key, `distance_icons/${T.personality}`, files[0], files[0].name);
  } else {
    component = "sounds";
    work = key => putFile(key, `sounds/${kind.slice(6)}`, files[0], files[0].name);
  }
  mutate({ component, control: `label:has(> [data-upload="${kind}"])`, done: `${label} added`, doneDetail: "Saved with this theme", failTitle: `Couldn't add ${label}`, label: `Adding ${label}`, work });
}

function mount(root) {
  const replica = root.querySelector("#replica");
  const stage = root.querySelector("#stage");
  buildStage();
  paint(root);
  const topbar = document.getElementById("topbar");
  const scaleObs = new ResizeObserver(() => {
    stage.style.transform = `scale(${replica.clientWidth / 2160})`;
    placeTapTargets();
    document.documentElement.style.setProperty("--topbar-h", `${topbar.offsetHeight}px`);
  });
  scaleObs.observe(replica);
  scaleObs.observe(topbar);
  stage.style.transform = `scale(${replica.clientWidth / 2160})`;

  const toSurface = e => {
    const g = surfaceGeom();
    const rect = replica.getBoundingClientRect();
    const scale = rect.width / 2160;
    return [(e.clientX - rect.left) / scale - g.ox, (e.clientY - rect.top) / scale - 30, g];
  };
  const hitAt = (x, y) => ["lead", "edges", "path", "lanes"].find(part => shapes[part] && ctx.isPointInPath(shapes[part].hit, x, y)) || null;
  const hitTest = (e, touch) => {
    const [x, y, g] = toSurface(e);
    if (x < 0 || y < 0 || x > g.W || y > g.H || !ctx) {
      return null;
    }
    const exact = hitAt(x, y);
    if (exact || !touch) {
      return exact;
    }
    const r = 22 / (replica.clientWidth / 2160);
    for (const k of [.5, 1]) {
      for (let a = 0; a < 8; a++) {
        const near = hitAt(x + Math.cos(a * Math.PI / 4) * r * k, y + Math.sin(a * Math.PI / 4) * r * k);
        if (near) {
          return near;
        }
      }
    }
    return null;
  };
  let raf = 0;
  stage.addEventListener("pointermove", e => {
    if (e.target.closest(".hit")) {
      if (hover) {
        hover = null;
        draw();
      }
      return;
    }
    const h = hitTest(e);
    stage.querySelector("#surface").style.cursor = h ? "pointer" : "default";
    if (h !== hover) {
      hover = h;
      cancelAnimationFrame(raf);
      raf = requestAnimationFrame(draw);
    }
  });
  stage.addEventListener("pointerleave", () => {
    hover = null;
    draw();
  });
  stage.addEventListener("click", e => {
    const hitEl = e.target.closest("[data-part]");
    if (hitEl) {
      selectPart(hitEl.dataset.part);
      return;
    }
    if (e.target.closest(".sb") || !e.target.closest("[data-surface]")) {
      return;
    }
    const h = hitTest(e, e.pointerType === "touch" || matchMedia("(max-width: 719px), (pointer: coarse)").matches);
    if (h) {
      selectPart(h);
      return;
    }
    setSidebar(!T.sidebar);
  });
  stage.addEventListener("focusin", e => {
    if (e.target.dataset.kbd) {
      hover = e.target.dataset.kbd;
      draw();
    }
  });
  stage.addEventListener("focusout", e => {
    if (e.target.dataset.kbd) {
      hover = null;
      draw();
    }
  });

  root.querySelector("#tap-layer").addEventListener("click", e => {
    const b = e.target.closest("[data-tap]");
    if (b) {
      selectPart(b.dataset.tap);
    }
  });
  root.querySelector("#strip").addEventListener("click", e => {
    const play = e.target.closest("[data-play-sound]");
    if (play) {
      playSound(play.dataset.playSound);
      return;
    }
    const b = e.target.closest(".slot-btn");
    if (b) {
      selectPart(b.dataset.part, b.dataset.sound, b);
    }
  });

  const controls = root.querySelector("#tm-controls");
  controls.addEventListener("change", e => {
    if (e.target.matches("[data-sidebar]")) {
      setSidebar(e.target.checked);
    }
  });
  controls.addEventListener("click", e => {
    const b = e.target.closest("[data-btn]");
    if (b) {
      T.button = b.dataset.btn;
      P.setParam("button", T.button);
      controls.querySelectorAll("[data-btn]").forEach(x => x.setAttribute("aria-pressed", String(x === b)));
      if (T.sidebar) {
        buildStage();
      } else {
        setSidebar(true);
      }
    }
    const d = e.target.closest("[data-drive]");
    if (d) {
      T.drive = d.dataset.drive;
      controls.querySelectorAll("[data-drive]").forEach(x => x.setAttribute("aria-pressed", String(x === d)));
      buildStage();
      if (T.part === "edges") {
        renderInspector();
      }
    }
  });

  const insp = root.querySelector("#inspector");
  insp.addEventListener("input", e => {
    const ed = e.target.closest("[data-color]");
    if (ed) {
      const key = ed.dataset.color;
      const kind = e.target.dataset.in;
      let patch = null;
      if (kind === "picker") {
        patch = { hex: e.target.value.toUpperCase() };
      } else if (kind === "alpha") {
        patch = { a: Number(e.target.value) };
      } else if (kind === "hex") {
        let v = e.target.value.trim();
        if (!v.startsWith("#")) {
          v = "#" + v;
        }
        const ok = /^#[0-9a-f]{6}$/i.test(v);
        e.target.setAttribute("aria-invalid", String(!ok && v.length >= 7));
        if (ok) {
          patch = { hex: v.toUpperCase() };
        }
      }
      if (patch) {
        edit(copied => editColor(key, patch, copied ? "" : kind));
      }
      return;
    }
    if (e.target.matches("[data-ms]")) {
      const ms = Number(e.target.value);
      edit(() => setSignalMs(ms));
    }
  });
  insp.addEventListener("change", e => {
    if (e.target.matches("[data-blind]")) {
      T.blind = e.target.checked;
      if (!T.signal) {
        T.signal = "left";
      }
      updateSignal(true);
    }
    if (e.target.matches("[data-show-pers]")) {
      T.showPersonality = e.target.checked;
      buildStage();
    }
    if (e.target.matches("[data-sound-pack]") && e.target.dataset.value) {
      const source = e.target.dataset.value;
      edit(() => copySoundPack(source));
    }
    const up = e.target.closest("[data-upload]");
    if (up && up.files.length) {
      const kind = up.dataset.upload;
      const files = [...up.files].sort((a, b) => a.name.localeCompare(b.name));
      up.value = "";
      edit(() => uploadFiles(kind, files));
    }
  });
  insp.addEventListener("click", e => {
    const preset = e.target.closest("[data-preset]");
    if (preset) {
      const key = preset.closest("[data-color]").dataset.color;
      edit(() => editColor(key, { hex: preset.dataset.preset }, "preset"));
      return;
    }
    const play = e.target.closest("[data-play-sound]");
    if (play) {
      T.soundFocus = play.dataset.playSound;
      insp.querySelectorAll("[data-snd-row]").forEach(r => r.classList.toggle("focus", r.dataset.sndRow === T.soundFocus));
      playSound(play.dataset.playSound);
      return;
    }
    const remove = e.target.closest("[data-remove-slot]");
    if (remove) {
      const [component, slot] = remove.dataset.removeSlot.split("/");
      const done = component === "sounds" ? "The stock sound plays in that slot now" : "Removed from this theme";
      mutate({ component, control: `[data-remove-slot="${remove.dataset.removeSlot}"]`, done, failTitle: "Couldn't remove it", label: "Removing it", work: key => P.request("DELETE", `/api/user_themes/${enc(key)}/${component}/${slot}`) });
      return;
    }
    const ch = e.target.closest("[data-choice]");
    if (ch) {
      const kind = ch.dataset.choice;
      const v = ch.dataset.value;
      if (kind.startsWith("icon-") && kind !== "icon-settings") {
        showButton(kind.slice(5));
      }
      if (v === "own" || ch.getAttribute("aria-pressed") === "true") {
        buildStage();
      } else {
        edit(() => chooseOption(kind, v));
      }
      return;
    }
    const style = e.target.closest("[data-style]");
    if (style) {
      if (style.getAttribute("aria-pressed") !== "true") {
        edit(() => setSignalStyle(style.dataset.style));
      }
      return;
    }
    const pers = e.target.closest("[data-pers]");
    if (pers) {
      T.personality = pers.dataset.pers;
      T.showPersonality = true;
      buildStage();
      renderInspector();
    }
  });

  root.querySelector("[data-theme-pick]").addEventListener("change", async e => {
    const key = e.currentTarget.dataset.value;
    const button = e.currentTarget.querySelector(".dd-btn");
    await flushColors();
    T.current = key;
    P.setParam("theme", T.current);
    P.busy(button, P.refresh());
  });
  updateSignal(true);
  return () => {
    clearInterval(sigTimer);
    clearTimeout(msTimer);
    scaleObs.disconnect();
    flushColors();
    stopSound();
  };
}

P.page("theme", {
  title: "Theme Maker",
  async render(params) {
    await loadData();
    const wanted = params.get("theme");
    if (wanted && themes.some(t => t.key === wanted)) {
      T.current = wanted;
    } else if (!themes.some(t => t.key === T.current)) {
      T.current = (themes.find(t => t.user) || themes.find(t => t.parts.colors === data.components.colors.selected) || themes[0]).key;
    }
    const sp = params.get("sidebar");
    if (sp) {
      T.sidebar = sp === "open";
    }
    if (params.get("button") === "flag" || params.get("button") === "home") {
      T.button = params.get("button");
    }
    if (params.get("part") && META[params.get("part")]) {
      T.part = params.get("part");
    }
    if (T.part === "signals" && !T.signal) {
      T.signal = "left";
    }
    const t = cur();
    look = await loadLook(t);
    await loadPacks(PART_COMPONENT[T.part]);
    const builtIn = t.user ? "" : 'aria-disabled="true"';
    const builtInNote = t.user ? "" : "<small>Built in</small>";
    return {
      title: "Theme Maker",
      actions: `<button class="btn btn-quiet btn-small" data-act="tm-submit">${ic("send")}<span>Submit<span class="lbl-long"> theme</span></span></button>
        <button class="btn btn-primary btn-small" data-act="tm-apply">${ic("check")}<span>Apply<span class="lbl-long"> to car</span></span></button>`,
      body: `
        <div class="tm-head">
          <label class="field"><span>Editing</span>${P.dropdown({ value: t.key, options: themes.map(x => [x.key, `${x.name}${x.user ? "" : " (built in)"}`]), label: "Editing", attrs: "data-theme-pick" })}</label>
          <div class="menu-wrap">
            <button class="btn btn-quiet btn-small" data-act="menu" aria-expanded="false" aria-haspopup="true">Theme${ic("down", "ic-s")}</button>
            <div class="menu tm-menu" hidden role="menu" aria-label="Theme">
              <button class="menu-item" role="menuitem" data-act="tm-new">${ic("plus")}<span>New theme</span></button>
              <button class="menu-item" role="menuitem" data-act="tm-import">${ic("upload")}<span>Import</span></button>
              <button class="menu-item" role="menuitem" data-act="tm-rename" ${builtIn}>${ic("pencil")}<span>Rename</span>${builtInNote}</button>
              <button class="menu-item" role="menuitem" data-act="tm-duplicate">${ic("copy")}<span>Duplicate</span></button>
              <button class="menu-item" role="menuitem" data-act="tm-export" ${builtIn}>${ic("download")}<span>Export</span>${builtInNote}</button>
              <button class="menu-item" role="menuitem" data-act="tm-delete" ${builtIn}>${ic("trash")}<span>Delete</span>${builtInNote}</button>
            </div>
          </div>
          ${t.user ? "" : `<button class="btn btn-primary btn-small" data-act="tm-duplicate">${ic("copy")}Make my own copy</button>`}
          <span class="grow"></span>
          <span class="tm-using" id="tm-using">The car is using <b>${esc(usingName())}</b></span>
        </div>
        <div class="tm-grid">
          <div class="tm-stage-col">
            <div class="replica" id="replica" aria-label="Driving screen preview. Pick a part to edit it."><div class="stage" id="stage"></div><div class="tap-layer" id="tap-layer" aria-hidden="true"></div></div>
            <div class="tm-controls" id="tm-controls">${controlsHTML()}</div>
            <section class="panel panel-pad strip" id="strip" aria-label="Everything you can theme">${stripHTML()}</section>
          </div>
          <aside class="panel inspector" id="inspector" aria-labelledby="insp-h">${inspectorHTML()}</aside>
        </div>`,
      mount
    };
  }
});

P.act("tm-signal", b => {
  T.signal = T.signal === b.dataset.side ? null : b.dataset.side;
  updateSignal(true);
});

P.act("tm-duplicate", async button => {
  const t = cur();
  await flushColors();
  const name = uniqueName(t.name);
  const handle = P.job({ label: `Copying ${esc(t.name)}` });
  try {
    const key = await P.busy(near(button), enqueue(() => createFrom(name, t)));
    handle.done(`Made “${esc(name)}”`, "Edit away; the original is untouched");
    openTheme(key);
  } catch (error) {
    handle.fail(error, `Couldn't copy ${esc(t.name)}`, MESSAGES);
    P.refresh();
  }
});

P.act("tm-new", async button => {
  await flushColors();
  const sources = [["active", "What the car shows now"], ...themes.filter(t => !t.user && t.key !== "stock").map(t => [t.key, t.name])];
  const d = await nameDialog({
    icon: "plus", title: "New theme", confirm: "Make theme",
    extra: `<label class="field"><span>Start from</span>${P.dropdown({ value: "active", options: sources, label: "Start from", attrs: "data-new-source" })}</label>`
  });
  if (!d) {
    return;
  }
  const name = d.querySelector("[data-name]").value.trim();
  const sourceKey = d.querySelector("[data-new-source]").dataset.value;
  const source = sourceKey === "active" ? "active" : themes.find(t => t.key === sourceKey);
  const handle = P.job({ label: `Making ${esc(name || "the theme")}` });
  try {
    const key = await P.busy(near(button), enqueue(() => createFrom(name, source)));
    handle.done(`Made “${esc(name)}”`);
    openTheme(key);
  } catch (error) {
    handle.fail(error, "Couldn't make the theme", MESSAGES);
  }
});

P.act("tm-import", async button => {
  await flushColors();
  const d = await nameDialog({
    icon: "upload", title: "Import a theme", confirm: "Import theme",
    extra: `<label class="field"><span>Theme file</span><input class="input" type="file" accept=".zip,application/zip" data-import-file></label>`
  });
  if (!d) {
    return;
  }
  const file = d.querySelector("[data-import-file]").files[0];
  const name = d.querySelector("[data-name]").value.trim() || (file ? cleanName(file.name.replace(/\.zip$/i, "")).slice(0, userThemes.limits.name_length) : "");
  if (!file) {
    P.toast("Couldn't import the theme", "Choose the theme's .zip file", "alert");
    return;
  }
  const handle = P.job({ label: `Importing ${P.fileLabel(file.name)}` });
  try {
    const { key } = await P.busy(near(button), enqueue(() => P.request("POST", "/api/user_themes/import", fileForm([["name", name], ["file", file, file.name]]))));
    handle.done(`Imported “${esc(name)}”`);
    openTheme(key);
  } catch (error) {
    handle.fail(error, "Couldn't import the theme", MESSAGES);
  }
});

P.act("tm-rename", async button => {
  const t = cur();
  if (!t.user) {
    await flushColors();
    copyBuiltIn(undefined, "Built-in themes keep their names, so the new name goes on a copy of your own.");
    return;
  }
  await flushColors();
  const d = await nameDialog({ icon: "pencil", title: "Rename theme", confirm: "Save name", value: t.name });
  if (!d) {
    return;
  }
  const v = d.querySelector("[data-name]").value.trim();
  if (!v || v === t.name) {
    return;
  }
  try {
    const { key } = await P.busy(near(button), enqueue(() => P.request("PUT", `/api/user_themes/${enc(t.key)}/name`, { name: v })));
    T.made.delete(t.key);
    P.toast(`Renamed to “${esc(v)}”`);
    openTheme(key);
  } catch (error) {
    P.fail(error, "Couldn't rename the theme", MESSAGES);
  }
});

P.act("tm-export", async () => {
  const t = cur();
  if (!t.user) {
    P.toast("Make your own copy to export it", "", "info");
    return;
  }
  await flushColors();
  await P.download(`/api/user_themes/${enc(t.key)}/export`, `${t.name}.zip`);
});

P.act("tm-delete", async button => {
  const t = cur();
  if (!t.user) {
    P.toast("Built-in themes can't be deleted", "", "info");
    return;
  }
  const ok = await P.dialog({
    icon: "trash", tone: "danger", confirmTone: "danger", title: `Delete “${esc(t.name)}”?`,
    body: "<p>Its colors, images, and sounds are removed from the device. This can't be undone.</p>",
    confirm: "Delete theme"
  });
  if (!ok) {
    return;
  }
  await flushColors();
  try {
    await P.busy(near(button), enqueue(() => P.request("DELETE", `/api/user_themes/${enc(t.key)}`)));
  } catch (error) {
    P.fail(error, `Couldn't delete ${esc(t.name)}`, MESSAGES);
    return;
  }
  for (const component of COMPONENTS) {
    forget(t.key, component);
  }
  T.made.delete(t.key);
  T.current = "";
  P.setParam("theme", "");
  P.toast(`Deleted ${esc(t.name)}`);
  P.refresh();
});

P.act("tm-reset-part", () => {
  const t = cur();
  if (!t.user) {
    return;
  }
  const part = T.part;
  const keys = PART_COLORS[part];
  const reset = { control: "[data-act='tm-reset-part']", done: `${META[part].title} reset to stock`, failTitle: "Couldn't reset it", label: `Resetting ${META[part].title.toLowerCase()}` };
  if (keys) {
    for (const key of keys) {
      look.colors[key] = toColor(look.stockColors[key]);
    }
    mutate({ ...reset, component: "colors", work: key => P.request("PUT", `/api/user_themes/${enc(key)}/colors`, colorsBody()) });
  } else {
    mutate({ ...reset, component: PART_COMPONENT[part], work: key => P.request("DELETE", `/api/user_themes/${enc(key)}/${PART_COMPONENT[part]}`) });
  }
});

P.act("tm-apply", async button => {
  const t = cur();
  const notes = [];
  if (!t.user && !data.custom_themes) {
    notes.push("Custom Themes is off on the car, so it keeps the stock look until you turn it on.");
  }
  if (data.holiday_themes) {
    notes.push("With Holiday Themes on, a holiday's theme still takes over for its week, then yours comes back.");
  }
  const ok = await P.dialog({
    icon: "theme", title: `Use “${esc(t.name)}” on the car?`,
    body: `<p>Its colors, steering wheel, turn signals, sidebar buttons, personality icons, and sounds replace <b>${esc(usingName())}</b> on the driving screen right away.</p>
      ${notes.map(note => `<div class="notice">${ic("info")}<span>${note}</span></div>`).join("")}`,
    confirm: "Apply to car"
  });
  if (!ok) {
    return;
  }
  await flushColors();
  const handle = P.job({ label: `Applying ${esc(t.name)}` });
  try {
    await P.busy(button, enqueue(async () => {
      if (t.user) {
        await P.request("POST", `/api/user_themes/${enc(t.key)}/apply`);
        return;
      }
      for (const component of COMPONENTS) {
        await P.request("PUT", "/api/themes/selection", { component, key: t.parts[component] });
      }
    }));
    handle.done(`The car is now using ${esc(t.name)}`);
  } catch (error) {
    handle.fail(error, `Couldn't apply ${esc(t.name)}`, MESSAGES);
  }
  await reloadCurrent(t.key);
});

P.act("tm-submit", async button => {
  const t = cur();
  if (!t.user) {
    await flushColors();
    copyBuiltIn("Make your own copy to submit", `Everyone already has ${esc(t.name)}, so make your own copy, change it, then submit that.`);
    return;
  }
  const d = await P.dialog({
    icon: "send", title: "Submit your theme",
    body: `<p>Submit it so it can be offered with the downloadable themes.</p>
      <label class="field"><span>Theme name</span><input class="input" data-name value="${esc(t.name)}" maxlength="${userThemes.limits.name_length}" autocomplete="off"></label>
      <label class="field"><span>Your Discord name</span><input class="input" data-sub-discord autocomplete="off" maxlength="${userThemes.limits.username_length}"></label>`,
    confirm: "Submit theme",
    requireCheck: "I made these images and sounds, or I'm allowed to share them"
  });
  if (!d) {
    return;
  }
  const name = d.querySelector("[data-name]").value.trim();
  const discord = d.querySelector("[data-sub-discord]").value.trim();
  await flushColors();
  let renamed = "";
  const handle = P.job({ label: `Submitting ${esc(name)}` });
  try {
    await P.busy(button, enqueue(async () => {
      if (keyFromName(name) !== t.key) {
        renamed = (await P.request("PUT", `/api/user_themes/${enc(t.key)}/name`, { name })).key;
      }
      await P.request("POST", `/api/user_themes/${enc(renamed || t.key)}/submit`, { discord_username: discord });
    }));
    handle.done("Theme submitted", "Thanks for sharing it");
  } catch (error) {
    handle.fail(error, "Couldn't submit the theme", MESSAGES);
  }
  if (renamed) {
    T.made.delete(t.key);
    openTheme(renamed);
  }
});
})();
