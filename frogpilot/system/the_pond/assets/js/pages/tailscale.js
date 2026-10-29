(() => {
"use strict";

const P = window.Pond;

const GUIDE = {
  intro: "Tailscale links your devices into one private network, called a tailnet. The phone or computer you open The Pond on needs Tailscale too, signed in to the same account as this device.",
  sections: [
    ["On this page", [
      "Choose <b>Install Tailscale</b>, then <b>Sign in</b>.",
      "Open the sign-in page, or scan its code with your phone, and log in with the account you want to use. The status here changes to <b>Connected</b>."
    ]],
    ["On your phone or computer", [
      `Install the Tailscale app: from the ${link("https://apps.apple.com/app/tailscale/id1470499037", "App Store")} on an iPhone or iPad, from ${link("https://play.google.com/store/apps/details?id=com.tailscale.ipn", "Google Play")} on Android, or from ${link("https://tailscale.com/download", "tailscale.com/download")} on Windows and Mac.`,
      "Open the app and sign in. On a phone, tap <b>Get Started</b> and accept the prompt to install a VPN configuration. On Windows, right-click the Tailscale icon in the system tray and choose <b>Log in</b>.",
      "<b>Sign in with the same account you used for this device.</b> A different account puts the phone or computer in a different tailnet, where it can't see this device."
    ]],
    ["In the Tailscale admin console", [
      `Open the ${link("https://login.tailscale.com/admin/dns", "DNS page")}. If it shows <b>Enable MagicDNS</b>, choose it. MagicDNS gives this device its short address. Without it, use the IP address link.`,
      `Open the ${link("https://login.tailscale.com/admin/machines", "Machines page")} and check that this device and your phone or computer are both listed and connected.`
    ]],
    ["Open The Pond", [
      "On your phone or computer, open one of the links under <b>Open The Pond</b> on this page."
    ]]
  ],
  works: "The Pond opens on your phone or computer, and <b>Other devices online</b> on this page counts it.",
  problems: [
    ["The phone or computer uses a different account", "Sign out of the Tailscale app there and sign in with the account this device uses. Both must show on the same Machines page."],
    ["Tailscale is off on the phone or computer", "Open the Tailscale app and turn it on, then try the link again."],
    ["The short address doesn't open", "MagicDNS is off in your tailnet, or that device doesn't use Tailscale's DNS settings. Turn on MagicDNS on the admin console's DNS page, or use the IP address link."],
    ["Another VPN is on", "iPhones and Android phones run only one VPN at a time, so a work or other VPN turns Tailscale off. Turn the other VPN off. On a computer, another VPN can also block Tailscale's addresses."],
    ["The Pond asks for a pairing code", "Each address counts as a new browser. Enter the code shown on the device next to <b>Manage Your Settings At</b> in FrogPilot's Navigation settings."]
  ]
};
const MESSAGES = { invalid_hostname: "Use up to 63 letters, numbers, and dashes, starting and ending with a letter or number" };
const QR_BLOCKS = [[10, 1, 16], [16, 1, 28], [26, 1, 44], [18, 2, 32], [24, 2, 43], [16, 4, 27]];
const QR_MASKS = [
  (x, y) => (x + y) % 2 === 0,
  (x, y) => y % 2 === 0,
  x => x % 3 === 0,
  (x, y) => (x + y) % 3 === 0,
  (x, y) => (Math.floor(x / 3) + Math.floor(y / 2)) % 2 === 0,
  (x, y) => (x * y) % 2 + (x * y) % 3 === 0,
  (x, y) => ((x * y) % 2 + (x * y) % 3) % 2 === 0,
  (x, y) => ((x + y) % 2 + (x * y) % 3) % 2 === 0
];
const STATES = { NeedsLogin: "Signed out", NeedsMachineAuth: "Waiting for approval", NoState: "Starting", Running: "Connected", Starting: "Starting", Stopped: "Stopped" };

let tracking = false;

function link(href, text) {
  return `<a class="link" href="${href}" target="_blank" rel="noopener noreferrer">${text}</a>`;
}

function guideBody() {
  return `<p>${GUIDE.intro}</p>
    <div class="guide">
      ${GUIDE.sections.map(([heading, steps]) => `<section><h3>${heading}</h3><ol class="guide-steps">${steps.map(step => `<li><span>${step}</span></li>`).join("")}</ol></section>`).join("")}
      <section><h3>When it works</h3><p>${GUIDE.works}</p></section>
      <section><h3>If it doesn't work</h3><dl class="guide-problems">${GUIDE.problems.map(([problem, fix]) => `<dt>${problem}</dt><dd>${fix}</dd>`).join("")}</dl></section>
    </div>`;
}

function gfMultiply(x, y) {
  let z = 0;
  for (let i = 7; i >= 0; i--) {
    z = (z << 1) ^ ((z >>> 7) * 0x11D);
    z ^= ((y >>> i) & 1) * x;
  }
  return z;
}

function errorCorrection(data, degree) {
  const divisor = new Array(degree).fill(0);
  divisor[degree - 1] = 1;
  let root = 1;
  for (let i = 0; i < degree; i++) {
    for (let j = 0; j < degree; j++) {
      divisor[j] = gfMultiply(divisor[j], root);
      if (j + 1 < degree) {
        divisor[j] ^= divisor[j + 1];
      }
    }
    root = gfMultiply(root, 2);
  }
  const remainder = new Array(degree).fill(0);
  for (const byte of data) {
    const factor = byte ^ remainder.shift();
    remainder.push(0);
    divisor.forEach((coefficient, i) => {
      remainder[i] ^= gfMultiply(coefficient, factor);
    });
  }
  return remainder;
}

function qrCodewords(bytes, version) {
  const [ecLength, blockCount, blockLength] = QR_BLOCKS[version - 1];
  const capacity = blockCount * blockLength * 8;
  const bits = [];
  const push = (value, length) => {
    for (let i = length - 1; i >= 0; i--) {
      bits.push((value >>> i) & 1);
    }
  };
  push(4, 4);
  push(bytes.length, 8);
  bytes.forEach(byte => push(byte, 8));
  push(0, Math.min(4, capacity - bits.length));
  push(0, (8 - (bits.length % 8)) % 8);
  for (let pad = 0xEC; bits.length < capacity; pad ^= 0xEC ^ 0x11) {
    push(pad, 8);
  }

  const data = [];
  for (let i = 0; i < bits.length; i += 8) {
    data.push(bits.slice(i, i + 8).reduce((byte, bit) => (byte << 1) | bit, 0));
  }
  const blocks = [];
  for (let i = 0; i < blockCount; i++) {
    blocks.push(data.slice(i * blockLength, (i + 1) * blockLength));
  }
  const corrections = blocks.map(block => errorCorrection(block, ecLength));

  const codewords = [];
  for (let i = 0; i < blockLength; i++) {
    blocks.forEach(block => codewords.push(block[i]));
  }
  for (let i = 0; i < ecLength; i++) {
    corrections.forEach(correction => codewords.push(correction[i]));
  }
  return codewords;
}

function qrPenalty(modules) {
  const size = modules.length;
  let penalty = 0;
  let dark = 0;
  const lines = [...modules, ...modules.map((_, x) => modules.map(row => row[x]))];
  for (const line of lines) {
    let run = 1;
    for (let i = 1; i <= size; i++) {
      if (i < size && line[i] === line[i - 1]) {
        run++;
        continue;
      }
      if (run >= 5) {
        penalty += run - 2;
      }
      run = 1;
    }
    const text = line.map(Number).join("");
    penalty += 40 * ((text.match(/(?=10111010000)/g) || []).length + (text.match(/(?=00001011101)/g) || []).length);
  }
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      if (modules[y][x]) {
        dark++;
      }
      if (x < size - 1 && y < size - 1 && modules[y][x] === modules[y][x + 1] && modules[y][x] === modules[y + 1][x] && modules[y][x] === modules[y + 1][x + 1]) {
        penalty += 3;
      }
    }
  }
  return penalty + 10 * (Math.ceil(Math.abs(dark * 20 - size * size * 10) / (size * size)) - 1);
}

function applyMask(modules, reserved, condition) {
  modules.forEach((row, y) => row.forEach((isDark, x) => {
    if (!reserved[y][x] && condition(x, y)) {
      row[x] = !isDark;
    }
  }));
}

function qrCode(text) {
  const bytes = [...new TextEncoder().encode(text)];
  const version = QR_BLOCKS.findIndex(([, blockCount, blockLength]) => bytes.length <= blockCount * blockLength - 2) + 1;
  if (!version) {
    return "";
  }
  const size = version * 4 + 17;
  const modules = Array.from({ length: size }, () => new Array(size).fill(false));
  const reserved = Array.from({ length: size }, () => new Array(size).fill(false));
  const set = (x, y, isDark) => {
    modules[y][x] = isDark;
    reserved[y][x] = true;
  };
  const drawFormat = mask => {
    let remainder = mask;
    for (let i = 0; i < 10; i++) {
      remainder = (remainder << 1) ^ ((remainder >>> 9) * 0x537);
    }
    const bits = ((mask << 10) | remainder) ^ 0x5412;
    const bit = i => ((bits >>> i) & 1) === 1;
    for (let i = 0; i <= 5; i++) {
      set(8, i, bit(i));
    }
    set(8, 7, bit(6));
    set(8, 8, bit(7));
    set(7, 8, bit(8));
    for (let i = 9; i < 15; i++) {
      set(14 - i, 8, bit(i));
    }
    for (let i = 0; i < 8; i++) {
      set(size - 1 - i, 8, bit(i));
    }
    for (let i = 8; i < 15; i++) {
      set(8, size - 15 + i, bit(i));
    }
    set(8, size - 8, true);
  };

  for (let i = 0; i < size; i++) {
    set(6, i, i % 2 === 0);
    set(i, 6, i % 2 === 0);
  }
  for (const [cx, cy] of [[3, 3], [size - 4, 3], [3, size - 4]]) {
    for (let dy = -4; dy <= 4; dy++) {
      for (let dx = -4; dx <= 4; dx++) {
        const distance = Math.max(Math.abs(dx), Math.abs(dy));
        if (cx + dx >= 0 && cx + dx < size && cy + dy >= 0 && cy + dy < size) {
          set(cx + dx, cy + dy, distance !== 2 && distance !== 4);
        }
      }
    }
  }
  if (version > 1) {
    for (let dy = -2; dy <= 2; dy++) {
      for (let dx = -2; dx <= 2; dx++) {
        set(size - 7 + dx, size - 7 + dy, Math.max(Math.abs(dx), Math.abs(dy)) !== 1);
      }
    }
  }
  drawFormat(0);

  const codewords = qrCodewords(bytes, version);
  let index = 0;
  for (let right = size - 1; right >= 1; right -= 2) {
    if (right === 6) {
      right = 5;
    }
    for (let vertical = 0; vertical < size; vertical++) {
      for (let j = 0; j < 2; j++) {
        const x = right - j;
        const y = ((right + 1) & 2) === 0 ? size - 1 - vertical : vertical;
        if (!reserved[y][x] && index < codewords.length * 8) {
          modules[y][x] = ((codewords[index >>> 3] >>> (7 - (index & 7))) & 1) === 1;
          index++;
        }
      }
    }
  }

  let best = null;
  QR_MASKS.forEach((condition, mask) => {
    applyMask(modules, reserved, condition);
    drawFormat(mask);
    const penalty = qrPenalty(modules);
    if (!best || penalty < best.penalty) {
      best = { modules: modules.map(row => [...row]), penalty };
    }
    applyMask(modules, reserved, condition);
  });

  let path = "";
  best.modules.forEach((row, y) => row.forEach((isDark, x) => {
    if (isDark) {
      path += `M${x + 4} ${y + 4}h1v1h-1z`;
    }
  }));
  return `<svg viewBox="0 0 ${size + 8} ${size + 8}" shape-rendering="crispEdges" aria-hidden="true"><rect width="100%" height="100%" fill="#fff"/><path d="${path}" fill="#000"/></svg>`;
}

function signature(data) {
  return JSON.stringify([data.installed, data.job?.kind, data.job?.state, data.status?.backend_state, data.status?.auth_url, data.status?.dns_name, data.status?.hostname,
    data.status?.magic_dns, data.status?.peers_online]);
}

async function track(kind) {
  tracking = true;
  const install = kind === "install";
  const handle = P.job({ label: install ? "Installing Tailscale" : "Uninstalling Tailscale", progress: install ? 0 : null });
  let job;
  try {
    job = await P.poll(async () => {
      const data = await P.get("/api/tailscale");
      if (data.job?.state !== "running") {
        return data.job;
      }
      if (install && data.job.total) {
        handle.update(`${P.size(data.job.downloaded)} of ${P.size(data.job.total)}`, data.job.downloaded / data.job.total);
      }
    });
  } catch (error) {
    tracking = false;
    handle.fail(error, install ? "Couldn't install Tailscale" : "Couldn't uninstall Tailscale");
    return;
  }
  tracking = false;
  if (job?.state === "done") {
    handle.done(install ? "Tailscale installed" : "Tailscale removed", install ? "Sign in to add the device to your tailnet." : "");
  } else {
    handle.finish();
    P.toast(install ? "Couldn't install Tailscale" : "Couldn't uninstall Tailscale", "The job failed on the device", "alert");
  }
  if (location.pathname === "/tailscale") {
    P.refresh();
  }
}

function statusBody(ts, signingIn, port) {
  if (!ts) {
    return `<p class="muted">Tailscale isn't running on the device right now. This page updates when it starts.</p>`;
  }
  if (ts.backend_state === "Running") {
    const hosts = [...(ts.magic_dns && ts.dns_name ? [ts.dns_name.split(".")[0], ts.dns_name] : []), ...ts.ips.slice(0, 1)];
    return `<dl class="kv"><dt>Other devices online</dt><dd>${ts.peers_online}</dd></dl>
      <h3 class="field-label ts-open">Open The Pond</h3>
      <ul class="ts-links">${hosts.map(host => `<li><a class="link mono" href="http://${P.esc(host)}:${port}" target="_blank" rel="noopener noreferrer">http://${P.esc(host)}:${port}</a></li>`).join("")}</ul>
      <p class="muted small-print">${ts.magic_dns ? "Open these on a phone or computer signed in to Tailscale with the same account. If the first one doesn't open, try the next." : "MagicDNS is off in your tailnet, so only the IP address works. Turn it on in the Tailscale admin console's DNS page for an address with this device's name."}</p>`;
  }
  if (signingIn && ts.auth_url) {
    const qr = qrCode(ts.auth_url);
    return `<div class="ts-signin">${qr ? `<span class="ts-qr" role="img" aria-label="QR code for the Tailscale sign-in page">${qr}</span>` : ""}
      <div><p>Open the sign-in page${qr ? ", or scan the code with your phone," : ""} and add this device to your tailnet. This page updates once you're signed in.</p>
      <p class="mt-12"><a class="link" href="${P.esc(ts.auth_url)}" target="_blank" rel="noopener noreferrer">${P.ic("link")}Sign in to Tailscale</a></p></div></div>`;
  }
  if (signingIn) {
    return `<p class="muted">Getting a sign-in link…</p>`;
  }
  if (ts.backend_state === "NeedsMachineAuth") {
    return `<p>Approve this device in your Tailscale admin console.</p>`;
  }
  return `<p>Sign in to add this device to your tailnet, then open The Pond from any device on it.</p>`;
}

P.page("tailscale", {
  title: "Tailscale",
  async render() {
    const data = await P.get("/api/tailscale");
    const busy = data.job?.state === "running";
    const mount = root => {
      if (busy && data.job.kind !== "login" && !tracking) {
        track(data.job.kind);
      }
      let stopped = false;
      P.poll(async () => {
        if (stopped) {
          return true;
        }
        if (document.hidden) {
          return;
        }
        const next = await P.get("/api/tailscale");
        if (stopped || signature(next) === signature(data)) {
          return;
        }
        if (next.status?.backend_state === "Running" && data.status?.backend_state !== "Running") {
          P.toast("Tailscale connected");
        }
        P.refresh();
        return true;
      }, 3000).catch(error => {
        if (!stopped) {
          P.fail(error, "Couldn't refresh the Tailscale status");
        }
      });
      return () => {
        stopped = true;
      };
    };
    if (!data.installed) {
      return {
        body: `<section class="panel"><div class="empty"><h3>Tailscale isn't installed</h3><p>Open The Pond away from the car's Wi-Fi. Install Tailscale, then sign in with the link it gives you.</p><button class="btn btn-primary btn-small" data-act="tailscale-install" ${busy ? "disabled" : ""}>${P.ic("download")}Install Tailscale</button>
          <button class="btn btn-text btn-small" data-act="tailscale-guide">${P.ic("info")}How to open The Pond over Tailscale</button></div></section>`,
        mount
      };
    }
    const ts = data.status;
    const running = ts?.backend_state === "Running";
    const signingIn = !running && busy && data.job.kind === "login";
    const label = signingIn ? "Waiting for sign-in" : ts ? STATES[ts.backend_state] || ts.backend_state : "Not running";
    const showGuide = !running || !ts.peers_online;
    const foot = running ? `<button class="btn btn-quiet btn-small" data-act="tailscale-logout">Sign out…</button>`
      : ts && !signingIn ? `<button class="btn btn-quiet btn-small" data-act="tailscale-login">${ts.backend_state === "Stopped" ? "Connect" : "Sign in"}</button>` : "";
    const uninstall = `<button class="btn btn-danger-quiet btn-small" data-act="tailscale-uninstall" ${busy && !signingIn ? "disabled" : ""}>Uninstall Tailscale…</button>`;
    const guideButton = showGuide ? "" : `<button class="btn btn-text btn-small ts-guide" data-act="tailscale-guide">${P.ic("info")}How to connect a phone or computer</button>`;
    const name = running ? `<section class="panel"><div class="panel-head"><h2>Device name</h2></div>
      <div class="panel-body"><form class="form-grid" data-ts-name>
        <p class="muted">The name in this device's short address. Use letters, numbers, and dashes. Signing out of Tailscale sets it back to the default.</p>
        <label class="field"><span>Name</span><input class="input mono" name="hostname" value="${P.esc(ts.hostname || "")}" autocomplete="off" autocapitalize="off" spellcheck="false"></label>
        <div><button class="btn btn-primary btn-small" type="submit">Save name</button></div>
      </form></div></section>` : "";
    return {
      body: `
      <div class="two-col">
        <div class="stack">
          <section class="panel"><div class="panel-head"><h2>Status</h2><span class="aside state-line"><span class="dot ${running ? "" : ts ? "idle" : "off"}"></span>${P.esc(label)}</span></div>
            <div class="panel-body">${statusBody(ts, signingIn, data.port)}${guideButton}</div>
            <div class="panel-foot">${foot}${uninstall}</div></section>
          ${showGuide ? name : ""}
        </div>
        <div class="stack">
          ${showGuide ? `<section class="panel"><div class="panel-head"><h2>Open The Pond from your phone or computer</h2></div><div class="panel-body ts-guide-body">${guideBody()}</div></section>` : name}
        </div>
      </div>`,
      mount: root => {
        root.querySelector("[data-ts-name]")?.addEventListener("submit", async e => {
          e.preventDefault();
          const form = e.currentTarget;
          try {
            await P.busy(form.querySelector("[type=submit]"), P.request("PUT", "/api/tailscale/hostname", { hostname: form.hostname.value.trim() }));
          } catch (error) {
            P.fail(error, "Couldn't change the device name", MESSAGES);
            return;
          }
          P.toast("Device name saved", "The address updates once Tailscale picks up the new name.");
        });
        return mount(root);
      }
    };
  }
});

P.act("tailscale-guide", () => {
  P.dialog({ icon: "tailscale", wide: true, title: "Open The Pond over Tailscale", body: guideBody(), confirm: "Got it", cancel: "" });
});

P.act("tailscale-install", async b => {
  try {
    await P.busy(b, P.request("POST", "/api/tailscale"));
  } catch (error) {
    P.fail(error, "Couldn't install Tailscale");
    return;
  }
  track("install");
});

P.act("tailscale-login", async b => {
  if (!await P.pinWarning("tailscale", "Signing in to Tailscale from The Pond lets that Tailscale account reach the device from anywhere.")) {
    return;
  }

  try {
    await P.busy(b, P.request("POST", "/api/tailscale/login"));
  } catch (error) {
    P.fail(error, "Couldn't start the Tailscale sign-in");
    return;
  }
  P.refresh();
});

P.act("tailscale-logout", async () => {
  const ok = await P.dialog({ icon: "tailscale", tone: "danger", confirmTone: "danger", title: "Sign out of Tailscale?", body: `<p>The device drops off your tailnet right away. Anyone using The Pond over Tailscale loses the connection until you sign in again.</p>`, confirm: "Sign out" });
  if (!ok) {
    return;
  }
  try {
    await P.request("POST", "/api/tailscale/logout");
  } catch (error) {
    P.fail(error, "Couldn't sign out of Tailscale");
    return;
  }
  P.toast("Signed out of Tailscale");
  P.refresh();
});

P.act("tailscale-uninstall", async () => {
  const ok = await P.dialog({ icon: "tailscale", tone: "danger", confirmTone: "danger", title: "Uninstall Tailscale?", body: `<p>The device drops off your tailnet right away. Anyone using The Pond over Tailscale loses the connection.</p>${P.lossList(["Tailscale and its sign-in on this device"], ["The Pond on your local Wi-Fi", "Everything else on the device"])}`, confirm: "Uninstall Tailscale" });
  if (!ok) {
    return;
  }
  try {
    await P.request("DELETE", "/api/tailscale");
  } catch (error) {
    P.fail(error, "Couldn't uninstall Tailscale");
    return;
  }
  track("uninstall");
});
})();
