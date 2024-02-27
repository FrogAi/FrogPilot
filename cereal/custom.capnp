using Cxx = import "./include/c++.capnp";
$Cxx.namespace("cereal");

@0xb526ba661d550a59;

# custom.capnp: a home for empty structs reserved for custom forks
# These structs are guaranteed to remain reserved and empty in mainline
# cereal, so use these if you want custom events in your fork.

# you can rename the struct, but don't change the identifier
struct FrogPilotCarControl @0x81c2f05a394cf4af {
  alwaysOnLateral @0 :Bool;
  speedLimitChanged @1 :Bool;
}

struct FrogPilotDeviceState @0xaedffd8f31e7b55d {
  freeSpace @0 :Int16;
  usedSpace @1 :Int16;
}

struct FrogPilotNavigation @0xf35cc4560bbf6ec2 {
  approachingIntersection @0 :Bool;
  approachingTurn @1 :Bool;
}

struct FrogPilotPlan @0xda96579883444c35 {
  adjustedCruise @0 :Float64;
  conditionalExperimental @1 :Bool;
  desiredFollowDistance @2 :Int16;
  laneWidthLeft @3 :Float32;
  laneWidthRight @4 :Float32;
  redLight @5 :Bool;
  safeObstacleDistance @6 :Int16;
  safeObstacleDistanceStock @7 :Int16;
  slcOverridden @8 :Bool;
  slcOverriddenSpeed @9 :Float64;
  slcSpeedLimit @10 :Float64;
  slcSpeedLimitOffset @11 :Float32;
  stoppedEquivalenceFactor @12 :Int16;
  unconfirmedSlcSpeedLimit @13 :Float64;
  vtscControllingCurve @14 :Bool;
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
