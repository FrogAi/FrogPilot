(() => {
"use strict";

const P = window.Pond;

const UPDATER_STATES = { "checking...": "Checking for updates", "downloading...": "Downloading", "finalizing update...": "Finishing" };

let pending = null;

function releaseNotes(html) {
  const doc = new DOMParser().parseFromString(html, "text/html");
  const items = [...doc.querySelectorAll("li")].map(li => {
    const copy = li.cloneNode(true);
    copy.querySelectorAll("ul, ol").forEach(list => list.remove());
    return copy.textContent.trim();
  });
  const notes = items.length ? items : doc.body.textContent.split("\n").map(line => line.replace(/^[*-]\s*/, "").trim()).filter(line => line && !/^(Version |=+$)/.test(line));
  return `<ul class="notes">${notes.map(n => `<li>${P.esc(n)}</li>`).join("")}</ul>`;
}

function etaLabel(seconds) {
  if (seconds < 60) {
    return "Less than a minute left";
  }
  return `About ${Math.round(seconds / 60)}${P.NB}min left`;
}

function downloadProgress(sw) {
  return sw.state !== "idle" && sw.download_progress && "percent" in sw.download_progress ? sw.download_progress : null;
}

function statusHTML(sw) {
  const progress = downloadProgress(sw);
  if (progress) {
    return `<div class="up-row" role="status" data-sw-status><span class="lbl">${progress.percent}%</span><span>${progress.eta === null ? "" : etaLabel(progress.eta)}</span><div class="progress"><i data-w="${progress.percent}"></i></div></div>`;
  }
  let label = UPDATER_STATES[sw.state] || sw.state;
  if (sw.state === "idle") {
    label = pending.label;
  }
  return `<div class="up-row" role="status" data-sw-status><span class="lbl">${P.esc(label)}</span><span class="spinner" aria-hidden="true"></span></div>`;
}

function isFinished(sw) {
  if (sw.state !== "idle") {
    if (pending) {
      pending.seenBusy = true;
    }
    return false;
  }
  return !pending || pending.seenBusy || sw.last_update_time !== pending.before.last_update_time || sw.failed_count !== pending.before.failed_count;
}

async function startUpdater(label, method, path, body) {
  const before = await P.get("/api/software");
  await P.request(method, path, body);
  pending = { before, label, seenBusy: false };
}

async function runUpdater(button, { label, failTitle, method, path, body }) {
  try {
    await P.busy(button, startUpdater(label, method, path, body));
  } catch (error) {
    P.fail(error, failTitle);
    return false;
  }
  if (location.pathname === "/software") {
    P.refresh();
  } else {
    P.go("/software");
  }
  return true;
}

P.act("check-update", button => runUpdater(button, { label: "Checking for updates", failTitle: "Couldn't check for updates", method: "POST", path: "/api/software/check" }));
P.act("download-update", button => runUpdater(button, { label: "Downloading", failTitle: "Couldn't download the update", method: "POST", path: "/api/software/download" }));

P.act("install-update", async button => {
  let sw;
  try {
    sw = await P.get("/api/software");
  } catch (error) {
    P.fail(error, "Couldn't load the update");
    return;
  }
  if (!sw.update_available) {
    P.setSoftware(sw);
    P.refresh();
    return;
  }
  const ok = await P.dialog({
    icon: "software", title: "Install the update?",
    body: `<p>FrogPilot installs <b>${P.esc(sw.new_description)}</b> and the device reboots into it.</p>
      ${P.lossList([], ["Your settings, drives, themes, and keys"])}
      ${sw.new_release_notes ? releaseNotes(sw.new_release_notes) : ""}`,
    confirm: "Install and reboot"
  });
  if (!ok) {
    return;
  }
  const before = (await P.shell()).device;
  try {
    await P.busy(button, P.request("POST", "/api/software/install"));
  } catch (error) {
    P.fail(error, "Couldn't install the update");
    return;
  }
  await P.restart("Installing the update…", "The device is restarting. This page reconnects on its own.");
  const { device } = await P.shell();
  if (device.commit === before.commit && device.branch === before.branch) {
    P.toast("The update didn't install", `The device is still on FrogPilot ${P.esc(device.version)}`, "alert");
    return;
  }
  P.toast(`Updated to FrogPilot ${P.esc(device.version)}`, `${P.esc(device.branch)} · <span class="mono">${P.esc((device.commit || "").slice(0, 7))}</span>`);
});

P.page("software", {
  title: "Software Updates",
  async render() {
    const sw = await P.get("/api/software");
    P.setSoftware(sw);
    const busy = !isFinished(sw);
    if (!busy) {
      pending = null;
    }
    const branches = sw.branches.length ? sw.branches : [sw.branch];
    const heading = sw.update_available ? "An update is ready" : sw.fetch_available ? "An update is available" : "You're up to date";
    return {
      body: `
      <div class="two-col wide-left">
        <section class="panel">
          <div class="panel-head"><h2>${heading}</h2></div>
          <div class="panel-body grid-12">
            ${busy ? statusHTML(sw) : ""}
            ${sw.failed_count > 0 ? `<div class="notice danger">${P.ic("alert")}<span>The last ${P.plural(sw.failed_count, "update check")} failed. Check the device's internet connection, then check again.</span></div>` : ""}
            <dl class="kv"><dt>Installed</dt><dd class="mono">${P.esc(sw.current_description)}</dd>${sw.update_available ? `<dt>New</dt><dd class="mono">${P.esc(sw.new_description)}</dd>` : ""}<dt>Last checked</dt><dd>${sw.last_update_time ? P.whenLabel(sw.last_update_time) : "Never"}</dd></dl>
            ${sw.update_available && sw.new_release_notes ? `<h3 class="field-label">What's new</h3>${releaseNotes(sw.new_release_notes)}` : ""}
          </div>
          <div class="panel-foot"><button class="btn btn-quiet btn-small" data-act="check-update" ${busy ? "disabled" : ""}>Check for Updates</button>${sw.update_available ? `<button class="btn btn-primary btn-small" data-act="install-update">${P.ic("software")}<span>Install</span></button>`
            : sw.fetch_available ? `<button class="btn btn-primary btn-small" data-act="download-update" ${busy ? "disabled" : ""}>${P.ic("download")}<span>Download</span></button>` : ""}</div>
        </section>
        <div class="stack">
          ${sw.automatic_updates_changeable ? `<section class="panel"><div class="panel-head"><h2>Automatic updates</h2></div>
            <div class="panel-body"><label class="switch"><input type="checkbox" data-auto ${sw.automatic_updates ? "checked" : ""}><span class="track"></span><span>Download updates in the background and install them when the car is off</span></label></div></section>` : ""}
          <section class="panel"><div class="panel-head"><h2>Branch</h2></div>
            <div class="panel-body grid-10"><label class="field"><span>Install from</span>${P.dropdown({ value: sw.target_branch || sw.branch, options: branches.map(b => [b, b]), label: "Install from", attrs: "data-branch" })}</label>
            <p class="muted small-print">Testing and development branches get changes first and can misbehave.</p></div></section>
        </div>
      </div>`,
      mount: root => {
        let active = true;
        if (busy) {
          let statusMarkup = statusHTML(sw);
          P.poll(async () => {
            if (!active) {
              return null;
            }
            const next = await P.get("/api/software");
            if (isFinished(next)) {
              return next;
            }
            const status = root.querySelector("[data-sw-status]");
            const progress = downloadProgress(next);
            const bar = status.querySelector(".progress > i");
            if (progress && bar) {
              bar.style.width = `${progress.percent}%`;
              P.showPercent(status.querySelector(".lbl"), `${progress.percent}%`);
              status.querySelector(".lbl + span").textContent = progress.eta === null ? "" : etaLabel(progress.eta);
            } else {
              const markup = statusHTML(next);
              if (markup !== statusMarkup) {
                status.outerHTML = markup;
                statusMarkup = markup;
                P.widths(root);
              }
            }
            return undefined;
          }).then(next => {
            if (!active || !next) {
              return;
            }
            pending = null;
            P.setSoftware(next);
            P.refresh();
          }).catch(error => P.fail(error, "Couldn't follow the updater"));
        }
        root.querySelector("[data-auto]")?.addEventListener("change", async e => {
          const box = e.currentTarget;
          try {
            await P.request("PUT", "/api/software/automatic_updates", { enabled: box.checked });
          } catch (error) {
            box.checked = !box.checked;
            P.fail(error, "Couldn't change automatic updates");
            return;
          }
          P.toast(box.checked ? "Automatic updates on" : "Automatic updates off");
        });
        root.querySelector("[data-branch]").addEventListener("change", async e => {
          const wrap = e.currentTarget;
          const target = wrap.dataset.value;
          const ok = await P.dialog({ icon: "software", title: `Switch to ${P.esc(target)}?`, body: `<p>The device downloads ${P.esc(target)}. When it's ready, install it to reboot into it. Your settings and drives stay.</p>`, confirm: `Switch to ${P.esc(target)}` });
          if (!ok) {
            P.setDropdown(wrap, sw.target_branch || sw.branch);
            return;
          }
          const started = await runUpdater(wrap.querySelector(".dd-btn"), { label: `Switching to ${target}`, failTitle: `Couldn't switch to ${P.esc(target)}`, method: "PUT", path: "/api/software/branch", body: { branch: target, download: true } });
          if (!started) {
            P.setDropdown(wrap, sw.target_branch || sw.branch);
          }
        });
        return () => {
          active = false;
        };
      }
    };
  }
});
})();
