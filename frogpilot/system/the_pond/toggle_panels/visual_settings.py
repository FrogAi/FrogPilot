from openpilot.frogpilot.common import frogpilot_variables
from openpilot.frogpilot.system.the_pond.toggle_panels import rows

DEVELOPER_SIDEBAR_METRIC_OPTIONS = tuple({"label": label, "value": value} for value, label in frogpilot_variables.DEVELOPER_SIDEBAR_METRIC_LABELS.items())


def openpilot_longitudinal_visible(context, key):
  return context.level_reason(key) or context.car_reason(context.has_openpilot_longitudinal)


def lead_marker_reason(context):
  params = context.params
  hide_lead_marker = params.get_bool("AdvancedCustomUI") and params.get_bool("HideLeadMarker") and not params.get_bool("DebugMode")
  return context.setting_reason(not (context.car_detected and context.has_openpilot_longitudinal and hide_lead_marker), "HideLeadMarker", wanted=False)


def adjacent_leads_visible(context):
  reason = context.level_reason("AdjacentLeadsUI") or context.car_reason(context.has_radar) or lead_marker_reason(context)
  return reason or context.setting_reason(context.has_openpilot_longitudinal or context.params.get_bool("LeadInfo"), "LeadInfo")


def clear_sidebar_metrics(context, keys):
  for key in keys:
    context.params.put_bool(key, False)
  return False


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
    "children": ("Theme and Appearance", "DRIVING VIEW", "Developer UI"),
    "description": ("<b>Put openpilot's internal numbers on screen, so you can see what it is thinking while it drives.</b><br><br>None of it " +
                    "changes how the car behaves. It is for working out why openpilot did something, and it is worth turning back off once you " +
                    "are done."),
    "key": "DeveloperUI",
    "order": 1,
    "path": ("Theme and Appearance", "DRIVING VIEW"),
    "title": "Developer UI",
    "widget": "manage"
  },
  {
    "children": ("Theme and Appearance", "DRIVING VIEW", "Developer UI", "Developer Metrics"),
    "description": "<b>Show readouts of what openpilot's sensors and hardware are doing while you drive.</b>",
    "key": "DeveloperMetrics",
    "order": 0,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Developer UI"),
    "title": "Developer Metrics",
    "widget": "manage"
  },
  {
    "description": ("<b>Print how wide the lanes either side of you are, right on their paths.</b><br><br>It draws the adjacent lane paths itself, " +
                    "so there is something to print onto, even with \"Adjacent Lanes\" switched off."),
    "key": "AdjacentPathMetrics",
    "order": 0,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Developer UI", "Developer Metrics"),
    "title": "Adjacent Lane Metrics",
    "widget": "toggle"
  },
  {
    "description": ("<b>Color the edge of the driving screen to show what openpilot is reacting to.</b><br><br>You can switch on the blind spot, " +
                    "steering effort, and turn signal borders separately. The blind spot one only appears on cars with factory blind spot " +
                    "monitoring, and the steering effort one only appears on cars where openpilot sends a steering effort rather than a steering " +
                    "angle."),
    "key": "BorderMetrics",
    "order": 1,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Developer UI", "Developer Metrics"),
    "sub_toggles": ({"key": "BlindSpotMetrics", "label": "Blind Spot", "visible": lambda context: context.car_reason(context.has_bsm)},
                    {"key": "ShowSteering", "label": "Steering Torque", "visible": lambda context: context.car_reason(not context.is_angle_car)},
                    {"key": "SignalMetrics", "label": "Turn Signal"}),
    "title": "Border Metrics",
    "widget": "button_toggle"
  },
  {
    "description": ("<b>Show how many frames a second the driving screen is drawing, along the bottom.</b><br><br>It also tracks the lowest and " +
                    "highest it has seen."),
    "key": "FPSCounter",
    "order": 2,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Developer UI", "Developer Metrics"),
    "title": "FPS Display",
    "widget": "toggle"
  },
  {
    "description": ("<b>Print how far away the car ahead is and how fast it is going, underneath its marker.</b><br><br>On cars where openpilot " +
                    "handles the gas and brake, it also shows the gap openpilot is aiming for, so you can see it closing in on the distance you " +
                    "asked for."),
    "key": "LeadInfo",
    "order": 3,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Developer UI", "Developer Metrics"),
    "title": "Lead Info",
    "visible": lambda context: context.level_reason("LeadInfo") or lead_marker_reason(context),
    "widget": "toggle"
  },
  {
    "description": ("<b>Show the device temperature as a number in the sidebar instead of a word like \"GOOD\".</b><br><br>Useful for spotting the " +
                    "device getting hot before it slows itself down."),
    "key": "NumericalTemp",
    "order": 4,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Developer UI", "Developer Metrics"),
    "sub_toggles": ({"key": "Fahrenheit", "label": "Fahrenheit"},),
    "title": "Numerical Temperature Gauge",
    "widget": "button_toggle"
  },
  {
    "description": ("<b>Fill the sidebar with how hard the device is working, how much storage is left, and its IP address.</b><br><br>They take " +
                    "the place of the vehicle, connection, and signal indicators normally shown there.<br><br>CPU and GPU cannot both be on, and " +
                    "RAM, SSD Left, and SSD Used are one at a time, so picking one clears the other."),
    "id": "SidebarMetrics",
    "key": None,
    "order": 5,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Developer UI", "Developer Metrics"),
    "sub_toggles": (
      {"key": "ShowCPU", "label": "CPU", "on_change": lambda context, old_value, new_value: clear_sidebar_metrics(context, ("ShowGPU",))},
      {"key": "ShowGPU", "label": "GPU", "on_change": lambda context, old_value, new_value: clear_sidebar_metrics(context, ("ShowCPU",))},
      {"key": "ShowIP", "label": "IP"},
      {"key": "ShowMemoryUsage", "label": "RAM",
       "on_change": lambda context, old_value, new_value: clear_sidebar_metrics(context, ("ShowStorageLeft", "ShowStorageUsed"))},
      {"key": "ShowStorageLeft", "label": "SSD Left",
       "on_change": lambda context, old_value, new_value: clear_sidebar_metrics(context, ("ShowMemoryUsage", "ShowStorageUsed"))},
      {"key": "ShowStorageUsed", "label": "SSD Used",
       "on_change": lambda context, old_value, new_value: clear_sidebar_metrics(context, ("ShowMemoryUsage", "ShowStorageLeft"))}
    ),
    "title": "Sidebar Metrics",
    "widget": "buttons"
  },
  {
    "description": ("<b>Show the developer readouts in plain scientific units instead of the friendlier ones.</b><br><br>This only affects the " +
                    "developer numbers, not the speed or distances you drive by."),
    "key": "UseSI",
    "order": 6,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Developer UI", "Developer Metrics"),
    "title": "Use International System of Units",
    "widget": "toggle"
  },
  {
    "children": ("Theme and Appearance", "DRIVING VIEW", "Developer UI", "Developer Sidebar"),
    "description": ("<b>Add a second sidebar down the right-hand side, holding up to seven readouts you pick yourself.</b><br><br>It makes the " +
                    "camera view narrower, so it is worth turning off when you are not using it. Setting all seven to \"None\" leaves the bar " +
                    "there but empty."),
    "key": "DeveloperSidebar",
    "order": 1,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Developer UI"),
    "title": "Developer Sidebar",
    "widget": "manage"
  },
  {
    "description": ("<b>Choose what the first slot in the developer sidebar shows.</b><br><br>The list covers steering, acceleration, engagement, " +
                    "and auto-tune readouts. Every slot starts already filled in, and picking \"None\" closes the gap rather than leaving a hole."),
    "key": "DeveloperSidebarMetric1",
    "options": DEVELOPER_SIDEBAR_METRIC_OPTIONS,
    "order": 0,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Developer UI", "Developer Sidebar"),
    "picker": {"title": "Select a metric for the first slot"},
    "title": "Metric #1",
    "widget": "select"
  },
  {
    "description": "<b>Choose what the second slot in the developer sidebar shows.</b>",
    "key": "DeveloperSidebarMetric2",
    "options": DEVELOPER_SIDEBAR_METRIC_OPTIONS,
    "order": 1,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Developer UI", "Developer Sidebar"),
    "picker": {"title": "Select a metric for the second slot"},
    "title": "Metric #2",
    "widget": "select"
  },
  {
    "description": "<b>Choose what the third slot in the developer sidebar shows.</b>",
    "key": "DeveloperSidebarMetric3",
    "options": DEVELOPER_SIDEBAR_METRIC_OPTIONS,
    "order": 2,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Developer UI", "Developer Sidebar"),
    "picker": {"title": "Select a metric for the third slot"},
    "title": "Metric #3",
    "widget": "select"
  },
  {
    "description": "<b>Choose what the fourth slot in the developer sidebar shows.</b>",
    "key": "DeveloperSidebarMetric4",
    "options": DEVELOPER_SIDEBAR_METRIC_OPTIONS,
    "order": 3,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Developer UI", "Developer Sidebar"),
    "picker": {"title": "Select a metric for the fourth slot"},
    "title": "Metric #4",
    "widget": "select"
  },
  {
    "description": "<b>Choose what the fifth slot in the developer sidebar shows.</b>",
    "key": "DeveloperSidebarMetric5",
    "options": DEVELOPER_SIDEBAR_METRIC_OPTIONS,
    "order": 4,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Developer UI", "Developer Sidebar"),
    "picker": {"title": "Select a metric for the fifth slot"},
    "title": "Metric #5",
    "widget": "select"
  },
  {
    "description": "<b>Choose what the sixth slot in the developer sidebar shows.</b>",
    "key": "DeveloperSidebarMetric6",
    "options": DEVELOPER_SIDEBAR_METRIC_OPTIONS,
    "order": 5,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Developer UI", "Developer Sidebar"),
    "picker": {"title": "Select a metric for the sixth slot"},
    "title": "Metric #6",
    "widget": "select"
  },
  {
    "description": "<b>Choose what the seventh slot in the developer sidebar shows.</b>",
    "key": "DeveloperSidebarMetric7",
    "options": DEVELOPER_SIDEBAR_METRIC_OPTIONS,
    "order": 6,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Developer UI", "Developer Sidebar"),
    "picker": {"title": "Select a metric for the seventh slot"},
    "title": "Metric #7",
    "widget": "select"
  },
  {
    "children": ("Theme and Appearance", "DRIVING VIEW", "Developer UI", "Developer Widgets"),
    "description": ("<b>Draw what openpilot's model and radar are seeing straight onto the driving screen.</b><br><br>Your car needs radar for two " +
                    "of these, and openpilot handling the gas and brake for the third, so this menu can come up almost empty."),
    "key": "DeveloperWidgets",
    "order": 2,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Developer UI"),
    "title": "Developer Widgets",
    "widget": "manage"
  },
  {
    "description": ("<b>Mark the cars your radar sees in the lanes either side of you.</b><br><br>Your car needs radar for these to appear, and " +
                    "they hang off the same marker \"Hide Lead Marker\" removes."),
    "key": "AdjacentLeadsUI",
    "order": 0,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Developer UI", "Developer Widgets"),
    "title": "Adjacent Leads Tracking",
    "visible": adjacent_leads_visible,
    "widget": "toggle"
  },
  {
    "description": ("<b>Drop a stop sign on the road where openpilot is planning to come to a stop.</b><br><br>It only marks stops openpilot works " +
                    "out for itself, so it stays away when openpilot is slowing for a car ahead, and \"Traffic Mode\" switches it off entirely. " +
                    "\"Show Distance\" also prints how far away that stop is."),
    "key": "ShowStoppingPoint",
    "order": 1,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Developer UI", "Developer Widgets"),
    "sub_toggles": ({"key": "ShowStoppingPointMetrics", "label": "Show Distance"},),
    "title": "Model Stopping Point",
    "visible": lambda context: openpilot_longitudinal_visible(context, "ShowStoppingPoint"),
    "widget": "button_toggle"
  },
  {
    "description": ("<b>Show every object your car's radar is returning, not just the car you are following.</b><br><br>Expect a lot of dots, " +
                    "including signs, barriers, and parked cars."),
    "key": "RadarTracksUI",
    "order": 2,
    "path": ("Theme and Appearance", "DRIVING VIEW", "Developer UI", "Developer Widgets"),
    "title": "Radar Tracks",
    "visible": lambda context: context.level_reason("RadarTracksUI") or context.car_reason(context.has_radar),
    "widget": "toggle"
  },
  {
    "children": ("Theme and Appearance", "DRIVING VIEW", "Driving Screen Widgets"),
    "description": ("<b>Add extra things to the driving screen that stock openpilot does not show.</b><br><br>One of these does change how the car " +
                    "drives. The \"Driving Personality Button\" switches your following distance."),
    "key": "CustomUI",
    "order": 2,
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
                    "under \"Wheel Buttons\", gives you those actions instead. This changes how the car actually drives, not just what you see. It " +
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
    "order": 3,
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
    "order": 4,
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
    "order": 5,
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
