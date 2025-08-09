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
  dangerJerk @2 :Float32;
  desiredFollowDistance @3 :Int64;
  experimentalMode @4 :Bool;
  forcingStop @5 :Bool;
  forcingStopLength @6 :Float32;
  frogpilotEvents @7 :List(Car.CarEvent);
  lateralCheck @8 :Bool;
  laneWidthLeft @9 :Float32;
  laneWidthRight @10 :Float32;
  maxAcceleration @11 :Float32;
  minAcceleration @12 :Float32;
  mtscSpeed @13 :Float32;
  redLight @14 :Bool;
  roadCurvature @15 :Float32;
  slcMapSpeedLimit @16 :Float32;
  slcMapboxSpeedLimit @17 :Float32;
  slcNextSpeedLimit @18 :Float32;
  slcOverriddenSpeed @19 :Float32;
  slcSpeedLimit @20 :Float32;
  slcSpeedLimitOffset @21 :Float32;
  slcSpeedLimitSource @22 :Text;
  speedJerk @23 :Float32;
  speedJerkStock @24 :Float32;
  speedLimitChanged @25 :Bool;
  tFollow @26 :Float32;
  themeUpdated @27 :Bool;
  togglesUpdated @28 :Bool;
  trackingLead @29 :Bool;
  unconfirmedSlcSpeedLimit @30 :Float32;
  vCruise @31 :Float32;
  vtscControllingCurve @32 :Bool;
  vtscSpeed @33 :Float32;
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
