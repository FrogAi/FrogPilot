import { html, reactive } from "https://esm.sh/@arrow-js/core"

export function ThemeToggle() {
  const state = reactive({
    theme: calculateSettingAsThemeString(),
  })

  setTheme(state.theme)

  function setTheme(theme) {
    console.log(
      `Setting theme to ${theme ?? state.theme === "dark" ? "light" : "dark"}`
    )
    state.theme = theme ?? (state.theme === "dark" ? "light" : "dark")

    // update theme attribute on HTML to switch theme in CSS
    document.querySelector("html").setAttribute("data-theme", state.theme)
    const color = getComputedStyle(document.documentElement).getPropertyValue(
      "--main-bg"
    ) // #999999
    document
      .querySelector('meta[name="theme-color"]')
      .setAttribute("content", color)

    // update in local storage
    localStorage.setItem("theme", state.theme)
  }

  return html`
    <div>
      <button
        id="theme_button"
        @click="${() => setTheme()}"
        type="button"
        data-theme-toggle
        aria-label="Change to light theme"
      >
        ${() =>
          state.theme === "dark"
            ? html`<i class="bi bi-brightness-high"></i>Light mode`
            : html`<i class="bi bi-moon-fill"></i>Dark mode`}
      </button>
    </div>
  `
}

/**
 * @returns {"dark"|"light"} The theme setting as a string
 */
function calculateSettingAsThemeString() {
  const localStorageTheme = localStorage.getItem("theme")
  if (localStorageTheme !== null) {
    return localStorageTheme
  }

  const systemSettingDark = window.matchMedia("(prefers-color-scheme: dark)")
  return systemSettingDark.matches ? "dark" : "light"
}
