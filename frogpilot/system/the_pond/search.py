import re

from aiohttp import web
from functools import cache

from openpilot.frogpilot.common import frogpilot_paths, frogpilot_variables
from openpilot.frogpilot.system.the_pond import backups, toggles, utilities

STRING = r'"(?:[^"\\]|\\.)*"'
TRANSLATED = rf"tr\(((?:\s*{STRING})+)\s*\)"

BRANCH = re.compile(r'param == "(\w+)"')
CLASS_METHOD = re.compile(r"^(\w+)::~?\w+\(", re.M)
CONNECT = re.compile(r"\bconnect\(")
CONTROL_ROW = re.compile(rf'new \w+\(\s*"(\w+)",\s*{TRANSLATED}(?:,\s*{TRANSLATED})?')
DEFAULT_LIST = re.compile(r"else\s*\{\s*(\w+)->addItem\(")
KEY_SET = re.compile(r"QSet<QString> (\w+) = \{([^}]*)\}")
LITERAL_KEY = re.compile(r'"(\w+)"')
LOOP = re.compile(r"for \(const auto &\[param[^\]]*\] : (\w+)\)")
PANEL_BUTTON = re.compile(r'\{tr\("([^"]+)"\), (\w+)\}')
PANEL_BUTTON_ROW = re.compile(r"\{(\{tr\([^}]*\}(?:, \{tr\([^}]*\})*)\}")
PANEL_LIST = re.compile(r"(\w+) = new ScrollView\((\w+),")
PANEL_MAP_ENTRY = re.compile(r'\{"(\w+)", (\w+)\}')
PANEL_SECTION = re.compile(r'\{tr\("([^"]+)"\), tr\(')
SET_LIST = re.compile(r"(\w+)\.contains\(param\)\)\s*\{\s*(\w+)->addItem\(")
SHOW_PANEL = re.compile(r"setCurrentWidget\((\w+)\)")
STOCK_PANEL = re.compile(r'\{tr\("(\w+)"\), (?:new )?(\w+)')
TAG = re.compile(r"<[^>]+>")
TOKEN = re.compile(rf"//[^\n]*|/\*.*?\*/|{STRING}|'(?:[^'\\]|\\.)*'|[{{}}]", re.S)
TUPLE_ROW = re.compile(rf'\{{\s*"(\w+)",\s*{TRANSLATED}(?:,\s*{TRANSLATED})?')
VARIABLE_CLASS = re.compile(r"(?:\w+ \*|auto )(\w+) = new (\w+)\(")

routes = web.RouteTableDef()

def block_end(text, start):
  depth = 0
  for token in TOKEN.finditer(text, text.index("{", start)):
    if token[0] == "{":
      depth += 1
    elif token[0] == "}":
      depth -= 1
      if depth == 0:
        return token.end()
  return len(text)


def block(text, marker):
  start = text.index(marker)
  return text[start:block_end(text, start)]


def plain_text(literals):
  if literals is None:
    return ""

  text = "".join(re.sub(r"\\(.)", r"\1", literal[1:-1]) for literal in re.findall(STRING, literals))
  return " ".join(TAG.sub(" ", text).split())


def get_panel_paths():
  frogpilot_window = (frogpilot_paths.FROGPILOT_PANELS_PATH / "frogpilot_settings.cc").read_text(encoding="utf-8")
  frogpilot_classes = dict(VARIABLE_CLASS.findall(frogpilot_window))

  sections = PANEL_SECTION.findall(block(frogpilot_window, "panelInfo = {"))
  button_rows = PANEL_BUTTON_ROW.findall(block(frogpilot_window, "panelButtons = {"))

  panel_paths = {}
  for section, button_row in zip(sections, button_rows, strict=True):
    buttons = PANEL_BUTTON.findall(button_row)
    for label, variable in buttons:
      if len(buttons) > 1:
        panel_paths[frogpilot_classes[variable]] = ["FrogPilot", section, label.title()]
      else:
        panel_paths[frogpilot_classes[variable]] = ["FrogPilot", section]

  settings_window = (frogpilot_paths.STOCK_PANELS_PATH / "offroad/settings.cc").read_text(encoding="utf-8")
  stock_classes = dict(VARIABLE_CLASS.findall(settings_window))
  for name, variable in STOCK_PANEL.findall(block(settings_window, "panels = {")):
    panel_paths[stock_classes.get(variable, variable)] = [name]

  return panel_paths


def read_panel_file(path, panel_paths):
  text = path.read_text(encoding="utf-8")

  methods = [(match.start(), match[1]) for match in CLASS_METHOD.finditer(text)]
  file_panels = [panel_paths[name] for _, name in methods if name in panel_paths]
  if not file_panels:
    return None

  def panel_at(position):
    names = [name for start, name in methods if start <= position]
    if names and names[-1] in panel_paths:
      return panel_paths[names[-1]]
    return file_panels[0]

  loops = []
  vectors = []
  for match in LOOP.finditer(text):
    loops.append((match.start(), block_end(text, match.start())))

    vector_start = re.search(rf"\b{match[1]}\s*=?\s*\{{", text).start()
    vectors.append((vector_start, block_end(text, vector_start)))

  def loop_end(position):
    return next((end for start, end in loops if start <= position < end), None)

  branches = [match for match in BRANCH.finditer(text) if loop_end(match.start()) is not None]

  def branch_at(position):
    owners = [match[1] for match in branches if match.start() < position]
    return owners[-1] if owners else None

  rows = {}
  sub_controls = {}
  for match in [*TUPLE_ROW.finditer(text), *CONTROL_ROW.finditer(text)]:
    control = {"description": plain_text(match[3]), "position": match.start(), "title": plain_text(match[2])}
    if match.re is CONTROL_ROW and loop_end(match.start()) is not None:
      sub_controls.setdefault(match[1], {**control, "owner": branch_at(match.start())})
    else:
      rows.setdefault(match[1], {**control, "panel": panel_at(match.start()), "parent": None})

  branch_keys = {}
  for index, match in enumerate(branches):
    if index + 1 < len(branches):
      end = branches[index + 1].start()
    else:
      end = loop_end(match.start())

    for key in LITERAL_KEY.findall(text, match.end(), end):
      branch_keys.setdefault(key, match[1])

  list_panels = {list_name: panel for panel, list_name in PANEL_LIST.findall(text)}

  panel_parents = {}
  for match in SHOW_PANEL.finditer(text):
    connect_starts = [connect.start() for connect in CONNECT.finditer(text, 0, match.start())]
    if not connect_starts:
      continue

    connect_start = connect_starts[-1]
    if "clicked" not in text[connect_start:text.index("\n", connect_start)].lower():
      continue

    markers = [branch for branch in branches if branch.start() < connect_start]
    markers += CONTROL_ROW.finditer(text, 0, connect_start)
    if markers:
      owner = max(markers, key=lambda marker: marker.start())
      panel_parents.setdefault(match[1], owner[1])

  for key, panel in PANEL_MAP_ENTRY.findall(text):
    if panel in list_panels.values():
      panel_parents.setdefault(panel, key)

  header_path = path.with_suffix(".h")
  header = header_path.read_text(encoding="utf-8") if header_path.is_file() else ""
  key_sets = {name: LITERAL_KEY.findall(keys) for name, keys in KEY_SET.findall(header + text)}

  set_lists = list(SET_LIST.finditer(text))
  default_list = None
  if set_lists:
    default_match = DEFAULT_LIST.search(text, set_lists[-1].end())
    if default_match:
      default_list = default_match[1]

  for key, row in rows.items():
    list_name = next((match[2] for match in set_lists if key in key_sets.get(match[1], [])), None)
    if list_name is None and any(start <= row["position"] < end for start, end in vectors):
      list_name = default_list
    row["parent"] = panel_parents.get(list_panels.get(list_name))

  return {"branch_keys": branch_keys, "panel_at": panel_at, "rows": rows, "sub_controls": sub_controls, "text": text}


def find_row(key, rows, panel_files):
  if key in rows:
    return key, rows[key]["description"]

  for panel_file in panel_files:
    if key in panel_file["sub_controls"]:
      sub_control = panel_file["sub_controls"][key]
      return sub_control["owner"], sub_control["description"]

  for panel_file in panel_files:
    if key in panel_file["branch_keys"]:
      return panel_file["branch_keys"][key], ""

  name = backups.get_setting_label(frogpilot_variables.SETTING_LABELS[key]["name"], False)
  for panel_file in panel_files:
    position = panel_file["text"].find(f'tr("{name}")')
    if position != -1:
      rows[key] = {"description": "", "panel": panel_file["panel_at"](position), "parent": None, "title": name}
      return key, ""

  prefixes = [row_key for row_key in rows if key.startswith(row_key)]
  if prefixes:
    return max(prefixes, key=len), ""
  return None, ""


@cache
def get_settings():
  tuning_levels = toggles.get_tuning_levels()

  panel_paths = get_panel_paths()
  sources = [*sorted(frogpilot_paths.FROGPILOT_PANELS_PATH.glob("*.cc")), *sorted((frogpilot_paths.STOCK_PANELS_PATH / "offroad").glob("*.cc")),
             frogpilot_paths.STOCK_PANELS_PATH / "network/networking.cc"]

  panel_files = []
  rows = {}
  for path in sources:
    panel_file = read_panel_file(path, panel_paths)
    if panel_file is not None:
      panel_files.append(panel_file)
      for key, row in panel_file["rows"].items():
        rows.setdefault(key, row)

  settings = []
  for key, labels in frogpilot_variables.SETTING_LABELS.items():
    owner, description = find_row(key, rows, panel_files)
    if owner is None:
      continue

    chain = []
    row_key = owner
    while row_key is not None:
      chain.insert(0, row_key)
      row_key = rows[row_key]["parent"]

    path = ["Settings"]
    for step in [*rows[chain[0]]["panel"], *(rows[row_key]["title"].rstrip(":") for row_key in chain)]:
      if step.lower() != path[-1].lower():
        path.append(step)

    settings.append({
      "description": description or rows[owner]["description"],
      "key": key,
      "level": max(tuning_levels.get(row_key, 0) for row_key in [key, *chain]),
      "name": labels["name"],
      "path": path,
      "toggle": key in toggles.KEYS,
    })
  return settings


@routes.get("/api/search")
async def get_search(request):
  metric = request.app[utilities.THE_POND].params.get_bool("IsMetric")
  return web.json_response({
    "levels": list(frogpilot_variables.SETTING_LABELS["TuningLevel"]["values"].values()),
    "settings": [{**setting, "name": backups.get_setting_label(setting["name"], metric)} for setting in get_settings()],
  })
