(() => {
"use strict";

const P = window.Pond;

const FROGPILOT_STATS = [
  ["AEBEvents", "Collision alerts"],
  ["Disengages", "Disengagements"],
  ["CurrentMonthsMeters", "Distance this month"],
  ["CruiseSpeedTimes", "Favorite set speed"],
  ["MaxAcceleration", "Highest acceleration"],
  ["LongestDistanceWithoutOverride", "Longest without an override"],
  ["ModelTimes", "Most-used model"],
  ["PersonalityTimes", "Most-used personality"],
  ["Overrides", "Overrides"],
  ["ExperimentalModeTime", "Time in Experimental Mode"],
  ["LongitudinalTime", "Time openpilot controlled speed"],
  ["LateralTime", "Time openpilot steered"],
  ["AOLTime", "Time with Always On Lateral"]
];
const STORAGE_WARNING = 80;

function statValue(stats, key) {
  const { type, value } = stats[key];
  if (type === "accel") {
    return `${value.toFixed(2)}${P.NB}m/s²`;
  }
  if (type === "count") {
    return value.toLocaleString("en-US");
  }
  if (type === "distance") {
    return P.distance(value);
  }
  if (type === "timePercent") {
    return `${stats.TrackedTime.value ? Math.round((value / stats.TrackedTime.value) * 100) : 0}%`;
  }
  if (type === "speed") {
    if (!value) {
      return "None yet";
    }
    return P.speed(value.speed);
  }
  const [top] = Object.entries(value).sort((a, b) => b[1] - a[1]);
  if (!top) {
    return "None yet";
  }
  return P.esc(key === "ModelTimes" ? top[0].replace("_default", "").replace("(Default)", "").trim() : top[0]);
}

function storageHTML(storage) {
  const total = storage.free_space + storage.used_space;
  const used = total ? (storage.used_space / total) * 100 : 0;
  const deleting = storage.deletion_threshold;
  const warning = Math.min(STORAGE_WARNING, deleting);
  return `<p class="figure num">${storage.free_space}${P.NB}GB <span>free</span></p>
    <div class="progress" role="img" aria-label="${storage.used_space} of ${total} GB used. The oldest drives are deleted automatically above ${Math.round(deleting)}% used"><i class="storage-used" data-w="${used}"><span class="storage-zones" data-w="${used ? 10000 / used : 100}"><span data-w="${warning}"></span><span data-w="${deleting - warning}"></span><span data-w="${100 - deleting}"></span></span></i></div>
    <p class="meta">${storage.used_space} of ${total}${P.NB}GB used</p>
    ${used >= deleting ? `<p class="meta">Oldest drives are deleted automatically to make room</p>` : ""}`;
}

function softwareFigure(sw) {
  if (sw.update_available) {
    return "Update ready";
  }
  if (sw.fetch_available) {
    return "Update available";
  }
  return "Up to date";
}

function updateAction(sw) {
  if (sw.update_available) {
    return `<button class="btn btn-primary btn-small" data-act="install-update">${P.ic("software")}<span>Install update</span></button>`;
  }
  if (sw.fetch_available) {
    return `<button class="btn btn-primary btn-small" data-act="download-update">${P.ic("download")}<span>Download update</span></button>`;
  }
  return "";
}

P.act("home-reset-stats", async () => {
  const ok = await P.dialog({ icon: "refresh", tone: "danger", confirmTone: "danger", title: "Are you sure you want to reset all of your FrogPilot stats?", body: "<p>Every FrogPilot stat goes back to zero, including the FrogPilot row under Driving.</p>", confirm: "Reset" });
  if (!ok) {
    return;
  }
  try {
    await P.request("DELETE", "/api/frogpilot_stats");
  } catch (error) {
    P.fail(error, "Couldn't reset the stats", { car_on: "Stats can't be reset while the car is on. Turn the car off and try again." });
    return;
  }
  P.toast("FrogPilot stats reset");
  P.refresh();
});

P.page("home", {
  title: "Home",
  async render() {
    const [home, doors, shell] = await Promise.all([P.get("/api/home"), P.get("/api/doors"), P.shell()]);
    const ds = home.drive_stats;
    const sw = shell.software;
    const v = shell.vehicle;
    const last = home.last_drive;
    const repo = home.software.repository;
    const statRow = (label, s, cls = "") => `<tr class="${cls}"><th scope="row">${label}</th><td>${s.drives.toLocaleString("en-US")}</td><td>${Math.round(s.distance).toLocaleString("en-US")}</td><td>${Math.round(s.hours).toLocaleString("en-US")}</td></tr>`;
    const doorText = doors.available && doors.lock_status !== "unavailable" ? ` · Doors ${doors.lock_status}` : "";

    return {
      actions: updateAction(sw),
      body: `
      <div class="home">
        <section class="block home-stats" aria-labelledby="stats-h">
          <h2 id="stats-h">Driving</h2>
          <table class="stats">
            <thead><tr><th scope="col"><span class="sr">Period</span></th><th scope="col">Drives</th><th scope="col">${home.is_metric ? "Kilometers" : "Miles"}</th><th scope="col">Hours</th></tr></thead>
            <tbody>${statRow("All time", ds.all)}${statRow("Past week", ds.week)}${statRow("FrogPilot", ds.frogpilot, "frogpilot")}</tbody>
          </table>
        </section>
        ${last ? `<section class="block home-last" aria-labelledby="last-h">
          <h2 id="last-h">Last drive</h2>
          <a class="last" href="/drive?route=${encodeURIComponent(last.route)}" ${last.video ? `data-preview="/api/routes/${encodeURIComponent(last.route)}/preview"` : ""}>
            <span class="thumb" data-route="${P.esc(last.route)}">${P.clips.thumbHTML(last.video ? `/api/routes/${encodeURIComponent(last.route)}/thumbnail` : "")}</span>
            <span><b>${P.esc(P.clips.itemTitle(last))}</b><span class="meta" data-last-meta>${last.name ? `${P.whenLabel(last.date)} · ` : ""}${P.duration(last.segments * 60)}</span></span>
          </a>
        </section>` : ""}
        <section class="block home-storage" aria-labelledby="store-h">
          <h2 id="store-h">Storage</h2>
          ${storageHTML(home.storage)}
        </section>
        ${doors.available ? `<section class="block" aria-labelledby="car-h">
          <h2 id="car-h">Car</h2>
          <p class="meta">${P.esc(v.model)}${doorText}</p>
          ${P.doorButtons(doors)}
        </section>` : ""}
        <section class="block home-software" aria-labelledby="sw-h">
          <h2 id="sw-h">Software</h2>
          <p class="figure">${softwareFigure(sw)}</p>
          <p class="meta mono">${P.esc(sw.update_available ? sw.new_description : sw.current_description)}</p>
          <div class="home-software-foot">
            ${repo ? `<p class="meta">Maintained by <a href="${P.esc(repo.url)}" target="_blank" rel="noopener">${P.esc(repo.owner)}</a></p>` : ""}
            <a class="link" href="/software">${sw.update_available || sw.fetch_available ? "What's new" : "Software Updates"}</a>
          </div>
        </section>
        <section class="block home-fpstats" aria-labelledby="fpstats-h">
          <h2 id="fpstats-h">FrogPilot stats</h2>
          <dl class="fp-stats">${FROGPILOT_STATS.map(([key, label]) => `<div><dt>${label}</dt><dd>${statValue(home.frogpilot_stats, key)}</dd></div>`).join("")}</dl>
          <button class="btn btn-quiet btn-small" data-act="home-reset-stats">Reset stats</button>
        </section>
      </div>`,
      mount: root => {
        if (!last) {
          return null;
        }
        const watching = new AbortController();
        P.clips.watchPreviews(root, watching.signal);
        P.clips.watchThumbs(root, watching.signal);
        let active = true;
        const thumb = root.querySelector(".last .thumb");
        P.get(`/api/routes/${encodeURIComponent(last.route)}/drive`).then(drive => {
          if (!active) {
            return;
          }
          root.querySelector("[data-last-meta]").textContent = `${last.name ? `${P.whenLabel(last.date)} · ` : ""}${P.distance(drive.distance)} · ${P.duration(last.segments * 60)}`;
          if (!last.video) {
            P.clips.drawRoute(thumb, drive.path);
          }
        }).catch(error => {
          if (!(error instanceof P.PondError) || error.status !== 404) {
            P.fail(error, "Couldn't load the last drive's distance");
          }
          if (active && !last.video) {
            P.clips.drawRoute(thumb, []);
          }
        });
        return () => {
          active = false;
          watching.abort();
        };
      }
    };
  }
});
})();
