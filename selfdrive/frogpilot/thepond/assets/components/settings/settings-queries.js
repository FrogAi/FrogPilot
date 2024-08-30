/**
 * @typedef Setting
 * @prop {string} setting.key - The key of the setting (the filename on device)
 * @prop {string} setting.section - The settings section (replicating the device settings)
 * @prop {string} setting.title - Nice title for the setting
 * @prop {string | number} setting.value - The actual value. This might be a string or an int
 * @prop {"toggle"|"text"|"dropdown"|"select"} type - The type of the setting. Determines the rendering
 * @prop {string[]} [setting.options] - An optional list of available options to choose from.
 * @prop {Setting[]} subsettings - A list of settings that should be displayed as submenus.
 * @prop {Setting[]} toggles - An optional list of nested settings.
 */

/**
 * Getter for all settings
 * @returns { Promise<Setting[]> } All available settings
 */
export async function getSettings(ignoreCache = false) {
  if (!ignoreCache) {
    const cachedValue = localStorage.getItem("settings")
    if (cachedValue) {
      const cache = JSON.parse(cachedValue)
      if (new Date().getTime() - cache.time < 1000 * 60) {
        return cache.value
      }
    }
  }

  const response = await fetch(`/api/settings`, {
    headers: {
      Accept: "application/json",
    },
  })
  if (response.ok) {
    const data = await response.json()
    const settings = data

    const cache = {
      value: settings,
      time: new Date().getTime(),
    }

    console.log("Updating settings cache")
    localStorage.setItem("settings", JSON.stringify(cache))
    return settings
  }
}

/**
 * Setter for a specific settings key
 * @param {string} key
 * @param {string} value
 * @returns {Promise<boolean>} A promise that resolves to true or false if the saving succeeded
 */
export async function saveSetting(key, value) {
  const response = await fetch(`/api/settings/${key}`, {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify({ value }),
  })
  if (!response.ok) {
    const data = await response.json()
    console.log(`Failed to save setting for ${key}`)
    console.log(`Error message: ${data.message}`)
    showSnackbar(`Failed to save setting ${key}`)
    return false
  }
  //showSnackbar(`Saved ${key}`)
  await getSettings(true)
  return true
}
