#include "opendbc/safety/declarations.h"

bool get_longitudinal_allowed(void) {
  return controls_allowed && !gas_pressed_prev;
}

// Safety checks for longitudinal actuation
bool longitudinal_accel_checks(int desired_accel, const LongitudinalLimits limits) {
  bool accel_valid = get_longitudinal_allowed() && !safety_max_limit_check(desired_accel, limits.max_accel, limits.min_accel);
  bool accel_inactive = desired_accel == limits.inactive_accel;
  return !(accel_valid || accel_inactive);
}

bool longitudinal_speed_checks(int desired_speed, const LongitudinalLimits limits) {
  return !get_longitudinal_allowed() && (desired_speed != limits.inactive_speed);
}

bool longitudinal_transmission_rpm_checks(int desired_transmission_rpm, const LongitudinalLimits limits) {
  bool transmission_rpm_valid = get_longitudinal_allowed() && !safety_max_limit_check(desired_transmission_rpm, limits.max_transmission_rpm, limits.min_transmission_rpm);
  bool transmission_rpm_inactive = desired_transmission_rpm == limits.inactive_transmission_rpm;
  return !(transmission_rpm_valid || transmission_rpm_inactive);
}

bool longitudinal_gas_checks(int desired_gas, const LongitudinalLimits limits) {
  bool gas_valid = get_longitudinal_allowed() && !safety_max_limit_check(desired_gas, limits.max_gas, limits.min_gas);
  bool gas_inactive = desired_gas == limits.inactive_gas;
  return !(gas_valid || gas_inactive);
}

bool longitudinal_brake_checks(int desired_brake, const LongitudinalLimits limits) {
  bool violation = false;
  violation |= !get_longitudinal_allowed() && (desired_brake != 0);
  violation |= desired_brake > limits.max_brake;
  return violation;
}

// OPGM variables
bool longitudinal_interceptor_checks(const CANPacket_t *msg, int max_interceptor_track_1, int max_interceptor_track_2) {
  int interceptor_track_1 = (msg->data[0] << 8) | msg->data[1];
  int interceptor_track_2 = (msg->data[2] << 8) | msg->data[3];

  bool interceptor_active = (interceptor_track_1 != 0) || (interceptor_track_2 != 0);
  bool violation = false;
  violation |= (!get_longitudinal_allowed() || brake_pressed_prev) && interceptor_active;
  violation |= interceptor_track_1 > max_interceptor_track_1;
  violation |= interceptor_track_2 > max_interceptor_track_2;

  return violation;
}
