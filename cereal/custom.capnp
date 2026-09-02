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
  hasDashboardSpeedLimit @4 :Bool;
  isHDA2 @5 :Bool;
  openpilotLongitudinalControlDisabled @6 :Bool;
  safetyConfigs @7 :List(SafetyConfig);

  struct SafetyConfig {
    safetyParam @0 :UInt16;
  }
}

struct FrogPilotCarState @0xf35cc4560bbf6ec2 {
  accelPressed @0 :Bool;
  alwaysOnLateralEnabled @1 :Bool;
  brakeLights @2 :Bool;
  dashboardSpeedLimit @3 :Float32;
  decelPressed @4 :Bool;
  distancePressed @5 :Bool;
  distanceLongPressed @6 :Bool;
  distanceVeryLongPressed @7 :Bool;
  ecoGear @8 :Bool;
  forceCoast @9 :Bool;
  pauseLateral @10 :Bool;
  pauseLongitudinal @11 :Bool;
  sportGear @12 :Bool;
  trafficModeEnabled @13 :Bool;
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
    nnffLoaded @9;
    openpilotCrashed @10;
    pedalInterceptorNoBrake @11;
    speedLimitChanged @12;
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

struct FrogPilotPlan @0xf98d843bfd7004a3 {
  accelerationJerk @0 :Float32;
  cscActive @1 :Bool;
  cscControllingSpeed @2 :Bool;
  cscMaxLateralAcceleration @3 :Float32;
  cscSpeed @4 :Float32;
  cscTraining @5 :Bool;
  dangerJerk @6 :Float32;
  desiredFollowDistance @7 :Int64;
  experimentalMode @8 :Bool;
  forcingStop @9 :Bool;
  forcingStopLength @10 :Float32;
  frogpilotEvents @11 :List(FrogPilotOnroadEvent);
  frogpilotToggles @12 :Text;
  increasedStoppedDistance @13 :Float32;
  laneWidthLeft @14 :Float32;
  laneWidthRight @15 :Float32;
  lateralCheck @16 :Bool;
  maxAcceleration @17 :Float32;
  minAcceleration @18 :Float32;
  redLight @19 :Bool;
  roadCurvature @20 :Float32;
  slcMapboxSpeedLimit @21 :Float32;
  slcMapboxWayId @22 :Int64;
  slcMapSpeedLimit @23 :Float32;
  slcNextSpeedLimit @24 :Float32;
  slcOverriddenSpeed @25 :Float32;
  slcSpeedLimit @26 :Float32;
  slcSpeedLimitOffset @27 :Float32;
  slcSpeedLimitSource @28 :Text;
  speedJerk @29 :Float32;
  speedLimitChanged @30 :Bool;
  tFollow @31 :Float32;
  themeUpdated @32 :Bool;
  trackingLead @33 :Bool;
  unconfirmedSlcSpeedLimit @34 :Float32;
  vCruise @35 :Float32;
  weatherDaytime @36 :Bool;
  weatherId @37 :Int16;
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

struct MapdDownloadLocationDetails @0xff889853e7b0987f {
  location @0 :Text;
  totalFiles @1 :UInt32;
  downloadedFiles @2 :UInt32;
}

struct MapdDownloadProgress @0xfaa35dcac85073a2 {
  active @0 :Bool;
  cancelled @1 :Bool;
  totalFiles @2 :UInt32;
  downloadedFiles @3 :UInt32;
  locations @4 :List(Text);
  locationDetails @5 :List(MapdDownloadLocationDetails);
}

struct MapdPathPoint @0xd6f78acca1bc3939 {
  latitude @0 :Float64;
  longitude @1 :Float64;
  curvature @2 :Float32;
  targetVelocity @3 :Float32;
}

struct MapdPosition @0xde9705979aca8339 {
  latitude @0 :Float64;
  longitude @1 :Float64;
}

struct MapdExtendedOut @0xa30662f84033036c {
  downloadProgress @0 :MapdDownloadProgress;
  settings @1 :Text;
  path @2 :List(MapdPathPoint);
  position @3 :MapdPosition;
  loopRateAverage @4 :Float32;
  loopRateMin @5 :Float32;
}

enum MapdInputType {
  download @0;
  reloadSettings @9;
  saveSettings @10;
  loadDefaultSettings @21;
  loadRecommendedSettings @22;
  loadPersistentSettings @26;
  cancelDownload @27;
  setJsonPathFloat @43;
  setJsonPathText @44;
  setJsonPathBool @45;
  acceptSpeedLimit @34;

  # DEPRECATED settings inputs
  setLogLevel @6;
  setLogSource @29;
  setLogJson @28;
  setTargetLateralAccel @1;
  setSpeedLimitOffset @2;
  setSpeedLimitControl @3;
  setMapCurveSpeedControl @4;
  setVisionCurveSpeedControl @5;
  setVisionCurveTargetLatA @7;
  setVisionCurveMinTargetV @8;
  setEnableSpeed @11;
  setVisionCurveUseEnableSpeed @12;
  setMapCurveUseEnableSpeed @13;
  setSpeedLimitUseEnableSpeed @14;
  setHoldLastSeenSpeedLimit @15;
  setTargetSpeedJerk @16;
  setTargetSpeedAccel @17;
  setTargetSpeedTimeOffset @18;
  setDefaultLaneWidth @19;
  setMapCurveTargetLatA @20;
  setSlowDownForNextSpeedLimit @23;
  setSpeedUpForNextSpeedLimit @24;
  setHoldSpeedLimitWhileChangingSetSpeed @25;
  setExternalSpeedLimitControl @30;
  setExternalSpeedLimit @31;
  setSpeedLimitPriority @32;
  setSpeedLimitChangeRequiresAccept @33;
  setPressGasToAcceptSpeedLimit @35;
  setAdjustSetSpeedToAcceptSpeedLimit @36;
  setAcceptSpeedLimitTimeout @37;
  setPressGasToOverrideSpeedLimit @38;
  setConditionalSpeedLimitControl @39;
  setShadowCarState @40;
  setShadowModelV2 @41;
  setShadowGpsLocation @42;
  setShadowGpsLocationExternal @46;
}

enum WaySelectionType {
  current @0;
  predicted @1;
  possible @2;
  extended @3;
  fail @4;
}

enum SpeedLimitOffsetType {
  static @0;
  percent @1;
}

struct MapdIn @0xc86a3d38d13eb3ef {
  type @0 :MapdInputType;
  float @1 :Float32;
  str @2 :Text;
  bool @3 :Bool;
  jsonPath @4 :Text;
}

enum RoadContext {
  freeway @0;
  city @1;
  unknown @2;
}

# WARNING: must be kept in perfect sync (names and values) with the
# HighwayClass enum in cereal/offline/offline.capnp — state.go casts directly
# between the two generated enum types.
# unknown either means the way's highway tag was not one of the listed values
# or the loaded map tiles predate this field.
enum HighwayClass {
  unknown @0;
  motorway @1;
  motorwayLink @2;
  trunk @3;
  trunkLink @4;
  primary @5;
  primaryLink @6;
  secondary @7;
  secondaryLink @8;
  tertiary @9;
  tertiaryLink @10;
  unclassified @11;
  residential @12;
  livingStreet @13;
}

struct MapdOut @0xa4f1eb3323f5f582 {
  wayName @0 :Text;
  wayRef @1 :Text;
  roadName @2 :Text;
  speedLimit @3 :Float32;
  nextSpeedLimit @4 :Float32;
  nextSpeedLimitDistance @5 :Float32;
  hazard @6 :Text;
  nextHazard @7 :Text;
  nextHazardDistance @8 :Float32;
  advisorySpeed @9 :Float32;
  nextAdvisorySpeed @10 :Float32;
  nextAdvisorySpeedDistance @11 :Float32;
  oneWay @12 :Bool;
  lanes @13 :UInt8;
  tileLoaded @14 :Bool;
  speedLimitSuggestedSpeed @15 :Float32;
  suggestedSpeed @16 :Float32;
  estimatedRoadWidth @17 :Float32;
  roadContext @18 :RoadContext;
  distanceFromWayCenter @19 :Float32;
  visionCurveSpeed @20 :Float32;
  mapCurveSpeed @21 :Float32;
  waySelectionType @22 :WaySelectionType;
  speedLimitAccepted @23 :Bool;
  highwayClass @24 :HighwayClass;
  wayId @25 :Int64;
  conditionalSpeedLimit @26 :Text;
}
