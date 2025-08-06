import { html, reactive } from "https://esm.sh/@arrow-js/core"
import { Modal } from "/assets/components/modal.js"

export function ToggleControl () {
  const state = reactive({
    showResetDefaultModal: false,
    showResetStockModal: false,
  });

  const fileInput = document.createElement("input")
  fileInput.type = "file"
  fileInput.accept = ".json"
  fileInput.style.display = "none"
  fileInput.addEventListener("change", restoreToggles)
  document.body.appendChild(fileInput)

  async function backupToggles () {
    const response = await fetch("/api/toggles/backup", { method: "POST" })
    const blob = await response.blob()

    const downloadUrl = URL.createObjectURL(blob)
    const downloadLink = document.createElement("a")
    downloadLink.href = downloadUrl
    downloadLink.download = "toggle-backup.json"
    downloadLink.click()
    URL.revokeObjectURL(downloadUrl)
  }

  async function restoreToggles (event) {
    const uploadedFile = event.target.files[0]
    if (uploadedFile) {
      const fileContents = await uploadedFile.text()
      const toggleData = JSON.parse(fileContents)

      const response = await fetch("/api/toggles/restore", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(toggleData)
      })

      const result = await response.json()
      showSnackbar(result.message || "Toggles restored!")

      event.target.value = ""
    }
  }

  function confirmResetDefault () {
    state.showResetDefaultModal = true;
  }

  async function resetTogglesToDefault () {
    state.showResetDefaultModal = false;
    showSnackbar("Resetting toggles to their default values...");
    await new Promise(resolve => setTimeout(resolve, 3000));
    showSnackbar("Rebooting...");
    await new Promise(resolve => setTimeout(resolve, 3000));
    await fetch("/api/toggles/reset_default", { method: "POST" });
  }

  function confirmResetStock () {
    state.showResetStockModal = true;
  }

  async function resetTogglesToStock () {
    state.showResetStockModal = false;
    showSnackbar("Resetting toggles to stock openpilot values...");
    await new Promise(resolve => setTimeout(resolve, 3000));
    showSnackbar("Rebooting...");
    await new Promise(resolve => setTimeout(resolve, 3000));
    await fetch("/api/toggles/reset_stock", { method: "POST" });
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
        <button class="toggle-control-button" @click="${backupToggles}">Backup Toggles</button>
        <button class="toggle-control-button" @click="${triggerRestorePrompt}">Restore Toggles</button>
      </section>

      <section class="toggle-control-widget" style="margin-left: 1.5rem">
        <div class="toggle-control-title">Reset Toggles to Default FrogPilot/Stock openpilot</div>
        <p class="toggle-control-text">
          Reset all toggles to default FrogPilot/stock openpilot settings.
        </p>
        <button class="toggle-control-button" @click="${confirmResetDefault}">
          Reset Toggles to Default
        </button>
        <button class="toggle-control-button" @click="${confirmResetStock}">
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
