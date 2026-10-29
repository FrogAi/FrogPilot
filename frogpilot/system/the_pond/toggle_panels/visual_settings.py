from openpilot.frogpilot.system.the_pond.toggle_panels import rows


def openpilot_longitudinal_visible(context, key):
  return context.level_reason(key) or context.car_reason(context.has_openpilot_longitudinal)


def speed_limit_controller_active(context):
  return context.has_openpilot_longitudinal and context.params.get_bool("SpeedLimitController")


def blind_spot_path_visible(context):
  return context.level_reason("BlindSpotPath") or context.car_reason(context.has_bsm)


def show_speed_limits_visible(context):
  return context.level_reason("ShowSpeedLimits") or context.setting_reason(not speed_limit_controller_active(context), "SpeedLimitController", wanted=False)


def show_speed_limits_from_mapbox_visible(context):
  params = context.params
  return (context.level_reason("SLCMapboxFiller") or context.setting_reason(params.get_bool("ShowSpeedLimits"), "ShowSpeedLimits") or
          context.setting_reason(not speed_limit_controller_active(context), "SpeedLimitController", wanted=False) or
          context.set_reason("MapboxPublicKey"))


def use_vienna_visible(context):
  shown = context.params.get_bool("ShowSpeedLimits") or speed_limit_controller_active(context)
  return context.level_reason("UseVienna") or context.setting_reason(shown, "ShowSpeedLimits")


ROWS = [
  {
    "children": ("Theme and Appearance", "DRIVING VIEW", "Advanced UI Controls"),
    "description": ("<b>Fine-tune how the driving screen looks, beyond what the everyday settings cover.</b><br><br>None of these change how the car " +
                    "drives."),
    "key": "AdvancedCustomUI",
    "order": 0,
    "path": ("Theme and Appearance", "DRIVING VIEW"),
    "title": "Advanced UI Controls",
    "widget": "manage"
  },
  {
    "description": "<b>Take your current speed off the driving screen.</b><br><br>Your dashboard still shows it.",
    "key": "HideSpeed",
    "order": 0,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Advanced UI Controls"),
    "title": "Hide Current Speed",
    "widget": "toggle"
  },
  {
    "description": ("<b>Take the marker off the car in front of you on the driving screen.</b><br><br>openpilot still tracks that car and still " +
                    "follows it.<br><br>\"Lead Info\" and \"Adjacent Leads Tracking\" have nothing left to attach to, so they disappear too."),
    "key": "HideLeadMarker",
    "order": 1,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Advanced UI Controls"),
    "title": "Hide Lead Marker",
    "visible": lambda context: openpilot_longitudinal_visible(context, "HideLeadMarker"),
    "widget": "toggle"
  },
  {
    "description": "<b>Take the speed you have set off the driving screen.</b><br><br>openpilot still holds that speed.",
    "key": "HideMaxSpeed",
    "order": 2,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Advanced UI Controls"),
    "title": "Hide Max Speed",
    "widget": "toggle"
  },
  {
    "description": ("<b>Take the everyday informational messages off the driving screen.</b><br><br>Anything that actually needs you, like a warning " +
                    "or a takeover request, still comes through."),
    "key": "HideAlerts",
    "order": 3,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Advanced UI Controls"),
    "title": "Hide Non-Critical Alerts",
    "widget": "toggle"
  },
  {
    "description": ("<b>Take the posted speed limit sign off the driving screen.</b><br><br>If you use \"Speed Limit Controller\" it keeps working " +
                    "on the limit it reads, so your speed can still change for a sign you can no longer see. The sign does come back when it asks " +
                    "you to confirm a new limit."),
    "key": "HideSpeedLimit",
    "order": 4,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Advanced UI Controls"),
    "title": "Hide Speed Limits",
    "widget": "toggle"
  },
  {
    "description": ("<b>Show the speed your wheels are actually turning at instead of the slightly optimistic number your dashboard " +
                    "shows.</b><br><br>Most cars read a little high on purpose, so this usually reads one or two lower. It only changes the number " +
                    "on screen, never how openpilot drives."),
    "key": "WheelSpeed",
    "order": 5,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Advanced UI Controls"),
    "title": "Use Wheel Speed",
    "widget": "toggle"
  },
  {
    "children": ("Theme and Appearance", "DRIVING VIEW", "Driving Screen Widgets"),
    "description": ("<b>Add extra things to the driving screen that stock openpilot does not show.</b><br><br>One of these does change how the car " +
                    "drives: the \"Driving Personality Button\" switches your following distance."),
    "key": "CustomUI",
    "order": 1,
    "path": ("Theme and Appearance", "DRIVING VIEW"),
    "title": "Driving Screen Widgets",
    "widget": "manage"
  },
  {
    "description": ("<b>Color the driving path green when openpilot is speeding up and red when it is slowing down.</b><br><br>Handy for seeing a " +
                    "slowdown coming before you feel it."),
    "key": "AccelerationPath",
    "order": 0,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Driving Screen Widgets"),
    "title": "Acceleration Path",
    "visible": lambda context: openpilot_longitudinal_visible(context, "AccelerationPath"),
    "widget": "toggle"
  },
  {
    "description": ("<b>Draw the paths of the lanes either side of you, so you can see where openpilot thinks they run.</b><br><br>They only appear " +
                    "above your \"Minimum Lane Change Speed\" (20 mph or 32 km/h by default), and only where openpilot sees a lane between you and " +
                    "the road edge."),
    "key": "AdjacentPath",
    "order": 1,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Driving Screen Widgets"),
    "title": "Adjacent Lanes",
    "widget": "toggle"
  },
  {
    "description": ("<b>Turn the lane beside you red whenever your car's sensors see something in that blind spot.</b><br><br>It only shows up above " +
                    "your \"Minimum Lane Change Speed\" (20 mph or 32 km/h by default), so it stays away in slow traffic and parking lots. Keep " +
                    "checking your mirrors regardless."),
    "key": "BlindSpotPath",
    "order": 2,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Driving Screen Widgets"),
    "title": "Blind Spot Path",
    "visible": blind_spot_path_visible,
    "widget": "toggle"
  },
  {
    "description": ("<b>Add a compass to the driving screen showing which way you are heading.</b><br><br>Without a GPS fix it freezes on the last " +
                    "heading it knew rather than disappearing, so treat a compass that never moves as no reading at all."),
    "key": "Compass",
    "order": 3,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Driving Screen Widgets"),
    "title": "Compass",
    "widget": "toggle"
  },
  {
    "description": ("<b>Add a button to the driving screen that switches your following distance between Aggressive, Standard, and Relaxed without " +
                    "going into the menus.</b><br><br>It works as a second \"Distance\" button, so holding it, or changing what that button does " +
                    "under \"WHEEL BUTTONS\", gives you those actions instead. This changes how the car actually drives, not just what you see. It " +
                    "also shows which one is active."),
    "key": "OnroadDistanceButton",
    "order": 4,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Driving Screen Widgets"),
    "title": "Driving Personality Button",
    "visible": lambda context: openpilot_longitudinal_visible(context, "OnroadDistanceButton"),
    "widget": "toggle"
  },
  {
    "description": ("<b>Show gas and brake indicators on the driving screen that light up as the car speeds up or slows down, whether openpilot or " +
                    "you are driving.</b><br><br>\"Dynamic\" fades them in and out with how hard the car is accelerating or braking. \"Static\" " +
                    "shows them fully lit when active and dim when not."),
    "exclusive": True,
    "key": "PedalsOnUI",
    "order": 5,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Driving Screen Widgets"),
    "sub_toggles": ({"key": "DynamicPedalsOnUI", "label": "Dynamic"}, {"key": "StaticPedalsOnUI", "label": "Static"}),
    "title": "Gas / Brake Pedal Indicators",
    "visible": lambda context: openpilot_longitudinal_visible(context, "PedalsOnUI"),
    "widget": "button_toggle"
  },
  {
    "description": "<b>Turn the steering wheel picture on screen in time with your real steering wheel.</b>",
    "key": "RotatingWheel",
    "order": 6,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Driving Screen Widgets"),
    "title": "Rotating Steering Wheel",
    "widget": "toggle"
  },
  {
    "children": ("Theme and Appearance", "DRIVING VIEW", "Model UI"),
    "description": ("<b>Change how openpilot draws the road ahead, including the driving path, the lane lines, and the road edges.</b><br><br>Turning " +
                    "this off falls back to stock openpilot's sizes, though the colored path edges go away with it."),
    "key": "ModelUI",
    "order": 2,
    "path": ("Theme and Appearance", "DRIVING VIEW"),
    "title": "Model UI",
    "widget": "manage"
  },
  {
    "description": ("<b>Make the driving path narrower when openpilot is doing less of the driving, so you can tell at a glance how much control it " +
                    "has.</b><br><br>Full width while openpilot is driving, three quarters while \"Always On Lateral\" is steering for you, and half " +
                    "the rest of the time."),
    "key": "DynamicPathWidth",
    "order": 0,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Model UI"),
    "title": "Dynamic Path Width",
    "widget": "toggle"
  },
  {
    "description": ("<b>Set how thick the lane lines are drawn on the driving screen.</b><br><br>The default matches the 4 inch lines actually " +
                    "painted on US roads. This only changes the picture, never where openpilot steers."),
    "description_metric": ("<b>Set how thick the lane lines are drawn on the driving screen.</b><br><br>The default matches the 10 cm lines actually " +
                           "painted on roads. This only changes the picture, never where openpilot steers."),
    "key": "LaneLinesWidth",
    "order": 1,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Model UI"),
    "title": "Lane Lines Width",
    "value": rows.number(0, 24, unit=" inches", labels={0: "Off", 1: "1 inch"},
                         metric=rows.number(0, 60, unit=" cm", labels={0: "Off"})),
    "widget": "value"
  },
  {
    "description": ("<b>Set how thick the colored stripe down each side of the driving path is, which is what tells you which mode openpilot is " +
                    "in.</b><br><br>By default the two stripes together take up a fifth of the path width. Set it to zero to hide the stripe " +
                    "entirely.<br><br>Light blue means \"Always On Lateral\" is steering for you, green is normal driving, orange is Experimental " +
                    "Mode, red is Traffic Mode, and yellow means you have overridden Conditional Experimental Mode to keep Experimental Mode off."),
    "key": "PathEdgeWidth",
    "order": 2,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Model UI"),
    "title": "Path Edges Width",
    "value": rows.number(0, 100, unit="%", labels={0: "Off"}),
    "widget": "value"
  },
  {
    "description": ("<b>Set how wide the driving path is drawn ahead of your car.</b><br><br>The default of 6.1 feet is roughly the width of a real " +
                    "car."),
    "description_metric": ("<b>Set how wide the driving path is drawn ahead of your car.</b><br><br>The default of 1.9 meters is roughly the width " +
                           "of a real car."),
    "key": "PathWidth",
    "order": 3,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Model UI"),
    "title": "Path Width",
    "value": rows.number(0, 10, 0.1, unit=" feet", labels={0: "Off", 1: "1.0 foot"},
                         metric=rows.number(0, 3, 0.1, labels=rows.unit_labels(0, 3, 0.1, " meter", " meters", zero="Off"))),
    "widget": "value"
  },
  {
    "description": "<b>Set how thick the road edges are drawn on the driving screen.</b><br><br>The default is half a lane line.",
    "key": "RoadEdgesWidth",
    "order": 4,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Model UI"),
    "title": "Road Edges Width",
    "value": rows.number(0, 24, unit=" inches", labels={0: "Off", 1: "1 inch"},
                         metric=rows.number(0, 60, unit=" cm", labels={0: "Off"})),
    "widget": "value"
  },
  {
    "children": ("Theme and Appearance", "DRIVING VIEW", "Navigation Widgets"),
    "description": ("<b>Change what navigation shows on the driving screen, from the road name to speed limit signs.</b><br><br>Some of these need " +
                    "map data downloaded under \"Maps and Navigation\" before they show anything."),
    "key": "NavigationUI",
    "order": 3,
    "path": ("Theme and Appearance", "DRIVING VIEW"),
    "title": "Navigation Widgets",
    "widget": "manage"
  },
  {
    "description": ("<b>Show the name of the road you are on along the bottom of the driving screen.</b><br><br>It comes from downloaded map data, " +
                    "so it stays blank on roads you have not downloaded."),
    "key": "RoadNameUI",
    "order": 0,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Navigation Widgets"),
    "title": "Road Name",
    "widget": "toggle"
  },
  {
    "description": ("<b>Show the posted speed limit as a sign in the top-left corner of the driving screen.</b><br><br>The limit comes from your " +
                    "downloaded map data first, then from your car's dashboard where it can read one, and from signs read by the camera otherwise."),
    "key": "ShowSpeedLimits",
    "order": 1,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Navigation Widgets"),
    "title": "Show Speed Limits",
    "visible": show_speed_limits_visible,
    "widget": "toggle"
  },
  {
    "description": ("<b>Fall back to Mapbox for the speed limit when none of your downloaded maps, your dashboard, or signs read by the camera know " +
                    "one.</b><br><br>Needs your Public Mapbox Key set up under \"Maps and Navigation\" and a working internet connection. It sends " +
                    "your location to Mapbox to look up the limit."),
    "id": "ShowSpeedLimitsFromMapbox",
    "key": "SLCMapboxFiller",
    "level_key": "SLCMapboxFiller",
    "order": 2,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Navigation Widgets"),
    "title": "Show Speed Limits from Mapbox",
    "visible": show_speed_limits_from_mapbox_visible,
    "widget": "toggle"
  },
  {
    "description": ("<b>Draw speed limit signs in the round European style instead of the rectangular American one.</b><br><br>Needs \"Show Speed " +
                    "Limits\" or \"Speed Limit Controller\" switched on, or the sign disappears instead of changing shape. It never changes the " +
                    "limit openpilot reads."),
    "key": "UseVienna",
    "order": 3,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Navigation Widgets"),
    "title": "Use Vienna-Style Speed Signs",
    "visible": use_vienna_visible,
    "widget": "toggle"
  },
  {
    "children": ("Theme and Appearance", "DRIVING VIEW", "Quality of Life"),
    "description": "<b>Pick which camera you watch, and a couple of smaller driving screen touches that did not fit anywhere else.</b>",
    "key": "QOLVisuals",
    "order": 4,
    "path": ("Theme and Appearance", "DRIVING VIEW"),
    "title": "Quality of Life",
    "widget": "manage"
  },
  {
    "description": ("<b>Choose which camera the driving screen shows, from the wide one to the driver-facing one.</b><br><br>openpilot keeps using " +
                    "every camera to drive no matter which one you put on screen."),
    "key": "CameraView",
    "options": ({"label": "Auto", "value": 0}, {"label": "Driver", "value": 1}, {"label": "Standard", "value": 2}, {"label": "Wide", "value": 3}),
    "order": 0,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Quality of Life"),
    "title": "Camera View",
    "widget": "select"
  },
  {
    "description": ("<b>Switch the driving screen to the driver camera whenever you put the car in reverse.</b><br><br>This is not a backup camera. " +
                    "It faces you, not the road behind."),
    "key": "DriverCamera",
    "order": 1,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Quality of Life"),
    "title": "Show Driver Camera When In Reverse",
    "widget": "toggle"
  },
  {
    "description": ("<b>Replace your speed with a running timer once you come to a complete stop, so you can see how long you have been " +
                    "waiting.</b><br><br>It stays off for the first minute of a drive."),
    "key": "StoppedTimer",
    "order": 2,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Quality of Life"),
    "title": "Stopped Timer",
    "widget": "toggle"
  }
]
