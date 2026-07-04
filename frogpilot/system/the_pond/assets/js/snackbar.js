function showSnackbar(msg, level, timeout = 3500) {
  let wrapper = document.getElementById("snackbar_wrapper")

  if (!wrapper) {
    wrapper = document.createElement("div")
    wrapper.id = "snackbar_wrapper"
    wrapper.setAttribute("role", "status")
    wrapper.setAttribute("aria-live", "polite")
    document.body.appendChild(wrapper)
  } else {
    if (!wrapper.hasAttribute("role")) wrapper.setAttribute("role", "status")
    if (!wrapper.hasAttribute("aria-live")) wrapper.setAttribute("aria-live", "polite")
  }

  if (wrapper.children.length >= 2) {
    const first = wrapper.children[0]
    first.style.opacity = 0
    setTimeout(() => first.remove(), 1000)
  }

  const snackbar = document.createElement("div")
  snackbar.textContent = msg
  snackbar.className = "snackbar show"
  snackbar.id = `snackbar_${Math.random().toString(36).slice(5)}`

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
