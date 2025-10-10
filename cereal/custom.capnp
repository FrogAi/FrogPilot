using Cxx = import "./include/c++.capnp";
$Cxx.namespace("cereal");

using Car = import "car.capnp";

@0xb526ba661d550a59;

# custom.capnp: a home for empty structs reserved for custom forks
# These structs are guaranteed to remain reserved and empty in mainline
# cereal, so use these if you want custom events in your fork.

# DO rename the structs
# DON'T change the identifier (e.g. @0x81c2f05a394cf4af)

struct FrogPilotCarControl @0x81c2f05a394cf4af {
  hudControl @0 :HUDControl;

  struct HUDControl {
    audibleAlert @0 :AudibleAlert;

    enum AudibleAlert {
      none @0;

      engage @1;
      disengage @2;
      refuse @3;

      warningSoft @4;
      warningImmediate @5;

      prompt @6;
      promptRepeat @7;
      promptDistracted @8;

      # Random Events
      angry @9;
      continued @10;
      dejaVu @11;
      doc @12;
      fart @13;
      firefox @14;
      goat @15;
      hal9000 @16;
      mail @17;
      nessie @18;
      noice @19;
      startup @20;
      thisIsFine @21;
      uwu @22;
    }
  }
}

struct FrogPilotCarParams @0xaedffd8f31e7b55d {
  alternativeExperience @0 :Int16;
  canUsePedal @1 :Bool;
  canUseSDSU @2 :Bool;
  flags @3 :UInt32;
  isHDA2 @4 :Bool;
  openpilotLongitudinalControlDisabled @5 :Bool;
  safetyConfigs @6 :List(SafetyConfig);

  lateralTuning :union {
    pid @7 :Car.CarParams.LateralPIDTuning;
    torque @8 :Car.CarParams.LateralTorqueTuning;
  }

  struct SafetyConfig {
    safetyParam @0 :UInt16;
  }
}

struct FrogPilotCarState @0xf35cc4560bbf6ec2 {
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

struct FrogPilotDeviceState @0xda96579883444c35 {
  freeSpace @0 :Int16;
  usedSpace @1 :Int16;
}

struct FrogPilotModelDataV2 @0x80ae746ee2596b11 {
  turnDirection @0 :TurnDirection;

  enum TurnDirection {
    none @0;
    turnLeft @1;
    turnRight @2;
  }
}

struct FrogPilotOnroadEvent @0xa5cd762cd951a455 {
  name @0 :EventName;

  enable @1 :Bool;
  noEntry @2 :Bool;
  warning @3 :Bool;
  userDisable @4 :Bool;
  softDisable @5 :Bool;
  immediateDisable @6 :Bool;
  preEnable @7 :Bool;
  permanent @8 :Bool;
  overrideLateral @10 :Bool;
  overrideLongitudinal @9 :Bool;

  enum EventName {
    blockUser @0;
    customStartupAlert @1;
    forcingStop @2;
    goatSteerSaturated @3;
    greenLight @4;
    holidayActive @5;
    laneChangeBlockedLoud @6;
    leadDeparting @7;
    noLaneAvailable @8;
    openpilotCrashed @9;
    pedalInterceptorNoBrake @10;
    speedLimitChanged @11;
    trafficModeActive @12;
    trafficModeInactive @13;
    turningLeft @14;
    turningRight @15;

    # Random Events
    accel30 @16;
    accel35 @17;
    accel40 @18;
    dejaVuCurve @19;
    firefoxSteerSaturated @20;
    hal9000 @21;
    openpilotCrashedRandomEvent @22;
    thisIsFineSteerSaturated @23;
    toBeContinued @24;
    vCruise69 @25;
    yourFrogTriedToKillMe @26;
    youveGotMail @27;
  }
}

struct FrogPilotPlan @0xf98d843bfd7004a3 {
  accelerationJerk @0 :Float32;
  cscControllingSpeed @1 :Bool;
  cscSpeed @2 :Float32;
  cscTraining @3 :Bool;
  dangerJerk @4 :Float32;
  desiredFollowDistance @5 :Int64;
  experimentalMode @6 :Bool;
  forcingStop @7 :Bool;
  forcingStopLength @8 :Float32;
  frogpilotEvents @9 :List(FrogPilotOnroadEvent);
  increasedStoppedDistance @10 :Float32;
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
  speedLimitChanged @26 :Bool;
  tFollow @27 :Float32;
  themeUpdated @28 :Bool;
  togglesUpdated @29 :Bool;
  unconfirmedSlcSpeedLimit @30 :Float32;
  vCruise @31 :Float32;
  weatherDaytime @32 :Bool;
  weatherId @33 :Int16;
}

struct FrogPilotRadarState @0xb86e6369214c01c8 {
  leadLeft @0 :LeadData;
  leadRight @1 :LeadData;

  struct LeadData {
    dRel @0 :Float32;
    yRel @1 :Float32;
    vRel @2 :Float32;
    aRel @3 :Float32;
    vLead @4 :Float32;
    dPath @5 :Float32;
    vLat @6 :Float32;
    vLeadK @7 :Float32;
    aLeadK @8 :Float32;
    fcw @9 :Bool;
    status @10 :Bool;
    aLeadTau @11 :Float32;
    modelProb @12 :Float32;
    radar @13 :Bool;
    radarTrackId @14 :Int32 = -1;
  }
}

struct FrogPilotSelfdriveState @0xf416ec09499d9d19 {
  alertText1 @0 :Text;
  alertText2 @1 :Text;
  alertStatus @2 :AlertStatus;
  alertSize @3 :AlertSize;
  alertType @4 :Text;
  alertSound @5 :Car.CarControl.HUDControl.AudibleAlert;

  enum AlertStatus {
    normal @0;
    userPrompt @1;
    critical @2;
    frogpilot @3;
  }

  enum AlertSize {
    none @0;
    small @1;
    mid @2;
    full @3;
  }
}

struct CustomReserved9 @0xa1680744031fdb2d {
}

struct CustomReserved10 @0xcb9fd56c7057593a {
}

struct CustomReserved11 @0xc2243c65e0340384 {
}

struct CustomReserved12 @0x9ccdc8676701b412 {
}

struct CustomReserved13 @0xcd96dafb67a082d0 {
}

struct CustomReserved14 @0xb057204d7deadf3f {
}

struct CustomReserved15 @0xbd443b539493bc68 {
}

struct CustomReserved16 @0xfc6241ed8877b611 {
}

struct CustomReserved17 @0xa30662f84033036c {
}

struct CustomReserved18 @0xc86a3d38d13eb3ef {
}

struct CustomReserved19 @0xa4f1eb3323f5f582 {
}
