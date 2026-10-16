(() => {
"use strict";

const P = window.Pond;

const RECENT_DRIVES = 6;
const REPORT_ERRORS = {
  invalid_issue: "Pick what went wrong from the list",
  invalid_route: "That drive is no longer on the device. Pick another one",
  no_failed_report: "There's no unsent report to retry",
  offline: "Connect the device to Wi-Fi or a hotspot first, then send your report",
  pending: "Your last report is still sending"
};

let alertToast = null;
let errors = REPORT_ERRORS;
let sending = false;

function driveLabel(route) {
  const length = route.distance === undefined ? P.size(route.size) : P.distance(route.distance);
  return `${P.clips.itemTitle(route)} · ${length}`;
}

function refresh() {
  if (location.pathname === "/report") {
    P.refresh();
  }
}

async function send(path, body, button) {
  sending = true;
  alertToast?.remove();
  const handle = P.job({ label: "Sending report" });
  let status;
  try {
    const { request_time } = await P.request("POST", path, body);
    status = await P.poll(async () => {
      const report = await P.get(`/api/issue_report?request_time=${request_time}`);
      if (report.status !== "pending") {
        return report.status;
      }
    });
  } catch (error) {
    sending = false;
    button.disabled = false;
    button.lastChild.textContent = button.dataset.label;
    alertToast = handle.fail(error, "Couldn't send the report", errors);
    return;
  }
  sending = false;
  if (status === "sent") {
    handle.done("Report sent");
  } else {
    handle.finish();
    alertToast = P.toast("The report wasn't confirmed", "Retry it from Report an Issue to avoid sending a duplicate.", "alert");
  }
  refresh();
}

function watch() {
  sending = true;
  P.poll(async () => {
    const report = await P.get("/api/issue_report");
    if (report.status !== "pending") {
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
      return { body: `<section class="panel report-panel"><div class="empty"><h3>Reporting isn't available</h3><p>Reports reach the FrogPilot team only from FrogPilot installed from its own GitHub.</p></div></section>` };
    }
    const routes = (await P.get("/api/routes")).sort((a, b) => b.date - a.date);
    const picked = routes.find(r => r.route === params.get("route"));
    const drives = routes.slice(0, RECENT_DRIVES);
    if (picked && !drives.includes(picked)) {
      drives.push(picked);
    }
    const pending = sending || report.status === "pending";
    const issue = report.issues[report.issues.length - 1];
    errors = {
      ...REPORT_ERRORS,
      invalid_description: `Describe what happened in ${report.description_min_length} to ${report.description_max_length} characters`,
      invalid_discord_user: `Enter your Discord username (up to ${report.discord_user_max_length} characters)`
    };
    return {
      body: `
      <section class="panel report-panel"><div class="panel-body"><form class="form-grid" data-report novalidate>
        ${report.status === "failed" ? `<div class="notice danger">${P.ic("alert")}<span><b>Your last report wasn't confirmed.</b> Retry it rather than sending it again, to avoid a duplicate.</span></div>
          <div><button class="btn btn-quiet btn-small" type="button" data-act="report-retry" data-label="Retry last report">${P.ic("refresh")}Retry last report</button></div>` : ""}
        <label class="field"><span>What went wrong?</span>${P.dropdown({ value: issue, options: report.issues.map(i => [i, i]), label: "What went wrong?", attrs: "data-report-issue" })}</label>
        <label class="field" data-report-description><span>What happened?</span><textarea class="input" name="description" maxlength="${report.description_max_length}" aria-describedby="report-description-error" placeholder="For example: the car braked hard behind a slowing truck on the highway"></textarea><small class="hint report-error" id="report-description-error" aria-hidden="true" hidden>${errors.invalid_description}</small></label>
        <label class="field"><span>Which drive?</span>${P.dropdown({ value: (picked || drives[0])?.route || "", options: [["", "Not during a drive"], ...drives.map(r => [r.route, driveLabel(r)])], label: "Which drive?", attrs: "data-report-route" })}</label>
        <label class="field"><span>Your Discord name</span><input class="input" name="discord_user" maxlength="${report.discord_user_max_length}" autocomplete="off" required aria-describedby="report-discord-error" value="${P.esc(report.discord_user)}" placeholder="So the FrogPilot team can reach you"><small class="hint report-error" id="report-discord-error" aria-hidden="true" hidden>${errors.invalid_discord_user}</small></label>
        <p class="muted small-print">Your settings and the most recent error log go along with it so the problem can be traced.</p>
        <div><button class="btn btn-primary btn-small" type="submit" data-label="Send report" ${pending ? "disabled" : ""}>${P.ic("send")}${pending ? "Sending…" : "Send report"}</button></div>
      </form></div></section>`,
      mount: root => {
        const form = root.querySelector("[data-report]");
        const picker = form.querySelector("[data-report-issue]");
        const description = form.querySelector("[data-report-description]");
        const drive = form.querySelector("[data-report-route]");
        const showDescription = () => {
          description.hidden = !report.described_issues.includes(picker.dataset.value);
        };
        showDescription();
        picker.addEventListener("change", showDescription);
        if (report.status === "pending" && !sending) {
          watch();
        }
        const invalid = (field, error) => {
          field.setAttribute("aria-invalid", String(Boolean(error)));
          root.querySelector(`#${field.getAttribute("aria-describedby")}`).hidden = !error;
        };
        form.addEventListener("submit", e => {
          e.preventDefault();
          const body = { discord_user: form.discord_user.value.trim(), issue: picker.dataset.value };
          if (drive.dataset.value) {
            body.route = drive.dataset.value;
          }
          const text = form.description.value.replace(/\s+/g, " ").trim();
          const shortDescription = !description.hidden && text.length < report.description_min_length;
          invalid(form.description, shortDescription);
          invalid(form.discord_user, !body.discord_user);
          if (shortDescription) {
            form.description.focus();
            return;
          }
          if (!body.discord_user) {
            form.discord_user.focus();
            return;
          }
          if (!description.hidden) {
            body.description = text;
          }
          const button = form.querySelector("[type='submit']");
          button.disabled = true;
          button.lastChild.textContent = "Sending…";
          send("/api/issue_report", body, button);
        });
      }
    };
  }
});

P.act("report-retry", b => {
  b.disabled = true;
  send("/api/issue_report/retry", undefined, b);
});
})();
