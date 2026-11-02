import time

from cereal import car
from msgq.visionipc import VisionStreamType

from openpilot.selfdrive.ui.mici.onroad.driver_camera_dialog import DriverCameraDialog
from openpilot.selfdrive.ui.ui_state import ui_state

DRIVER_VIEW = 1
STANDARD_VIEW = 2
WIDE_VIEW = 3

REVERSE_DELAY = 0.5


class FrogPilotCameraView:
  def __init__(self):
    self.driver_view_active = False
    self.reversing = False

    self._driver_camera = None
    self._reverse_time = None

  def road_stream(self, target, available_streams):
    camera_view = ui_state.frogpilot_toggles.get("camera_view")
    if camera_view == STANDARD_VIEW:
      return VisionStreamType.VISION_STREAM_ROAD
    if camera_view == WIDE_VIEW and VisionStreamType.VISION_STREAM_WIDE_ROAD in available_streams:
      return VisionStreamType.VISION_STREAM_WIDE_ROAD
    return target

  def update(self):
    reverse = ui_state.sm["carState"].gearShifter == car.CarState.GearShifter.reverse and ui_state.frogpilot_toggles.get("driver_camera_in_reverse")
    if not reverse:
      self._reverse_time = None
    elif self._reverse_time is None:
      self._reverse_time = time.monotonic()

    self.reversing = ui_state.started and self._reverse_time is not None and time.monotonic() - self._reverse_time >= REVERSE_DELAY
    self.driver_view_active = self.reversing or (ui_state.started and ui_state.frogpilot_toggles.get("camera_view") == DRIVER_VIEW)

  def render_driver_view(self, rect):
    if self._driver_camera is None:
      self._driver_camera = DriverCameraDialog(no_escape=True)

    self._driver_camera._camera_view.render(rect)
    self._driver_camera._draw_face_detection(rect)


frogpilot_camera_view = FrogPilotCameraView()
