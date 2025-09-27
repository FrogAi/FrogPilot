function showSnackbar(msg, level, timeout = 3500) {
  const wrapper = document.getElementById("snackbar_wrapper")

  // Ensure max 2 snackbars are visible
  if (wrapper.children.length >= 2) {
    const first = wrapper.children[0]
    first.style.opacity = 0
    setTimeout(() => first.remove(), 1000)
  }

  const snackbar = document.createElement("div")
  snackbar.innerHTML = msg
  snackbar.className = "snackbar show"
  snackbar.id = `snackbar_${Math.random().toString(36).slice(5)}`

  if (level === "error") {
    snackbar.style.backgroundColor = "#f44336"
  }

  wrapper.appendChild(snackbar)

  setTimeout(() => {
    snackbar.style.opacity = 0
    setTimeout(() => snackbar.remove(), 1000)
  }, timeout)
}
