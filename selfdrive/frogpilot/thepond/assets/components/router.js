import {
  createBrowserHistory,
  createRouter,
} from "https://esm.sh/@remix-run/router@1.3.1"
import { html, reactive } from "https://esm.sh/@arrow-js/core"
import { Sidebar } from "./sidebar.js"
import { Overview } from "./overview.js"
import { SettingsView } from "./settings/settings.js"
import { NavDestination } from "./navigation/nav-destination.js"
import { ErrorLogs } from "./error-logs.js"
import { RecordedRoutes } from "./recordings/routes.js"
import { RecordedRoute } from "./recordings/route.js"
import { hideSidebar } from "../js/utils.js"

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
    createRoute("root", "/", Overview),
    createRoute("routes", "/routes", RecordedRoutes),
    createRoute("route", "/routes/:routeDate", RecordedRoute),
    createRoute("settings", "/settings/:section/:subsection?", SettingsView),
    createRoute("navdestination", "/navigation", NavDestination),
    createRoute("errorLogs", "/error-logs", ErrorLogs),
  ]

  router = createRouter({
    routes,
    history: createBrowserHistory(),
  }).initialize()

  routerState = reactive({
    //state: router.state,
    activePath: "/",
    activePathFull: "/",
    initialized: false,
    navigation: {
      state: "loading",
    },
    errors: [],
    params: {},
  })

  router.subscribe((state) => {
    console.log("Router state updated", state)
    routerState.initialized = state.initialized
    routerState.activePath = state.matches[0].route.path
    routerState.activePathFull = state.matches[0].pathname
    routerState.navigation.state = state.navigation.state
    routerState.params = state.matches[0].params
    routerState.errors = state.errors
    //routerState.state = state
  })

  return html` ${() => Sidebar(routerState.activePathFull)}
    <div class="content">
      ${() => {
        if (
          routerState.initialized === false ||
          routerState.navigation.state === "loading"
        ) {
          return html`<div>Loading...</div>`
        } else if (routerState?.errors?.root?.status === 404) {
          return html`<h1>Not Found</h1>`
        } else {
          console.log("re-render", JSON.stringify(routerState))
          const element = routes.find(
            (route) => route.path === routerState.activePath
          )
          return element.element({ params: routerState.params })
        }
      }}
    </div>`
}

export function Link(href, children, onClick, classes = "") {
  return html`<a
    class="${classes}"
    href="${() => href}"
    @click="${(e) => {
      e.preventDefault()
      router.navigate(e.currentTarget.href)
      hideSidebar()
      if (onClick !== undefined) {
        onClick()
      }
    }}"
    >${children}</a
  >`
}

export function Navigate(href) {
  router.navigate(href)
  // since we change the page, scroll to the top of the page
  window.scrollTo(0, 0)
}

Root()(document.getElementById("app"))
