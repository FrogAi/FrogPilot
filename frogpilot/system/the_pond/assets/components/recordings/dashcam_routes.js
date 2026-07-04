import { html, reactive } from "/assets/vendor/arrow.mjs"
import { getOrdinalSuffix } from "/assets/components/navigation/navigation_utilities.js"
import { Modal } from "/assets/components/modal.js";
import { onRouteLeave } from "/assets/components/router.js"

const state = reactive({
  loading: true,
  error: null,
  routes: [],
  selectedRoute: null,
  showPreservedOnly: false,
  progress: 0,
  total: 0,
  showDeleteAllModal: false,
  isDeletingAll: false,
})

function formatRouteDate(dateString) {
  const date = new Date(dateString)
  if (isNaN(date.getTime())) {
    return dateString
  }
  const month = date.toLocaleString("en-US", { month: "long" })
  const day = date.getDate()
  const year = date.getFullYear()
  let hour = date.getHours()
  const minute = date.getMinutes()
  const ampm = hour >= 12 ? "pm" : "am"
  hour = hour % 12
  hour = hour || 12
  const minuteStr = minute < 10 ? "0" + minute : minute
  return `${month} ${day}${getOrdinalSuffix(day)}, ${year} - ${hour}:${minuteStr}${ampm}`
}

let routesController = null

async function fetchRoutes() {
  if (routesController) routesController.abort()
  const controller = new AbortController()
  routesController = controller
  try {
    const userTimezone = Intl.DateTimeFormat().resolvedOptions().timeZone;
    const response = await fetch(`/api/routes?timezone=${encodeURIComponent(userTimezone)}`, { signal: controller.signal });
    if (!response.ok) throw new Error();

    const reader = response.body.getReader();
    const decoder = new TextDecoder();
    let buffer = "";

    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      if (controller.signal.aborted) return;

      buffer += decoder.decode(value, { stream: true });
      const lines = buffer.split("\n\n");
      buffer = lines.pop();

      for (const line of lines) {
        if (line.startsWith("data: ")) {
          try {
            const data = JSON.parse(line.substring(6));
            if (data.progress !== undefined && data.total !== undefined) {
              state.progress = data.progress;
              state.total = data.total;
            }
            if (data.routes) {
              const routes = data.routes.map(route => ({
                ...route,
                timestamp: formatRouteDate(route.timestamp),
              }));
              state.routes.push(...routes);
            }
          } catch (e) {
            console.error("Failed to parse JSON:", e);
          }
        }
      }
    }
  } catch (_) {
    if (controller.signal.aborted) return;
    state.error = "Couldn't load routes. Please try again later..."
  } finally {
    if (routesController === controller) state.loading = false
  }
}

fetchRoutes()
onRouteLeave(() => { if (routesController) routesController.abort() })

function refresh() {
  if (routesController) routesController.abort()
  state.error = null
  state.loading = true
  state.routes = []
  fetchRoutes()
}

let overlay = null

function openDialog(template) {
  const o = document.createElement("div")
  o.className = "dialog-overlay"
  template(o)
  document.body.appendChild(o)
  const box = o.querySelector(".dialog-box")
  if (box) {
    box.setAttribute("role", "dialog")
    box.setAttribute("aria-modal", "true")
  }
  const focusTarget = o.querySelector("input") || o.querySelector('button, [href], [tabindex]:not([tabindex="-1"])')
  if (focusTarget) focusTarget.focus()
  o.addEventListener("keydown", e => {
    if (e.key === "Escape") {
      e.preventDefault()
      closeDialog(o)
      return
    }
    if (e.key === "Tab") {
      const focusable = o.querySelectorAll(
        'button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])'
      )
      if (!focusable.length) return
      const first = focusable[0]
      const last = focusable[focusable.length - 1]
      const active = document.activeElement
      if (e.shiftKey && active === first) {
        e.preventDefault()
        last.focus()
      } else if (!e.shiftKey && active === last) {
        e.preventDefault()
        first.focus()
      }
    }
  })
  return o
}

function closeDialog(o) {
  if (o) o.remove()
}

async function deleteRoute(route) {
  const onDelete = async () => {
    try {
      const res = await fetch(`/api/routes/${encodeURIComponent(route.name)}`, { method: "DELETE" })
      if (res.ok) {
        state.routes = state.routes.filter(r => r.name !== route.name)
        closeDialog(dlg)
        closeOverlay()
        refresh()
        showSnackbar("Route deleted!")
      } else {
        showSnackbar("Delete failed...", "error")
      }
    } catch {
      showSnackbar("Delete failed...", "error")
    }
  }
  const dlg = openDialog(html`
    <div class="dialog-box">
      <p>Delete “${() => route.timestamp}”?</p>
      <div class="dialog-buttons">
        <button class="btn btn-cancel" @click="${() => closeDialog(dlg)}">Cancel</button>
        <button class="btn btn-danger btn-del" @click="${onDelete}">Delete</button>
      </div>
    </div>`)
}

async function resetRouteName(route, dlg) {
  try {
    const res = await fetch(`/api/routes/clear_name`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ name: route.name })
    });
    if (res.ok) {
      const { timestamp } = await res.json();
      closeDialog(dlg);
      const routeInList = state.routes.find(r => r.name === route.name);
      if (routeInList) {
        routeInList.timestamp = formatRouteDate(timestamp);
      }
      route.timestamp = formatRouteDate(timestamp);
      const overlayTitleSpan = overlay?.querySelector(".media-player-title span");
      if (overlayTitleSpan) {
        overlayTitleSpan.textContent = formatRouteDate(timestamp);
      }
      showSnackbar("Route name reset!");
    } else {
      showSnackbar("Resetting name failed...", "error");
    }
  } catch {
    showSnackbar("Resetting name failed...", "error");
  }
}

async function renameRoute(route) {
  const onSave = async () => {
    const newName = dlg.querySelector(".rn-input").value.trim();
    if (!newName) return;
    try {
      const res = await fetch(`/api/routes/rename`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ old: route.name, new: newName })
      });
      if (res.ok) {
        closeDialog(dlg);
        const routeInList = state.routes.find(r => r.name === route.name);
        if (routeInList) {
          routeInList.timestamp = newName;
        }
        route.timestamp = newName;
        const overlayTitleSpan = overlay?.querySelector(".media-player-title span");
        if (overlayTitleSpan) {
          overlayTitleSpan.textContent = newName;
        }
        showSnackbar("Route renamed!");
      } else {
        showSnackbar("Rename failed...", "error");
      }
    } catch {
      showSnackbar("Rename failed...", "error");
    }
  };
  const dlg = openDialog(html`
    <div class="dialog-box">
      <p>Rename "${() => route.timestamp}"</p>
      <input class="rn-input" aria-label="New route name" value="${() => route.timestamp}" />
      <div class="dialog-buttons">
        <button class="btn-cancel" @click="${() => closeDialog(dlg)}">Cancel</button>
        <button class="btn-reset" @click="${() => resetRouteName(route, dlg)}">Reset</button>
        <button class="btn-save" @click="${onSave}">Save</button>
      </div>
    </div>`);
}

async function openOverlay(route) {
  if (overlay) return;

  overlay = document.createElement("div");
  overlay.className = "media-player-overlay";
  overlay.innerHTML = `
    <div class="media-player-content">
      <div class="media-player-title">
        <span></span>
        <button type="button" class="action-rename-icon" aria-label="Rename route"><i class="bi bi-pencil-fill"></i></button>
      </div>
      <video controls autoplay muted>
        <source type="video/mp4">
      </video>
      <div class="button-row">
        <button class="close-button action-close">Close</button>
        <button class="close-button camera-button active" data-camera="forward">Forward</button>
        <button class="close-button camera-button" data-camera="wide">Wide</button>
        <button class="close-button camera-button" data-camera="driver">Driver</button>
        <button class="close-button action-download">Download</button>
        <button class="close-button action-delete">Delete</button>
      </div>
    </div>`;
  overlay.querySelector(".media-player-title span").textContent = route.timestamp;
  overlay.querySelector("video source").setAttribute(
    "src",
    `/thumbnails/${encodeURIComponent(route.name)}--0/preview.png`
  );
  document.body.appendChild(overlay);
  overlay.setAttribute("role", "dialog");
  overlay.setAttribute("aria-modal", "true");
  overlay.setAttribute("aria-label", route.timestamp ? `Route: ${route.timestamp}` : "Route video");

  overlay.addEventListener("click", e => {
    if (e.target === overlay) closeOverlay();
  });
  overlay.addEventListener("keydown", e => {
    if (e.key === "Escape") { e.preventDefault(); closeOverlay(); return; }
    if (e.key === "Tab") {
      const f = overlay.querySelectorAll('button, [href], input, [tabindex]:not([tabindex="-1"])');
      if (!f.length) return;
      const first = f[0], last = f[f.length - 1], active = document.activeElement;
      if (e.shiftKey && active === first) { e.preventDefault(); last.focus(); }
      else if (!e.shiftKey && active === last) { e.preventDefault(); first.focus(); }
    }
  });
  overlay.querySelector(".action-close")?.focus();
  overlay.querySelector(".action-rename-icon").onclick = () => renameRoute(route);
  overlay.querySelector(".action-close").onclick = closeOverlay;
  overlay.querySelector(".action-delete").onclick = () => deleteRoute(route);

  const vid = overlay.querySelector("video");
  const downloadButton = overlay.querySelector(".action-download");

  let segments;
  let current = 0;
  let selectedCamera = "forward";

  const cameraButtons = overlay.querySelectorAll(".camera-button");
  cameraButtons.forEach(btn => { btn.disabled = true; });

  downloadButton.onclick = () => {
    const link = document.createElement("a");
    const videoPath = `/video/${encodeURIComponent(route.name)}/combined?camera=${selectedCamera}`;
    link.href = videoPath;
    link.download = `${route.timestamp}-${selectedCamera}.mp4`;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  (async () => {
    try {
      const response = await fetch(`/api/routes/${encodeURIComponent(route.name)}`);
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }
      const data = await response.json();
      segments = data.segment_urls;

      if (!segments || segments.length === 0) {
        segments = [`/video/${encodeURIComponent(route.name)}--0`];
      }
      vid.src = `${segments[0]}?camera=forward`;
      vid.load();
      vid.play();
      cameraButtons.forEach(btn => { btn.disabled = false; });
    } catch (error) {
      showSnackbar("Error: Could not load combined route video.", "error");
    }
  })();

  vid.addEventListener("ended", () => {
    if (!segments || !segments.length) return;
    current++;
    if (current < segments.length) {
      const videoPath = segments[current].includes("?") ? `${segments[current]}&camera=${selectedCamera}` : `${segments[current]}?camera=${selectedCamera}`
      vid.src = videoPath;
      vid.load();
      vid.play();
    }
  });

  overlay.querySelectorAll(".camera-button").forEach(button => {
    button.addEventListener("click", e => {
      if (!segments || !segments.length) return;
      overlay.querySelectorAll(".camera-button").forEach(btn => btn.classList.remove("active"));
      e.target.classList.add("active");
      selectedCamera = e.target.dataset.camera;
      vid.src = segments[current].includes("?") ? `${segments[current]}&camera=${selectedCamera}` : `${segments[current]}?camera=${selectedCamera}`;
      vid.load();
      vid.play();
    });
  });
}

function closeOverlay() {
  if (!overlay) return
  overlay.remove()
  overlay = null
  state.selectedRoute = null
}

async function togglePreserved(route, e) {
  e.stopPropagation()
  const newPreservedState = !route.is_preserved
  const method = newPreservedState ? "POST" : "DELETE"
  try {
    const response = await fetch(`/api/routes/${encodeURIComponent(route.name)}/preserve`, { method })
    if (response.ok) {
      route.is_preserved = newPreservedState
    } else {
      const errorData = await response.json()
      showSnackbar(errorData.error || "Failed to update preserved state...", "error")
    }
  } catch (_) {
    showSnackbar("An error occurred...", "error")
  }
}

async function deleteAllRoutes() {
  state.showDeleteAllModal = false
  state.isDeletingAll = true
  try {
    const res = await fetch("/api/routes/delete_all", { method: "DELETE" })
    if (!res.ok) throw new Error()
    await refresh()
    showSnackbar("All routes deleted!")
  } catch {
    showSnackbar("An error occurred while deleting all routes...", "error")
  } finally {
    state.isDeletingAll = false
  }
}

export function RouteRecordings() {
  onRouteLeave(closeOverlay)
  if (state.selectedRoute && !overlay) openOverlay(state.selectedRoute);

  return html`
    <div class="screen-recordings-wrapper">
      <div class="screen-recordings-widget">
        <div class="screen-recordings-title">Dashcam Routes</div>
        <button
          class="show-preserved-button"
          @click="${() => (state.showPreservedOnly = !state.showPreservedOnly)}"
          disabled="${() => state.loading && state.routes.length === 0}"
        >
          ${() => (state.showPreservedOnly ? "Show All" : "Show Only Preserved Routes")}
        </button>

        ${() => {
          const routesToShow = state.routes.filter(r => !state.showPreservedOnly || r.is_preserved);

          if (routesToShow.length === 0) {
            if (state.loading && state.total > 0) {
              return html`<p class="screen-recordings-message">Processing Routes: ${state.progress} of ${state.total}</p>`;
            }
            if (state.loading && !state.isDeletingAll) {
              return html`<p class="screen-recordings-message">Loading...</p>`;
            }
            if (state.isDeletingAll) {
              return html`<p class="screen-recordings-message">Deleting routes...</p>`;
            }
            if (state.showPreservedOnly) {
              return html`<p class="screen-recordings-message">No preserved routes...</p>`;
            }
            if (state.error) {
              return html`<p class="screen-recordings-message">${state.error}</p>`;
            }
            return html`<p class="screen-recordings-message">No routes found...</p>`;
          }

          return html`
            <div class="screen-recordings-grid">
              ${routesToShow.map(
                route => html`
                  <div
                    class="recording-card"
                    @mouseenter="${e => {
                      if (state.selectedRoute) return;

                      const card = e.currentTarget;
                      const gif = card.querySelector(".recording-preview-gif");
                      const png = card.querySelector(".recording-preview-png");

                      if (card.dataset.gifLoaded) {
                        png.style.display = "none";
                        gif.style.display = "block";
                        return;
                      }

                      card.dataset.loadingGif = "true";
                      const preloader = new Image();
                      preloader.onload = () => {
                        if (card.dataset.loadingGif === "true") {
                          gif.src = preloader.src;
                          png.style.display = "none";
                          gif.style.display = "block";
                          card.dataset.gifLoaded = true;
                        }
                        delete card.dataset.loadingGif;
                      };
                      preloader.onerror = () => {
                        console.error("Failed to load preview GIF:", preloader.src);
                        delete card.dataset.loadingGif;
                      };

                      preloader.src = gif.dataset.src;
                    }}"
                    @mouseleave="${e => {
                      const card = e.currentTarget;
                      card.querySelector(".recording-preview-png").style.display = "block";
                      card.querySelector(".recording-preview-gif").style.display = "none";
                      if (card.dataset.loadingGif === "true") {
                        delete card.dataset.loadingGif;
                      }
                    }}"
                    @click="${() => {
                      state.selectedRoute = route;
                    }}"
                  >
                    <button type="button" class="preserved-icon" aria-label="${() => (route.is_preserved ? "Remove preserved route" : "Preserve route")}" @click="${e => togglePreserved(route, e)}">
                      ${() => html`<i class="bi ${route.is_preserved ? "bi-heart-fill" : "bi-heart"}"></i>`}
                    </button>
                    <div class="recording-preview-container">
                      <img
                        src="${route.png}"
                        class="recording-preview recording-preview-png"
                        style="display:block;"
                        @error="${e => { e.target.style.visibility = "hidden" }}"
                      >
                      <img
                        data-src="${route.gif}"
                        class="recording-preview recording-preview-gif"
                        style="display:none;"
                      >
                    </div>
                    <p class="recording-filename">${() => route.timestamp}</p>
                  </div>
                `
              )}
            </div>
          `;
        }}
        ${() => {
          if (state.routes.length > 0) {
            return html`
              <button
                class="delete-all-button"
                @click="${() => (state.showDeleteAllModal = true)}"
                disabled="${() => state.isDeletingAll}"
              >
                ${() => (state.isDeletingAll ? "Deleting..." : "Delete All Routes")}
              </button>
            `;
          }
          return "";
        }}
      </div>
      ${() => state.showDeleteAllModal ? Modal({
          title: "Confirm Delete All",
          message: "Are you sure you want to delete all routes? This action cannot be undone...",
          onConfirm: deleteAllRoutes,
          onCancel: () => { state.showDeleteAllModal = false; },
          confirmText: "Delete All"
      }) : ""}
    </div>
  `;
}
