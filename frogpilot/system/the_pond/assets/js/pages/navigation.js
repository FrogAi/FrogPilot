(() => {
"use strict";

const P = window.Pond;

const DAY_SECONDS = 86400;
const FAVORITE_TYPES = { home: { icon: "home", label: "Home" }, work: { icon: "flag", label: "Work" } };
const MAPBOX_API_URL = "https://api.mapbox.com";
const MESSAGES = {
  invalid_name: "The name is empty or too long",
  too_many_favorites: "You have as many favorites as you can save. Delete one first"
};
const SEARCH_LIMIT = 5;
const UNNAMED_PLACE = "Unnamed place";

const previews = new Map();

class MapboxError extends Error {
  constructor(message, detail) {
    super(message);
    this.detail = detail;
  }
}

let destination = null;
let favorites = [];
let found = null;
let lastPosition = null;
let map = null;
let picked = null;
let publicKey = "";
let sessionToken = newSessionToken();

function newSessionToken() {
  let token = "";

  for (const byte of crypto.getRandomValues(new Uint8Array(16))) {
    token += byte.toString(16).padStart(2, "0");
  }

  return token;
}

function showError(error, title) {
  if (error instanceof MapboxError) {
    P.toast(error.message, error.detail, "alert");
    return;
  }

  P.fail(error, title, MESSAGES);
}

async function mapbox(path, query) {
  const url = new URL(`${MAPBOX_API_URL}/${path}`);
  url.search = new URLSearchParams({ ...query, access_token: publicKey });

  let response;

  try {
    response = await fetch(url);
  } catch {
    throw new MapboxError("Couldn't reach Mapbox", "Check this phone or computer's internet connection and try again.");
  }

  if (response.status === 401 || response.status === 403) {
    throw new MapboxError("Mapbox didn't accept your public key", `Check it on the <a class="link" href="/keys">Keys</a> page.`);
  }

  if (!response.ok) {
    throw new MapboxError("Mapbox had a problem", `It answered with HTTP ${response.status}. Try again in a moment.`);
  }

  return response.json();
}

async function placeOf(result) {
  if (!result.place) {
    const data = await mapbox(`search/searchbox/v1/retrieve/${encodeURIComponent(result.mapbox_id)}`, { session_token: sessionToken });
    sessionToken = newSessionToken();

    const feature = data.features[0];

    if (!feature) {
      throw new MapboxError("Mapbox has no location for that place", "Try another result.");
    }

    const [longitude, latitude] = feature.geometry.coordinates;
    result.place = { latitude, longitude, name: result.name };
  }

  return result.place;
}

function placeKey(place) {
  return `${place.latitude},${place.longitude}`;
}

function destinationPlace() {
  if (!destination) {
    return null;
  }

  return { latitude: destination.latitude, longitude: destination.longitude, name: destination.place_name || UNNAMED_PLACE };
}

function shownPlace() {
  if (picked) {
    return picked;
  }

  return destinationPlace();
}

function isDestination(place) {
  return Boolean(destination) && placeKey(place) === placeKey(destination);
}

function favoriteAt(place) {
  for (const favorite of favorites) {
    if (favorite.latitude === place.latitude && favorite.longitude === place.longitude) {
      return favorite;
    }
  }

  return null;
}

function favoriteById(id) {
  for (const favorite of favorites) {
    if (favorite.id === id) {
      return favorite;
    }
  }

  return null;
}

function favoriteType(favorite) {
  if (favorite.is_home) {
    return "home";
  }

  if (favorite.is_work) {
    return "work";
  }

  return "";
}

function favoriteRank(favorite) {
  const type = favoriteType(favorite);

  if (type === "home") {
    return 0;
  }

  if (type === "work") {
    return 1;
  }

  return 2;
}

async function setDestination(place) {
  await P.request("PUT", "/api/navigation/destination", { latitude: place.latitude, longitude: place.longitude, name: place.name });
  found = null;
  picked = null;
}

async function saveFavorite(place) {
  if (favoriteAt(place)) {
    P.toast(`${P.esc(place.name)} is already a favorite`);
    return false;
  }

  await P.request("POST", "/api/navigation/favorites", { latitude: place.latitude, longitude: place.longitude, name: place.name });
  return true;
}

function arrival(seconds) {
  const time = P.now() + seconds;

  if (seconds < DAY_SECONDS) {
    return P.timeLabel(time);
  }

  return P.fmt(time, { weekday: "short", hour: "numeric", minute: "2-digit" });
}

function statsHTML(entry) {
  const route = entry.routes[entry.selected];
  return `<dt>Distance</dt><dd>${P.distance(route.distance)}</dd>
    <dt>Travel time</dt><dd>${P.duration(route.duration)}</dd>
    <dt>Arrival</dt><dd>${arrival(route.duration)}</dd>`;
}

function routeButtons(entry) {
  if (entry.routes.length < 2) {
    return "";
  }

  const buttons = [];
  entry.routes.forEach((route, index) => {
    const button = `<button type="button" data-act="nav-route" data-index="${index}" aria-pressed="${index === entry.selected}">`;
    buttons.push(`${button}${P.duration(route.duration)}</button>`);
  });
  return `<div class="seg nav-routes" role="group" aria-label="Routes">${buttons.join("")}</div>`;
}

function routeHTML(entry) {
  if (!entry || (!entry.routes && !entry.error)) {
    return `<p class="muted" role="status">Finding routes…</p>`;
  }

  if (entry.error) {
    return `<div class="notice danger">${P.ic("alert")}<span><b>${entry.error.title}</b> ${entry.error.detail}</span></div>`;
  }

  return `<div class="nav-map" data-nav-map aria-label="Map of the route" role="img"></div>
    <dl class="kv nav-stats" data-nav-stats>${statsHTML(entry)}</dl>
    ${routeButtons(entry)}
    <p class="hint nav-route-hint">FrogPilot plans its own route, which may differ from these previews.</p>`;
}

function previewPanel(place) {
  const own = isDestination(place);
  let title = "Route preview";
  let close = "";
  let name = `<p class="nav-place"><b>${P.esc(place.name)}</b></p>`;

  if (own) {
    title = "Route";
    name = "";
  }

  if (picked) {
    close = `<button class="icon-btn nav-close" data-act="nav-preview-close" aria-label="Close the route preview" title="Close">${P.ic("x")}</button>`;
  }

  const actions = [];

  if (!own) {
    actions.push(`<button class="btn btn-primary btn-small" data-act="nav-preview-go">Navigate</button>`);
  }

  if (!own && !favoriteAt(place)) {
    actions.push(`<button class="btn btn-quiet btn-small" data-act="nav-preview-save">${P.ic("star")}Save as favorite</button>`);
  }

  let foot = "";

  if (actions.length) {
    foot = `<div class="panel-foot">${actions.join("")}</div>`;
  }

  return `<section class="panel" aria-labelledby="nav-preview-h"><div class="panel-head"><h2 id="nav-preview-h">${title}</h2>${close}</div>
    <div class="panel-body">${name}${routeHTML(previews.get(placeKey(place)))}</div>${foot}</section>`;
}

function removeMap() {
  if (map) {
    map.remove();
    map = null;
  }
}

function color(name) {
  return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
}

function routePaint(selected) {
  if (selected) {
    return { "line-color": color("--pond"), "line-opacity": 1, "line-width": 6 };
  }

  return { "line-color": color("--ink-3"), "line-opacity": 0.8, "line-width": 4 };
}

function point(coordinates) {
  return { data: { geometry: { coordinates, type: "Point" }, type: "Feature" }, type: "geojson" };
}

function selectRoute(index) {
  const entry = previews.get(placeKey(shownPlace()));
  const holder = document.querySelector("[data-nav-preview]");
  entry.selected = index;
  holder.querySelector("[data-nav-stats]").innerHTML = statsHTML(entry);

  for (const button of holder.querySelectorAll('[data-act="nav-route"]')) {
    button.setAttribute("aria-pressed", String(Number(button.dataset.index) === index));
  }

  if (map && map.getLayer("start")) {
    entry.routes.forEach((route, other) => {
      for (const [property, value] of Object.entries(routePaint(other === index))) {
        map.setPaintProperty(`route-${other}`, property, value);
      }
    });
    map.moveLayer(`route-${index}`, "start");
  }
}

async function drawMap(holder, place, entry) {
  await P.loadMapbox();

  if (!holder.isConnected) {
    return;
  }

  const start = [lastPosition.longitude, lastPosition.latitude];
  const end = [place.longitude, place.latitude];
  const bounds = new mapboxgl.LngLatBounds(start, start);

  for (const route of entry.routes) {
    for (const coordinate of route.geometry.coordinates) {
      bounds.extend(coordinate);
    }
  }

  mapboxgl.accessToken = publicKey;
  const shown = new mapboxgl.Map({ bounds, container: holder, fitBoundsOptions: { maxZoom: 16, padding: 40 }, style: P.MAP_STYLE });
  map = shown;
  let loaded = false;

  const fail = () => {
    if (loaded) {
      return;
    }

    shown.off("error", fail);
    removeMap();
    holder.outerHTML = `<div class="notice danger nav-map-failed">${P.ic("alert")}<span><b>The map didn't load</b>
      Check your Mapbox public key on the <a class="link" href="/keys">Keys</a> page.</span></div>`;
  };

  shown.on("error", fail);
  shown.on("load", () => {
    const endPaint = { "circle-color": color("--season"), "circle-radius": 8, "circle-stroke-color": "#fff", "circle-stroke-width": 3 };
    const layout = { "line-cap": "round", "line-join": "round" };
    const startPaint = { "circle-color": color("--card"), "circle-radius": 6, "circle-stroke-color": color("--pond"), "circle-stroke-width": 3 };
    loaded = true;

    entry.routes.forEach((route, index) => {
      shown.addSource(`route-${index}`, { data: { geometry: route.geometry, type: "Feature" }, type: "geojson" });
      shown.addLayer({ id: `route-${index}`, layout, paint: routePaint(index === entry.selected), source: `route-${index}`, type: "line" });
      shown.on("click", `route-${index}`, () => {
        selectRoute(index);
      });
    });

    shown.addSource("start", point(start));
    shown.addSource("end", point(end));
    shown.addLayer({ id: "start", paint: startPaint, source: "start", type: "circle" });
    shown.addLayer({ id: "end", paint: endPaint, source: "end", type: "circle" });
    shown.moveLayer(`route-${entry.selected}`, "start");
  });
}

async function loadRoutes(place) {
  const key = placeKey(place);
  const entry = { error: null, routes: null, selected: 0 };
  previews.set(key, entry);

  if (lastPosition) {
    const path = `directions/v5/mapbox/driving-traffic/${lastPosition.longitude},${lastPosition.latitude};${place.longitude},${place.latitude}`;

    try {
      const data = await mapbox(path, { alternatives: "true", geometries: "geojson", overview: "full" });
      entry.routes = data.routes || [];
    } catch (error) {
      if (!(error instanceof MapboxError)) {
        throw error;
      }

      entry.error = { detail: error.detail, title: error.message };
    }
  } else {
    entry.error = { detail: "Route previews show once the device has found its GPS location.", title: "The device's location isn't known yet" };
  }

  if (entry.routes && !entry.routes.length) {
    entry.error = { detail: "Try a different place.", title: "Mapbox found no driving route there" };
  }

  const shown = shownPlace();

  if (shown && placeKey(shown) === key) {
    showPreview();
  }
}

function showPreview() {
  const holder = document.querySelector("[data-nav-preview]");

  if (!holder) {
    return;
  }

  removeMap();
  const place = shownPlace();

  if (!place) {
    holder.innerHTML = "";
    return;
  }

  holder.innerHTML = previewPanel(place);
  const entry = previews.get(placeKey(place));

  if (!entry) {
    loadRoutes(place);
    return;
  }

  if (entry.routes && entry.routes.length) {
    drawMap(holder.querySelector("[data-nav-map]"), place, entry);
  }
}

function previewPlace(place) {
  picked = place;
  showPreview();

  let behavior = "smooth";

  if (P.reducedMotion.matches) {
    behavior = "auto";
  }

  document.querySelector("[data-nav-preview]").scrollIntoView({ behavior, block: "nearest" });
}

function destinationPanel() {
  let body = `<p class="muted">No destination is set. Search for a place or choose a favorite to preview its route.</p>`;

  if (destination) {
    const name = P.esc(destination.place_name || UNNAMED_PLACE);
    let save = `<button class="btn btn-quiet btn-small" data-act="nav-destination-save">${P.ic("star")}Save as favorite</button>`;

    if (favoriteAt(destination)) {
      save = "";
    }

    body = `<div class="list-row">
        <span class="grow"><b>${name}</b><span>${destination.latitude.toFixed(5)}, ${destination.longitude.toFixed(5)}</span></span>
        <span class="acts">${save}<button class="btn btn-danger-quiet btn-small" data-act="nav-cancel">Cancel destination</button></span>
      </div>
      <p class="hint">The device keeps it until you arrive or turn the car off.</p>`;
  }

  return `<section class="panel" aria-labelledby="nav-destination-h"><div class="panel-head"><h2 id="nav-destination-h">Destination</h2></div>
    <div class="panel-body">${body}</div></section>`;
}

function resultRow(result, index) {
  const name = P.esc(result.name);
  const address = P.esc(result.full_address || result.place_formatted || "");
  const save = `<button class="icon-btn" data-act="nav-result-save" data-index="${index}" aria-label="Save ${name} as a favorite" title="Save as favorite">`;
  return `<div class="list-row">
    <span class="grow"><b>${name}</b><span>${address}</span></span>
    <span class="acts">
      ${save}${P.ic("star")}</button>
      <button class="btn btn-quiet btn-small" data-act="nav-result-preview" data-index="${index}">Preview</button>
    </span>
  </div>`;
}

function resultsHTML() {
  if (!found) {
    return "";
  }

  if (!found.results.length) {
    return `<p class="muted nav-no-results">No places match “${P.esc(found.query)}”. Try another search.</p>`;
  }

  const rows = [];
  found.results.forEach((result, index) => {
    rows.push(resultRow(result, index));
  });
  return rows.join("");
}

function searchPanel() {
  let query = "";

  if (found) {
    query = P.esc(found.query);
  }

  const input = `<input class="input" type="search" name="query" value="${query}" placeholder="Place or address" autocomplete="off" enterkeyhint="search">`;
  return `<section class="panel" aria-labelledby="nav-search-h"><div class="panel-head"><h2 id="nav-search-h">Find a place</h2></div>
    <div class="panel-body">
      <form class="nav-search" data-nav-search>
        <label class="search"><span class="sr">Search for a place</span>${P.ic("search")}${input}</label>
        <button class="btn btn-primary" type="submit">Search</button>
      </form>
      <p class="hint nav-privacy">Your searches and route previews go to Mapbox.</p>
      <div class="nav-results" data-nav-results aria-live="polite">${resultsHTML()}</div>
    </div></section>`;
}

function favoriteRow(favorite) {
  const id = P.esc(favorite.id);
  const name = P.esc(favorite.name);
  const type = favoriteType(favorite);
  let meta = "";

  if (type) {
    meta = `<span>${FAVORITE_TYPES[type].label}</span>`;
  }

  const typeItems = [];

  for (const [kind, info] of Object.entries(FAVORITE_TYPES)) {
    let label = `Set as ${kind}`;
    let value = kind;

    if (type === kind) {
      label = `Remove as ${kind}`;
      value = "";
    }

    const item = `<button class="menu-item" role="menuitem" data-act="nav-favorite-type" data-id="${id}" data-type="${value}">`;
    typeItems.push(`${item}${P.ic(info.icon)}<span>${label}</span></button>`);
  }

  const more = `<button class="icon-btn" data-act="menu" aria-expanded="false" aria-haspopup="true" aria-label="More for ${name}" title="More">`;
  const rename = `<button class="menu-item" role="menuitem" data-act="nav-favorite-rename" data-id="${id}">`;
  const remove = `<button class="icon-btn danger" data-act="nav-favorite-delete" data-id="${id}" aria-label="Delete ${name}" title="Delete">`;
  return `<div class="list-row">
    <span class="grow"><b>${name}</b>${meta}</span>
    <span class="acts">
      <button class="btn btn-quiet btn-small" data-act="nav-favorite-preview" data-id="${id}">Preview</button>
      <span class="menu-wrap">${more}${P.ic("dots")}</button>
        <div class="menu" role="menu" hidden>
          ${rename}${P.ic("pencil")}<span>Rename…</span></button>
          ${typeItems.join("")}
        </div></span>
      ${remove}${P.ic("trash")}</button>
    </span>
  </div>`;
}

function favoritesPanel() {
  const sorted = [...favorites].sort((left, right) => {
    return favoriteRank(left) - favoriteRank(right) || left.name.localeCompare(right.name);
  });

  let body = `<div class="empty"><h3>No favorites yet</h3>
    <p>Save a place from your search results or your destination to find it here next time.</p></div>`;
  let count = "";

  if (sorted.length) {
    const rows = [];

    for (const favorite of sorted) {
      rows.push(favoriteRow(favorite));
    }

    body = rows.join("");
    count = `<span class="aside muted">${sorted.length} saved</span>`;
  }

  return `<section class="panel" aria-labelledby="nav-favorites-h"><div class="panel-head"><h2 id="nav-favorites-h">Favorites</h2>${count}</div>
    <div class="panel-body">${body}</div></section>`;
}

async function searchPlaces(form) {
  const input = form.query;
  const query = input.value.trim();

  if (!query) {
    input.setAttribute("aria-invalid", "true");
    P.toast("Enter a place or address first", "", "alert");
    return;
  }

  input.removeAttribute("aria-invalid");

  const parameters = { limit: SEARCH_LIMIT, q: query, session_token: sessionToken };

  if (lastPosition) {
    parameters.proximity = `${lastPosition.longitude},${lastPosition.latitude}`;
  }

  let data;

  try {
    data = await P.busy(form.querySelector("[type=submit]"), mapbox("search/searchbox/v1/suggest", parameters));
  } catch (error) {
    showError(error, "Couldn't search");
    return;
  }

  const results = [];

  for (const suggestion of data.suggestions) {
    if (suggestion.feature_type !== "category") {
      results.push(suggestion);
    }
  }

  found = { query, results };
  form.closest(".panel").querySelector("[data-nav-results]").innerHTML = resultsHTML();
}

P.act("nav-cancel", async button => {
  try {
    await P.busy(button, P.request("DELETE", "/api/navigation/destination"));
  } catch (error) {
    P.fail(error, "Couldn't cancel the destination");
    return;
  }

  P.toast("Destination canceled");
  P.refresh();
});

P.act("nav-destination-save", async button => {
  let saved;

  try {
    saved = await P.busy(button, saveFavorite(destinationPlace()));
  } catch (error) {
    P.fail(error, "Couldn't save the favorite", MESSAGES);
    return;
  }

  if (saved) {
    P.toast(`${P.esc(destinationPlace().name)} saved to your favorites`);
    P.refresh();
  }
});

P.act("nav-favorite-delete", async button => {
  const favorite = favoriteById(button.dataset.id);
  const name = P.esc(favorite.name);
  const ok = await P.dialog({
    icon: "trash", tone: "danger", confirmTone: "danger", title: `Delete ${name}?`,
    body: `<p>It's removed from your favorites here and on the device. Your destination stays as it is.</p>`,
    confirm: "Delete favorite"
  });

  if (!ok) {
    return;
  }

  try {
    await P.request("DELETE", `/api/navigation/favorites/${encodeURIComponent(favorite.id)}`);
  } catch (error) {
    P.fail(error, "Couldn't delete the favorite");
    return;
  }

  P.toast(`${name} deleted from your favorites`);
  P.refresh();
});

P.act("nav-favorite-preview", button => {
  const favorite = favoriteById(button.dataset.id);
  previewPlace({ latitude: favorite.latitude, longitude: favorite.longitude, name: favorite.name });
});

P.act("nav-favorite-rename", async button => {
  const favorite = favoriteById(button.dataset.id);
  const dialog = await P.dialog({
    icon: "pencil", title: `Rename ${P.esc(favorite.name)}`,
    body: `<label class="field"><span>Name</span><input class="input" data-name value="${P.esc(favorite.name)}" autocomplete="off"></label>`,
    confirm: "Save name"
  });

  if (!dialog) {
    return;
  }

  const name = dialog.querySelector("[data-name]").value.trim();

  if (name === favorite.name) {
    return;
  }

  try {
    await P.request("PUT", `/api/navigation/favorites/${encodeURIComponent(favorite.id)}`, { name });
  } catch (error) {
    P.fail(error, "Couldn't rename the favorite", MESSAGES);
    return;
  }

  P.toast("Favorite renamed", P.esc(name));
  P.refresh();
});

P.act("nav-favorite-type", async button => {
  const favorite = favoriteById(button.dataset.id);
  const type = button.dataset.type;
  let value = null;

  if (type) {
    value = type;
  }

  try {
    await P.request("PUT", `/api/navigation/favorites/${encodeURIComponent(favorite.id)}`, { type: value });
  } catch (error) {
    P.fail(error, "Couldn't change the favorite", MESSAGES);
    return;
  }

  if (type) {
    P.toast(`${FAVORITE_TYPES[type].label} set`, P.esc(favorite.name));
  } else {
    P.toast(`${P.esc(favorite.name)} is no longer ${favoriteType(favorite)}`);
  }

  P.refresh();
});

P.act("nav-preview-close", () => {
  picked = null;
  showPreview();
});

P.act("nav-preview-go", async button => {
  const name = picked.name;

  try {
    await P.busy(button, setDestination(picked));
  } catch (error) {
    P.fail(error, "Couldn't set the destination", MESSAGES);
    return;
  }

  P.toast("Destination set", P.esc(name));
  P.refresh();
});

P.act("nav-preview-save", async button => {
  const place = picked;
  let saved;

  try {
    saved = await P.busy(button, saveFavorite(place));
  } catch (error) {
    P.fail(error, "Couldn't save the favorite", MESSAGES);
    return;
  }

  if (saved) {
    P.toast(`${P.esc(place.name)} saved to your favorites`);
    P.refresh();
  }
});

P.act("nav-result-preview", async button => {
  const result = found.results[Number(button.dataset.index)];
  let place;

  try {
    place = await P.busy(button, placeOf(result));
  } catch (error) {
    showError(error, "Couldn't find that place");
    return;
  }

  previewPlace(place);
});

P.act("nav-result-save", async button => {
  const result = found.results[Number(button.dataset.index)];
  let saved;

  try {
    saved = await P.busy(button, placeOf(result).then(saveFavorite));
  } catch (error) {
    showError(error, "Couldn't save the favorite");
    return;
  }

  if (saved) {
    P.toast(`${P.esc(result.name)} saved to your favorites`);
    P.refresh();
  }
});

P.act("nav-route", button => {
  selectRoute(Number(button.dataset.index));
});

P.page("navigation", {
  title: "Navigation",
  async render() {
    const [navigation, keys] = await Promise.all([P.get("/api/navigation"), P.get("/api/keys")]);

    if (!keys.mapbox_public || !keys.mapbox_secret) {
      return {
        body: `<section class="panel"><div class="empty"><h3>Add your Mapbox keys first</h3>
          <p>Navigation needs both Mapbox keys. Your public key finds places and previews routes,
            and your secret key lets the device show the route on your driving screen.</p>
          <a class="btn btn-primary btn-small" href="/keys">${P.ic("keys")}Go to Keys</a></div></section>`
      };
    }

    destination = navigation.destination;
    favorites = navigation.favorites;
    lastPosition = navigation.last_position;
    publicKey = keys.mapbox_public;
    return {
      body: `
      <div class="two-col wide-left">
        <div class="stack">
          ${destinationPanel()}
          <div class="nav-preview" data-nav-preview></div>
        </div>
        <div class="stack">
          ${searchPanel()}
          ${favoritesPanel()}
        </div>
      </div>`,
      mount: root => {
        root.querySelector("[data-nav-search]").addEventListener("submit", event => {
          event.preventDefault();
          searchPlaces(event.currentTarget);
        });

        showPreview();

        return () => {
          removeMap();

          if (location.pathname !== "/navigation") {
            found = null;
            picked = null;
            previews.clear();
          }
        };
      }
    };
  }
});
})();
