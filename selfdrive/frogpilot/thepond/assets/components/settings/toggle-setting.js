import { html, reactive } from "https://esm.sh/@arrow-js/core"
import { saveSetting } from "./settings-queries.js"
import { toggleCollapsed } from "./settings.js"
import { InputSetting } from "./input-setting.js"
import { SelectSetting } from "./select-setting.js"
import { NumberSetting } from "./number-setting.js"
import { Navigate } from "../router.js"

/**
 * A ToggleSetting component. This is for settings that can just be
 * set to "on" or "off", so it renders a toggle, or switch
 * @param {Setting} setting
 * @param {boolean} isSubToggle
 */
export function ToggleSetting(setting, isSubToggle = false) {
  const state = reactive({
    enabled: setting.value === 1 ? true : false,
    loading: false,
  })

  async function toggled() {
    const newState = !state.enabled
    state.enabled = newState
    state.loading = true
    const result = await saveSetting(setting.key, newState ? 1 : 0)
    if (!result) {
      state.enabled = !newState
    }
    state.loading = false
  }

  function Toggle() {
    return html` <label class="switch">
      <input
        class="checkbox"
        checked="${() => state.enabled}"
        type="checkbox"
        @click="${toggled}"
      />
      <span
        class="${() =>
          state.loading ? "loading slider round" : "slider round"}"
      ></span>
    </label>`
  }

  function SubMenu() {
    return html`<i class="bi bi-chevron-right switch"></i>`
  }

  const hasSubsettings = Object.keys(setting.subsettings ?? {}).length > 0
  function gotoSubsettings() {
    if (!hasSubsettings) return
    console.log("Goto subsettings", setting.key)
    Navigate(`/settings/${setting.section}/${setting.key}`)
  }

  // Setting expanded to true forces the description to expand by default,
  // and settings with subsettings acts as links so they should show description as well
  const shouldBeCollapsed = !hasSubsettings && !setting.expanded
  return html`
    <div
      class="setting ${isSubToggle ? "subtoggle" : ""} ${hasSubsettings
        ? "subsetting_link"
        : ""}"
      id="${setting.key}"
      @click="${gotoSubsettings}"
    >
      <p class="title wide" @click="${toggleCollapsed}">${setting.title}</p>
      <div class="description ${shouldBeCollapsed ? "collapsed" : ""}">
        ${setting.description}
      </div>
      ${hasSubsettings ? SubMenu() : Toggle()}
    </div>
    ${() =>
      state.enabled &&
      Object.values(setting.toggles ?? {})?.map((sub) => {
        switch (sub.type) {
          case "toggle":
            return ToggleSetting(sub, true).key(sub.key)
          case "text":
            return InputSetting(sub, true).key(sub.key)
          case "select":
            return SelectSetting(sub, true).key(sub.key)
          case "number":
            return NumberSetting(sub, true).key(sub.key)
        }
      })}
  `
}
