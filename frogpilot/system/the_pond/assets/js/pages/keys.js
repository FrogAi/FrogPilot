(() => {
"use strict";

const P = window.Pond;

const KEYS = {
  amap: { label: "Amap key", name: "Amap key", placeholder: "Your Web Service key", service: "Amap", title: "Amap key" },
  mapbox_public: { label: "public key", name: "Mapbox public key", placeholder: "pk.…", service: "Mapbox", title: "Mapbox public key" },
  mapbox_secret: { label: "secret key", name: "Mapbox secret key", placeholder: "sk.…", service: "Mapbox", title: "Mapbox secret key" },
  weather: { label: "weather key", name: "Weather key", placeholder: "32 letters and numbers", service: "OpenWeatherMap", title: "weather key" }
};

const LOST_EFFECTS = {
  amap: "Searching for places with Amap stops working until you add a key again.",
  mapbox_public: "Navigation is locked and openpilot stops getting speed limits from Mapbox until you add a public key again.",
  mapbox_secret: "Navigation is locked and the map stops showing on your driving screen until you add a secret key again.",
  weather: "FrogPilot goes back to shared weather, updated every 15 minutes."
};

const MAPBOX_SCOPES = ["DATASETS:WRITE", "MAP:WRITE", "STYLES:LIST", "STYLES:WRITE", "TOKENS:READ", "TOKENS:WRITE", "UPLOADS:WRITE", "USER:READ"];

const SAVE_MESSAGES = {
  mapbox_public: {
    empty_key: "Enter your public key first",
    invalid_key: "That doesn't look like a Mapbox key. Copy the whole key and try again",
    wrong_key_type: "That's your secret key. Enter your public key here"
  },
  mapbox_secret: {
    empty_key: "Enter your secret key first",
    invalid_key: "That doesn't look like a Mapbox key. Copy the whole key and try again",
    wrong_key_type: "That's your public key. Enter your secret key here"
  },
  weather: { empty_key: "Enter your weather key first", invalid_key: "That doesn't look like a weather key. It's up to 32 letters and numbers" }
};

const MAP_REBOOT_DETAIL = `The driving screen reads your secret key when the device starts. Reboot from the <a class="link" href="/device">Device</a> page.`;

P.mapRebootNotice = `<div class="notice">${P.ic("refresh")}<span><b>Reboot for the map to appear.</b> ${MAP_REBOOT_DETAIL}</span></div>`;

function link(href, text) {
  return `<a class="link" href="${href}" target="_blank" rel="noopener">${text}</a>`;
}

const GUIDES = {
  amap: {
    title: "Get your Amap key",
    intro: "Your Amap key lets the device search for places with Amap on the Navigation page while <b>Destination Search Provider</b> is set to Amap. " +
           "It has to be a Web Service key. Routes and the map on your driving screen still come from Mapbox, so navigation needs your Mapbox keys too.",
    sections: mode => [
      ["On lbs.amap.com", [
        `Open ${link("https://console.amap.com/", "Amap's console")} and log in, or register a developer account first.`,
        "Open <b>应用管理</b> (App Management), choose <b>创建新应用</b> (Create New App), and fill in the form.",
        "Under <b>我的应用</b> (My Apps), choose <b>添加 Key</b> (Add Key) for your app, and pick <b>Web 服务</b> (Web Service) as the " +
        "service platform.",
        "Copy the key it creates."
      ]],
      ["In The Pond", [
        `On this page, choose <b>${mode}…</b> on the <b>Web Service key</b> row, paste the key, and choose <b>Save key</b>.`,
        "Choose <b>Test</b> on the same row."
      ]]
    ],
    looks: "It's one line of letters and numbers.",
    sample: "3f2a9c7e1b4d8a6f0c5e9b2d7a4f1c8e",
    works: "You see <b>Your Amap key works</b>. Searches on the Navigation page then go through Amap.",
    problems: [
      ["This isn't a Web Service key", "The key was made for another platform, such as <b>Web 端 (JS API)</b>. Add a new key and pick " +
                                       "<b>Web 服务</b> as its service platform."],
      ["Amap didn't accept this key", "The key was copied wrong or has expired. Copy it again from <b>我的应用</b> (My Apps)."],
      ["Couldn't reach Amap", "The device isn't online. Connect it to Wi-Fi and choose <b>Test</b> again."]
    ]
  },
  mapbox_public: {
    title: "Get your Mapbox public key",
    intro: "Your public key lets you search for places and preview routes on the Navigation page, and lets FrogPilot get speed limits from Mapbox. " +
           "Mapbox only charges for use beyond its free monthly allowance. FrogPilot stops asking Mapbox for speed limits once it reaches 100,000 " +
           "lookups in a month, the free allowance for those lookups. Searches and route previews on the Navigation page aren't part of that count, " +
           "because your browser sends them to Mapbox directly.",
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
    intro: "Your secret key draws the map on your driving screen and gets your navigation route from Mapbox. Set up your public key first, since " +
           "this key comes from the same Mapbox account and needs the card from step 3 of that guide. Keep it to yourself, because anyone who has it " +
           "can use your Mapbox account and run up charges.",
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
    return `${service} didn't accept this key. Check it and add it again.`;
  }
  if (result.result === "wrong_key_type") {
    return "This isn't a Web Service key. Add a new key in Amap's console at lbs.amap.com and pick Web 服务 (Web Service) as its service platform.";
  }
  if (result.result === "failed" && kind === "amap") {
    return "Amap refused the request.";
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
  if (!key) {
    throw new P.PondError(400, "empty_key");
  }

  const result = await P.request("PUT", `/api/keys/${kind}`, { key });
  P.refresh();
  P.toast(`${KEYS[kind].name} saved`, result.reboot_required ? "It takes effect after the device reboots" : "");
}

async function saveWeather(form) {
  const input = form.key;
  const message = form.querySelector("[data-weather-error]");
  try {
    await saveKey("weather", input.value.trim());
  } catch (error) {
    if (!(error instanceof P.PondError)) {
      throw error;
    }

    if (!error.handled) {
      input.setAttribute("aria-invalid", "true");
      message.textContent = SAVE_MESSAGES.weather[error.code] || error.message;
      message.hidden = false;
      input.focus();
    }
  }
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

P.act("keys-edit", button => {
  const kind = button.dataset.kind;
  const key = KEYS[kind];
  P.dialog({
    icon: "keys", title: `${button.dataset.mode} your ${key.title}`,
    body: `<label class="field"><span>${key.name}</span><input class="input mono" data-key autocomplete="off" spellcheck="false" placeholder="${key.placeholder}"></label>`,
    confirm: "Save key",
    errors: SAVE_MESSAGES[kind],
    onConfirm: dialog => {
      return saveKey(kind, dialog.querySelector("[data-key]").value.trim());
    }
  });
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
    let says = "";
    if (result.message) {
      says = `<span class="keys-says">${KEYS[kind].service} says: "${P.esc(result.message)}"</span>`;
    }

    showTestResult(button, false, `Your ${KEYS[kind].label} didn't work.`, `${P.esc(testMessage(kind, result))}${says}`);
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

    let publicKey = "Not set";
    if (k.mapbox_public) {
      publicKey = `${P.esc(k.mapbox_public.slice(0, 8))}…${P.esc(k.mapbox_public.slice(-4))}`;
    }

    let weatherSaved = "";
    if (k.weather) {
      weatherSaved = `<small class="hint" id="keys-weather-saved">Saved, hidden. Enter a new key to replace it.</small>`;
    }

    let secretKey = "Not set";
    if (k.mapbox_secret) {
      secretKey = "Saved, hidden";
    }

    let amapPanel = "";
    let publicKeyLabel = "Public key, for searching places, route previews, and speed limits";
    if (k.amap_search) {
      let amapKey = "Not set";
      if (k.amap) {
        amapKey = "Saved, hidden";
      }

      amapPanel = `<section class="panel"><div class="panel-head"><h2>Amap</h2></div>
          <div class="panel-body keys-body">
            <div class="keys-rows">${keyRow("amap", "Web Service key, for searching places", amapKey)}</div>
          </div></section>`;
      publicKeyLabel = "Public key, for route previews and speed limits";
    }

    let rebootNotice = "";
    if (k.map_reboot_required) {
      rebootNotice = P.mapRebootNotice;
    }

    return {
      body: `
      <div class="two-col">
        ${amapPanel}
        <section class="panel"><div class="panel-head"><h2>Mapbox</h2></div>
          <div class="panel-body keys-body">
            <div><div class="keys-usage"><b>Requests this month</b><span class="num"><b>${k.mapbox_requests.toLocaleString("en-US")}</b> <span class="muted">of ${k.mapbox_request_limit.toLocaleString("en-US")}</span></span></div>
              <div class="progress keys-progress" role="img" aria-label="${Math.round(share)}% of this month's requests used"><i data-w="${share}"></i></div></div>
            <div class="keys-rows">
              ${keyRow("mapbox_public", publicKeyLabel, publicKey, Boolean(k.mapbox_public))}
              ${keyRow("mapbox_secret", "Secret key, for routes and the map on the driving screen", secretKey)}
            </div>
            ${rebootNotice}
          </div></section>
        <section class="panel"><div class="panel-head"><h2>Weather</h2></div>
          <div class="panel-body"><form class="form-grid" data-weather>
            <p class="muted">Weather updates every 15 minutes. Add your own OpenWeatherMap key to update it every minute.</p>
            <label class="field"><span>Weather key</span>
              <input class="input mono" name="key" autocomplete="off" spellcheck="false" placeholder="${KEYS.weather.placeholder}"
                aria-describedby="keys-weather-saved keys-weather-error">
              ${weatherSaved}
              <small class="hint keys-error" id="keys-weather-error" role="alert" data-weather-error hidden></small></label>
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
        saveWeather(e.currentTarget);
      })
    };
  }
});
})();
