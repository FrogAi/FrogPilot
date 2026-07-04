import { html, reactive } from "/assets/vendor/arrow.mjs"
import { Modal } from "/assets/components/modal.js";
import { fetchJson } from "/assets/js/api.js"

export function DoorControl () {
  const state = reactive({
    busy: false,
    showUnlockModal: false,
  });

  async function lockDoors () {
    state.busy = true;
    try {
      const result = await fetchJson("/api/doors/lock", { method: "POST" })
      showSnackbar((result && result.message) || "Doors locked!")
    } catch (e) {
      showSnackbar(e.message || "Failed to lock doors...", "error")
    } finally {
      state.busy = false;
    }
  }

  function confirmUnlock () {
    state.showUnlockModal = true;
  }

  async function unlockDoors () {
    state.showUnlockModal = false;
    state.busy = true;
    try {
      const result = await fetchJson("/api/doors/unlock", { method: "POST" })
      showSnackbar((result && result.message) || "Doors unlocked!")
    } catch (e) {
      showSnackbar(e.message || "Failed to unlock doors...", "error")
    } finally {
      state.busy = false;
    }
  }

  return html`
    <div class="door-control-wrapper">
      <section class="door-control-widget">
        <div class="door-control-title">Lock/Unlock Doors</div>
        <p class="door-control-text">
          Remotely lock or unlock your car doors using the buttons below.
        </p>
        <button class="door-control-button" disabled="${() => state.busy}" @click="${lockDoors}">🔒 Lock Doors</button>
        <button class="door-control-button" disabled="${() => state.busy}" @click="${confirmUnlock}">🔓 Unlock Doors</button>
      </section>
      ${() => state.showUnlockModal ? Modal({
          title: "Confirm Unlock",
          message: "Are you sure you want to unlock your car doors?",
          onConfirm: unlockDoors,
          onCancel: () => { state.showUnlockModal = false; },
          confirmText: "Unlock"
      }) : ""}
    </div>
  `
}
