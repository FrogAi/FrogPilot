import { html, reactive } from "https://esm.sh/@arrow-js/core"
import { Link } from "./router.js"
import { SettingsView } from "./settings/settings.js"

/**
 * Renders the Overview component that loads when you load
 * thepond. Contains one section with the number of rides,
 * duration and distance for all time, current week and while
 * using frogpilot.
 * Then there is a section with the disk usage
 * Then there are shortcuts to settings search and nav input
 *
 * @returns
 */
export function Overview() {
  const state = reactive({
    data: "{}",
    search: "",
  })

  async function fetchData() {
    const response = await fetch("/api/stats")
    const data = await response.json()
    state.data = JSON.stringify(data)
    if (data.diskError) {
      showSnackbar(data.diskError.join("<br>"), "error", 6000)
    }
    if (data.driveErrors) {
      showSnackbar(data.driveErrors.join("<br>"), "error", 6000)
    }
  }

  function Search(e) {
    state.search = e.target.value
  }

  fetchData()

  console.log("rendering overview")

  return html`
    <div>
      ${() => {
        return html`${() => {
            if (state.search) {
              return ""
            }
            return html`
              <h1>ThePond</h1>
              <div class="drivingStats">
                ${() =>
                  DriveStat(
                    "All Time",
                    JSON.parse(state.data)?.driveStats?.all ?? {}
                  )}
                ${() =>
                  DriveStat(
                    "Past Week",
                    JSON.parse(state.data)?.driveStats?.week ?? {}
                  )}
                ${() =>
                  DriveStat(
                    "FrogPilot",
                    JSON.parse(state.data)?.driveStats?.frogpilot ?? {}
                  )}
              </div>
              <div class="diskUsage">
                <h2>Disk Usage</h2>
                ${() => {
                  const data = JSON.parse(state.data)
                  if (data.diskError) {
                    return html`<p>${data.diskError.join("<br>")}</p>`
                  }
                  return data?.diskUsage?.map((disk) =>
                    DiskUsage(disk))
                  }}
              </div>
            `
          }}
          <div class="shortcuts">
            ${() => {
              return !state.search ? html`<h2>Shortcuts</h2>` : ""
            }}
            <div class="searchBar">
              ${() => {
                return html`
                  <input
                    class="searchfield"
                    type="text"
                    placeholder="Search for settings"
                    value="${() => state.search}"
                    @input="${Search}"
                  />
                `.key("searchfield")
              }}
            </div>
            ${() => {
              return !state.search
                ? html`${Link(
                    "/navigation",
                    html`
                      <div class="shortcutLink">
                        <p>Set Navigation Destination</p>
                        <i class="bi bi-arrow-right"></i>
                      </div>
                    `
                  )}`
                : ""
            }}
            ${() => {
              if (state.search) {
                return SettingsView({
                  params: { section: "controls" },
                  searchQuery: state.search,
                })
              }
            }}
          </div> `
      }}
    </div>
  `
}

/**
 *
 * @param {string} title
 * @param {Object} stats
 * @param {number} stats.drives
 * @param {number} stats.distance
 * @param {number} stats.hours
 * @returns
 */
function DriveStat(title, stats) {
  const formatNumber = (num) => {
    return num.toLocaleString('en-US', {minimumFractionDigits: 0, maximumFractionDigits: 0});
  };

  return html`
    <div class="drivingStat">
      <h2>${title}</h2>
      <div>
        <p>${formatNumber(stats.drives || 0)}</p>
        <p>drives</p>
      </div>
      <div>
        <p>${formatNumber(stats.distance || 0)}</p>
        <p>${stats.unit}</p>
      </div>
      <div>
        <p>${formatNumber(stats.hours || 0)}</p>
        <p>hours</p>
      </div>
    </div>
  `;
}

/**
 *
 * @param {Object} disk
 * @param {string} disk.mount
 * @param {string} disk.used
 * @param {string} disk.available
 * @param {string} disk.usedPercentage
 * @param {string} disk.size
 */
function DiskUsage(disk) {
  return html`
    <div class="disk">
      <h4>${disk.mount}</h4>
      <p>${disk.used} used of ${disk.size}</p>
      <div class="progress">
        <div class="bar" style="width: ${disk.usedPercentage}"></div>
      </div>
    </div>
  `
}
