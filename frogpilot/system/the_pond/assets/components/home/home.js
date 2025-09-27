import { html, reactive } from "https://esm.sh/@arrow-js/core";

function DiskUsage(disk) {
  const used = parseFloat(disk.usedPercentage) || 0;
  const rightRadius = used >= 100 ? "0" : "var(--border-radius-md)";

  return html`
    <div class="disk">
      <p>${disk.used} used of ${disk.size}</p>
      <div class="progress">
        <div
          class="bar"
          style="
            border-bottom-right-radius: ${rightRadius};
            border-top-right-radius: ${rightRadius};
            width: ${100 - used}%;
          "
        ></div>
      </div>
    </div>
  `;
}

function DriveStat(title, stats = {}, defaultUnit) {
  const format = (n) =>
    n?.toLocaleString("en-US", {
      minimumFractionDigits: 0,
      maximumFractionDigits: 0,
    }) ?? "0";

  return html`
    <div class="drivingStat">
      <h2>${title}</h2>
      <div><p>${format(stats.drives)}</p><p>drives</p></div>
      <div><p>${format(stats.distance)}</p><p>${stats.unit ?? defaultUnit}</p></div>
      <div><p>${format(stats.hours)}</p><p>hours</p></div>
    </div>
  `;
}

function renderSoftwareInfo(info = {}) {
  const fields = [
    ["Branch Name", info.branchName],
    ["Build", info.buildEnvironment],
    ["Commit Hash", info.commitHash],
    ["Fork Maintainer", info.forkMaintainer],
    ["Update Available", info.updateAvailable],
    ["Version Date", info.versionDate],
  ];

  return fields.map(
    ([label, value]) =>
      html`<p><strong>${label}:</strong> ${value ?? "Unknown"}</p>`
  );
}

function renderDiskUsageSection({ diskError, diskUsage }) {
  if (diskError) {
    return html`<p>${diskError.join("<br>")}</p>`;
  }
  if (diskUsage?.length) {
    return diskUsage.map(DiskUsage);
  }
  return DiskUsage({ size: "0 GB", used: "0 GB", usedPercentage: "0" });
}

export function Home() {
  const state = reactive({
    data: null,
    unit: "miles",
    isLoading: true,
    error: null,
  });

  async function initialize() {
    try {
      const [statsResponse, unitResponse] = await Promise.all([
        fetch("/api/stats"),
        fetch("/api/params?key=IsMetric"),
      ]);

      if (!statsResponse.ok) throw new Error(`API error: ${statsResponse.statusText}`);
      if (!unitResponse.ok) throw new Error(`API error: ${unitResponse.statusText}`);

      const statsJson = await statsResponse.json();
      const isMetricText = (await unitResponse.text()).trim();
      const isMetric = isMetricText === "1";

      state.data = statsJson;
      state.unit = isMetric ? "kilometers" : "miles";
      localStorage.setItem("isMetric", isMetricText);
    } catch (err) {
      console.error("Failed to initialize component:", err);
      state.error = err.message;
    } finally {
      state.isLoading = false;
    }
  }

  initialize();

  return html`
    <div>
      ${() => {
        if (state.isLoading) {
          return html`<p>Loading...</p>`;
        }

        if (state.error) {
          return html`<p class="error">Failed to load data: ${state.error}</p>`;
        }

        if (state.data) {
          const { driveStats, firehoseStats, softwareInfo } = state.data;
          return html`
            <h1>The Pond</h1>

            <div class="drivingStats">
              ${DriveStat("All Time", driveStats?.all, state.unit)}
              ${DriveStat("Past Week", driveStats?.week, state.unit)}
              ${DriveStat("FrogPilot", driveStats?.frogpilot, state.unit)}
            </div>

            <h2>Disk Usage</h2>
            <div class="diskUsage">
              ${renderDiskUsageSection(state.data)}
            </div>

            <h2>Firehose Segments</h2>
            <div class="firehoseStats">
              <p>
                <strong>${(firehoseStats?.segments ?? 0).toLocaleString("en-US")}</strong>
                segments in training data.
              </p>
            </div>

            <h2>Software Info</h2>
            <div class="softwareInfo">
              <div class="softwareGrid">${renderSoftwareInfo(softwareInfo)}</div>
            </div>
          `;
        }

        return html`<p>No data available.</p>`;
      }}
    </div>
  `;
}
