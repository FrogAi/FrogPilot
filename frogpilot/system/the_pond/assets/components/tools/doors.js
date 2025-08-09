import { html } from "https://esm.sh/@arrow-js/core"

export function DoorControl () {
  async function lockDoors () {
    const response = await fetch("/api/doors/lock", { method: "POST" })
    const result = await response.json()
    showSnackbar(result.message || "Doors locked!")
  }

  async function unlockDoors () {
    const response = await fetch("/api/doors/unlock", { method: "POST" })
    const result = await response.json()
    showSnackbar(result.message || "Doors unlocked!")
  }

  return html`
    <div class="door-control-wrapper">
      <section class="door-control-widget">
        <div class="door-control-title">Lock/Unlock Doors</div>
        <p class="door-control-text">
          Remotely lock or unlock your car doors using the buttons below.
        </p>
        <button class="door-control-button" @click="${lockDoors}">🔒 Lock Doors</button>
        <button class="door-control-button" @click="${unlockDoors}">🔓 Unlock Doors</button>
      </section>
    </div>
  `
}
