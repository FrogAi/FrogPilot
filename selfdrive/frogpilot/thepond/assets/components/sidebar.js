import { html, reactive } from "https://esm.sh/@arrow-js/core"
import { Link } from "./router.js"
import { upperFirst } from "../js/utils.js"
import { hideSidebar, showSidebar } from "../js/utils.js"
import { ThemeToggle } from "./theme-toggle.js"

const MenuItems = {
  recordings: [
    {
      name: "Routes",
      link: "/routes",
      icon: "bi-camera-reels",
    },
    {
      name: "Saved routes",
      link: "/saved_routes",
      icon: "bi-box2-heart",
      unimplemented: true,
    },
    {
      name: "Screen recordings",
      link: "/screen_recordings",
      icon: "bi-record-circle",
      unimplemented: true,
    },
  ],
  settings: [
    {
      name: "Device",
      link: "/settings/device",
      icon: "bi-phone",
      unimplemented: true,
    },
    {
      name: "Toggles",
      link: "/settings/toggles",
      icon: "bi-sliders",
    },
    {
      name: "Software",
      link: "/settings/software",
      icon: "bi-code-slash",
      unimplemented: true,
    },
    {
      name: "Controls",
      link: "/settings/controls",
      icon: "bi-gear-wide-connected",
    },
    {
      name: "Navigation",
      link: "/settings/navigation",
      icon: "bi-sign-turn-left",
    },
    {
      name: "Vehicles",
      link: "/settings/vehicles",
      icon: "bi-car-front",
    },
    {
      name: "Visuals",
      link: "/settings/visuals",
      icon: "bi-view-stacked",
      unimplemented: true,
    },
    {
      name: "Error logs",
      link: "/error-logs",
      icon: "bi-exclamation-triangle",
    },
  ],
  navigation: [
    {
      name: "Set destination",
      link: "/navigation",
      icon: "bi-globe-americas",
    },
  ],
}

export function Sidebar(activePath) {
  const activeRoute = Object.values(MenuItems)
    .flat()
    .find((item) => item.link === activePath)?.name

  const state = reactive({
    activeRoute: activeRoute ?? "",
  })

  function navigate(link) {
    state.activeRoute = link.name
    // since we change the page, scroll to the top of the page
    window.scrollTo(0, 0)
    // Since we navigated, hide the sidebar
    hideSidebar()
  }

  return html`
    <div id="sidebarUnderlay" class="hidden">
    </div>
    <div id="sidebar" class="sidebar">
      <div>
        <div class="title">
            <div>
              ${Link(
                "/",
                html`
                  <img
                    style="width: 50px;height: 50px;"
                    src="/assets/images/frog.png"
                  />
                  <p>ThePond</p>
                  `
              )}
            </div>
            <div>
              <a href="https://github.com/Aidenir">by Aidenir</a>
            </div>
        </div>
        <hr>
        ${() =>
          Object.entries(MenuItems).map(([section, links]) => {
            return html` <ul class="menu_section">
              <li>
                <a href="">
                  <span>${upperFirst(section)}</span>
                </a>
                <ul id="${section}">
                  ${() =>
                    links.map((link) => {
                      let classes = []
                      if (link.unimplemented) {
                        classes.push("not_implemented")
                      }
                      if (state.activeRoute === link.name) {
                        classes.push("active")
                      }
                      return html` <li class="${classes.join(" ")}">
                        <i class="${"bi " + link.icon}"></i>
                        ${link.unimplemented === true
                          ? html`<a href="#">${upperFirst(link.name)}</a>`
                          : Link(link.link, upperFirst(link.name), () =>
                              navigate(link)
                            )}
                      </li>`
                    })}
                </ul>
              </li>
            </ul>`
          })}
      </div>
      ${ThemeToggle()}
    </div>`
}

/**
 * Helper function run on startup that hooks up the menu button
 */
function setupMenuButton() {
  const sidebarUnderlay = document.getElementById("sidebarUnderlay")
  const sidebar = document.getElementById("sidebar")

  const button = document.getElementById("menu_button")
  button.addEventListener("click", () => {
    const isVisible = sidebar.classList.contains("visible")
    if (isVisible) {
      hideSidebar()
    } else {
      showSidebar()
    }
  })
  sidebarUnderlay.addEventListener("click", hideSidebar)
}

document.addEventListener("DOMContentLoaded", setupMenuButton, false)
