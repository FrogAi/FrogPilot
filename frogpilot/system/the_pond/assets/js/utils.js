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

export function parseErrorLogToDate(filename) {
  const [datePart, timePart] = filename.replace(/\.(log|txt)$/, "").split("--")
  if (!datePart || !timePart) {
    throw new Error("Filename format invalid: " + filename)
  }

  const [year, month, day] = datePart.split("-")
  const [hour, minute, second] = timePart.split("-")

  const date = new Date(`${year}-${month}-${day}T${hour}:${minute}:${second}`)
  if (isNaN(date.getTime())) {
    throw new Error("Filename date invalid: " + filename)
  }
  return date
}

export function upperFirst(str) {
  return str ? str[0].toUpperCase() + str.slice(1) : ""
}

export function showSidebar() {
  const html = document.documentElement
  document.getElementById("sidebar")?.classList.add("visible")
  document.getElementById("sidebarUnderlay")?.classList.remove("hidden")
  html.classList.add("no_scroll")
  const btn = document.getElementById("menu_button")
  if (btn) { btn.setAttribute("aria-expanded", "true"); btn.setAttribute("aria-label", "Close menu") }
}

export function hideSidebar() {
  const html = document.documentElement
  document.getElementById("sidebar")?.classList.remove("visible")
  document.getElementById("sidebarUnderlay")?.classList.add("hidden")
  html.classList.remove("no_scroll")
  const btn = document.getElementById("menu_button")
  if (btn) { btn.setAttribute("aria-expanded", "false"); btn.setAttribute("aria-label", "Open menu") }
}
