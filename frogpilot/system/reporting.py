import uuid

from openpilot.frogpilot.common.frogpilot_api import API_VERSION
from openpilot.frogpilot.common.frogpilot_variables import ERROR_LOGS_PATH

def capture_report(discord_user, report, frogpilot_toggles, frogpilot_api, params_memory):
  params_memory.remove("IssueReported")

  error_content = "No error log found."
  error_file_path = ERROR_LOGS_PATH / "error.txt"

  if error_file_path.exists():
    error_content = error_file_path.read_text()[-500:]

  payload = {
    "discord_user": discord_user,
    "error_content": error_content,
    "frogpilot_toggles": frogpilot_toggles,
    "report": report,
    "report_id": str(uuid.uuid4()),
    "report_schema_version": API_VERSION,
  }

  response = frogpilot_api.post("/v1/reports", json=payload, headers={"User-Agent": "frogpilot-api/1.0"}, timeout=30)

  if response is not None and 200 <= response.status_code < 300:
    print("Successfully sent error report!")
  else:
    status = "no_response" if response is None else response.status_code
    print(f"Error sending report (status={status})")
