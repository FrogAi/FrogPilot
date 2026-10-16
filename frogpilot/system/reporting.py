import json
import uuid

from openpilot.frogpilot.common import frogpilot_variables

def capture_report(report, frogpilot_toggles, frogpilot_api):
  if "payload" not in report:
    error_content = "No error log found."

    if frogpilot_variables.ERROR_LOG_PATH.exists():
      error_content = frogpilot_variables.ERROR_LOG_PATH.read_text()[-500:]

    payload = {
      "discord_user": report["DiscordUser"],
      "error_content": error_content,
      "frogpilot_toggles": frogpilot_toggles,
      "report": report["Issue"],
      "report_id": str(uuid.uuid4()),
      "report_schema_version": 1,
    }
    if "Route" in report:
      payload["route"] = report["Route"]

    report = {"payload": json.dumps(payload)}

  response = frogpilot_api.post("/v1/reports", data=report["payload"], headers={"Content-Type": "application/json"}, timeout=30)

  if frogpilot_api.succeeded(response):
    print("Successfully sent error report!")
    return {"status": "sent"}

  status = "no_response" if response is None else response.status_code
  print(f"Error sending report (status={status})")
  return {**report, "status": "failed"}
