import { html, reactive } from "https://esm.sh/@arrow-js/core"
import FuzzySearch from "https://esm.sh/fuzzy-search"
import { getSettings } from "./settings-queries.js"
import { upperFirst } from "../../js/utils.js"
import { InputSetting } from "./input-setting.js"
import { SelectSetting } from "./select-setting.js"
import { ToggleSetting } from "./toggle-setting.js"
import { NumberSetting } from "./number-setting.js"
import { Link, Navigate } from "../router.js"

export function SettingsView({ params, searchQuery }) {
  // Random number between 0 and 100
  const random = Math.floor(Math.random() * 100)
  console.log("rendering settings 1 ", random)

  const state = reactive({
    selectedSection: params.section,
    selectedSubsection: params.subsection,
    settings: "[]",
    loading: true,
    heading: params.section,
    allSettings: "[]",
    isSearching: false,
  })

  getSettings().then((results) => {
    state.loading = false
    // If there is a subsection, try to find it, otherwise redirect to the first parent
    if (state.selectedSubsection) {
      const setting =
        results.find((item) => item.key === state.selectedSubsection) ?? {}
      if (Object.keys(setting.subsettings).length === 0) {
        console.log(
          `No subsettings found for ${state.selectedSubsection}, redirecting to parent`
        )
        return Navigate(`/settings/${state.selectedSection}`)
      }
      state.heading = setting.key
      state.settings = JSON.stringify([
        {
          ...setting,
          description:
            setting.description +
            "<br><br><b>Disabling this will make settings below irrelevant</b>",
          subsettings: undefined,
          expanded: true,
        },
        ...Object.entries(setting.subsettings).map(([key, value]) => {
          return { key, section: state.selectedSection, ...value }
        }),
      ])
      return
    }
    console.log("Updating settings state", random)
    state.settings = JSON.stringify(results)
    state.allSettings = state.settings

    if (searchQuery !== undefined) {
      SearchSettings(searchQuery)
    }
  })

  function SearchSettings(query) {
    if (JSON.parse(state.settings).length === 0) {
      return
    }
    if (query === "") {
      state.settings = state.allSettings
      state.isSearching = false
      return
    }
    console.log("Performing search", random)
    state.isSearching = true

    const search = query.toLowerCase()
    const settings = JSON.parse(state.allSettings)

    const searcher = new FuzzySearch(settings, ["title", "subsettings.title"], {
      caseSensitive: false,
    })
    const matchingSettings = searcher.search(search)
    // Check if the match was found in a subsetting, if so move that to the toggles object instaed

    const result = matchingSettings.map((setting) => {
      if (setting.subsettings) {
        const searcher = new FuzzySearch(setting.subsettings, ["title"], {
          caseSensitive: false,
        })
        const matchingSubsettings = searcher.search(search)
        if (matchingSubsettings.length > 0) {
          setting.toggles = matchingSubsettings
          setting.subsettings = undefined
        }
      }
      return setting
    })
    console.log("Resulting settings", result, random)
    state.settings = JSON.stringify(result)
  }

  console.log("rendering settings ", random)

  return html`
    <div class="settings" id="settingsWrapper">
      ${() =>
        state.selectedSubsection
          ? Link(
              `/settings/${state.selectedSection}`,
              "Back",
              undefined,
              "button"
            )
          : searchQuery === undefined
          ? html`
              <div class="searchBar">
                <input
                  class="searchfield"
                  type="text"
                  placeholder="Search settings (searches across all sections)"
                  @input="${(e) => SearchSettings(e.target.value)}"
                />
              </div>
            `
          : ""}
      <h1>
        ${() => (state.isSearching ? "Matching" : upperFirst(state.heading))}
        settings
      </h1>
      ${() => {
        const d = JSON.parse(state.settings).filter(
          (el) => el.section === state.selectedSection || state.isSearching
        )
        console.log("Rendering settings list", d, random)
        return html`<div>${SettingsList(d)}</div>`
      }}
    </div>
  `
}

function SettingsList(settings) {
  return settings.map((setting) => {
    switch (setting.type) {
      case "toggle":
        return ToggleSetting(setting).key(setting.key)
      case "text":
        return InputSetting(setting).key(setting.key)
      case "select":
      case "radio":
        return SelectSetting(setting).key(setting.key)
      case "number":
        return NumberSetting(setting).key(setting.key)
    }
  })
}

/**
 * Minor helper function to toggle the hidden class
 * on the description field when clicking the title
 * @param {Event} e
 */
export function toggleCollapsed(e) {
  const description =
    e.target.parentNode.getElementsByClassName("description")[0]
  description.classList.toggle("collapsed")
}
