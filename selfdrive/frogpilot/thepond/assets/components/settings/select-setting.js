import { html, reactive } from "https://esm.sh/@arrow-js/core"
import { saveSetting } from "./settings-queries.js"
import { toggleCollapsed } from "./settings.js"

/**
 * Component for multi-choice options with up to 3 options.
 * Will be rendered as selectable radio buttons.
 * @param {Setting} setting
 * @param {boolean} isSubsetting
 */
export function SelectSetting(setting, isSubsetting = false) {
  const state = reactive({
    value: setting.value,
    loading: false,
    selectedOption: setting.options?.[setting.value] ?? "",
  })

  async function saveValue(option) {
    state.selectedOption = option
    const value = setting.options.indexOf(option)
    state.value = value
    state.loading = true
    await saveSetting(setting.key, value)
    state.loading = false
  }

  function createRadioButtons(options) {
    return html` <div class="options">
      ${options.map(
        (option) => html`
          <input
            type="radio"
            name="${setting.title}"
            value="${option}"
            id="${option}"
            @click="${() => saveValue(option)}"
            checked="${() => state.selectedOption === option}"
          />
          <label for="${option}">
            ${() =>
              state.loading && state.selectedOption == option
                ? html`<i class="fa fa-circle-o-notch fa-spin"></i>`
                : option}
          </label>
        `
      )}
    </div>`
  }

  function createDropdown(options) {
    const name = `select - ${setting.key}`
    if (options === undefined || options.length === 0) {
      return html`<p>Options not found</p>`
    }
    return html` <div class="dropdown">
      <select @change="${(e) => saveValue(e.target.value)}" name="${name}">
        ${options.map(
          (option) =>
            html` <option
              value="${option}"
              selected="${() => state.value === options.indexOf(option)}"
            >
              ${option}
            </option>`
        )}
      </select>
    </div>`
  }

  return html`
    <div class="setting" id="${setting.key}">
      <p class="title" @click="${toggleCollapsed}">${setting.title}</p>
      <div class="description collapsed">${setting.description}</div>
      ${setting.type === "select"
        ? createDropdown(setting.options)
        : createRadioButtons(setting.options)}
    </div>
  `
}
