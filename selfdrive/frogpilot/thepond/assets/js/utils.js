/**
 * Format a number of seconds to a human readable string
 * @param {number} seconds - The number of seconds to format
 * @param {"days"|"hours"|"minutes"} precision - The precision of the output, default is minutes
 * @returns {string} - The formatted string
 */
export function formatSecondsToHuman(seconds, precision = "minutes") {
  const days = Math.floor(seconds / 86400)
  seconds -= days * 86400
  const hours = Math.floor(seconds / 3600)
  seconds -= hours * 3600
  const minutes = Math.floor(seconds / 60)

  let result = []
  if (days > 0) {
    result.push(`${days} days`)
  }
  if (hours > 0) {
    result.push(`${hours} hours`)
  }
  if (minutes > 0) {
    result.push(`${minutes} minutes`)
  }
  let slice = precision === "days" ? 1 : precision === "hours" ? 2 : 3
  return result.slice(0, slice).join(", ")
}

/**
 * Parse a filename into a date
 * @param {string} filename - The filename to parse (format: YYYY-MM-DD--HH-MM-SS)
 * @returns {Date} - The parsed date
 */
export function parseErrorLogToDate(filename) {
  // Dateformat is YYYY-MM-DD--HH-MM-SS
  const date = filename.split("--")[0]
  const time = filename.split("--")[1]
  // Parse it into a date
  const d = new Date()
  d.setFullYear(date.slice(0, 4))
  d.setMonth(parseInt(date.slice(5, 7)) - 1) // Months are 0-indexed
  d.setDate(date.slice(8, 10))
  d.setHours(time.slice(0, 2))
  d.setMinutes(time.slice(3, 5))
  d.setSeconds(time.slice(6, 8))
  return d
}

/**
 * Converts the first char of a string to upper case
 * @param {string} str
 */
export function upperFirst(str) {
  return str[0].toUpperCase() + str.slice(1)
}

/**
 * Opens the sidebar on mobile devices
 */
export function showSidebar() {
  const sidebarUnderlay = document.getElementById("sidebarUnderlay")
  const sidebar = document.getElementById("sidebar")
  const html = document.getElementsByTagName("html")[0]
  sidebar.classList.add("visible")
  sidebarUnderlay.classList.remove("hidden")
  html.classList.add("no_scroll")
}

/**
 * Closes the sidebar menu on mobile devices
 */
export function hideSidebar() {
  const sidebarUnderlay = document.getElementById("sidebarUnderlay")
  const sidebar = document.getElementById("sidebar")
  const html = document.getElementsByTagName("html")[0]
  sidebar.classList.remove("visible")
  sidebarUnderlay.classList.add("hidden")
  html.classList.remove("no_scroll")
}
