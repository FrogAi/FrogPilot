import { html, reactive } from "/assets/vendor/arrow.mjs"
import { Modal } from "/assets/components/modal.js"
import { onRouteLeave } from "/assets/components/router.js"
import { fetchJson, downloadBlob } from "/assets/js/api.js"

export function ToggleControl () {
  const state = reactive({
    showResetDefaultModal: false,
    showResetStockModal: false,
    busy: false,
  });

  const fileInput = document.createElement("input")
  fileInput.type = "file"
  fileInput.accept = ".json"
  fileInput.style.display = "none"
  fileInput.addEventListener("change", restoreToggles)
  document.body.appendChild(fileInput)
  onRouteLeave(() => fileInput.remove())

  async function backupToggles () {
    state.busy = true
    try {
      const response = await fetch("/api/toggles/backup", { method: "POST" })
      if (!response.ok) throw new Error("Backup failed...")
      downloadBlob(await response.blob(), "toggle-backup.json")
      showSnackbar("Toggle backup downloaded!")
    } catch (e) {
      showSnackbar(e.message || "Backup failed...", "error")
    } finally {
      state.busy = false
    }
  }

  async function restoreToggles (event) {
    const uploadedFile = event.target.files[0]
    if (!uploadedFile) return
    state.busy = true
    try {
      const toggleData = JSON.parse(await uploadedFile.text())
      const result = await fetchJson("/api/toggles/restore", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(toggleData)
      })
      showSnackbar((result && result.message) || "Toggles restored!")
    } catch (e) {
      showSnackbar(e.message || "Restore failed...", "error")
    } finally {
      event.target.value = ""
      state.busy = false
    }
  }

  function confirmResetDefault () {
    state.showResetDefaultModal = true;
  }

  async function resetTogglesToDefault () {
    state.showResetDefaultModal = false;
    state.busy = true;
    showSnackbar("Resetting toggles to their default values...");
    try {
      const res = await fetch("/api/toggles/reset_default", { method: "POST" });
      if (!res.ok) {
        showSnackbar("Reset to default failed...", "error");
        return;
      }
      showSnackbar("Rebooting...");
    } catch (e) {
      showSnackbar(e.message || "Reset to default failed...", "error");
    } finally {
      state.busy = false;
    }
  }

  function confirmResetStock () {
    state.showResetStockModal = true;
  }

  async function resetTogglesToStock () {
    state.showResetStockModal = false;
    state.busy = true;
    showSnackbar("Resetting toggles to stock openpilot values...");
    try {
      const res = await fetch("/api/toggles/reset_stock", { method: "POST" });
      if (!res.ok) {
        showSnackbar("Reset to stock failed...", "error");
        return;
      }
      showSnackbar("Rebooting...");
    } catch (e) {
      showSnackbar(e.message || "Reset to stock failed...", "error");
    } finally {
      state.busy = false;
    }
  }

  function triggerRestorePrompt () {
    fileInput.click()
  }

  return html`
    <div class="toggle-control-wrapper">
      <section class="toggle-control-widget">
        <div class="toggle-control-title">Backup/Restore Toggles</div>
        <p class="toggle-control-text">
          Use the buttons below to backup or restore your toggles.
        </p>
        <button class="toggle-control-button" disabled="${() => state.busy}" @click="${backupToggles}">Backup Toggles</button>
        <button class="toggle-control-button" disabled="${() => state.busy}" @click="${triggerRestorePrompt}">Restore Toggles</button>
      </section>

      <section class="toggle-control-widget" style="margin-left: 1.5rem">
        <div class="toggle-control-title">Reset Toggles to Default FrogPilot/Stock openpilot</div>
        <p class="toggle-control-text">
          Reset all toggles to default FrogPilot/stock openpilot settings.
        </p>
        <button class="toggle-control-button" disabled="${() => state.busy}" @click="${confirmResetDefault}">
          Reset Toggles to Default
        </button>
        <button class="toggle-control-button" disabled="${() => state.busy}" @click="${confirmResetStock}">
          Reset Toggles to Stock
        </button>
      </section>
    </div>
    ${() => state.showResetDefaultModal ? Modal({
        title: "Reset Toggles",
        message: "Are you sure you want to reset all toggles to their default FrogPilot values?",
        onConfirm: resetTogglesToDefault,
        onCancel: () => { state.showResetDefaultModal = false; },
        confirmText: "Reset to Default"
      }) : ""}
    ${() => state.showResetStockModal ? Modal({
        title: "Reset Toggles",
        message: "Are you sure you want to reset all toggles to stock openpilot values?",
        onConfirm: resetTogglesToStock,
        onCancel: () => { state.showResetStockModal = false; },
        confirmText: "Reset to Stock"
      }) : ""}
  `
}
