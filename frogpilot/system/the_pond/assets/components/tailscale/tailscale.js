import { html, reactive } from "/assets/vendor/arrow.mjs"
import { Modal } from "/assets/components/modal.js";
import { fetchJson } from "/assets/js/api.js"

export function isValidTailscaleAuthUrl(u) {
  if (typeof u !== "string" || u.length === 0) {
    return false
  }
  let parsed
  try {
    parsed = new URL(u)
  } catch {
    return false
  }
  return parsed.protocol === "https:" && parsed.hostname === "login.tailscale.com"
}

export function TailscaleControl() {
  const state = reactive({
    status: "checking",
    installed: false,
    showUninstallModal: false,
  });

  async function checkInstallStatus() {
    try {
      const response = await fetch("/api/tailscale/installed")
      if (!response.ok) {
        throw new Error(`Request failed with status ${response.status}`)
      }
      const result = await response.json()
      state.installed = result.installed
    } catch (error) {
      console.error("Failed to check Tailscale install status:", error)
      showSnackbar("Could not check Tailscale status", "error")
    } finally {
      if (state.status === "checking") state.status = "idle"
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
    const label = action.charAt(0).toUpperCase() + action.slice(1)
    try {
      const result = await fetchJson(endpoint, { method: "POST" })
      showSnackbar((result && result.message) || `${label} triggered...`)
      if (result && result.auth_url) {
        if (isValidTailscaleAuthUrl(result.auth_url)) {
          window.location.href = result.auth_url;
        } else {
          showSnackbar("Received an invalid Tailscale login URL. Not redirecting.", "error")
        }
      }
    } catch (e) {
      showSnackbar(e.message || `${label} failed...`, "error")
    } finally {
      await checkInstallStatus();
      state.status = "idle"
    }
  }

  checkInstallStatus()

  return html`
    <div class="tailscale-wrapper">
      <section class="tailscale-widget">
        <div class="tailscale-title">
          ${() => state.installed ? "Uninstall Tailscale" : "Install Tailscale"}
        </div>
        <p class="tailscale-text">
          Tailscale creates a secure, private connection between your openpilot device and your phone or PC so you can access and control it from anywhere!
        </p>
        <div class="tailscale-button-wrapper">
          <button
            class="tailscale-button"
            @click="${() => state.installed ? confirmUninstall() : handleAction()}"
            disabled="${() => state.status === "checking" || state.status === "installing" || state.status === "uninstalling"}"
          >
            ${() => {
              if (state.status === "checking") return "Checking..."
              if (state.status === "installing") return "Installing..."
              if (state.status === "uninstalling") return "Uninstalling..."
              if (state.installed) return "Uninstall"
              return "Install"
            }}
          </button>
          <a class="tailscale-link" href="https://tailscale.com/download" target="_blank" rel="noopener noreferrer">
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
