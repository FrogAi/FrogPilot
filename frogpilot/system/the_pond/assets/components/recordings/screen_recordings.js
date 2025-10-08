import { html, reactive } from "https://esm.sh/@arrow-js/core"
import { Modal } from "/assets/components/modal.js";

const state = reactive({
  loading: true,
  error: null,
  recordings: [],
  selectedRecording: null,
  showDeleteModal: false,
  recordingToDelete: null,
  showDeleteAllModal: false,
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


async function fetchRecordings() {
  try {
    const response = await fetch("/api/screen_recordings/list");
    if (!response.ok) throw new Error("Network response was not ok");

    const reader = response.body.getReader();
    const decoder = new TextDecoder();
    let buffer = "";

    while (true) {
      const { value, done } = await reader.read();
      if (done) break;

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
    state.error = "Couldn't load recordings. Please try again later..."
  } finally {
    state.loading = false
  }
}

fetchRecordings()

function refresh() {
  state.loading = true
  state.recordings = []
  fetchRecordings()
}

let overlay = null

function openDialog(htmlStr) {
  const o = document.createElement("div")
  o.className = "dialog-overlay"
  o.innerHTML = htmlStr
  document.body.appendChild(o)
  return o
}

function closeDialog(o) { if (o) o.remove() }

async function renameFile(rec) {
  const base = rec.filename.replace(/\.mp4$/i, "")
  const dlg = openDialog(`
    <div class="dialog-box">
      <p>Rename “${rec.filename}”</p>
      <input class="rn-input" value="${base}" />
      <div class="dialog-buttons">
        <button class="btn-cancel">Cancel</button>
        <button class="btn-save">Save</button>
      </div>
    </div>`)
  dlg.querySelector(".btn-cancel").onclick = () => closeDialog(dlg)
  dlg.querySelector(".btn-save").onclick = async () => {
    const val = dlg.querySelector(".rn-input").value.trim()
    if (!val) return
    const oldFilename = rec.filename
    const newFilename = val + ".mp4"

    const res = await fetch("/api/screen_recordings/rename", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ old: oldFilename, new: newFilename }),
    })

    if (res.ok) {
      closeDialog(dlg)

      const recordingToUpdate = state.recordings.find(r => r.filename === oldFilename)
      if (recordingToUpdate) {
        recordingToUpdate.filename = newFilename
        recordingToUpdate.is_custom_name = true
        recordingToUpdate.gif = `/screen_recordings/${val}.gif`
        recordingToUpdate.png = `/screen_recordings/${val}.png`
      }

      const overlayTitleSpan = overlay.querySelector(".media-player-title span");
      if (overlayTitleSpan) {
        overlayTitleSpan.textContent = val.replace(/_/g, " ");
      }
      showSnackbar("Recording renamed!")
    } else {
      showSnackbar("Rename failed...", "error")
    }
  }
}

function confirmDeleteFile(rec) {
    state.recordingToDelete = rec;
    state.showDeleteModal = true;
}

async function deleteFile() {
  if (!state.recordingToDelete) return;
  const rec = state.recordingToDelete;

  const res = await fetch(`/api/screen_recordings/delete/${encodeURIComponent(rec.filename)}`, { method: "DELETE" })
  if (res.ok) {
      closeOverlay();
      refresh();
      showSnackbar("Recording deleted!");
  } else {
      showSnackbar("Delete failed...", "error");
  }

  state.showDeleteModal = false;
  state.recordingToDelete = null;
}

function openOverlay(rec) {
  if (overlay) return
  overlay = document.createElement("div")
  overlay.className = "media-player-overlay"
  const displayName = rec.is_custom_name ? rec.filename.replace(/\.mp4$/i, "") : formatScreenRecordingDate(rec.timestamp);
  overlay.innerHTML = `
    <div class="media-player-content">
      <div class="media-player-title">
        <span>${displayName}</span>
        <i class="bi bi-pencil-fill action-rename-icon"></i>
      </div>
      <video controls autoplay muted>
        <source src="/api/screen_recordings/download/${rec.filename}" type="video/mp4">
      </video>
      <div class="button-row">
        <button class="close-button action-close">Close</button>
        <button class="close-button action-download">Download</button>
        <button class="close-button action-delete">Delete</button>
      </div>
    </div>`
  overlay.addEventListener("click", e => { if (e.target === overlay) closeOverlay() })
  overlay.querySelector(".action-close").onclick = closeOverlay
  overlay.querySelector(".action-rename-icon").onclick = () => renameFile(rec)
  overlay.querySelector(".action-delete").onclick = () => confirmDeleteFile(rec)
  overlay.querySelector(".action-download").onclick = () => {
    const link = document.createElement("a");
    link.href = `/api/screen_recordings/download/${rec.filename}`;
    link.download = rec.filename;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  }
  document.body.appendChild(overlay)
}

function closeOverlay() {
  if (!overlay) return
  overlay.remove()
  overlay = null
  state.selectedRecording = null
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

        <div class="screen-recordings-grid">
          ${() => state.recordings.map(rec => {
            const displayName = rec.is_custom_name ? rec.filename.replace(/\.mp4$/i, "").replace(/_/g, " ") : formatScreenRecordingDate(rec.timestamp)
            return html`
              <div
                class="recording-card"
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
                  <img src="${rec.png}" class="recording-preview recording-preview-png" style="display:block;">
                  <img data-src="${rec.gif}" class="recording-preview recording-preview-gif" style="display:none;">
                </div>
                <p class="recording-filename">${displayName}</p>
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
              >
                Delete All Recordings
              </button>
            `
          }
          return ""
        }}
      </div>
      ${() => state.showDeleteModal ? Modal({
          title: "Confirm Delete",
          message: `Are you sure you want to delete <strong>${state.recordingToDelete.filename}</strong>?`,
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
