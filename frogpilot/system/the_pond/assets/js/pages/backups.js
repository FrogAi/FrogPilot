(() => {
"use strict";

const P = window.Pond;

const BACKUP_NAME_ERRORS = {
  invalid_name: "Use letters, numbers, dots, dashes, and underscores, and leave out “_auto” and “_in_progress”",
  no_name: "Give the backup a name"
};
const FROGPILOT_IMPORT_ERRORS = { ...BACKUP_NAME_ERRORS, invalid_file: "That file isn't a FrogPilot backup" };
const SETTINGS_ERRORS = {
  invalid_file: "That file isn't a settings backup",
  invalid_name: "That name can only use letters, numbers, dots, dashes, and underscores"
};

let tracking = false;

function frogpilotBackupPath(name, rest = "") {
  return `/api/frogpilot_backups/${encodeURIComponent(name)}${rest}`;
}

function settingsBackupPath(name, rest = "") {
  return `/api/settings_backups/${encodeURIComponent(name)}${rest}`;
}

function newBackupName(backups) {
  const date = new Date();
  const base = `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, "0")}-${String(date.getDate()).padStart(2, "0")}_manual`;
  const names = new Set(backups.map(b => b.name));
  let name = base;
  for (let count = 1; names.has(name); count++) {
    name = `${base}${count}`;
  }
  return name;
}

function askBackupName(icon, save) {
  const input = `<input class="input mono" autocomplete="off" spellcheck="false" placeholder="before-0.10.4">`;
  const hint = `<p class="muted small-print">Letters, numbers, dots, dashes, and underscores. Spaces become underscores.</p>`;

  return P.dialog({
    icon, title: "Name your backup", body: `<label class="field"><span>Backup name</span>${input}</label>${hint}`,
    confirm: "Back up", errors: BACKUP_NAME_ERRORS,
    onConfirm: async dialog => {
      const name = dialog.querySelector("input").value.trim().replaceAll(" ", "_");

      if (!name) {
        throw new P.PondError(400, "no_name");
      }

      await save(name);
    }
  });
}

P.act("backups-new", async () => {
  const dialog = await askBackupName("backups", name => {
    return P.request("POST", "/api/settings_backups", { name });
  });

  if (!dialog) {
    return;
  }

  P.toast("Backup saved on the device");

  if (location.pathname === "/backups") {
    P.refresh();
  }
});

P.act("backups-now", async button => {
  const body = button.closest(".dlg-body");
  const done = body.querySelector("[data-backup-done]");
  const failure = body.querySelector("[data-backup-failure]");
  done.hidden = true;
  failure.hidden = true;

  let name = "";
  try {
    await P.busy(button, P.get("/api/settings_backups").then(backups => {
      name = newBackupName(backups);
      return P.request("POST", "/api/settings_backups", { name });
    }));
  } catch (error) {
    if (!(error instanceof P.PondError)) {
      throw error;
    }
    failure.querySelector("span").textContent = `Couldn't back up your settings. ${error.message}.`;
    failure.hidden = error.handled;
    return;
  }

  done.hidden = false;
  done.querySelector("span").textContent = `Your settings are backed up as “${name}”.`;

  if (location.pathname === "/backups") {
    P.refresh();
  }
});

P.act("backups-download", button => P.download(settingsBackupPath(button.dataset.name, "/download"), `${button.dataset.name}.zip`));

P.act("backups-delete", async button => {
  const name = button.dataset.name;
  const ok = await P.dialog({
    icon: "trash", tone: "danger", confirmTone: "danger", title: `Delete “${P.esc(name)}”?`,
    body: `<p>This backup is removed from the device. Your current settings don't change.</p>`,
    confirm: "Delete backup"
  });
  if (!ok) {
    return;
  }
  try {
    await P.request("DELETE", settingsBackupPath(name));
  } catch (error) {
    P.fail(error, "Couldn't delete the backup");
    return;
  }
  P.refresh();
  P.toast("Backup deleted");
});

P.act("backups-delete-all", async () => {
  const ok = await P.dialog({
    icon: "trash", tone: "danger", confirmTone: "danger", title: "Delete all settings backups?",
    body: `<p>This includes the copies FrogPilot saves automatically. Your current settings don't change.</p>`,
    confirm: "Delete all"
  });
  if (!ok) {
    return;
  }
  try {
    await P.request("DELETE", "/api/settings_backups");
  } catch (error) {
    P.fail(error, "Couldn't delete the backups");
    return;
  }
  P.refresh();
  P.toast("Settings backups deleted");
});

P.act("backups-restore", async button => {
  const name = button.dataset.name;
  let changes;
  try {
    changes = await P.get(settingsBackupPath(name, "/changes"));
  } catch (error) {
    P.fail(error, "Couldn't read the backup");
    return;
  }
  if (!changes.length) {
    P.toast("Nothing to restore", "Every setting in this backup matches how it's set now");
    return;
  }
  const rows = changes.map(c => `<tr><td>${P.esc(c.name)}${c.secret ? ` <span class="badge">${P.ic("lock")}secret</span>` : ""}</td><td class="from">${c.secret ? "Current value" : P.esc(c.current)}</td><td class="to">${c.secret ? "Value from the backup" : P.esc(c.value)}</td></tr>`).join("");
  const ok = await P.dialog({
    icon: "backups", wide: true, title: `Restore “${P.esc(name)}”?`,
    body: `<p>Here's what changes. Everything not listed stays exactly as it is now.</p>
      <table class="change-table"><thead><tr><th>Setting</th><th>Now</th><th>After restoring</th></tr></thead><tbody>${rows}</tbody></table>`,
    confirm: `Restore ${P.plural(changes.length, "setting")}`
  });
  if (!ok) {
    return;
  }
  const handle = P.job({ label: "Restoring settings" });
  try {
    await P.request("POST", settingsBackupPath(name, "/restore"));
  } catch (error) {
    handle.fail(error, "Couldn't restore the backup");
    return;
  }
  handle.done(`${P.plural(changes.length, "setting")} restored`);
});

P.act("backups-reset", async button => {
  const stock = button.dataset.kind === "stock";

  let summary;
  if (stock) {
    summary = "FrogPilot's features are turned off and openpilot's own settings are used.";
  } else {
    summary = "Every FrogPilot setting and some of openpilot's own settings go back to how they shipped.";
  }

  let consequences;
  try {
    consequences = await P.get("/api/settings/reset");
  } catch (error) {
    P.fail(error, "Couldn't reset your settings");
    return;
  }

  const ok = await P.dialog({
    icon: "alert", tone: "danger", confirmTone: "danger",
    title: stock ? "Reset to stock openpilot?" : "Reset to FrogPilot defaults?",
    body: `<p>${summary}</p>
      ${P.lossList(consequences.lost, consequences.kept)}
      <p>${P.esc(consequences.note)}</p>
      <p><button type="button" class="btn btn-text btn-small backups-first" data-act="backups-now">${P.ic("plus")}Back up my settings first</button></p>
      <div class="notice" data-backup-done role="status" hidden>${P.ic("check")}<span></span></div>
      <div class="notice danger" data-backup-failure role="alert" hidden>${P.ic("alert")}<span></span></div>`,
    confirm: stock ? "Erase and reset to stock" : "Erase and reset to defaults",
    requireCheck: "I understand everything under “What goes” is removed"
  });
  if (!ok) {
    return;
  }
  const handle = P.job({ label: stock ? "Resetting to stock openpilot" : "Resetting to FrogPilot defaults" });
  try {
    await P.request("POST", "/api/settings/reset", { stock });
  } catch (error) {
    handle.fail(error, "Couldn't reset your settings");
    return;
  }
  handle.done("Settings reset");
  P.restart("Rebooting");
});

async function importBackup(input) {
  const file = input.files[0];
  input.value = "";
  if (!file) {
    return;
  }
  const name = file.name.replace(/\.(json|zip)$/i, "").replace(/_auto$/, "").replace(/[^A-Za-z0-9._-]+/g, "_");
  const form = new FormData();
  form.append("name", name);
  form.append("file", file);
  const handle = P.job({ label: `Importing ${P.fileLabel(file.name)}` });
  try {
    await P.request("POST", "/api/settings_backups/import", form);
  } catch (error) {
    handle.fail(error, `Couldn't import ${P.fileLabel(file.name)}`, SETTINGS_ERRORS);
    return;
  }
  handle.done(`“${P.fileLabel(name)}” added. Restore it to see what changes.`);
  if (location.pathname === "/backups") {
    P.refresh();
  }
}

async function importFrogpilotBackup(input) {
  const file = input.files[0];
  input.value = "";
  if (!file) {
    return;
  }
  const name = file.name.replace(/\.tar\.zst$/i, "").replace(/_auto$/, "").replace(/[^A-Za-z0-9._-]+/g, "_");
  const form = new FormData();
  form.append("name", name);
  form.append("file", file);
  const handle = P.job({ label: `Importing ${P.fileLabel(file.name)}` });
  try {
    await P.request("POST", "/api/frogpilot_backups/import", form);
  } catch (error) {
    handle.fail(error, `Couldn't import ${P.fileLabel(file.name)}`, FROGPILOT_IMPORT_ERRORS);
    return;
  }
  handle.done(`“${P.fileLabel(name)}” added`);
  if (location.pathname === "/backups") {
    P.refresh();
  }
}

async function track(kind, name) {
  tracking = true;
  const restore = kind === "restore";
  const handle = P.job({ label: restore ? "Restoring FrogPilot" : "Backing up FrogPilot", detail: restore ? "The device reboots when it's done" : `<span class="mono">${P.fileLabel(name)}</span>` });
  let job;
  try {
    job = await P.poll(async () => {
      let data;
      try {
        data = await P.get("/api/frogpilot_backups");
      } catch (error) {
        if (restore && error instanceof P.PondError && error.code === "connection_lost") {
          return { state: "done" };
        }
        throw error;
      }
      if (data.job?.state !== "running") {
        return data.job;
      }
    });
  } catch (error) {
    tracking = false;
    handle.fail(error, restore ? "Couldn't restore FrogPilot" : "Couldn't back up FrogPilot");
    return;
  }
  tracking = false;
  if (job?.state === "done" && restore) {
    handle.finish();
    P.restart("Rebooting");
    return;
  }
  if (job?.state === "done") {
    handle.done("FrogPilot backed up", `<span class="mono">${P.fileLabel(name)}</span>`);
  } else {
    handle.finish();
    P.toast(restore ? "Couldn't restore FrogPilot" : "Couldn't back up FrogPilot", "The job failed on the device", "alert");
  }
  if (location.pathname === "/backups") {
    P.refresh();
  }
}

P.act("frogbackups-new", async () => {
  let name = "";
  await askBackupName("frogbackups", async typed => {
    await P.request("POST", "/api/frogpilot_backups", { name: typed });
    name = typed;
  });

  if (!name) {
    return;
  }

  track("create", name);
});

P.act("frogbackups-restore", async b => {
  if (!await P.pinWarning("frogbackups-restore", "Restoring a FrogPilot backup from The Pond replaces the software the device runs.")) {
    return;
  }

  const { device } = await P.shell();
  const ok = await P.dialog({ icon: "frogbackups", tone: "danger", confirmTone: "danger", title: `Restore FrogPilot from “${P.esc(b.dataset.name)}”?`, body: `<p>The installed FrogPilot is replaced with the version in this backup, then the device reboots. Wait for The Pond to come back before you start the car.</p>${P.lossList([`The FrogPilot version installed now (${P.esc(device.version)}, ${P.esc(device.branch)})`], ["Your drives, recordings, and settings", "Other backups"])}`, confirm: "Restore and reboot" });
  if (!ok) {
    return;
  }
  try {
    await P.request("POST", frogpilotBackupPath(b.dataset.name, "/restore"));
  } catch (error) {
    P.fail(error, "Couldn't restore FrogPilot");
    return;
  }
  track("restore", b.dataset.name);
});

P.act("frogbackups-download", b => P.saveLink(frogpilotBackupPath(b.dataset.name, "/download")));

P.act("frogbackups-delete", async b => {
  const ok = await P.dialog({ icon: "trash", tone: "danger", confirmTone: "danger", title: `Delete “${P.esc(b.dataset.name)}”?`, body: `<p>Frees ${P.size(Number(b.dataset.size))}. You won't be able to roll back to this copy.</p>`, confirm: "Delete backup" });
  if (!ok) {
    return;
  }
  try {
    await P.request("DELETE", frogpilotBackupPath(b.dataset.name));
  } catch (error) {
    P.fail(error, "Couldn't delete the backup");
    return;
  }
  P.toast("Backup deleted");
  P.refresh();
});

P.act("frogbackups-delete-all", async b => {
  const ok = await P.dialog({ icon: "trash", tone: "danger", confirmTone: "danger", title: "Delete all FrogPilot backups?", body: `<p>Frees ${P.size(Number(b.dataset.size))}. This includes the backups FrogPilot makes automatically, so you won't be able to roll back to any of them.</p>`, confirm: "Delete all" });
  if (!ok) {
    return;
  }
  try {
    await P.request("DELETE", "/api/frogpilot_backups");
  } catch (error) {
    P.fail(error, "Couldn't delete the backups");
    return;
  }
  P.toast("FrogPilot backups deleted");
  P.refresh();
});

P.page("backups", {
  title: "Backups",
  async render() {
    const [backups, frogpilot] = await Promise.all([P.get("/api/settings_backups"), P.get("/api/frogpilot_backups")]);
    const frogpilotSize = frogpilot.backups.reduce((total, b) => total + b.size, 0);
    return {
      body: `
      <div class="stack">
        <section class="panel backups-panel" aria-labelledby="bk-h">
          <div class="panel-head"><h2 id="bk-h">Settings backups</h2><span class="aside backups-acts">
            ${backups.length ? `<button class="btn btn-danger-quiet btn-small" data-act="backups-delete-all">Delete all…</button>` : ""}
            <label class="btn btn-quiet btn-small file-btn">${P.ic("upload")}<span>Import</span>
              <input type="file" data-import accept=".json,.zip,application/json,application/zip">
            </label>
            <button class="btn btn-primary btn-small" data-act="backups-new">${P.ic("plus")}<span>Back up<span class="lbl-long"> now</span></span></button>
          </span></div>
          <p class="hint backups-lede">Just your settings, so you can undo changes or move them to another device.</p>
          <div class="panel-body two-col wide-left">
            ${backups.length ? `<ul class="rows rule-list">${backups.map(b => `<li class="list-row">
              <span class="grow"><b class="mono">${P.esc(b.name)}</b><span>${b.automatic ? "Automatic" : "Made by you"} · ${P.whenLabel(b.modified)}</span></span>
              <span class="acts">
                <button class="btn btn-quiet btn-small" data-act="backups-restore" data-name="${P.esc(b.name)}"
                  aria-label="Restore ${P.esc(b.name)}">Restore…</button>
                <button class="icon-btn" data-act="backups-download" data-name="${P.esc(b.name)}" aria-label="Download ${P.esc(b.name)}" title="Download">${P.ic("download")}</button>
                <button class="icon-btn danger" data-act="backups-delete" data-name="${P.esc(b.name)}" aria-label="Delete ${P.esc(b.name)}" title="Delete">${P.ic("trash")}</button>
              </span>
            </li>`).join("")}</ul>` : `<div class="empty"><h3>No backups yet</h3><p>Back up now to keep a copy of how everything is set today.</p></div>`}
            <section class="backups-reset" aria-labelledby="reset-h">
              <h3 id="reset-h">Start over</h3>
              <div class="danger-zone"><span><b>FrogPilot defaults</b><span class="hint">Every setting as FrogPilot ships it</span></span><button class="btn btn-danger-quiet btn-small" data-act="backups-reset" data-kind="default">Reset…</button></div>
              <div class="danger-zone"><span><b>Stock openpilot</b><span class="hint">FrogPilot's features turned off</span></span><button class="btn btn-danger-quiet btn-small" data-act="backups-reset" data-kind="stock">Reset…</button></div>
            </section>
          </div>
        </section>
        <section class="panel backups-panel" aria-labelledby="fb-h">
          <div class="panel-head"><h2 id="fb-h">FrogPilot backups</h2><span class="aside backups-acts">
            ${frogpilot.backups.length ? `<button class="btn btn-danger-quiet btn-small" data-act="frogbackups-delete-all" data-size="${frogpilotSize}">Delete all…</button>` : ""}
            <label class="btn btn-quiet btn-small file-btn">${P.ic("upload")}<span>Import</span><input type="file" data-frogpilot-import accept=".zst,application/zstd"></label>
            <button class="btn btn-primary btn-small" data-act="frogbackups-new">${P.ic("plus")}<span>Back up<span class="lbl-long"> FrogPilot</span></span></button>
          </span></div>
          <p class="hint backups-lede">The whole FrogPilot install, so you can go back to an earlier version. Your settings and drives stay as they are.</p>
          <div class="panel-body">
            ${frogpilot.backups.map(b => `<div class="list-row"><span class="grow"><b class="mono">${P.esc(b.name)}</b><span>${b.automatic ? "Automatic" : "Made by you"} · ${P.whenLabel(b.modified)} · ${P.size(b.size)}</span></span><span class="acts">
              <button class="btn btn-quiet btn-small" data-act="frogbackups-restore" data-name="${P.esc(b.name)}"
                aria-label="Restore ${P.esc(b.name)}">Restore…</button>
              <button class="icon-btn" data-act="frogbackups-download" data-name="${P.esc(b.name)}" aria-label="Download ${P.esc(b.name)}" title="Download">${P.ic("download")}</button>
              <button class="icon-btn danger" data-act="frogbackups-delete" data-name="${P.esc(b.name)}" data-size="${b.size}" aria-label="Delete ${P.esc(b.name)}">${P.ic("trash")}</button>
            </span></div>`).join("") || `<div class="empty"><h3>No FrogPilot backups</h3>
              <p>Back up FrogPilot to keep a copy of the version installed today.</p></div>`}
          </div>
        </section>
      </div>`,
      mount: root => {
        root.querySelector("[data-frogpilot-import]").addEventListener("change", e => importFrogpilotBackup(e.currentTarget));
        root.querySelector("[data-import]").addEventListener("change", e => importBackup(e.currentTarget));
        if (frogpilot.job?.state === "running" && !tracking) {
          track(frogpilot.job.kind, frogpilot.job.name);
        }
      }
    };
  }
});
})();
