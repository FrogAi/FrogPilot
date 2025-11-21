#pragma once
#include <QString>
#include <QStringList>
#include <map>
#include <QCoreApplication>
#include <QRegularExpression>

struct AlertTranslation {
    const char *raw_text1;
    const char *raw_text2;
    const char *tr_text1;
    const char *tr_text2;
};

inline std::vector<AlertTranslation> alertTranslations = {
  {"Hop in and buckle up!", "Human-tested, frog-approved 🐸", QT_TRANSLATE_NOOP("Alerts", "Hop in and buckle up!"), QT_TRANSLATE_NOOP("Alerts", "Human-tested, frog-approved 🐸")},
  {"Be ready to take over at any time", "Always keep hands on wheel and eyes on road", QT_TRANSLATE_NOOP("Alerts", "Be ready to take over at any time"), QT_TRANSLATE_NOOP("Alerts", "Always keep hands on wheel and eyes on road")},
  {"WARNING: This branch is not tested", "", QT_TRANSLATE_NOOP("Alerts", "WARNING: This branch is not tested"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Drive above %1 to engage", "", QT_TRANSLATE_NOOP("Alerts", "Drive above %1 to engage"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Steer Unavailable Below %1", "", QT_TRANSLATE_NOOP("Alerts", "Steer Unavailable Below %1"), QT_TRANSLATE_NOOP("Alerts", "")},
  //manualy edited
  {"Calibration in Progress: %1%", "Drive Above %1 km/h", QT_TRANSLATE_NOOP("Alerts", "Calibration in Progress: %1%"), QT_TRANSLATE_NOOP("Alerts", "Drive Above %1 km/h")},
  {"Recalibration in Progress: %1%", "Drive Above %1 km/h", QT_TRANSLATE_NOOP("Alerts", "Recalibration in Progress: %1%"), QT_TRANSLATE_NOOP("Alerts", "Drive Above %1 km/h")},
  {"Calibration in Progress: %1%", "Drive Above %1 mph", QT_TRANSLATE_NOOP("Alerts", "Calibration in Progress: %1%"), QT_TRANSLATE_NOOP("Alerts", "Drive Above %1 mph")},
  {"Recalibration in Progress: %1%", "Drive Above %1 mph", QT_TRANSLATE_NOOP("Alerts", "Recalibration in Progress: %1%"), QT_TRANSLATE_NOOP("Alerts", "Drive Above %1 mph")},

   //special alerts
   {"openpilot Unavailable", "", QT_TRANSLATE_NOOP("Alerts", "openpilot Unavailable"), QT_TRANSLATE_NOOP("Alerts", "")},
   {"TAKE CONTROL IMMEDIATELY", "", QT_TRANSLATE_NOOP("Alerts", "TAKE CONTROL IMMEDIATELY"), QT_TRANSLATE_NOOP("Alerts", "")},
   {"openpilot will disengage", "", QT_TRANSLATE_NOOP("Alerts", "openpilot will disengage"), QT_TRANSLATE_NOOP("Alerts", "")},
   {"Posenet Speed Invalid", "", QT_TRANSLATE_NOOP("Alerts", "Posenet Speed Invalid"), QT_TRANSLATE_NOOP("Alerts", "")},
   {"Process Not Running", "", QT_TRANSLATE_NOOP("Alerts", "Process Not Running"), QT_TRANSLATE_NOOP("Alerts", "")},
   {"Communication Issue Between Processes", "", QT_TRANSLATE_NOOP("Alerts", "Communication Issue Between Processes"), QT_TRANSLATE_NOOP("Alerts", "")},
   {"Camera Malfunction, %1", "", QT_TRANSLATE_NOOP("Alerts", "Camera Malfunction, %1"), QT_TRANSLATE_NOOP("Alerts", "")},
   {"Calibration Invalid", "Remount Device (Pitch: %1°, Yaw: %2°)", QT_TRANSLATE_NOOP("Alerts", "Calibration Invalid"), QT_TRANSLATE_NOOP("Alerts", "Remount Device (Pitch: %1°, Yaw: %2°)")},
   {"Enable Cruise Control to Engage", "", QT_TRANSLATE_NOOP("Alerts", "Enable Cruise Control to Engage"), QT_TRANSLATE_NOOP("Alerts", "")},
   {"Enable Main Switch to Engage", "", QT_TRANSLATE_NOOP("Alerts", "Enable Main Switch to Engage"), QT_TRANSLATE_NOOP("Alerts", "")},
   {"Enable Adaptive Cruise to Engage", "", QT_TRANSLATE_NOOP("Alerts", "Enable Adaptive Cruise to Engage"), QT_TRANSLATE_NOOP("Alerts", "")},
   {"Speed Error: %1 m/s", "", QT_TRANSLATE_NOOP("Alerts", "Speed Error: %1 m/s"), QT_TRANSLATE_NOOP("Alerts", "")},


  {"Out of Storage", "%1% full", QT_TRANSLATE_NOOP("Alerts", "Out of Storage"), QT_TRANSLATE_NOOP("Alerts", "%1% full")},
  {"Camera Malfunction", "", QT_TRANSLATE_NOOP("Alerts", "Camera Malfunction"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Calibration Invalid", "", QT_TRANSLATE_NOOP("Alerts", "Calibration Invalid"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"System Overheated", "%1 °C", QT_TRANSLATE_NOOP("Alerts", "System Overheated"), QT_TRANSLATE_NOOP("Alerts", "%1 °C")},
  {"Low Memory", "%1% used", QT_TRANSLATE_NOOP("Alerts", "Low Memory"), QT_TRANSLATE_NOOP("Alerts", "%1% used")},
  {"High CPU Usage", "%1% used", QT_TRANSLATE_NOOP("Alerts", "High CPU Usage"), QT_TRANSLATE_NOOP("Alerts", "%1% used")},
  {"Driving Model Lagging", "%1% frames dropped", QT_TRANSLATE_NOOP("Alerts", "Driving Model Lagging"), QT_TRANSLATE_NOOP("Alerts", "%1% frames dropped")},
  {"Joystick Mode", "Gas: %1%, Steer: %2%", QT_TRANSLATE_NOOP("Alerts", "Joystick Mode"), QT_TRANSLATE_NOOP("Alerts", "Gas: %1%, Steer: %2%")},
  {"Forcing the car to stop in %1", "Press the gas pedal or 'Resume' button to override", QT_TRANSLATE_NOOP("Alerts", "Forcing the car to stop in %1"), QT_TRANSLATE_NOOP("Alerts", "Press the gas pedal or 'Resume' button to override")},
  {"No lane available", "Detected lane width is only %1", QT_TRANSLATE_NOOP("Alerts", "No lane available"), QT_TRANSLATE_NOOP("Alerts", "Detected lane width is only %1")},
  {"NNFF Torque Controller not available", "Donate logs to Twilsonco to get your car supported!", QT_TRANSLATE_NOOP("Alerts", "NNFF Torque Controller not available"), QT_TRANSLATE_NOOP("Alerts", "Donate logs to Twilsonco to get your car supported!")},
  {"NNFF Torque Controller loaded", "", QT_TRANSLATE_NOOP("Alerts", "NNFF Torque Controller loaded"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Joystick Mode", "", QT_TRANSLATE_NOOP("Alerts", "Joystick Mode"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"System Initializing", "", QT_TRANSLATE_NOOP("Alerts", "System Initializing"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Be ready to take over at any time", "", QT_TRANSLATE_NOOP("Alerts", "Be ready to take over at any time"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Dashcam mode", "", QT_TRANSLATE_NOOP("Alerts", "Dashcam mode"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Dashcam mode", "", QT_TRANSLATE_NOOP("Alerts", "Dashcam mode"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Dashcam mode for unsupported car", "", QT_TRANSLATE_NOOP("Alerts", "Dashcam mode for unsupported car"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Car Unrecognized", "Check comma power connections", QT_TRANSLATE_NOOP("Alerts", "Car Unrecognized"), QT_TRANSLATE_NOOP("Alerts", "Check comma power connections")},
  {"Dashcam Mode", "Security Key Not Available", QT_TRANSLATE_NOOP("Alerts", "Dashcam Mode"), QT_TRANSLATE_NOOP("Alerts", "Security Key Not Available")},
  {"Dashcam Mode", "", QT_TRANSLATE_NOOP("Alerts", "Dashcam Mode"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Stock LKAS is on", "Turn off stock LKAS to engage", QT_TRANSLATE_NOOP("Alerts", "Stock LKAS is on"), QT_TRANSLATE_NOOP("Alerts", "Turn off stock LKAS to engage")},
  {"Dashcam Mode", "Car Unrecognized", QT_TRANSLATE_NOOP("Alerts", "Dashcam Mode"), QT_TRANSLATE_NOOP("Alerts", "Car Unrecognized")},
  {"BRAKE!", "Stock AEB: Risk of Collision", QT_TRANSLATE_NOOP("Alerts", "BRAKE!"), QT_TRANSLATE_NOOP("Alerts", "Stock AEB: Risk of Collision")},
  {"Stock AEB: Risk of Collision", "", QT_TRANSLATE_NOOP("Alerts", "Stock AEB: Risk of Collision"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"BRAKE!", "Risk of Collision", QT_TRANSLATE_NOOP("Alerts", "BRAKE!"), QT_TRANSLATE_NOOP("Alerts", "Risk of Collision")},
  {"Lane Departure Detected", "", QT_TRANSLATE_NOOP("Alerts", "Lane Departure Detected"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Steering Temporarily Unavailable", "", QT_TRANSLATE_NOOP("Alerts", "Steering Temporarily Unavailable"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Pay Attention", "", QT_TRANSLATE_NOOP("Alerts", "Pay Attention"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Pay Attention", "Driver Distracted", QT_TRANSLATE_NOOP("Alerts", "Pay Attention"), QT_TRANSLATE_NOOP("Alerts", "Driver Distracted")},
  {"DISENGAGE IMMEDIATELY", "Driver Distracted", QT_TRANSLATE_NOOP("Alerts", "DISENGAGE IMMEDIATELY"), QT_TRANSLATE_NOOP("Alerts", "Driver Distracted")},
  {"Touch Steering Wheel: No Face Detected", "", QT_TRANSLATE_NOOP("Alerts", "Touch Steering Wheel: No Face Detected"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Touch Steering Wheel", "Driver Unresponsive", QT_TRANSLATE_NOOP("Alerts", "Touch Steering Wheel"), QT_TRANSLATE_NOOP("Alerts", "Driver Unresponsive")},
  {"DISENGAGE IMMEDIATELY", "Driver Unresponsive", QT_TRANSLATE_NOOP("Alerts", "DISENGAGE IMMEDIATELY"), QT_TRANSLATE_NOOP("Alerts", "Driver Unresponsive")},
  {"TAKE CONTROL", "Resume Driving Manually", QT_TRANSLATE_NOOP("Alerts", "TAKE CONTROL"), QT_TRANSLATE_NOOP("Alerts", "Resume Driving Manually")},
  {"Press Resume to Exit Standstill", "", QT_TRANSLATE_NOOP("Alerts", "Press Resume to Exit Standstill"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Steer Left to Start Lane Change Once Safe", "", QT_TRANSLATE_NOOP("Alerts", "Steer Left to Start Lane Change Once Safe"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Steer Right to Start Lane Change Once Safe", "", QT_TRANSLATE_NOOP("Alerts", "Steer Right to Start Lane Change Once Safe"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Car Detected in Blindspot", "", QT_TRANSLATE_NOOP("Alerts", "Car Detected in Blindspot"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Changing Lanes", "", QT_TRANSLATE_NOOP("Alerts", "Changing Lanes"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Take Control", "Turn Exceeds Steering Limit", QT_TRANSLATE_NOOP("Alerts", "Take Control"), QT_TRANSLATE_NOOP("Alerts", "Turn Exceeds Steering Limit")},
  {"Fan Malfunction", "Likely Hardware Issue", QT_TRANSLATE_NOOP("Alerts", "Fan Malfunction"), QT_TRANSLATE_NOOP("Alerts", "Likely Hardware Issue")},
  {"Camera Malfunction: Reboot Your Device", "", QT_TRANSLATE_NOOP("Alerts", "Camera Malfunction: Reboot Your Device"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Camera Frame Rate Low", "Reboot your Device", QT_TRANSLATE_NOOP("Alerts", "Camera Frame Rate Low"), QT_TRANSLATE_NOOP("Alerts", "Reboot your Device")},
  {"Camera Frame Rate Low: Reboot Your Device", "", QT_TRANSLATE_NOOP("Alerts", "Camera Frame Rate Low: Reboot Your Device"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"locationd Temporary Error", "", QT_TRANSLATE_NOOP("Alerts", "locationd Temporary Error"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"locationd Permanent Error", "", QT_TRANSLATE_NOOP("Alerts", "locationd Permanent Error"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"locationd Permanent Error", "", QT_TRANSLATE_NOOP("Alerts", "locationd Permanent Error"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"locationd Permanent Error", "", QT_TRANSLATE_NOOP("Alerts", "locationd Permanent Error"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"paramsd Temporary Error", "", QT_TRANSLATE_NOOP("Alerts", "paramsd Temporary Error"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"paramsd Permanent Error", "", QT_TRANSLATE_NOOP("Alerts", "paramsd Permanent Error"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"paramsd Permanent Error", "", QT_TRANSLATE_NOOP("Alerts", "paramsd Permanent Error"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"paramsd Permanent Error", "", QT_TRANSLATE_NOOP("Alerts", "paramsd Permanent Error"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Cancel Pressed", "", QT_TRANSLATE_NOOP("Alerts", "Cancel Pressed"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Brake Hold Active", "", QT_TRANSLATE_NOOP("Alerts", "Brake Hold Active"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Parking Brake Engaged", "", QT_TRANSLATE_NOOP("Alerts", "Parking Brake Engaged"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Pedal Pressed", "", QT_TRANSLATE_NOOP("Alerts", "Pedal Pressed"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Release Brake to Engage", "", QT_TRANSLATE_NOOP("Alerts", "Release Brake to Engage"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Press Set to Engage", "", QT_TRANSLATE_NOOP("Alerts", "Press Set to Engage"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Adaptive Cruise Disabled", "", QT_TRANSLATE_NOOP("Alerts", "Adaptive Cruise Disabled"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Steering Temporarily Unavailable", "", QT_TRANSLATE_NOOP("Alerts", "Steering Temporarily Unavailable"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Vehicle Steering Time Limit", "", QT_TRANSLATE_NOOP("Alerts", "Vehicle Steering Time Limit"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Out of Storage", "", QT_TRANSLATE_NOOP("Alerts", "Out of Storage"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Sensor Data Invalid", "Possible Hardware Issue", QT_TRANSLATE_NOOP("Alerts", "Sensor Data Invalid"), QT_TRANSLATE_NOOP("Alerts", "Possible Hardware Issue")},
  {"Sensor Data Invalid", "", QT_TRANSLATE_NOOP("Alerts", "Sensor Data Invalid"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Poor GPS reception", "Ensure device has a clear view of the sky", QT_TRANSLATE_NOOP("Alerts", "Poor GPS reception"), QT_TRANSLATE_NOOP("Alerts", "Ensure device has a clear view of the sky")},
  {"Speaker not found", "Reboot your Device", QT_TRANSLATE_NOOP("Alerts", "Speaker not found"), QT_TRANSLATE_NOOP("Alerts", "Reboot your Device")},
  {"Speaker not found", "", QT_TRANSLATE_NOOP("Alerts", "Speaker not found"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Distraction Level Too High", "", QT_TRANSLATE_NOOP("Alerts", "Distraction Level Too High"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"System Overheated", "", QT_TRANSLATE_NOOP("Alerts", "System Overheated"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Gear not D", "", QT_TRANSLATE_NOOP("Alerts", "Gear not D"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Calibration Invalid: Remount Device & Recalibrate", "", QT_TRANSLATE_NOOP("Alerts", "Calibration Invalid: Remount Device & Recalibrate"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Calibration in Progress", "", QT_TRANSLATE_NOOP("Alerts", "Calibration in Progress"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Remount Detected: Recalibrating", "", QT_TRANSLATE_NOOP("Alerts", "Remount Detected: Recalibrating"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Door Open", "", QT_TRANSLATE_NOOP("Alerts", "Door Open"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Seatbelt Unlatched", "", QT_TRANSLATE_NOOP("Alerts", "Seatbelt Unlatched"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Electronic Stability Control Disabled", "", QT_TRANSLATE_NOOP("Alerts", "Electronic Stability Control Disabled"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Low Battery", "", QT_TRANSLATE_NOOP("Alerts", "Low Battery"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Low Communication Rate Between Processes", "", QT_TRANSLATE_NOOP("Alerts", "Low Communication Rate Between Processes"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Controls Process Lagging: Reboot Your Device", "", QT_TRANSLATE_NOOP("Alerts", "Controls Process Lagging: Reboot Your Device"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Radar Error: Restart the Car", "", QT_TRANSLATE_NOOP("Alerts", "Radar Error: Restart the Car"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Driving Model Lagging", "", QT_TRANSLATE_NOOP("Alerts", "Driving Model Lagging"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Device Fell Off Mount", "", QT_TRANSLATE_NOOP("Alerts", "Device Fell Off Mount"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Low Memory: Reboot Your Device", "", QT_TRANSLATE_NOOP("Alerts", "Low Memory: Reboot Your Device"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Cruise Fault: Restart the Car", "", QT_TRANSLATE_NOOP("Alerts", "Cruise Fault: Restart the Car"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Cruise Fault: Restart the car to engage", "", QT_TRANSLATE_NOOP("Alerts", "Cruise Fault: Restart the car to engage"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Cruise Fault: Restart the Car", "", QT_TRANSLATE_NOOP("Alerts", "Cruise Fault: Restart the Car"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Controls Mismatch", "", QT_TRANSLATE_NOOP("Alerts", "Controls Mismatch"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Controls Mismatch", "", QT_TRANSLATE_NOOP("Alerts", "Controls Mismatch"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Camera CRC Error - Road", "", QT_TRANSLATE_NOOP("Alerts", "Camera CRC Error - Road"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Camera CRC Error - Road Fisheye", "", QT_TRANSLATE_NOOP("Alerts", "Camera CRC Error - Road Fisheye"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Camera CRC Error - Driver", "", QT_TRANSLATE_NOOP("Alerts", "Camera CRC Error - Driver"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"USB Error: Reboot Your Device", "", QT_TRANSLATE_NOOP("Alerts", "USB Error: Reboot Your Device"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"USB Error: Reboot Your Device", "", QT_TRANSLATE_NOOP("Alerts", "USB Error: Reboot Your Device"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"CAN Error", "", QT_TRANSLATE_NOOP("Alerts", "CAN Error"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"CAN Error: Check Connections", "", QT_TRANSLATE_NOOP("Alerts", "CAN Error: Check Connections"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"CAN Error: Check Connections", "", QT_TRANSLATE_NOOP("Alerts", "CAN Error: Check Connections"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"CAN Bus Disconnected", "", QT_TRANSLATE_NOOP("Alerts", "CAN Bus Disconnected"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"CAN Bus Disconnected: Likely Faulty Cable", "", QT_TRANSLATE_NOOP("Alerts", "CAN Bus Disconnected: Likely Faulty Cable"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"CAN Bus Disconnected: Check Connections", "", QT_TRANSLATE_NOOP("Alerts", "CAN Bus Disconnected: Check Connections"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"LKAS Fault: Restart the Car", "", QT_TRANSLATE_NOOP("Alerts", "LKAS Fault: Restart the Car"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"LKAS Fault: Restart the car to engage", "", QT_TRANSLATE_NOOP("Alerts", "LKAS Fault: Restart the car to engage"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"LKAS Fault: Restart the Car", "", QT_TRANSLATE_NOOP("Alerts", "LKAS Fault: Restart the Car"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Reverse%1Gear", "", QT_TRANSLATE_NOOP("Alerts", "Reverse<br>Gear"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Reverse Gear", "", QT_TRANSLATE_NOOP("Alerts", "Reverse Gear"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Reverse Gear", "", QT_TRANSLATE_NOOP("Alerts", "Reverse Gear"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Cruise Is Off", "", QT_TRANSLATE_NOOP("Alerts", "Cruise Is Off"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Harness Relay Malfunction", "", QT_TRANSLATE_NOOP("Alerts", "Harness Relay Malfunction"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Harness Relay Malfunction", "Check Hardware", QT_TRANSLATE_NOOP("Alerts", "Harness Relay Malfunction"), QT_TRANSLATE_NOOP("Alerts", "Check Hardware")},
  {"Harness Relay Malfunction", "", QT_TRANSLATE_NOOP("Alerts", "Harness Relay Malfunction"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"openpilot Canceled", "Speed too low", QT_TRANSLATE_NOOP("Alerts", "openpilot Canceled"), QT_TRANSLATE_NOOP("Alerts", "Speed too low")},
  {"Speed Too High", "Model uncertain at this speed", QT_TRANSLATE_NOOP("Alerts", "Speed Too High"), QT_TRANSLATE_NOOP("Alerts", "Model uncertain at this speed")},
  {"Slow down to engage", "", QT_TRANSLATE_NOOP("Alerts", "Slow down to engage"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Cruise Fault: Restart the car to engage", "", QT_TRANSLATE_NOOP("Alerts", "Cruise Fault: Restart the car to engage"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Cruise Fault: Restart the Car", "", QT_TRANSLATE_NOOP("Alerts", "Cruise Fault: Restart the Car"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"LKAS Disabled: Enable LKAS to engage", "", QT_TRANSLATE_NOOP("Alerts", "LKAS Disabled: Enable LKAS to engage"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"LKAS Disabled", "", QT_TRANSLATE_NOOP("Alerts", "LKAS Disabled"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Vehicle Sensors Invalid", "", QT_TRANSLATE_NOOP("Alerts", "Vehicle Sensors Invalid"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Vehicle Sensors Calibrating", "Drive to Calibrate", QT_TRANSLATE_NOOP("Alerts", "Vehicle Sensors Calibrating"), QT_TRANSLATE_NOOP("Alerts", "Drive to Calibrate")},
  {"Vehicle Sensors Calibrating", "", QT_TRANSLATE_NOOP("Alerts", "Vehicle Sensors Calibrating"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Don't use the 'Development' branch!", "Forcing you into 'Dashcam Mode' for your safety", QT_TRANSLATE_NOOP("Alerts", "Don't use the 'Development' branch!"), QT_TRANSLATE_NOOP("Alerts", "Forcing you into 'Dashcam Mode' for your safety")},
  {"JESUS TAKE THE WHEEL!!", "Turn Exceeds Steering Limit", QT_TRANSLATE_NOOP("Alerts", "JESUS TAKE THE WHEEL!!"), QT_TRANSLATE_NOOP("Alerts", "Turn Exceeds Steering Limit")},
  {"Light turned green", "", QT_TRANSLATE_NOOP("Alerts", "Light turned green"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Car Detected in Blindspot", "", QT_TRANSLATE_NOOP("Alerts", "Car Detected in Blindspot"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Lead departed", "", QT_TRANSLATE_NOOP("Alerts", "Lead departed"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"openpilot crashed", "Please post the 'Error Log' in the FrogPilot Discord!", QT_TRANSLATE_NOOP("Alerts", "openpilot crashed"), QT_TRANSLATE_NOOP("Alerts", "Please post the 'Error Log' in the FrogPilot Discord!")},
  {"openpilot crashed", "Please post the 'Error Log' in the FrogPilot Discord!", QT_TRANSLATE_NOOP("Alerts", "openpilot crashed"), QT_TRANSLATE_NOOP("Alerts", "Please post the 'Error Log' in the FrogPilot Discord!")},
  {"Braking Unavailable", "Shift to L", QT_TRANSLATE_NOOP("Alerts", "Braking Unavailable"), QT_TRANSLATE_NOOP("Alerts", "Shift to L")},
  {"Speed limit changed", "", QT_TRANSLATE_NOOP("Alerts", "Speed limit changed"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"This is fine ☕", "Turn Exceeds Steering Limit", QT_TRANSLATE_NOOP("Alerts", "This is fine ☕"), QT_TRANSLATE_NOOP("Alerts", "Turn Exceeds Steering Limit")},
  {"Traffic Mode enabled", "", QT_TRANSLATE_NOOP("Alerts", "Traffic Mode enabled"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Traffic Mode Disabled", "", QT_TRANSLATE_NOOP("Alerts", "Traffic Mode Disabled"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Turning left", "", QT_TRANSLATE_NOOP("Alerts", "Turning left"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Turning right", "", QT_TRANSLATE_NOOP("Alerts", "Turning right"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"UwU u went a bit fast there!", "(⁄ ⁄•⁄ω⁄•⁄ ⁄)", QT_TRANSLATE_NOOP("Alerts", "UwU u went a bit fast there!"), QT_TRANSLATE_NOOP("Alerts", "(⁄ ⁄•⁄ω⁄•⁄ ⁄)")},
  {"I ain't giving you no tree-fiddy", "You damn Loch Ness Monsta!", QT_TRANSLATE_NOOP("Alerts", "I ain't giving you no tree-fiddy"), QT_TRANSLATE_NOOP("Alerts", "You damn Loch Ness Monsta!")},
  {"Great Scott!", "🚗💨", QT_TRANSLATE_NOOP("Alerts", "Great Scott!"), QT_TRANSLATE_NOOP("Alerts", "🚗💨")},
  {"♬♪ Deja vu! ᕕ(⌐■_■)ᕗ ♪♬", "🏎️", QT_TRANSLATE_NOOP("Alerts", "♬♪ Deja vu! ᕕ(⌐■_■)ᕗ ♪♬"), QT_TRANSLATE_NOOP("Alerts", "🏎️")},
  {"IE Has Stopped Responding...", "Turn Exceeds Steering Limit", QT_TRANSLATE_NOOP("Alerts", "IE Has Stopped Responding..."), QT_TRANSLATE_NOOP("Alerts", "Turn Exceeds Steering Limit")},
  {"I'm sorry Dave", "I'm afraid I can't do that...", QT_TRANSLATE_NOOP("Alerts", "I'm sorry Dave"), QT_TRANSLATE_NOOP("Alerts", "I'm afraid I can't do that...")},
  {"openpilot crashed 💩", "Please post the 'Error Log' in the FrogPilot Discord!", QT_TRANSLATE_NOOP("Alerts", "openpilot crashed 💩"), QT_TRANSLATE_NOOP("Alerts", "Please post the 'Error Log' in the FrogPilot Discord!")},
  {"openpilot crashed 💩", "Please post the 'Error Log' in the FrogPilot Discord!", QT_TRANSLATE_NOOP("Alerts", "openpilot crashed 💩"), QT_TRANSLATE_NOOP("Alerts", "Please post the 'Error Log' in the FrogPilot Discord!")},
  {"To be continued...", "⬅️", QT_TRANSLATE_NOOP("Alerts", "To be continued..."), QT_TRANSLATE_NOOP("Alerts", "⬅️")},
  {"Lol 69", "", QT_TRANSLATE_NOOP("Alerts", "Lol 69"), QT_TRANSLATE_NOOP("Alerts", "")},
  {"Your Frog tried to kill me...", "👺", QT_TRANSLATE_NOOP("Alerts", "Your Frog tried to kill me..."), QT_TRANSLATE_NOOP("Alerts", "👺")},
  {"You've got mail! 📧", "", QT_TRANSLATE_NOOP("Alerts", "You've got mail! 📧"), QT_TRANSLATE_NOOP("Alerts", "")},
};


// Helper function to build a regex from a pattern with %N placeholders.
inline QString makeRegex(const QString &pattern) {
    // Use QRegularExpression to find all %N placeholders.
    QRegularExpression re("%[1-9][0-9]*");
    QString result;
    int lastPos = 0;
    auto it = re.globalMatch(pattern);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        int start = m.capturedStart();
        int end = m.capturedEnd();
        // Escape the text before the placeholder.
        result += QRegularExpression::escape(pattern.mid(lastPos, start - lastPos));
        // Replace placeholder with capturing group that matches any character including newlines.
        result += "([\\s\\S]*)";
        lastPos = end;
    }
    // Escape any trailing text after the last placeholder.
    result += QRegularExpression::escape(pattern.mid(lastPos));
    return result;
}


// Unified translation function for both alert text1 and text2.
inline QString translateAlert(const QString &text, const QStringList &params = {}) {
    // Try to match against both raw_text1 and raw_text2 in alertTranslations.
    for (const auto &alert : alertTranslations) {
        // Check raw_text1
        if (alert.raw_text1 && strlen(alert.raw_text1) > 0) {
            QString pattern = alert.raw_text1;
            if (pattern.contains("%")) {
                QString regexPattern = makeRegex(pattern);
                QRegularExpression rx("^" + regexPattern + "$");
                QRegularExpressionMatch match = rx.match(text);
                if (match.hasMatch()) {
                    QString translated = QCoreApplication::translate("Alerts", alert.tr_text1);
                    QStringList usedParams = params;
                    if (usedParams.isEmpty()) {
                        // Extract captured groups as parameters.
                        for (int i = 1; i <= match.lastCapturedIndex(); ++i) {
                            usedParams << match.captured(i);
                        }
                    }
                    for (int i = 0; i < usedParams.size(); ++i) {
                        translated = translated.arg(usedParams[i]);
                    }
                    return translated;
                }
            } else if (pattern == text) {
                QString translated = QCoreApplication::translate("Alerts", alert.tr_text1);
                QStringList usedParams = params;
                for (int i = 0; i < usedParams.size(); ++i) {
                    translated = translated.arg(usedParams[i]);
                }
                return translated;
            }
        }
        // Check raw_text2
        if (alert.raw_text2 && strlen(alert.raw_text2) > 0) {
            QString pattern = alert.raw_text2;
            if (pattern.contains("%")) {
                QString regexPattern = makeRegex(pattern);
                QRegularExpression rx("^" + regexPattern + "$");
                QRegularExpressionMatch match = rx.match(text);
                if (match.hasMatch()) {
                    QString translated = QCoreApplication::translate("Alerts", alert.tr_text2);
                    QStringList usedParams = params;
                    if (usedParams.isEmpty()) {
                        // Extract captured groups as parameters.
                        for (int i = 1; i <= match.lastCapturedIndex(); ++i) {
                            usedParams << match.captured(i);
                        }
                    }
                    for (int i = 0; i < usedParams.size(); ++i) {
                        translated = translated.arg(usedParams[i]);
                    }
                    return translated;
                }
            } else if (pattern == text) {
                QString translated = QCoreApplication::translate("Alerts", alert.tr_text2);
                QStringList usedParams = params;
                for (int i = 0; i < usedParams.size(); ++i) {
                    translated = translated.arg(usedParams[i]);
                }
                return translated;
            }
        }
    }
    // No match found, return the original text.
    return text;
}