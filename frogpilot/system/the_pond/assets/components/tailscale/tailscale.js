import { html, reactive } from "https://esm.sh/@arrow-js/core"
import { Modal } from "/assets/components/modal.js";

export function TailscaleControl() {
  const state = reactive({
    status: "idle",
    installed: false,
    showUninstallModal: false,
  });

  async function checkInstallStatus() {
    try {
      const response = await fetch("/api/tailscale/installed")
      const result = await response.json()
      state.installed = result.installed
    } catch (error) {
      console.error("Failed to check Tailscale install status:", error)
    }
  }

  function confirmUninstall() {
    state.showUninstallModal = true;
  }

  async function handleAction() {
    if (state.status !== "idle") {
      return
    }

    const action = state.installed ? "uninstall" : "install"
    state.status = state.installed ? "uninstalling" : "installing"

    state.showUninstallModal = false;

    showSnackbar(`${action.charAt(0).toUpperCase() + action.slice(1)} started...`)

    const endpoint = state.installed ? "/api/tailscale/uninstall" : "/api/tailscale/setup"
    const response = await fetch(endpoint, { method: "POST" })
    const result = await response.json()

    showSnackbar(result.message || `${action.charAt(0).toUpperCase() + action.slice(1)} triggered...`)

    if (result.auth_url) {
      window.location.href = result.auth_url;
    }

    // Refresh install status after action
    await checkInstallStatus();
    state.status = "idle"
  }

  checkInstallStatus()

  return html`
    <div class="tailscale-wrapper">
      <section class="tailscale-widget">
        <div class="tailscale-title">
          ${() => state.installed ? 'Uninstall Tailscale' : 'Install Tailscale'}
        </div>
        <p class="tailscale-text">
          Tailscale creates a secure, private connection between your openpilot device and your phone or PC so you can access and control it from anywhere!
        </p>
        <div class="tailscale-button-wrapper">
          <button
            class="tailscale-button"
            @click="${() => state.installed ? confirmUninstall() : handleAction()}"
            disabled="${() => state.status === 'installing' || state.status === 'uninstalling'}"
          >
            ${() => {
              if (state.status === 'installing') return 'Installing...'
              if (state.status === 'uninstalling') return 'Uninstalling...'
              if (state.installed) return 'Uninstall'
              return 'Install'
            }}
          </button>
          <a class="tailscale-link" href="https://tailscale.com/download" target="_blank">
            Download Tailscale on your other devices
          </a>
        </div>
      </section>
      ${() => state.showUninstallModal ? Modal({
          title: "Confirm Uninstall",
          message: "Are you sure you want to uninstall Tailscale?",
          onConfirm: handleAction,
          onCancel: () => { state.showUninstallModal = false; },
          confirmText: "Uninstall"
      }) : ""}
    </div>
  `
}
