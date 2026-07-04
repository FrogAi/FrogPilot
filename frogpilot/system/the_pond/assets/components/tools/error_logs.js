import { html, reactive } from "/assets/vendor/arrow.mjs";
import { formatSecondsToHuman, parseErrorLogToDate } from "/assets/js/utils.js";
import { Modal } from "/assets/components/modal.js";

const state = reactive({
  loading: true,
  loadError: false,
  files: [],
  selectedLog: undefined,
  confirmDelete: {
    visible: false,
    filename: null,
  },
  showDeleteAllModal: false,
});

;(async () => {
  try {
    const res = await fetch("/api/error_logs", { headers: { Accept: "application/json" } });
    const data = await res.json();
    state.files = data.map(f => {
      let date = null;
      try { date = parseErrorLogToDate(f); } catch {}
      return {
        filename: f,
        date: date ? date.toLocaleString() : f,
        timeSince: date ? (Date.now() - date.getTime()) / 1000 : null,
      };
    });
  } catch (e) {
    console.error("Failed to load error logs:", e);
    state.loadError = true;
  } finally {
    state.loading = false;
  }
})();

async function deleteAllLogs() {
  state.showDeleteAllModal = false;
  try {
    const res = await fetch("/api/error_logs/delete_all", { method: "DELETE" });
    if (res.ok) {
      showSnackbar("All error logs deleted!");
      state.files = [];
      state.selectedLog = undefined;
    } else {
      showSnackbar("Delete all failed...", "error");
    }
  } catch (err) {
    showSnackbar("An error occurred while deleting error logs...", "error");
  }
}

export function ErrorLogs() {
  return html`
  <div class="error-logs-wrapper">
    <div id="errorLogs">
      <div id="fileList">
        ${() =>
          state.loading
            ? html`<div class="fileEntry"><p>Loading...</p></div>`
            : state.loadError
              ? html`<div class="fileEntry"><p>Failed to load error logs</p></div>`
              : state.files.length === 0
              ? html`<div class="fileEntry"><p>No error logs!</p></div>`
              : state.files.map(file => html`
                <div class="fileEntry" role="button" tabindex="0"
                  @click="${() => {
                    state.selectedLog = state.selectedLog === file.filename ? undefined : file.filename;
                  }}"
                  @keydown="${(e) => {
                    if (e.key === "Enter" || e.key === " ") {
                      e.preventDefault();
                      state.selectedLog = state.selectedLog === file.filename ? undefined : file.filename;
                    }
                  }}">
                  <p>${() => file.date}</p>
                  <p class="time-since">
                    ${file.timeSince == null
                      ? "unknown"
                      : file.timeSince < 60
                        ? "just now"
                        : `${formatSecondsToHuman(file.timeSince, "minutes")} ago`}
                  </p>
                </div>
              `)
        }
        ${() =>
          state.files.length > 0
            ? html`
              <div style="text-align: center; padding: 1rem;">
                <button
                  class="delete-all-button"
                  @click="${() => (state.showDeleteAllModal = true)}"
                >
                  Delete All Error Logs
                </button>
              </div>
            `
            : ""
        }
      </div>
      ${() =>
        state.selectedLog ? Logviewer(state.selectedLog, () => (state.selectedLog = undefined)) : ""
      }
    </div>
  </div>
  ${() => state.confirmDelete.visible ? Modal({
    title: "Confirm Delete",
    message: html`Are you sure you want to delete <strong>${() => state.confirmDelete.filename}</strong>?`,
    onConfirm: async () => {
      const filename = state.confirmDelete.filename;
      if (!filename) return;
      try {
        const res = await fetch(`/api/error_logs/${encodeURIComponent(filename)}`, {
          method: "DELETE"
        });
        if (!res.ok) {
          showSnackbar("Delete failed...", "error");
          return;
        }
        state.files = state.files.filter(f => f.filename !== filename);
        if (state.selectedLog === filename) {
          state.selectedLog = undefined;
        }
      } catch (err) {
        showSnackbar("An error occurred while deleting the log...", "error");
      } finally {
        state.confirmDelete.visible = false;
        state.confirmDelete.filename = null;
      }
    },
    onCancel: () => {
      state.confirmDelete.visible = false;
      state.confirmDelete.filename = null;
    }
  }) : ""}
  ${() => state.showDeleteAllModal ? Modal({
    title: "Confirm Delete All",
    message: "Are you sure you want to delete all error logs? This action cannot be undone...",
    onConfirm: deleteAllLogs,
    onCancel: () => { state.showDeleteAllModal = false; },
    confirmText: "Delete All"
  }) : ""}
`;
}

function Logviewer(filename, closeFn) {
  const logState = reactive({
    loading: true,
    content: ""
  });

  ;(async () => {
    try {
      const res = await fetch(`/api/error_logs/${encodeURIComponent(filename)}`);
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      logState.content = await res.text();

      setTimeout(() => {
        window.scrollTo({
          top: document.body.scrollHeight,
          behavior: "smooth"
        });
      }, 0);
    } catch (err) {
      console.error("Failed to load log:", err);
      logState.content = "Failed to load log";
    } finally {
      logState.loading = false;
    }
  })();

  const deleteLog = () => {
    state.confirmDelete.filename = filename;
    state.confirmDelete.visible = true;
  };

  const downloadLog = () => {
    const link = document.createElement("a");
    link.href = `/api/error_logs/${encodeURIComponent(filename)}`;
    link.download = filename;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  const copyLog = () => {
    if (navigator.clipboard && window.isSecureContext) {
      navigator.clipboard.writeText(logState.content);
    } else {
      const textArea = document.createElement("textarea");
      textArea.value = logState.content;
      textArea.style.position = "fixed";
      textArea.style.left = "-9999px";
      document.body.appendChild(textArea);
      textArea.focus();
      textArea.select();
      try {
        document.execCommand("copy");
      } catch (err) {
        console.error("Fallback: Oops, unable to copy", err);
      }
      document.body.removeChild(textArea);
    }
    showSnackbar("Copied to clipboard!");
  };

  return html`
  <div id="fileViewer">
    <div>
      <p>${() => filename}</p>
      <button @click="${closeFn}" aria-label="Close log viewer">
        <i class="bi bi-x-lg"></i>
      </button>
      <button @click="${deleteLog}" aria-label="Delete log">
        <i class="bi bi-trash"></i>
      </button>
      <button @click="${copyLog}" aria-label="Copy log to clipboard">
        <i class="bi bi-clipboard"></i>
      </button>
      <button @click="${downloadLog}" aria-label="Download log">
        <i class="bi bi-download"></i>
      </button>
    </div>
    <pre>${() => (logState.loading ? "Loading..." : logState.content)}</pre>
  </div>
`;
}
