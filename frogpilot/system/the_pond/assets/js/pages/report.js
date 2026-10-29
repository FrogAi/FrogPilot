(() => {
"use strict";

const P = window.Pond;

const RECENT_DRIVES = 6;
const REPORT_ERRORS = {
  invalid_issue: "Pick what went wrong from the list",
  invalid_moment: "Pick the minute the issue happened",
  invalid_route: "That drive is no longer on the device. Pick another one",
  invalid_type: "Pick Quick report or Full details",
  no_failed_report: "There's no unsent report to retry",
  no_logs: "That drive has no finished logs yet. Pick another drive or try again in a minute",
  offline: "Connect the device to Wi-Fi or a hotspot first, then send your report",
  pending: "Your last report is still sending"
};
const REPORT_TYPES = [
  ["quick", "Quick report", "Best for settings, speed limits, alerts, engagement, and crashes"],
  ["full", "Full details", "Best for steering, braking, car support, misread signs, and anything split-second"]
];

let alertToast = null;
let errors = REPORT_ERRORS;
let sending = false;

function driveLabel(route) {
  let length = P.distance(route.distance);
  if (route.distance === undefined) {
    length = P.size(route.size);
  }

  return `${P.clips.itemTitle(route)} · ${length}`;
}

function failedNotice(report) {
  if (report.status !== "failed") {
    return "";
  }

  let label = "Retry last report";
  let text = "<b>Your last report wasn't confirmed.</b> Retry it rather than sending it again, to avoid a duplicate.";
  if (report.total !== undefined) {
    label = "Send the rest";
    text = `<b>Your last report was sent, but ${P.plural(report.total - report.done, "log file")} didn't upload.</b> Retry to send the rest.`;
  }

  const notice = `<div class="notice danger">${P.ic("alert")}<span>${text}</span></div>`;
  const button = `<button class="btn btn-quiet btn-small" type="button" data-act="report-retry" data-label="${label}">${P.ic("refresh")}${label}</button>`;
  return `${notice}<div>${button}</div>`;
}

function isBusy(status) {
  return status === "pending" || status === "uploading";
}

function momentField() {
  const hint = "Play or drag through the drive to when the issue happened. Full details sends that minute's full logs and road video.";
  const video = `<video class="report-video" playsinline muted preload="metadata" data-report-player></video>`;
  const play = `<button class="icon-btn" type="button" data-report-play aria-label="Play">${P.ic("play")}</button>`;
  const back = `<button class="icon-btn" type="button" data-report-step="-60" aria-label="Back a minute">${P.ic("rew")}</button>`;
  const forward = `<button class="icon-btn" type="button" data-report-step="60" aria-label="Forward a minute">${P.ic("ffw")}</button>`;
  const sliderLabels = `aria-labelledby="report-moment-label" aria-describedby="report-moment-error"`;
  const slider = `<input type="range" min="0" step="1" value="0" data-report-slider ${sliderLabels}>`;
  const error = `<small class="hint report-error" id="report-moment-error" aria-hidden="true" hidden>${errors.invalid_moment}</small>`;

  return `
        <div class="field" data-report-moment hidden>
          <span id="report-moment-label">When did it happen?</span>
          <small class="hint">${hint}</small>
          ${video}
          <div class="report-moment">${play}${back}${slider}${forward}</div>
          <output class="report-moment-value" data-report-moment-value></output>
          ${error}
        </div>`;
}

function momentLabel(route, seconds) {
  const minute = Math.floor(seconds / route.segment_length) + 1;
  return `${P.timeLabel(route.date + seconds)} · minute ${minute} of ${route.segments}`;
}

function refresh() {
  if (location.pathname === "/report") {
    P.refresh();
  }
}

function resetButton(button) {
  button.disabled = false;
  button.lastChild.textContent = button.dataset.label;
}

function showProgress(handle, report) {
  if (report.status === "uploading") {
    handle.update(`${report.percent}%`, report.percent / 100);
  }
}

function typeField() {
  const buttons = [];
  for (const [id, label, hint] of REPORT_TYPES) {
    const text = `<b>${label}</b><span>${hint}</span>`;
    buttons.push(`<button class="report-type" type="button" data-report-type="${id}" aria-pressed="${id === "quick"}">${text}</button>`);
  }

  return `
        <div class="field" data-report-logs>
          <span id="report-type-label">What to send</span>
          <div class="report-types" role="group" aria-labelledby="report-type-label">${buttons.join("")}</div>
        </div>`;
}

async function send(path, body, button) {
  sending = true;

  if (alertToast) {
    alertToast.remove();
  }

  const handle = P.job({ label: "Sending report" });

  let report;
  try {
    const { request_time } = await P.request("POST", path, body);
    report = await P.poll(async () => {
      const current = await P.get(`/api/issue_report?request_time=${request_time}`);
      showProgress(handle, current);

      if (!isBusy(current.status)) {
        return current;
      }
    });
  } catch (error) {
    sending = false;
    resetButton(button);
    alertToast = handle.fail(error, "Couldn't send the report", errors);
    return;
  }

  sending = false;

  if (report.status === "sent") {
    handle.done("Report sent");
  } else if (report.total === undefined) {
    handle.finish();
    alertToast = P.toast("The report wasn't confirmed", "Retry it from Report an Issue to avoid sending a duplicate.", "alert");
  } else {
    handle.finish();
    alertToast = P.toast("Some logs didn't upload", "Your report was sent. Retry from Report an Issue to send the rest.", "alert");
  }

  refresh();
}

async function confirmSize(body) {
  const files = await P.request("POST", "/api/issue_report/files", body);
  if (!files.metered) {
    return true;
  }

  let video = "";
  if (body.type === "full") {
    video = " and road video";
  }

  const dialog = await P.dialog({
    icon: "alert",
    title: "Send logs over a metered connection?",
    body: `<p>This report sends ${P.size(files.size_bytes)} of logs${video}, which counts against your data plan.</p>`,
    confirm: "Send report"
  });
  return dialog !== null;
}

function watch() {
  sending = true;

  P.poll(async () => {
    const report = await P.get("/api/issue_report");
    if (!isBusy(report.status)) {
      return report.status;
    }
  }).then(() => {
    sending = false;
    refresh();
  }, error => {
    sending = false;
    P.fail(error, "Couldn't check on the report");
  });
}

P.page("report", {
  title: "Report an Issue",
  async render(params) {
    let report;
    try {
      report = await P.get("/api/issue_report");
    } catch (error) {
      if (!(error instanceof P.PondError) || error.status !== 404) {
        throw error;
      }

      const unavailable = "<h3>Reporting isn't available</h3><p>Reports reach the FrogPilot team only from FrogPilot installed from its own GitHub.</p>";
      return { body: `<section class="panel report-panel"><div class="empty">${unavailable}</div></section>` };
    }

    const routes = await P.get("/api/routes");
    routes.sort((a, b) => {
      return b.date - a.date;
    });

    let picked = null;
    for (const route of routes) {
      if (route.route === params.get("route")) {
        picked = route;
      }
    }

    const drives = routes.slice(0, RECENT_DRIVES);
    if (picked !== null && !drives.includes(picked)) {
      drives.push(picked);
    }

    let defaultDrive = "";
    if (picked !== null) {
      defaultDrive = picked.route;
    } else if (drives.length) {
      defaultDrive = drives[0].route;
    }

    const driveOptions = [["", "Not during a drive"]];
    for (const route of drives) {
      driveOptions.push([route.route, driveLabel(route)]);
    }

    const issueOptions = [];
    for (const issue of report.issues) {
      issueOptions.push([issue, issue]);
    }

    errors = {
      ...REPORT_ERRORS,
      invalid_description: `Describe what happened in ${report.description_min_length} to ${report.description_max_length} characters`,
      invalid_discord_user: `Enter your Discord username (up to ${report.discord_user_max_length} characters)`
    };

    const pending = sending || isBusy(report.status);

    let sendLabel = "Send report";
    let sendState = "";
    if (pending) {
      sendLabel = "Sending…";
      sendState = "disabled";
    }

    const issue = report.issues[report.issues.length - 1];
    const issuePicker = P.dropdown({ value: issue, options: issueOptions, label: "What went wrong?", attrs: "data-report-issue" });
    const drivePicker = P.dropdown({ value: defaultDrive, options: driveOptions, label: "Which drive?", attrs: "data-report-route" });

    const descriptionError = `<small class="hint report-error" id="report-description-error" aria-hidden="true" hidden>${errors.invalid_description}</small>`;
    const descriptionPlaceholder = "For example: the car braked hard behind a slowing truck on the highway";
    const descriptionInput = `<textarea class="input" name="description" maxlength="${report.description_max_length}"
      aria-describedby="report-description-error" placeholder="${descriptionPlaceholder}"></textarea>`;

    const discordError = `<small class="hint report-error" id="report-discord-error" aria-hidden="true" hidden>${errors.invalid_discord_user}</small>`;
    const discordInput = `<input class="input" name="discord_user" maxlength="${report.discord_user_max_length}" autocomplete="off" required
      aria-describedby="report-discord-error" value="${P.esc(report.discord_user)}" placeholder="So the FrogPilot team can reach you">`;

    const logContents = "exact locations and times, your car's data and VIN, driver monitoring, your device's settings and IDs, "
      + "road snapshots about once a minute, and cabin audio if \"Record and Upload Microphone Audio\" is on";
    const privacy = `<span data-report-privacy> The drive's logs go with it too, as recorded, so they include ${logContents}. `
      + "FrogPilot keeps them until they're deleted.</span>";
    const video = `<span data-report-video> Full details also includes road video, which can show license plates and people.</span>`;
    const included = "Your settings and the most recent error log go along with it so the problem can be traced.";
    const smallPrint = `<p class="muted small-print">${included}${privacy}${video}</p>`;

    const sendButton = `<button class="btn btn-primary btn-small" type="submit" data-label="Send report" ${sendState}>${P.ic("send")}${sendLabel}</button>`;

    return {
      body: `
      <section class="panel report-panel"><div class="panel-body"><form class="form-grid" data-report novalidate>
        ${failedNotice(report)}
        <label class="field"><span>What went wrong?</span>${issuePicker}</label>
        <label class="field" data-report-description><span>What happened?</span>${descriptionInput}${descriptionError}</label>
        <label class="field"><span>Which drive?</span>${drivePicker}</label>
        ${typeField()}
        ${momentField()}
        <label class="field"><span>Your Discord name</span>${discordInput}${discordError}</label>
        ${smallPrint}
        <div>${sendButton}</div>
      </form></div></section>`,
      mount: root => {
        const form = root.querySelector("[data-report]");
        const picker = form.querySelector("[data-report-issue]");
        const description = form.querySelector("[data-report-description]");
        const drive = form.querySelector("[data-report-route]");
        const logs = form.querySelector("[data-report-logs]");
        const moment = form.querySelector("[data-report-moment]");
        const slider = form.querySelector("[data-report-slider]");
        const momentValue = form.querySelector("[data-report-moment-value]");
        const video = form.querySelector("[data-report-player]");
        const playButton = form.querySelector("[data-report-play]");

        let momentPicked = false;
        let playing = false;
        let reportType = "quick";
        let videoIndex = null;
        let videoOffset = 0;

        const pickedRoute = () => {
          for (const route of drives) {
            if (route.route === drive.dataset.value) {
              return route;
            }
          }
          return null;
        };

        const invalid = (field, error) => {
          field.setAttribute("aria-invalid", String(Boolean(error)));
          root.querySelector(`#${field.getAttribute("aria-describedby")}`).hidden = !error;
        };

        const showDescription = () => {
          description.hidden = !report.described_issues.includes(picker.dataset.value);
        };

        const showMoment = () => {
          const route = pickedRoute();

          momentValue.textContent = "Not picked yet";
          if (momentPicked) {
            momentValue.textContent = momentLabel(route, Number(slider.value));
          }

          slider.setAttribute("aria-valuetext", momentValue.textContent);
        };

        const playVideo = () => {
          video.play().catch(error => {
            if (error.name !== "AbortError") {
              throw error;
            }
          });
        };

        const setPlaying = on => {
          playing = on;

          let icon = "play";
          let label = "Play";
          if (on) {
            icon = "pause";
            label = "Pause";
          }

          playButton.innerHTML = P.ic(icon);
          playButton.setAttribute("aria-label", label);

          if (!on) {
            video.pause();
          } else if (video.readyState >= 1) {
            playVideo();
          }
        };

        const showFrame = () => {
          const route = pickedRoute();
          const seconds = Number(slider.value);
          const index = Math.floor(seconds / route.segment_length);

          videoOffset = seconds - index * route.segment_length;

          if (index !== videoIndex) {
            videoIndex = index;
            video.src = `/api/routes/${encodeURIComponent(route.route)}/${route.segment_numbers[index]}/low`;
          } else if (video.readyState >= 1) {
            video.currentTime = videoOffset;
          }
        };

        const showLogs = () => {
          const route = pickedRoute();

          logs.hidden = route === null;
          moment.hidden = route === null || reportType !== "full";

          form.querySelector("[data-report-privacy]").hidden = logs.hidden;
          form.querySelector("[data-report-video]").hidden = moment.hidden;

          setPlaying(false);

          if (route !== null) {
            slider.max = String(route.segments * route.segment_length - 1);
            showMoment();
          }

          video.hidden = moment.hidden || !route.video;
          playButton.hidden = video.hidden;

          if (!video.hidden) {
            showFrame();
          }
        };

        const setMoment = seconds => {
          slider.value = String(seconds);
          momentPicked = true;
          invalid(slider, false);
          showMoment();
        };

        const pickMoment = seconds => {
          setMoment(seconds);

          if (!video.hidden) {
            showFrame();
          }
        };

        showDescription();
        showLogs();

        picker.addEventListener("change", showDescription);

        drive.addEventListener("change", () => {
          momentPicked = false;
          slider.value = "0";
          videoIndex = null;
          invalid(slider, false);
          showLogs();
        });

        for (const button of form.querySelectorAll("[data-report-type]")) {
          button.addEventListener("click", () => {
            reportType = button.dataset.reportType;

            for (const other of form.querySelectorAll("[data-report-type]")) {
              other.setAttribute("aria-pressed", String(other === button));
            }

            showLogs();
          });
        }

        slider.addEventListener("input", () => {
          setPlaying(false);
          pickMoment(Number(slider.value));
        });

        playButton.addEventListener("click", () => {
          setPlaying(!playing);
        });

        video.addEventListener("loadedmetadata", () => {
          video.currentTime = videoOffset;

          if (playing) {
            playVideo();
          }
        });

        video.addEventListener("timeupdate", () => {
          if (!video.paused) {
            setMoment(videoIndex * pickedRoute().segment_length + Math.floor(video.currentTime));
          }
        });

        video.addEventListener("ended", () => {
          const route = pickedRoute();
          if (videoIndex + 1 >= route.segments) {
            setPlaying(false);
            return;
          }

          pickMoment((videoIndex + 1) * route.segment_length);
        });

        for (const button of form.querySelectorAll("[data-report-step]")) {
          button.addEventListener("click", () => {
            const seconds = Number(slider.value) + Number(button.dataset.reportStep);
            pickMoment(Math.min(Number(slider.max), Math.max(0, seconds)));
          });
        }

        if (isBusy(report.status) && !sending) {
          watch();
        }

        form.addEventListener("submit", async event => {
          event.preventDefault();

          const body = { discord_user: form.discord_user.value.trim(), issue: picker.dataset.value };
          if (drive.dataset.value) {
            body.route = drive.dataset.value;
            body.type = reportType;
          }

          const text = form.description.value.replace(/\s+/g, " ").trim();
          const shortDescription = !description.hidden && text.length < report.description_min_length;
          const missingMoment = !moment.hidden && !momentPicked;

          invalid(form.description, shortDescription);
          invalid(slider, missingMoment);
          invalid(form.discord_user, !body.discord_user);

          if (shortDescription) {
            form.description.focus();
            return;
          }
          if (missingMoment) {
            slider.focus();
            return;
          }
          if (!body.discord_user) {
            form.discord_user.focus();
            return;
          }

          if (!description.hidden) {
            body.description = text;
          }
          if (!moment.hidden) {
            body.moment = Number(slider.value);
          }

          const button = form.querySelector("[type='submit']");
          button.disabled = true;
          button.lastChild.textContent = "Sending…";

          if (body.route) {
            let confirmed;
            try {
              confirmed = await confirmSize(body);
            } catch (error) {
              resetButton(button);
              P.fail(error, "Couldn't check the drive's logs", errors);
              return;
            }

            if (!confirmed) {
              resetButton(button);
              return;
            }
          }

          send("/api/issue_report", body, button);
        });
      }
    };
  }
});

P.act("report-retry", button => {
  button.disabled = true;
  send("/api/issue_report/retry", undefined, button);
});
})();
