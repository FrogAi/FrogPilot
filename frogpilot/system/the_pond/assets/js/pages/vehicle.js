(() => {
"use strict";

const P = window.Pond;

const DOORS_REFRESH_TIME = 2000;

const LOCK_LABELS = { locked: "Locked", unlocked: "Unlocked" };

const SECOC_ERRORS = {
  invalid_key: "The key must be 32 characters, using only 0 to 9 and a to f",
  invalid_name: "Give the key a name of up to 64 characters",
  name_taken: "A key with that name is already saved"
};

let doorResult = null;

function doorButtons(doors) {
  if (doors.lock_status === "locked") {
    return `<button class="btn btn-quiet btn-small" data-act="doors-unlock">Unlock doors</button>`;
  }
  if (doors.lock_status === "unlocked") {
    return `<button class="btn btn-quiet btn-small" data-act="doors-lock">Lock doors</button>`;
  }
  return `<button class="btn btn-quiet btn-small" data-act="doors-lock">Lock doors</button> <button class="btn btn-quiet btn-small" data-act="doors-unlock">Unlock doors</button>`;
}

function doorState(doors) {
  if (doors.lock_status !== "unavailable") {
    return `<span class="state-line">${LOCK_LABELS[doors.lock_status]}</span>`;
  }
  if (doorResult) {
    return `<span class="state-line">${LOCK_LABELS[doorResult.status]} at ${P.timeLabel(doorResult.time)}</span>`;
  }
  return `<span class="muted">Car is asleep, status unavailable</span>`;
}

function doorsPanel(doors) {
  return `<div class="panel-head"><h2>Doors</h2>${doorState(doors)}<span class="acts">${doorButtons(doors)}</span></div>`;
}

function degrees(value) {
  return value === null ? "Not calibrated" : `${value.toFixed(1)}°`;
}

async function sendDoors(lock) {
  if (!lock) {
    const shell = await P.shell();
    const ok = await P.dialog({
      icon: "unlock", title: "Unlock the doors?",
      body: `<p>Every door of the ${P.esc(shell.vehicle.model)} unlocks now.</p>`,
      confirm: "Unlock doors"
    });
    if (!ok) {
      return;
    }
  }
  const failTitle = lock ? "Couldn't lock the doors" : "Couldn't unlock the doors";
  let sent;
  try {
    sent = await P.request("POST", "/api/doors", { lock });
  } catch (error) {
    P.fail(error, failTitle);
    return;
  }
  const handle = P.job({ label: lock ? "Locking the doors" : "Unlocking the doors" });
  let doors;
  try {
    doors = await P.poll(async () => {
      const next = await P.get("/api/doors");
      return next.request_time >= sent.request_time && !["", "pending"].includes(next.status) ? next : undefined;
    }, 500);
  } catch (error) {
    handle.fail(error, failTitle);
    return;
  }
  if (doors.status === "failed") {
    handle.finish();
    P.toast(failTitle, "The car didn't respond. Make sure it's off, then try again.", "alert");
  } else if (doors.status === "busy") {
    handle.finish();
    P.toast(failTitle, "The car is on or still handling the last door command. Try again in a moment.", "alert");
  } else {
    doorResult = { status: doors.status, time: P.now() };
    handle.done(doors.status === "locked" ? "Doors locked" : "Doors unlocked");
  }
  P.refresh();
}

P.doorButtons = doorButtons;
P.act("doors-lock", () => sendDoors(true));
P.act("doors-unlock", () => sendDoors(false));

P.act("secoc-apply", async b => {
  let result;
  try {
    result = await P.busy(b, P.request("PUT", "/api/secoc_key", { name: b.dataset.name }));
  } catch (error) {
    P.fail(error, `Couldn't apply “${P.esc(b.dataset.name)}”`);
    return;
  }
  P.toast(`${P.esc(b.dataset.name)} applied`, result.restart_required ? "Turn the car off and on to use it" : "");
  P.refresh();
});

P.act("secoc-del", async b => {
  const ok = await P.dialog({ icon: "trash", tone: "danger", confirmTone: "danger", title: `Remove “${P.esc(b.dataset.name)}”?`, body: `<p>This saved key is deleted from the device. The key in use isn't affected.</p>`, confirm: "Remove key" });
  if (!ok) {
    return;
  }
  try {
    await P.request("DELETE", `/api/secoc_keys/${encodeURIComponent(b.dataset.name)}`);
  } catch (error) {
    P.fail(error, `Couldn't remove “${P.esc(b.dataset.name)}”`);
    return;
  }
  P.refresh();
});

P.act("secoc-add", async () => {
  const d = await P.dialog({ icon: "keys", title: "Add a security key", body: `<label class="field"><span>Name</span><input class="input" data-key-name placeholder="Spare" autocomplete="off"></label><label class="field"><span>Key</span><input class="input mono" data-key-value placeholder="32 hex characters" autocomplete="off" spellcheck="false"></label>`, confirm: "Save key" });
  if (!d) {
    return;
  }
  try {
    await P.request("POST", "/api/secoc_keys", { name: d.querySelector("[data-key-name]").value, value: d.querySelector("[data-key-value]").value.trim() });
  } catch (error) {
    P.fail(error, "Couldn't save the key", SECOC_ERRORS);
    return;
  }
  P.toast("Key saved");
  P.refresh();
});

P.act("vehicle-reset-calibration", async () => {
  const ok = await P.dialog({ icon: "refresh", tone: "danger", confirmTone: "danger", title: "Are you sure you want to reset calibration?", body: "<p>openpilot is continuously calibrating, resetting is rarely required. Resetting calibration will restart openpilot if the car is powered on.</p>", confirm: "Reset" });
  if (!ok) {
    return;
  }
  try {
    await P.request("DELETE", "/api/vehicle/calibration");
  } catch (error) {
    P.fail(error, "Couldn't reset calibration", { openpilot_steering: "Disengage to reset calibration" });
    return;
  }
  P.toast("Calibration reset");
  P.refresh();
});

P.act("vehicle-reset-curves", async () => {
  const ok = await P.dialog({ icon: "refresh", tone: "danger", confirmTone: "danger", title: "Are you sure you want to completely reset your curvature data?", body: "<p>Everything openpilot has learned about how you take curves is thrown away. The “Adaptive” curve speed profile goes back to its starting value and relearns as you drive.</p>", confirm: "Reset" });
  if (!ok) {
    return;
  }
  try {
    await P.request("DELETE", "/api/vehicle/curve_data");
  } catch (error) {
    P.fail(error, "Couldn't reset the curve data", { car_on: "Curve data can't be reset while the car is on. Turn the car off and try again." });
    return;
  }
  P.toast("Curve data reset");
  P.refresh();
});

P.page("vehicle", {
  title: "Vehicle and Doors",
  async render() {
    const [v, doors, secoc] = await Promise.all([P.get("/api/vehicle"), P.get("/api/doors"), P.get("/api/secoc_keys")]);
    const c = v.capabilities;
    const caps = c ? [["Blind spot monitoring", c.blind_spot_support], ["openpilot speed control", c.openpilot_longitudinal], ["Radar", c.radar_support], ["Stop and go", c.sng_support]] : [];
    if (c?.pedal_support) {
      caps.push(["Comma pedal", true]);
    }
    if (c?.sdsu_support) {
      caps.push(["SDSU", true]);
    }
    const cs = v.curve_speed;
    return {
      body: `
      <div class="two-col">
        ${doors.available ? `<section class="panel vehicle-doors" data-doors>${doorsPanel(doors)}</section>` : ""}
        <div class="stack">
          <section class="panel" id="vehicle-car"><div class="panel-head"><h2>This car</h2></div>
            <div class="panel-body"><dl class="kv"><dt>Car</dt><dd>${P.esc(v.model)}</dd><dt>Fingerprint</dt><dd class="mono">${P.esc(v.fingerprint)}</dd></dl>
            ${caps.length ? `<ul class="rows caps">${caps.map(([l, on]) => `<li>${P.ic(on ? "check" : "x", "ic-s")}<span ${on ? "" : 'class="muted"'}>${l}${on ? "" : ", not available"}</span></li>`).join("")}</ul>` : ""}</div></section>
          ${secoc.required || secoc.keys.length ? `<section class="panel"><div class="panel-head"><h2>Toyota security keys</h2><span class="aside muted">SecOC</span></div>
            <div>${secoc.keys.map(k => `<div class="list-row tight"><span class="grow"><b>${P.esc(k.name)}</b><span>${k.applied ? "In use" : "Saved"}</span></span><span class="acts">${k.applied ? "" : `<button class="btn btn-quiet btn-small" data-act="secoc-apply" data-name="${P.esc(k.name)}">Apply</button><button class="icon-btn danger" data-act="secoc-del" data-name="${P.esc(k.name)}" aria-label="Remove ${P.esc(k.name)}">${P.ic("trash")}</button>`}</span></div>`).join("")}</div>
            <div class="panel-foot"><button class="btn btn-quiet btn-small" data-act="secoc-add">Add a key</button></div></section>` : ""}
        </div>
        <div class="stack">
          <section class="panel"><div class="panel-head"><h2>Calibration</h2></div>
            <div class="panel-body"><dl class="kv"><dt>Pitch</dt><dd>${degrees(v.calibration.pitch)}</dd><dt>Yaw</dt><dd>${degrees(v.calibration.yaw)}</dd><dt>Curve speed learning</dt><dd>${Math.round(cs.calibration_progress)}%</dd><dt>Learned lateral grip</dt><dd>${cs.calibrated_lateral_acceleration.toFixed(2)}${P.NB}m/s²</dd><dt>Max lateral grip</dt><dd>${cs.max_lateral_acceleration.toFixed(2)}${P.NB}m/s²</dd></dl>
            <div class="vehicle-resets">
              <div class="danger-zone"><span><b>Reset calibration</b><span class="hint">openpilot is continuously calibrating, resetting is rarely required</span></span><button class="btn btn-danger-quiet btn-small" data-act="vehicle-reset-calibration">Reset</button></div>
              <div class="danger-zone"><span><b>Reset curve data</b><span class="hint">Start over on how you take curves</span></span><button class="btn btn-danger-quiet btn-small" data-act="vehicle-reset-curves">Reset</button></div>
            </div></div></section>
        </div>
      </div>`,
      mount: root => {
        const panel = root.querySelector("[data-doors]");
        if (!panel) {
          return;
        }
        let shown = doorsPanel(doors);
        const timer = setInterval(async () => {
          if (document.hidden) {
            return;
          }
          let next;
          try {
            next = await P.get("/api/doors");
          } catch (error) {
            clearInterval(timer);
            P.fail(error, "Couldn't refresh the door status");
            return;
          }
          const html = doorsPanel(next);
          if (html !== shown) {
            panel.innerHTML = html;
            shown = html;
          }
        }, DOORS_REFRESH_TIME);
        return () => clearInterval(timer);
      }
    };
  }
});
})();
