(() => {
"use strict";

const P = window.Pond;

const DOWNLOAD_STOPPED = ["Download cancelled...", "GitHub and GitLab are offline..."];
const MESSAGES = {
  in_use: "The car is using it. Pick another one first",
  invalid_names: "Those aren't available to download anymore",
  invalid_theme: "The device doesn't have that anymore",
  offline: "The device is offline. Connect it to the internet first",
  random_themes: "Random Themes is on. Turn it off in Toggles first"
};
const PARTS = [
  { component: "colors", noun: "color scheme", plural: "color schemes", title: "Color Scheme" },
  { component: "icons", noun: "icon pack", plural: "icon packs", title: "Icon Pack" },
  { component: "distance_icons", noun: "personality button pack", plural: "personality button packs", title: "Personality Button" },
  { component: "sounds", noun: "sound pack", plural: "sound packs", title: "Sound Pack" },
  { component: "steering_wheels", noun: "steering wheel", plural: "steering wheels", title: "Steering Wheel" },
  { component: "signals", noun: "signal animation", plural: "signal animations", title: "Turn Signal" }
];

let downloading = false;

function partFor(component) {
  return PARTS.find(part => part.component === component);
}

function isDownloading(download) {
  return download.failed + download.succeeded < download.count && !DOWNLOAD_STOPPED.includes(download.progress);
}

async function followDownload(requestTime, label) {
  if (downloading) {
    return;
  }
  downloading = true;
  if (location.pathname === "/themes") {
    P.refresh();
  }
  const handle = P.job({
    label: P.esc(label),
    detail: "Downloading...",
    progress: 0,
    cancel: async () => {
      try {
        await P.request("POST", "/api/themes/download/cancel");
      } catch (error) {
        P.fail(error, "Couldn't cancel the download");
      }
    }
  });
  let download;
  try {
    download = await P.poll(async () => {
      const next = await P.get("/api/themes/download");
      if (next.request_time < requestTime) {
        return undefined;
      }
      const finished = next.failed + next.succeeded;
      if (finished === next.count || DOWNLOAD_STOPPED.includes(next.progress)) {
        return next;
      }
      const percent = /^(\d+)%$/.exec(next.progress);
      const detail = next.count > 1 && percent ? `Downloading ${finished + 1} of ${next.count} (${next.progress})` : next.progress;
      handle.update(P.esc(detail), percent ? (finished + Number(percent[1]) / 100) / next.count : handle.progress);
      return undefined;
    });
  } catch (error) {
    downloading = false;
    handle.fail(error, `${P.esc(label)} stopped`);
    return;
  }
  downloading = false;
  handle.finish();
  if (DOWNLOAD_STOPPED.includes(download.progress)) {
    P.toast(`${P.esc(label)} stopped`, P.esc(download.progress), "alert");
  } else if (!download.failed) {
    P.toast("Downloaded!");
  } else if (download.count === 1) {
    P.toast("Download failed...", "", "alert");
  } else {
    P.toast(`${download.succeeded} of ${download.count} downloaded, ${download.failed} failed`, "", "alert");
  }
  if (location.pathname === "/themes") {
    P.refresh();
  }
}

async function startDownload(component, names, label) {
  let result;
  try {
    result = await P.request("POST", "/api/themes/download", { component, names });
  } catch (error) {
    P.fail(error, `Couldn't start ${P.esc(label.toLowerCase())}`, MESSAGES);
    return;
  }
  followDownload(result.request_time, label);
}

function options(data, part) {
  const info = data.components[part.component];
  const builtins = Object.entries(data.builtins).filter(([key]) => !info.excluded.includes(key)).map(([key, name]) => ({ deletable: false, key, kind: "Built in", name }));
  const installed = info.installed.map(entry => ({ deletable: !entry.user_created, key: entry.key, kind: entry.user_created ? "Made in Theme Maker" : "Downloaded", name: entry.name }));
  const list = [...builtins, ...installed].sort((a, b) => a.name.localeCompare(b.name));
  return [...list.filter(option => option.key === info.selected), ...list.filter(option => option.key !== info.selected)];
}

function optionRow(option, part, data) {
  const inUse = option.key === data.components[part.component].selected;
  const target = `data-component="${part.component}" data-key="${P.esc(option.key)}" data-name="${P.esc(option.name)}"`;
  const acts = [];
  if (inUse) {
    acts.push(`<span class="btn btn-small btn-in-use">${P.ic("check")}In use</span>`);
  } else if (!data.random_themes) {
    acts.push(`<button class="btn btn-quiet btn-small" data-act="themes-use" ${target}>Use</button>`);
  }
  if (option.deletable && !inUse) {
    acts.push(`<button class="icon-btn danger" data-act="themes-delete" ${target} aria-label="Delete ${P.esc(option.name)}" ${downloading ? "disabled" : ""}>${P.ic("trash")}</button>`);
  }
  return `<div class="list-row themes-row ${inUse ? "in-use" : ""}">
    <span class="grow"><b>${P.esc(option.name)}</b><span>${option.kind}</span></span>
    <span class="acts">${acts.join("")}</span>
  </div>`;
}

function downloadRow(name, ready) {
  return `<div class="list-row">
    <label class="grow pick"><input type="checkbox" data-pick value="${P.esc(name)}" ${ready ? "" : "disabled"}><b>${P.esc(name)}</b><span>Not downloaded</span></label>
  </div>`;
}

function partHTML(part, data, shown) {
  const info = data.components[part.component];
  const ready = data.online && !downloading;
  let more = `<p class="muted themes-empty">You have every ${part.noun} there is.</p>`;
  if (info.downloadable.length) {
    more = `${data.online ? "" : `<div class="themes-offline"><div class="notice">${P.ic("info")}<span>The device is offline. Connect it to the internet to download more.</span></div></div>`}
      <div class="themes-list">${info.downloadable.map(name => downloadRow(name, ready)).join("")}</div>
      <div class="panel-foot">
        <button class="btn btn-quiet btn-small" data-act="themes-download-picked" data-component="${part.component}" disabled>${P.ic("download")}<span data-pick-label>Download selected</span></button>
        ${info.downloadable.length > 1 ? `<button class="btn btn-quiet btn-small" data-act="themes-download-all" data-component="${part.component}" ${ready ? "" : "disabled"}>${P.ic("download")}Download all</button>` : ""}
      </div>`;
  }
  return `<div class="two-col" data-part="${part.component}" ${shown ? "" : "hidden"}>
    <section class="panel"><div class="panel-head"><h2>On the device</h2></div>
      <div class="themes-list">${options(data, part).map(option => optionRow(option, part, data)).join("")}</div>
    </section>
    <section class="panel"><div class="panel-head"><h2>Download more</h2>${info.downloadable.length ? `<span class="aside muted">${P.plural(info.downloadable.length, part.noun)}</span>` : ""}</div>
      ${more}
    </section>
  </div>`;
}

function notices(data) {
  const list = [];
  if (!data.custom_themes) {
    list.push("<b>Custom Themes is off,</b> so the car shows stock openpilot. Turn it on in Toggles, under Theme and Appearance, to use what you pick here.");
  }
  if (data.random_themes) {
    list.push("<b>Random Themes is on,</b> so the car picks a theme at the start of every drive from the ones on the device. Turn it off in Toggles to pick your own.");
  }
  if (data.holiday_themes && data.holiday && data.holiday !== "stock") {
    const holiday = data.holiday.replace(/_week$/, "");
    list.push(`<b>The ${P.esc(data.builtins[holiday] || holiday)} theme is on today.</b> It replaces your picks until the holiday ends.`);
  }
  return list.map(text => `<div class="notice">${P.ic("info")}<span>${text}</span></div>`).join("");
}

function showPart(component) {
  document.querySelectorAll("[data-act='themes-show']").forEach(button => button.setAttribute("aria-pressed", String(button.dataset.component === component)));
  document.querySelectorAll("[data-part]").forEach(part => {
    part.hidden = part.dataset.part !== component;
  });
  P.setParam("part", component === PARTS[0].component ? "" : component);
}

P.act("themes-show", button => showPart(button.dataset.component));

P.act("themes-use", async button => {
  const name = P.esc(button.dataset.name);
  try {
    await P.busy(button, P.request("PUT", "/api/themes/selection", { component: button.dataset.component, key: button.dataset.key }));
  } catch (error) {
    P.fail(error, `Couldn't switch to ${name}`, MESSAGES);
    return;
  }
  P.refresh();
  P.toast(`Switched to ${name}`);
});

P.act("themes-delete", async button => {
  const name = P.esc(button.dataset.name);
  const part = partFor(button.dataset.component);
  const ok = await P.dialog({ icon: "trash", tone: "danger", confirmTone: "danger", title: `Delete the “${name}” ${part.noun}?`, body: "<p>You can download it again later.</p>", confirm: "Delete" });
  if (!ok) {
    return;
  }
  try {
    await P.request("DELETE", `/api/themes/${part.component}/${encodeURIComponent(button.dataset.key)}`);
  } catch (error) {
    P.fail(error, `Couldn't delete ${name}`, MESSAGES);
    return;
  }
  P.refresh();
  P.toast(`Deleted ${name}`);
});

function updatePicked(part) {
  const picked = part.querySelectorAll("[data-pick]:checked");
  const button = part.querySelector('[data-act="themes-download-picked"]');
  button.disabled = picked.length === 0;
  button.querySelector("[data-pick-label]").textContent = picked.length > 1 ? `Download ${picked.length} selected` : "Download selected";
}

P.act("themes-download-picked", button => {
  const part = partFor(button.dataset.component);
  const names = [...button.closest("[data-part]").querySelectorAll("[data-pick]:checked")].map(box => box.value);
  startDownload(part.component, names, names.length === 1 ? `Downloading ${names[0]}` : `Downloading ${P.plural(names.length, part.noun)}`);
});

P.act("themes-download-all", async button => {
  const part = partFor(button.dataset.component);
  let names;
  try {
    names = (await P.get("/api/themes")).components[part.component].downloadable;
  } catch (error) {
    P.fail(error, `Couldn't download the ${part.plural}`, MESSAGES);
    return;
  }
  startDownload(part.component, names, `Downloading ${P.plural(names.length, part.noun)}`);
});

P.page("themes", {
  title: "Themes",
  async render(params) {
    const data = await P.get("/api/themes");
    const shown = partFor(params.get("part")) || PARTS[0];
    return {
      body: `
      <div class="themes">
        ${notices(data)}
        <div class="seg" role="group" aria-label="Part of the theme">${PARTS.map(part => `<button type="button" data-act="themes-show" data-component="${part.component}" aria-pressed="${part === shown}">${part.title}</button>`).join("")}</div>
        ${PARTS.map(part => partHTML(part, data, part === shown)).join("")}
      </div>`,
      mount: root => {
        root.querySelectorAll("[data-part]").forEach(part => part.addEventListener("change", () => updatePicked(part)));
        if (isDownloading(data.download)) {
          followDownload(data.download.request_time, "Downloading themes");
        }
      }
    };
  }
});
})();
