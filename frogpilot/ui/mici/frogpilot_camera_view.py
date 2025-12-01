from msgq.visionipc import VisionStreamType

from openpilot.selfdrive.ui.mici.onroad.driver_camera_dialog import DriverCameraDialog
from openpilot.selfdrive.ui.ui_state import ui_state

DRIVER_VIEW = 1
STANDARD_VIEW = 2
WIDE_VIEW = 3


class FrogPilotCameraView:
  def __init__(self):
    self.driver_view_active = False

    self._driver_camera = None

  def road_stream(self, target, available_streams):
    camera_view = ui_state.frogpilot_toggles.get("camera_view")
    if camera_view == STANDARD_VIEW:
      return VisionStreamType.VISION_STREAM_ROAD
    if camera_view == WIDE_VIEW and VisionStreamType.VISION_STREAM_WIDE_ROAD in available_streams:
      return VisionStreamType.VISION_STREAM_WIDE_ROAD
    return target

  def update(self):
    self.driver_view_active = ui_state.started and ui_state.frogpilot_toggles.get("camera_view") == DRIVER_VIEW

  def render_driver_view(self, rect):
    if self._driver_camera is None:
      self._driver_camera = DriverCameraDialog(no_escape=True)

    self._driver_camera._camera_view.render(rect)
    self._driver_camera._draw_face_detection(rect)


frogpilot_camera_view = FrogPilotCameraView()
