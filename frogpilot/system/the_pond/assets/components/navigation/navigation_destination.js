import { html, reactive } from "/assets/vendor/arrow.mjs";
import {
  addRouteToMap,
  ensureMapboxLoaded,
  formatMetersToHuman,
  formatSecondsToHuman,
  getCoordinatesFromSearch,
  getRoutes,
  removeRouteFromMap,
  getOrdinalSuffix,
  highlightRoute,
} from "./navigation_utilities.js";
import { fetchJson } from "/assets/js/api.js";
import { Modal } from "/assets/components/modal.js";
import { onRouteLeave } from "/assets/components/router.js";

async function setSpecial(favorite, type, state, loadFavoritesAlphabetically) {
  try {
    const isCurrentlyHome = favorite.is_home;
    const isCurrentlyWork = favorite.is_work;
    let newIsHome = null;
    let newIsWork = null;
    let message = "";
    if (type === "home") {
      if (isCurrentlyHome) {
        newIsHome = false;
        message = "Home location removed!";
      } else {
        newIsHome = true;
        if (isCurrentlyWork) newIsWork = false;
        message = "Home location set!";
      }
    } else if (type === "work") {
      if (isCurrentlyWork) {
        newIsWork = false;
        message = "Work location removed!";
      } else {
        newIsWork = true;
        if (isCurrentlyHome) newIsHome = false;
        message = "Work location set!";
      }
    }
    const body = { routeId: favorite.routeId, id: favorite.id };
    if (newIsHome !== null) body.is_home = newIsHome;
    if (newIsWork !== null) body.is_work = newIsWork;
    const response = await fetch("/api/navigation/favorite/rename", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body)
    });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    showSnackbar(message);
    const sorted = await loadFavoritesAlphabetically();
    state.suggestions = "[]";
    await new Promise(resolve => setTimeout(resolve, 0));
    state.suggestions = JSON.stringify(sorted);
  } catch {
    showSnackbar(`Failed to update ${type} location...`);
  }
}

let map;
let destinationMarker;
let favoriteMarkers = [];
let navInitStarted = false;
let searchController = null;
let searchTimer = null;
const state = reactive({
  confirmedRoute: null,
  confirmedRouteRefresh: 0,
  destination: undefined,
  favoriteRoutes: [],
  favoriteToRemove: null,
  favoriteToRename: null,
  favoritesCount: 0,
  favoritesVisible: false,
  initialized: false,
  isMetric: true,
  lastPosition: undefined,
  locationAvailable: null,
  loadingRoute: false,
  mapboxPublic: undefined,
  mapboxSecret: undefined,
  missingKeys: null,
  newFavoriteName: "",
  previousDestinations: [],
  selectedRoute: null,
  showRemoveFavoriteModal: false,
  showRenameFavoriteModal: false,
  suggestions: "[]"
});
const searchFieldState = reactive({ value: "" });
const sessionToken = crypto.randomUUID?.() || Math.random().toString(36).slice(2);

export function NavDestination() {
  onRouteLeave(() => {
    clearTimeout(searchTimer);
    searchController?.abort();
    searchController = null;
    navInitStarted = false;
    state.initialized = false;
    state.locationAvailable = null;
    state.missingKeys = null;
    try { map?.remove(); } catch (e) {}
    map = undefined;
  });

  function confirmRemoveFavorite(favorite) {
    state.favoriteToRemove = favorite;
    state.showRemoveFavoriteModal = true;
  }

  function confirmRenameFavorite(fav) {
    state.favoriteToRename = fav;
    state.newFavoriteName = fav.name;
    state.showRenameFavoriteModal = true;
  }

  async function setHome(favorite) {
    await setSpecial(favorite, "home", state, loadFavoritesAlphabetically);
  }

  async function setWork(favorite) {
    await setSpecial(favorite, "work", state, loadFavoritesAlphabetically);
  }

  async function initiateNavigation(destination, { resume = false } = {}) {
    state.selectedRoute = null;
    state.confirmedRoute = null;
    state.loadingRoute = true;
    try {
      const { name, longitude, latitude } = destination;
      const coords = [longitude, latitude];

      const inputEl = document.getElementById("search-field");
      if (inputEl && !resume) {
        inputEl.value = name;
      }

      if (destinationMarker) destinationMarker.remove();
      destinationMarker = new mapboxgl.Marker().setLngLat(coords).addTo(map);

      const routes = await getRoutes(
        `${state.lastPosition.longitude},${state.lastPosition.latitude}`,
        `${coords[0]},${coords[1]}`,
        state.mapboxPublic
      );

      removeRouteFromMap(map);

      if (routes.length > 0) {
        const selectedRouteId = "main";
        const selectedRouteData = routes[0];
        const selected = {
          name,
          duration: selectedRouteData.duration,
          distance: selectedRouteData.distance,
          destinationCoordinates: coords,
          startingCoordinates: [state.lastPosition.longitude, state.lastPosition.latitude],
          routeId: selectedRouteId
        };

        state.selectedRoute = selected;
        if (resume) state.confirmedRoute = JSON.parse(JSON.stringify(selected));

        addRouteToMap(
          map,
          routes,
          [state.lastPosition.longitude, state.lastPosition.latitude],
          coords,
          (route, routeId) => {
            state.selectedRoute = {
              ...state.selectedRoute,
              duration: route.duration,
              distance: route.distance,
              routeId
            };
            highlightRoute(map, routes, routeId);
          },
          state.isMetric,
          () => state.selectedRoute?.routeId ?? null
        );

        if (resume && map) {
          requestAnimationFrame(() => {
            map.flyTo({
              center: [state.lastPosition.longitude, state.lastPosition.latitude],
              zoom: 18,
              pitch: 45,
              speed: 1,
              curve: 1
            });
          });
        }
      }

      state.suggestions = "[]";
    } catch (err) {
      console.error("Failed to calculate route:", err);
      showSnackbar("Failed to calculate route...");
    } finally {
      state.loadingRoute = false;
    }
  }

  async function getNavigationData() {
    let data;
    try {
      const res = await fetch("/api/navigation");
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      data = await res.json();
    } catch {
      state.missingKeys = true;
      showSnackbar("Failed to load navigation data...", "error");
      return;
    }
    state.mapboxPublic = (data.mapboxPublic || "").trim();
    state.mapboxSecret = !!data.mapboxSecretSet;
    state.isMetric = data.isMetric ?? true;
    state.missingKeys = !(state.mapboxPublic && state.mapboxSecret);
    if (state.missingKeys) return;
    const latitude = Number(data.lastPosition?.latitude);
    const longitude = Number(data.lastPosition?.longitude);
    state.locationAvailable = Number.isFinite(latitude) && latitude >= -90 && latitude <= 90 &&
      Number.isFinite(longitude) && longitude >= -180 && longitude <= 180;
    state.lastPosition = state.locationAvailable ? { latitude, longitude } : undefined;
    try {
      state.destination = JSON.parse(data.destination);
    } catch {}
    try {
      const prev = JSON.parse(data.previousDestinations);
      state.previousDestinations = prev.map(d => ({ name: d.place_name }));
      state.suggestions = JSON.stringify(state.previousDestinations);
    } catch {}
    if (!state.locationAvailable) {
      loadFavoritesAlphabetically();
      return;
    }
    try {
      await ensureMapboxLoaded();
      setupMap();
      loadFavoritesAlphabetically();
    } catch {
      showSnackbar("Failed to load the map...", "error");
    }
  }

  async function handleFavoritesClick() {
    if (state.favoritesVisible) {
      state.suggestions = "[]";
      state.favoritesVisible = false;
      return;
    }
    searchFieldState.value = "";
    state.selectedRoute = null;
    state.confirmedRoute = null;
    const sorted = await loadFavoritesAlphabetically();
    state.suggestions = JSON.stringify(sorted);
    state.favoritesVisible = true;
  }

  async function handleSearchKey(e) {
    if (e.key === "Enter") {
      clearTimeout(searchTimer);
      const val = e.target.value.trim();
      searchFieldState.value = e.target.value;
      if (val.length < 3) {
        state.suggestions = "[]";
        return;
      }
      await searchSuggestions(val);
    }
  }

  function isRouteFavorited(route, favorites) {
    return favorites.some(fav =>
      fav.latitude === route.destinationCoordinates[1] &&
      fav.longitude === route.destinationCoordinates[0]
    );
  }

  function addFavoriteMarkers(favorites) {
    favoriteMarkers.forEach(marker => marker.remove());
    favoriteMarkers = [];
    favorites.forEach(fav => {
      const el = document.createElement("div");
      el.className = "favorite-marker";
      let icon = "❤️";
      let popupText = fav.name;
      if (fav.is_home) {
        icon = "🏠";
        el.className += " home-marker";
        popupText = `Home: ${fav.name}`;
      } else if (fav.is_work) {
        icon = "💼";
        el.className += " work-marker";
        popupText = `Work: ${fav.name}`;
      }
      el.innerHTML = icon;
      const marker = new mapboxgl.Marker(el)
        .setLngLat([fav.longitude, fav.latitude])
        .setPopup(new mapboxgl.Popup({ offset: 25, closeButton: false }).setText(popupText))
        .addTo(map);
      el.addEventListener("click", () => {
        if (marker.getPopup().isOpen()) {
          marker.togglePopup();
        }
        initiateNavigation(fav);
      });
      el.addEventListener("mouseenter", () => marker.togglePopup());
      el.addEventListener("mouseleave", () => marker.togglePopup());
      favoriteMarkers.push(marker);
    });
  }

  async function loadFavoritesAlphabetically() {
    try {
      const res = await fetch("/api/navigation/favorite");
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const json = await res.json();
      const sorted = json.favorites.sort((a, b) => (a.name || "").localeCompare(b.name || ""));
      state.favoritesCount = sorted.length;
      state.favoriteRoutes = sorted;
      if (map) addFavoriteMarkers(sorted);
      if (state.favoritesVisible) {
        state.suggestions = JSON.stringify(sorted);
      }
      return sorted;
    } catch {
      showSnackbar("Failed to load favorites...");
      return [];
    }
  }

  async function removeFavorite() {
    if (!state.favoriteToRemove) return;
    const { id, name, latitude, longitude, routeId } = state.favoriteToRemove;
    try {
      const response = await fetch("/api/navigation/favorite", {
        method: "DELETE",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ id, name, latitude, longitude, routeId })
      });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      await loadFavoritesAlphabetically();
      showSnackbar("Favorite removed!");
    } catch {
      showSnackbar("Failed to remove favorite...");
    } finally {
      state.showRemoveFavoriteModal = false;
      state.favoriteToRemove = null;
    }
  }

  async function renameFavorite() {
    const fav = state.favoriteToRename;
    const newName = state.newFavoriteName.trim();
    if (!fav || !newName || newName === fav.name) {
      state.showRenameFavoriteModal = false;
      return;
    }
    try {
      const response = await fetch("/api/navigation/favorite/rename", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          id: fav.id,
          name: newName,
          routeId: fav.routeId
        })
      });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);

      if (state.favoritesVisible) {
        state.suggestions = "[]";
        state.favoritesVisible = false;
      }

      await handleFavoritesClick();

      showSnackbar(`"${fav.name}" renamed to "${newName}"!`, "success");
    } catch {
      showSnackbar("Failed to edit favorite name...");
    } finally {
      state.showRenameFavoriteModal = false;
    }
  }

  async function searchInput(e) {
    const newVal = e.target.value.trim();
    searchFieldState.value = e.target.value;
    clearTimeout(searchTimer);
    if (newVal.length < 3) searchController?.abort();
    searchTimer = setTimeout(async () => {
      const val = newVal;
      if (val.length < 3) {
        state.suggestions = "[]";
        return;
      }
      await searchSuggestions(val);
    }, 800);
  }

  async function searchSuggestions(value) {
    state.selectedRoute = null;
    state.confirmedRoute = null;
    state.suggestions = "[]";
    if (!state.lastPosition) return;

    searchController?.abort();
    const controller = new AbortController();
    searchController = controller;
    const timeout = setTimeout(() => controller.abort(), 10000);
    const params = new URLSearchParams({
      proximity: `${state.lastPosition.longitude},${state.lastPosition.latitude}`,
      access_token: state.mapboxPublic,
      session_token: sessionToken,
      q: value,
      limit: 4
    });
    try {
      const response = await fetch(`https://api.mapbox.com/search/searchbox/v1/suggest?${params}`, { signal: controller.signal });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const data = await response.json();
      state.suggestions = JSON.stringify(Array.isArray(data.suggestions) ? data.suggestions : []);
    } catch (error) {
      if (error.name !== "AbortError") showSnackbar("Search failed - check your connection...", "error");
    } finally {
      clearTimeout(timeout);
      if (searchController === controller) searchController = null;
    }
  }

  async function selectSuggestion(sugg) {
    const label = sugg.full_address || sugg.name || sugg.address || "Unnamed Location";
    let coords;
    if (sugg.routeId) {
      initiateNavigation({
        name: sugg.name,
        longitude: sugg.longitude,
        latitude: sugg.latitude
      });
      return;
    }
    state.loadingRoute = true;
    try {
      if (sugg.geometry && Array.isArray(sugg.geometry.coordinates)) {
        coords = sugg.geometry.coordinates;
      } else if (sugg.mapbox_id) {
        const url = new URL(`https://api.mapbox.com/search/searchbox/v1/retrieve/${encodeURIComponent(sugg.mapbox_id)}`);
        url.searchParams.set("access_token", state.mapboxPublic);
        url.searchParams.set("session_token", sessionToken);
        const controller = new AbortController();
        const timeout = setTimeout(() => controller.abort(), 10000);
        const ret = await fetch(url, { signal: controller.signal }).finally(() => clearTimeout(timeout));
        if (!ret.ok) throw new Error(`HTTP ${ret.status}`);
        const retJson = await ret.json();
        coords = retJson.features[0].geometry.coordinates;
      } else {
        coords = await getCoordinatesFromSearch(label, state.mapboxPublic);
      }
      if (coords) {
        initiateNavigation({
          name: label,
          longitude: coords[0],
          latitude: coords[1]
        });
      } else {
        throw new Error("Could not determine location.");
      }
    } catch (err) {
      console.error(err);
      showSnackbar("Error: Could not determine location.", "error");
      state.loadingRoute = false;
    }
  }

  const setupMap = async () => {
    if (!state.mapboxPublic || !state.lastPosition || state.initialized) return;
    const container = document.getElementById("map");
    state.initialized = true;
    mapboxgl.accessToken = state.mapboxPublic;
    map = new mapboxgl.Map({
      container,
      center: [state.lastPosition.longitude, state.lastPosition.latitude],
      zoom: 15,
      pitch: 45,
      speed: 1,
      curve: 1,
      attributionControl: false,
      logoPosition: "bottom-right",
      style: "mapbox://styles/frogsgomoo/cmcfv151j000o01rcdxebhl76"
    });
    new mapboxgl.Marker().setLngLat([state.lastPosition.longitude, state.lastPosition.latitude]).addTo(map);
    map.on("load", () => {
      map.flyTo({
        center: [state.lastPosition.longitude, state.lastPosition.latitude],
        zoom: 18,
        pitch: 45,
        speed: 1,
        curve: 1
      });
      if (state.destination) {
        initiateNavigation(state.destination, { resume: true });
      }
    });
    map.on("style.load", () => {
      const labelLayer = map.getStyle().layers.find(l => l.type === "symbol" && l.layout["text-field"]).id;
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
            "fill-extrusion-height": ["interpolate", ["linear"], ["zoom"], 15, 0, 15.05, ["get", "height"]],
            "fill-extrusion-base": ["interpolate", ["linear"], ["zoom"], 15, 0, 15.05, ["get", "min_height"]],
            "fill-extrusion-opacity": 0.6
          }
        },
        labelLayer
      );
    });
  };

  if (!navInitStarted) {
    navInitStarted = true;
    getNavigationData();
  }

  return html`
    <div class="navigation-container">
      ${() => {
        if (state.missingKeys === null) return "";
        return state.missingKeys
          ? html`
              <section class="keys-required-wrapper">
                <div class="keys-required-widget">
                  <div class="keys-required-title">Mapbox Keys Required</div>
                  <p class="keys-required-text">You must set both your public and secret Mapbox keys before using navigation features.</p>
                  <a href="/manage_navigation_keys" class="keys-required-button">Go to "Manage Keys"</a>
                </div>
              </section>
            `
          : state.locationAvailable === false
            ? html`
              <section class="keys-required-wrapper">
                <div class="keys-required-widget">
                  <div class="keys-required-title">Waiting for Location</div>
                  <p class="keys-required-text">The map will be available after the device receives a GPS position.</p>
                </div>
              </section>
            `
            : html`
              <div class="map-wrapper">
                <div class="search-wrapper">
                  <div class="search-controls">
                    <input aria-label="Search for a destination" autocomplete="off" id="search-field" placeholder="Search here" value="${() => searchFieldState.value}" @input="${searchInput}" @keydown="${handleSearchKey}" />
                    ${() => (state.favoritesCount > 0 ? html`<button class="favorites-toggle-button" @click="${handleFavoritesClick}">❤️ Favorites</button>` : "")}
                  </div>
                  <p class="navigation-privacy-note">Destination searches and routes are sent to Mapbox.</p>
                  <div id="infobox">
                    ${() => {
                      if (state.loadingRoute) {
                        return html`<div class="navigation-summary-widget loading-status"><span class="spinner"></span> Calculating route...</div>`;
                      } else if (state.selectedRoute) {
                        return NavigationDestination({
                          ...state.selectedRoute,
                          isFavorited: isRouteFavorited(state.selectedRoute, state.favoriteRoutes),
                          isConfirmed: () => !!state.confirmedRoute,
                          map,
                          isMetric: state.isMetric,
                          cancelNavigationFn: () => {
                            state.selectedRoute = null;
                            state.confirmedRoute = null;
                            state.suggestions = JSON.stringify(state.previousDestinations);
                            if (destinationMarker) destinationMarker.remove();
                          },
                          onConfirm: () => {
                            state.confirmedRoute = JSON.parse(JSON.stringify(state.selectedRoute));
                            state.confirmedRouteRefresh = Math.random();
                          },
                          loadFavorites: loadFavoritesAlphabetically,
                          removeFavorite: confirmRemoveFavorite,
                          searchFieldState,
                          favoriteRoutes: state.favoriteRoutes
                        }, state.confirmedRouteRefresh);
                      } else if (JSON.parse(state.suggestions).length > 0) {
                        return SearchSuggestions({
                          suggestions: JSON.parse(state.suggestions),
                          selectSuggestion,
                          removeFavorite: confirmRemoveFavorite,
                          renameFavorite: confirmRenameFavorite,
                          setHome: setHome,
                          setWork: setWork
                        });
                      } else if (searchFieldState.value.trim().length >= 3) {
                        return html`<div class="navigation-summary-widget">No results found</div>`;
                      }
                    }}
                  </div>
                </div>
                <div id="map"></div>
              </div>
            `;
      }}
    </div>
    ${() => (state.showRemoveFavoriteModal ? Modal({
      title: "Remove Favorite",
      message: html`Are you sure you want to remove <strong>${() => state.favoriteToRemove?.name}</strong> from your favorites?`,
      onConfirm: removeFavorite,
      onCancel: () => { state.showRemoveFavoriteModal = false; state.favoriteToRemove = null; },
      confirmText: "Remove"
    }) : "")}
    ${() => (state.showRenameFavoriteModal ? Modal({
      title: "Rename Favorite",
      message: html`
        <div>
          <p>Rename <strong>${() => state.favoriteToRename.name}</strong> to:</p>
          <div style="margin-top: 10px;">
            <input class="modal-input" type="text" value="${() => state.newFavoriteName}" @click="${e => e.stopPropagation()}" @input="${e => state.newFavoriteName = e.target.value}" />
          </div>
        </div>
      `,
      onConfirm: renameFavorite,
      onCancel: () => { state.showRenameFavoriteModal = false; },
      confirmText: "Rename",
      confirmClass: "btn-primary"
    }) : "")}
  `;
}

function SearchSuggestions({ suggestions, selectSuggestion, removeFavorite, renameFavorite, setHome, setWork }) {
  const isFavorite = s => s.name && s.latitude != null && s.longitude != null && s.routeId;
  const item = s => html`
    <div class="suggestion-item" role="button" tabindex="0" @click="${() => selectSuggestion(s)}" @keydown="${e => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); selectSuggestion(s); } }}">
      <p>
        ${s.is_home ? "🏠 " : ""}
        ${s.is_work ? "💼 " : ""}
        ${() => s.name || s.address}
      </p>
      ${isFavorite(s) ? html`
        <div class="favorite-actions">
          <button class="home-favorite-button ${s.is_home ? "active" : ""}" title="Set as Home" aria-label="Set as Home" @click="${e => { e.stopPropagation(); setHome(s); }}">🏠</button>
          <button class="work-favorite-button ${s.is_work ? "active" : ""}" title="Set as Work" aria-label="Set as Work" @click="${e => { e.stopPropagation(); setWork(s); }}">💼</button>
          <button class="edit-favorite-button" title="Rename Favorite" aria-label="Rename Favorite" @click="${e => { e.stopPropagation(); renameFavorite(s); }}">✏️</button>
          <button class="remove-favorite-button" title="Remove from Favorites" aria-label="Remove from Favorites" @click="${e => { e.stopPropagation(); removeFavorite(s); }}">🗑️</button>
        </div>
      ` : ""}
    </div>
  `;
  return html`<div id="searchSuggestions">${suggestions.map(item)}</div>`;
}

function NavigationDestination({
  name,
  duration,
  distance,
  routeId,
  isConfirmed,
  destinationCoordinates,
  startingCoordinates,
  isMetric,
  map,
  cancelNavigationFn,
  onConfirm,
  loadFavorites,
  removeFavorite,
  searchFieldState,
  isFavorited,
  favoriteRoutes = []
}) {
  async function cancelNavigation() {
    try {
      const response = await fetch("/api/navigation", { method: "DELETE" });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      showSnackbar("Navigation cancelled...");
      removeRouteFromMap(map);
      cancelNavigationFn();
      map.flyTo({ center: startingCoordinates, zoom: 15, pitch: 45, speed: 1, curve: 1 });
    } catch {
      showSnackbar("Failed to cancel navigation...", "error");
    }
  }
  async function confirmDestination() {
    try {
      const response = await fetch("/api/navigation", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          name,
          longitude: destinationCoordinates[0],
          latitude: destinationCoordinates[1]
        })
      });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      onConfirm?.();
      showSnackbar("Navigation set!");
      await loadFavorites();
      const searchInputEl = document.getElementById("search-field");
      if (searchInputEl) searchInputEl.value = "";
      searchFieldState.value = "";
      requestAnimationFrame(() => {
        map?.flyTo({
          center: startingCoordinates,
          zoom: 18,
          pitch: 45,
          speed: 1,
          curve: 1
        });
      });
    } catch {
      showSnackbar("Failed to set navigation...", "error");
    }
  }
  async function favoriteDestination() {
    try {
      const { message } = await fetchJson("/api/navigation/favorite", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          name,
          longitude: destinationCoordinates[0],
          latitude: destinationCoordinates[1],
          routeId
        })
      });
      showSnackbar(message || "Added to favorites!");
      await loadFavorites();
    } catch (e) {
      showSnackbar(e.message, "error");
    }
  }
  async function toggleFavorite() {
    if (isFavorited) {
      const fav = favoriteRoutes.find(
        f => f.latitude === destinationCoordinates[1] && f.longitude === destinationCoordinates[0]
      );
      if (fav) {
        removeFavorite(fav);
      } else {
        showSnackbar("Couldn't find favorite entry...");
      }
    } else {
      await favoriteDestination();
    }
  }
  const safeDistance = Number.isFinite(distance) ? distance : null;
  const safeDuration = Number.isFinite(duration) ? duration : null;
  let etaString = "—";
  if (Number.isFinite(duration)) {
    const eta = new Date(Date.now() + duration * 1000);
    const isLong = duration > 86400;
    const timeStr = eta.toLocaleTimeString([], { hour: "numeric", minute: "2-digit" });
    const month = eta.toLocaleString([], { month: "long" });
    const day = eta.getDate();
    const year = eta.getFullYear();
    etaString = isLong ? `${month} ${day}${getOrdinalSuffix(day)}, ${year}, ${timeStr}` : timeStr;
  }
  return html`
    <div class="navigation-summary-widget">
      <div class="navigation-summary-title">${() => name}</div>
      <div class="summary-row">
        <span class="emoji">🛣️</span>
        <span class="label">Distance:</span>
        <span class="value">${safeDistance === null ? "—" : formatMetersToHuman(safeDistance, isMetric)}</span>
      </div>
      <div class="summary-row">
        <span class="emoji">⌛</span>
        <span class="label">Duration:</span>
        <span class="value">${safeDuration === null ? "—" : formatSecondsToHuman(safeDuration)}</span>
      </div>
      <div class="summary-row">
        <span class="emoji">🕗</span>
        <span class="label">ETA:</span>
        <span class="value">${etaString}</span>
      </div>
      <div class="buttonCluster">
        ${() =>
          isConfirmed()
            ? html`<button class="cancel" @click="${cancelNavigation}"><i class="bi bi-x-lg"></i> Cancel Navigation</button>`
            : html`<button class="directions" @click="${confirmDestination}"><i class="bi bi-sign-turn-right"></i> Start Navigation</button>`}
        <button class="favorite" @click="${toggleFavorite}">${isFavorited ? "💔 Unfavorite" : "❤️ Favorite"}</button>
      </div>
    </div>
  `;
}
