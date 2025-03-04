import { html, reactive } from "https://esm.sh/@arrow-js/core"
import { formatSecondsToHuman, parseErrorLogToDate } from "../js/utils.js"

export function ErrorLogs() {
  const state = reactive({
    files: "[]",
    selectedLog: undefined,
    loading: true,
  })

  async function getErrorLogs() {
    const response = await fetch("/api/error-logs", {
      headers: { Accept: "application/json" },
    })
    const data = await response.json()
    state.files = JSON.stringify(data.map((file) => {
      // Dateformat is YYYY-MM-DD--HH-MM-SS
      const date = parseErrorLogToDate(file)
      // Format it into a string
      const formattedDate = date.toLocaleString()
      const timeSince = Math.round(Date.now() - date.getTime()) / 1000

      return {
        filename: file,
        date: formattedDate,
        timeSince: timeSince,
      }
    }))
    state.loading = false
  }

  getErrorLogs()

  return html`<h1>Error Logs</h1>
    <div id="errorLogs">
      <div id="fileList">
        ${() => {
      if (state.loading) {
        return html`<div class="fileEntry">
              <p>Loading...</p>
            </div>`
      } else {
        return JSON.parse(state.files).map(
          (file) =>
            html`<div
                  class="fileEntry"
                  @click="${() => (state.selectedLog = file.filename)}"
                >
                  <p>${file.date}</p>
                  <p>${formatSecondsToHuman(file.timeSince, "days")} ago</p>
                </div>`
        )
      }
    }}
      </div>
      ${() =>
      state.selectedLog
        ? Logviewer(state.selectedLog, () => (state.selectedLog = undefined))
        : ""}
    </div> `
}

function Logviewer(filename, closeFn) {
  const logfile = reactive({
    loading: true,
    content: undefined,
    error: undefined,
  })

  async function getLogfile() {
    const response = await fetch(`/api/error-logs/${filename}`, {
      headers: { Accept: "application/json" },
    })
    const data = await response.text()
    logfile.content = data
    logfile.loading = false
  }

  getLogfile()

  return html`<div id="fileViewer">
    <div>
      <p>${filename}</p>
      <button @click="${closeFn}"><i class="bi bi-x-lg"></i></button>
    </div>
    <pre>${() => (logfile.loading ? "Loading..." : logfile.content)}</pre>
  </div>`
}
