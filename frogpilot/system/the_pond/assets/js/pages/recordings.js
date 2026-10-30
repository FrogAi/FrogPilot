(() => {
"use strict";

const P = window.Pond;

const RECORDER_NAME = /^\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}(_(replay_)?\d+)?(_|$)/;

function recordingItem(recording) {
  return {
    date: recording.recorded ? Date.parse(recording.recorded) / 1000 : recording.modified,
    id: recording.name,
    key: `${recording.name}:${recording.size}:${recording.modified}`,
    name: recording.name.replace(/\.mp4$/i, "").replace(RECORDER_NAME, "").replaceAll("_", " "),
    replay: recording.replay,
    size: recording.size
  };
}

function video(id) {
  return `/api/screen_recordings/${encodeURIComponent(id)}`;
}

async function deleteRecordings(items) {
  const one = items.length === 1;
  const bytes = items.reduce((sum, item) => sum + item.size, 0);
  const ok = await P.dialog({
    icon: "trash", tone: "danger", confirmTone: "danger",
    title: one ? `Delete “${P.esc(P.clips.itemTitle(items[0]))}”?` : `Delete ${items.length} recordings?`,
    body: `<p>This removes ${one ? "the recording" : "them"} from the device for good and frees <b>${P.size(bytes)}</b>. It can't be undone.</p>
      ${P.lossList([one ? "The video and its thumbnail" : `${P.plural(items.length, "video")} and their thumbnails`], ["Your drives and every other recording"])}`,
    confirm: `Delete ${one ? "recording" : P.plural(items.length, "recording")} (${P.size(bytes)})`
  });
  if (!ok) {
    return false;
  }
  let deleted = 0;
  for (const item of items) {
    try {
      await P.request("DELETE", video(item.id));
    } catch (error) {
      P.fail(error, `Couldn't delete “${P.esc(P.clips.itemTitle(item))}”`);
      return deleted > 0;
    }
    deleted += 1;
  }
  P.toast(one ? "Recording deleted" : `${items.length} recordings deleted`, `${P.size(bytes)} freed`);
  return true;
}

async function renameRecording(item, name) {
  if (!name) {
    P.toast("Couldn't rename the recording", "Enter a name", "alert");
    return;
  }
  let result;
  try {
    result = await P.request("PUT", `${video(item.id)}/name`, { name });
  } catch (error) {
    P.fail(error, "Couldn't rename the recording", { invalid_name: "Use letters, numbers, spaces, dots, dashes, and underscores" });
    return;
  }
  P.toast(`Renamed to “${P.fileLabel(name)}”`);
  P.go(RECORDINGS.href(result.name), { replace: true });
}

const RECORDINGS = {
  api: "/api/screen_recordings",
  busy: () => "",
  chip: seconds => P.clock(seconds),
  flags: () => "",
  href: id => `/recording?name=${encodeURIComponent(id)}`,
  item: recordingItem,
  label: "Screen Recordings",
  length: item => P.clips.lengths.get(item.key),
  maxName: 251,
  meta: item => (item.replay ? "Capture" : ""),
  none: `<h3>No screen recordings</h3><p>Tap Record on the car's screen to save a clip of it.</p>`,
  noun: "recording",
  only: [["replay", "Captures", item => item.replay], ["named", "Named", item => Boolean(item.name)]],
  path: "/recordings",
  preview: id => `${video(id)}/preview`,
  remove: deleteRecordings,
  rename: renameRecording,
  search: item => `${item.name}${item.replay ? " Capture" : ""}`,
  searchHint: "Name or day",
  sorts: [["newest", "Newest first"], ["oldest", "Oldest first"], ["largest", "Largest size"], ["name", "Name"]],
  thumbnail: id => `${video(id)}/thumbnail`,
  video
};

async function recordingPage(params) {
  const items = (await P.get(RECORDINGS.api)).map(recordingItem);
  const item = items.find(recording => recording.id === params.get("name"));
  if (!item) {
    return P.clips.notFound(RECORDINGS);
  }
  const length = RECORDINGS.length(item);
  const title = P.clips.itemTitle(item);
  return {
    title: P.esc(title),
    docTitle: title,
    crumb: { href: "/recordings", label: "Screen Recordings" },
    actions: `<button class="icon-btn" data-act="clips-rename" aria-label="Rename recording" title="Rename">${P.ic("pencil")}</button>
      <button class="btn btn-primary btn-small" data-act="recordings-download" data-name="${P.esc(item.id)}" aria-label="Download">${P.ic("download")}<span class="lbl-long">Download</span></button>`,
    body: `
    <div class="drive-grid">
      <div class="player-col">${P.clips.playerHTML({ date: item.date, duration: length, noun: "recording", poster: RECORDINGS.thumbnail(item.id) })}</div>
      <div class="drive-side">
        <dl class="kv">
          <dt>Recorded</dt><dd>${P.fmt(item.date, { weekday: "short", month: "short", day: "numeric" })}, ${P.timeLabel(item.date)}</dd>
          ${item.replay ? "<dt>Type</dt><dd>Capture</dd>" : ""}
          <dt>Length</dt><dd data-length-value>${length === undefined ? "…" : P.clock(length)}</dd>
        </dl>
        <div class="danger-zone side-row"><span class="muted">Frees ${P.size(item.size)}</span><button class="btn btn-danger-quiet btn-small" data-act="clips-delete-item">${P.ic("trash")}Delete recording</button></div>
        ${P.clips.pagerHTML(RECORDINGS, items.map(recording => recording.id), item.id)}
      </div>
    </div>`,
    mount: root => P.clips.mountDetail(root, RECORDINGS, item, {
      date: item.date,
      isDrive: false,
      onLength: seconds => {
        P.clips.lengths.set(item.key, seconds);
        root.querySelector("[data-length-value]").textContent = P.clock(seconds);
      },
      parts: 1,
      partLength: 0,
      src: () => video(item.id)
    })
  };
}

P.act("recordings-download", b => P.download(`${video(b.dataset.name)}?download`, b.dataset.name));

P.page("recordings", { title: "Screen Recordings", render: params => P.clips.listPage(RECORDINGS, params) });
P.page("recording", { parent: "recordings", title: "Screen Recording", render: recordingPage });
})();
