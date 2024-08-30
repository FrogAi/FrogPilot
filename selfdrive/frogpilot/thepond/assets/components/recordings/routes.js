import { html, reactive } from "https://esm.sh/@arrow-js/core"
import { Link } from "../router.js"

export function RecordedRoutes() {
  let state = reactive({
    routes: "[]",
  })

  async function fetchRoutes() {
    let response = await fetch("/api/routes")
    let routes = await response.json()
    state.routes = JSON.stringify(routes)
  }
  fetchRoutes()

  return html`
    <h1>Dashcam Routes</h1>
    <p>View & download recorded routes.</p>
    <div class="route_grid">
      ${() => {
        const routes = JSON.parse(state.routes)
        if (routes.length === 0) {
          fetchRoutes()
          return html`<div>Loading...</div>`
        }
        return routes.map((route) => {
          const formattedDate = new Date(route.date).toLocaleString()
          return html` ${Link(
            `/routes/${route.name}`,
            html`
              <div class="route_preview">
                <img src="${route.gif}" />
                <img class="image_preview" src="${route.png}" />
              </div>
              <p class="route_name">${formattedDate}</p>
            `,
            undefined,
            "route_card"
          )}`
        })
      }}
    </div>
  `
}
