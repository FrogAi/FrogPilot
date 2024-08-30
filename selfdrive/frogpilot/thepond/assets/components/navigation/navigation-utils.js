/**
 * Get the coordinates for a search value
 * @param {string} searchValue The value to search for, can be anything
 * @param {string} mapboxToken The mapbox token to use for the search
 * @returns {number[]} [longitude, latitude]
 */
export async function getCoordinatesFromSearch(searchValue, mapboxToken) {
  const params = new URLSearchParams({
    access_token: mapboxToken,
    q: searchValue,
  })
  const response = await fetch(
    `https://api.mapbox.com/search/geocode/v6/forward?${params.toString()}`
  )
  const data = await response.json()
  //console.log("Got data", data)
  const coordinates = data.features[0].geometry.coordinates
  //console.log("Coordinates", coordinates)
  return coordinates
}

/**
 * Get a route from point A to point B
 * @param {string} from The starting point of the route, in the format "longitude, latitude"
 * @param {string} to The destination of the route, in the format "longitude, latitude"
 * @param {string} mapboxToken The mapbox token to use for the search
 * @returns {{ distance: number, duration: number, geometry: { type: string, coordinates: number[][] }}}
 */
export async function getRoute(from, to, mapboxToken) {
  const url = `https://api.mapbox.com/directions/v5/mapbox/driving/${from};${to}?geometries=geojson&access_token=${mapboxToken}`
  console.log("Fetching route from", url)
  const route = await fetch(url)
  const routeData = await route.json()
  console.log("Got route data", routeData)
  return routeData.routes[0]
}

/**
 * Adds a rendered route to the map
 * @param {unknown} map The mapboxgl.Map instance
 * @param {number[]} startingPoint The starting point of the route, in the format [longitude, latitude]
 * @param {number[]} destinationCoordinates The destination of the route, in the format [longitude, latitude]
 * @param {*} lineCoordinates The coordinates of the route line, not really sure of the format
 */
export function addRouteToMap(
  map,
  startingPoint,
  destinationCoordinates,
  lineCoordinates
) {
  const line = {
    type: "Feature",
    geometry: {
      type: "LineString",
      coordinates: lineCoordinates,
    },
  }
  if (map.getSource("route")) {
    map.removeLayer("route")
    map.removeSource("route")
  }
  map.addSource("route", {
    type: "geojson",
    data: line,
  })
  map.addLayer({
    id: "route",
    type: "line",
    source: "route",
    layout: {
      "line-join": "round",
      "line-cap": "round",
    },
    paint: {
      "line-color": "#5cd5eb",
      "line-width": 4,
    },
  })

  // Set padding to 20 on phones and 100 on desktop
  const padding = window.innerWidth < 600 ? 20 : 100
  map.fitBounds(
    [
      [startingPoint[0], startingPoint[1]],
      [destinationCoordinates[0], destinationCoordinates[1]],
    ],
    {
      padding: padding,
      duration: 1000,
    }
  )
}

/**
 * If there is any route currenty on the map, remove it
 * @param {unknown} map The mapboxgl.Map instance
 * @param {number[]} centeringPosition The position to center the map to after removing the route, in the format [longitude, latitude]
 */
export function removeRouteFromMap(map, centeringPosition) {
  if (map.getSource("route")) {
    map.removeLayer("route")
    map.removeSource("route")
  }
  // Fly to the last known position
  map.flyTo({
    center: centeringPosition,
    zoom: 15,
    speed: 3,
    pitch: 45,
  })
}


/**
 * Formats seconds to human readable time
 * @param {number} seconds 
 * @returns {string}
 */
export function formatSecondsToHuman(seconds) {
  const hours = Math.floor(seconds / 3600)
  const minutes = Math.floor((seconds % 3600) / 60)
  return hours > 0 ? `${hours}h ${minutes} min` : `${minutes} min`
}


/**
 * Formats meters to human readable distance
 * @param {number} meters 
 * @returns {string} e.g. "1.2 km" or "500 m"
 */
export function formatMetersToHuman(meters) {
  if (meters > 1000) {
    return `${(meters / 1000).toFixed(1)} km`
  }
  return `${meters} m`
}