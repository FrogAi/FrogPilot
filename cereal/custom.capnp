using Cxx = import "./include/c++.capnp";
$Cxx.namespace("cereal");

using Car = import "car.capnp";

@0xb526ba661d550a59;

# custom.capnp: a home for empty structs reserved for custom forks
# These structs are guaranteed to remain reserved and empty in mainline
# cereal, so use these if you want custom events in your fork.

# you can rename the struct, but don't change the identifier
struct FrogPilotCarState @0x81c2f05a394cf4af {
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
  trafficMode @14 :Bool;
}

struct FrogPilotDeviceState @0xaedffd8f31e7b55d {
  freeSpace @0 :Int16;
  usedSpace @1 :Int16;
}

struct FrogPilotNavigation @0xf35cc4560bbf6ec2 {
  approachingIntersection @0 :Bool;
  approachingTurn @1 :Bool;
  navigationSpeedLimit @2 :Float32;
}

struct FrogPilotPlan @0xda96579883444c35 {
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
  slcOverridden @17 :Bool;
  slcOverriddenSpeed @18 :Float32;
  slcSpeedLimit @19 :Float32;
  slcSpeedLimitOffset @20 :Float32;
  slcSpeedLimitSource @21 :Text;
  speedJerk @22 :Float32;
  speedJerkStock @23 :Float32;
  speedLimitChanged @24 :Bool;
  tFollow @25 :Float32;
  togglesUpdated @26 :Bool;
  unconfirmedSlcSpeedLimit @27 :Float32;
  upcomingSLCSpeedLimit @28 :Float32;
  vCruise @29 :Float32;
  vtscControllingCurve @30 :Bool;
  vtscSpeed @31 :Float32;
}

struct CustomReserved4 @0x80ae746ee2596b11 {
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
