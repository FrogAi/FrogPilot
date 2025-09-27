/**
 * Format a number of seconds to a human readable string
 * @param {number} seconds
 * @param {"days"|"hours"|"minutes"} precision
 * @returns {string}
 */
export function formatSecondsToHuman(seconds, precision = "minutes") {
  const units = [
    { label: "days", value: Math.floor(seconds / 86400) },
    { label: "hours", value: Math.floor((seconds % 86400) / 3600) },
    { label: "minutes", value: Math.floor((seconds % 3600) / 60) }
  ]

  const slice = precision === "days" ? 1 : precision === "hours" ? 2 : 3
  return units
    .filter(u => u.value > 0)
    .slice(0, slice)
    .map(u => `${u.value} ${u.label}`)
    .join(", ")
}

/**
 * Parse a filename into a Date
 * Expected format: YYYY-MM-DD--HH-MM-SS
 * @param {string} filename
 * @returns {Date}
 */
export function parseErrorLogToDate(filename) {
  const [datePart, timePart] = filename.replace(".log", "").split("--")
  if (!datePart || !timePart) {
    throw new Error("Filename format invalid: " + filename)
  }

  const [year, month, day] = datePart.split("-")
  const [hour, minute, second] = timePart.split("-")

  return new Date(`${year}-${month}-${day}T${hour}:${minute}:${second}`)
}

/**
 * Capitalize the first character of a string
 * @param {string} str
 * @returns {string}
 */
export function upperFirst(str) {
  return str ? str[0].toUpperCase() + str.slice(1) : ""
}

/**
 * Show the sidebar (mobile)
 */
export function showSidebar() {
  const html = document.documentElement
  document.getElementById("sidebar")?.classList.add("visible")
  document.getElementById("sidebarUnderlay")?.classList.remove("hidden")
  html.classList.add("no_scroll")
}

/**
 * Hide the sidebar (mobile)
 */
export function hideSidebar() {
  const html = document.documentElement
  document.getElementById("sidebar")?.classList.remove("visible")
  document.getElementById("sidebarUnderlay")?.classList.add("hidden")
  html.classList.remove("no_scroll")
}
