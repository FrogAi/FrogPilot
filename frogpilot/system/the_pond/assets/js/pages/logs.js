(() => {
"use strict";

const P = window.Pond;

const CONSOLE_ERRORS = { invalid_key: "That key can't be sent to the console", invalid_text: "Commands can be up to 1000 characters, with no tabs or line breaks" };
const CONSOLE_KEYS = [["Esc", "Escape"], ["Tab", "Tab"], ["Ctrl", "Ctrl"], ["↑", "Up"], ["↓", "Down"], ["←", "Left"], ["→", "Right"]];
const CONSOLE_SESSIONS = [["openpilot", "openpilot"], ["shell", "Shell"]];
const FILE_PAGE = 50;
const FILE_PANELS = {
  error: { empty: `<div class="empty"><h3>No error logs</h3><p>Nothing has gone wrong lately.</p></div>`, title: "Error logs" },
  tmux: { empty: `<div class="empty"><h3>No saved console logs</h3><p>Save a copy to keep what the console shows right now.</p></div>`, title: "Saved console logs" }
};
const KEYBOARD_KEYS = { ArrowDown: "Down", ArrowUp: "Up" };
const NAME_ERRORS = { invalid_name: "Use up to 64 letters, numbers, dots, dashes, and underscores" };
const PATHS = { error: "/api/error_logs", tmux: "/api/tmux_logs" };

const fileLists = {};

let consoleSession = "openpilot";
let sending = Promise.resolve();

function sendConsole(body) {
  const session = consoleSession;
  const sent = sending.then(() => {
    return P.request("POST", "/api/tmux", { ...body, session });
  });
  sending = sent.catch(() => {});
  return sent.then(() => true, error => {
    P.fail(error, "Couldn't send to the console", CONSOLE_ERRORS);
    return false;
  });
}

function closeShell() {
  P.request("DELETE", "/api/tmux/shell").catch(error => {
    P.fail(error, "Couldn't close the shell");
  });
}

function consoleSessionButtons() {
  let buttons = "";
  for (const [session, label] of CONSOLE_SESSIONS) {
    buttons += `<button type="button" data-session="${session}" aria-pressed="${session === "openpilot"}">${label}</button>`;
  }

  return `<div class="seg" role="group" aria-label="Console">${buttons}</div>`;
}

function fileURL(button) {
  return `${PATHS[button.dataset.kind]}/${encodeURIComponent(button.dataset.name)}`;
}

function fileRows(kind, files) {
  return files.map(f => {
    const attrs = `data-kind="${kind}" data-name="${P.esc(f.name)}"`;
    return `<div class="list-row"><span class="grow"><b class="mono">${P.esc(f.name)}</b><span>${P.whenLabel(f.modified)} · ${P.size(f.size)}</span></span><span class="acts">
      <button class="btn btn-quiet btn-small" data-act="logs-view" ${attrs}>View</button>
      ${kind === "tmux" ? `<button class="icon-btn" data-act="logs-rename" ${attrs} aria-label="Rename ${P.esc(f.name)}">${P.ic("pencil")}</button>` : ""}
      <button class="icon-btn" data-act="logs-download" ${attrs} aria-label="Download ${P.esc(f.name)}">${P.ic("download")}</button>
      <button class="icon-btn danger" data-act="logs-delete" ${attrs} aria-label="Delete ${P.esc(f.name)}">${P.ic("trash")}</button>
    </span></div>`;
  }).join("");
}

function moreButton(kind, shown) {
  const left = fileLists[kind].length - shown;
  return left > 0 ? `<button class="btn btn-quiet btn-small logs-more" data-act="logs-more" data-kind="${kind}">Show ${Math.min(left, FILE_PAGE)} more</button>` : "";
}

function filesPanel(kind, files) {
  fileLists[kind] = files;
  return `<section class="panel" data-files="${kind}"><div class="panel-head"><h2>${FILE_PANELS[kind].title}</h2><span class="aside muted">${P.plural(files.length, "file")}</span>${files.length ? `<button class="btn btn-danger-quiet btn-small" data-act="logs-delete-all" data-kind="${kind}">Delete all…</button>` : ""}</div>
    <div class="logs-list">${fileRows(kind, files.slice(0, FILE_PAGE)) || FILE_PANELS[kind].empty}${moreButton(kind, FILE_PAGE)}</div>
    <div data-log-view></div>
  </section>`;
}

async function reloadFiles(kind) {
  let files;
  try {
    files = await P.get(PATHS[kind]);
  } catch (error) {
    P.fail(error, "Couldn't update the list");
    return;
  }
  const panel = document.querySelector(`[data-files="${kind}"]`);
  if (panel) {
    panel.outerHTML = filesPanel(kind, files);
  }
}

function setConsoleState(panel, dot, label) {
  panel.querySelector("[data-console-state]").innerHTML = `<span class="dot ${dot}"></span>${label}`;
}

function setToggle(button, pressed, icon, label) {
  button.setAttribute("aria-pressed", String(pressed));
  button.innerHTML = `${P.ic(icon)}<span>${label}</span>`;
}

P.page("logs", {
  title: "Error Logs and Console",
  async render() {
    consoleSession = "openpilot";

    const [errorLogs, tmuxLogs, pane] = await Promise.all([
      P.get("/api/error_logs"),
      P.get("/api/tmux_logs"),
      P.get("/api/tmux?session=openpilot")
    ]);

    return {
      body: `
      <div class="two-col">
        ${filesPanel("error", errorLogs)}
        <div class="stack">
          <section class="panel logs-console"><div class="panel-head"><h2>Live console</h2><span class="aside state-line" data-console-state><span class="dot"></span>Live</span></div>
            <div class="logs-term">
              <div class="logs-term-bar">
                ${consoleSessionButtons()}
                <button type="button" class="btn btn-small logs-term-btn" data-act="logs-pause" aria-pressed="false">${P.ic("pause")}<span>Pause</span></button>
                <button type="button" class="btn btn-small logs-term-btn" data-act="logs-expand" aria-pressed="false">${P.ic("full")}<span>Expand</span></button>
                <button type="button" class="btn btn-small logs-term-btn logs-save" data-act="logs-save">${P.ic("file")}<span>Save a copy</span></button>
              </div>
              <div class="console" tabindex="0" aria-label="Console output" aria-live="off" data-console>${P.esc(pane.lines.join("\n"))}</div>
              <div class="keybar" aria-label="Keys">${CONSOLE_KEYS.map(([label, key]) => `<button type="button" data-act="logs-key" data-key="${key}"${key === "Ctrl" ? ` aria-pressed="false"` : ""}>${label}</button>`).join("")}</div>
              <form class="console-input" data-console-input><label class="sr" for="console-in">Command</label><input class="input" id="console-in" autocomplete="off" autocapitalize="off" spellcheck="false" placeholder="Type a command"><button class="btn btn-lily btn-small" type="submit">Run</button></form>
            </div>
          </section>
          ${filesPanel("tmux", tmuxLogs)}
        </div>
      </div>`,
      mount: root => {
        const panel = root.querySelector(".logs-console");
        const con = root.querySelector("[data-console]");
        const pause = root.querySelector("[data-act='logs-pause']");
        let following = true;
        let id = pane.id;
        let lines = pane.lines;
        con.scrollTop = con.scrollHeight;
        root.querySelector("[aria-label='Console']").addEventListener("click", async e => {
          const button = e.target.closest("[data-session]");
          if (!button || button.dataset.session === consoleSession) {
            return;
          }

          if (button.dataset.session === "shell") {
            const warning = "The shell in The Pond can run any command on the device, including ones that change or erase its software.";
            if (!await P.pinWarning("shell", warning)) {
              return;
            }
          }

          if (consoleSession === "shell") {
            closeShell();
          }

          consoleSession = button.dataset.session;
          for (const option of button.parentElement.children) {
            option.setAttribute("aria-pressed", String(option === button));
          }

          id = "";
          lines = [];
          con.textContent = "";
          following = true;
        });
        con.addEventListener("scroll", () => {
          following = con.scrollHeight - con.scrollTop - con.clientHeight < 24;
        });
        new ResizeObserver(() => {
          if (following) {
            con.scrollTop = con.scrollHeight;
          }
        }).observe(con);
        const ctrl = root.querySelector("[data-key='Ctrl']");
        const form = root.querySelector("[data-console-input]");
        const input = form.querySelector("input");
        root.querySelector(".keybar").addEventListener("mousedown", e => e.preventDefault());
        form.addEventListener("submit", async e => {
          e.preventDefault();
          const text = input.value;
          if (await sendConsole(text ? { text } : { key: "Enter" }) && input.value === text) {
            input.value = "";
          }
        });
        input.addEventListener("input", e => {
          if (ctrl.getAttribute("aria-pressed") !== "true" || !/^[a-z]$/i.test(e.data || "")) {
            return;
          }
          const caret = input.selectionStart - 1;
          input.value = input.value.slice(0, caret) + input.value.slice(caret + 1);
          input.setSelectionRange(caret, caret);
          ctrl.setAttribute("aria-pressed", "false");
          sendConsole({ key: `C-${e.data.toLowerCase()}` });
        });
        input.addEventListener("keydown", e => {
          if (e.ctrlKey && e.key === "c" && input.selectionStart === input.selectionEnd) {
            e.preventDefault();
            sendConsole({ key: "C-c" });
          } else if (!input.value && KEYBOARD_KEYS[e.key]) {
            e.preventDefault();
            sendConsole({ key: KEYBOARD_KEYS[e.key] });
          }
        });
        let stopped = false;
        P.poll(async () => {
          if (stopped) {
            return true;
          }
          if (document.hidden || pause.getAttribute("aria-pressed") === "true") {
            return;
          }
          const session = consoleSession;
          const update = await P.get(`/api/tmux?session=${session}&since=${id}`);
          if (stopped || !update || session !== consoleSession || pause.getAttribute("aria-pressed") === "true") {
            return;
          }
          id = update.id;
          lines = lines.slice(0, update.keep).concat(update.lines);
          con.textContent = lines.join("\n");
          if (following) {
            con.scrollTop = con.scrollHeight;
          }
        }).catch(error => {
          if (stopped) {
            return;
          }
          pause.disabled = true;
          setConsoleState(panel, "off", "Stopped");
          P.fail(error, "The console stopped updating");
        });
        return () => {
          stopped = true;
          if (consoleSession === "shell") {
            closeShell();
          }
        };
      }
    };
  }
});

P.act("logs-view", async b => {
  const view = b.closest(".panel").querySelector("[data-log-view]");
  let text;
  try {
    text = await P.get(fileURL(b));
  } catch (error) {
    P.fail(error, `Couldn't open ${P.fileLabel(b.dataset.name)}`);
    return;
  }
  view.innerHTML = `<div class="panel-body logs-view"><div class="logs-view-head"><b class="mono">${P.esc(b.dataset.name)}</b><button class="icon-btn" aria-label="Close log" data-act="logs-close">${P.ic("x")}</button></div><pre class="code-box">${P.esc(text)}</pre></div>`;
});

P.act("logs-close", b => {
  b.closest("[data-log-view]").innerHTML = "";
});

P.act("logs-more", b => {
  const list = b.parentElement;
  const shown = list.querySelectorAll(".list-row").length;
  b.outerHTML = fileRows(b.dataset.kind, fileLists[b.dataset.kind].slice(shown, shown + FILE_PAGE)) + moreButton(b.dataset.kind, shown + FILE_PAGE);
  list.querySelectorAll(".list-row")[shown].querySelector("button").focus();
});

P.act("logs-download", b => P.download(`${fileURL(b)}?download=1`, b.dataset.name));

P.act("logs-delete", async b => {
  const crashLog = b.dataset.kind === "error" && b.dataset.name === "error.txt";
  const ok = await P.dialog({ icon: "trash", tone: "danger", confirmTone: "danger", title: "Delete this log?", body: `<p><span class="mono">${P.esc(b.dataset.name)}</span> is removed.${crashLog ? " Reports sent after this won't include crash details until a new crash happens." : ""}</p>`, confirm: "Delete log" });
  if (!ok) {
    return;
  }
  try {
    await P.request("DELETE", fileURL(b));
  } catch (error) {
    P.fail(error, "Couldn't delete the log");
    return;
  }
  P.toast("Log deleted");
  reloadFiles(b.dataset.kind);
});

P.act("logs-delete-all", async b => {
  const errorLogs = b.dataset.kind === "error";
  const ok = await P.dialog({ icon: "trash", tone: "danger", confirmTone: "danger", title: errorLogs ? "Delete all error logs?" : "Delete all saved console logs?", body: `<p>${errorLogs ? "Every error log is removed. Reports sent after this won't include crash details until a new crash happens." : "Every saved console log is removed."}</p>`, confirm: "Delete all" });
  if (!ok) {
    return;
  }
  try {
    await P.request("DELETE", PATHS[b.dataset.kind]);
  } catch (error) {
    P.fail(error, "Couldn't delete the logs");
    return;
  }
  P.toast("Logs deleted");
  reloadFiles(b.dataset.kind);
});

P.act("logs-key", b => {
  if (b.dataset.key === "Ctrl") {
    b.setAttribute("aria-pressed", String(b.getAttribute("aria-pressed") !== "true"));
    return;
  }
  sendConsole({ key: b.dataset.key });
});

P.act("logs-expand", b => {
  const expanded = b.getAttribute("aria-pressed") !== "true";
  b.closest(".logs-console").classList.toggle("expanded", expanded);
  setToggle(b, expanded, "full", expanded ? "Collapse" : "Expand");
});

P.act("logs-pause", b => {
  const paused = b.getAttribute("aria-pressed") !== "true";
  setToggle(b, paused, paused ? "play" : "pause", paused ? "Resume" : "Pause");
  setConsoleState(b.closest(".logs-console"), paused ? "idle" : "", paused ? "Paused" : "Live");
});

P.act("logs-rename", async b => {
  const current = b.dataset.name.replace(/\.log$/, "");
  const dlg = await P.dialog({ icon: "pencil", title: "Rename this log", body: `<label class="field"><span>Name</span><input class="input mono" maxlength="64" autocomplete="off" spellcheck="false" value="${P.esc(current)}"></label>`, confirm: "Rename" });
  if (!dlg) {
    return;
  }
  const name = dlg.querySelector("input").value.trim();
  if (!name || name === current) {
    return;
  }
  try {
    await P.request("PUT", fileURL(b), { name });
  } catch (error) {
    P.fail(error, "Couldn't rename the log", NAME_ERRORS);
    return;
  }
  P.toast("Log renamed", `<span class="mono">${P.fileLabel(name)}.log</span>`);
  reloadFiles("tmux");
});

P.act("logs-save", async b => {
  b.disabled = true;
  let saved;
  try {
    saved = await P.request("POST", "/api/tmux_logs", { session: consoleSession });
  } catch (error) {
    b.disabled = false;
    P.fail(error, "Couldn't save the console");
    return;
  }
  P.toast("Console saved", `<span class="mono">${P.fileLabel(saved.name)}</span>`);
  await reloadFiles("tmux");
  b.disabled = false;
});
})();
