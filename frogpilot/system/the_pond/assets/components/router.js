import { html, reactive } from "https://esm.sh/@arrow-js/core"
import { createBrowserHistory, createRouter } from "https://esm.sh/@remix-run/router@1.3.1"
import { hideSidebar } from "/assets/js/utils.js"
import { DoorControl } from "/assets/components/tools/doors.js"
import { ErrorLogs } from "/assets/components/tools/error_logs.js"
import { Home } from "/assets/components/home/home.js"
import { NavDestination } from "/assets/components/navigation/navigation_destination.js"
import { NavKeys } from "/assets/components/navigation/navigation_keys.js"
import { RouteRecordings } from "/assets/components/recordings/dashcam_routes.js"
import { SettingsView } from "/assets/components/settings.js"
import { ScreenRecordings } from "/assets/components/recordings/screen_recordings.js"
import { Sidebar } from "/assets/components/sidebar.js"
import { SpeedLimits } from "/assets/components/tools/speed_limits.js"
import { TailscaleControl } from "/assets/components/tailscale/tailscale.js"
import { TmuxLog } from "/assets/components/tools/tmux.js"
import { ToggleControl } from "/assets/components/tools/toggles.js"
import { TSKManager } from "/assets/components/tools/tsk_manager.js"

let router, routerState

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
    createRoute("settings", "/settings/:section/:subsection?", SettingsView),
    createRoute("speed_limits", "/download_speed_limits", SpeedLimits),
    createRoute("tailscale", "/manage_tailscale", TailscaleControl),
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

  router.subscribe(({ initialized, navigation, matches, errors }) => {
    const [match] = matches
    Object.assign(routerState, {
      initialized,
      activePath: match.route.path,
      activePathFull: match.pathname,
      navigation,
      params: match.params,
      errors,
    })
  })

  return html`
    ${() => Sidebar(routerState.activePathFull)}
    <div class="content">
      ${() => {
        if (!routerState.initialized || routerState.navigation.state === "loading") {
          return html`<div>Loading...</div>`
        }

        if (routerState.errors?.root?.status === 404) {
          return html`<h1>Not Found</h1>`
        }

        const match = routes.find(r => r.path === routerState.activePath)
        return match.element({ params: routerState.params })
      }}
    </div>
  `
}

export function Link(href, children, onClick, classes = "") {
  return html`<a
    class="${classes}"
    href="${() => href}"
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
