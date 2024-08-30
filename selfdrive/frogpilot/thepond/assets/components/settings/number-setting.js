import { html, reactive } from "https://esm.sh/@arrow-js/core"
import { saveSetting } from "./settings-queries.js"
import { toggleCollapsed } from "./settings.js"

/**
 * An input setting for arbitrary text input. Will be rendered
 * as a text field (which will be password if the key contains "key")
 * and a save button next to it.
 * @param {Setting} setting
 * @param {boolean} isSubToggle
 */
export function NumberSetting(setting, isSubToggle = false) {
  const state = reactive({
    value: setting.value,
    loading: false,
  })

  let timeout = undefined

  async function saveValue(newValue) {
    state.value = newValue
    // Debounce the save to avoid saving on every key press
    clearTimeout(timeout)
    timeout = setTimeout(async () => {
      state.loading = true
      await saveSetting(setting.key, state.value)
      state.loading = false
    }, 500)
  }

  return html`
    <div class="setting  ${isSubToggle ? "subtoggle" : ""}" id="${setting.key}">
      <p class="title" @click="${toggleCollapsed}">${setting.title}</p>
      <div class="description collapsed">${setting.description}</div>
      <div class="numberInput">
        <button
          @click="${() => saveValue(state.value - 1)}" 
          disabled="${() => state.value === setting.min}"
          >-</button>
        <p>${() => Math.round(state.value)} ${setting.unit}</p>
        <button
          @click="${() => saveValue(state.value + 1)}" 
          disabled="${() => state.value === setting.max}"
          >+</button>
      </div>
      </button>
    </div>
  `
}
