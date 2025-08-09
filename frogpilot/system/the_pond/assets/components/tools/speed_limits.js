import { html } from "https://esm.sh/@arrow-js/core"

export function SpeedLimits() {
  function handleDownload() {
    const link = document.createElement("a")
    link.href = "/api/speed_limits"
    link.download = "speed_limits.json"
    link.click()

    showSnackbar("Download started...")
  }

  return html`
    <div class="download-speed-limits-wrapper">
      <section class="download-speed-limits-widget">
        <div class="download-speed-limits-title">Download Speed Limits</div>
        <p class="download-speed-limits-text">
          Download speed limit data collected using "Speed Limit Filler".
        </p>
        <div class="download-speed-limits-button-wrapper">
          <button class="download-speed-limits-button" @click="${handleDownload}">Download</button>
          <a class="download-speed-limits-link" href="https://SpeedLimitFiller.frogpilot.download" target="_blank">
            Submit speed limits here
          </a>
        </div>
      </section>
    </div>
  `
}
