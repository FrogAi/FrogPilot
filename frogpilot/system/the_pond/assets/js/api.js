export async function fetchJson(url, opts = {}) {
  const res = await fetch(url, opts)
  if (!res.ok) {
    let detail = ""
    try { detail = (await res.json()).error || "" } catch {}
    const err = new Error(detail || `Request failed (${res.status})`)
    err.status = res.status
    throw err
  }
  return res.status === 204 ? null : res.json()
}

export function downloadBlob(blob, filename) {
  const url = URL.createObjectURL(blob)
  const a = document.createElement("a")
  a.href = url
  a.download = filename
  document.body.appendChild(a)
  a.click()
  a.remove()
  URL.revokeObjectURL(url)
}
