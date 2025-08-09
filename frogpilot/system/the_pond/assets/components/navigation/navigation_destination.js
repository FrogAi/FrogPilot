import { html, reactive } from "https://esm.sh/@arrow-js/core";
import {
  addRouteToMap,
  formatMetersToHuman,
  formatSecondsToHuman,
  getCoordinatesFromSearch,
  getRoutes,
  removeRouteFromMap,
  getOrdinalSuffix,
  highlightRoute,
} from "./navigation_utilities.js";
import { Modal } from "/assets/components/modal.js";

function sha1hex(str) {
  const rot = (v, s) => (v << s) | (v >>> (32 - s));
  const bytes = new TextEncoder().encode(str);
  const words = [];
  for (let i = 0; i < bytes.length; i++) {
    words[i >> 2] |= bytes[i] << ((3 - (i & 3)) << 3);
  }
  const bitLen = bytes.length << 3;
  words[bitLen >> 5] |= 0x80 << (24 - (bitLen & 31));
  words[((bitLen + 64 >> 9) << 4) + 15] = bitLen;
  let [a, b, c, d, e] = [0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0];
  const w = new Array(80);
  for (let i = 0; i < words.length; i += 16) {
    for (let t = 0; t < 16; t++) w[t] = words[i + t] | 0;
    for (let t = 16; t < 80; t++) {
      w[t] = rot(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
    }
    let [aa, bb, cc, dd, ee] = [a, b, c, d, e];
    for (let t = 0; t < 80; t++) {
      const k = t < 20 ? 0x5a827999 : t < 40 ? 0x6ed9eba1 : t < 60 ? 0x8f1bbcdc : 0xca62c1d6;
      const f = t < 20 ? (bb & cc) | (~bb & dd) : t < 40 ? bb ^ cc ^ dd : t < 60 ? (bb & cc) | (bb & dd) | (cc & dd) : bb ^ cc ^ dd;
      const tmp = (rot(aa, 5) + f + ee + k + w[t]) >>> 0;
      ee = dd;
      dd = cc;
      cc = rot(bb, 30) >>> 0;
      bb = aa;
      aa = tmp;
    }
    a = (a + aa) >>> 0;
    b = (b + bb) >>> 0;
    c = (c + cc) >>> 0;
    d = (d + dd) >>> 0;
    e = (e + ee) >>> 0;
  }
  return [a, b, c, d, e].map(x => x.toString(16).padStart(8, "0")).join("");
}

async function geometryHashFromRoute(route) {
  const flat = route.geometry.coordinates.flat().join(",");
  if (crypto?.subtle?.digest && window.isSecureContext) {
    const buf = await crypto.subtle.digest("SHA-1", new TextEncoder().encode(flat));
    return [...new Uint8Array(buf)].map(b => b.toString(16).padStart(2, "0")).join("");
  }
  return sha1hex(flat);
}

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
    await fetch("/api/navigation/favorite/rename", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body)
    });
    showSnackbar(message);
    const sorted = await loadFavoritesAlphabetically();
    state.suggestions = "[]";
    await new Promise(resolve => setTimeout(resolve, 0));
    state.suggestions = JSON.stringify(sorted);
  } catch {
    showSnackbar(`Failed to update ${type} location...`);
  }
}

export function NavDestination() {
  let map;
  let destinationMarker;
  let favoriteMarkers = [];
  const state = reactive({
    amap1Key: undefined,
    amap2Key: undefined,
    canToggleProvider: false,
    confirmedRoute: null,
    confirmedRouteRefresh: 0,
    destination: undefined,
    favoriteToRemove: null,
    favoriteToRename: null,
    favoritesCount: 0,
    favoritesVisible: false,
    initialized: false,
    isMetric: true,
    lastPosition: undefined,
    loadingRoute: false,
    mapboxPublic: undefined,
    mapboxSecret: undefined,
    missingKeys: null,
    newFavoriteName: "",
    previousDestinations: "[]",
    searchProvider: "mapbox",
    selectedRoute: null,
    showRemoveFavoriteModal: false,
    showRenameFavoriteModal: false,
    suggestions: "[]"
  });
  const searchFieldState = reactive({ value: "" });
  const sessionToken = crypto.randomUUID?.() || Math.random().toString(36).slice(2);

  function areRoutesEqual(a, b) {
    return a?.routeHash && b?.routeHash && a.routeHash === b.routeHash;
  }

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
        const routeHash = await geometryHashFromRoute(selectedRouteData);
        const selected = {
          name,
          duration: selectedRouteData.duration,
          distance: selectedRouteData.distance,
          destinationCoordinates: coords,
          startingCoordinates: [state.lastPosition.longitude, state.lastPosition.latitude],
          routeId: selectedRouteId,
          routeHash,
          steps: selectedRouteData?.legs?.[0]?.steps || []
        };

        state.selectedRoute = selected;
        if (resume) state.confirmedRoute = JSON.parse(JSON.stringify(selected));

        localStorage.setItem("lastRouteId", selected.routeId);

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
              routeId,
              steps: route?.legs?.[0]?.steps || []
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
      showSnackbar("Failed to calculate route…");
    } finally {
      state.loadingRoute = false;
    }
  }

  async function getNavigationData() {
    const res = await fetch("/api/navigation");
    const data = await res.json();
    state.mapboxPublic = data.mapboxPublic.trim();
    state.mapboxSecret = data.mapboxSecret.trim();
    state.amap1Key = data.amap1Key?.trim() || "";
    state.amap2Key = data.amap2Key?.trim() || "";
    state.isMetric = data.isMetric ?? true;
    const hasMapbox = !!state.mapboxPublic && !!state.mapboxSecret;
    const hasAMap = !!state.amap1Key && !!state.amap2Key;
    state.missingKeys = !hasMapbox;
    state.canToggleProvider = hasMapbox && hasAMap;
    state.searchProvider = hasMapbox ? "mapbox" : "";
    if (state.missingKeys) return;
    state.lastPosition = {
      latitude: parseFloat(data.lastPosition.latitude),
      longitude: parseFloat(data.lastPosition.longitude)
    };
    try {
      state.destination = JSON.parse(data.destination);
    } catch {}
    try {
      const prev = JSON.parse(data.previousDestinations);
      state.previousDestinations = prev.map(d => ({ name: d.place_name }));
      state.suggestions = JSON.stringify(state.previousDestinations);
    } catch {}
    setupMap();
    loadFavoritesAlphabetically();
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
      clearTimeout(window.searchTimeout);
      const val = e.target.value.trim();
      searchFieldState.value = e.target.value;
      if (val.length < 3) {
        if (val.length === 0) state.suggestions = "[]";
        return;
      }
      state.selectedRoute = null;
      state.confirmedRoute = null;
      state.suggestions = "[]";
      if (state.searchProvider === "mapbox") {
        const prox = `${state.lastPosition.longitude},${state.lastPosition.latitude}`;
        const params = new URLSearchParams({
          proximity: prox,
          access_token: state.mapboxPublic,
          session_token: sessionToken,
          q: val,
          limit: 4
        });
        const res = await fetch(`https://api.mapbox.com/search/searchbox/v1/suggest?${params}`);
        const data = await res.json();
        state.suggestions = JSON.stringify(data.suggestions);
      } else {
        const auto = new AMap.Autocomplete({ city: "auto" });
        auto.search(val, (status, result) => {
          if (status === "complete" && result.tips) {
            state.suggestions = JSON.stringify(result.tips);
          }
        });
      }
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
      const json = await res.json();
      const sorted = json.favorites.sort((a, b) => (a.name || "").localeCompare(b.name || ""));
      state.favoritesCount = sorted.length;
      state.favoriteRoutes = sorted;
      addFavoriteMarkers(sorted);
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
      await fetch("/api/navigation/favorite", {
        method: "DELETE",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ id, name, latitude, longitude, routeId })
      });
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
      await fetch("/api/navigation/favorite", {
        method: "DELETE",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(fav)
      });

      await fetch("/api/navigation/favorite", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          name: newName,
          longitude: fav.longitude,
          latitude: fav.latitude,
          routeId: fav.routeId
        })
      });

      if (state.favoritesVisible) {
        state.suggestions = "[]";
        state.favoritesVisible = false;
      }

      handleFavoritesClick();

      showSnackbar(`"${fav.name}" renamed to "${newName}"!`, "success");
    } catch {
      showSnackbar("Failed to edit favorite name…");
    } finally {
      state.showRenameFavoriteModal = false;
    }
  }

  async function searchInput(e) {
    const newVal = e.target.value.trim();
    searchFieldState.value = e.target.value;
    clearTimeout(window.searchTimeout);
    window.searchTimeout = setTimeout(async () => {
      const val = newVal;
      if (val.length < 3) {
        if (val.length === 0) {
          state.suggestions = "[]";
        }
        return;
      }
      state.selectedRoute = null;
      state.confirmedRoute = null;
      state.suggestions = "[]";
      if (state.searchProvider === "mapbox") {
        const prox = `${state.lastPosition.longitude},${state.lastPosition.latitude}`;
        const params = new URLSearchParams({
          proximity: prox,
          access_token: state.mapboxPublic,
          session_token: sessionToken,
          q: val,
          limit: 4
        });
        const res = await fetch(`https://api.mapbox.com/search/searchbox/v1/suggest?${params}`);
        const data = await res.json();
        state.suggestions = JSON.stringify(data.suggestions);
      } else {
        const auto = new AMap.Autocomplete({ city: "auto" });
        auto.search(val, (status, result) => {
          if (status === "complete" && result.tips) {
            state.suggestions = JSON.stringify(result.tips);
          }
        });
      }
    }, 800);
  }

  async function selectSuggestion(sugg) {
    const label = sugg.full_address || sugg.name || sugg.address || "Unnamed Location";
    let coords;
    if (sugg.routeId) {
      initiateNavigation({
        name: sugg.name,
        longitude: sugg.longitude,
        latitude: sugg.latitude,
        routeId: sugg.routeId
      });
      return;
    }
    state.loadingRoute = true;
    try {
      if (state.searchProvider === "mapbox") {
        if (sugg.geometry && Array.isArray(sugg.geometry.coordinates)) {
          coords = sugg.geometry.coordinates;
        } else if (sugg.mapbox_id) {
          const url = new URL(`https://api.mapbox.com/search/searchbox/v1/retrieve/${encodeURIComponent(sugg.mapbox_id)}`);
          url.searchParams.set("access_token", state.mapboxPublic);
          url.searchParams.set("session_token", sessionToken);
          const ret = await fetch(url);
          const retJson = await ret.json();
          coords = retJson.features[0].geometry.coordinates;
        } else {
          coords = await getCoordinatesFromSearch(label, state.mapboxPublic);
        }
      } else {
        coords = [sugg.location.lng, sugg.location.lat];
      }
      if (coords) {
        initiateNavigation({
          name: label,
          longitude: coords[0],
          latitude: coords[1],
          routeId: null
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
    if (!state.mapboxPublic || state.initialized) return;
    const container = document.getElementById("map");
    if (!container) {
      requestAnimationFrame(setupMap);
      return;
    }
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
        const savedId = localStorage.getItem("activeRouteId");
        initiateNavigation({ ...state.destination, routeId: savedId }, { resume: true });
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

  getNavigationData();

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
                  <a href="/navigation_keys" class="keys-required-button">Go to "Manage Keys"</a>
                </div>
              </section>
            `
          : html`
              <div class="map-wrapper">
                <div class="search-wrapper">
                  <div class="search-controls">
                    <input autocomplete="off" id="search-field" placeholder="Search here" value="${() => searchFieldState.value}" @input="${searchInput}" @keydown="${handleSearchKey}" />
                    ${() => (state.favoritesCount > 0 ? html`<button class="favorites-toggle-button" @click="${handleFavoritesClick}">❤️ Favorites</button>` : "")}
                    ${() => (state.canToggleProvider ? html`
                      <div class="search-provider-toggle">
                        <button class="${() => (state.searchProvider === "amap" ? "active" : "")}" @click="${() => { state.searchProvider = "amap"; state.suggestions = "[]"; }}">AMap</button>
                        <button class="${() => (state.searchProvider === "mapbox" ? "active" : "")}" @click="${() => { state.searchProvider = "mapbox"; state.suggestions = "[]"; }}">Mapbox</button>
                      </div>
                    ` : "")}
                  </div>
                  <div id="infobox">
                    ${() => {
                      if (state.loadingRoute) {
                        return html`<div class="navigation-summary-widget loading-status"><span class="spinner"></span> Calculating route...</div>`;
                      } else if (state.selectedRoute) {
                        return NavigationDestination({
                          ...state.selectedRoute,
                          isFavorited: isRouteFavorited(state.selectedRoute, state.favoriteRoutes),
                          isConfirmed: () => areRoutesEqual(state.selectedRoute, state.confirmedRoute),
                          map,
                          isMetric: state.isMetric,
                          cancelNavigationFn: () => {
                            state.selectedRoute = null;
                            state.confirmedRoute = null;
                            state.suggestions = state.previousDestinations;
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
      message: `Are you sure you want to remove <strong>${state.favoriteToRemove?.name}</strong> from your favorites?`,
      onConfirm: removeFavorite,
      onCancel: () => { state.showRemoveFavoriteModal = false; state.favoriteToRemove = null; },
      confirmText: "Remove"
    }) : "")}
    ${() => (state.showRenameFavoriteModal ? Modal({
      title: "Rename Favorite",
      message: html`
        <div>
          <p>Rename <strong>${state.favoriteToRename.name}</strong> to:</p>
          <div style="margin-top: 10px;">
            <input class="modal-input" type="text" value="${state.newFavoriteName}" @click="${e => e.stopPropagation()}" @input="${e => state.newFavoriteName = e.target.value}" />
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
    <div class="suggestion-item" @click="${() => selectSuggestion(s)}">
      <p>
        ${s.is_home ? "🏠 " : ""}
        ${s.is_work ? "💼 " : ""}
        ${s.name || s.address}
      </p>
      ${isFavorite(s) ? html`
        <div class="favorite-actions">
          <button class="home-favorite-button ${s.is_home ? "active" : ""}" title="Set as Home" @click="${e => { e.stopPropagation(); setHome(s); }}">🏠</button>
          <button class="work-favorite-button ${s.is_work ? "active" : ""}" title="Set as Work" @click="${e => { e.stopPropagation(); setWork(s); }}">💼</button>
          <button class="edit-favorite-button" title="Rename Favorite" @click="${e => { e.stopPropagation(); renameFavorite(s); }}">✏️</button>
          <button class="remove-favorite-button" title="Remove from Favorites" @click="${e => { e.stopPropagation(); removeFavorite(s); }}">🗑️</button>
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
  routeHash,
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
  favoriteRoutes = [],
  steps = []
}) {
  async function cancelNavigation() {
    showSnackbar("Navigation cancelled...");
    removeRouteFromMap(map);
    cancelNavigationFn();
    localStorage.removeItem("activeRouteId");
    map.flyTo({ center: startingCoordinates, zoom: 15, pitch: 45, speed: 1, curve: 1 });
    await fetch("/api/navigation", { method: "DELETE" });
  }
  async function confirmDestination() {
    onConfirm?.();
    showSnackbar("Navigation set!");
    localStorage.setItem("activeRouteId", routeId);
    await fetch("/api/navigation", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        name,
        longitude: destinationCoordinates[0],
        latitude: destinationCoordinates[1]
      })
    });
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
  }
  async function favoriteDestination() {
    try {
      const res = await fetch("/api/navigation/favorite", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          name,
          longitude: destinationCoordinates[0],
          latitude: destinationCoordinates[1],
          routeId
        })
      });
      const { message } = await res.json();
      showSnackbar(message || "Added to favorites!");
      await loadFavorites();
    } catch {
      showSnackbar("Failed to add to favorites…");
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
        showSnackbar("Couldn’t find favorite entry…");
      }
    } else {
      await favoriteDestination();
    }
  }
  const eta = new Date(Date.now() + duration * 1000);
  const isLong = duration > 86400;
  const timeStr = eta.toLocaleTimeString([], { hour: "numeric", minute: "2-digit" });
  const month = eta.toLocaleString([], { month: "long" });
  const day = eta.getDate();
  const year = eta.getFullYear();
  const etaString = isLong ? `${month} ${day}${getOrdinalSuffix(day)}, ${year}, ${timeStr}` : timeStr;
  return html`
    <div class="navigation-summary-widget">
      <div class="navigation-summary-title">${name}</div>
      <div class="summary-row">
        <span class="emoji">🛣️</span>
        <span class="label">Distance:</span>
        <span class="value">${formatMetersToHuman(distance, isMetric)}</span>
      </div>
      <div class="summary-row">
        <span class="emoji">⌛</span>
        <span class="label">Duration:</span>
        <span class="value">${formatSecondsToHuman(duration)}</span>
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
