(() => {
"use strict";

const P = window.Pond;

const DANGER_LABELS = ["DELETE", "FLASH", "REMOVE", "RESET"];
const DONE_TIME = 1500;
const FAST_STEPS = 5;
const HIGHLIGHT_TIME = 2000;
const HOLD_DELAY = 500;
const HOLD_INTERVAL = 150;
const MESSAGES = {
  already_confirmed: "The tuning level is already set",
  hidden: "That setting isn't available right now",
  invalid_value: "That value isn't allowed",
  openpilot_steering: "The device can't reboot while openpilot is steering. Disengage, then reboot from the Device panel.",
  unknown_action: "That button isn't in The Pond",
  unknown_setting: "That setting isn't in The Pond"
};
const NUMBER_WIDGETS = ["value", "value_buttons"];
const PAGE_LABELS = Object.fromEntries(P.NAV.flatMap(group => group.items).map(item => [item.id, item.label]));
const PAGE_STEPS = 10;
const PATH_JOIN = "›";
const REBOOT_TEXT = "Reboot required to take effect.";
const SAVE_DELAY = 150;
const SECTION_ICONS = {
  "Alerts and Sounds": "sound",
  "Driving Controls": "wheel",
  "Maps and Navigation": "maps",
  "System Settings": "device",
  "Theme and Appearance": "theme",
  "Vehicle Settings": "vehicle"
};

const localValues = new Map();
const openDescriptions = new Set();
const saveTimers = new Map();
const scrollMemory = new Map();
const warned = new Set();
const writes = new Map();

let data = null;
let editing = false;
let firstOpenShown = false;
let hold = null;
let lastDepth = 0;
let pending = null;
let restoring = false;
let root = null;
let view = [];

function pathKey(path) {
  return path.join(PATH_JOIN);
}

function slug(text) {
  return String(text).toLowerCase().replace(/&/g, "and").replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "");
}

function viewOf(path) {
  return path.map(slug).join("/");
}

function href(path) {
  return path.length ? `/toggles?view=${viewOf(path)}` : "/toggles";
}

function isCaps(text) {
  return text === text.toUpperCase() && /[A-Z]/.test(text);
}

function titleCase(text) {
  return text.toLowerCase().replace(/(^|[\s/(-])([a-z])/g, (match, lead, letter) => lead + letter.toUpperCase()).replace("Frogpilot", "FrogPilot");
}

function panelTitle(path) {
  const last = path[path.length - 1];
  if (last === "MANAGE") {
    return path[0];
  }
  return isCaps(last) ? titleCase(last) : last;
}

function rich(html) {
  return P.esc(html).replace(/&lt;(\/?)(b|i)&gt;/g, "<$1$2>").replace(/&lt;br\s*\/?&gt;/g, "<br>").replace(/\n/g, "<br>");
}

function leadOf(html) {
  const match = String(html || "").match(/^<b>(.*?)<\/b>/);
  return match ? match[1] : String(html || "").replace(/<[^>]+>/g, "");
}

function roundHalfAway(value) {
  return Math.sign(value) * Math.floor(Math.abs(value) + 0.5);
}

function factorOf(number) {
  return 10 ** number.decimals;
}

function clamp(value, number) {
  return Math.min(number.max, Math.max(number.min, value));
}

function displayed(row) {
  const factor = factorOf(row.number);
  return clamp(roundHalfAway(row.current * factor) / factor, row.number);
}

function formatNumber(number, value) {
  const factor = factorOf(number);
  for (const [labelValue, label] of number.labels) {
    if (roundHalfAway(labelValue * factor) === roundHalfAway(value * factor)) {
      return label;
    }
  }
  return value.toFixed(Math.max(number.decimals, 0)) + number.unit.replace(/^ /, P.NB);
}

function tuningRow() {
  return data.rows.find(row => row.key === "TuningLevel");
}

function levelName(level) {
  return tuningRow().options[level].label;
}

function levelLines() {
  return tuningRow().description.split("<br><br>")[1].split("<br>");
}

function sections() {
  return data.rows.filter(row => row.path.length === 0 && row.key !== "TuningLevel").sort((a, b) => a.order - b.order);
}

function pathRows(path) {
  return data.rows.filter(row => pathKey(row.path) === pathKey(path));
}

function panelRows(path) {
  return pathRows(path).filter(row => !row.pair).sort((a, b) => a.order - b.order);
}

function knownPaths() {
  const paths = [];
  for (const row of data.rows) {
    paths.push(row.path, ...row.children);
    for (const option of row.options || []) {
      if (option.children) {
        paths.push(option.children);
      }
    }
  }
  return paths;
}

function pathFromView(value) {
  if (!value) {
    return [];
  }
  const section = sections().find(row => slug(row.title) === value);
  if (section) {
    return [section.title];
  }
  return knownPaths().find(path => path.length > 1 && viewOf(path) === value) || [];
}

function opener(path) {
  for (const row of data.rows) {
    if (row.children.some(child => pathKey(child) === pathKey(path))) {
      return row;
    }
  }
  return null;
}

function aboutOf(path) {
  const parent = opener(path);
  if (parent) {
    return { description: parent.description, title: parent.title };
  }
  const section = sections().find(row => row.title === path[0]);
  return { description: section.description, title: section.title };
}

function rowsUnder(path) {
  return data.rows.filter(row => row.path.length >= path.length && path.every((step, index) => row.path[index] === step));
}

function changedCount(path) {
  return rowsUnder(path).filter(row => row.changed && !row.locked && row.widget !== "manage").length;
}

function reasonText(reason) {
  if (reason.kind === "level") {
    return `Shown at ${levelName(reason.level)}`;
  }
  if (reason.kind === "car") {
    return "Not available on this car";
  }
  if (reason.kind === "branch") {
    return "Not available on this branch";
  }
  if (reason.kind === "state") {
    return reason.text;
  }
  if (reason.kind === "set") {
    return `Shown once “${reason.title}” is set`;
  }
  return `Shown while “${reason.title}” is ${reason.wanted ? "on" : "off"}`;
}

function disabledText(reason) {
  if (reason.kind === "level") {
    return `Changes at ${levelName(reason.level)}`;
  }
  if (reason.kind === "car") {
    return "Not available on this car";
  }
  if (reason.kind === "branch") {
    return "Set by this branch";
  }
  if (reason.kind === "state") {
    return reason.text;
  }
  if (reason.kind === "set") {
    return `Set “${reason.title}” to change this`;
  }
  return `Turn ${reason.wanted ? "on" : "off"} “${reason.title}” to change this`;
}

function crumbsHtml(path) {
  const crumbs = [`<a href="/toggles">Toggles</a>`];
  for (let index = 1; index < path.length; index++) {
    const sub = path.slice(0, index);
    if (index === 1) {
      crumbs.push(`<a href="${href(sub)}">${P.esc(path[0])}</a>`);
    } else if (panelTitle(sub) === path[0]) {
      crumbs[crumbs.length - 1] = `<a href="${href(sub)}">${P.esc(path[0])}</a>`;
    } else {
      crumbs.push(`<a href="${href(sub)}">${P.esc(panelTitle(sub))}</a>`);
    }
  }
  if (path.length === 2 && panelTitle(path) === path[0]) {
    crumbs[1] = `<a href="${href(path)}" aria-current="page">${P.esc(path[0])}</a>`;
  }
  return `<nav class="toggles-trail" aria-label="Breadcrumb">${crumbs.join(P.ic("next", "toggles-sep"))}</nav>`;
}

function saveMark(row) {
  return `<span class="toggles-save" data-save="${P.esc(row.id)}" aria-hidden="true"></span>`;
}

function switchHtml(row) {
  return `<label class="switch toggles-switch"><input type="checkbox" role="switch" data-row="${P.esc(row.id)}" data-f="${P.esc(row.id)}:switch" ${row.current ? "checked" : ""}><span class="track"></span><span class="sr">${P.esc(row.title)}</span></label>`;
}

function stepperHtml(row) {
  const number = row.number;
  const value = shownValue(row);
  const text = formatNumber(number, value);
  return `<div class="toggles-step" data-step="${P.esc(row.id)}">
    <button type="button" class="toggles-step-btn" data-dir="-1" data-f="${P.esc(row.id)}:dec" aria-label="Decrease ${P.esc(row.title)}" ${value <= number.min ? "disabled" : ""}>${P.ic("minus")}</button>
    <span class="toggles-value num" role="spinbutton" tabindex="0" data-f="${P.esc(row.id)}:value" aria-label="${P.esc(row.title)}" aria-valuenow="${value}" aria-valuemin="${number.min}" aria-valuemax="${number.max}" aria-valuetext="${P.esc(text)}" title="Tap to type a value">${P.esc(text)}</span>
    <button type="button" class="toggles-step-btn" data-dir="1" data-f="${P.esc(row.id)}:inc" aria-label="Increase ${P.esc(row.title)}" ${value >= number.max ? "disabled" : ""}>${P.ic("plus")}</button>
  </div>`;
}

function pillsHtml(row, label) {
  const act = row.actions ? "toggles-action" : "toggles-option";
  return `<div class="seg toggles-seg" role="group" aria-label="${P.esc(label)}">${row.options.map(option => {
    const pressed = option.value === row.current;
    return `<button type="button" data-act="${act}" data-row="${P.esc(row.id)}" data-label="${P.esc(option.label)}" data-value="${P.esc(JSON.stringify(option.value))}" data-f="${P.esc(row.id)}:option${P.esc(option.value)}" aria-pressed="${pressed}"><span>${P.esc(option.label)}</span></button>`;
  }).join("")}</div>`;
}

function chipsHtml(row) {
  const off = row.widget === "button_toggle" && !row.current;
  return row.sub_toggles.map(sub => `<button type="button" class="chip toggles-chip" data-act="toggles-chip" data-row="${P.esc(row.id)}" data-key="${P.esc(sub.key)}" data-f="${P.esc(row.id)}:${P.esc(sub.key)}" aria-pressed="${sub.current}" ${off ? "disabled" : ""}>${P.esc(sub.label)}</button>`).join("");
}

function hasReset(row) {
  return (row.buttons || []).includes("Reset");
}

function displayHtml(text) {
  return text === undefined ? "" : `<span class="toggles-display">${P.esc(text)}</span>`;
}

function pageHtml(row) {
  return `<a class="btn btn-quiet btn-small toggles-btn" href="${row.link === "home" ? "/" : `/${row.link}`}" data-f="${P.esc(row.id)}:link">${P.esc(PAGE_LABELS[row.link])}${P.ic("next", "ic-s")}</a>`;
}

function isDanger(label) {
  return DANGER_LABELS.includes(label);
}

function actionsHtml(row) {
  if (row.checkable) {
    return { html: pillsHtml(row, row.title), wide: row.options.length > 2 ? "wide" : "" };
  }
  const buttons = row.actions.map(action => `<button type="button" class="btn ${isDanger(action.label) ? "btn-danger-quiet" : "btn-quiet"} btn-small toggles-btn" data-act="toggles-action" data-row="${P.esc(row.id)}" data-label="${P.esc(action.label)}" data-f="${P.esc(row.id)}:action:${P.esc(action.label)}">${P.esc(titleCase(action.label))}</button>`);
  return { html: displayHtml(row.display) + buttons.join("") };
}

function offered(options) {
  return options.filter(option => !option.locked);
}

function pickerValue(row) {
  const options = offered(row.picker.steps[0].options);
  return options.some(option => option.value === row.current) ? row.current : options[0].value;
}

function pickerText(row) {
  if (row.display !== undefined) {
    return row.display;
  }
  return row.picker.steps[0].options.find(option => option.value === pickerValue(row)).label;
}

function pickerHtml(row) {
  return `${displayHtml(pickerText(row))}<button type="button" class="btn btn-quiet btn-small toggles-btn" data-act="toggles-pick" data-row="${P.esc(row.id)}" data-f="${P.esc(row.id)}:pick" aria-label="Select ${P.esc(row.title)}">Select</button>`;
}

function lockedPickerHtml(row) {
  const reason = disabledText(row.disabled);
  return `${displayHtml(pickerText(row))}<span class="toggles-locked" title="${P.esc(reason)}"><button type="button" class="btn btn-quiet btn-small toggles-btn" aria-label="Select ${P.esc(row.title)}" aria-describedby="toggles-why-${P.esc(row.id)}" disabled>Select</button></span><span class="sr" id="toggles-why-${P.esc(row.id)}">${P.esc(reason)}</span>`;
}

function controlHtml(row) {
  if (row.link) {
    return { html: displayHtml(row.display) + pageHtml(row) };
  }
  if (row.locked && row.disabled && row.picker) {
    return { html: lockedPickerHtml(row), wide: "pick" };
  }
  if (row.locked) {
    return { html: row.disabled ? `${displayHtml(row.display)}<span class="toggles-hint">${P.esc(disabledText(row.disabled))}</span>` : "" };
  }
  if (row.actions && !row.writable) {
    return actionsHtml(row);
  }
  if (row.picker) {
    return { html: pickerHtml(row), wide: "pick" };
  }
  if (!row.writable) {
    return { html: displayHtml(row.display) };
  }
  const note = row.note ? `<span class="toggles-hint">${P.esc(row.note)}</span>` : "";
  if (row.widget === "toggle") {
    return { html: note + switchHtml(row) };
  }
  if (row.widget === "manage") {
    const child = row.children[0];
    const manage = row.manage_visible ? `<a class="btn btn-quiet btn-small toggles-btn toggles-manage ${row.current ? "" : "is-off"}" href="${href(child)}" data-f="${P.esc(row.id)}:manage" ${row.current ? "" : 'aria-disabled="true" tabindex="-1"'}>Manage${P.ic("next", "ic-s")}</a>` : "";
    return { html: manage + switchHtml(row) };
  }
  if (NUMBER_WIDGETS.includes(row.widget)) {
    const reset = hasReset(row) ? `<button type="button" class="btn btn-quiet btn-small toggles-btn" data-act="toggles-reset" data-row="${P.esc(row.id)}" data-f="${P.esc(row.id)}:default">Reset</button>` : "";
    return { html: stepperHtml(row) + reset, opts: row.sub_toggles ? chipsHtml(row) : "", wide: "step" };
  }
  if (row.widget === "button_toggle") {
    return { html: note + switchHtml(row), opts: chipsHtml(row) };
  }
  if (row.options) {
    return { html: pillsHtml(row, row.title), wide: row.options.length > 2 ? "wide" : "" };
  }
  return { html: "" };
}

function markHtml(row) {
  const parts = [];
  if (row.reboot) {
    parts.push(`<span class="toggles-tag" title="Reboot to apply">${P.ic("refresh", "ic-s")}<span class="toggles-tag-txt">Reboot to apply</span></span>`);
  }
  if (row.writable && !row.locked && !hasReset(row)) {
    if (row.changed) {
      const text = `Default ${defaultText(row)}`;
      parts.push(`<button type="button" class="toggles-reset" data-act="toggles-reset" data-row="${P.esc(row.id)}" data-f="${P.esc(row.id)}:reset" title="${P.esc(text)}" aria-label="Reset ${P.esc(row.title)}. ${P.esc(text)}">Reset</button>`);
    } else {
      parts.push('<span class="toggles-reset is-space" aria-hidden="true">Reset</span>');
    }
  }
  return parts.length ? `<div class="toggles-mark">${parts.join("")}</div>` : "";
}

function linkRowHtml(row) {
  const child = row.children[0];
  const count = changedCount(child);
  return `<div class="toggles-row toggles-linkrow" role="listitem" id="toggles-row-${P.esc(row.id)}"><a class="toggles-link" href="${href(child)}" data-f="${P.esc(row.id)}:link">
    <span class="toggles-head"><span class="toggles-ltitle">${P.esc(row.title)}${count ? `<span class="toggles-count">${count} changed</span>` : ""}</span><span class="toggles-lead">${P.esc(leadOf(row.description))}</span></span>
    ${P.ic("next", "toggles-chev")}
  </a></div>`;
}

function partHtml(row, base) {
  const control = controlHtml(row);
  const open = openDescriptions.has(row.id);
  const inside = row.widget === "manage" ? changedCount(row.children[0]) : 0;
  const cut = row.title.lastIndexOf(" ") + 1;
  const dot = row.changed ? '<span class="toggles-dot" aria-hidden="true"></span>' : "";
  const title = row.description
    ? `<button type="button" class="toggles-title" data-act="toggles-desc" data-row="${P.esc(row.id)}" data-f="${P.esc(row.id)}:title" aria-expanded="${open}" aria-controls="toggles-desc-${P.esc(row.id)}">${dot}<span>${P.esc(row.title.slice(0, cut))}<span class="toggles-nowrap">${P.esc(row.title.slice(cut))}${P.NB}${P.ic("down", "toggles-caret")}</span></span></button>`
    : `<span class="toggles-title is-static">${dot}${P.esc(row.title)}</span>`;
  const classes = [base, control.wide ? `w-${control.wide}` : "", control.opts ? "has-opts" : "", row.widget === "manage" ? "is-manage" : ""];
  return `<div class="${classes.join(" ")}" id="toggles-row-${P.esc(row.id)}" data-row="${P.esc(row.id)}" ${base === "toggles-row" ? 'role="listitem"' : ""}>
    <div class="toggles-head">${title}${row.changed ? '<span class="sr">Changed from default.</span>' : ""}${inside ? `<span class="toggles-count">${inside} changed</span>` : ""}${markHtml(row)}</div>
    ${row.description ? `<div class="toggles-desc" id="toggles-desc-${P.esc(row.id)}" ${open ? "" : "hidden"}>${rich(row.description)}</div>` : ""}
    ${control.opts ? `<div class="toggles-opts">${control.opts}</div>` : ""}
    <div class="toggles-ctl">${NUMBER_WIDGETS.includes(row.widget) && control.html ? saveMark(row) : ""}${control.html}</div>
  </div>`;
}

function rowHtml(row) {
  if (!row.link && row.widget !== "manage" && row.children.length) {
    return linkRowHtml(row);
  }
  const partner = data.rows.find(candidate => candidate.pair === row.id);
  if (!partner) {
    return partHtml(row, "toggles-row");
  }
  return `<div class="toggles-row is-pair" role="listitem">${partHtml(row, "toggles-part")}${partHtml(partner, "toggles-part")}</div>`;
}

function hiddenHtml(path) {
  const rows = panelRows(path).filter(row => row.hidden && row.hidden.kind !== "state");
  if (!rows.length) {
    return "";
  }
  const byLevel = new Map();
  const other = [];
  for (const row of rows) {
    if (row.hidden.kind === "level") {
      if (!byLevel.has(row.hidden.level)) {
        byLevel.set(row.hidden.level, []);
      }
      byLevel.get(row.hidden.level).push(row);
    } else {
      other.push(row);
    }
  }
  const lines = [...byLevel.entries()].sort((a, b) => a[0] - b[0]).map(([level, list]) => `<li class="toggles-hl" data-hidden="${P.esc(list.map(row => row.id).join(" "))}">${P.ic("lock", "ic-s")}<span><b>More at ${P.esc(levelName(level))}</b> <span class="muted">${P.esc(list.map(row => row.title).join(", "))}</span></span><button type="button" class="btn btn-text btn-small toggles-raise" data-act="toggles-level" data-value="${level}" data-f="raise:${level}">Switch to ${P.esc(levelName(level))}</button></li>`);
  for (const row of other) {
    lines.push(`<li class="toggles-hl" data-hidden="${P.esc(row.id)}">${P.ic("info", "ic-s")}<span><b>${P.esc(row.title)}</b> <span class="muted">${P.esc(reasonText(row.hidden))}</span></span></li>`);
  }
  return `<div class="toggles-hidden"><h3>Not shown here</h3><ul>${lines.join("")}</ul></div>`;
}

function changedHtml(path) {
  const here = pathRows(path).filter(row => row.changed && !row.locked).sort((a, b) => a.order - b.order);
  const deeper = rowsUnder(path).filter(row => row.changed && !row.locked && row.widget !== "manage" && pathKey(row.path) !== pathKey(path)).length;
  if (!here.length && !deeper) {
    return `<h3>Changed on this page</h3><p class="muted">Everything here is on its default.</p>`;
  }
  return `<h3>Changed on this page</h3><ul class="toggles-clist">${here.map(row => `<li><a href="#toggles-row-${P.esc(row.id)}" data-act="toggles-jump" data-row="${P.esc(row.id)}">${P.esc(row.title)}</a><span class="num">${P.esc(currentText(row))}</span></li>`).join("")}${deeper ? `<li class="muted">${deeper} more in deeper pages</li>` : ""}</ul>`;
}

function defaultText(row) {
  if (row.number) {
    return formatNumber(row.number, clamp(row.default, row.number));
  }
  if (row.options) {
    return row.options.find(option => option.value === row.default)?.label || "";
  }
  return row.default ? "on" : "off";
}

function currentText(row) {
  if (row.display !== undefined) {
    return row.display;
  }
  if (row.number) {
    return formatNumber(row.number, displayed(row));
  }
  if (row.options) {
    return row.options.find(option => option.value === row.current)?.label || "";
  }
  if (row.widget === "button_toggle") {
    return [row.current ? "On" : "Off", ...row.sub_toggles.filter(sub => sub.current).map(sub => sub.label)].join(", ");
  }
  return row.current ? "On" : "Off";
}

function rebootHtml(inline) {
  const rows = data.rows.filter(row => row.reboot);
  if (!rows.length) {
    return "";
  }
  return `<div class="toggles-reboot ${inline ? "is-inline" : ""}" role="status">
    ${P.ic("refresh")}
    <div class="toggles-rtext"><b>Reboot to apply</b><span>${P.esc(rows.map(row => row.title).join(", "))}</span></div>
    <button type="button" class="btn btn-primary btn-small toggles-btn" data-act="toggles-reboot" data-f="reboot">Reboot now</button>
  </div>`;
}

function levelHtml() {
  const row = tuningRow();
  return `<section class="toggles-level" id="toggles-row-TuningLevel" aria-labelledby="toggles-level-h">
    <div class="toggles-level-text">
      <h2 id="toggles-level-h">${P.esc(row.title)}</h2>
      <p>${rich(row.description.split("<br><br>")[0])}</p>
    </div>
    <div class="toggles-level-ctl">
      <div class="seg toggles-seg toggles-lseg" role="group" aria-label="${P.esc(row.title)}">${row.options.map(option => `<button type="button" data-act="toggles-level" data-value="${option.value}" data-f="TuningLevel:${option.value}" aria-pressed="${option.value === data.level}"><span>${P.esc(option.label)}</span></button>`).join("")}</div>
      <p class="toggles-level-cap" aria-live="polite">${P.esc((levelLines()[data.level] || "").split(" - ").slice(1).join(" - "))}</p>
    </div>
  </section>`;
}

function sectionHtml(section) {
  const shown = section.options.filter(option => !option.locked);
  const hidden = section.options.filter(option => option.locked);
  const total = changedCount([section.title]);
  const parts = section.description.split("<br><br>");
  const hints = hidden.map(option => `${P.esc(titleCase(option.label))} ${option.locked.kind === "level" ? `appears at ${P.esc(levelName(option.locked.level))}` : "isn't available on this car"}`);
  return `<section class="toggles-sec" id="toggles-sec-${slug(section.title)}" aria-labelledby="toggles-sh-${slug(section.title)}">
    <div class="toggles-sec-head">${P.ic(SECTION_ICONS[section.title])}<h2 id="toggles-sh-${slug(section.title)}">${P.esc(section.title)}</h2>${total ? `<span class="toggles-count">${total} changed</span>` : ""}</div>
    <p class="toggles-sec-desc">${rich(parts[0])}</p>
    ${parts[1] ? `<p class="toggles-sec-note">${rich(parts[1])}</p>` : ""}
    <div class="toggles-subs">${shown.map(option => {
      const count = changedCount(option.children);
      const label = option.label === "MANAGE" ? "Manage" : titleCase(option.label);
      return `<a class="toggles-subbtn" href="${href(option.children)}" data-f="sub:${viewOf(option.children)}"><span>${P.esc(label)}</span>${count ? `<span class="toggles-n" aria-label="${count} changed">${count}</span>` : ""}${P.ic("next", "ic-s")}</a>`;
    }).join("")}</div>
    ${hints.length ? `<p class="toggles-sec-hint">${P.ic("lock", "ic-s")}<span>${hints.join(". ")}.</span></p>` : ""}
  </section>`;
}

function rootHtml() {
  return `<div class="toggles-root">${levelHtml()}${rebootHtml(true)}<div class="toggles-secs">${sections().map(sectionHtml).join("")}</div></div>`;
}

function offSwitch(path) {
  let parent = opener(path);
  while (parent) {
    if (parent.widget === "manage" && !parent.current && !parent.locked) {
      return parent;
    }
    parent = opener(parent.path);
  }
  return null;
}

function panelHtml(path) {
  const rows = panelRows(path);
  const shown = rows.filter(row => !row.hidden);
  const about = aboutOf(path);
  const parent = offSwitch(path);
  const blocked = shown.length && shown.every(row => row.locked && !row.disabled) ? shown[0].locked : null;
  let off = "";
  if (parent) {
    off = `<p class="notice toggles-off">${P.ic("info", "ic-s")}<span>Turn on <a href="/toggles?key=${encodeURIComponent(parent.key)}" data-f="off:${P.esc(parent.id)}">“${P.esc(parent.title)}”</a> to change these.</span></p>`;
  } else if (blocked) {
    off = `<p class="notice toggles-off">${P.ic("info", "ic-s")}<span>${P.esc(reasonText(blocked))}</span></p>`;
  }
  let list;
  if (!rows.length) {
    list = `<div class="empty toggles-empty"><h3>Not in The Pond yet</h3><p>Change these settings on the device for now.</p></div>`;
  } else if (!shown.length) {
    list = `<div class="empty toggles-empty"><h3>Nothing here at ${P.esc(levelName(data.level))}</h3><p>Every setting on this page is above your tuning level or doesn't apply to this car.</p></div>`;
  } else {
    list = `<div class="toggles-list" role="list">${shown.map(rowHtml).join("")}</div>`;
  }
  return `${crumbsHtml(path)}
  <div class="toggles-page">
    <div class="toggles-main">
      <p class="toggles-intro">${P.esc(leadOf(about.description))}</p>
      ${off}
      ${list}
      ${hiddenHtml(path)}
    </div>
    <aside class="toggles-aside" aria-label="About this page">
      <div class="toggles-ablock"><h3>${P.esc(about.title)}</h3><p>${rich(about.description)}</p></div>
      <div class="toggles-ablock">${changedHtml(path)}</div>
    </aside>
  </div>`;
}

function bodyHtml(direction = 0) {
  const isPanel = view.length > 1;
  const motion = P.reducedMotion.matches || !direction ? "" : direction > 0 ? "enter-fwd" : "enter-back";
  return `<div class="toggles-view ${motion}">${isPanel ? panelHtml(view) : rootHtml()}</div>${isPanel ? rebootHtml(false) : ""}`;
}

function unitsHtml() {
  return `<span class="toggles-units-label">Units</span><div class="seg toggles-units" role="group" aria-label="Units"><button type="button" data-act="toggles-units" data-value="0" data-f="units:0" aria-pressed="${!data.metric}">mph</button><button type="button" data-act="toggles-units" data-value="1" data-f="units:1" aria-pressed="${data.metric}">km/h</button></div>`;
}

function focusKey() {
  return document.activeElement?.closest?.("[data-f]")?.dataset.f || null;
}

function focusElement(element) {
  if (!element.matches("a[href], button, input, [tabindex]")) {
    element.tabIndex = -1;
  }
  element.focus({ preventScroll: true });
}

function restoreFocus(...keys) {
  for (const key of keys) {
    let element = null;
    if (key && key.startsWith("row:")) {
      element = document.getElementById(`toggles-row-${key.slice(4)}`);
    } else if (key) {
      element = document.querySelector(`[data-f="${CSS.escape(key)}"]`);
    }
    if (element && !element.disabled) {
      focusElement(element);
      return;
    }
  }
}

function redraw(focusKeys = [focusKey()]) {
  if (!root || !root.isConnected) {
    return;
  }
  root.innerHTML = bodyHtml();
  if (!focusKeys.some(Boolean)) {
    return;
  }
  restoreFocus(...focusKeys);
  if (!root.contains(document.activeElement) && !document.getElementById("topbar").contains(document.activeElement)) {
    root.focus({ preventScroll: true });
  }
}

function merged(payload) {
  if (payload.rows) {
    return payload;
  }
  const { state, ...page } = payload;
  for (const row of data.rows) {
    if (!writes.get(row.id)) {
      Object.assign(row, state[row.id]);
    }
  }
  return { ...data, ...page };
}

function apply(payload, focusKeys) {
  if (hold || editing) {
    pending = payload;
    return;
  }
  pending = null;
  data = merged(payload);
  redraw(focusKeys);
}

function announce(text) {
  let live = document.getElementById("toggles-live");
  if (!live) {
    live = document.createElement("p");
    live.id = "toggles-live";
    live.className = "sr";
    live.setAttribute("aria-live", "polite");
    document.body.append(live);
  }
  live.textContent = text;
}

function showDone(selector, previous) {
  const control = document.querySelector(selector);
  if (control && control !== previous) {
    const mark = document.createElement("span");
    mark.className = "busy-mark";
    mark.setAttribute("aria-hidden", "true");
    mark.innerHTML = P.ic("check");
    control.style.setProperty("--busy-color", getComputedStyle(control).color);
    control.classList.add("is-busy");
    control.append(mark);
    setTimeout(() => {
      mark.remove();
      control.classList.remove("is-busy");
      control.style.removeProperty("--busy-color");
    }, DONE_TIME);
  }
  announce("Saved");
}

function flushPending() {
  if (pending && !hold && !editing) {
    apply(pending);
  }
}

function rowById(id) {
  return data.rows.find(row => row.id === id);
}

async function reload() {
  try {
    apply(await P.get("/api/toggles"));
  } catch (error) {
    P.fail(error, "Couldn't reload the settings");
  }
}

async function softReboot() {
  try {
    await P.request("POST", "/api/device/soft_reboot");
  } catch (error) {
    P.fail(error, "Couldn't reboot", MESSAGES);
    return;
  }
  P.restart("Soft rebooting");
}

async function promptReboot() {
  if (await P.dialog({ title: "Reboot", body: `<p>${P.esc(REBOOT_TEXT)}</p>`, confirm: "Reboot Now", cancel: "Reboot Later" })) {
    softReboot();
  }
}

async function save(row, key, value, selector, fallbackFocus = "") {
  const focus = focusKey();
  const control = root ? root.querySelector(selector) : null;
  const isSwitch = Boolean(control?.classList.contains("track"));
  const target = key === row.key ? row : row.sub_toggles.find(sub => sub.key === key);
  const previous = target.current;
  target.current = value;
  writes.set(row.id, (writes.get(row.id) || 0) + 1);
  const request = P.request("PUT", `/api/toggles/${encodeURIComponent(key)}`, { value });
  let result;
  try {
    result = await (control && !isSwitch ? P.busy(control, request) : request);
  } catch (error) {
    target.current = previous;
    redraw();
    P.fail(error, `Couldn't change ${P.esc(row.title)}`, MESSAGES);
    reload();
    return;
  } finally {
    writes.set(row.id, writes.get(row.id) - 1);
  }
  if (isSwitch) {
    await Promise.allSettled(control.getAnimations({ subtree: true }).map(animation => animation.finished));
  }
  apply(result, [focus, fallbackFocus]);
  if (isSwitch) {
    announce("Saved");
  } else {
    showDone(selector, control);
  }
  if (result.notice) {
    await showNotice(row, result.notice);
  }
  if (result.reboot) {
    promptReboot();
  }
}

function feedbackSelector(row, value) {
  if (NUMBER_WIDGETS.includes(row.widget)) {
    return `[data-save="${CSS.escape(row.id)}"]`;
  }
  if (row.picker) {
    return `[data-f="${CSS.escape(`${row.id}:pick`)}"]`;
  }
  if (row.options) {
    return `[data-f="${CSS.escape(`${row.id}:option${value}`)}"]`;
  }
  return `[data-f="${CSS.escape(`${row.id}:switch`)}"] + .track`;
}

function confirmDialog(row, confirm, danger = false) {
  const tone = danger ? { confirmTone: "danger", icon: "alert", tone: "danger" } : {};
  return P.dialog({ ...tone, title: P.esc(row.title), body: `<p>${rich(confirm.text || row.description)}</p>`, confirm: P.esc(confirm.confirm), cancel: P.esc(confirm.cancel), wide: !confirm.text });
}

async function showNotice(row, notice) {
  const ok = await P.dialog({ title: P.esc(row.title), body: `<p>${rich(notice.text)}</p>`, confirm: P.esc(notice.confirm), cancel: P.esc(notice.cancel) });
  if (ok && notice.action) {
    runAction(row, notice.action, null);
  }
}

async function runAction(row, label, button) {
  const action = row.actions.find(candidate => candidate.label === label);
  if (action.blocked) {
    await P.dialog({ title: P.esc(row.title), body: `<p>${P.esc(action.messages[action.blocked] || new P.PondError(409, action.blocked).message)}</p>`, confirm: "Ok", cancel: "" });
    return;
  }
  if (action.confirm && !(await confirmDialog(row, action.confirm, isDanger(label)))) {
    return;
  }
  const focus = focusKey();
  const request = P.request("POST", `/api/toggles/${encodeURIComponent(row.id)}/actions/${encodeURIComponent(label)}`, {});
  let result;
  try {
    result = await (button ? P.busy(button, request) : request);
  } catch (error) {
    P.fail(error, P.esc(row.title), { ...MESSAGES, ...action.messages });
    reload();
    return;
  }
  if (action.restart && result.succeeded) {
    P.restart(action.restart);
    return;
  }
  apply(result, [focus]);
  if (button && result.succeeded) {
    showDone(`[data-f="${CSS.escape(button.dataset.f)}"]`, button);
  }
  if (action.status) {
    P.toast(P.esc(result.succeeded ? action.status.success : action.status.failure), "", result.succeeded ? "check" : "alert");
  }
}

async function pickOne(title, options, current) {
  let picked = options.findIndex(option => option.value === current);
  const body = `<div class="toggles-picks" role="radiogroup" aria-label="${P.esc(title)}">${options.map((option, index) => `<button type="button" class="menu-item" role="radio" data-index="${index}" aria-checked="${index === picked}">${P.ic("check")}<span>${P.esc(option.label)}</span></button>`).join("")}</div>`;
  const onOpen = dialog => {
    const confirm = dialog.querySelector("[data-confirm]");
    confirm.disabled = true;
    dialog.querySelectorAll(".toggles-picks [role='radio']").forEach(button => button.addEventListener("click", () => {
      dialog.querySelectorAll(".toggles-picks [role='radio']").forEach(other => other.setAttribute("aria-checked", String(other === button)));
      picked = Number(button.dataset.index);
      confirm.disabled = options[picked].value === current;
    }));
    dialog.querySelector(".toggles-picks [aria-checked='true']")?.scrollIntoView({ block: "center" });
  };
  const dialog = await P.dialog({ icon: "settings", title: P.esc(title), body, confirm: "Select", wide: true, onOpen });
  return dialog ? options[picked].value : undefined;
}

async function pick(row, button) {
  const selector = `[data-f="${CSS.escape(button.dataset.f)}"]`;
  const { fill, steps, stops } = row.picker;
  if (steps.length === 1) {
    const value = await pickOne(steps[0].title, offered(steps[0].options), row.display !== undefined ? row.current : pickerValue(row));
    if (value !== undefined) {
      save(row, row.key, value, selector);
    }
    return;
  }
  const picks = [];
  for (const step of steps) {
    const options = offered(step.options).filter(option => !picks.includes(option.value));
    if (options.length === 1 && options[0].value === fill) {
      break;
    }
    const value = await pickOne(step.title, options, null);
    if (value === undefined) {
      break;
    }
    picks.push(value);
    if (stops.includes(value)) {
      break;
    }
  }
  if (picks.length) {
    save(row, row.key, picks, selector);
  }
}

function warnFirst(row) {
  if (!row.warning || warned.has(row.id)) {
    return false;
  }
  warned.add(row.id);
  P.dialog({ icon: "alert", title: P.esc(row.title), body: `<p>${rich(row.warning)}</p>`, confirm: "Ok", cancel: "" });
  return true;
}

function shownValue(row) {
  return localValues.has(row.id) ? localValues.get(row.id) : displayed(row);
}

function commitNumber(id) {
  const value = localValues.get(id);
  clearTimeout(saveTimers.get(id));
  saveTimers.delete(id);
  localValues.delete(id);
  const row = rowById(id);
  const factor = factorOf(row.number);
  if (roundHalfAway(row.current * factor) === roundHalfAway(value * factor)) {
    return;
  }
  save(row, row.key, value, `[data-save="${CSS.escape(id)}"]`);
}

function saveNumber(id, value) {
  clearTimeout(saveTimers.get(id));
  localValues.set(id, value);
  saveTimers.set(id, setTimeout(() => commitNumber(id), SAVE_DELAY));
}

function paintStepper(row, value) {
  const stepper = root.querySelector(`[data-step="${CSS.escape(row.id)}"]`);
  if (!stepper) {
    return;
  }
  const text = formatNumber(row.number, value);
  const label = stepper.querySelector(".toggles-value");
  label.textContent = text;
  label.setAttribute("aria-valuenow", value);
  label.setAttribute("aria-valuetext", text);
  stepper.querySelector('[data-dir="-1"]').disabled = value <= row.number.min;
  stepper.querySelector('[data-dir="1"]').disabled = value >= row.number.max;
}

function stepOnce(state) {
  const number = state.row.number;
  const factor = factorOf(number);
  const delta = number.fast && state.accelerated ? number.step * FAST_STEPS : number.step;
  state.value = clamp(roundHalfAway(clamp(state.value + state.direction * delta, number) * factor) / factor, number);
  if (Math.abs(state.value - state.start) > FAST_STEPS * number.step && roundHalfAway(state.value / number.step) % FAST_STEPS === 0) {
    state.accelerated = true;
  }
  paintStepper(state.row, state.value);
}

function startHold(button) {
  const row = rowById(button.closest("[data-step]").dataset.step);
  clearTimeout(saveTimers.get(row.id));
  const value = shownValue(row);
  hold = { accelerated: false, button, direction: Number(button.dataset.dir), row, start: value, timer: 0, value };
  stepOnce(hold);
  const repeat = () => {
    if (!hold) {
      return;
    }
    stepOnce(hold);
    if (hold.button.disabled) {
      stopHold();
      return;
    }
    hold.timer = setTimeout(repeat, HOLD_INTERVAL);
  };
  hold.timer = setTimeout(repeat, HOLD_DELAY);
}

function stopHold() {
  if (!hold) {
    return;
  }
  clearTimeout(hold.timer);
  const { row, value } = hold;
  hold = null;
  saveNumber(row.id, value);
  flushPending();
}

function snap(number, value) {
  const factor = factorOf(number);
  const clamped = clamp(value, number);
  const snapped = number.min + Math.round((clamped - number.min) / number.step) * number.step;
  return clamp(roundHalfAway(snapped * factor) / factor, number);
}

function editValue(label) {
  const row = rowById(label.closest("[data-step]").dataset.step);
  const number = row.number;
  const input = document.createElement("input");
  input.className = "toggles-value toggles-edit num";
  input.type = "text";
  input.inputMode = number.min < 0 ? "text" : number.decimals > 0 ? "decimal" : "numeric";
  input.value = shownValue(row).toFixed(Math.max(number.decimals, 0));
  input.style.width = `${label.offsetWidth}px`;
  input.setAttribute("aria-label", `${row.title}, from ${formatNumber(number, number.min)} to ${formatNumber(number, number.max)}`);
  label.replaceWith(input);
  editing = true;
  input.focus();
  input.select();
  let done = false;
  const finish = commit => {
    if (done) {
      return;
    }
    done = true;
    editing = false;
    if (commit) {
      const raw = input.value.trim();
      const labelMatch = number.labels.find(([, text]) => text.toLowerCase() === raw.toLowerCase());
      const parsed = labelMatch ? labelMatch[0] : parseFloat(raw.replace(",", "."));
      if (Number.isFinite(parsed)) {
        const value = snap(number, parsed);
        redraw();
        if (value !== shownValue(row) && !warnFirst(row)) {
          paintStepper(row, value);
          saveNumber(row.id, value);
        }
        restoreFocus(`${row.id}:value`);
        flushPending();
        return;
      }
    }
    redraw();
    restoreFocus(`${row.id}:value`);
    flushPending();
  };
  input.addEventListener("keydown", event => {
    if (event.key === "Enter") {
      event.preventDefault();
      finish(true);
    } else if (event.key === "Escape") {
      event.preventDefault();
      event.stopPropagation();
      finish(false);
    }
  });
  input.addEventListener("blur", () => finish(true));
}

async function changeLevel(level, button, fallbackFocus) {
  if (level === data.level) {
    return;
  }
  const option = tuningRow().options[level];
  if (option.confirm && !(await P.dialog({ title: P.esc(tuningRow().title), body: `<p>${rich(option.confirm)}</p>`, confirm: P.esc(option.confirm_button), cancel: "Cancel" }))) {
    return;
  }
  save(tuningRow(), "TuningLevel", level, `[data-f="${CSS.escape(button.dataset.f)}"]`, fallbackFocus);
}

function flash(element) {
  element.scrollIntoView({ behavior: P.reducedMotion.matches ? "auto" : "smooth", block: "center" });
  element.classList.add("search-flash");
  setTimeout(() => element.classList.remove("search-flash"), HIGHLIGHT_TIME);
}

function targetFor(key) {
  const row = data.rows.find(candidate => candidate.key === key || candidate.id === key || (candidate.keys || []).includes(key))
    || data.rows.find(candidate => (candidate.sub_toggles || []).some(sub => sub.key === key));
  if (!row) {
    return null;
  }
  if (!row.path.length) {
    return { path: [], selector: `#toggles-row-${row.id}` };
  }
  if (row.locked && !row.hidden) {
    let path = row.path;
    let parent = opener(path);
    while (parent && parent.locked) {
      path = parent.path;
      parent = opener(path);
    }
    if (parent) {
      return { path: parent.path.length > 1 ? parent.path : [], selector: `#toggles-row-${parent.id}`, toast: `Turn on “${parent.title}” to change “${row.title}”` };
    }
  }
  return { path: row.path, selector: row.hidden ? `[data-hidden~="${row.id}"]` : `#toggles-row-${row.id}` };
}

async function showFirstOpen() {
  firstOpenShown = true;
  const onOpen = dialog => {
    const keepOpen = event => event.preventDefault();
    const keepOpenOnEscape = event => {
      if (event.key === "Escape") {
        event.preventDefault();
      }
    };
    dialog.addEventListener("cancel", keepOpen);
    dialog.addEventListener("keydown", keepOpenOnEscape);
    dialog.addEventListener("close", () => {
      dialog.removeEventListener("cancel", keepOpen);
      dialog.removeEventListener("keydown", keepOpenOnEscape);
    }, { once: true });
  };
  const ok = await P.dialog({ icon: "settings", title: P.esc(tuningRow().title), body: `<p>${rich(data.first_open.message)}</p>`, confirm: "Ok", cancel: "", onOpen });
  if (!ok) {
    return;
  }
  try {
    apply(await P.request("POST", "/api/toggles/tuning_level"));
  } catch (error) {
    P.fail(error, "Couldn't set the tuning level", MESSAGES);
  }
}

P.act("toggles-desc", button => {
  const row = rowById(button.dataset.row);
  if (openDescriptions.has(row.id)) {
    openDescriptions.delete(row.id);
  } else {
    openDescriptions.add(row.id);
  }
  button.setAttribute("aria-expanded", String(openDescriptions.has(row.id)));
  root.querySelector(`#toggles-desc-${CSS.escape(row.id)}`).hidden = !openDescriptions.has(row.id);
});

P.act("toggles-level", button => {
  const level = Number(button.dataset.value);
  const revealed = button.closest("[data-hidden]");
  changeLevel(level, button, revealed ? `row:${revealed.dataset.hidden.split(" ")[0]}` : `TuningLevel:${level}`);
});

P.act("toggles-option", button => {
  const row = rowById(button.dataset.row);
  const value = JSON.parse(button.dataset.value);
  if (value === row.current) {
    return;
  }
  save(row, row.key, value, `[data-f="${CSS.escape(button.dataset.f)}"]`);
});

P.act("toggles-chip", button => {
  const row = rowById(button.dataset.row);
  const sub = row.sub_toggles.find(candidate => candidate.key === button.dataset.key);
  const value = row.exclusive || !sub.current;
  button.setAttribute("aria-pressed", String(value));
  save(row, sub.key, value, `[data-f="${CSS.escape(button.dataset.f)}"]`);
});

P.act("toggles-reset", async button => {
  const row = rowById(button.dataset.row);
  const confirm = row.reset_confirm || (row.default === true && !row.current ? row.confirm_enable : null);
  if (confirm && !(await confirmDialog(row, confirm))) {
    return;
  }
  clearTimeout(saveTimers.get(row.id));
  saveTimers.delete(row.id);
  localValues.delete(row.id);
  save(row, row.key, row.default, feedbackSelector(row, row.default), `${row.id}:title`);
});

P.act("toggles-action", button => runAction(rowById(button.dataset.row), button.dataset.label, button));

P.act("toggles-pick", button => pick(rowById(button.dataset.row), button));

P.act("toggles-reboot", async () => promptReboot());

P.act("toggles-units", async button => {
  const metric = button.dataset.value === "1";
  if (metric === data.metric) {
    return;
  }
  let result;
  try {
    result = await P.busy(button, P.request("POST", "/api/toggles/metric", { metric }));
  } catch (error) {
    P.fail(error, "Couldn't change the units", MESSAGES);
    return;
  }
  P.state.is_metric = result.metric;
  pending = result;
  await P.refresh();
  restoreFocus(button.dataset.f);
  showDone(`[data-f="${button.dataset.f}"]`, button);
});

window.addEventListener("popstate", () => {
  restoring = true;
});

P.page("toggles", {
  title: "Toggles",
  async render(params) {
    if (pending) {
      data = merged(pending);
    } else {
      data = data ? merged(await P.get("/api/toggles/state")) : await P.get("/api/toggles");
    }
    pending = null;
    let target = null;
    if (params.get("key")) {
      target = targetFor(params.get("key"));
      view = target ? target.path : [];
    } else {
      view = pathFromView(params.get("view") || "");
    }
    const owner = view.length > 1 ? opener(view) : null;
    if (owner?.link) {
      P.go(owner.link === "home" ? "/" : `/${owner.link}`, { replace: true });
      return { body: "" };
    }
    const direction = Math.sign(view.length - lastDepth);
    lastDepth = view.length;
    const parent = view.length > 2 ? view.slice(0, -1) : [];
    const isPanel = view.length > 1;
    return {
      actions: unitsHtml(),
      body: bodyHtml(direction),
      crumb: isPanel ? { href: href(parent), label: parent.length ? P.esc(panelTitle(parent)) : "Toggles" } : null,
      docTitle: isPanel ? panelTitle(view) : "Toggles",
      title: P.esc(isPanel ? panelTitle(view) : "Toggles"),
      mount: element => {
        root = element;
        warned.clear();
        if (restoring || direction < 0) {
          restoring = false;
          window.scrollTo(0, scrollMemory.get(viewOf(view)) || 0);
        }
        if (params.get("key")) {
          history.replaceState(null, "", href(view));
        }
        let scrollTarget = null;
        if (target) {
          scrollTarget = root.querySelector(target.selector);
          if (scrollTarget) {
            focusElement(scrollTarget);
          } else {
            root.focus({ preventScroll: true });
          }
        } else if (view.length === 1 && !window.scrollY) {
          scrollTarget = root.querySelector(`#toggles-sec-${slug(view[0])}`);
        }
        if (scrollTarget) {
          flash(scrollTarget);
        }
        if (target?.toast) {
          P.toast(P.esc(target.toast), "", "info");
        }
        if (!data.confirmed && data.first_open && !firstOpenShown) {
          showFirstOpen();
        }

        const onChange = async event => {
          const input = event.target.closest('input[role="switch"]');
          if (!input) {
            return;
          }
          const row = rowById(input.dataset.row);
          if (input.checked && row.confirm_enable && !(await confirmDialog(row, row.confirm_enable))) {
            input.checked = false;
            return;
          }
          save(row, row.key, input.checked, `[data-f="${CSS.escape(input.dataset.f)}"] + .track`);
        };
        const onPointerDown = event => {
          const button = event.target.closest(".toggles-step-btn");
          if (!button || button.disabled || event.button !== 0) {
            return;
          }
          event.preventDefault();
          button.focus({ preventScroll: true });
          const pressed = button.isConnected ? button : root.querySelector(`[data-f="${CSS.escape(button.dataset.f)}"]`);
          if (!pressed || pressed.disabled) {
            return;
          }
          pressed.focus({ preventScroll: true });
          if (warnFirst(rowById(pressed.closest("[data-step]").dataset.step))) {
            return;
          }
          pressed.setPointerCapture?.(event.pointerId);
          startHold(pressed);
        };
        const onClick = event => {
          const jump = event.target.closest('[data-act="toggles-jump"]');
          if (jump) {
            event.preventDefault();
            event.stopPropagation();
            const row = root.querySelector(`#toggles-row-${CSS.escape(jump.dataset.row)}`);
            if (row) {
              flash(row);
            }
            return;
          }
          const button = event.target.closest(".toggles-step-btn");
          if (button && event.detail === 0 && !button.disabled) {
            const row = rowById(button.closest("[data-step]").dataset.step);
            if (!warnFirst(row)) {
              const state = { accelerated: false, direction: Number(button.dataset.dir), row, start: shownValue(row), value: shownValue(row) };
              stepOnce(state);
              saveNumber(row.id, state.value);
            }
          }
          const label = event.target.closest("span.toggles-value");
          if (label) {
            editValue(label);
          }
        };
        const onKeyDown = event => {
          const label = event.target.closest("span.toggles-value");
          if (!label) {
            return;
          }
          const row = rowById(label.closest("[data-step]").dataset.step);
          const moves = { ArrowDown: -1, ArrowLeft: -1, ArrowRight: 1, ArrowUp: 1, PageDown: -PAGE_STEPS, PageUp: PAGE_STEPS };
          const number = row.number;
          let value = null;
          if (event.key === "Enter" || event.key === " ") {
            event.preventDefault();
            editValue(label);
          } else if (event.key === "Home") {
            value = number.min;
          } else if (event.key === "End") {
            value = number.max;
          } else if (event.key in moves) {
            const factor = factorOf(number);
            value = clamp(roundHalfAway(clamp(shownValue(row) + moves[event.key] * number.step, number) * factor) / factor, number);
          }
          if (value !== null) {
            event.preventDefault();
            if (warnFirst(row)) {
              return;
            }
            paintStepper(row, value);
            saveNumber(row.id, value);
          }
        };
        root.addEventListener("change", onChange);
        root.addEventListener("pointerdown", onPointerDown);
        root.addEventListener("click", onClick);
        root.addEventListener("keydown", onKeyDown);
        ["pointerup", "pointercancel", "lostpointercapture"].forEach(type => document.addEventListener(type, stopHold));
        const mountedView = viewOf(view);
        return () => {
          scrollMemory.set(mountedView, window.scrollY);
          stopHold();
          [...saveTimers.keys()].forEach(commitNumber);
          root.removeEventListener("change", onChange);
          root.removeEventListener("pointerdown", onPointerDown);
          root.removeEventListener("click", onClick);
          root.removeEventListener("keydown", onKeyDown);
          ["pointerup", "pointercancel", "lostpointercapture"].forEach(type => document.removeEventListener(type, stopHold));
          root = null;
        };
      }
    };
  }
});
})();
