class MemoryParams:
  """The small Params API used by Vision's worker and controller tests."""

  def __init__(self):
    self.values = {}

  def get(self, key):
    return self.values.get(key)

  def get_bool(self, key):
    return bool(self.get(key))

  def put(self, key, value):
    self.values[key] = value

  put_nonblocking = put

  def remove(self, key):
    self.values.pop(key, None)
