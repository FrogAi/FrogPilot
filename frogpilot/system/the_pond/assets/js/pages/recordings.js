(() => {
"use strict";

const P = window.Pond;

const MAX_FILE_NAME_LENGTH = 255;
const RECORDER_NAME = /^\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}(_(replay_)?\d+)?(_|$)/;

function recorderName(id) {
  const match = id.replace(/\.mp4$/i, "").match(RECORDER_NAME);
  if (!match) {
    return "";
  }

  return match[0].replace(/_$/, "");
}

function maxNameLength(item) {
  const prefix = recorderName(item.id);
  if (!prefix) {
    return MAX_FILE_NAME_LENGTH - ".mp4".length;
  }

  return MAX_FILE_NAME_LENGTH - prefix.length - "_.mp4".length;
}

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

async function deleteRecordings(items, others) {
  const one = items.length === 1;
  const bytes = items.reduce((sum, item) => sum + item.size, 0);

  let otherData = "Your drives";
  if (others) {
    otherData = "Your drives and every other recording";
  }

  let lost = `${P.plural(items.length, "video")} and their thumbnails`;
  if (one) {
    lost = "The video and its thumbnail";
  }

  const ok = await P.dialog({
    icon: "trash", tone: "danger", confirmTone: "danger",
    title: one ? `Delete “${P.esc(P.clips.itemTitle(items[0]))}”?` : `Delete ${items.length} recordings?`,
    body: `<p>This removes ${one ? "the recording" : "them"} from the device for good and frees <b>${P.size(bytes)}</b>. It can't be undone.</p>
      ${P.lossList([lost], [otherData])}`,
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
  if (!name && !recorderName(item.id)) {
    throw new P.PondError(400, "empty_name");
  }

  const result = await P.request("PUT", `${video(item.id)}/name`, { name });

  let message = "Name cleared";
  if (name) {
    message = `Renamed to “${P.fileLabel(name)}”`;
  }

  P.toast(message);
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
  maxName: maxNameLength,
  meta: item => (item.replay ? "Capture" : ""),
  nameErrors: { empty_name: "Enter a name", invalid_name: "That name has characters that can't be used" },
  nameRule: "Use letters, numbers, spaces, dots, dashes, and underscores",
  newer: (a, b) => {
    return b.date - a.date;
  },
  none: `<h3>No screen recordings</h3><p>Tap Record on the car's screen to save a clip of it.</p>`,
  noun: "recording",
  only: [["replay", "Captures", item => item.replay], ["named", "Named", item => Boolean(item.name)]],
  path: "/recordings",
  preview: id => `${video(id)}/preview`,
  remove: deleteRecordings,
  rename: renameRecording,
  search: item => `${item.name}${item.replay ? " Capture" : ""}`,
  searchHint: "Name or day",
  sorts: [["newest", "Newest first"], ["oldest", "Oldest first"], ["largest", "Largest first"], ["name", "By name"]],
  thumbnail: id => `${video(id)}/thumbnail`,
  video
};

async function recordingPage(params) {
  const items = (await P.get(RECORDINGS.api)).map(recordingItem);
  const name = params.get("name") || "";
  let item = items.find(recording => recording.id === name);
  if (!item && recorderName(name)) {
    item = items.find(recording => recorderName(recording.id) === recorderName(name));
    if (item) {
      P.setParam("name", item.id);
    }
  }

  if (!item) {
    return P.clips.notFound(RECORDINGS);
  }
  const length = RECORDINGS.length(item);
  const title = P.clips.itemTitle(item);

  let type = "";
  if (item.replay) {
    type = "<dt>Type</dt><dd>Capture of recent footage</dd>";
  }

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
          ${type}
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
      others: items.length > 1,
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
