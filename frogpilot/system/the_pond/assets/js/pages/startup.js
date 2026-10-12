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

function lineField(label, name, value, maxLength) {
  const input = `<input class="input" name="${name}" maxlength="${maxLength}" value="${P.esc(value)}" aria-describedby="startup-${name}-hint">`;
  const hint = `<small class="hint" id="startup-${name}-hint" aria-hidden="true">Up to ${maxLength} characters</small>`;
  return `<label class="field"><span>${label}</span>${input}${hint}</label>`;
}

async function save(form, alert) {
  const top = form.top.value.trim();
  const bottom = form.bottom.value.trim();
  form.top.setAttribute("aria-invalid", String(!top));
  form.bottom.setAttribute("aria-invalid", String(!bottom));

  try {
    if (!top || !bottom) {
      form.querySelector("[aria-invalid='true']").focus();
      throw new P.PondError(400, "invalid_text");
    }

    await P.busy(form.querySelector("[type=submit]"), P.request("PUT", "/api/themes/startup_alert", { StartupMessageBottom: bottom, StartupMessageTop: top }));
  } catch (error) {
    const invalidText = `Both lines need some text, up to ${alert.StartupMessageTop.max_length} characters on top and ${alert.StartupMessageBottom.max_length} on the bottom`;
    P.fail(error, "Couldn't save the startup message", { invalid_text: invalidText });
    return;
  }
  P.toast("Startup message saved", "Shows next time the car starts");
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
          ${lineField("Top line", "top", top, alert.StartupMessageTop.max_length)}
          ${lineField("Bottom line", "bottom", bottom, alert.StartupMessageBottom.max_length)}
          <div><button class="btn btn-primary btn-small" type="submit">Save message</button></div>
        </form></div></section>
        <section class="panel"><div class="panel-head"><h2>On the car</h2></div><div class="panel-body">
          <div class="alert-preview"><b data-pt>${P.esc(top)}</b><span data-pb>${P.esc(bottom)}</span></div>
          <p class="hint mt-12">openpilot shows this on the car's screen at the start of every drive.</p>
        </div></section>
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
        for (const button of form.querySelectorAll("[data-preset]")) {
          button.addEventListener("click", () => {
            const id = button.dataset.preset;
            if (id === "custom") {
              for (const other of form.querySelectorAll("[data-preset]")) {
                other.setAttribute("aria-pressed", String(other === button));
              }

              form.top.focus();
              return;
            }

            form.top.value = alert.StartupMessageTop[id];
            form.bottom.value = alert.StartupMessageBottom[id];
            show();
          });
        }

        form.addEventListener("submit", e => {
          e.preventDefault();
          save(form, alert);
        });
      }
    };
  }
});
})();
