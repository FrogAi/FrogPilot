using Cxx = import "./include/c++.capnp";
$Cxx.namespace("cereal");

using Car = import "car.capnp";

@0xb526ba661d550a59;

# custom.capnp: a home for empty structs reserved for custom forks
# These structs are guaranteed to remain reserved and empty in mainline
# cereal, so use these if you want custom events in your fork.

# you can rename the struct, but don't change the identifier
struct FrogPilotCarParams @0x81c2f05a394cf4af {
  fpFlags @0 :UInt32;
  isHDA2 @1 :Bool;
  openpilotLongitudinalControlDisabled @2 :Bool;
}

struct FrogPilotCarState @0xaedffd8f31e7b55d {
  struct ButtonEvent {
    enum Type {
      lkas @0;
    }
  }

  accelPressed @0 :Bool;
  alwaysOnLateralAllowed @1 :Bool;
  alwaysOnLateralEnabled @2 :Bool;
  brakeLights @3 :Bool;
  dashboardSpeedLimit @4 :Float32;
  decelPressed @5 :Bool;
  distancePressed @6 :Bool;
  distanceLongPressed @7 :Bool;
  distanceVeryLongPressed @8 :Bool;
  ecoGear @9 :Bool;
  forceCoast @10 :Bool;
  pauseLateral @11 :Bool;
  pauseLongitudinal @12 :Bool;
  sportGear @13 :Bool;
  trafficModeEnabled @14 :Bool;
}

struct FrogPilotDeviceState @0xf35cc4560bbf6ec2 {
  freeSpace @0 :Int16;
  usedSpace @1 :Int16;
}

struct FrogPilotNavigation @0xda96579883444c35 {
  approachingIntersection @0 :Bool;
  approachingTurn @1 :Bool;
  navigationSpeedLimit @2 :Float32;
}

struct FrogPilotPlan @0x80ae746ee2596b11 {
  accelerationJerk @0 :Float32;
  accelerationJerkStock @1 :Float32;
  cscControllingSpeed @2 :Bool;
  cscSpeed @3 :Float32;
  cscTraining @4 :Bool;
  dangerJerk @5 :Float32;
  desiredFollowDistance @6 :Int64;
  experimentalMode @7 :Bool;
  forcingStop @8 :Bool;
  forcingStopLength @9 :Float32;
  frogpilotEvents @10 :List(Car.CarEvent);
  lateralCheck @11 :Bool;
  laneWidthLeft @12 :Float32;
  laneWidthRight @13 :Float32;
  maxAcceleration @14 :Float32;
  minAcceleration @15 :Float32;
  redLight @16 :Bool;
  roadCurvature @17 :Float32;
  slcMapSpeedLimit @18 :Float32;
  slcMapboxSpeedLimit @19 :Float32;
  slcNextSpeedLimit @20 :Float32;
  slcOverriddenSpeed @21 :Float32;
  slcSpeedLimit @22 :Float32;
  slcSpeedLimitOffset @23 :Float32;
  slcSpeedLimitSource @24 :Text;
  speedJerk @25 :Float32;
  speedJerkStock @26 :Float32;
  speedLimitChanged @27 :Bool;
  tFollow @28 :Float32;
  themeUpdated @29 :Bool;
  togglesUpdated @30 :Bool;
  trackingLead @31 :Bool;
  unconfirmedSlcSpeedLimit @32 :Float32;
  vCruise @33 :Float32;
}

struct CustomReserved5 @0xa5cd762cd951a455 {
}

struct CustomReserved6 @0xf98d843bfd7004a3 {
}

struct CustomReserved7 @0xb86e6369214c01c8 {
}

struct CustomReserved8 @0xf416ec09499d9d19 {
}

struct CustomReserved9 @0xa1680744031fdb2d {
}
