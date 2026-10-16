(() => {
"use strict";

const P = window.Pond;

const KEYS = {
  mapbox_public: { label: "public key", name: "Mapbox public key", placeholder: "pk.…", service: "Mapbox", title: "Mapbox public key" },
  mapbox_secret: { label: "secret key", name: "Mapbox secret key", placeholder: "sk.…", service: "Mapbox", title: "Mapbox secret key" },
  weather: { label: "weather key", name: "Weather key", placeholder: "32 letters and numbers", service: "OpenWeatherMap", title: "weather key" }
};

const LOST_EFFECTS = {
  mapbox_public: "openpilot stops getting speed limits from Mapbox until you add a public key again.",
  mapbox_secret: "The map stops showing on your driving screen until you add a secret key again.",
  weather: "FrogPilot goes back to shared weather, updated every 15 minutes."
};

const MAPBOX_SCOPES = ["DATASETS:WRITE", "MAP:WRITE", "STYLES:LIST", "STYLES:WRITE", "TOKENS:READ", "TOKENS:WRITE", "UPLOADS:WRITE", "USER:READ"];

const SAVE_MESSAGES = {
  mapbox_public: { invalid_key: "That doesn't look like a Mapbox key. Copy the whole key and try again", wrong_key_type: "That's your secret key. Enter your public key here" },
  mapbox_secret: { invalid_key: "That doesn't look like a Mapbox key. Copy the whole key and try again", wrong_key_type: "That's your public key. Enter your secret key here" },
  weather: { invalid_key: "That doesn't look like a weather key. It's up to 32 letters and numbers" }
};

function link(href, text) {
  return `<a class="link" href="${href}" target="_blank" rel="noopener">${text}</a>`;
}

const GUIDES = {
  mapbox_public: {
    title: "Get your Mapbox public key",
    intro: "Your public key lets FrogPilot get speed limits from Mapbox. Mapbox only charges for use beyond its free monthly allowance. FrogPilot stops asking Mapbox once it reaches 100,000 lookups in a month, the free allowance for those lookups, so the key doesn't cost you anything.",
    sections: mode => [
      ["On mapbox.com", [
        `Open ${link("https://account.mapbox.com/auth/signup/", "Mapbox's sign-up page")}, fill in your details, and create your account. If you choose <b>Start without a credit card</b>, you add a card in step 3.`,
        "Open the email from <span class=\"mono\">no-reply@mapbox.com</span> and select its link to verify your email. The link works for 24 hours.",
        `Log in at ${link("https://console.mapbox.com/", "console.mapbox.com")}. If the <b>Welcome to Mapbox</b> banner shows at the top, choose <b>Add payment method</b>, enter your card, and pick <b>Rotate token</b> when it asks. Mapbox needs a card on file to give you its full free allowance and to let you make the secret key.`,
        `Open ${link("https://console.mapbox.com/account/access-tokens/", "Access tokens")}. Your <b>Default public token</b> is at the top of the list. Select the copy button next to it.`
      ]],
      ["In The Pond", [
        `On this page, choose <b>${mode}…</b> on the <b>Public key</b> row, paste the key, and choose <b>Save key</b>.`,
        "Choose <b>Test</b> on the same row."
      ]]
    ],
    looks: "It starts with <span class=\"mono\">pk.</span> and is one long line of about 90 or more letters, numbers, dots, dashes, and underscores.",
    sample: "pk.eyJ1IjoieW91cm5hbWUiLCJhIjoi…",
    works: "You see <b>Your public key works</b>. If the car is on when you save it, it starts working after the device reboots.",
    problems: [
      ["That's your secret key", "Your public key starts with <span class=\"mono\">pk.</span>, not <span class=\"mono\">sk.</span> Copy the <b>Default public token</b> instead."],
      ["That doesn't look like a Mapbox key", "Part of the key is missing. Copy it again with the copy button instead of selecting the text."],
      ["Mapbox didn't accept this key", "The key was copied wrong or no longer exists. If you chose <b>Rotate token</b> after copying it, the old key stopped working, so copy the new <b>Default public token</b>."],
      ["Couldn't reach Mapbox", "The device isn't online. Connect it to Wi-Fi and choose <b>Test</b> again."]
    ]
  },
  mapbox_secret: {
    title: "Get your Mapbox secret key",
    intro: "Your secret key draws the map on your driving screen. Set up your public key first, since this key comes from the same Mapbox account and needs the card from step 3 of that guide. Keep it to yourself, because anyone who has it can use your Mapbox account and run up charges.",
    sections: mode => [
      ["On mapbox.com", [
        `Log in at ${link("https://console.mapbox.com/", "console.mapbox.com")}, open ${link("https://console.mapbox.com/account/access-tokens/", "Access tokens")}, and choose the blue <b>Create a token</b> button.`,
        "Give it any name you like, such as FrogPilot.",
        "Under <b>Public scopes</b>, tick every box.",
        `Under <b>Secret scopes</b>, tick these eight and leave the rest unticked.<span class="keys-scopes">${MAPBOX_SCOPES.map(scope => `<span class="mono">${scope}</span>`).join("")}</span>`,
        "Scroll down, choose <b>Create token</b>, and enter your Mapbox password if it asks.",
        "Back on <b>Access tokens</b>, copy the new token with its copy button right away. Mapbox shows a secret token only once, so if you lose it, delete it and create another."
      ]],
      ["In The Pond", [
        `On this page, choose <b>${mode}…</b> on the <b>Secret key</b> row, paste the key, and choose <b>Save key</b>.`,
        "Choose <b>Test</b> on the same row.",
        "Reboot the device. The map shows on your driving screen after the reboot."
      ]]
    ],
    looks: "It starts with <span class=\"mono\">sk.</span> and is one long line of about 90 or more letters, numbers, dots, dashes, and underscores.",
    sample: "sk.eyJ1IjoieW91cm5hbWUiLCJhIjoi…",
    works: "You see <b>Your secret key works</b>. After the reboot, the map shows on your driving screen.",
    problems: [
      ["That's your public key", "Your secret key starts with <span class=\"mono\">sk.</span>, not <span class=\"mono\">pk.</span> Create a new token as above instead of using the <b>Default public token</b>."],
      ["You can't create a token", "Mapbox only lets accounts with a card on file create tokens. Add one with <b>Add payment method</b>, as in step 3 of the public key guide."],
      ["That doesn't look like a Mapbox key", "Part of the key is missing. Copy it again with the copy button instead of selecting the text."],
      ["Mapbox didn't accept this key", "The key was copied wrong or deleted on Mapbox. Create a new one and add it again."],
      ["The map doesn't show", "The device hasn't rebooted since you saved the key. Reboot it."]
    ]
  },
  weather: {
    title: "Get your weather key",
    intro: "Your own OpenWeatherMap key updates FrogPilot's weather every minute instead of every 15 minutes. It needs OpenWeatherMap's One Call by Call subscription, which is how OpenWeatherMap sells One Call API 4.0. The first 1,000 requests each day are free, and FrogPilot asks about once a minute while you drive, so normal driving stays free.",
    sections: () => [
      ["On openweathermap.org", [
        `Open ${link("https://home.openweathermap.org/users/sign_up", "OpenWeatherMap's sign-up page")}, fill in the form, tick the boxes for your age and the terms, and create your account. Then select the link in the email OpenWeatherMap sends you to verify your email.`,
        `<b>Subscribe to One Call by Call. No key works without it, not even a new one.</b> Open the ${link("https://openweathermap.org/price", "Pricing page")}. Under <b>Timeline-Based Weather One Call API</b>, on the <b>One Call pay-per-call</b> card, choose <b>Subscribe</b>, then fill in the billing form.`,
        "So you're never charged, open the <b>Billing plans</b> tab in your account and set the daily limit for One Call API 4.0 to 1,000 calls. It starts at 2,000.",
        `Open the ${link("https://home.openweathermap.org/api_keys", "API keys")} tab in your account and copy your key.`
      ]],
      ["In The Pond", [
        "On this page, paste it into <b>Weather key</b> and choose <b>Save key</b>.",
        "Choose <b>Test</b>. OpenWeatherMap can take up to about 2 hours to start accepting a new key or subscription, so if the test fails right after you subscribe, choose <b>Test</b> again later."
      ]]
    ],
    looks: "It's 32 letters and numbers, with no spaces or dots.",
    sample: "3f2a9c7e1b4d8a6f0c5e9b2d7a4f1c8e",
    works: "You see <b>Your weather key works</b> and <b>It's valid for One Call API 4.0</b>. Without a working key, FrogPilot uses shared weather, updated every 15 minutes.",
    problems: [
      ["OpenWeatherMap didn't accept this key", "The account doesn't have an active One Call by Call subscription yet, or the key was copied wrong. A new key or subscription can take up to about 2 hours to start working. Check the subscription on the <b>Billing plans</b> tab, then choose <b>Test</b> again later."],
      ["That doesn't look like a weather key", "You copied more than the key. Copy just the 32 letters and numbers."],
      ["OpenWeatherMap's request limit was reached", "Today's daily limit is used up. It resets just after midnight UTC."],
      ["Couldn't reach OpenWeatherMap", "The device isn't online. Connect it to Wi-Fi and choose <b>Test</b> again."]
    ]
  }
};

function testMessage(kind, result) {
  const service = KEYS[kind].service;
  if (result.result === "invalid" && kind === "weather") {
    return `OpenWeatherMap didn't accept this key. Your OpenWeatherMap account needs the One Call by Call subscription. A new key or subscription can take up to about 2 hours to start working, so choose Test again later. Until then, FrogPilot uses shared weather, updated every 15 minutes.`;
  }
  if (result.result === "invalid") {
    return "Mapbox didn't accept this key. Check it and add it again.";
  }
  if (result.result === "rate_limited") {
    return `${service}'s request limit was reached. Try again later.`;
  }
  if (result.result === "incomplete") {
    return "OpenWeatherMap returned incomplete weather data. Try again later.";
  }
  if (result.result === "unreachable") {
    return `Couldn't reach ${service}. Check the device's internet connection and try again.`;
  }
  return `The ${service} request failed (HTTP ${result.status}).`;
}

async function saveKey(kind, key) {
  let result;
  try {
    result = await P.request("PUT", `/api/keys/${kind}`, { key });
  } catch (error) {
    P.fail(error, `Couldn't save your ${KEYS[kind].label}`, SAVE_MESSAGES[kind]);
    return false;
  }
  P.refresh();
  P.toast(`${KEYS[kind].name} saved`, result.reboot_required ? "It takes effect after the device reboots" : "");
  return true;
}

function keyRow(kind, label, value, mono = false) {
  const mode = value === "Not set" ? "Add" : "Replace";
  return `<div class="list-row">
    <span class="grow"><b>${label}</b><span class="${mono ? "mono" : ""}">${value}</span></span>
    <span class="acts">
      <button class="icon-btn" data-act="keys-help" data-kind="${kind}" data-mode="${mode}" aria-label="How to get your ${KEYS[kind].label}" title="How to get it">${P.ic("info")}</button>
      ${value !== "Not set" ? `<button class="btn btn-quiet btn-small" data-act="keys-test" data-kind="${kind}">Test</button>` : ""}
      <button class="btn btn-quiet btn-small" data-act="keys-edit" data-kind="${kind}" data-mode="${mode}">${mode}…</button>
      ${value !== "Not set" ? `<button class="icon-btn danger" data-act="keys-remove" data-kind="${kind}" aria-label="Remove your ${KEYS[kind].label}" title="Remove">${P.ic("trash")}</button>` : ""}
    </span>
  </div>`;
}

P.act("keys-help", button => {
  const guide = GUIDES[button.dataset.kind];
  const sections = guide.sections(button.dataset.mode).map(([heading, steps]) => `<section><h3>${heading}</h3><ol class="guide-steps">${steps.map(step => `<li><span>${step}</span></li>`).join("")}</ol></section>`);
  P.dialog({
    icon: "info", wide: true, title: guide.title,
    body: `<p>${guide.intro}</p>
      <div class="guide">
        ${sections[0]}
        <section><h3>What the key looks like</h3><p>${guide.looks}</p><code class="code-box keys-sample">${guide.sample}</code></section>
        ${sections.slice(1).join("")}
        <section><h3>When it works</h3><p>${guide.works}</p></section>
        <section><h3>If it doesn't work</h3><dl class="guide-problems">${guide.problems.map(([message, fix]) => `<dt>${message}</dt><dd>${fix}</dd>`).join("")}</dl></section>
      </div>`,
    confirm: "Got it", cancel: ""
  });
});

P.act("keys-edit", async button => {
  const kind = button.dataset.kind;
  const key = KEYS[kind];
  const dialog = await P.dialog({
    icon: "keys", title: `${button.dataset.mode} your ${key.title}`,
    body: `<label class="field"><span>${key.name}</span><input class="input mono" data-key autocomplete="off" spellcheck="false" placeholder="${key.placeholder}"></label>`,
    confirm: "Save key"
  });
  if (!dialog) {
    return;
  }
  const value = dialog.querySelector("[data-key]").value.trim();
  if (value) {
    saveKey(kind, value);
  }
});

function showTestResult(button, works, title, detail) {
  const notice = document.createElement("div");
  notice.className = `notice keys-result ${works ? "" : "danger"}`;
  notice.setAttribute("role", "status");
  notice.innerHTML = `${P.ic(works ? "check" : "alert")}<span><b>${title}</b>${detail ? ` ${detail}` : ""}</span><button class="icon-btn" type="button" data-act="keys-result-close" aria-label="Dismiss" title="Dismiss">${P.ic("x")}</button>`;
  button.closest(".keys-body, .form-grid").append(notice);
}

P.act("keys-result-close", button => button.closest(".keys-result").remove());

P.act("keys-test", async button => {
  const kind = button.dataset.kind;
  document.querySelector(".keys-result")?.remove();
  let result;
  try {
    result = await P.busy(button, P.request("POST", `/api/keys/${kind}/test`));
  } catch (error) {
    P.fail(error, `Couldn't test your ${KEYS[kind].label}`);
    return;
  }
  if (result.result === "valid") {
    showTestResult(button, true, `Your ${KEYS[kind].label} works.`, kind === "weather" ? "It's valid for One Call API 4.0." : "");
  } else {
    showTestResult(button, false, `Your ${KEYS[kind].label} didn't work.`, `${P.esc(testMessage(kind, result))}${result.message ? `<span class="keys-says">OpenWeatherMap says: "${P.esc(result.message)}"</span>` : ""}`);
  }
});

P.act("keys-remove", async button => {
  const kind = button.dataset.kind;
  const key = KEYS[kind];
  const ok = await P.dialog({
    icon: "trash", tone: "danger", confirmTone: "danger", title: `Remove your ${key.title}?`,
    body: `<p>${LOST_EFFECTS[kind]}</p>${P.lossList([`Your ${key.title}`], kind === "weather" ? [] : ["Favorite places", "Speed limit maps"])}`,
    confirm: "Remove key"
  });
  if (!ok) {
    return;
  }
  try {
    await P.request("DELETE", `/api/keys/${kind}`);
  } catch (error) {
    P.fail(error, `Couldn't remove your ${key.label}`);
    return;
  }
  P.refresh();
  P.toast(`${key.name} removed`);
});

P.page("keys", {
  title: "Keys",
  async render() {
    const k = await P.get("/api/keys");
    const share = Math.min(100, k.mapbox_requests / k.mapbox_request_limit * 100);
    return {
      body: `
      <div class="two-col">
        <section class="panel"><div class="panel-head"><h2>Mapbox</h2></div>
          <div class="panel-body keys-body">
            <div><div class="keys-usage"><b>Requests this month</b><span class="num"><b>${k.mapbox_requests.toLocaleString("en-US")}</b> <span class="muted">of ${k.mapbox_request_limit.toLocaleString("en-US")}</span></span></div>
              <div class="progress keys-progress" role="img" aria-label="${Math.round(share)}% of this month's requests used"><i data-w="${share}"></i></div></div>
            <div class="keys-rows">
              ${keyRow("mapbox_public", "Public key, for speed limits", k.mapbox_public ? `${P.esc(k.mapbox_public.slice(0, 8))}…${P.esc(k.mapbox_public.slice(-4))}` : "Not set", Boolean(k.mapbox_public))}
              ${keyRow("mapbox_secret", "Secret key, for the map on the driving screen", k.mapbox_secret ? "Saved, hidden" : "Not set")}
            </div>
          </div></section>
        <section class="panel"><div class="panel-head"><h2>Weather</h2></div>
          <div class="panel-body"><form class="form-grid" data-weather>
            <p class="muted">Weather updates every 15 minutes. Add your own OpenWeatherMap key to update it every minute.</p>
            <label class="field"><span>Weather key</span><input class="input mono" name="key" autocomplete="off" spellcheck="false" placeholder="${k.weather ? "Saved, hidden. Enter a new key to replace it" : KEYS.weather.placeholder}"></label>
            <div class="keys-buttons">
              <button class="btn btn-primary btn-small" type="submit">Save key</button>
              ${k.weather ? `<button class="btn btn-quiet btn-small" type="button" data-act="keys-test" data-kind="weather">Test</button>
              <button class="btn btn-danger-quiet btn-small" type="button" data-act="keys-remove" data-kind="weather">Remove</button>` : ""}
              <button class="btn btn-text btn-small keys-guide" type="button" data-act="keys-help" data-kind="weather">${P.ic("info")}How to get a key</button>
            </div>
          </form></div></section>
      </div>`,
      mount: root => root.querySelector("[data-weather]").addEventListener("submit", e => {
        e.preventDefault();
        const input = e.currentTarget.key;
        const value = input.value.trim();
        if (!value) {
          input.setAttribute("aria-invalid", "true");
          P.toast("Enter your weather key first", "", "alert");
          return;
        }
        input.removeAttribute("aria-invalid");
        saveKey("weather", value);
      })
    };
  }
});
})();
