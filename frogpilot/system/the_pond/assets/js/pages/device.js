(() => {
"use strict";

const P = window.Pond;

const POWER = {
  off: { body: "The device shuts down and The Pond goes offline until the car starts again.", confirm: "Power off", fail: "Couldn't power off", path: "/api/device/power_off", restart: "The device is off", title: "Power off the device?" },
  reboot: { body: "The device restarts and The Pond reconnects on its own.", confirm: "Reboot", fail: "Couldn't reboot", path: "/api/device/reboot", restart: "Rebooting", title: "Reboot the device?" },
  soft: { body: "openpilot restarts without restarting the device, and The Pond reconnects on its own.", confirm: "Soft reboot", fail: "Couldn't soft reboot", messages: { openpilot_steering: "Disengage to soft reboot" }, path: "/api/device/soft_reboot", restart: "Soft rebooting", title: "Are you sure you want to soft reboot?" }
};
const STORAGE_LABELS = [["dashcam", "Drives"], ["screen_recordings", "Screen recordings"], ["maps", "Speed limit maps"], ["models", "Driving models"],
  ["frogpilot_backups", "FrogPilot backups"], ["themes", "Themes"], ["software", "FrogPilot software"], ["the_pond", "The Pond cache"]];
const STRENGTHS = { good: "good", great: "strong", moderate: "fair", poor: "weak" };
const THERMAL = { danger: "too hot", green: "normal", red: "hot", yellow: "warm" };

function network(n) {
  const parts = [P.NETWORK_TYPES[n.type] || n.type];
  if (STRENGTHS[n.strength]) {
    parts.push(STRENGTHS[n.strength]);
  }
  parts.push(n.metered ? "metered" : "not metered");
  return parts.join(", ");
}

function nowHTML(d) {
  const avg = Math.round(d.cpu_usage.reduce((a, b) => a + b, 0) / d.cpu_usage.length);
  return `<dt>Device</dt><dd>${P.esc(d.model)}</dd>
    <dt>Car battery</dt><dd>${d.voltage.toFixed(1)}${P.NB}V</dd>
    <dt>Temperature</dt><dd>${d.temperature.toFixed(1)}${P.NB}°C, ${THERMAL[d.thermal_status] || P.esc(d.thermal_status)}</dd>
    <dt>CPU</dt><dd>${avg}% average</dd>
    <dt>GPU</dt><dd>${d.gpu_usage}% busy</dd>
    <dt>Network</dt><dd>${P.esc(network(d.network))}</dd>`;
}

function storageRows(d, usage) {
  const rows = STORAGE_LABELS.map(([key, label]) => {
    const bytes = key === "dashcam" ? Object.values(usage.dashcam).reduce((a, b) => a + b, 0) : usage[key];
    return [label, bytes];
  });
  const other = d.storage.used_space * 2 ** 30 - rows.reduce((sum, [, bytes]) => sum + bytes, 0);
  if (other > 0) {
    rows.push(["Other", other]);
  }
  return rows.map(([label, bytes]) => `<dt>${label}</dt><dd>${P.size(bytes)}</dd>`).join("");
}

function alertsHTML(d) {
  const notices = d.alerts.map(a => `<div class="notice ${a.severity > 0 ? "danger" : ""}">${P.ic("alert")}<span>${P.esc(a.text)}</span></div>`);
  return notices.join("") || `<p class="muted">Nothing needs attention.</p>`;
}

P.page("device", {
  title: "Device and Power",
  async render() {
    const [d, usage] = await Promise.all([P.get("/api/device"), P.get("/api/device/storage")]);
    return {
      body: `
      <div class="two-col">
        <div class="stack">
          <section class="panel"><div class="panel-head"><h2>Right now</h2></div>
            <div class="panel-body"><dl class="kv" data-now>${nowHTML(d)}</dl></div></section>
          <section class="panel"><div class="panel-head"><h2>Power</h2></div>
            <div class="panel-body">
              <div class="danger-zone"><span><b>Reboot</b><span class="hint">The Pond reconnects when it is back up</span></span><button class="btn btn-quiet btn-small" data-act="power" data-kind="reboot">Reboot</button></div>
              <div class="danger-zone"><span><b>Soft reboot</b><span class="hint">Restarts openpilot without restarting the device</span></span><button class="btn btn-quiet btn-small" data-act="power" data-kind="soft">Soft reboot</button></div>
              <div class="danger-zone"><span><b>Power off</b><span class="hint">It starts again when the car does</span></span><button class="btn btn-danger-quiet btn-small" data-act="power" data-kind="off">Power off</button></div>
            </div></section>
        </div>
        <div class="stack">
          <section class="panel" id="device-storage"><div class="panel-head"><h2>Storage</h2></div>
            <div class="panel-body"><p class="big-figure">${d.storage.free_space}${P.NB}GB<small>free</small></p>
              <dl class="kv">${storageRows(d, usage)}</dl></div></section>
          <section class="panel" id="device-alerts"><div class="panel-head"><h2>Alerts on the car</h2></div>
            <div class="panel-body grid-10">${alertsHTML(d)}</div></section>
        </div>
      </div>`,
      mount: root => {
        const timer = setInterval(async () => {
          if (document.hidden) {
            return;
          }
          try {
            root.querySelector("[data-now]").innerHTML = nowHTML(await P.get("/api/device"));
          } catch (error) {
            clearInterval(timer);
            P.fail(error, "Couldn't refresh the device status");
          }
        }, 5000);
        return () => clearInterval(timer);
      }
    };
  }
});

P.act("power", async b => {
  const off = b.dataset.kind === "off";
  const power = POWER[b.dataset.kind];
  const ok = await P.dialog({ icon: "power", tone: off ? "danger" : "", confirmTone: off ? "danger" : "primary", title: power.title, body: `<p>${power.body}</p>`, confirm: power.confirm });
  if (!ok) {
    return;
  }
  try {
    await P.request("POST", power.path);
  } catch (error) {
    P.fail(error, power.fail, power.messages);
    return;
  }
  P.restart(power.restart);
});
})();
