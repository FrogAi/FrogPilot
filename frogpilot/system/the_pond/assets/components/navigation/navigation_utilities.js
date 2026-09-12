let mapboxPromise = null;

export function ensureMapboxLoaded() {
  if (window.mapboxgl) return Promise.resolve();
  if (mapboxPromise) return mapboxPromise;

  mapboxPromise = new Promise((resolve, reject) => {
    const stylesheet = document.createElement("link");
    stylesheet.rel = "stylesheet";
    stylesheet.href = "/assets/vendor/mapbox-gl/mapbox-gl.css";
    document.head.appendChild(stylesheet);

    const script = document.createElement("script");
    script.src = "/assets/vendor/mapbox-gl/mapbox-gl.js";
    script.onload = resolve;
    script.onerror = () => {
      mapboxPromise = null;
      stylesheet.remove();
      script.remove();
      reject(new Error("Mapbox could not be loaded"));
    };
    document.head.appendChild(script);
  });
  return mapboxPromise;
}

async function fetchMapbox(url) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 10000);
  try {
    const response = await fetch(url, { signal: controller.signal });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    return await response.json();
  } finally {
    clearTimeout(timeout);
  }
}

export function highlightRoute(map, routes, selectedRouteId) {
  if (!map.isStyleLoaded() || !routes) return;
  routes.forEach((route, idx) => {
    const routeId = idx === 0 ? "main" : `alt-${idx}`;
    const layerId = `route-line-${routeId}`;
    if (map.getLayer(layerId)) {
      const isSelected = routeId === selectedRouteId;
      map.setPaintProperty(layerId, "line-width", isSelected ? 5 : 3);
      map.setPaintProperty(layerId, "line-opacity", isSelected ? 1 : 0.5);
      if (isSelected) {
        map.moveLayer(layerId);
      }
    }
  });
}

function addRouteSource(map, sourceId, feature) {
  if (map.getSource(sourceId)) {
    const layerId = `route-line-${sourceId.replace("route-", "")}`;
    const clickLayerId = `route-click-${sourceId.replace("route-", "")}`;
    if (map.getLayer(layerId)) map.removeLayer(layerId);
    if (map.getLayer(clickLayerId)) map.removeLayer(clickLayerId);
    map.removeSource(sourceId);
  }
  map.addSource(sourceId, {
    type: "geojson",
    data: { type: "FeatureCollection", features: [feature] },
    lineMetrics: true
  });
}

function addRouteLayers(map, sourceId, layerId, clickLayerId, route) {
  map.addLayer({
    id: layerId,
    type: "line",
    source: sourceId,
    layout: { 'line-cap': "round", 'line-join': "round" },
    paint: {
      'line-width': 3,
      'line-opacity': 0.5,
      'line-gradient': buildGradientExpression(route.geometry.coordinates, route.legs[0].annotation.congestion)
    }
  });
  map.addLayer({
    id: clickLayerId,
    type: "line",
    source: sourceId,
    layout: { 'line-cap': "round", 'line-join': "round" },
    paint: { 'line-width': 25, 'line-opacity': 0 }
  });
}

function buildTooltipRow(emoji, label, value) {
  const row = document.createElement("div");
  row.className = "tooltip-row";
  const emojiSpan = document.createElement("span");
  emojiSpan.className = "emoji";
  emojiSpan.textContent = emoji;
  const labelSpan = document.createElement("span");
  labelSpan.className = "label";
  labelSpan.textContent = label;
  const valueSpan = document.createElement("span");
  valueSpan.className = "value";
  valueSpan.textContent = value;
  row.appendChild(emojiSpan);
  row.appendChild(labelSpan);
  row.appendChild(valueSpan);
  return row;
}

function handleRouteEvents(map, clickLayerId, onRouteSelect, routes, useMetric, feature, getSelectedRouteId) {
  const layerId = clickLayerId.replace("click", "line");
  const showTooltip = (e) => {
    map.getCanvas().style.cursor = "pointer";
    map.setPaintProperty(layerId, "line-width", 5);
    map.setPaintProperty(layerId, "line-opacity", 1);
    document.querySelectorAll(".mapboxgl-popup.route-tooltip").forEach(p => p.remove());
    const props = feature.properties;
    const duration = formatSecondsToHuman(props.duration);
    const distance = formatMetersToHuman(props.distance, useMetric);
    const arrival = new Date(Date.now() + props.duration * 1000);
    const isLong = props.duration > 24 * 3600;
    const timeStr = arrival.toLocaleTimeString([], { hour: "numeric", minute: "2-digit" });
    const month = arrival.toLocaleString([], { month: "long" });
    const day = arrival.getDate();
    const year = arrival.getFullYear();
    const suffix = getOrdinalSuffix(day);
    const eta = isLong ? `${month} ${day}${suffix}, ${year}, ${timeStr}` : timeStr;
    const tooltip = document.createElement("div");
    tooltip.className = "custom-tooltip";
    tooltip.style.whiteSpace = "nowrap";
    tooltip.appendChild(buildTooltipRow("🛣️", "Distance:", distance));
    tooltip.appendChild(buildTooltipRow("⌛", "Duration:", duration));
    tooltip.appendChild(buildTooltipRow("🕗", "ETA:", eta));
    new mapboxgl.Popup({ closeButton: false, closeOnClick: true, className: "route-tooltip", maxWidth: "none" })
      .setLngLat(e.lngLat)
      .setDOMContent(tooltip)
      .addTo(map);
  };

  const onClick = (e) => {
    e.preventDefault();
    const routeId = feature.properties.routeId;
    const route = routes.find((r, i) => (i === 0 ? "main" : `alt-${i}`) === routeId);
    onRouteSelect(route, routeId);
    showTooltip(e);
  };
  const onLeave = () => {
    map.getCanvas().style.cursor = "";
    highlightRoute(map, routes, getSelectedRouteId());
    document.querySelectorAll(".mapboxgl-popup").forEach(p => p.remove());
  };

  map.__routeHandlers = map.__routeHandlers || {};
  const prev = map.__routeHandlers[clickLayerId];
  if (prev) {
    map.off("click", clickLayerId, prev.click);
    map.off("mouseenter", clickLayerId, prev.enter);
    map.off("mouseleave", clickLayerId, prev.leave);
  }
  map.on("click", clickLayerId, onClick);
  map.on("mouseenter", clickLayerId, showTooltip);
  map.on("mouseleave", clickLayerId, onLeave);
  map.__routeHandlers[clickLayerId] = { click: onClick, enter: showTooltip, leave: onLeave };
}

export function addRouteToMap(map, routes, start, dest, onRouteSelect, useMetric = true, getSelectedRouteId) {
  routes.forEach((route, idx) => {
    const routeId = idx === 0 ? "main" : `alt-${idx}`;
    const sourceId = `route-${routeId}`;
    const layerId = `route-line-${routeId}`;
    const clickLayerId = `route-click-${routeId}`;
    const feature = {
      type: "Feature",
      geometry: { type: "LineString", coordinates: route.geometry.coordinates },
      properties: {
        congestion: route.legs[0].annotation.congestion,
        routeId,
        duration: route.duration,
        distance: route.distance
      }
    };
    addRouteSource(map, sourceId, feature);
    addRouteLayers(map, sourceId, layerId, clickLayerId, route);
    handleRouteEvents(map, clickLayerId, onRouteSelect, routes, useMetric, feature, getSelectedRouteId);
  });

  map.once("idle", () => {
    highlightRoute(map, routes, getSelectedRouteId());
  });

  if (!map.__popupClickBound) {
    map.__popupClickBound = true;
    map.on("click", (e) => {
      setTimeout(() => {
        if (!e.defaultPrevented) {
          document.querySelectorAll(".mapboxgl-popup").forEach(p => p.remove());
        }
      }, 100);
    });
  }

  const padding = window.innerWidth < 600 ? 100 : 250;
  map.fitBounds([start, dest], { padding, duration: 1000 });
}

export async function getCoordinatesFromSearch(searchValue, mapboxPublic) {
  const params = new URLSearchParams({ access_token: mapboxPublic, q: searchValue });
  const data = await fetchMapbox(`https://api.mapbox.com/search/geocode/v6/forward?${params.toString()}`);
  return data.features[0].geometry.coordinates;
}

export async function getRoutes(from, to, mapboxPublic) {
  const url = `https://api.mapbox.com/directions/v5/mapbox/driving-traffic/${from};${to}?geometries=geojson&annotations=congestion&overview=full&alternatives=true&access_token=${mapboxPublic}`;
  const data = await fetchMapbox(url);
  return data.routes;
}

function buildGradientExpression(coords, congestion) {
  const count = congestion.length;
  if (count === 0 || coords.length < 2) {
    return ["interpolate", ["linear"], ["line-progress"], 0, "#ccc", 1, "#ccc"];
  }
  if (count === 1) {
    const color = congestionToColor(congestion[0] || "unknown");
    return ["interpolate", ["linear"], ["line-progress"], 0, color, 1, color];
  }
  const stops = [];
  for (let i = 0; i < count; i++) {
    stops.push(i / (count - 1), congestionToColor(congestion[i] || "unknown"));
  }
  return ["interpolate", ["linear"], ["line-progress"], ...stops];
}

export function removeRouteFromMap(map) {
  if (!map || !map.getStyle || !map.getStyle()) return;
  const layers = map.getStyle().layers || [];
  layers.forEach(l => {
    if ((l.id.startsWith("route-line-") || l.id.startsWith("route-click-")) && map.getLayer(l.id)) {
      map.removeLayer(l.id);
    }
  });
  const sources = map.getStyle().sources || {};
  Object.keys(sources).forEach(id => {
    if (id.startsWith("route-") && map.getSource(id)) map.removeSource(id);
  });
}

export function formatSecondsToHuman(s) {
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  return h > 0 ? `${h}h ${m} min` : `${m} min`;
}

export function formatMetersToHuman(m, metric = true) {
  return metric ? (m >= 1000 ? `${(m / 1000).toFixed(1)} km` : `${Math.round(m)} m`) : (m * 3.28084 >= 5280 ? `${((m * 3.28084) / 5280).toFixed(1)} mi` : `${Math.round(m * 3.28084)} ft`);
}

function congestionToColor(lvl) {
  const map = { low: "#2ecc71", moderate: "#f1c40f", heavy: "#e67e22", severe: "#e74c3c", unknown: "#2ecc71" };
  return map[lvl] || "#999";
}

export function getOrdinalSuffix(n) {
  const s = ["th", "st", "nd", "rd"];
  const v = n % 100;
  return s[(v - 20) % 10] || s[v] || s[0];
}
