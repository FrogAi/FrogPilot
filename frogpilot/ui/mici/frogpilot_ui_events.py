from functools import cache

import cereal.messaging as messaging


@cache
def ui_event_publisher():
  return messaging.PubMaster(["frogpilotUIEvent"])


def send_ui_event(pm, name):
  msg = messaging.new_message("frogpilotUIEvent")
  msg.valid = True
  setattr(msg.frogpilotUIEvent, name, None)

  pm.send("frogpilotUIEvent", msg)
