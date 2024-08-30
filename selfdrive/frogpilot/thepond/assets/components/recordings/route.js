import { html, reactive } from "https://esm.sh/@arrow-js/core"
import { Link } from "../router.js"

/**
 * @typedef {Object} Route
 * @prop {string} date - The date of the route
 * @prop {string[]} segment_urls - The URLs for the segments of the route
 * @prop {string} gif - The URL for the GIF preview
 * @prop {string} png - The URL for the PNG preview
 * @prop {string[]} available_cameras - The available cameras for the route
 * @prop {number} total_duration - The total duration of the route in seconds
 * @prop {string} name - The name of the route
 */

export function RecordedRoute({ params }) {
  let state = reactive({
    route: undefined,
    playing: true,
    currentIndex: 0,
    isSeeking: false,
    currentTime: "",
    selectedCamera: "forward",
  })

  async function fetchRoute() {
    let response = await fetch(`/api/routes/${params.routeDate}`)
    let data = await response.json()
    state.route = data
  }
  fetchRoute()

  function playPauseHandler() {
    console.log("Play/pause")
    const videoElement = document.getElementById("video")
    if (videoElement.paused) {
      videoElement.play()
    } else {
      videoElement.pause()
    }
    state.playing = !state.playing
  }

  function fullscreenHandler() {
    const videoElement = document.getElementById("video")
    if (videoElement.requestFullscreen) {
      videoElement.requestFullscreen()
    } else if (videoElement.mozRequestFullScreen) {
      videoElement.mozRequestFullScreen()
    } else if (videoElement.webkitRequestFullscreen) {
      videoElement.webkitRequestFullscreen()
    } else if (videoElement.msRequestFullscreen) {
      videoElement.msRequestFullscreen()
    }
  }

  function videoEndedHandler(e) {
    const videoElement = e.target
    state.currentIndex++
    if (state.currentIndex >= state.route.segment_urls.length) {
      state.currentIndex = 0
    }
    videoElement.src = state.route.segment_urls[state.currentIndex]
    videoElement.load()
    videoElement.play()
  }

  function timeupdateHandler(e) {
    const videoElement = e.target
    if (state.isSeeking) {
      return
    }
    const currentTime = Math.round(videoElement.currentTime)
    const actualTime = state.currentIndex * 60 + currentTime
    state.currentTime = actualTime
  }

  async function handleSeek(e) {
    const value = e.target.value
    const videoElement = document.getElementById("video")
    // Find which segment the slider is in (each segment is 60 seconds)
    const desiredIndex = Math.floor(value / 60)
    if (desiredIndex != state.currentIndex) {
      console.log("Switching to segment: ", desiredIndex)
      state.currentIndex = desiredIndex
      videoElement.src = getUrlForIndex(
        state.route,
        state.selectedCamera,
        state.currentIndex
      )
      videoElement.load()
      videoElement.play()
      // Really weird behaviour in FF, when the video is just
      // loaded the duration is ~1, so seeking to 30 causes
      // the video to end. So we wait for the duration to be more than 2
      for (let i = 0; i < 10; i++) {
        await wait(100)
        if (videoElement.duration > 2) {
          break
        }
      }
    }
    // Seek to the correct time in the video based on the slider value
    const correctTime = value - state.currentIndex * 60
    videoElement.currentTime = correctTime
  }

  function renderRoute(route) {
    const isWideAvailable = route.available_cameras.includes("wide")
    const isDriverAvailable = route.available_cameras.includes("driver")
    const formattedDate = new Date(route.date).toLocaleString()
    const formattedDuration = formatSeconds(route.total_duration)

    return html`
      <h1 id="route_name">${formattedDate}</h1>
      <div class="camera_selector">
        <div class="selected_camera" id="forward">
          <p>Forward Camera</p>
        </div>
        <div class="${isWideAvailable ? "" : "unavailable"}" id="wide">
          <p>Wide Camera</p>
        </div>
        <div class="${isDriverAvailable ? "" : "unavailable"}" id="driver">
          <p>Driver Camera</p>
        </div>
      </div>
      <div class="video_wrapper">
        <video
          id="video"
          autoplay=""
          muted=""
          playsinline=""
          @click="${playPauseHandler}"
          @fullscreenchange="${(e) =>
            document.fullscreenElement
              ? (e.target.controls = true)
              : (e.target.controls = false)}"
          @ended="${videoEndedHandler}"
          @timeupdate="${timeupdateHandler}"
        >
          <source src="${route.segment_urls[0]}" type="video/mp4" />
          Your browser does not support the video tag.
        </video>
        <div class="videocontrols">
          <button id="playpause" @click="${playPauseHandler}">
            ${() =>
              state.playing
                ? html`<i class="bi bi-pause-fill"></i>`
                : html`<i class="bi bi-play-fill"></i>`}
          </button>
          <input
            id="seekslider"
            type="range"
            min="0"
            max="${route.total_duration}"
            value="${() => state.currentTime}"
            @mousedown="${() => (state.isSeeking = true)}"
            @mouseup="${() => (state.isSeeking = false)}"
            @input="${(e) => (state.currentTime = e.target.value)}"
            @change="${handleSeek}"
            step="1"
          />
          <p>
            <span id="current-time"
              >${() => formatSeconds(state.currentTime)}</span
            >/
            <span id="duration">${formattedDuration}</span>
          </p>
          <button id="fullscreen" @click="${fullscreenHandler}">
            <i class="bi bi-fullscreen"></i>
          </button>
        </div>
      </div>
    `
  }

  return html`
    <div class="route">
      ${Link("/routes", "Back", undefined, "button")}
      ${() => (state.route ? renderRoute(state.route) : "Loading...")}
    </div>
  `
}

/**
 * Gets the URL for the given segment
 * @param {Route} route
 * @param {"forward"|"wide"|"driver"} selectedCamera
 * @param {number} segmentIndex
 * @returns {string} - The URL for the given segment and camera
 */
function getUrlForIndex(route, selectedCamera, segmentIndex) {
  let url = route.segment_urls[segmentIndex]

  if (selectedCamera === "driver") {
    url += "?camera=driver"
  } else if (selectedCamera === "wide") {
    url += "?camera=wide"
  }
  return url
}

/**
 * Formats number of seconds to a human readable format,
 * with optional hours, minutes and seconds.
 * @param {number} seconds
 * @returns {string} - The formatted string, e.g. 1:23:45
 */
function formatSeconds(seconds) {
  let hours = Math.floor(seconds / 3600)
  let minutes = Math.floor((seconds - hours * 3600) / 60)
  let remainingSeconds = seconds - hours * 3600 - minutes * 60
  let formatted = ""
  if (hours > 0) {
    formatted += hours + ":"
  }
  if (minutes < 10) {
    formatted += "0"
  }
  formatted += minutes + ":"
  if (remainingSeconds < 10) {
    formatted += "0"
  }
  formatted += remainingSeconds
  return formatted
}

async function wait(ms) {
  return new Promise((resolve) => {
    setTimeout(resolve, ms)
  })
}
