from openpilot.selfdrive.ui.ui_state import ui_state
from openpilot.system.ui.lib.multilang import tr


def frogpilot_model_name():
  toggles = ui_state.frogpilot_toggles

  if toggles.get("model_randomizer"):
    return tr("Mystery Model")

  return toggles.get("model_name", "")


def version_second_line(commit, release_branch):
  if release_branch:
    return frogpilot_model_name()

  return f"{commit}  {frogpilot_model_name()}".rstrip()
