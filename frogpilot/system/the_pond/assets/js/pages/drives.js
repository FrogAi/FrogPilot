(() => {
"use strict";

const P = window.Pond;

const CAMERAS = [
  { file: "Forward camera", id: "forward", label: "Forward" },
  { file: "Wide camera", id: "wide", label: "Wide" },
  { file: "Driver camera", id: "driver", label: "Driver" },
  { file: "Low-res camera", id: "low", label: "Low-res" }
];
const CEM_REASONS = {
  curve: "curve",
  lead: "slower lead",
  signal: "turn signal",
  speed: "low speed",
  speed_limit: "speed limit",
  stop_light: "stop light",
  user_disabled: "turned off by driver",
  user_overridden: "turned on by driver"
};
const DATES = [["any", "Any day"], ["today", "Today"], ["yesterday", "Yesterday"], ["week", "Last 7 days"], ["month", "Last 30 days"], ["day", "Pick a day…"]];
const DISENGAGE_REASONS = {
  buttonCancel: "cancel",
  parkBrake: "parking brake",
  pcmDisable: "cruise off",
  pedalPressed: "pedal",
  reverseGear: "reverse",
  steerDisengage: "steering",
  wrongCarMode: "car mode",
  wrongCruiseMode: "cruise mode"
};
const EVENT_KINDS = {
  alert: { icon: "alert", label: "Warnings" },
  bookmark: { icon: "bookmark", label: "Flags" },
  curve: { icon: "curve", label: "Curves" },
  disengaged: { icon: "x", label: "Disengaged" },
  experimental: { icon: "flask", label: "Experimental Mode" },
  gear: { icon: "vehicle", label: "Gear" },
  hard_brake: { icon: "brake", label: "Hard braking" },
  lane_change: { icon: "lane", label: "Lane changes" },
  personality: { icon: "person", label: "Personality" },
  road: { icon: "signpost", label: "Roads" },
  speed_limit: { icon: "gauge", label: "Speed limits" },
  stop: { icon: "stop", label: "Stops" }
};
const GROUP_LABELS = { largest: "Largest first", longest: "Longest first", name: "By name" };
const LOGS = [["qlog", "Logs (qlog)"], ["rlog", "Full logs (rlog)"]];
const MAP_STYLE = "mapbox://styles/frogsgomoo/cmcfv151j000o01rcdxebhl76";
const SAVING_CHECK_INTERVAL = 2000;
const THUMB_MAP_HEIGHT = 200;
const THUMB_MAP_WIDTH = 320;
const THUMB_RETRIES = 3;
const THUMB_RETRY_DELAY = 2000;

const hiddenKinds = new Set(["gear"]);
const lengths = new Map();

let current = null;
let list = null;
let mapboxScript = null;
let player = null;
let previewing = null;
let theater = false;

function shortDur(seconds) {
  const m = Math.round(seconds / 60);
  return m < 60 ? `${m}${P.NB}min` : `${Math.floor(m / 60)}${P.NB}h ${String(m % 60).padStart(2, "0")}`;
}

function dayValue(t) {
  return P.fmt(t, { year: "numeric", month: "2-digit", day: "2-digit" }).replace(/(\d+)\/(\d+)\/(\d+)/, "$3-$1-$2");
}

function itemTitle(item) {
  return item.name || `${P.dayLabel(item.date)}, ${P.timeLabel(item.date)}`;
}

function flag(icon, label) {
  return `<span class="flag" title="${label}">${P.ic(icon)}<span class="sr">${label}</span></span>`;
}

function filtered(state) {
  const { f, kind } = state;
  const q = f.q.trim().toLowerCase();
  const today = P.dayIndex(P.now());
  const shown = state.items.filter(item => {
    if (q) {
      const hay = `${kind.search(item)} ${P.dayLabel(item.date)} ${P.fmt(item.date, { weekday: "long", month: "long", day: "numeric" })} ${P.timeLabel(item.date)}`.toLowerCase();
      if (!hay.includes(q)) {
        return false;
      }
    }
    for (const [key, , test] of kind.only) {
      if (f.only.has(key) && !test(item)) {
        return false;
      }
    }
    const age = today - P.dayIndex(item.date);
    if (f.date === "today" && age !== 0) {
      return false;
    }
    if (f.date === "yesterday" && age !== 1) {
      return false;
    }
    if (f.date === "week" && age > 6) {
      return false;
    }
    if (f.date === "month" && age > 29) {
      return false;
    }
    if (f.date === "day" && f.day && dayValue(item.date) !== f.day) {
      return false;
    }
    return true;
  });
  const sorters = {
    largest: (a, b) => b.size - a.size,
    longest: (a, b) => b.duration - a.duration,
    name: (a, b) => (a.name || "~").localeCompare(b.name || "~") || b.date - a.date,
    newest: (a, b) => b.date - a.date,
    oldest: (a, b) => a.date - b.date
  };
  return shown.sort(sorters[f.sort]);
}

function isFiltered(f) {
  return Boolean(f.q.trim() || f.only.size || f.date !== "any");
}

function filterLabel(f) {
  return f.only.size ? `Filter · ${f.only.size}` : "Filter";
}

function thumbHTML(src) {
  if (!src) {
    return `<span class="thumb-state" aria-hidden="true"><span class="spinner"></span></span>`;
  }
  return `<span class="thumb-state" aria-hidden="true"><span class="spinner"></span></span><img src="${src}" alt="" width="482" height="302" loading="lazy">`;
}

function drawRoute(thumb, path) {
  thumb.closest("[data-preview]")?.removeAttribute("data-preview");
  thumb.querySelectorAll(":scope > .thumb-state, :scope > img").forEach(element => element.remove());
  if (!path.length) {
    thumb.insertAdjacentHTML("afterbegin", `<span class="thumb-tile">${P.ic("drives")}</span>`);
    return;
  }
  thumb.insertAdjacentHTML("afterbegin", `<span class="thumb-map" aria-hidden="true"></span>`);
  svgMap(thumb.firstElementChild, path, THUMB_MAP_WIDTH, THUMB_MAP_HEIGHT).move(1);
}

async function loadRoute(thumb) {
  let path = [];
  try {
    path = (await P.get(`/api/routes/${encodeURIComponent(thumb.dataset.route)}/drive`)).path;
  } catch (error) {
    if (!(error instanceof P.PondError) || error.status !== 404) {
      P.fail(error, "Couldn't load the drive's route");
    }
  }
  if (thumb.isConnected) {
    drawRoute(thumb, path);
  }
}

function loadRoutes(root) {
  root.querySelectorAll(".thumb[data-route]:not(:has(> img))").forEach(loadRoute);
}

function watchThumbs(root, signal) {
  const settle = e => {
    const img = e.target;
    if (!img.matches(".thumb > img")) {
      return;
    }
    const thumb = img.parentElement;
    const retries = Number(img.dataset.retries || 0);
    if (e.type === "load") {
      thumb.classList.add("ready");
    } else if (retries < THUMB_RETRIES) {
      img.dataset.retries = String(retries + 1);
      setTimeout(() => {
        if (img.isConnected) {
          img.replaceWith(img.cloneNode());
        }
      }, THUMB_RETRY_DELAY);
    } else if (thumb.dataset.route) {
      loadRoute(thumb);
    } else {
      thumb.closest("[data-preview]")?.removeAttribute("data-preview");
      thumb.querySelectorAll(":scope > .thumb-state, :scope > img").forEach(element => element.remove());
      thumb.insertAdjacentHTML("afterbegin", `<span class="thumb-tile">${P.ic("recordings")}</span>`);
    }
  };
  root.addEventListener("error", settle, { capture: true, signal });
  root.addEventListener("load", settle, { capture: true, signal });
}

function watchSaving(state, signal) {
  const { items, kind } = state;
  const saving = items.find(item => kind.busy(item));
  if (!saving) {
    return;
  }
  const refresh = () => {
    if (!signal.aborted) {
      P.refresh();
    }
  };
  P.poll(async () => {
    if (signal.aborted) {
      return true;
    }
    if (document.hidden) {
      return;
    }
    const response = await fetch(kind.thumbnail(saving.id));
    if (response.status !== 202) {
      return true;
    }
  }, SAVING_CHECK_INTERVAL).then(refresh, refresh);
}

function favoriteButton(item, cls) {
  return `<button class="${cls} fav" type="button" data-act="drives-favorite" data-id="${P.esc(item.id)}" aria-pressed="${item.preserved}" title="Favorites are kept when space runs low">${P.ic("star")}<span class="${cls === "clip-act" ? "sr" : ""}">Favorite</span></button>`;
}

function clipCard(state, item) {
  const { kind, selected, selecting } = state;
  const title = item.name || P.timeLabel(item.date);
  const seconds = kind.length(item);
  const busy = kind.busy(item);
  const route = kind.routeMap && !busy ? `data-route="${P.esc(item.id)}"` : "";
  const thumbnail = kind.routeMap && !item.video ? "" : kind.thumbnail(item.id);
  const inner = `
      <span class="thumb" ${route}>${busy ? `<span class="thumb-state"><span class="spinner" aria-hidden="true"></span><span>${busy}</span></span>` : thumbHTML(thumbnail)}<span class="dur" data-length="${P.esc(item.id)}" ${seconds === undefined ? "hidden" : ""}>${seconds === undefined ? "" : kind.chip(seconds)}</span></span>
      <span class="clip-title">${P.esc(title)}</span>
      <span class="clip-meta">${[P.fmt(item.date, { weekday: "short", month: "short", day: "numeric" }), item.name ? P.timeLabel(item.date) : "", kind.meta(item)].filter(Boolean).join(" · ")}${kind.flags(item)}</span>`;
  return `<li class="clip" data-id="${P.esc(item.id)}" ${selecting ? `aria-selected="${selected.has(item.id)}"` : ""}>
    ${busy ? `<div class="clip-link clip-busy">${inner}</div>` : `<a class="clip-link" href="${kind.href(item.id)}" ${thumbnail ? `data-preview="${kind.preview(item.id)}"` : ""}>${inner}</a>
    <span class="clip-acts">${kind.favorite ? favoriteButton(item, "clip-act") : ""}<button class="clip-act" type="button" data-act="clips-delete-card" data-id="${P.esc(item.id)}" title="Delete">${P.ic("trash")}<span class="sr">Delete ${P.esc(title)}</span></button></span>`}
    <label class="clip-sel"><input type="checkbox" data-select="${P.esc(item.id)}" ${selected.has(item.id) ? "checked" : ""}><span class="tick" aria-hidden="true">${P.ic("check")}</span><span class="sr">Select ${P.esc(title)}</span></label>
  </li>`;
}

function clipsHTML(state) {
  const { f, items, kind, selecting } = state;
  if (!items.length) {
    return `<div class="empty">${kind.none}</div>`;
  }
  const shown = filtered(state);
  if (!shown.length) {
    return `<div class="empty"><h3>No ${kind.noun}s match</h3><button class="btn btn-quiet btn-small" data-act="clips-clear">Clear filters</button></div>`;
  }
  const grouped = f.sort === "newest" || f.sort === "oldest";
  const groups = [];
  if (grouped) {
    for (const item of shown) {
      const day = P.dayIndex(item.date);
      if (groups.at(-1)?.day !== day) {
        groups.push({ date: item.date, day, items: [], label: P.dayLabel(item.date) });
      }
      groups.at(-1).items.push(item);
    }
  } else {
    groups.push({ items: shown, label: GROUP_LABELS[f.sort] });
  }
  return groups.map(group => {
    const recent = grouped && P.dayIndex(P.now()) - group.day < 7;
    const bytes = group.items.reduce((sum, item) => sum + item.size, 0);
    return `<section class="clip-day">
      <div class="day-head"><h2>${group.label}</h2>${recent ? `<span>${P.fmt(group.date, { month: "short", day: "numeric" })}</span>` : ""}<span>· ${P.plural(group.items.length, kind.noun)} · ${P.size(bytes)}</span></div>
      <ul class="clips ${selecting ? "selecting" : ""}" aria-label="${group.label}">${group.items.map(item => clipCard(state, item)).join("")}</ul>
    </section>`;
  }).join("");
}

function listStatusHTML(state) {
  if (!isFiltered(state.f)) {
    return "";
  }
  return `<p class="result-line"><span>${filtered(state).length} of ${P.plural(state.items.length, state.kind.noun)}</span><button class="btn btn-text btn-small" data-act="clips-clear">Clear filters</button></p>`;
}

function selectBarHTML(state) {
  const { items, kind, selected, selecting } = state;
  if (!selecting) {
    return "";
  }
  const chosen = items.filter(item => selected.has(item.id));
  const bytes = chosen.reduce((sum, item) => sum + item.size, 0);
  const favorites = chosen.length > 0 && chosen.every(item => item.preserved);
  return `<div class="select-bar" role="region" aria-label="Selected ${kind.noun}s">
    <b>${chosen.length ? `${P.plural(chosen.length, kind.noun)} · ${P.size(bytes)}` : `Select ${kind.noun}s`}</b>
    <button class="btn btn-text btn-small" data-act="clips-select-all">Select all shown</button>
    <span class="grow"></span>
    <button class="btn btn-text btn-small" data-act="clips-select-done">Done</button>
    ${kind.favorite ? `<button class="btn btn-text btn-small" data-act="drives-favorite-selected" ${chosen.length ? "" : "disabled"}>${P.ic("star")}${favorites ? "Unfavorite" : "Favorite"}</button>` : ""}
    <button class="btn btn-danger btn-small" data-act="clips-delete-selected" ${chosen.length ? "" : "disabled"}>${P.ic("trash")}Delete${chosen.length ? ` ${P.plural(chosen.length, kind.noun)}` : ""}</button>
  </div>`;
}

function refreshList() {
  const content = document.getElementById("content");
  const { f } = list;
  stopPreview();
  content.querySelector("#clip-results").innerHTML = clipsHTML(list);
  loadRoutes(content);
  content.querySelector("#list-status").innerHTML = listStatusHTML(list);
  content.querySelector("#select-holder").innerHTML = selectBarHTML(list);
  const label = content.querySelector("[data-filter-label]");
  if (label) {
    label.textContent = filterLabel(f);
  }
  document.querySelector("[data-act='clips-select-toggle']")?.setAttribute("aria-pressed", String(list.selecting));
  P.setParam("q", f.q);
  P.setParam("sort", f.sort === "newest" ? "" : f.sort);
  P.setParam("date", f.date === "any" ? "" : f.date);
  P.setParam("day", f.date === "day" ? f.day : "");
  P.setParam("only", [...f.only].join(","));
}

function startPreview(link) {
  if (previewing?.link === link) {
    return;
  }
  stopPreview();
  const video = document.createElement("video");
  video.className = "clip-preview";
  video.muted = true;
  video.loop = true;
  video.playsInline = true;
  video.addEventListener("playing", () => video.classList.add("on"));
  video.addEventListener("error", () => stopPreview(link));
  video.src = link.dataset.preview;
  const thumb = link.querySelector(".thumb");
  thumb.insertBefore(video, thumb.querySelector(".dur"));
  previewing = { link, video };
  video.play().catch(error => {
    if (error.name !== "AbortError" && error.name !== "NotSupportedError") {
      throw error;
    }
  });
}

function stopPreview(link = previewing?.link) {
  if (!previewing || previewing.link !== link) {
    return;
  }
  const { video } = previewing;
  video.pause();
  video.removeAttribute("src");
  video.load();
  video.remove();
  previewing = null;
}

function watchPreviews(root, signal) {
  root.addEventListener("pointerover", e => {
    const link = e.target.closest("[data-preview]");
    if (e.pointerType === "mouse" && link && !link.contains(e.relatedTarget)) {
      startPreview(link);
    }
  }, { signal });
  root.addEventListener("pointerout", e => {
    const link = e.target.closest("[data-preview]");
    if (link && !link.contains(e.relatedTarget)) {
      stopPreview(link);
    }
  }, { signal });
  root.addEventListener("focusin", e => {
    const link = e.target.closest("[data-preview]");
    if (link) {
      startPreview(link);
    }
  }, { signal });
  root.addEventListener("focusout", e => {
    const link = e.target.closest("[data-preview]");
    if (link) {
      stopPreview(link);
    }
  }, { signal });
  signal.addEventListener("abort", () => stopPreview());
}

function videoLength(src) {
  return new Promise(resolve => {
    const video = document.createElement("video");
    video.preload = "metadata";
    video.onloadedmetadata = () => {
      video.onerror = null;
      resolve(video.duration);
      video.removeAttribute("src");
      video.load();
    };
    video.onerror = () => resolve(undefined);
    video.src = src;
  });
}

async function loadLengths(state, signal) {
  const { kind } = state;
  for (const item of state.items) {
    if (kind.length(item) !== undefined) {
      continue;
    }
    const seconds = await videoLength(kind.video(item.id));
    if (signal.aborted) {
      return;
    }
    if (!Number.isFinite(seconds)) {
      continue;
    }
    lengths.set(item.key, seconds);
    document.querySelectorAll(`[data-length="${CSS.escape(item.id)}"]`).forEach(chip => {
      chip.textContent = kind.chip(seconds);
      chip.hidden = false;
    });
  }
}

function mountList(root, state) {
  list = state;
  const { f, kind } = state;
  const listening = new AbortController();
  const signal = listening.signal;
  let typing = 0;
  root.querySelector("[data-list-q]")?.addEventListener("input", e => {
    clearTimeout(typing);
    typing = setTimeout(() => {
      f.q = e.target.value;
      refreshList();
    }, 120);
  });
  root.addEventListener("change", e => {
    const target = e.target;
    if (target.matches("[data-list-sort]")) {
      f.sort = target.dataset.value;
    } else if (target.matches("[data-list-date]")) {
      f.date = target.dataset.value;
      const day = root.querySelector("[data-list-day]");
      day.hidden = f.date !== "day";
      if (f.date === "day") {
        if (!f.day) {
          f.day = dayValue(P.now());
        }
        day.value = f.day;
        day.focus();
      }
    } else if (target.matches("[data-list-day]")) {
      f.day = target.value;
    } else if (target.matches("[data-only]")) {
      if (target.checked) {
        f.only.add(target.dataset.only);
      } else {
        f.only.delete(target.dataset.only);
      }
    } else if (target.matches("[data-select]")) {
      if (target.checked) {
        state.selected.add(target.dataset.select);
      } else {
        state.selected.delete(target.dataset.select);
      }
      target.closest(".clip").setAttribute("aria-selected", String(target.checked));
      root.querySelector("#select-holder").innerHTML = selectBarHTML(state);
      return;
    } else {
      return;
    }
    refreshList();
  }, { signal });
  root.addEventListener("click", e => {
    const link = e.target.closest(".clip-link");
    if (!state.selecting || !link) {
      return;
    }
    e.preventDefault();
    e.stopPropagation();
    const box = link.closest(".clip").querySelector("[data-select]");
    box.checked = !box.checked;
    box.dispatchEvent(new Event("change", { bubbles: true }));
  }, { capture: true, signal });
  watchPreviews(root, signal);
  watchThumbs(root, signal);
  loadRoutes(root);
  watchSaving(state, signal);
  if (kind.video) {
    loadLengths(state, signal);
  }
  return () => {
    listening.abort();
    clearTimeout(typing);
    list = null;
  };
}

async function listPage(kind, params) {
  const items = (await P.get(kind.api)).map(kind.item);
  const sort = params.get("sort");
  const date = params.get("date");
  const state = {
    f: {
      date: DATES.some(([value]) => value === date) ? date : "any",
      day: params.get("day") || "",
      only: new Set((params.get("only") || "").split(",").filter(key => kind.only.some(([only]) => only === key))),
      q: params.get("q") || "",
      sort: kind.sorts.some(([value]) => value === sort) ? sort : "newest"
    },
    items,
    kind,
    selected: new Set(),
    selecting: false
  };
  const f = state.f;
  const has = items.length > 0;
  return {
    actions: has ? `<button class="btn btn-quiet btn-small" data-act="clips-select-toggle" aria-pressed="false">Select</button>` : "",
    body: `
      ${has ? `<div class="toolbar" role="search">
        <label class="search"><span class="sr">Search ${kind.label.toLowerCase()}</span>${P.ic("search")}<input class="input" type="search" data-list-q placeholder="${kind.searchHint}" value="${P.esc(f.q)}" autocomplete="off"></label>
        <div class="tb-when">${P.dropdown({ attrs: "data-list-date", label: "When", options: DATES, value: f.date })}</div>
        <div class="tb-sort">${P.dropdown({ attrs: "data-list-sort", label: "Sort", options: kind.sorts, value: f.sort })}</div>
        <div class="menu-wrap tb-filter">
          <button class="btn btn-quiet filter-btn" data-act="menu" aria-expanded="false" aria-haspopup="true"><span data-filter-label>${filterLabel(f)}</span>${P.ic("down", "ic-s")}</button>
          <div class="menu" hidden aria-label="Show only">
            <h3>Show only</h3>
            ${kind.only.map(([key, label]) => `<label class="menu-item check-item"><input type="checkbox" data-only="${key}" ${f.only.has(key) ? "checked" : ""}><span>${label}</span></label>`).join("")}
          </div>
        </div>
        <input class="input day-pick" type="date" data-list-day value="${P.esc(f.day)}" max="${dayValue(P.now())}" aria-label="Day" ${f.date === "day" ? "" : "hidden"}>
      </div>` : ""}
      <div id="list-status" aria-live="polite">${listStatusHTML(state)}</div>
      <div id="clip-results">${clipsHTML(state)}</div>
      <div id="select-holder"></div>`,
    mount: root => mountList(root, state)
  };
}

function scaleHTML(isDrive, date, duration) {
  if (!duration) {
    return "";
  }
  return [0, 1, 2, 3, 4].map(i => `<span>${isDrive ? P.timeLabel(date + (duration * i) / 4) : P.clock((duration * i) / 4)}</span>`).join("");
}

function playerHTML({ audio = false, camera, cameras, date, duration, noun, partLength, poster }) {
  const isDrive = Boolean(cameras);
  const step = isDrive ? partLength : 10;
  const stepWords = isDrive ? "one minute" : "10 seconds";
  const speeds = isDrive ? [1, 4, 16] : [1, 2, 4];
  return `
    <div class="player ${isDrive ? "" : "screen"}" id="player">
      <video class="frame on" playsinline preload="metadata" poster="${poster}"></video>
      ${isDrive ? `<div class="osd"><span data-osd-cam>${CAMERAS.find(c => c.id === camera).label} camera</span><span data-osd-clock>${P.timeLabel(date)}</span></div>` : ""}
      <button class="big-play" data-act="clips-play" aria-label="Play">${P.ic("play")}</button>
    </div>
    <div class="player-bar">
      <button class="icon-btn" data-act="clips-play" aria-label="Play" data-play-btn>${P.ic("play")}</button>
      <button class="icon-btn" data-act="clips-seek" data-by="-${step}" aria-label="Back ${stepWords}">${P.ic("rew")}</button>
      <button class="icon-btn" data-act="clips-seek" data-by="${step}" aria-label="Forward ${stepWords}">${P.ic("ffw")}</button>
      <span class="time"><span data-time>0:00</span> <span class="muted">/ <span data-total>${duration ? P.clock(duration) : "…"}</span></span></span>
      <span class="grow"></span>
      <div class="seg speed-seg" role="group" aria-label="Speed">${speeds.map(s => `<button type="button" data-act="clips-speed" data-speed="${s}" aria-pressed="${s === 1}">${s}×</button>`).join("")}</div>
      <button class="icon-btn speed-cycle" type="button" data-act="clips-speed-cycle" title="Playback speed"><span class="sr">Playback speed </span><span data-speed-text>1×</span></button>
      ${audio ? `<button class="icon-btn" type="button" data-act="clips-mute" aria-label="Mute" aria-pressed="false" title="Mute (m)" ${camera === "low" ? "" : "hidden"}>${P.ic("sound")}</button>` : ""}
      <button class="icon-btn view-theater" type="button" data-act="clips-theater" aria-label="Theater mode" aria-pressed="${theater}" title="Theater mode (t)">${P.ic("theater")}</button>
      <button class="icon-btn view-full" type="button" data-act="clips-fullscreen" aria-label="Full screen" aria-pressed="false" title="Full screen (f)">${P.ic("full")}</button>
      ${isDrive && cameras.length > 1 ? `<div class="seg" role="group" aria-label="Camera">${cameras.map(c => `<button type="button" data-act="clips-camera" data-camera="${c.id}" aria-pressed="${c.id === camera}">${c.label}</button>`).join("")}</div>` : ""}
    </div>
    <div class="timeline-wrap">
      <div class="timeline" id="timeline" tabindex="0" role="slider" aria-label="Position in ${noun}" aria-valuemin="0" aria-valuemax="${duration || 0}" aria-valuenow="0" aria-valuetext="0:00">
        <div class="tl-track"></div>
        ${isDrive ? `<div class="tl-ticks"></div>` : ""}
        <div class="tl-head" data-head></div>
        <div class="tl-hover" data-hover>0:00</div>
      </div>
      <div class="tl-scale" data-scale>${scaleHTML(isDrive, date, duration)}</div>
      ${isDrive ? `<p class="tl-legend"><span><span class="sw"></span>openpilot engaged</span><span>Each tick is one minute</span></p>` : ""}
    </div>`;
}

function mountPlayer(root, options) {
  const { date, isDrive, parts, partLength, src } = options;
  const listening = new AbortController();
  const signal = listening.signal;
  const view = root.querySelector("#player");
  const video = view.querySelector("video");
  const column = root.querySelector(".player-col");
  const grid = root.querySelector(".drive-grid");
  const muteButton = root.querySelector("[data-act='clips-mute']");
  const speedButtons = [...root.querySelectorAll("[data-act='clips-speed']")];
  const timeline = root.querySelector("#timeline");
  const head = timeline.querySelector("[data-head]");
  const hover = timeline.querySelector("[data-hover]");
  const timeEl = root.querySelector("[data-time]");
  const osdClock = root.querySelector("[data-osd-clock]");
  const step = isDrive ? partLength : 10;
  let camera = options.camera;
  let duration = isDrive ? parts * partLength : 0;
  let index = 0;
  let offset = 0;
  let playing = false;
  let t = 0;

  function load(next, at) {
    index = next;
    offset = at;
    video.src = src(index, camera);
  }
  function playVideo() {
    video.play().catch(error => {
      if (error.name !== "AbortError") {
        throw error;
      }
    });
  }
  function where(seconds) {
    return isDrive ? `${P.clock(seconds)}, minute ${Math.min(parts, Math.floor(seconds / 60) + 1)} of ${parts}` : P.clock(seconds);
  }
  function show() {
    const fraction = duration ? t / duration : 0;
    head.style.left = `${fraction * 100}%`;
    timeEl.textContent = P.clock(t);
    if (osdClock) {
      osdClock.textContent = P.fmt(date + t, { hour: "numeric", minute: "2-digit", second: "2-digit" });
    }
    timeline.setAttribute("aria-valuenow", String(Math.round(t)));
    timeline.setAttribute("aria-valuetext", where(t));
    if (options.onMove) {
      options.onMove(fraction);
    }
  }
  function setPlaying(on) {
    playing = on;
    view.classList.toggle("playing", on);
    root.querySelectorAll("[data-act='clips-play']").forEach(b => {
      b.setAttribute("aria-label", on ? "Pause" : "Play");
      if (b.hasAttribute("data-play-btn")) {
        b.innerHTML = P.ic(on ? "pause" : "play");
      }
    });
    if (!on) {
      video.pause();
      return;
    }
    if (t >= duration) {
      seekTo(0);
    }
    if (video.readyState >= 1) {
      playVideo();
    }
  }
  function seekTo(seconds) {
    t = Math.max(0, Math.min(duration, seconds));
    const next = isDrive ? Math.min(parts - 1, Math.floor(t / partLength)) : 0;
    const at = t - next * (isDrive ? partLength : 0);
    if (next !== index) {
      load(next, at);
    } else if (video.readyState >= 1) {
      video.currentTime = at;
    } else {
      offset = at;
    }
    show();
  }
  function setSpeed(value) {
    video.defaultPlaybackRate = value;
    video.playbackRate = value;
    speedButtons.forEach(b => b.setAttribute("aria-pressed", String(Number(b.dataset.speed) === value)));
    root.querySelector("[data-speed-text]").textContent = `${value}×`;
  }
  function toggleFullscreen() {
    if (document.fullscreenElement) {
      document.exitFullscreen();
    } else if (column.requestFullscreen) {
      column.requestFullscreen();
    } else {
      video.webkitEnterFullscreen();
    }
  }
  function toggleMute() {
    video.muted = !video.muted;
    muteButton.setAttribute("aria-pressed", String(video.muted));
    muteButton.innerHTML = P.ic(video.muted ? "mute" : "sound");
  }
  function toggleTheater() {
    theater = !theater;
    grid.classList.toggle("theater", theater);
    root.querySelector("[data-act='clips-theater']").setAttribute("aria-pressed", String(theater));
  }
  function fromPointer(e) {
    const rect = timeline.getBoundingClientRect();
    return Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width)) * duration;
  }
  async function reportError() {
    const failed = src(index, camera);
    const title = isDrive ? `Couldn't play minute ${index + 1}` : "Couldn't play the recording";
    setPlaying(false);
    const response = await fetch(failed, { headers: { Range: "bytes=0-0" } });
    if (signal.aborted) {
      return;
    }
    if (response.ok) {
      P.toast(title, camera !== "low" && options.cameras?.some(c => c.id === "low") ? "This browser can't play this camera. Pick Low-res to watch it." : "This browser can't play this video", "alert");
    } else {
      P.fail(new P.PondError(response.status, (await response.text()).trim()), title);
    }
  }

  video.addEventListener("loadedmetadata", () => {
    if (!isDrive) {
      duration = video.duration;
      root.querySelector("[data-total]").textContent = P.clock(duration);
      root.querySelector("[data-scale]").innerHTML = scaleHTML(false, date, duration);
      timeline.setAttribute("aria-valuemax", String(Math.round(duration)));
      if (options.onLength) {
        options.onLength(duration);
      }
    }
    video.currentTime = offset;
    if (playing) {
      playVideo();
    }
  }, { signal });
  video.addEventListener("timeupdate", () => {
    if (video.readyState < 1) {
      return;
    }
    t = Math.min(duration, index * (isDrive ? partLength : 0) + video.currentTime);
    show();
  }, { signal });
  video.addEventListener("ended", () => {
    if (index + 1 < parts) {
      load(index + 1, 0);
      return;
    }
    t = duration;
    show();
    setPlaying(false);
  }, { signal });
  video.addEventListener("error", reportError, { signal });
  timeline.addEventListener("pointermove", e => {
    const at = fromPointer(e);
    const rect = timeline.getBoundingClientRect();
    hover.style.left = `${Math.min(rect.width - 40, Math.max(40, e.clientX - rect.left))}px`;
    hover.textContent = isDrive ? `${P.clock(at)} · minute ${Math.min(parts, Math.floor(at / 60) + 1)}` : P.clock(at);
  }, { signal });
  timeline.addEventListener("pointerdown", e => {
    if (e.target.closest(".tl-ev")) {
      return;
    }
    timeline.setPointerCapture(e.pointerId);
    seekTo(fromPointer(e));
    const move = ev => seekTo(fromPointer(ev));
    timeline.addEventListener("pointermove", move);
    timeline.addEventListener("pointerup", () => timeline.removeEventListener("pointermove", move), { once: true });
  }, { signal });
  timeline.addEventListener("keydown", e => {
    const steps = { ArrowLeft: -step, ArrowRight: step, PageDown: -step * 10, PageUp: step * 10 };
    if (steps[e.key]) {
      seekTo(t + steps[e.key]);
    } else if (e.key === "Home") {
      seekTo(0);
    } else if (e.key === "End") {
      seekTo(duration);
    } else if (e.key === " ") {
      setPlaying(!playing);
    } else {
      return;
    }
    e.preventDefault();
  }, { signal });
  document.addEventListener("fullscreenchange", () => {
    root.querySelector("[data-act='clips-fullscreen']").setAttribute("aria-pressed", String(document.fullscreenElement === column));
  }, { signal });
  document.addEventListener("keydown", e => {
    const key = e.key.toLowerCase();
    if (e.altKey || e.ctrlKey || e.metaKey || document.querySelector("dialog[open]") || e.target.closest("input, select, textarea, [contenteditable], .timeline, .menu")) {
      return;
    }
    if (key === " " || key === "k") {
      if (e.target.closest("button, a")) {
        return;
      }
      setPlaying(!playing);
    } else if (e.key === "ArrowLeft") {
      seekTo(t - step);
    } else if (e.key === "ArrowRight") {
      seekTo(t + step);
    } else if (key === "f") {
      toggleFullscreen();
    } else if (key === "m" && muteButton && !muteButton.hidden) {
      toggleMute();
    } else if (key === "t") {
      toggleTheater();
    } else {
      return;
    }
    e.preventDefault();
  }, { signal });

  grid.classList.toggle("theater", theater);

  const ticks = timeline.querySelector(".tl-ticks");
  if (ticks) {
    ticks.style.backgroundSize = `${100 / Math.min(parts, 120)}% 100%`;
  }
  const start = Math.max(0, Number(P.params().get("t")) || 0);
  const first = isDrive ? Math.min(parts - 1, Math.floor(start / partLength)) : 0;
  load(first, start - first * (isDrive ? partLength : 0));
  t = Math.min(duration, start);
  show();

  player = {
    cycleSpeed: () => {
      const speeds = speedButtons.map(b => Number(b.dataset.speed));
      setSpeed(speeds[(speeds.indexOf(video.playbackRate) + 1) % speeds.length]);
    },
    fullscreen: toggleFullscreen,
    jump: seconds => {
      seekTo(seconds);
      setPlaying(true);
    },
    mute: toggleMute,
    pause: () => setPlaying(false),
    part: () => index,
    play: () => setPlaying(!playing),
    seek: by => seekTo(t + by),
    setCamera: next => {
      camera = next;
      load(index, video.readyState >= 1 ? video.currentTime : offset);
      root.querySelectorAll("[data-act='clips-camera']").forEach(b => b.setAttribute("aria-pressed", String(b.dataset.camera === next)));
      root.querySelector("[data-osd-cam]").textContent = `${CAMERAS.find(c => c.id === next).label} camera`;
      if (muteButton) {
        muteButton.hidden = next !== "low";
      }
      P.setParam("camera", next === options.cameras[0].id ? "" : next);
    },
    setSpeed,
    theater: toggleTheater,
    time: () => t
  };
  return () => {
    listening.abort();
    video.pause();
    video.removeAttribute("src");
    video.load();
    player = null;
  };
}

function mountDetail(root, kind, item, options) {
  current = { item, kind };
  const stopPlayer = mountPlayer(root, options);
  return () => {
    stopPlayer();
    current = null;
  };
}

function pagerHTML(kind, ids, id) {
  const index = ids.indexOf(id);
  const link = (other, label, icon) => (other
    ? `<a class="icon-btn" href="${kind.href(other)}" aria-label="${label}">${P.ic(icon)}</a>`
    : `<button class="icon-btn" type="button" disabled aria-label="${label}">${P.ic(icon)}</button>`);
  return `<div class="pager"><span>${index + 1} of ${ids.length}</span>${link(ids[index - 1], `Newer ${kind.noun}`, "back")}${link(ids[index + 1], `Older ${kind.noun}`, "next")}</div>`;
}

function notFound(kind) {
  return {
    body: `<div class="empty"><h3>This ${kind.noun} isn't on the device anymore</h3><a class="btn btn-quiet btn-small" href="${kind.path}">Back to ${kind.label.toLowerCase()}</a></div>`,
    crumb: { href: kind.path, label: kind.label },
    title: `${kind.noun[0].toUpperCase()}${kind.noun.slice(1)} not found`
  };
}

async function renameItem(kind, item) {
  const d = await P.dialog({
    icon: "pencil",
    title: `Rename ${kind.noun}`,
    body: `<label class="field"><span>Name</span><input class="input" data-name maxlength="${kind.maxName}" value="${P.esc(item.name)}" placeholder="${P.esc(itemTitle({ ...item, name: "" }))}"></label>`,
    confirm: "Save name",
    onOpen: dl => {
      const input = dl.querySelector("[data-name]");
      setTimeout(() => {
        input.focus();
        input.select();
      }, 0);
    }
  });
  if (!d) {
    return;
  }
  const name = d.querySelector("[data-name]").value.trim();
  if (name !== item.name) {
    await kind.rename(item, name);
  }
}

function mapSVG(points, width, height) {
  const streets = [];
  for (let x = 0; x <= width; x += 20) {
    streets.push(`M${x} 0V${height}`);
  }
  for (let y = 0; y <= height; y += 20) {
    streets.push(`M0 ${y}H${width}`);
  }
  const d = points.map(([x, y], i) => `${i ? "L" : "M"}${x} ${y}`).join(" ");
  return `<svg viewBox="0 0 ${width} ${height}" role="img" aria-label="Map of the route">
    <rect width="${width}" height="${height}" fill="var(--paper)"/>
    <path d="${streets.join("")}" stroke="color-mix(in srgb, var(--ink) 7%, transparent)" stroke-width="1"/>
    <path d="${d}" fill="none" stroke="#fff" stroke-width="8" stroke-linejoin="round" stroke-linecap="round"/>
    <path d="${d}" fill="none" stroke="var(--pond)" stroke-width="4" stroke-linejoin="round" stroke-linecap="round"/>
    <circle cx="${points[0][0]}" cy="${points[0][1]}" r="7" fill="var(--card)" stroke="var(--pond)" stroke-width="3"/>
    <g data-car><circle r="9" fill="var(--season)" stroke="#fff" stroke-width="3"/></g>
  </svg>`;
}

function svgMap(holder, path, width = 400, height = 300) {
  const middle = path.reduce((sum, [lat]) => sum + lat, 0) / path.length;
  const squeeze = Math.cos((middle * Math.PI) / 180);
  const xs = path.map(([, lon]) => lon * squeeze);
  const ys = path.map(([lat]) => -lat);
  const minX = Math.min(...xs);
  const minY = Math.min(...ys);
  const spanX = Math.max(Math.max(...xs) - minX, 1e-4);
  const spanY = Math.max(Math.max(...ys) - minY, 1e-4);
  const scale = Math.min((width - 60) / spanX, (height - 60) / spanY);
  const left = (width - spanX * scale) / 2;
  const top = (height - spanY * scale) / 2;
  const points = xs.map((x, i) => [Math.round((left + (x - minX) * scale) * 10) / 10, Math.round((top + (ys[i] - minY) * scale) * 10) / 10]);
  holder.innerHTML = mapSVG(points, width, height);
  const car = holder.querySelector("[data-car]");
  return {
    move: fraction => {
      const [x, y] = points[Math.round(fraction * (points.length - 1))];
      car.setAttribute("transform", `translate(${x} ${y})`);
    },
    remove: () => {}
  };
}

function loadMapbox() {
  if (!mapboxScript) {
    mapboxScript = new Promise((resolve, reject) => {
      const style = document.createElement("link");
      style.rel = "stylesheet";
      style.href = "/assets/vendor/mapbox-gl/mapbox-gl.css";
      document.head.append(style);
      const script = document.createElement("script");
      script.src = "/assets/vendor/mapbox-gl/mapbox-gl.js";
      script.onload = resolve;
      script.onerror = () => reject(new Error("Couldn't load /assets/vendor/mapbox-gl/mapbox-gl.js"));
      document.head.append(script);
    });
  }
  return mapboxScript;
}

async function mapboxMap(holder, path, key, signal, onFail) {
  await loadMapbox();
  if (signal.aborted) {
    return null;
  }
  const coordinates = path.map(([lat, lon]) => [lon, lat]);
  const point = coordinate => ({ geometry: { coordinates: coordinate, type: "Point" }, type: "Feature" });
  const styles = getComputedStyle(document.documentElement);
  const color = name => styles.getPropertyValue(name).trim();
  const bounds = coordinates.reduce((box, coordinate) => box.extend(coordinate), new mapboxgl.LngLatBounds(coordinates[0], coordinates[0]));
  mapboxgl.accessToken = key;
  const map = new mapboxgl.Map({ bounds, container: holder, fitBoundsOptions: { maxZoom: 16, padding: 30 }, style: MAP_STYLE });
  let car = coordinates[0];
  let loaded = false;
  const fail = () => {
    if (loaded) {
      return;
    }
    map.off("error", fail);
    map.remove();
    onFail();
  };
  map.on("error", fail);
  map.on("load", () => {
    loaded = true;
    map.addSource("route", { data: { geometry: { coordinates, type: "LineString" }, type: "Feature" }, type: "geojson" });
    map.addSource("start", { data: point(coordinates[0]), type: "geojson" });
    map.addSource("car", { data: point(car), type: "geojson" });
    map.addLayer({ id: "route-casing", layout: { "line-cap": "round", "line-join": "round" }, paint: { "line-color": "#fff", "line-width": 8 }, source: "route", type: "line" });
    map.addLayer({ id: "route", layout: { "line-cap": "round", "line-join": "round" }, paint: { "line-color": color("--pond"), "line-width": 4 }, source: "route", type: "line" });
    map.addLayer({ id: "start", paint: { "circle-color": color("--card"), "circle-radius": 5.5, "circle-stroke-color": color("--pond"), "circle-stroke-width": 3 }, source: "start", type: "circle" });
    map.addLayer({ id: "car", paint: { "circle-color": color("--season"), "circle-radius": 7.5, "circle-stroke-color": "#fff", "circle-stroke-width": 3 }, source: "car", type: "circle" });
  });
  return {
    move: fraction => {
      car = coordinates[Math.round(fraction * (coordinates.length - 1))];
      map.getSource("car")?.setData(point(car));
    },
    remove: () => map.remove()
  };
}

function momentLabel(moment) {
  const { kind, reason } = moment;
  if (kind === "alert") {
    return P.esc(reason);
  }
  if (kind === "bookmark") {
    return "Flagged moment";
  }
  if (kind === "curve") {
    return "Slowed for a curve";
  }
  if (kind === "experimental") {
    return `Experimental Mode (${CEM_REASONS[reason]})`;
  }
  if (kind === "gear") {
    return `Shifted to ${reason}`;
  }
  if (kind === "hard_brake") {
    return "Hard braking";
  }
  if (kind === "lane_change") {
    return reason === "none" ? "Lane change" : `Lane change ${reason}`;
  }
  if (kind === "personality") {
    return `Switched to ${reason} personality`;
  }
  if (kind === "road") {
    return P.esc(reason);
  }
  if (kind === "speed_limit") {
    return `Speed limit ${P.speed(reason)}`;
  }
  if (kind === "stop") {
    return "Stopped";
  }
  return DISENGAGE_REASONS[reason] ? `Disengaged (${DISENGAGE_REASONS[reason]})` : "Disengaged";
}

function showTimeline(root, drive, date, duration) {
  const timeline = root.querySelector("#timeline");
  const at = seconds => `${(seconds / duration) * 100}%`;
  const track = timeline.querySelector(".tl-track");
  for (const [start, end] of drive.engaged) {
    const span = document.createElement("i");
    span.style.left = at(start);
    span.style.width = at(end - start);
    track.append(span);
  }
  if (!drive.events.length) {
    return;
  }
  const kinds = Object.keys(EVENT_KINDS).filter(kind => drive.events.some(moment => moment.kind === kind)).sort((a, b) => EVENT_KINDS[a].label.localeCompare(EVENT_KINDS[b].label));
  const hidden = moment => (hiddenKinds.has(moment.kind) ? "hidden" : "");
  timeline.querySelector("[data-head]").insertAdjacentHTML("beforebegin", drive.events.map(moment => `<button class="tl-ev ${moment.kind}" data-act="drives-jump" data-kind="${moment.kind}" data-seconds="${moment.time}" aria-label="${momentLabel(moment)} at ${P.timeLabel(date + moment.time)}, ${P.clock(moment.time)} in" ${hidden(moment)}>${P.ic(EVENT_KINDS[moment.kind].icon)}</button>`).join(""));
  timeline.querySelectorAll(".tl-ev").forEach(marker => {
    marker.style.left = at(Number(marker.dataset.seconds));
  });
  root.querySelector(".timeline-wrap").insertAdjacentHTML("afterend", `
    <div class="tl-filter" role="group" aria-label="Moments to show">${kinds.map(kind => `<button type="button" class="chip" data-act="drives-moment-kind" data-kind="${kind}" aria-pressed="${!hiddenKinds.has(kind)}">${P.ic(EVENT_KINDS[kind].icon)}${EVENT_KINDS[kind].label}<span class="n">${drive.events.filter(moment => moment.kind === kind).length}</span></button>`).join("")}</div>
    <ul class="moments" aria-label="Moments in this drive">${drive.events.map(moment => `<li data-kind="${moment.kind}" ${hidden(moment)}><button type="button" data-act="drives-jump" data-seconds="${moment.time}"><span class="t">${P.timeLabel(date + moment.time)}</span>${P.ic(EVENT_KINDS[moment.kind].icon, `ev-${moment.kind}`)}<span>${momentLabel(moment)}</span></button></li>`).join("")}</ul>`);
}

async function deleteDrives(items) {
  const one = items.length === 1;
  const bytes = items.reduce((sum, item) => sum + item.size, 0);
  const minutes = items.reduce((sum, item) => sum + item.segments, 0);
  const flagged = items.filter(item => item.bookmarked).length;
  const preserved = items.filter(item => item.preserved);
  const ok = await P.dialog({
    icon: "trash", tone: "danger", confirmTone: "danger",
    title: one ? `Delete “${P.esc(itemTitle(items[0]))}”?` : `Delete ${items.length} drives?`,
    body: `<p>This removes ${one ? "the drive" : "them"} from the device for good and frees <b>${P.size(bytes)}</b>. It can't be undone.</p>
      ${P.lossList(
        [`All camera video (forward, wide, driver, and low-res) for ${P.plural(minutes, "minute")} of driving`, "The logs, the route on the map, and the timeline", ...(flagged ? [`Flagged moments in ${P.plural(flagged, "drive")}`] : []), ...(preserved.length ? [`<b>${P.plural(preserved.length, "favorite drive")}</b>: ${preserved.map(item => P.esc(itemTitle(item))).join(", ")}`] : [])],
        ["Anything already uploaded to comma connect", "Screen recordings and every other drive"]
      )}`,
    confirm: `Delete ${one ? "drive" : P.plural(items.length, "drive")} (${P.size(bytes)})`
  });
  if (!ok) {
    return false;
  }
  try {
    if (one) {
      await P.request("DELETE", `/api/routes/${encodeURIComponent(items[0].id)}`);
    } else {
      await P.request("POST", "/api/routes/delete", { routes: items.map(item => item.id) });
    }
  } catch (error) {
    P.fail(error, one ? "Couldn't delete the drive" : `Couldn't delete the ${items.length} drives`, { recording: one ? "It's still recording. Try again after the drive ends" : "One of them is still recording. Try again after the drive ends" });
    return false;
  }
  P.toast(one ? "Drive deleted" : `${items.length} drives deleted`, `${P.size(bytes)} freed`);
  return true;
}

async function renameDrive(item, name) {
  const path = `/api/routes/${encodeURIComponent(item.id)}/name`;
  try {
    if (name) {
      await P.request("PUT", path, { name });
    } else {
      await P.request("DELETE", path);
    }
  } catch (error) {
    P.fail(error, "Couldn't rename the drive", { invalid_name: "Use up to 64 characters" });
    return;
  }
  P.toast(name ? `Renamed to “${P.esc(name)}”` : "Name cleared");
  P.setParam("t", String(Math.floor(player.time())));
  P.refresh();
}

const DRIVES = {
  api: "/api/routes",
  busy: item => (item.recording ? "Saving…" : ""),
  chip: seconds => shortDur(seconds),
  favorite: true,
  flags: item => (item.bookmarked ? flag("bookmark", "Flagged") : ""),
  href: id => `/drive?route=${encodeURIComponent(id)}`,
  item: route => ({ ...route, duration: route.segments * route.segment_length, id: route.route }),
  label: "Drives",
  length: item => item.duration,
  maxName: 64,
  meta: item => (item.distance === undefined ? P.size(item.size) : P.distance(item.distance)),
  none: `<h3>No drives yet</h3><p>Drives show up here after the car has been driven.</p>`,
  noun: "drive",
  only: [["named", "Named", item => Boolean(item.name)], ["bookmarked", "Flagged", item => item.bookmarked], ["preserved", "Favorites", item => item.preserved]],
  path: "/drives",
  preview: id => `/api/routes/${encodeURIComponent(id)}/preview`,
  remove: deleteDrives,
  rename: renameDrive,
  routeMap: true,
  search: item => `${item.name} ${item.route}`,
  searchHint: "Name, day, or route ID",
  sorts: [["newest", "Newest first"], ["oldest", "Oldest first"], ["longest", "Longest first"], ["largest", "Largest size"], ["name", "Name"]],
  thumbnail: id => `/api/routes/${encodeURIComponent(id)}/thumbnail`
};

async function drivePage(params) {
  const id = params.get("route") || "";
  if (!id) {
    return notFound(DRIVES);
  }
  const route = encodeURIComponent(id);
  let detail;
  let keys;
  try {
    [detail, keys] = await Promise.all([P.get(`/api/routes/${route}`), P.get("/api/keys")]);
  } catch (error) {
    if (error.status !== 404) {
      throw error;
    }
    return notFound(DRIVES);
  }
  const item = DRIVES.item(detail);
  if (item.recording) {
    return {
      body: `<div class="empty"><h3>This drive is still recording</h3><p>It opens here once the drive ends.</p><a class="btn btn-quiet btn-small" href="/drives">Back to drives</a></div>`,
      crumb: { href: "/drives", label: "Drives" },
      title: P.esc(itemTitle(item))
    };
  }
  const duration = detail.segments * detail.segment_length;
  const cameras = CAMERAS.filter(c => detail.cameras.includes(c.id));
  const camera = cameras.some(c => c.id === params.get("camera")) ? params.get("camera") : (cameras[0] || CAMERAS[0]).id;
  const uploads = [...cameras.map(c => [c.file, detail.uploaded[c.id]]), ...LOGS.map(([kind, label]) => [label, detail.uploaded[kind]])];
  const complete = uploads.filter(([, count]) => count >= detail.segments).length;
  const title = itemTitle(item);
  return {
    title: P.esc(title),
    docTitle: title,
    crumb: { href: "/drives", label: "Drives" },
    actions: `<button class="icon-btn" data-act="clips-rename" aria-label="Rename drive" title="Rename">${P.ic("pencil")}</button>
      <div class="menu-wrap">
        <button class="btn btn-primary btn-small" data-act="menu" aria-expanded="false" aria-haspopup="true" aria-label="Download">${P.ic("download")}<span class="lbl-long">Download</span>${P.ic("down", "ic-s")}</button>
        <div class="menu" hidden role="menu" aria-label="Download">
          <div class="seg dl-scope" role="group" aria-label="How much">
            <button type="button" data-act="drives-scope" data-scope="all" aria-pressed="true">Whole drive</button>
            <button type="button" data-act="drives-scope" data-scope="minute" aria-pressed="false">This minute only</button>
            <button type="button" data-act="drives-scope" data-scope="range" aria-pressed="false">Minutes</button>
          </div>
          <div class="dl-range" hidden>
            <label>From minute <input class="input" type="number" data-range-from min="1" max="${detail.segment_numbers.length}" value="1"></label>
            <label>to <input class="input" type="number" data-range-to min="1" max="${detail.segment_numbers.length}" value="${detail.segment_numbers.length}"></label>
          </div>
          <h3>Video</h3>
          ${cameras.map(c => `<button class="menu-item" role="menuitem" data-act="drives-download" data-kind="${c.id}">${P.ic("download")}<span>${c.file}</span></button>`).join("")}
          <hr><h3>Logs</h3>
          <p class="dl-note">Logs include your VIN, device serial, GitHub SSH keys, and exact GPS track.</p>
          ${LOGS.map(([kind, label]) => `<button class="menu-item" role="menuitem" data-act="drives-download" data-kind="${kind}" data-log>${P.ic("file")}<span>${label}</span></button>`).join("")}
        </div>
      </div>`,
    body: `
    <div class="drive-grid">
      <div class="player-col">${playerHTML({ audio: detail.audio, camera, cameras, date: item.date, duration, noun: "drive", partLength: detail.segment_length, poster: DRIVES.thumbnail(id) })}</div>
      <div class="drive-side">
        <div class="map" data-map></div>
        <dl class="kv">
          <dt>Date</dt><dd>${P.fmt(item.date, { weekday: "short", month: "short", day: "numeric" })}</dd>
          <dt>Time</dt><dd>${P.timeLabel(item.date)} to ${P.timeLabel(item.date + duration)}</dd>
          <dt>Distance</dt><dd data-distance>…</dd>
          <dt>Driving time</dt><dd>${P.duration(duration)}</dd>
          <dt>openpilot engaged</dt><dd data-engaged>…</dd>
          <dt>Model</dt><dd data-model>…</dd>
          <dt>Route ID</dt><dd class="mono">${P.esc(item.route)}</dd>
        </dl>
        <div class="side-row fav-row">${favoriteButton(item, "btn btn-quiet btn-small")}<span class="hint">Favorites are kept until everything else is gone when space runs low.</span></div>
        <details class="uploads side-row">
          <summary><b>Upload status</b><span class="muted">${complete} of ${uploads.length} fully uploaded</span></summary>
          ${uploads.map(([label, count]) => `<div class="up-row"><span class="lbl">${label}</span><span>${count >= detail.segments ? "All uploaded" : count === 0 ? "Not uploaded" : `${count} of ${detail.segments} minutes`}</span><div class="progress"><i data-w="${(count / detail.segments) * 100}"></i></div></div>`).join("")}
        </details>
        <div class="side-row"><a class="link" href="/report?route=${encodeURIComponent(item.route)}">${P.ic("report")}Report a problem with this drive</a></div>
        <div class="danger-zone side-row"><span class="muted">Frees ${P.size(item.size)}</span><button class="btn btn-danger-quiet btn-small" data-act="clips-delete-item">${P.ic("trash")}Delete drive</button></div>
        ${pagerHTML(DRIVES, detail.route_ids, id)}
      </div>
    </div>`,
    mount: root => {
      const listening = new AbortController();
      const signal = listening.signal;
      let map = null;
      let fraction = 0;
      const stopDetail = mountDetail(root, DRIVES, item, {
        camera,
        cameras,
        date: item.date,
        isDrive: true,
        onMove: value => {
          fraction = value;
          if (map) {
            map.move(value);
          }
        },
        partLength: detail.segment_length,
        parts: detail.segment_numbers.length,
        src: (index, cam) => `/api/routes/${route}/${detail.segment_numbers[index]}/${cam}`
      });
      current.segments = detail.segment_numbers;
      P.get(`/api/routes/${route}/drive`).then(async drive => {
        if (signal.aborted) {
          return;
        }
        root.querySelector("[data-distance]").textContent = P.distance(drive.distance);
        root.querySelector("[data-engaged]").innerHTML = `${P.duration(drive.engaged_time)} <span class="muted">(${Math.round((drive.engaged_time / duration) * 100)}%)</span>`;
        root.querySelector("[data-model]").textContent = drive.model ? drive.model.replace("_default", "").replace("(Default)", "").trim() : "Not available";
        showTimeline(root, drive, item.date, duration);
        const holder = root.querySelector("[data-map]");
        if (!drive.path.length) {
          holder.remove();
          return;
        }
        const drawSvg = () => {
          map = svgMap(holder, drive.path);
          map.move(fraction);
        };
        map = keys.mapbox_public ? await mapboxMap(holder, drive.path, keys.mapbox_public, signal, drawSvg) : svgMap(holder, drive.path);
        if (map) {
          map.move(fraction);
        }
      }, error => {
        if (signal.aborted) {
          return;
        }
        root.querySelector("[data-distance]").textContent = "Not available";
        root.querySelector("[data-engaged]").textContent = "Not available";
        root.querySelector("[data-model]").textContent = "Not available";
        root.querySelector("[data-map]").remove();
        if (error.status !== 404) {
          P.fail(error, "Couldn't load the drive's distance and route");
        }
      });
      return () => {
        listening.abort();
        stopDetail();
        if (map) {
          map.remove();
        }
      };
    }
  };
}

P.clips = { drawRoute, itemTitle, lengths, listPage, mountDetail, notFound, pagerHTML, playerHTML, thumbHTML, watchPreviews, watchThumbs };

P.act("clips-camera", b => player.setCamera(b.dataset.camera));
P.act("clips-clear", () => P.go(list.kind.path, { replace: true }));
P.act("clips-delete-card", async b => {
  const item = list.items.find(entry => entry.id === b.dataset.id);
  if (await list.kind.remove([item])) {
    P.refresh();
  }
});
P.act("clips-delete-item", async () => {
  const { item, kind } = current;
  player.pause();
  if (await kind.remove([item])) {
    P.go(kind.path, { replace: true });
  }
});
P.act("clips-delete-selected", async () => {
  const chosen = list.items.filter(item => list.selected.has(item.id));
  if (await list.kind.remove(chosen)) {
    P.refresh();
  }
});
P.act("clips-fullscreen", () => player.fullscreen());
P.act("clips-mute", () => player.mute());
P.act("clips-play", () => player.play());
P.act("clips-rename", () => renameItem(current.kind, current.item));
P.act("clips-seek", b => player.seek(Number(b.dataset.by)));
P.act("clips-select-all", () => {
  for (const item of filtered(list)) {
    list.selected.add(item.id);
  }
  refreshList();
});
P.act("clips-select-done", () => {
  list.selecting = false;
  list.selected.clear();
  refreshList();
});
P.act("clips-select-toggle", () => {
  list.selecting = !list.selecting;
  if (!list.selecting) {
    list.selected.clear();
  }
  refreshList();
});
P.act("clips-speed", b => player.setSpeed(Number(b.dataset.speed)));
P.act("clips-speed-cycle", () => player.cycleSpeed());
P.act("clips-theater", () => player.theater());
function rangeMinute(input, last) {
  const minute = Math.round(Number(input.value)) || 1;
  return Math.min(last, Math.max(1, minute));
}

P.act("drives-download", b => {
  const id = current.item.id;
  const kind = b.dataset.kind;
  const menu = b.closest(".menu");
  const scope = menu.querySelector("[data-act='drives-scope'][aria-pressed='true']").dataset.scope;
  if (scope === "minute") {
    const segment = current.segments[player.part()];
    P.download(`/api/routes/${encodeURIComponent(id)}/${segment}/${kind}`, `${id}--${segment}-${kind}.mp4`);
    return;
  }
  let range = "";
  if (scope === "range") {
    const last = current.segments.length;
    const first = rangeMinute(menu.querySelector("[data-range-from]"), last);
    const second = rangeMinute(menu.querySelector("[data-range-to]"), last);
    range = `?start=${current.segments[Math.min(first, second) - 1]}&end=${current.segments[Math.max(first, second) - 1]}`;
  }
  P.saveLink(`/api/routes/${encodeURIComponent(id)}/download/${kind}${range}`);
  P.toast(`Downloading ${b.querySelector("span").textContent}`, "Your browser shows the progress");
});
P.act("drives-favorite", async b => {
  const item = list ? list.items.find(entry => entry.id === b.dataset.id) : current.item;
  const on = !item.preserved;
  try {
    await P.busy(b, P.request(on ? "PUT" : "DELETE", `/api/routes/${encodeURIComponent(item.id)}/preserve`));
  } catch (error) {
    P.fail(error, on ? "Couldn't add the drive to favorites" : "Couldn't remove the drive from favorites", { recording: "It's still recording. Try again after the drive ends" });
    return;
  }
  item.preserved = on;
  b.setAttribute("aria-pressed", String(on));
  P.toast(on ? "Added to favorites" : "Removed from favorites", on ? "Kept until everything else is gone when space runs low" : "");
});
P.act("drives-favorite-selected", async b => {
  const chosen = list.items.filter(item => list.selected.has(item.id));
  const on = chosen.some(item => !item.preserved);
  const changing = chosen.filter(item => item.preserved !== on);
  try {
    await P.busy(b, (async () => {
      for (const item of changing) {
        await P.request(on ? "PUT" : "DELETE", `/api/routes/${encodeURIComponent(item.id)}/preserve`);
        item.preserved = on;
      }
    })());
  } catch (error) {
    P.fail(error, on ? "Couldn't add the drives to favorites" : "Couldn't remove the drives from favorites", { recording: "One of them is still recording. Try again after the drive ends" });
    refreshList();
    return;
  }
  P.toast(on ? `${P.plural(changing.length, "drive")} added to favorites` : `${P.plural(changing.length, "drive")} removed from favorites`, on ? "Kept until everything else is gone when space runs low" : "");
  refreshList();
});
P.act("drives-jump", b => {
  window.scrollTo({ top: 0, behavior: P.reducedMotion.matches ? "auto" : "smooth" });
  player.jump(Number(b.dataset.seconds));
});
P.act("drives-moment-kind", b => {
  const kind = b.dataset.kind;
  const show = hiddenKinds.has(kind);
  if (show) {
    hiddenKinds.delete(kind);
  } else {
    hiddenKinds.add(kind);
  }
  b.setAttribute("aria-pressed", String(show));
  document.querySelectorAll(`.tl-ev[data-kind="${kind}"], .moments li[data-kind="${kind}"]`).forEach(element => {
    element.hidden = !show;
  });
});
P.act("drives-scope", b => {
  const menu = b.closest(".menu");
  menu.querySelectorAll("[data-act='drives-scope']").forEach(x => x.setAttribute("aria-pressed", String(x === b)));
  menu.querySelector(".dl-range").hidden = b.dataset.scope !== "range";
  if (b.dataset.scope === "range") {
    const minute = player.part() + 1;
    menu.querySelector("[data-range-from]").value = minute;
    menu.querySelector("[data-range-to]").value = Math.min(current.segments.length, minute + 1);
  }
  menu.querySelectorAll("[data-log]").forEach(x => {
    x.disabled = b.dataset.scope === "minute";
  });
});

P.page("drives", { title: "Drives", render: params => listPage(DRIVES, params) });
P.page("drive", { parent: "drives", title: "Drive", render: drivePage });
})();
