import json
import uuid

from openpilot.frogpilot.common import frogpilot_variables

def capture_report(report, frogpilot_toggles, frogpilot_api):
  if "payload" not in report:
    error_content = "No error log found."
    error_file_path = frogpilot_variables.ERROR_LOGS_PATH / "error.txt"

    if error_file_path.exists():
      error_content = error_file_path.read_text()[-500:]

    payload = {
      "discord_user": report["DiscordUser"],
      "error_content": error_content,
      "frogpilot_toggles": frogpilot_toggles,
      "report": report["Issue"],
      "report_id": str(uuid.uuid4()),
      "report_schema_version": 1,
    }
    report = {"payload": json.dumps(payload)}

  response = frogpilot_api.post("/v1/reports", data=report["payload"], headers={"Content-Type": "application/json"}, timeout=30)

  if response is not None and 200 <= response.status_code < 300:
    print("Successfully sent error report!")
    return {"status": "sent"}

  status = "no_response" if response is None else response.status_code
  print(f"Error sending report (status={status})")
  return {**report, "status": "failed"}
