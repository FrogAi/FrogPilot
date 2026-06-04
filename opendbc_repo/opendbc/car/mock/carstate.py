from cereal import custom
from opendbc.car import structs
from opendbc.car.interfaces import CarStateBase


class CarState(CarStateBase):
  def update(self, *_) -> tuple[structs.CarState, custom.FrogPilotCarState]:
    return structs.CarState(), custom.FrogPilotCarState.new_message()
