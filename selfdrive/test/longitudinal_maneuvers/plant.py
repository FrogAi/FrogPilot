#!/usr/bin/env python3
import time
import numpy as np

from types import SimpleNamespace

from cereal import log
import cereal.messaging as messaging
from openpilot.common.conversions import Conversions as CV
from openpilot.common.realtime import Ratekeeper, DT_MDL
from openpilot.selfdrive.controls.lib.drive_helpers import V_CRUISE_MAX
from openpilot.selfdrive.controls.lib.longcontrol import LongCtrlState
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import A_CHANGE_COST, CRUISE_MIN_ACCEL, DANGER_ZONE_COST, J_EGO_COST
from openpilot.selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import get_jerk_factor, get_T_FOLLOW
from openpilot.selfdrive.modeld.constants import ModelConstants
from openpilot.selfdrive.controls.lib.longitudinal_planner import LongitudinalPlanner, get_max_accel
from openpilot.selfdrive.controls.radard import _LEAD_ACCEL_TAU


class Plant:
  messaging_initialized = False

  def __init__(self, lead_relevancy=False, speed=0.0, distance_lead=2.0,
               enabled=True, only_lead2=False, only_radar=False, e2e=False, personality=0, force_decel=False):
    self.rate = 1. / DT_MDL

    if not Plant.messaging_initialized:
      Plant.radar = messaging.pub_sock('radarState')
      Plant.controls_state = messaging.pub_sock('controlsState')
      Plant.car_state = messaging.pub_sock('carState')
      Plant.plan = messaging.sub_sock('longitudinalPlan')
      Plant.messaging_initialized = True

    self.v_lead_prev = 0.0

    self.distance = 0.
    self.speed = speed
    self.should_stop = False
    self.acceleration = 0.0

    # lead car
    self.lead_relevancy = lead_relevancy
    self.distance_lead = distance_lead
    self.enabled = enabled
    self.only_lead2 = only_lead2
    self.only_radar = only_radar
    self.e2e = e2e
    self.personality = personality
    self.force_decel = force_decel

    self.rk = Ratekeeper(self.rate, print_delay_threshold=100.0)
    self.ts = 1. / self.rate
    time.sleep(0.1)
    self.sm = messaging.SubMaster(['longitudinalPlan'])

    from openpilot.selfdrive.car.honda.values import CAR
    from openpilot.selfdrive.car.honda.interface import CarInterface

    # FrogPilot variables
    self.frogpilot_toggles = SimpleNamespace(curve_speed_controller=False, force_torque_controller=False, honda_alt_tune=False, human_following=False,
                                             nnff_lite_selected=False, nnff_selected=False, old_long_api=False, taco_tune=False)

    CP = CarInterface.get_non_essential_params(CAR.HONDA_CIVIC, self.frogpilot_toggles)
    self.frogpilot_toggles.longitudinalActuatorDelay = CP.longitudinalActuatorDelay
    self.frogpilot_toggles.vEgoStopping = CP.vEgoStopping

    self.planner = LongitudinalPlanner(CP, init_v=self.speed)

  @property
  def current_time(self):
    return float(self.rk.frame) / self.rate

  def step(self, v_lead=0.0, prob_lead=1.0, v_cruise=50., pitch=0.0, prob_throttle=1.0):
    # ******** publish a fake model going straight and fake calibration ********
    # note that this is worst case for MPC, since model will delay long mpc by one time step
    radar = messaging.new_message('radarState')
    control = messaging.new_message('controlsState')
    car_state = messaging.new_message('carState')
    car_control = messaging.new_message('carControl')
    model = messaging.new_message('modelV2')

    # FrogPilot variables
    frogpilot_car_state = messaging.new_message('frogpilotCarState')
    frogpilot_plan = messaging.new_message('frogpilotPlan')
    live_parameters = messaging.new_message('liveParameters')

    a_lead = (v_lead - self.v_lead_prev)/self.ts
    self.v_lead_prev = v_lead

    if self.lead_relevancy:
      d_rel = np.maximum(0., self.distance_lead - self.distance)
      v_rel = v_lead - self.speed
      if self.only_radar:
        status = True
      elif prob_lead > .5:
        status = True
      else:
        status = False
    else:
      d_rel = 200.
      v_rel = 0.
      prob_lead = 0.0
      status = False

    lead = log.RadarState.LeadData.new_message()
    lead.dRel = float(d_rel)
    lead.yRel = 0.0
    lead.vRel = float(v_rel)
    lead.aRel = float(a_lead - self.acceleration)
    lead.vLead = float(v_lead)
    lead.vLeadK = float(v_lead)
    lead.aLeadK = float(a_lead)
    # TODO use real radard logic for this
    lead.aLeadTau = float(_LEAD_ACCEL_TAU)
    lead.status = status
    lead.modelProb = float(prob_lead)
    if not self.only_lead2:
      radar.radarState.leadOne = lead
    radar.radarState.leadTwo = lead

    # Simulate model predicting slightly faster speed
    # this is to ensure lead policy is effective when model
    # does not predict slowdown in e2e mode
    position = log.XYZTData.new_message()
    position.x = [float(x) for x in (self.speed + 0.5) * np.array(ModelConstants.T_IDXS)]
    model.modelV2.position = position
    velocity = log.XYZTData.new_message()
    velocity.x = [float(x) for x in (self.speed + 0.5) * np.ones_like(ModelConstants.T_IDXS)]
    model.modelV2.velocity = velocity
    acceleration = log.XYZTData.new_message()
    acceleration.x = [float(x) for x in np.zeros_like(ModelConstants.T_IDXS)]
    model.modelV2.acceleration = acceleration
    model.modelV2.meta.disengagePredictions.gasPressProbs = [float(prob_throttle) for _ in range(6)]

    # FrogPilot variables
    model.modelV2.init('leadsV3', 2)

    control.controlsState.longControlState = LongCtrlState.pid if self.enabled else LongCtrlState.off
    control.controlsState.vCruise = float(v_cruise * 3.6)
    control.controlsState.experimentalMode = self.e2e
    control.controlsState.personality = self.personality
    control.controlsState.forceDecel = self.force_decel
    car_state.carState.vEgo = float(self.speed)
    car_state.carState.standstill = bool(self.speed < 0.01)
    car_control.carControl.orientationNED = [0., float(pitch), 0.]

    # FrogPilot variables
    jerk_factor = get_jerk_factor(control.controlsState.personality)
    frogpilot_plan.frogpilotPlan.accelerationJerk = A_CHANGE_COST * jerk_factor
    frogpilot_plan.frogpilotPlan.dangerJerk = DANGER_ZONE_COST
    frogpilot_plan.frogpilotPlan.maxAcceleration = get_max_accel(self.speed)
    frogpilot_plan.frogpilotPlan.minAcceleration = CRUISE_MIN_ACCEL
    frogpilot_plan.frogpilotPlan.speedJerk = J_EGO_COST * jerk_factor
    frogpilot_plan.frogpilotPlan.tFollow = get_T_FOLLOW(control.controlsState.personality)
    frogpilot_plan.frogpilotPlan.vCruise = min(control.controlsState.vCruise, V_CRUISE_MAX) * CV.KPH_TO_MS

    # ******** get controlsState messages for plotting ***
    sm = {'radarState': radar.radarState,
          'carState': car_state.carState,
          'carControl': car_control.carControl,
          'controlsState': control.controlsState,
          'modelV2': model.modelV2,

          # FrogPilot variables
          'frogpilotCarState': frogpilot_car_state.frogpilotCarState,
          'frogpilotPlan': frogpilot_plan.frogpilotPlan,
          'liveParameters': live_parameters.liveParameters}
    self.planner.update(sm, True, False, self.frogpilot_toggles)
    self.acceleration = self.planner.output_a_target
    self.speed = self.speed + self.acceleration * self.ts
    self.should_stop = self.planner.output_should_stop
    fcw = self.planner.fcw
    self.distance_lead = self.distance_lead + v_lead * self.ts

    # ******** run the car ********
    #print(self.distance, speed)
    if self.speed <= 0 or (self.should_stop and self.speed < self.frogpilot_toggles.vEgoStopping and self.acceleration <= 0):
      self.speed = 0
      self.acceleration = 0
    self.distance = self.distance + self.speed * self.ts

    # *** radar model ***
    if self.lead_relevancy:
      d_rel = np.maximum(0., self.distance_lead - self.distance)
      v_rel = v_lead - self.speed
    else:
      d_rel = 200.
      v_rel = 0.

    # print at 5hz
    # if (self.rk.frame % (self.rate // 5)) == 0:
    #   print("%2.2f sec   %6.2f m  %6.2f m/s  %6.2f m/s2   lead_rel: %6.2f m  %6.2f m/s"
    #         % (self.current_time, self.distance, self.speed, self.acceleration, d_rel, v_rel))


    # ******** update prevs ********
    self.rk.monitor_time()

    return {
      "distance": self.distance,
      "speed": self.speed,
      "acceleration": self.acceleration,
      "should_stop": self.should_stop,
      "distance_lead": self.distance_lead,
      "fcw": fcw,
    }

# simple engage in standalone mode
def plant_thread():
  plant = Plant()
  while 1:
    plant.step()


if __name__ == "__main__":
  plant_thread()
