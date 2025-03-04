import { html, reactive, watch } from "https://esm.sh/@arrow-js/core"
import {
  getCoordinatesFromSearch,
  getRoute,
  addRouteToMap,
  formatSecondsToHuman,
  formatMetersToHuman,
  removeRouteFromMap,
} from "./navigation-utils.js"

export function NavDestination() {
  // Random between 0 and 100
  const random = Math.floor(Math.random() * 100)
  let map
  let destinationMarker

  const state = reactive({
    mapboxToken: undefined,
    initialized: false,
    lastPosition: undefined,
    destination: undefined,
    route: undefined,
    suggestions: "[]",
    previousDestinations: "[]",
  })

  const searchFieldState = reactive({
    value: "",
  })

  const getNavigationData = async () => {
    const response = await fetch("/api/navigation")
    const data = await response.json()
    state.mapboxToken = data.mapboxToken.trim()
    try {
      state.lastPosition = JSON.parse(data.lastPosition.trim())
    } catch (e) {
      console.log("Failed to parse last position", data.lastPosition)
      state.lastPosition = { longitude: 0, latitude: 0 }
    }
    try {
      state.destination = JSON.parse(data.destination?.trim())
    } catch (e) {}
    try {
      state.suggestions = JSON.stringify(
        JSON.parse(data.previousDestinations)?.map((destination) => ({
          name: destination.place_name,
        }))
      )
      state.previousDestinations = state.suggestions
    } catch (e) {}
    setupMap()
  }

  async function searchInput(e) {
    searchFieldState.value = e.target.value
    if (window.searchTimeout) {
      clearTimeout(window.searchTimeout)
    }
    window.searchTimeout = setTimeout(async () => {
      const searchValue = e.target.value
      if (searchValue.length < 3) {
        console.log("Search value too short")
        return
      }
      // Remove any route
      state.route = undefined

      console.log("Searching for", searchValue)
      const lastPosition = `${state.lastPosition.longitude},${state.lastPosition.latitude}`
      const params = new URLSearchParams({
        proximity: lastPosition,
        access_token: state.mapboxToken,
        session_token: "sfsf",
        q: searchValue,
        limit: 4,
      })
      const response = await fetch(
        `https://api.mapbox.com/search/searchbox/v1/suggest?${params.toString()}`
      )
      const data = await response.json()
      const suggestions = JSON.stringify(data.suggestions)
      console.log("Got suggestions", suggestions)

      state.suggestions = suggestions
    }, 800)
  }

  const setupMap = async () => {
    if (state.mapboxToken === undefined) {
      console.log("Mapbox token not set")
      return
    }
    if (state.initialized === true) {
      console.log("Called setupMap, but already initialized")
      return
    }
    state.initialized = true

    console.log("Setting up map")
    mapboxgl.accessToken = state.mapboxToken

    map = new mapboxgl.Map({
      container: "map", // container ID
      center: [state.lastPosition.longitude, state.lastPosition.latitude], // starting position [lng, lat]
      zoom: 15, // starting zoom
      pitch: 45,
      attributionControl: false,
      logoPosition: "bottom-right",
      style: "mapbox://styles/mapbox/dark-v11",
    })

    // Add a destination and last position marker
    destinationMarker = new mapboxgl.Marker()
    new mapboxgl.Marker()
      .setLngLat([state.lastPosition.longitude, state.lastPosition.latitude])
      .addTo(map)

    map.on("style.load", async () => {
      // Insert the layer beneath any symbol layer.
      const layers = map.getStyle().layers
      const labelLayerId = layers.find(
        (layer) => layer.type === "symbol" && layer.layout["text-field"]
      ).id

      // The 'building' layer in the Mapbox Streets
      // vector tileset contains building height data
      // from OpenStreetMap.
      map.addLayer(
        {
          id: "add-3d-buildings",
          source: "composite",
          "source-layer": "building",
          filter: ["==", "extrude", "true"],
          type: "fill-extrusion",
          minzoom: 15,
          paint: {
            "fill-extrusion-color": "#aaa",

            // Use an 'interpolate' expression to
            // add a smooth transition effect to
            // the buildings as the user zooms in.
            "fill-extrusion-height": [
              "interpolate",
              ["linear"],
              ["zoom"],
              15,
              0,
              15.05,
              ["get", "height"],
            ],
            "fill-extrusion-base": [
              "interpolate",
              ["linear"],
              ["zoom"],
              15,
              0,
              15.05,
              ["get", "min_height"],
            ],
            "fill-extrusion-opacity": 0.6,
          },
        },
        labelLayerId
      )

      // If a destination is set, add it to the map
      console.log("Destination", state.destination)
      if (state.destination) {
        console.log("Destination set, rendering it")
        // Fetch the place, the get the route and add both to the map
        const coordinates = [
          state.destination.longitude,
          state.destination.latitude,
        ]

        // Destionation marker
        destinationMarker.setLngLat([coordinates[0], coordinates[1]]).addTo(map)

        const route = await getRoute(
          `${state.lastPosition.longitude},${state.lastPosition.latitude}`,
          `${coordinates[0]},${coordinates[1]}`,
          state.mapboxToken
        )
        addRouteToMap(
          map,
          [state.lastPosition.longitude, state.lastPosition.latitude],
          coordinates,
          route.geometry.coordinates
        )

        state.route = {
          name: state.destination.name,
          duration: route.duration,
          distance: route.distance,
          destinationCoordinates: coordinates,
          startingCoordinates: [
            state.lastPosition.longitude,
            state.lastPosition.latitude,
          ],
          confirmed: true,
        }
      }
    })
  }

  getNavigationData()

  console.log("NavDestination rendered ", random)

  return html`
    <h1>Navigation</h1>
    <div class="map-wrapper">
      <div class="search-wrapper">
        <input
          id="search-field"
          placeholder="Search here"
          value="${() => searchFieldState.value}"
          @input="${searchInput}"
        />
        <div id="infobox">
          ${() => {
            console.log(state.suggestions)
            if (state.route) {
              return NavigationDestination({
                ...state.route,
                map: map,
                cancelNavigationFn: () => {
                  state.route = undefined
                  state.suggestions = state.previousDestinations
                },
              })
            } else if (JSON.parse(state.suggestions).length > 0) {
              return SearchSuggestions({
                suggestions: JSON.parse(state.suggestions),
                mapboxToken: state.mapboxToken,
                map: map,
                lastPosition: state.lastPosition,
                destinationMarker,
                setRouteFn: (route) => {
                  state.route = route
                },
              })
            }
          }}
        </div>
      </div>
      <div id="map"></div>
    </div>
  `
}

/**
 * A component listing search suggestions below the search field
 * @param {string} suggestions An array of suggestions from mapbox
 * @param {string} suggestions[].full_address The full address of the suggestion
 * @param {string} suggestions[].name The name of the suggestion
 * @param {string} suggestions[].place_formatted The place formatted
 * @returns {HTMLElement}
 */
function SearchSuggestions({
  suggestions,
  mapboxToken,
  map,
  lastPosition,
  destinationMarker,
  setRouteFn,
}) {
  function formatSuggestion(suggestion) {
    return [suggestion.name, suggestion.place_formatted]
      .filter((it) => it !== undefined)
      .join(", ")
  }

  const clickHandler = async (suggestion) => {
    const searchQuery = formatSuggestion(suggestion)
    console.log("Clicked on", suggestion)
    // Update the search field with the suggestion
    const searchField = document.getElementById("search-field")
    searchField.value = searchQuery

    // Fetch the place, the get the route and add both to the map
    const coordinates = await getCoordinatesFromSearch(searchQuery, mapboxToken)
    // Destionation marker
    destinationMarker.setLngLat([coordinates[0], coordinates[1]]).addTo(map)

    const route = await getRoute(
      `${lastPosition.longitude},${lastPosition.latitude}`,
      `${coordinates[0]},${coordinates[1]}`,
      mapboxToken
    )
    console.log(coordinates)
    addRouteToMap(
      map,
      [lastPosition.longitude, lastPosition.latitude],
      coordinates,
      route.geometry.coordinates
    )

    setRouteFn({
      name: searchQuery,
      duration: route.duration,
      distance: route.distance,
      destinationCoordinates: coordinates,
      startingCoordinates: [lastPosition.longitude, lastPosition.latitude],
    })
  }

  const suggestionItem = (suggestion) => {
    return html` <span @click="${() => clickHandler(suggestion)}">
      <p>${formatSuggestion(suggestion)}</p>
    </span>`
  }

  return html`
    <div id="searchSuggestions">${suggestions.map(suggestionItem)}</div>
  `
}

/**
 * The component showing the navigation summary
 * @param {Object} navigation The navigation object
 * @param {string} navigation.name The name of the destination
 * @param {number} navigation.duration The duration of the navigation in seconds
 * @param {number} navigation.distance The distance of the navigation in meters
 * @param {boolean} navigation.confirmed If the navigation is confirmed
 * @param {number[]} navigation.coordinates The coordinates of the destination
 * @returns
 */
function NavigationDestination({
  name,
  duration,
  distance,
  confirmed,
  destinationCoordinates,
  startingCoordinates,
  map,
  cancelNavigationFn,
}) {
  const state = reactive({
    confirmed: confirmed ?? false,
  })

  async function confirmDestination() {
    console.log("Navigating to", name)
    showSnackbar("Navigation saved")
    state.confirmed = true
    const navDestination = {
      name,
      longitude: destinationCoordinates[0],
      latitude: destinationCoordinates[1],
    }
    console.log("Setting nav destination", navDestination)
    await fetch("/api/navigation", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(navDestination),
    })
  }

  async function cancelNavigation() {
    console.log("Cancelling navigation")
    showSnackbar("Navigation cancelled")
    state.confirmed = false
    removeRouteFromMap(map, startingCoordinates)
    cancelNavigationFn()
    await fetch("/api/navigation", {
      method: "DELETE",
    })
  }

  return html`
    <div id="navigationSummary">
      <p>${name}</p>
      <p>${formatSecondsToHuman(duration)}, ${formatMetersToHuman(distance)}</p>
      <div class="buttonCluster">
        ${() =>
          state.confirmed ?? false
            ? html`<button class="cancel" @click="${cancelNavigation}">
                <i class="bi bi-x-lg"></i> Cancel navigation
              </button>`
            : html`<button class="directions" @click="${confirmDestination}">
                <i class="bi bi-sign-turn-right"></i> Directions
              </button>`}

        <button class="favorite"><i class="bi bi-suit-heart-fill"></i></button>
      </div>
    </div>
  `
}
