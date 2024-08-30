from flask import Flask, render_template, Response, request, send_from_directory
from datetime import datetime

import json
import re
import secrets

from . import utils
from . import fleet_manager_helpers

RUNNING_ON_COMMA = utils.is_running_on_comma()

# Import comma code if running on comma device
if RUNNING_ON_COMMA:
  from openpilot.common.realtime import set_core_affinity
  from openpilot.common.swaglog import cloudlog

app = Flask(__name__)

def setup(app):
  app = app

  @app.errorhandler(404)
  def not_found(e):
    return render_template("index.html")

  @app.route("/")
  def index():
    return render_template("index.html")

  @app.route("/playground")
  def playground():
    return render_template("playground.html")

  @app.route('/assets/<path:path>')
  def send_assets(path):
    """
    Not the best strategy for serving static files, but it works for now.
    """
    return send_from_directory('assets', path)

  @app.route("/api/settings")
  def get_settings():
    return utils.load_settings()

  @app.route("/api/settings/<key>", methods=["POST"])
  def save_settings(key):
    body = request.json
    params = utils.load_settings()
    param = utils.find_setting(key, params)
    if param == None:
      return {"message": "Invalid key"}, 400

    newValue = body.get("value")
    if isinstance(param.get("value"), int) and not isinstance(newValue, int):
      return {"message": "Invalid value, a number is required"}, 400

    utils.save_setting(key, newValue)
    return {"message": "Settings saved"}


  @app.route("/api/stats")
  def get_stats():
    drives, drive_errors = utils.get_drive_stats()
    disk, disk_error = utils.get_disk_usage()
    result = {
      "driveStats": drives,
      "diskUsage": disk,
    }
    if drive_errors is not None:
      result["driveErrors"] = drive_errors
    if disk_error is not None:
      result["diskError"] = disk_error
    return result


  # Error logs start here
  @app.route("/api/error-logs")
  def get_error_logs():
    if request.accept_mimetypes['text/html']:
      return render_template("v2/error-logs.jinja", active="error_logs")
    elif request.accept_mimetypes['application/json']:
      errorLogs=fleet_manager_helpers.list_file(utils.ERROR_LOGS_PATH)
      return errorLogs, 200


  @app.route("/api/error-logs/<filename>")
  def get_error_log(filename):
    with open(f"{utils.ERROR_LOGS_PATH}{filename}") as f:
      content = f.read()
    return content, 200
  # Error logs end here

  @app.route("/api/navigation", methods=["GET"])
  def navigation():
    params = utils.load_settings()
    return {
      "mapboxToken": utils.find_setting("MapboxPublicKey", params).get("value", None),
      "lastPosition": utils.find_setting("LastGPSPosition", params).get("value", None),
      "destination": utils.find_setting("NavDestination", params).get("value", None),
      "previousDestinations": utils.find_setting("ApiCache_NavDestinations", params).get("value", None),
    }

  @app.route("/api/navigation", methods=["POST"])
  def set_nav_destination():
    body = request.json
    utils.save_setting("NavDestination", json.dumps(body))
    return {"message": "Destination set"}

  @app.route("/api/navigation", methods=["DELETE"])
  def clear_nav_destination():
    utils.save_setting("NavDestination", 0) # undefined/None is not valid json
    return {"message": "Destination cleared"}

  ## Routes/Dashcam endpoints start here
  @app.route("/api/routes")
  def v2_routes():
    route_names = fleet_manager_helpers.get_routes_names(utils.FOOTAGE_PATH)
    routes = []
    # Regex to match route names in the format "00000009--53e66ae16d"
    valid_format_regex = re.compile(r'^[0-9a-fA-F]{8}--[0-9a-fA-F]{10}$')

    for route_name in route_names:
      # Since the format is not a date, we skip date parsing and just use the route name
      first_segment_path = f"{utils.FOOTAGE_PATH}{route_name}--0"
      qcamera_path = f"{first_segment_path}/qcamera.ts"
      gif_path = f"{first_segment_path}/preview.gif"
      png_path = f"{first_segment_path}/preview.png"

      # Ensure thumbnails are created if they don't exist
      try:
        utils.video_to_png(qcamera_path, png_path)
        utils.video_to_gif(qcamera_path, gif_path)
      except Exception as e:
        print(f"Failed to generate thumbnails for {route_name}")
        print(e)

      routes.append({
        "name": route_name,
        "gif": f"/thumbnails/{route_name}--0/preview.gif",
        "png": f"/thumbnails/{route_name}--0/preview.png"
      })

    return routes, 200

  @app.route("/api/routes/<name>")
  def v2_route(name):
    route_date = datetime.strptime(name, '%Y-%m-%d--%H-%M-%S')
    segment_urls = []
    for segment in fleet_manager_helpers.get_segments_in_route(name, utils.FOOTAGE_PATH):
      segment_urls.append(f"/video/{segment}")
    lastSegmentDuration = utils.get_video_duration(f"{utils.FOOTAGE_PATH}{name}--{len(segment_urls)-1}/qcamera.ts")
    total_duration = round(lastSegmentDuration + ((len(segment_urls) - 1) * 60))
    available_cameras = utils.get_available_cameras(f"{utils.FOOTAGE_PATH}{name}--0")
    route_data = {
      "name": name,
      "segment_urls": segment_urls,
      "total_duration": total_duration,
      "date": route_date,
      "available_cameras": available_cameras
    }
    return route_data, 200

  @app.route("/thumbnails/<path:file_path>", methods=['GET'])
  def find_previewgif(file_path):
    directory = utils.FOOTAGE_PATH
    return send_from_directory(directory, file_path, as_attachment=True)

  @app.route("/video/<path>")
  def v2_video_file(path):
    video_file = "qcamera.ts" # default to qcamera (same as fcamera, just lower quality)
    if request.args.get("camera") == "driver":
      video_file = "dcamera.hevc"
    elif request.args.get("camera") == "wide":
      video_file = "ecamera.hevc"
    filepath = f"{utils.FOOTAGE_PATH}{path}/{video_file}"
    return Response(fleet_manager_helpers.ffmpeg_mp4_wrap_process_builder(filepath).stdout.read(), status=200, mimetype='video/mp4')

  return

  ## Settings endpoints start here
  @app.route("/6settings/<section>")
  def settings(section):
    params = utils.load_settings()
    # Special case for vehicle settings
    brands_and_models = None
    if section == "vehicles":
      brands_and_models = utils.get_all_car_models()
      selected_model = params["CarModel"]["value"]
      brand = selected_model.split(" ")[0]
      params["CarModel"]["options"] = brands_and_models[brand]
      params["CarModel"]["value"] = " ".join(selected_model.split(" ")[1:])

    # Convert the params dict to a list of dicts and filter out the ones that don't belong to the current section
    relevant_params = [param for param in params.values() if param.get("section", None) == section]
    return render_template("v2/settings.jinja", active=f"settings_{section}", settings=relevant_params, section=section, brands_and_models=brands_and_models)





  # Navigation ends here




setup(app)

def main():
  if RUNNING_ON_COMMA:
    try:
      set_core_affinity([0, 1, 2, 3])
    except Exception:
      cloudlog.exception("the_pond: failed to set core affinity")

  debug = False
  port = 8083
  if not RUNNING_ON_COMMA or __package__ == "thepond":
    debug = True
    port = 8084

  print(f"The Pond is{' not' if not RUNNING_ON_COMMA else ''} running on comma device, in debug mode {debug}")
  app.secret_key = secrets.token_hex(32)
  app.run(host="0.0.0.0", port=port, debug=debug)


if __name__ == '__main__':
  main()
