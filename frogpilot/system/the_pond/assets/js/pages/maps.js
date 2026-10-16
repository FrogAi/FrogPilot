(() => {
"use strict";

const P = window.Pond;

const MESSAGES = {
  invalid_regions: "One of those regions isn't available",
  invalid_schedule: "That schedule isn't available",
  no_maps_selected: "Choose at least one region first"
};
const SCHEDULES = [["0", "Manually, when I press Download"], ["1", "Weekly, on Sundays"], ["2", "Monthly, on the 1st"]];

let following = false;
let saving = Promise.resolve();

function isBusy(download) {
  return download.downloading || download.active;
}

async function followDownload(requestTime) {
  if (following) {
    return;
  }
  following = true;
  let cancelled = false;
  const handle = P.job({
    label: "Downloading speed limit maps",
    detail: "Starting…",
    cancel: async () => {
      cancelled = true;
      try {
        await P.request("POST", "/api/maps/cancel");
      } catch (error) {
        P.fail(error, "Couldn't cancel the download");
      }
    }
  });
  let result;
  try {
    result = await P.poll(async () => {
      const download = await P.get("/api/maps/download");
      if (download.active) {
        handle.update(`${download.downloaded_files.toLocaleString("en-US")} of ${P.plural(download.total_files, "file")}`, download.total_files ? download.downloaded_files / download.total_files : null);
        return undefined;
      }
      if (download.request_time < requestTime || download.downloading) {
        return undefined;
      }
      return download;
    });
  } catch (error) {
    following = false;
    handle.fail(error, "The map download stopped");
    return;
  }
  following = false;
  if (cancelled || result.cancelled) {
    handle.done("Map download canceled");
  } else if (result.total_files > 0 && result.downloaded_files === result.total_files) {
    handle.done("Maps downloaded", P.plural(result.total_files, "file"));
  } else {
    handle.finish();
    P.toast("Map download failed", `${result.downloaded_files.toLocaleString("en-US")} of ${P.plural(result.total_files, "file")} downloaded. Try again.`, "alert");
  }
  if (location.pathname === "/maps") {
    P.refresh();
  }
}

function filterRegions(root, query) {
  const text = query.trim().toLowerCase();
  const matches = {};
  root.querySelectorAll("[data-regions]").forEach(list => {
    let shown = 0;
    list.querySelectorAll("[data-group]").forEach(group => {
      let groupShown = 0;
      group.querySelectorAll(".check").forEach(label => {
        label.hidden = !label.textContent.toLowerCase().includes(text);
        if (!label.hidden) {
          groupShown++;
        }
      });
      group.hidden = groupShown === 0;
      shown += groupShown;
    });
    list.querySelector("[data-empty]").hidden = shown > 0;
    matches[list.dataset.regions] = shown;
  });
  const current = root.querySelector("[data-regions]:not([hidden])").dataset.regions;
  const other = current === "states" ? "countries" : "states";
  if (!matches[current] && matches[other]) {
    showRegions(other);
  }
}

function showRegions(show) {
  document.querySelectorAll("[data-act='maps-show']").forEach(b => b.setAttribute("aria-pressed", String(b.dataset.show === show)));
  document.querySelectorAll("[data-regions]").forEach(list => {
    list.hidden = list.dataset.regions !== show;
  });
  P.setParam("show", show === "states" ? "" : show);
}

function regionGroups(groups, prefix, selected, label) {
  return `${Object.entries(groups).map(([group, regions]) => `<div data-group>
    <h3 class="field-label maps-group">${P.esc(group)}</h3>
    <div class="region-grid">${Object.entries(regions).map(([code, name]) => `<label class="check"><input type="checkbox" data-region="${P.esc(prefix + code)}" ${selected.has(prefix + code) ? "checked" : ""}><span>${P.esc(name)}</span></label>`).join("")}</div></div>`).join("")}
    <p class="muted maps-empty" data-empty hidden>No ${label} match your search.</p>`;
}

P.act("maps-show", button => showRegions(button.dataset.show));

P.act("maps-download", async button => {
  let result;
  try {
    result = await P.busy(button, saving.then(() => P.request("POST", "/api/maps/download")));
  } catch (error) {
    P.fail(error, "Couldn't start the map download", MESSAGES);
    return;
  }
  if (location.pathname === "/maps") {
    P.refresh();
  }
  followDownload(result.request_time);
});

P.act("maps-remove", async button => {
  const ok = await P.dialog({
    icon: "trash", tone: "danger", confirmTone: "danger", title: "Remove speed limit maps?",
    body: `<p>Only the map data around where you park comes back on its own, the next time the device is on Wi-Fi. Everywhere else has no map speed limits until you choose your regions again and download them.</p>
      ${P.lossList(["Downloaded map data", "Your chosen regions"], ["Your keys and favorite places"])}`,
    confirm: "Remove maps"
  });
  if (!ok) {
    return;
  }
  try {
    await P.busy(button, P.request("DELETE", "/api/maps"));
  } catch (error) {
    P.fail(error, "Couldn't remove the maps");
    return;
  }
  P.refresh();
  P.toast("Speed limit maps removed");
});

P.page("maps", {
  title: "Speed Limit Maps",
  async render(params) {
    const maps = await P.get("/api/maps");
    const selected = new Set(maps.selected);
    const show = params.get("show") === "countries" ? "countries" : "states";
    const busy = isBusy(maps.download) || following;
    return {
      body: `
      <div class="two-col wide-left">
        <section class="panel"><div class="panel-head"><h2>Regions</h2><span class="aside muted" data-count>${selected.size} chosen</span></div>
          <div class="panel-body">
            <div class="maps-tools">
              <div class="seg" role="group" aria-label="Show">
                <button type="button" data-act="maps-show" data-show="states" aria-pressed="${show === "states"}">U.S. states</button>
                <button type="button" data-act="maps-show" data-show="countries" aria-pressed="${show === "countries"}">Countries</button>
              </div>
              <label class="search maps-search"><span class="sr">Search regions</span>${P.ic("search")}<input class="input" type="search" data-region-search placeholder="Search regions" autocomplete="off"></label>
            </div>
            <div data-regions="states" ${show === "states" ? "" : "hidden"}>${regionGroups(maps.states, maps.state_prefix, selected, "U.S. states")}</div>
            <div data-regions="countries" ${show === "countries" ? "" : "hidden"}>${regionGroups(maps.countries, maps.country_prefix, selected, "countries")}</div>
          </div></section>
        <section class="panel maps-device"><div class="panel-head"><h2>On the device</h2></div>
          <div class="panel-body grid-12">
            <dl class="kv"><dt>Storage used</dt><dd>${maps.storage_used ? P.size(maps.storage_used) : "None"}</dd><dt>Last updated</dt><dd>${maps.last_updated ? P.esc(maps.last_updated) : "Never"}</dd></dl>
            <label class="field"><span>Update schedule</span>${P.dropdown({ value: String(maps.schedule), options: SCHEDULES, label: "Update schedule", attrs: "data-schedule" })}</label>
          </div>
          <div class="panel-foot">
            <button class="btn btn-primary btn-small" data-act="maps-download" ${busy ? "disabled" : ""}>${P.ic("download")}Download regions</button>
            ${maps.storage_used || selected.size ? `<button class="btn btn-danger-quiet btn-small maps-remove" data-act="maps-remove" ${busy ? "disabled" : ""}>Remove maps…</button>` : ""}
          </div>
        </section>
      </div>`,
      mount: root => {
        if (busy) {
          followDownload(maps.download.request_time);
        }
        root.querySelector("[data-region-search]").addEventListener("input", e => filterRegions(root, e.currentTarget.value));
        root.querySelectorAll("[data-region]").forEach(box => box.addEventListener("change", () => {
          if (box.checked) {
            selected.add(box.dataset.region);
          } else {
            selected.delete(box.dataset.region);
          }
          root.querySelector("[data-count]").textContent = `${selected.size} chosen`;
          const regions = [...selected];
          saving = saving.then(() => P.request("PUT", "/api/maps/selection", { regions })).catch(error => {
            P.fail(error, "Couldn't save your regions", MESSAGES);
            P.refresh();
          });
        }));
        root.querySelector("[data-schedule]").addEventListener("change", async e => {
          const wrap = e.currentTarget;
          try {
            await P.request("PUT", "/api/maps/schedule", { schedule: Number(wrap.dataset.value) });
          } catch (error) {
            P.setDropdown(wrap, String(maps.schedule));
            P.fail(error, "Couldn't change the schedule", MESSAGES);
            return;
          }
          maps.schedule = Number(wrap.dataset.value);
          P.toast("Map schedule saved");
        });
      }
    };
  }
});
})();
