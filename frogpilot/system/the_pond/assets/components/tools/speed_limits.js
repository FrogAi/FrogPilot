import { html, reactive } from "/assets/vendor/arrow.mjs"
import { downloadBlob } from "/assets/js/api.js"

export function SpeedLimits() {
  const state = reactive({ busy: false })
  async function handleDownload() {
    if (state.busy) return
    state.busy = true
    try {
      const res = await fetch("/api/speed_limits", { method: "POST" })
      if (!res.ok) {
        showSnackbar("Download failed...", "error")
        return
      }
      downloadBlob(await res.blob(), "speed_limits.json")
      showSnackbar("Download started...")
    } catch {
      showSnackbar("Download failed...", "error")
    } finally {
      state.busy = false
    }
  }

  return html`
    <div class="download-speed-limits-wrapper">
      <section class="download-speed-limits-widget">
        <div class="download-speed-limits-title">Download Speed Limits</div>
        <p class="download-speed-limits-text">
          Download speed limit data collected using "Speed Limit Filler", then review it on the Speed Limit Filler website before submitting any OSM edits.
        </p>
        <div class="download-speed-limits-button-wrapper">
          <button class="download-speed-limits-button" disabled="${() => state.busy}" @click="${handleDownload}">${() => state.busy ? "Downloading..." : "Download"}</button>
          <a class="download-speed-limits-button" href="https://speedlimitfiller.frogpilot.com" target="_blank" rel="noopener noreferrer">Open Website</a>
        </div>
      </section>
    </div>
  `
}
