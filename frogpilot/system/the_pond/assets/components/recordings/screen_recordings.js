import { html, reactive } from "/assets/vendor/arrow.mjs"
import { Modal } from "/assets/components/modal.js";
import { onRouteLeave } from "/assets/components/router.js"

const state = reactive({
  loading: true,
  error: null,
  recordings: [],
  selectedRecording: null,
  showDeleteModal: false,
  recordingToDelete: null,
  showDeleteAllModal: false,
  isDeletingAll: false,
  progress: 0,
  total: 0,
})

function getOrdinalSuffix(n) {
  const s = ["th", "st", "nd", "rd"];
  const v = n % 100;
  return s[(v - 20) % 10] || s[v] || s[0];
}

function formatScreenRecordingDate(dateString) {
  const date = new Date(dateString);
  if (isNaN(date)) return "Unknown date";
  const month = date.toLocaleString("en-US", { month: "long" });
  const day = date.getDate();
  const year = date.getFullYear();
  let hour = date.getHours();
  const minute = date.getMinutes();
  const ampm = hour >= 12 ? "pm" : "am";
  hour = hour % 12;
  hour = hour ? hour : 12;
  const minuteStr = minute < 10 ? "0" + minute : minute;
  return `${month} ${day}${getOrdinalSuffix(day)}, ${year} - ${hour}:${minuteStr}${ampm}`;
}

let recordingsController = null

async function fetchRecordings() {
  if (recordingsController) recordingsController.abort()
  const controller = new AbortController()
  recordingsController = controller
  try {
    const response = await fetch("/api/screen_recordings/list", { signal: controller.signal });
    if (!response.ok) throw new Error("Network response was not ok");

    const reader = response.body.getReader();
    const decoder = new TextDecoder();
    let buffer = "";

    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      if (controller.signal.aborted) return;

      buffer += decoder.decode(value, { stream: true });
      const lines = buffer.split("\n\n");
      buffer = lines.pop();

      for (const line of lines) {
        if (line.startsWith("data: ")) {
          try {
            const data = JSON.parse(line.substring(6));
            if (data.progress !== undefined && data.total !== undefined) {
              state.progress = data.progress;
              state.total = data.total;
            }
            if (data.recordings) {
              state.recordings.push(...data.recordings);
            }
          } catch (e) {
            console.error("Failed to parse JSON:", e);
          }
        }
      }
    }
  } catch (_) {
    if (controller.signal.aborted) return;
    state.error = "Couldn't load recordings. Please try again later..."
  } finally {
    if (recordingsController === controller) state.loading = false
  }
}

fetchRecordings()
onRouteLeave(() => { if (recordingsController) recordingsController.abort() })

function refresh() {
  if (recordingsController) recordingsController.abort()
  state.error = null
  state.loading = true
  state.recordings = []
  fetchRecordings()
}

let overlay = null
let overlayOpener = null

function openDialog(template) {
  const opener = document.activeElement
  const o = document.createElement("div")
  o.className = "dialog-overlay"
  template(o)
  document.body.appendChild(o)
  const box = o.querySelector(".dialog-box")
  if (box) {
    box.setAttribute("role", "dialog")
    box.setAttribute("aria-modal", "true")
    const heading = box.querySelector("p")
    if (heading) box.setAttribute("aria-label", heading.textContent.trim())
  }
  o.__opener = opener
  const input = o.querySelector(".rn-input")
  if (input) input.focus()
  o.addEventListener("keydown", e => {
    if (e.key === "Escape") {
      e.preventDefault()
      closeDialog(o)
      return
    }
    if (e.key === "Tab") {
      const focusable = o.querySelectorAll(
        'button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])'
      )
      if (!focusable.length) return
      const first = focusable[0]
      const last = focusable[focusable.length - 1]
      const active = document.activeElement
      if (e.shiftKey && active === first) {
        e.preventDefault()
        last.focus()
      } else if (!e.shiftKey && active === last) {
        e.preventDefault()
        first.focus()
      }
    }
  })
  return o
}

function closeDialog(o) { if (o) { const opener = o.__opener; o.remove(); if (opener && opener.focus) opener.focus(); } }

async function renameFile(rec) {
  const base = rec.filename.replace(/\.mp4$/i, "")
  const onSave = async () => {
    const val = dlg.querySelector(".rn-input").value.trim()
    if (!val) return
    const oldFilename = rec.filename
    const newFilename = val + ".mp4"

    try {
      const res = await fetch("/api/screen_recordings/rename", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ old: oldFilename, new: newFilename }),
      })

      if (res.ok) {
        closeDialog(dlg)

        const overlayTitleSpan = overlay && overlay.querySelector(".media-player-title span");
        if (overlayTitleSpan) {
          overlayTitleSpan.textContent = val.replace(/_/g, " ");
        }
        refresh()
        showSnackbar("Recording renamed!")
      } else {
        showSnackbar("Rename failed...", "error")
      }
    } catch {
      showSnackbar("Rename failed...", "error")
    }
  }
  const dlg = openDialog(html`
    <div class="dialog-box">
      <p>Rename “${() => rec.filename}”</p>
      <input class="rn-input" aria-label="New recording name" .value="${() => base}" @keydown="${e => { if (e.key === 'Enter') { e.preventDefault(); onSave(); } }}" />
      <div class="dialog-buttons">
        <button class="btn-cancel" @click="${() => closeDialog(dlg)}">Cancel</button>
        <button class="btn-save" @click="${onSave}">Save</button>
      </div>
    </div>`)
}

function confirmDeleteFile(rec) {
    state.recordingToDelete = rec;
    state.showDeleteModal = true;
}

async function deleteFile() {
  if (!state.recordingToDelete) return;
  const rec = state.recordingToDelete;

  try {
    const res = await fetch(`/api/screen_recordings/delete/${encodeURIComponent(rec.filename)}`, { method: "DELETE" })
    if (res.ok) {
        closeOverlay();
        refresh();
        showSnackbar("Recording deleted!");
    } else {
        showSnackbar("Delete failed...", "error");
    }
  } catch {
    showSnackbar("Delete failed...", "error");
  } finally {
    state.showDeleteModal = false;
    state.recordingToDelete = null;
  }
}

function openOverlay(rec) {
  if (overlay) return
  overlayOpener = document.activeElement
  overlay = document.createElement("div")
  overlay.className = "media-player-overlay"
  const displayName = rec.is_custom_name ? rec.filename.replace(/\.mp4$/i, "").replace(/_/g, " ") : formatScreenRecordingDate(rec.timestamp);
  const downloadUrl = `/api/screen_recordings/download/${encodeURIComponent(rec.filename)}`
  const onDownload = () => {
    const link = document.createElement("a");
    link.href = downloadUrl;
    link.download = rec.filename;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  }
  html`
    <div class="media-player-content">
      <div class="media-player-title">
        <span>${() => displayName}</span>
        <button type="button" class="action-rename-icon" aria-label="Rename recording" @click="${() => renameFile(rec)}"><i class="bi bi-pencil-fill"></i></button>
      </div>
      <video controls autoplay muted>
        <source src="${downloadUrl}" type="video/mp4">
      </video>
      <div class="button-row">
        <button class="close-button action-close" @click="${closeOverlay}">Close</button>
        <button class="close-button action-download" @click="${onDownload}">Download</button>
        <button class="close-button action-delete" @click="${() => confirmDeleteFile(rec)}">Delete</button>
      </div>
    </div>`(overlay)
  overlay.addEventListener("click", e => { if (e.target === overlay) closeOverlay() })
  overlay.addEventListener("keydown", e => { if (e.key === "Escape") { e.preventDefault(); closeOverlay() } })
  overlay.setAttribute("role", "dialog")
  overlay.setAttribute("aria-modal", "true")
  overlay.setAttribute("aria-label", displayName ? `Recording: ${displayName}` : "Recording")
  document.body.appendChild(overlay)
  const closeBtn = overlay.querySelector(".action-close")
  if (closeBtn) closeBtn.focus()
}

function closeOverlay() {
  if (!overlay) return
  overlay.remove()
  overlay = null
  state.selectedRecording = null
  if (overlayOpener && overlayOpener.focus) overlayOpener.focus()
  overlayOpener = null
}

async function deleteAllRecordings() {
  state.showDeleteAllModal = false
  state.isDeletingAll = true
  try {
    const res = await fetch("/api/screen_recordings/delete_all", { method: "DELETE" })
    if (!res.ok) throw new Error()
    await refresh()
    showSnackbar("All screen recordings deleted!")
  } catch {
    showSnackbar("An error occurred while deleting all screen recordings...", "error")
  } finally {
    state.isDeletingAll = false
  }
}

export function ScreenRecordings() {
  onRouteLeave(closeOverlay)
  if (state.selectedRecording && !overlay) openOverlay(state.selectedRecording)

  return html`
    <div class="screen-recordings-wrapper">
      <div class="screen-recordings-widget">
        <div class="screen-recordings-title">Screen Recordings</div>

        ${() => {
          if (state.loading && state.recordings.length === 0) return html`<p class="screen-recordings-message">Loading...</p>`
          if (state.error) return html`<p class="screen-recordings-message">${state.error}</p>`
          if (state.progress > 0 && state.progress < state.total) {
            return html`<p class="screen-recordings-message">Processing Recordings: ${state.progress} of ${state.total}</p>`
          }
          if (state.recordings.length === 0 && !state.loading) {
            return html`<p class="screen-recordings-message">No screen recordings found...</p>`
          }
          return ""
        }}

        <div class="screen-recordings-grid ${() => state.isDeletingAll ? "disabled" : ""}">
          ${() => state.recordings.map(rec => {
            const displayName = rec.is_custom_name ? rec.filename.replace(/\.mp4$/i, "").replace(/_/g, " ") : formatScreenRecordingDate(rec.timestamp)
            return html`
              <div
                class="recording-card"
                role="button"
                tabindex="0"
                aria-label="${() => "Open recording " + displayName}"
                @keydown="${e => { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); state.selectedRecording = rec; } }}"
                @mouseenter="${e => {
                  if (state.selectedRecording) return;

                  const card = e.currentTarget;
                  const gif = card.querySelector(".recording-preview-gif");
                  const png = card.querySelector(".recording-preview-png");

                  if (card.dataset.gifLoaded) {
                    png.style.display = "none";
                    gif.style.display = "block";
                    return;
                  }

                  card.dataset.loadingGif = "true";
                  const preloader = new Image();
                  preloader.onload = () => {
                    if (card.dataset.loadingGif === "true") {
                        gif.src = preloader.src;
                        png.style.display = "none";
                        gif.style.display = "block";
                        card.dataset.gifLoaded = true;
                    }
                    delete card.dataset.loadingGif;
                  };
                  preloader.onerror = () => {
                    console.error("Failed to load preview GIF:", preloader.src);
                    delete card.dataset.loadingGif;
                  };

                  preloader.src = gif.dataset.src;
                }}"
                @mouseleave="${e => {
                  const card = e.currentTarget;
                  card.querySelector(".recording-preview-png").style.display = "block";
                  card.querySelector(".recording-preview-gif").style.display = "none";
                  if (card.dataset.loadingGif === "true") {
                      delete card.dataset.loadingGif;
                  }
                }}"
                @click="${() => { state.selectedRecording = rec }}"
              >
                <div class="recording-preview-container">
                  <img src="${rec.png}" class="recording-preview recording-preview-png" style="display:block;" @error="${e => { e.target.style.visibility = 'hidden' }}">
                  <img data-src="${rec.gif}" class="recording-preview recording-preview-gif" style="display:none;">
                </div>
                <p class="recording-filename">${() => displayName}</p>
              </div>
            `
          })}
        </div>

        ${() => {
          if (state.recordings.length > 0) {
            return html`
              <button
                class="delete-all-button"
                @click="${() => (state.showDeleteAllModal = true)}"
                disabled="${() => state.isDeletingAll}"
              >
                ${() => (state.isDeletingAll ? "Deleting..." : "Delete All Recordings")}
              </button>
            `
          }
          return ""
        }}
      </div>
      ${() => state.showDeleteModal ? Modal({
          title: "Confirm Delete",
          message: html`Are you sure you want to delete <strong>${() => state.recordingToDelete?.filename ?? ""}</strong>?`,
          onConfirm: deleteFile,
          onCancel: () => { state.showDeleteModal = false; state.recordingToDelete = null; },
          confirmText: "Delete"
      }) : ""}
      ${() => state.showDeleteAllModal ? Modal({
        title: "Confirm Delete All",
        message: "Are you sure you want to delete all screen recordings? This action cannot be undone...",
        onConfirm: deleteAllRecordings,
        onCancel: () => { state.showDeleteAllModal = false; },
        confirmText: "Delete All"
      }) : ""}
    </div>
  `
}
