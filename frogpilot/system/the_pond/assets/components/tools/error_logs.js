import { html, reactive } from "https://esm.sh/@arrow-js/core";
import { formatSecondsToHuman, parseErrorLogToDate } from "/assets/js/utils.js";
import { Modal } from "/assets/components/modal.js";

const state = reactive({
  loading: true,
  files: [],
  selectedLog: undefined,
  confirmDelete: {
    visible: false,
    filename: null,
  },
  showDeleteAllModal: false,
});

;(async () => {
  const res = await fetch("/api/error_logs", {
    headers: { Accept: "application/json" }
  });
  const data = await res.json();

  state.files = data.map(f => {
    const date = parseErrorLogToDate(f);
    return {
      filename: f,
      date: date.toLocaleString(),
      timeSince: (Date.now() - date.getTime()) / 1000,
    };
  });

  state.loading = false;
})();

async function deleteAllLogs() {
  state.showDeleteAllModal = false;
  try {
    const res = await fetch('/api/error_logs/delete_all', { method: 'DELETE' });
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
            : state.files.length === 0
              ? html`<div class="fileEntry"><p>No error logs!</p></div>`
              : state.files.map(file => html`
                <div class="fileEntry"
                  @click="${() => {
                    state.selectedLog = state.selectedLog === file.filename ? undefined : file.filename;
                  }}">
                  <p>${file.date}</p>
                  <p class="time-since">
                    ${file.timeSince < 60 ? "just now" : `${formatSecondsToHuman(file.timeSince, "minutes")} ago`}
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
    message: html`Are you sure you want to delete <strong>${state.confirmDelete.filename}</strong>?`,
    onConfirm: async () => {
      const filename = state.confirmDelete.filename;
      if (!filename) return;
      await fetch(`/api/error_logs/${filename}`, {
        method: "DELETE"
      });
      state.files = state.files.filter(f => f.filename !== filename);
      state.confirmDelete.visible = false;
      state.confirmDelete.filename = null;
      if (state.selectedLog === filename) {
        state.selectedLog = undefined;
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
    const res = await fetch(`/api/error_logs/${filename}`);
    logState.content = await res.text();
    logState.loading = false;

    setTimeout(() => {
      window.scrollTo({
        top: document.body.scrollHeight,
        behavior: 'smooth'
      });
    }, 0);
  })();

  const deleteLog = () => {
    state.confirmDelete.filename = filename;
    state.confirmDelete.visible = true;
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
      <p>${filename}</p>
      <button @click="${closeFn}">
        <i class="bi bi-x-lg"></i>
      </button>
      <button @click="${deleteLog}">
        <i class="bi bi-trash"></i>
      </button>
      <button @click="${copyLog}">
        <i class="bi bi-clipboard"></i>
      </button>
      <a href="/api/error_logs/${filename}" download>
        <button>
          <i class="bi bi-download"></i>
        </button>
      </a>
    </div>
    <pre>${() => (logState.loading ? "Loading..." : logState.content)}</pre>
  </div>
`;
}
