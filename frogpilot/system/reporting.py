import hashlib
import json
import os
import requests
import time
import uuid

from pathlib import Path

from openpilot.frogpilot.common import frogpilot_paths
from openpilot.frogpilot.common.frogpilot_api import FrogPilotAPIError

RETRY_STATUSES = (404, 503)
RETRY_WAIT_SECONDS = 60
RETRY_WAITS = 10

class ProgressReader:
  def __init__(self, file, on_read):
    self.file = file
    self.on_read = on_read

  def __len__(self):
    return os.fstat(self.file.fileno()).st_size

  def read(self, size=-1):
    chunk = self.file.read(size)
    self.on_read(len(chunk))
    return chunk

def build_report(report, frogpilot_toggles):
  error_content = "No error log found."

  if frogpilot_paths.ERROR_LOG_PATH.exists():
    error_content = frogpilot_paths.ERROR_LOG_PATH.read_text()[-500:]

  payload = {
    "discord_user": report["DiscordUser"],
    "error_content": error_content,
    "frogpilot_toggles": frogpilot_toggles,
  }

  files = []
  report_id = str(uuid.uuid4())
  report_schema_version = 1

  if "Logs" in report:
    declared_files = []
    for file in report["Logs"]["files"]:
      try:
        with open(file["path"], "rb") as log:
          sha256 = hashlib.file_digest(log, "sha256").hexdigest()
      except FileNotFoundError:
        continue

      size_bytes = os.path.getsize(file["path"])

      declared_files.append({"kind": file["kind"], "segment": file["segment"], "sha256": sha256, "size_bytes": size_bytes})
      files.append({**file, "sha256": sha256, "size_bytes": size_bytes})

    payload["logs"] = {"files": declared_files, "type": report["Logs"]["type"]}
    report_schema_version = 2

  payload["report"] = report["Issue"]
  payload["report_id"] = report_id
  payload["report_schema_version"] = report_schema_version

  if "Route" in report:
    payload["route"] = report["Route"]

  return {"files": files, "payload": json.dumps(payload), "report_id": report_id}

def capture_report(report, frogpilot_toggles, frogpilot_api, publish):
  if "DiscordUser" in report:
    report = build_report(report, frogpilot_toggles)

  if "payload" in report:
    response = frogpilot_api.post("/v1/reports", data=report["payload"], headers={"Content-Type": "application/json"}, timeout=30)

    if not frogpilot_api.succeeded(response):
      if response is None:
        print("Error sending report (status=no_response)")
      else:
        print(f"Error sending report (status={response.status_code})")

      publish({"status": "failed"})
      return report

    print("Successfully sent error report!")

    total_bytes = sum(file["size_bytes"] for file in report["files"])
    report = {"done": 0, "files": report["files"], "report_id": report["report_id"], "sent_bytes": 0, "total": len(report["files"]), "total_bytes": total_bytes}

  return upload_files(report, frogpilot_api, publish)

def upload_file(file, report_id, frogpilot_api, on_read):
  descriptor = {"kind": file["kind"], "report_id": report_id, "segment": file["segment"], "sha256": file["sha256"], "size_bytes": file["size_bytes"]}

  response = frogpilot_api.post("/v1/report-files", json=descriptor, timeout=30)
  for _ in range(RETRY_WAITS):
    if response is None or response.status_code not in RETRY_STATUSES:
      break

    time.sleep(RETRY_WAIT_SECONDS)
    response = frogpilot_api.post("/v1/report-files", json=descriptor, timeout=30)

  if not frogpilot_api.succeeded(response):
    return False

  try:
    upload = response.json().get("upload")
  except requests.exceptions.JSONDecodeError:
    return False

  if upload is not None:
    try:
      with open(file["path"], "rb") as data:
        frogpilot_api.put_upload(upload, ProgressReader(data, on_read), Path(file["path"]).name, "report_file")
    except FileNotFoundError:
      return True
    except (FrogPilotAPIError, requests.exceptions.RequestException):
      return False

  return True

def upload_files(report, frogpilot_api, publish):
  progress = {"done": report["done"], "percent": 0, "status": "uploading", "total": report["total"]}
  sent_bytes = report["sent_bytes"]

  def on_read(size):
    nonlocal sent_bytes
    sent_bytes += size

    percent = 100 * sent_bytes // report["total_bytes"]
    if percent != progress["percent"]:
      progress["percent"] = percent
      publish(progress)

  for index, file in enumerate(report["files"]):
    done = report["done"] + index
    file_start = sent_bytes

    progress["done"] = done
    progress["percent"] = 100 * sent_bytes // report["total_bytes"]
    publish(progress)

    if not upload_file(file, report["report_id"], frogpilot_api, on_read):
      print(f"Error uploading report files ({done} of {report['total']} uploaded)")

      publish({"done": done, "status": "failed", "total": report["total"]})
      return {**report, "done": done, "files": report["files"][index:], "sent_bytes": file_start}

    sent_bytes = file_start + file["size_bytes"]

  publish({"status": "sent"})
  return None
