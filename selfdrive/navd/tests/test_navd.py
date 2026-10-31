import http.server
import json
import logging
import random
import socket
import threading
import numpy as np

from parameterized import parameterized

import cereal.messaging as messaging
from openpilot.common.logging_extra import SwagFormatter
from openpilot.common.params import Params
from openpilot.common.swaglog import cloudlog
from openpilot.selfdrive.navd.helpers import Coordinate
from openpilot.selfdrive.navd.navd import RouteEngine
from openpilot.system.manager.process_config import managed_processes


class TestNavd:
  def setup_method(self):
    self.params = Params()
    self.sm = messaging.SubMaster(['navRoute', 'navInstruction'])

  def teardown_method(self):
    managed_processes['navd'].stop()

  def _check_route(self, start, end, check_coords=True):
    self.params.put("NavDestination", json.dumps(end))
    self.params.put("LastGPSPosition", json.dumps(start))

    managed_processes['navd'].start()
    for _ in range(30):
      self.sm.update(1000)
      if all(f > 0 for f in self.sm.recv_frame.values()):
        break
    else:
      raise Exception("didn't get a route")

    assert managed_processes['navd'].proc.is_alive()
    managed_processes['navd'].stop()

    # ensure start and end match up
    if check_coords:
      coords = self.sm['navRoute'].coordinates
      assert np.allclose([start['latitude'], start['longitude'], end['latitude'], end['longitude']],
                         [coords[0].latitude, coords[0].longitude, coords[-1].latitude, coords[-1].longitude],
                         rtol=1e-3)

  def test_simple(self):
    start = {
      "latitude": 32.7427228,
      "longitude": -117.2321177,
    }
    end = {
      "latitude": 32.7557004,
      "longitude": -117.268002,
    }
    self._check_route(start, end)

  @parameterized.expand([(i,) for i in range(10)])
  def test_random(self, index):
    start = {"latitude": random.uniform(-90, 90), "longitude": random.uniform(-180, 180)}
    end = {"latitude": random.uniform(-90, 90), "longitude": random.uniform(-180, 180)}
    self._check_route(start, end, check_coords=False)

  # FrogPilot variables
  @parameterized.expand([("http_error",), ("connection_error",)])
  def test_failed_route_request_does_not_log_token(self, failure):
    class UnauthorizedHandler(http.server.BaseHTTPRequestHandler):
      def do_GET(self):
        self.send_response(401)
        self.end_headers()

      def log_message(self, *args):
        pass

    if failure == "http_error":
      server = http.server.HTTPServer(("127.0.0.1", 0), UnauthorizedHandler)
      threading.Thread(target=server.serve_forever, daemon=True).start()
      port = server.server_port
    else:
      with socket.socket() as unused_socket:
        unused_socket.bind(("127.0.0.1", 0))
        port = unused_socket.getsockname()[1]

    records = []
    handler = logging.Handler()
    handler.setFormatter(SwagFormatter(cloudlog))
    handler.emit = lambda record: records.append(handler.format(record))
    cloudlog.addHandler(handler)

    token = "sk.test_secret_token"
    self.params.put("MapboxSecretKey", token)
    route_engine = RouteEngine(None, messaging.PubMaster(['frogpilotNavigation', 'navRoute']))
    route_engine.mapbox_host = f"http://127.0.0.1:{port}"
    route_engine.last_position = Coordinate(32.7427228, -117.2321177)
    try:
      route_engine.calculate_route(Coordinate(32.7557004, -117.268002))
    finally:
      cloudlog.removeHandler(handler)
      if failure == "http_error":
        server.shutdown()
        server.server_close()

    assert any("failed to get route" in record for record in records)
    assert not any(token in record for record in records)
