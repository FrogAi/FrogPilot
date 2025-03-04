function showSnackbar(msg, level, timeout = 3000) {
  const wrapper = document.getElementById("snackbar_wrapper")
  // We want a max of 2 snackbars at a time
  if (wrapper.children.length >= 2) {
    wrapper.children[0].style.opacity = 0
    setTimeout(() => {
      wrapper.children[0].remove()
    }, 1000)
  }
  // Add another div inside the snackbar div
  const snackbarId = Math.random().toString(36).substring(5)
  const innerDiv = document.createElement("div")
  innerDiv.innerHTML = msg
  innerDiv.className = "snackbar show"
  if (level == "error") {
    innerDiv.style.backgroundColor = "#f44336"
  }
  innerDiv.id = snackbarId
  wrapper.appendChild(innerDiv)

  // After X seconds, remove the snackbardiv
  setTimeout(() => {
    const snackbar = document.getElementById(snackbarId)
    if (snackbar) {
      snackbar.style.opacity = 0
      setTimeout(() => {
        snackbar.remove()
      }, 1000)
    }
  }, timeout - 1000)
}
window.showSnackbar = showSnackbar
