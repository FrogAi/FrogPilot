import { html, reactive } from "https://esm.sh/@arrow-js/core"
import { Modal } from "/assets/components/modal.js";

export function NavKeys() {
  const state = reactive({
    initialMapboxComplete: false,
    showMapboxHelp: false,
    visible: false,

    imageVersion: 0,

    error: "",
    lastGroup: "",
    message: "",

    amap1Key: "", amap2Key: "",
    editA1: false, editA2: false,
    savedA1: false, savedA2: false,

    publicKey: "", secretKey: "",
    editPublic: false, editSecret: false,
    savedPublic: false, savedSecret: false,

    showDeleteModal: false,
    keyToDelete: null,
  })

  const bumpImageVersion = () => state.imageVersion++

  let clearTimer = null
  let fadeTimer = null

  function showMessage(type, text, group) {
    clearTimer && clearTimeout(clearTimer)
    fadeTimer && clearTimeout(fadeTimer)

    state.error = type === "error" ? text : ""
    state.message = type === "message" ? text : ""

    state.lastGroup = group

    state.visible = true

    clearTimer = setTimeout(() => { state.message = "", state.error = "" }, 5000)
    fadeTimer = setTimeout(() => state.visible = false, 5000)
  }

  const util = {
    prefix: (key, prefix) => key.startsWith(prefix) ? key : prefix ? prefix + key : key,

    mask: (key) => {
      if (!key) {
        return ""
      }

      const prefix = ["pk.", "sk."].find(p => key.startsWith(p)) || ""
      return prefix + "x".repeat(key.length - prefix.length)
    },

    req: async (url, opts) => {
      const response = await fetch(url, opts)
      return { ok: response.ok, data: await response.json().catch(() => ({})) }
    }
  }

  const meta = {
    amap1:  { prop: "amap1Key",  saved: "savedA1",     edit: "editA1",     prefix: "",    body: "amap1", minLength: 39  },
    amap2:  { prop: "amap2Key",  saved: "savedA2",     edit: "editA2",     prefix: "",    body: "amap2", minLength: 39  },
    public: { prop: "publicKey", saved: "savedPublic", edit: "editPublic", prefix: "pk.", body: "public", minLength: 80 },
    secret: { prop: "secretKey", saved: "savedSecret", edit: "editSecret", prefix: "sk.", body: "secret", minLength: 80 }
  }

  const canSave = (kind) => {
    const keyMeta = meta[kind];
    if (!keyMeta) return false;

    const value = state[keyMeta.prop]?.trim() || "";
    if (!value) return false;

    if (!state[keyMeta.saved]) {
      const fullValue = util.prefix(value, keyMeta.prefix);
      return fullValue.length >= keyMeta.minLength;
    }
    return false;
  }

  const getDeleteLabel = (kind) => {
    switch (kind) {
      case "amap1": return "Amap 1"
      case "amap2": return "Amap 2"
      case "public": return "Public Mapbox"
      case "secret": return "Secret Mapbox"
      default: return kind
    }
  }

  const api = {
    path: {
      key: "/api/navigation_key",
      nav: "/api/navigation"
    },

    load: async () => {
      const { ok, data } = await util.req(api.path.nav)
      if (!ok) {
        return showMessage("error", "Failed to load keys...", "")
      }

      state.amap1Key = data.amap1Key ?? ""
      state.amap2Key = data.amap2Key ?? ""
      state.savedA1 = !!state.amap1Key
      state.savedA2 = !!state.amap2Key

      state.publicKey = data.mapboxPublic ?? ""
      state.secretKey = data.mapboxSecret ?? ""
      state.savedPublic = !!state.publicKey
      state.savedSecret = !!state.secretKey

      state.initialMapboxComplete = state.savedPublic && state.savedSecret

      bumpImageVersion()
    },

    save: (kind) => async () => {
      const group = kind.startsWith("amap") ? "amap" : "mapbox"
      const keyMeta = meta[kind]
      const value = util.prefix(state[keyMeta.prop].trim(), keyMeta.prefix)

      const { ok, data } = await util.req(api.path.key, {
        body: JSON.stringify({ [keyMeta.body]: value }),
        headers: { "Content-Type": "application/json" },
        method: "POST"
      })

      if (!ok) {
        const input = document.getElementById(`${kind}-key`)
        if (input) {
          input.value = ""
          state[keyMeta.edit] = true
          state[keyMeta.saved] = false
          state[keyMeta.prop] = ""
          input.focus()
        }
        return showMessage("error", data.error || "Save failed...", group)
      }

      Object.assign(state, {
        [keyMeta.edit]: false,
        [keyMeta.saved]: true,
        [keyMeta.prop]: value
      })

      const input = document.getElementById(`${kind}-key`)
      if (input) {
        input.blur()
        input.value = ""
        requestAnimationFrame(() => { input.value = util.mask(state[keyMeta.prop]) })
      }

      if (group === "mapbox") {
        bumpImageVersion()
      }

      showMessage("message", data.message || "Saved!", group)
    },

    confirmDelete: (kind) => {
      state.keyToDelete = kind;
      state.showDeleteModal = true;
    },

    delete: async () => {
      const kind = state.keyToDelete;
      if (!kind) return;

      const group = kind.startsWith("amap") ? "amap" : "mapbox"
      const keyMeta = meta[kind]

      const { ok, data } = await util.req(`${api.path.key}?type=${kind}`, {
        method: "DELETE"
      })

      state.showDeleteModal = false;

      if (!ok) {
        return showMessage("error", data.error || "Delete failed...", group)
      }

      Object.assign(state, {
        [keyMeta.saved]: false,
        [keyMeta.prop]: ""
      })

      if (group === "mapbox") {
        state.initialMapboxComplete = false
        bumpImageVersion()
      }

      showMessage("message", data.message || "Deleted!", group)
    }
  }

  queueMicrotask(api.load)

  function renderGroup(title, kinds) {
    const isMapbox = title === "Mapbox Keys"

    return html`
      <div class="navkeys-group">
        <div class="navkeys-title">
          ${title}
          ${isMapbox ? html`
            <span class="navkeys-help-icon" @click="${() => state.showMapboxHelp = !state.showMapboxHelp}">
              <i class="bi bi-question-circle-fill"></i>
            </span>
          ` : ""}
        </div>

        ${kinds.map(kind => {
          const keyMeta = meta[kind]
          const label = kind[0].toUpperCase() + kind.slice(1).replace(/[0-9]/, d => " " + d)

          return html`
            <label class="navkeys-label" for="${kind}-key">${label} Key</label>
            <div class="navkeys-row">
              <input
                autocomplete="off"
                class="navkeys-input"
                id="${kind}-key"
                placeholder="${keyMeta.prefix || ""}xxxxxx..."
                value="${() => state[keyMeta.saved] ? util.mask(state[keyMeta.prop]) : state[keyMeta.prop]}"
                @keydown="${(e) => {
                  if (state[keyMeta.saved] && !state[keyMeta.edit]) {
                    state[keyMeta.edit] = true
                    state[keyMeta.saved] = false
                    state[keyMeta.prop] = ""
                    e.target.value = ""
                  }
                }}"
                @input="${(e) => state[keyMeta.prop] = e.target.value}"
              />
              <button
                class="${() => `navkeys-btn ${state[keyMeta.saved] ? "delete" : ""}`}"
                @click="${() => state[keyMeta.saved] ? api.confirmDelete(kind) : api.save(kind)()}"
                disabled="${() => !state[keyMeta.saved] && !canSave(kind)}">
                ${() => state[keyMeta.saved] ? "🗑️" : "💾"}
              </button>
            </div>
          `
        })}

        ${() => {
          if (isMapbox && state.showMapboxHelp) {
            return html`
              <div class="navkeys-help-img">
                <img
                  alt="Mapbox key setup guide"
                  src="${() => {
                    const bothKeysSet = state.savedPublic && state.savedSecret

                    let imageSource = "/mapbox-help/no_keys_set.png"
                    if (bothKeysSet) {
                      imageSource = state.initialMapboxComplete ? "/mapbox-help/setup_completed.png" : "/mapbox-help/both_keys_set.png"
                    } else if (state.savedPublic) {
                      imageSource = "/mapbox-help/public_key_set.png"
                    }
                    return `${imageSource}?v=${state.imageVersion}`
                  }}"
                />
              </div>
            `
          }
          return ""
        }}
      </div>
    `
  }

  function renderStatus(group) {
    return html`
      <div class="navkeys-status">
        <div
          class="navkeys-message"
          style="${() => state.lastGroup === group && state.message ? `opacity: ${state.visible ? 1 : 0}` : "opacity: 0"}">
          ${() => state.message}
        </div>
        <div
          class="navkeys-error"
          style="${() => state.lastGroup === group && state.error ? `opacity: ${state.visible ? 1 : 0}` : "opacity: 0"}">
          ${() => state.error}
        </div>
      </div>
    `
  }

  return html`
    <div class="navkeys-wrapper navkeys-offset-top">
      <div class="navkeys-container">
        ${renderGroup("AMap Keys", ["amap1", "amap2"])}
        ${renderStatus("amap")}
      </div>
      <div class="navkeys-container">
        ${renderGroup("Mapbox Keys", ["public", "secret"])}
        ${renderStatus("mapbox")}
      </div>
    </div>
    ${() => state.showDeleteModal ? Modal({
      title: "Confirm Delete",
      message: `Are you sure you want to delete your <strong>${getDeleteLabel(state.keyToDelete)}</strong> key?`,
      onConfirm: api.delete,
      onCancel: () => { state.showDeleteModal = false },
      confirmText: "Yes, Delete"
    }) : ""}
  `
}
