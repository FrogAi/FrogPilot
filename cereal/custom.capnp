using Cxx = import "./include/c++.capnp";
$Cxx.namespace("cereal");

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
  dashcamOnly @3 :Bool;
  flags @4 :UInt32;
  hasDashboardSpeedLimit @5 :Bool;
  openpilotLongitudinalControlDisabled @6 :Bool;
  safetyConfigs @7 :List(SafetyConfig);

  struct SafetyConfig {
    safetyParam @0 :UInt16;
  }
}

struct FrogPilotCarState @0xf35cc4560bbf6ec2 {
  accelPressCount @0 :UInt64;
  alwaysOnLateralEnabled @1 :Bool;
  brakeLights @2 :Bool;
  dashboardSpeedLimit @3 :Float32;
  decelPressCount @4 :UInt64;
  distancePressed @5 :Bool;
  distanceLongPressed @6 :Bool;
  distanceVeryLongPressed @7 :Bool;
  ecoGear @8 :Bool;
  experimentalModePressCount @9 :UInt64;
  forceCoast @10 :Bool;
  lkasButtonPressCount @11 :UInt64;
  pauseLateral @12 :Bool;
  pauseLongitudinal @13 :Bool;
  pedalInterceptorNoBrake @14 :Bool;
  sportGear @15 :Bool;
  trafficModeEnabled @16 :Bool;
}

struct FrogPilotDeviceState @0xda96579883444c35 {
  forceOnroadClearedCount @0 :UInt64;
  freeSpace @1 :Int16;
  usedSpace @2 :Int16;
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
  overrideLongitudinal @9 :Bool;
  overrideLateral @10 :Bool;

  enum EventName {
    customStartupAlert @0;
    forcingStop @1;
    goatSteerSaturated @2;
    greenLight @3;
    holidayActive @4;
    laneChangeBlockedLoud @5;
    leadDeparting @6;
    nnffLoaded @7;
    noLaneAvailable @8;
    openpilotCrashed @9;
    pedalInterceptorNoBrake @10;
    pedalInterceptorNoBrakeNoEntry @11;
    recordingFailed @12;
    recordingSaved @13;
    recordingStarted @14;
    recordingStartFailed @15;
    replayFailed @16;
    replaySaved @17;
    speedLimitChanged @18;
    trafficModeActive @19;
    trafficModeInactive @20;
    turningLeft @21;
    turningRight @22;

    # Random Events
    accel30 @23;
    accel35 @24;
    accel40 @25;
    dejaVuCurve @26;
    firefoxSteerSaturated @27;
    hal9000 @28;
    openpilotCrashedRandomEvent @29;
    thisIsFineSteerSaturated @30;
    toBeContinued @31;
    vCruise69 @32;
    yourFrogTriedToKillMe @33;
    youveGotMail @34;
  }
}

struct FrogPilotPlan @0xf98d843bfd7004a3 {
  accelerationJerk @0 :Float32;
  ceStatus @1 :UInt8;
  cscActive @2 :Bool;
  cscControllingSpeed @3 :Bool;
  cscLateralAcceleration @4 :Float32;
  cscSpeed @5 :Float32;
  cscTraining @6 :Bool;
  dangerJerk @7 :Float32;
  desiredFollowDistance @8 :Int64;
  experimentalMode @9 :Bool;
  forcingStop @10 :Bool;
  forcingStopLength @11 :Float32;
  frogpilotEvents @12 :List(FrogPilotOnroadEvent);
  frogpilotToggles @13 :Text;
  gpsBearing @14 :Float32;
  increasedStoppedDistance @15 :Float32;
  laneWidthLeft @16 :Float32;
  laneWidthRight @17 :Float32;
  lateralCheck @18 :Bool;
  maxAcceleration @19 :Float32;
  minAcceleration @20 :Float32;
  redLight @21 :Bool;
  roadCurvature @22 :Float32;
  slcMapboxIsForward @23 :Bool;
  slcMapboxSpeedLimit @24 :Float32;
  slcMapboxWayId @25 :Int64;
  slcMapSpeedLimit @26 :Float32;
  slcNextSpeedLimit @27 :Float32;
  slcOverriddenSpeed @28 :Float32;
  slcSpeedLimit @29 :Float32;
  slcSpeedLimitOffset @30 :Float32;
  slcSpeedLimitSource @31 :Text;
  speedJerk @32 :Float32;
  speedLimitChanged @33 :Bool;
  tFollow @34 :Float32;
  themeUpdateCount @35 :UInt64;
  unconfirmedSlcSpeedLimit @36 :Float32;
  vCruise @37 :Float32;
  weatherDaytime @38 :Bool;
  weatherId @39 :Int16;
  wheelImageUpdateCount @40 :UInt64;
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
  alertSound @5 :FrogPilotCarControl.HUDControl.AudibleAlert;
  hasDisableEvents @6 :Bool;
  hasPriorityAlert @7 :Bool;

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

struct FrogPilotSignReading @0xa1680744031fdb2d {
  speedLimit @0 :Float32;
}

struct FrogPilotUIEvent @0xc2243c65e0340384 {
  union {
    distanceButtonPressed @0 :Bool;
    experimentalModePressed @1 :Void;
    screenRecorderEvent @2 :FrogPilotOnroadEvent.EventName;
    speedLimitAccepted @3 :Void;
  }
}

struct FrogPilotProcessState @0x9ccdc8676701b412 {
  downloadMapsRequestTime @0 :UInt64;
  downloadThemeRequestTime @1 :UInt64;
  downloadingMaps @2 :Bool;
  downloadingModels @3 :Bool;
  flashPandaRequestTime @4 :UInt64;
  flashedPanda @5 :Bool;
  flashingPanda @6 :Bool;
  issueReport @7 :Text;
  issueReportRequestTime @8 :UInt64;
  modelDownloadProgress @9 :Text;
  modelDownloadRequestTime @10 :UInt64;
  statsSavedCount @11 :UInt64;
  themeDownloadCount @12 :UInt32;
  themeDownloadFailedCount @13 :UInt32;
  themeDownloadProgress @14 :Text;
  themeDownloadSuccessCount @15 :UInt32;
}

struct FrogPilotUIRequest @0xcd96dafb67a082d0 {
  union {
    cancelMapsDownload @0 :Void;
    cancelModelDownload @1 :Void;
    cancelThemeDownload @2 :Void;
    downloadAllModels @3 :Void;
    downloadMaps @4 :Void;
    downloadModels @5 :List(Text);
    downloadTheme @6 :List(ThemeDownload);
    flashPanda @7 :Void;
    issueReport @8 :Text;
    testAlert @9 :Text;
    updateChecks @10 :Void;
    updateToggles @11 :Void;
  }

  struct ThemeDownload {
    component @0 :Text;
    theme @1 :Text;
  }
}

struct CustomReserved10 @0xcb9fd56c7057593a {
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
  isForward @27 :Bool;  # Travel follows the way's stored node order.
}
