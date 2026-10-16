(() => {
"use strict";

const P = window.Pond;

const PRESETS = [["stock", "Stock"], ["frogpilot", "FrogPilot"], ["custom", "Custom"]];

function presetOf(alert, top, bottom) {
  for (const [preset] of PRESETS.slice(0, 2)) {
    if (top === alert.StartupMessageTop[preset] && bottom === alert.StartupMessageBottom[preset]) {
      return preset;
    }
  }
  return "custom";
}

async function save(button, alert, top, bottom, text) {
  try {
    await P.busy(button, P.request("PUT", "/api/themes/startup_alert", { StartupMessageBottom: bottom, StartupMessageTop: top }));
  } catch (error) {
    const invalidText = `Both lines need some text, up to ${alert.StartupMessageTop.max_length} characters on top and ${alert.StartupMessageBottom.max_length} on the bottom`;
    P.fail(error, "Couldn't save the startup message", { invalid_text: invalidText });
    return false;
  }
  P.toast(text, "Shows next time the car starts");
  return true;
}

P.page("startup", {
  title: "Startup Alert",
  async render() {
    const alert = (await P.get("/api/themes")).startup_alert;
    const top = alert.StartupMessageTop.current;
    const bottom = alert.StartupMessageBottom.current;
    const preset = presetOf(alert, top, bottom);
    return {
      body: `
      <div class="two-col">
        <section class="panel"><div class="panel-body"><form class="form-grid" data-startup>
          <div class="field"><span>Message</span><div class="seg startup-presets" role="group" aria-label="Message">${PRESETS.map(([id, label]) => `<button type="button" data-preset="${id}" aria-pressed="${id === preset}">${label}</button>`).join("")}</div></div>
          <label class="field"><span>Top line</span><input class="input" name="top" maxlength="${alert.StartupMessageTop.max_length}" value="${P.esc(top)}"></label>
          <label class="field"><span>Bottom line</span><input class="input" name="bottom" maxlength="${alert.StartupMessageBottom.max_length}" value="${P.esc(bottom)}"></label>
          <div><button class="btn btn-primary btn-small" type="submit">Save message</button></div>
        </form></div></section>
        <section class="panel"><div class="panel-head"><h2>On the car</h2></div><div class="panel-body"><div class="alert-preview"><b data-pt>${P.esc(top)}</b><span data-pb>${P.esc(bottom)}</span></div></div></section>
      </div>`,
      mount: root => {
        const form = root.querySelector("[data-startup]");
        const show = () => {
          root.querySelector("[data-pt]").textContent = form.top.value;
          root.querySelector("[data-pb]").textContent = form.bottom.value;
          const current = presetOf(alert, form.top.value.trim(), form.bottom.value.trim());
          form.querySelectorAll("[data-preset]").forEach(button => button.setAttribute("aria-pressed", String(button.dataset.preset === current)));
        };
        form.addEventListener("input", show);
        form.querySelectorAll("[data-preset]").forEach(button => button.addEventListener("click", async () => {
          const id = button.dataset.preset;
          if (id === "custom") {
            form.querySelectorAll("[data-preset]").forEach(other => other.setAttribute("aria-pressed", String(other === button)));
            form.top.focus();
            return;
          }
          const label = PRESETS.find(([preset]) => preset === id)[1];
          if (await save(button, alert, alert.StartupMessageTop[id], alert.StartupMessageBottom[id], `${label} startup message saved`)) {
            form.top.value = alert.StartupMessageTop[id];
            form.bottom.value = alert.StartupMessageBottom[id];
            show();
          }
        }));
        form.addEventListener("submit", e => {
          e.preventDefault();
          save(form.querySelector("[type=submit]"), alert, form.top.value.trim(), form.bottom.value.trim(), "Startup message saved");
        });
      }
    };
  }
});
})();
