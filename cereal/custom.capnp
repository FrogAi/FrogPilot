using Cxx = import "./include/c++.capnp";
$Cxx.namespace("cereal");

using Car = import "car.capnp";

@0xb526ba661d550a59;

# custom.capnp: a home for empty structs reserved for custom forks
# These structs are guaranteed to remain reserved and empty in mainline
# cereal, so use these if you want custom events in your fork.

# you can rename the struct, but don't change the identifier
struct FrogPilotCarControl {
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

struct FrogPilotCarEvent @0x81c2f05a394cf4af {
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

  enum EventName @0xaedffd8f31e7b55d {
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
    torqueNNLoad @12;
    trafficModeActive @13;
    trafficModeInactive @14;
    turningLeft @15;
    turningRight @16;

    # Random Events
    accel30 @17;
    accel35 @18;
    accel40 @19;
    dejaVuCurve @20;
    firefoxSteerSaturated @21;
    hal9000 @22;
    openpilotCrashedRandomEvent @23;
    thisIsFineSteerSaturated @24;
    toBeContinued @25;
    vCruise69 @26;
    yourFrogTriedToKillMe @27;
    youveGotMail @28;
  }
}

struct FrogPilotCarParams @0xf35cc4560bbf6ec2 {
  canUsePedal @0 :Bool;
  canUseSDSU @1 :Bool;
  fpFlags @2 :UInt32;
  isHDA2 @3 :Bool;
  openpilotLongitudinalControlDisabled @4 :Bool;
  safetyConfigs @5 :List(SafetyConfig);

  lateralTuning :union {
    pid @6 :Car.CarParams.LateralPIDTuning;
    torque @7 :Car.CarParams.LateralTorqueTuning;
  }

  struct SafetyConfig {
    safetyParam @0 :UInt16;
  }
}

struct FrogPilotCarState @0xda96579883444c35 {
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

  struct ButtonEvent {
    enum Type {
      lkas @0;
    }
  }
}

struct FrogPilotControlsState @0x80ae746ee2596b11 {
  alertStatus @0 :AlertStatus;
  alertText1 @1 :Text;
  alertText2 @2 :Text;
  alertSize @3 :AlertSize;
  alertBlinkingRate @4 :Float32;
  alertType @5 :Text;
  alertSound @6 :Car.CarControl.HUDControl.AudibleAlert;

  enum AlertSize {
    none @0;    # don't display the alert
    small @1;   # small box
    mid @2;     # mid screen
    full @3;    # full screen
  }

  enum AlertStatus {
    normal @0;       # low priority alert for user's convenience
    userPrompt @1;   # mid priority alert that might require user intervention
    critical @2;     # high priority alert that needs immediate user intervention
    frogpilot @3;    # FrogPilot startup alert
  }
}

struct FrogPilotDeviceState @0xa5cd762cd951a455 {
  freeSpace @0 :Int16;
  usedSpace @1 :Int16;
}

struct FrogPilotModelDataV2 @0xf98d843bfd7004a3 {
  turnDirection @0 :TurnDirection;

  enum TurnDirection {
    none @0;
    turnLeft @1;
    turnRight @2;
  }
}

struct FrogPilotNavigation @0xf416ec09499d9d19 {
  approachingIntersection @0 :Bool;
  approachingTurn @1 :Bool;
  navigationSpeedLimit @2 :Float32;
}

struct FrogPilotPlan @0xa1680744031fdb2d {
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
  frogpilotEvents @10 :List(FrogPilotCarEvent);
  increasedStoppedDistance @11 :Float32;
  lateralCheck @12 :Bool;
  laneWidthLeft @13 :Float32;
  laneWidthRight @14 :Float32;
  maxAcceleration @15 :Float32;
  minAcceleration @16 :Float32;
  redLight @17 :Bool;
  roadCurvature @18 :Float32;
  slcMapSpeedLimit @19 :Float32;
  slcMapboxSpeedLimit @20 :Float32;
  slcNextSpeedLimit @21 :Float32;
  slcOverriddenSpeed @22 :Float32;
  slcSpeedLimit @23 :Float32;
  slcSpeedLimitOffset @24 :Float32;
  slcSpeedLimitSource @25 :Text;
  speedJerk @26 :Float32;
  speedJerkStock @27 :Float32;
  speedLimitChanged @28 :Bool;
  tFollow @29 :Float32;
  themeUpdated @30 :Bool;
  togglesUpdated @31 :Bool;
  trackingLead @32 :Bool;
  unconfirmedSlcSpeedLimit @33 :Float32;
  vCruise @34 :Float32;
}

struct FrogPilotRadarState @0xcb9fd56c7057593a {
  leadLeft @0 :LeadData;
  leadRight @1 :LeadData;

  struct LeadData {
    dRel @0 :Float32;
    yRel @1 :Float32;
    vRel @2 :Float32;
    aRel @3 :Float32;
    vLead @4 :Float32;
    dPath @6 :Float32;
    vLat @7 :Float32;
    vLeadK @8 :Float32;
    aLeadK @9 :Float32;
    fcw @10 :Bool;
    status @11 :Bool;
    aLeadTau @12 :Float32;
    modelProb @13 :Float32;
    radar @14 :Bool;
    radarTrackId @15 :Int32 = -1;

    aLeadDEPRECATED @5 :Float32;
  }
}
