import { html, reactive } from "/assets/vendor/arrow.mjs"
import { createBrowserHistory, createRouter } from "/assets/vendor/remix-router.js"
import { hideSidebar } from "/assets/js/utils.js"
import { DoorControl } from "/assets/components/tools/doors.js"
import { ErrorLogs } from "/assets/components/tools/error_logs.js"
import { Home } from "/assets/components/home/home.js"
import { NavDestination } from "/assets/components/navigation/navigation_destination.js"
import { NavKeys } from "/assets/components/navigation/navigation_keys.js"
import { RouteRecordings } from "/assets/components/recordings/dashcam_routes.js"
import { ScreenRecordings } from "/assets/components/recordings/screen_recordings.js"
import { Sidebar } from "/assets/components/sidebar.js"
import { SpeedLimits } from "/assets/components/tools/speed_limits.js"
import { TailscaleControl } from "/assets/components/tailscale/tailscale.js"
import { ThemeMaker } from "/assets/components/tools/theme_maker.js"
import { TmuxLog } from "/assets/components/tools/tmux.js"
import { ToggleControl } from "/assets/components/tools/toggles.js"
import { TSKManager } from "/assets/components/tools/tsk_manager.js"

let router, routerState

function _teardownStore() {
  if (!_teardownStore.list) _teardownStore.list = []
  return _teardownStore.list
}
export function onRouteLeave(fn) {
  _teardownStore().push(fn)
}
function _runTeardowns() {
  const list = _teardownStore()
  while (list.length) {
    try { list.pop()() } catch (e) { console.error("teardown error:", e) }
  }
}

function createRoute(id, path, component) {
  return {
    id,
    path,
    loader: () => {},
    element: component,
  }
}

function Root() {
  let routes = [
    createRoute("doors", "/lock_or_unlock_doors", DoorControl),
    createRoute("errorLogs", "/manage_error_logs", ErrorLogs),
    createRoute("navdestination", "/set_navigation_destination", NavDestination),
    createRoute("navkeys", "/manage_navigation_keys", NavKeys),
    createRoute("root", "/", Home),
    createRoute("routes", "/dashcam_routes", RouteRecordings),
    createRoute("screen_recordings", "/screen_recordings", ScreenRecordings),
    createRoute("speed_limits", "/download_speed_limits", SpeedLimits),
    createRoute("tailscale", "/manage_tailscale", TailscaleControl),
    createRoute("thememaker", "/theme_maker", ThemeMaker),
    createRoute("tmux", "/manage_tmux", TmuxLog),
    createRoute("toggles", "/manage_toggles", ToggleControl),
    createRoute("tsk_manager", "/tsk_manager", TSKManager),
  ]

  router = createRouter({
    routes,
    history: createBrowserHistory(),
  }).initialize()

  routerState = reactive({
    activePath: "/",
    activePathFull: "/",
    initialized: false,
    navigation: { state: "loading" },
    errors: [],
    params: {},
  })

  let prevPath = null
  router.subscribe(({ initialized, navigation, matches, errors }) => {
    const [match] = matches
    if (prevPath !== null && prevPath !== match.route.path) {
      _runTeardowns()
    }
    prevPath = match.route.path
    Object.assign(routerState, {
      initialized,
      activePath: match.route.path,
      activePathFull: match.pathname,
      navigation,
      params: match.params,
      errors,
    })
  })

  const onroadState = reactive({ onroad: false })
  async function pollOnroad() {
    try {
      onroadState.onroad = (await (await fetch("/api/onroad")).json()).onroad
    } catch {
    }
  }
  pollOnroad()
  setInterval(pollOnroad, 1000)

  return html`
    ${() => Sidebar(routerState.activePathFull)}
    <div class="content">
      ${() => {
        if (!routerState.initialized || routerState.navigation.state === "loading") {
          return html`<div>Loading...</div>`
        }

        if (routerState.errors && Object.values(routerState.errors).some(e => e?.status === 404)) {
          return html`<h1>Not Found</h1>`
        }

        const match = routes.find(r => r.path === routerState.activePath)
        if (!match) {
          return html`<h1>Not Found</h1>`
        }
        return match.element({ params: routerState.params })
      }}
    </div>
    ${() => {
      if (!onroadState.onroad) return ""
      return html`
      <div class="onroad-overlay" role="dialog" aria-modal="true" aria-labelledby="onroad-title">
        <div class="onroad-message">
          <i class="bi bi-car-front-fill onroad-icon" aria-hidden="true"></i>
          <h1 id="onroad-title">The Pond is locked while driving</h1>
          <p>Shift into Park to use The Pond.</p>
        </div>
      </div>`
    }}
  `
}

export function Link(href, children, onClick, classes = "", ariaCurrent = null) {
  return html`<a
    class="${classes}"
    href="${() => href}"
    aria-current="${() => ariaCurrent || false}"
    @click="${(e) => {
      e.preventDefault()
      router.navigate(e.currentTarget.href)
      hideSidebar()
      onClick?.()
    }}"
  >${children}</a>`
}

export function Navigate(href) {
  router.navigate(href)
  window.scrollTo(0, 0)
}

Root()(document.getElementById("app"))
