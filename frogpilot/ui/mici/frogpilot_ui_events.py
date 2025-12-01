from functools import cache

import cereal.messaging as messaging


@cache
def ui_event_publisher():
  return messaging.PubMaster(["frogpilotUIEvent"])


@cache
def ui_request_publisher():
  return messaging.PubMaster(["frogpilotUIRequest"])


def send_ui_event(pm, name):
  msg = messaging.new_message("frogpilotUIEvent")
  msg.valid = True
  setattr(msg.frogpilotUIEvent, name, None)

  pm.send("frogpilotUIEvent", msg)


def send_ui_request(pm, name):
  msg = messaging.new_message("frogpilotUIRequest")
  msg.valid = True
  setattr(msg.frogpilotUIRequest, name, None)

  pm.send("frogpilotUIRequest", msg)
