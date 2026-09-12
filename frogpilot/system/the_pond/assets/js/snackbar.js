function showSnackbar(msg, level, timeout = 3500) {
  const wrapper = document.getElementById("snackbar_wrapper")
  wrapper.setAttribute("role", "status")
  wrapper.setAttribute("aria-live", "polite")

  if (wrapper.children.length >= 2) {
    const first = wrapper.children[0]
    first.style.opacity = 0
    setTimeout(() => first.remove(), 1000)
  }

  const snackbar = document.createElement("div")
  snackbar.textContent = msg
  snackbar.className = "snackbar show"

  if (level === "error") {
    snackbar.style.backgroundColor = "#f44336"
    snackbar.setAttribute("role", "alert")
  } else if (level === "success") {
    snackbar.style.backgroundColor = "var(--success-bg)"
  }

  wrapper.appendChild(snackbar)

  setTimeout(() => {
    snackbar.style.opacity = 0
    setTimeout(() => snackbar.remove(), 1000)
  }, timeout)
}
