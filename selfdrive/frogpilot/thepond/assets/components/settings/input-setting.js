import { html, reactive } from "https://esm.sh/@arrow-js/core"
import { saveSetting } from "./settings-queries.js"
import { toggleCollapsed } from "./settings.js"

/**
 * An input setting for arbitrary text input. Will be rendered
 * as a text field (which will be password if the key contains "key")
 * and a save button next to it.
 * @param {Setting} setting
 * @param {boolean} isSubsetting
 */
export function InputSetting(setting, isSubsetting = false) {
  const state = reactive({
    value: setting.value,
    loading: false,
  })

  const inputClass = setting.key.toLowerCase().includes("key") ? "password" : ""

  async function saveValue() {
    state.loading = true
    await saveSetting(setting.key, state.value)
    state.loading = false
  }

  return html`
    <div class="setting" id="${setting.key}">
      <p class="title" @click="${toggleCollapsed}">${setting.title}</p>
      <div class="description collapsed">${setting.description}</div>
      <input
        class="${inputClass + " textinput"}"
        autocomplete="off"
        type="text"
        value="${() => state.value}"
        @input="${(e) => (state.value = e.target.value)}"
      />
      <button
        class="${() => (state.loading ? "savebutton loading" : "savebutton")}"
        @click="${() => saveValue()}"
      >
        ${() => {
          if (state.loading) {
            return html`<i class="fa fa-circle-o-notch fa-spin"></i>`
          } else {
            return "Save"
          }
        }}
      </button>
    </div>
  `
}
