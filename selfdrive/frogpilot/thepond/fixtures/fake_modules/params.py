import pathlib

DIR_OF_THIS_FILE = pathlib.Path(__file__).parent.absolute()
PARAMS_FILE = f"{DIR_OF_THIS_FILE}/params.txt" # This is where param values are stored when running in debug

class Params:
  def __init__(self, path="") -> None:
    pass

  def get(self, requestedKey):
    with open(PARAMS_FILE) as f:
      lines = f.readlines()
      for line in lines:
        if line == "\n":
          continue
        try:
          key, value = line.strip().split(":", 1) # Split the line into key and value
          if requestedKey == key:
            return str.encode(value)
        except:
          print(f"Error parsing line from params.txt:{lines.index(line)}: ", line)
  
  def put(self, key, value):
    with open(PARAMS_FILE, "r") as f:
      lines = f.readlines()
    with open(PARAMS_FILE, "w") as f:
      hasWritten = False
      for line in lines:
        if line.startswith(f"{key}:"):
          f.write(f"{key}: {value}\n")
          hasWritten = True
        else:
          f.write(line)
      if not hasWritten:
        f.write(f"\n{key}: {value}\n")
  
  def put_bool(self, key, value):
    """
    Don't care about this, it's only used to update "FrogPilotTogglesUpdated"
    """
    pass
