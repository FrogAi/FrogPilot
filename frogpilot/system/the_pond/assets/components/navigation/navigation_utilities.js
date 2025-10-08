export function highlightRoute(map, routes, selectedRouteId) {
  if (!map.isStyleLoaded() || !routes) return;
  routes.forEach((route, idx) => {
    const routeId = idx === 0 ? 'main' : `alt-${idx}`;
    const layerId = `route-line-${routeId}`;
    if (map.getLayer(layerId)) {
      const isSelected = routeId === selectedRouteId;
      map.setPaintProperty(layerId, 'line-width', isSelected ? 5 : 3);
      map.setPaintProperty(layerId, 'line-opacity', isSelected ? 1 : 0.5);
      if (isSelected) {
        map.moveLayer(layerId);
      }
    }
  });
}

function addRouteSource(map, sourceId, feature) {
  if (map.getSource(sourceId)) {
    const layerId = `route-line-${sourceId.replace('route-', '')}`;
    const clickLayerId = `route-click-${sourceId.replace('route-', '')}`;
    if (map.getLayer(layerId)) map.removeLayer(layerId);
    if (map.getLayer(clickLayerId)) map.removeLayer(clickLayerId);
    map.removeSource(sourceId);
  }
  map.addSource(sourceId, {
    type: 'geojson',
    data: { type: 'FeatureCollection', features: [feature] },
    lineMetrics: true
  });
}

function addRouteLayers(map, sourceId, layerId, clickLayerId, route) {
  map.addLayer({
    id: layerId,
    type: 'line',
    source: sourceId,
    layout: { 'line-cap': 'round', 'line-join': 'round' },
    paint: {
      'line-width': 3,
      'line-opacity': 0.5,
      'line-gradient': buildGradientExpression(route.geometry.coordinates, route.legs[0].annotation.congestion)
    }
  });
  map.addLayer({
    id: clickLayerId,
    type: 'line',
    source: sourceId,
    layout: { 'line-cap': 'round', 'line-join': 'round' },
    paint: { 'line-width': 25, 'line-opacity': 0 }
  });
}

function safeGetId(fn) {
  try { return fn?.() ?? null } catch { return null }
}

function handleRouteEvents(map, clickLayerId, onRouteSelect, routes, useMetric, feature, getSelectedRouteId) {
  const layerId = clickLayerId.replace('click', 'line');
  const showTooltip = (e) => {
    map.getCanvas().style.cursor = 'pointer';
    map.setPaintProperty(layerId, 'line-width', 5);
    map.setPaintProperty(layerId, 'line-opacity', 1);
    document.querySelectorAll('.mapboxgl-popup.route-tooltip').forEach(p => p.remove());
    const props = feature.properties;
    const duration = useMetric ? formatSecondsToHuman(props.duration) : formatSecondsToAmerican(props.duration);
    const distance = useMetric ? formatMetersToHuman(props.distance, true) : formatMetersToMiles(props.distance);
    const arrival = new Date(Date.now() + props.duration * 1000);
    const isLong = props.duration > 24 * 3600;
    const timeStr = arrival.toLocaleTimeString([], { hour: 'numeric', minute: '2-digit' });
    const month = arrival.toLocaleString([], { month: 'long' });
    const day = arrival.getDate();
    const year = arrival.getFullYear();
    const suffix = getOrdinalSuffix(day);
    const eta = isLong ? `${month} ${day}${suffix}, ${year}, ${timeStr}` : timeStr;
    const tooltip = document.createElement('div');
    tooltip.className = 'custom-tooltip';
    tooltip.style.whiteSpace = 'nowrap';
    tooltip.innerHTML = `
      <div class="tooltip-row"><span class="emoji">🛣️</span><span class="label">Distance:</span><span class="value">${distance}</span></div>
      <div class="tooltip-row"><span class="emoji">⌛</span><span class="label">Duration:</span><span class="value">${duration}</span></div>
      <div class="tooltip-row"><span class="emoji">🕗</span><span class="label">ETA:</span><span class="value">${eta}</span></div>
    `;
    new mapboxgl.Popup({ closeButton: false, closeOnClick: true, className: 'route-tooltip', maxWidth: 'none' })
      .setLngLat(e.lngLat)
      .setDOMContent(tooltip)
      .addTo(map);
  };

  map.on('click', clickLayerId, (e) => {
    e.preventDefault();
    const routeId = feature.properties.routeId;
    const route = routes.find((r, i) => (i === 0 ? 'main' : `alt-${i}`) === routeId);
    onRouteSelect(route, routeId);
    showTooltip(e);
  });
  map.on('mouseenter', clickLayerId, showTooltip);
  map.on('mouseleave', clickLayerId, () => {
    map.getCanvas().style.cursor = '';
    const id = safeGetId(getSelectedRouteId);
    highlightRoute(map, routes, id);
    document.querySelectorAll('.mapboxgl-popup').forEach(p => p.remove());
  });
}

export function addRouteToMap(map, routes, start, dest, onRouteSelect, useMetric = true, getSelectedRouteId) {
  routes.forEach((route, idx) => {
    const routeId = idx === 0 ? 'main' : `alt-${idx}`;
    const sourceId = `route-${routeId}`;
    const layerId = `route-line-${routeId}`;
    const clickLayerId = `route-click-${routeId}`;
    const feature = {
      type: 'Feature',
      geometry: { type: 'LineString', coordinates: route.geometry.coordinates },
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

  map.once('idle', () => {
    const id = safeGetId(getSelectedRouteId);
    highlightRoute(map, routes, id);
  });

  map.on('click', (e) => {
    setTimeout(() => {
      if (!e.defaultPrevented) {
        document.querySelectorAll('.mapboxgl-popup').forEach(p => p.remove());
      }
    }, 100);
  });


  const padding = window.innerWidth < 600 ? 100 : 250;
  map.fitBounds([start, dest], { padding, duration: 1000 });
}

export async function getCoordinatesFromSearch(searchValue, mapboxPublic) {
  const params = new URLSearchParams({ access_token: mapboxPublic, q: searchValue });
  const response = await fetch(`https://api.mapbox.com/search/geocode/v6/forward?${params.toString()}`);
  const data = await response.json();
  return data.features[0].geometry.coordinates;
}

export async function getRoutes(from, to, mapboxPublic) {
  const url = `https://api.mapbox.com/directions/v5/mapbox/driving-traffic/${from};${to}?geometries=geojson&annotations=congestion&overview=full&alternatives=true&access_token=${mapboxPublic}`;
  const response = await fetch(url);
  const data = await response.json();
  return data.routes;
}

function buildGradientExpression(coords, congestion) {
  const count = congestion.length;
  if (count === 0 || coords.length < 2) {
    return ['interpolate', ['linear'], ['line-progress'], 0, '#ccc', 1, '#ccc'];
  }
  const stops = [];
  for (let i = 0; i < count; i++) {
    stops.push(i / (count - 1), congestionToColor(congestion[i] || 'unknown'));
  }
  return ['interpolate', ['linear'], ['line-progress'], ...stops];
}

export function removeRouteFromMap(map) {
  if (!map || !map.getStyle || !map.getStyle()) return;
  const layers = map.getStyle().layers || [];
  layers.forEach(l => {
    if ((l.id.startsWith('route-line-') || l.id.startsWith('route-click-')) && map.getLayer(l.id)) {
      map.removeLayer(l.id);
    }
  });
  const sources = map.getStyle().sources || {};
  Object.keys(sources).forEach(id => {
    if (id.startsWith('route-') && map.getSource(id)) map.removeSource(id);
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

export function formatSecondsToAmerican(s) {
  const mins = Math.round(s / 60);
  const h = Math.floor(mins / 60);
  const m = mins % 60;
  return h > 0 ? `${h} hr ${m} min` : `${m} min`;
}

export function formatMetersToMiles(m) {
  const miles = m / 1609.34;
  return miles >= 0.1 ? `${miles.toFixed(1)} mi` : `${(miles * 5280).toFixed(0)} ft`;
}

function congestionToColor(lvl) {
  const map = { low: '#2ecc71', moderate: '#f1c40f', heavy: '#e67e22', severe: '#e74c3c', unknown: '#2ecc71' };
  return map[lvl] || '#999';
}

export function getOrdinalSuffix(n) {
  const s = ['th', 'st', 'nd', 'rd'];
  const v = n % 100;
  return s[(v - 20) % 10] || s[v] || s[0];
}
