import math

ENABLE_CONFIRMATION = {"cancel": "Cancel", "confirm": "Enable", "text": None}


def car_title(context, title, car_value):
  if not context.car_detected:
    return title
  return f"{title} (Default: {car_value:.2f})"


def number(minimum, maximum, step=1, unit="", labels=None, fast=False, decimals=None, metric=None):
  if decimals is None:
    decimals = math.ceil(-math.log10(step))

  return {"decimals": decimals, "fast": fast, "labels": labels or {}, "max": maximum, "metric": metric, "min": minimum, "step": step, "unit": unit}


def unit_labels(minimum, maximum, step, singular, plural, zero=None):
  decimals = math.ceil(-math.log10(step))

  labels = {}
  for index in range(round(minimum / step), round(maximum / step) + 1):
    value = round(index * step, decimals)
    if value == 0 and zero is not None:
      labels[value] = zero
    elif value == 1:
      labels[value] = f"{value:.{decimals}f}{singular}"
    else:
      labels[value] = f"{value:.{decimals}f}{plural}"
  return labels
