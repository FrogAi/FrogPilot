(() => {
"use strict";

const P = window.Pond;

const DOWNLOAD_CANCELED = "Download cancelled...";
const FINISHED = ["All models downloaded!", "Downloaded!"];
const MESSAGES = {
  invalid_model: "That model isn't on the device",
  invalid_models: "That model can't be changed right now",
  tuning_level: "Choose Standard or a higher tuning level to switch driving models"
};

let following = false;
let randomizerTarget = null;

function percent(progress) {
  const match = /^(\d+)%$/.exec(progress);
  return match ? Number(match[1]) / 100 : null;
}

function canDownload(data) {
  return data.online && !data.download.downloading;
}

async function followDownload(requestTime, what, queue = []) {
  if (following) {
    return;
  }
  following = true;
  const handle = P.job({
    label: `Downloading ${P.esc(what)}`,
    detail: "Starting…",
    cancel: async () => {
      try {
        await P.request("POST", "/api/models/cancel");
      } catch (error) {
        P.fail(error, "Couldn't cancel the download");
      }
    }
  });
  let progress;
  try {
    progress = await P.poll(async () => {
      const { download, models } = await P.get("/api/models");
      if (download.request_time < requestTime) {
        return undefined;
      }
      if (download.downloading) {
        const downloaded = new Set(models.filter(m => m.downloaded).map(m => m.id));
        const left = queue.filter(m => !downloaded.has(m.id));
        if (left.length) {
          handle.label(`Downloading ${P.esc(left[0].name)} (${queue.length - left.length + 1} of ${queue.length})`);
        }
        handle.update(P.esc(download.progress), percent(download.progress));
        return undefined;
      }
      return download.progress;
    });
  } catch (error) {
    following = false;
    handle.fail(error, `Couldn't download ${P.esc(what)}`);
    return;
  }
  following = false;
  handle.finish();
  if (FINISHED.includes(progress)) {
    P.toast(`Downloaded ${P.esc(what)}`);
  } else if (progress === DOWNLOAD_CANCELED) {
    P.toast(`Download of ${P.esc(what)} canceled`);
  } else {
    P.toast(`Couldn't download ${P.esc(what)}`, P.esc(progress), "alert");
  }
  if (location.pathname === "/models") {
    P.refresh();
  }
}

async function startDownload(body, what) {
  let queue = [];
  let result;
  try {
    queue = (await P.get("/api/models")).models.filter(m => !m.downloaded && (!body.ids || body.ids.includes(m.id)));
    result = await P.request("POST", "/api/models/download", body);
  } catch (error) {
    P.fail(error, `Couldn't start downloading ${P.esc(what)}`, MESSAGES);
    return;
  }
  if (location.pathname === "/models") {
    P.refresh();
  }
  followDownload(result.request_time, what, queue);
}

function modelList(data) {
  const others = data.models.filter(m => m.id !== data.default.id).sort((a, b) => b.downloaded - a.downloaded || a.name.localeCompare(b.name));
  const models = [{ ...data.default, available: true, isDefault: true }, ...others.map(m => ({ ...m, available: m.downloaded, isDefault: false }))];
  return [...models.filter(m => m.id === data.in_use), ...models.filter(m => m.id !== data.in_use)];
}

function modelRow(m, data) {
  const blacklisted = data.blacklist.includes(m.id);
  const inUse = m.id === data.in_use;
  const meta = [
    m.id === data.selected && !inUse ? "Selected" : "",
    m.isDefault ? "Default" : "",
    m.available ? "" : "Not downloaded",
    m.available && data.randomizer ? `${P.plural(m.drives, "drive")}${m.score != null ? ` · rated ${Math.round(m.score)}%` : ""}` : "",
    blacklisted && data.randomizer ? "Left out of random picks" : ""
  ].filter(Boolean).join(" · ");
  const acts = [];
  if (inUse) {
    acts.push(`<span class="btn btn-small btn-in-use">${P.ic("check")}In use</span>`);
  } else if (m.available && m.id !== data.selected && !(data.randomizer && data.randomizer_changeable) && data.model_changeable) {
    acts.push(`<button class="btn btn-quiet btn-small" data-act="models-use" data-id="${P.esc(m.id)}" data-name="${P.esc(m.name)}">Use</button>`);
  }
  if (data.randomizer && (m.available || blacklisted)) {
    let icon = "x";
    let label = "Leave out of random picks";
    if (blacklisted) {
      icon = "plus";
      label = "Put back in random picks";
    }

    const target = `data-id="${P.esc(m.id)}" data-name="${P.esc(m.name)}"`;
    const more = `<button class="icon-btn" data-act="menu" aria-expanded="false" aria-label="More for ${P.esc(m.name)}" title="More">${P.ic("dots")}</button>`;
    const item = `<button class="menu-item" data-act="models-blacklist" ${target}>${P.ic(icon)}<span>${label}</span></button>`;
    acts.push(`<span class="menu-wrap">${more}<div class="menu" hidden>${item}</div></span>`);
  }
  if (data.deletable.includes(m.id)) {
    acts.push(`<button class="icon-btn danger" data-act="models-delete" data-id="${P.esc(m.id)}" data-name="${P.esc(m.name)}" aria-label="Delete ${P.esc(m.name)}">${P.ic("trash")}</button>`);
  } else if (m.available) {
    acts.push(`<button class="icon-btn" data-act="models-delete-blocked" data-default="${m.isDefault}" data-name="${P.esc(m.name)}" aria-disabled="true" aria-label="Delete ${P.esc(m.name)}">${P.ic("trash")}</button>`);
  }
  let name = `<span class="grow"><b>${P.esc(m.name)}</b><span>${meta}</span></span>`;
  if (!m.available) {
    let disabled = "";
    if (!canDownload(data)) {
      disabled = "disabled";
    }

    name = `<label class="grow pick"><input type="checkbox" data-pick value="${P.esc(m.id)}" ${disabled}><b>${P.esc(m.name)}</b><span>${meta}</span></label>`;
  }
  return `<div class="list-row model-row ${inUse ? "in-use" : ""}">
    ${name}
    <span class="acts">${acts.join("")}</span>
  </div>`;
}

async function setRandomizer(box, showRows, offerDownload, online) {
  const enabled = box.checked;
  randomizerTarget = enabled;
  box.disabled = true;
  try {
    await P.request("PUT", "/api/models/randomizer", { enabled });
  } catch (error) {
    box.checked = !enabled;
    P.fail(error, "Couldn't change random picks");
    return;
  } finally {
    randomizerTarget = null;
    box.disabled = false;
    if (!box.isConnected && location.pathname === "/models") {
      P.refresh();
    }
  }
  if (box.isConnected) {
    showRows(enabled);
  }
  P.toast(enabled ? "Random picks on" : "Random picks off");
  if (!enabled || !offerDownload) {
    return;
  }
  if (!online) {
    P.dialog({ icon: "download", title: "Download more models to pick from", body: `<p>Random picks only come from models on the device. Connect the device to the internet to download the rest.</p>`, confirm: "Got it", cancel: "" });
  } else if (await P.dialog({ icon: "download", title: "Download every model?", body: `<p>Random picks only come from models on the device. Download the rest now so any of them can be picked.</p>`, confirm: "Download all", cancel: "Not now" })) {
    startDownload({}, "every model");
  }
}

P.act("models-use", async button => {
  let result;
  try {
    result = await P.busy(button, P.request("PUT", "/api/models/selected", { id: button.dataset.id }));
  } catch (error) {
    P.fail(error, `Couldn't switch to ${P.esc(button.dataset.name)}`, MESSAGES);
    return;
  }
  P.refresh();
  P.toast(result.reboot_required ? `${P.esc(button.dataset.name)} drives after the device reboots` : `${P.esc(button.dataset.name)} drives from the next start`);
});

function updatePicked(root) {
  const picked = root.querySelectorAll("[data-pick]:checked");
  const button = root.querySelector('[data-act="models-download-picked"]');
  if (!button) {
    return;
  }
  button.disabled = picked.length === 0;
  button.querySelector("[data-pick-label]").textContent = picked.length > 1 ? `Download ${picked.length} selected` : "Download selected";
}

P.act("models-download-picked", button => {
  const picked = [...button.closest(".panel").querySelectorAll("[data-pick]:checked")];

  let what = `${picked.length} models`;
  if (picked.length === 1) {
    what = picked[0].nextElementSibling.textContent;
  }

  startDownload({ ids: picked.map(box => box.value) }, what);
});

P.act("models-download-all", () => startDownload({}, "every model"));

P.act("models-delete", async button => {
  const name = P.esc(button.dataset.name);
  const ok = await P.dialog({ icon: "trash", tone: "danger", confirmTone: "danger", title: `Delete ${name}?`, body: `<p>The model file is removed. You can download it again later. Your ratings for it stay.</p>`, confirm: "Delete model" });
  if (!ok) {
    return;
  }
  try {
    await P.request("DELETE", "/api/models", { ids: [button.dataset.id] });
  } catch (error) {
    P.fail(error, `Couldn't delete ${name}`, MESSAGES);
    return;
  }
  P.refresh();
  P.toast(`${name} deleted`);
});

P.act("models-delete-blocked", button => {
  const name = P.esc(button.dataset.name);
  if (button.dataset.default === "true") {
    P.toast(`${name} can't be deleted`, "It's the default model, so it always stays on the device", "info");
  } else {
    P.toast(`${name} can't be deleted while it's in use`, "Switch to another model first", "info");
  }
});

P.act("models-blacklist", async button => {
  const id = button.dataset.id;
  const name = P.esc(button.dataset.name);
  let blacklist;
  try {
    blacklist = (await P.get("/api/models")).blacklist;
    const leaving = !blacklist.includes(id);
    await P.request("PUT", "/api/models/blacklist", { ids: leaving ? [...blacklist, id] : blacklist.filter(other => other !== id) });
    P.refresh();
    P.toast(leaving ? `${name} is left out of random picks` : `${name} is back in random picks`);
  } catch (error) {
    P.fail(error, "Couldn't change the random picks", MESSAGES);
  }
});

P.act("models-ratings", async () => {
  const ok = await P.dialog({ icon: "alert", tone: "danger", confirmTone: "danger", title: "Reset model ratings?", body: `<p>How many drives and what rating each driving model has go back to none. Your drives themselves are not touched.</p>`, confirm: "Reset ratings" });
  if (!ok) {
    return;
  }
  try {
    await P.request("DELETE", "/api/models/ratings");
  } catch (error) {
    P.fail(error, "Couldn't reset the ratings");
    return;
  }
  P.refresh();
  P.toast("Model ratings reset");
});

P.page("models", {
  title: "Driving Models",
  async render() {
    const data = await P.get("/api/models");
    data.randomizer = randomizerTarget ?? data.randomizer;
    const models = modelList(data);
    const missing = models.some(m => !m.available);
    const rated = models.some(m => m.drives > 0);

    let offline = "";
    if (missing && !data.online) {
      const notice = `<div class="notice">${P.ic("info")}<span>The device is offline. Connect it to the internet to download more.</span></div>`;
      offline = `<div class="models-offline">${notice}</div>`;
    }

    let downloadAll = "";
    if (!canDownload(data)) {
      downloadAll = "disabled";
    }

    let modelNotice = "";
    if (!data.model_changeable) {
      modelNotice = `<div class="notice">${P.ic("info")}<span>Choose Standard or a higher tuning level in Toggles to switch driving models.</span></div>`;
    }

    return {
      body: `
      <section class="panel">
        <div class="panel-head"><h2>Models</h2>${data.randomizer_changeable ? `<label class="switch aside"><input type="checkbox" data-random ${data.randomizer ? "checked" : ""} ${randomizerTarget === null ? "" : "disabled"}><span class="track"></span><span class="models-random">Pick a random model each drive</span></label>` : ""}</div>
        ${offline}
        ${modelNotice}
        <div class="models-list">${models.map(m => modelRow(m, data)).join("")}</div>
        ${missing || rated ? `<div class="panel-foot" ${missing || data.randomizer ? "" : "hidden"}>
          ${missing ? `<button class="btn btn-quiet btn-small" data-act="models-download-picked" disabled>${P.ic("download")}<span data-pick-label>Download selected</span></button>
          <button class="btn btn-quiet btn-small" data-act="models-download-all" ${downloadAll}>${P.ic("download")}Download all</button>` : ""}
          ${rated ? `<button class="btn btn-danger-quiet btn-small models-ratings" data-act="models-ratings" ${data.randomizer ? "" : "hidden"}>Reset ratings…</button>` : ""}
        </div>` : ""}
      </section>`,
      mount: root => {
        if (data.download.downloading) {
          followDownload(data.download.request_time, "models");
        }
        const box = root.querySelector("[data-random]");
        const showRows = enabled => {
          data.randomizer = enabled;
          root.querySelector(".models-list").innerHTML = models.map(m => modelRow(m, data)).join("");
          updatePicked(root);
          if (rated) {
            root.querySelector(".models-ratings").hidden = !enabled;
            root.querySelector(".panel-foot").hidden = !missing && !enabled;
          }
        };
        box?.addEventListener("change", () => setRandomizer(box, showRows, missing && !data.download.downloading, data.online));
        root.querySelector(".models-list").addEventListener("change", () => updatePicked(root));
      }
    };
  }
});
})();
