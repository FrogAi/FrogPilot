using Cxx = import "./include/c++.capnp";
$Cxx.namespace("cereal");

@0xb526ba661d550a59;

# custom.capnp: a home for empty structs reserved for custom forks
# These structs are guaranteed to remain reserved and empty in mainline
# cereal, so use these if you want custom events in your fork.

# you can rename the struct, but don't change the identifier
struct FrogPilotCarControl {
  struct HUDControl {
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

struct FrogPilotAccountState @0x8fa22efa18fa36e4 {
  link @0 :Text;
  linkRequestTime @1 :UInt64;
  url @2 :Text;
  urlRequestTime @3 :UInt64;
}

struct FrogPilotCarEvent @0x81c2f05a394cf4af {
  name @0 :EventName;

  # event types
  enable @1 :Bool;
  noEntry @2 :Bool;
  warning @3 :Bool;   # alerts presented only when  enabled or soft disabling
  userDisable @4 :Bool;
  softDisable @5 :Bool;
  immediateDisable @6 :Bool;
  preEnable @7 :Bool;
  permanent @8 :Bool; # alerts presented regardless of openpilot state
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
    nnffLoaded @8;
    noLaneAvailable @9;
    openpilotCrashed @10;
    pedalInterceptorNoBrake @11;
    pedalInterceptorNoBrakeNoEntry @12;
    recordingFailed @13;
    recordingSaved @14;
    recordingStarted @15;
    recordingStartFailed @16;
    replayFailed @17;
    replaySaved @18;
    speedLimitChanged @19;
    trafficModeActive @20;
    trafficModeInactive @21;
    turningLeft @22;
    turningRight @23;

    # Random Events
    accel30 @24;
    accel35 @25;
    accel40 @26;
    dejaVuCurve @27;
    firefoxSteerSaturated @28;
    hal9000 @29;
    openpilotCrashedRandomEvent @30;
    thisIsFineSteerSaturated @31;
    toBeContinued @32;
    vCruise69 @33;
    yourFrogTriedToKillMe @34;
    youveGotMail @35;
  }
}

struct FrogPilotCarParams @0xf35cc4560bbf6ec2 {
  canUsePedal @0 :Bool;
  canUseSDSU @1 :Bool;
  fpFlags @2 :UInt32;
  hasDashboardSpeedLimit @3 :Bool;
  openpilotLongitudinalControlDisabled @4 :Bool;
  safetyConfigs @5 :List(SafetyConfig);

  struct SafetyConfig {
    safetyParam @0 :UInt16;
  }
}

struct FrogPilotCarState @0xda96579883444c35 {
  accelPressCount @0 :UInt64;
  alwaysOnLateralAllowed @1 :Bool;
  alwaysOnLateralEnabled @2 :Bool;
  brakeLights @3 :Bool;
  dashboardSpeedLimit @4 :Float32;
  decelPressCount @5 :UInt64;
  distancePressed @6 :Bool;
  distanceLongPressed @7 :Bool;
  distanceVeryLongPressed @8 :Bool;
  doorLockStatus @9 :DoorLockStatus;
  ecoGear @10 :Bool;
  experimentalModePressCount @11 :UInt64;
  forceCoast @12 :Bool;
  parked @13 :Bool;
  pauseLateral @14 :Bool;
  pauseLongitudinal @15 :Bool;
  pedalInterceptorNoBrake @16 :Bool;
  sportGear @17 :Bool;
  trafficModeEnabled @18 :Bool;
  vEgo @19 :Float32;

  enum DoorLockStatus {
    unavailable @0;
    locked @1;
    unlocked @2;
  }
}

struct FrogPilotControlsState @0x80ae746ee2596b11 {
  alertStatus @0 :AlertStatus;
  alertText1 @1 :Text;
  alertText2 @2 :Text;
  alertSize @3 :AlertSize;
  alertBlinkingRate @4 :Float32;
  alertType @5 :Text;
  alertSound @6 :FrogPilotCarControl.HUDControl.AudibleAlert;
  hasDisableEvents @7 :Bool;

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
  forceOnroadClearedCount @0 :UInt64;
  freeSpace @1 :Int16;
  usedSpace @2 :Int16;
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
  ceStatus @1 :UInt8;
  cscActive @2 :Bool;
  cscControllingSpeed @3 :Bool;
  cscMaxLateralAcceleration @4 :Float32;
  cscSpeed @5 :Float32;
  cscTraining @6 :Bool;
  dangerJerk @7 :Float32;
  desiredFollowDistance @8 :Int64;
  experimentalMode @9 :Bool;
  forcingStop @10 :Bool;
  forcingStopLength @11 :Float32;
  frogpilotEvents @12 :List(FrogPilotCarEvent);
  gpsBearing @13 :Float32;
  increasedStoppedDistance @14 :Float32;
  laneWidthLeft @15 :Float32;
  laneWidthRight @16 :Float32;
  lateralCheck @17 :Bool;
  maxAcceleration @18 :Float32;
  minAcceleration @19 :Float32;
  redLight @20 :Bool;
  roadCurvature @21 :Float32;
  slcMapboxSpeedLimit @22 :Float32;
  slcMapSpeedLimit @23 :Float32;
  slcNextSpeedLimit @24 :Float32;
  slcOverriddenSpeed @25 :Float32;
  slcSpeedLimit @26 :Float32;
  slcSpeedLimitOffset @27 :Float32;
  slcSpeedLimitSource @28 :Text;
  speedJerk @29 :Float32;
  speedLimitChanged @30 :Bool;
  tFollow @31 :Float32;
  themeUpdateCount @32 :UInt64;
  togglesUpdateTime @33 :UInt64;
  unconfirmedSlcSpeedLimit @34 :Float32;
  vCruise @35 :Float32;
  weatherDaytime @36 :Bool;
  weatherId @37 :Int16;
  wheelImageUpdateCount @38 :UInt64;
}

struct FrogPilotProcessState @0x9ccdc8676701b412 {
  doorCommand @0 :Text;
  doorCommandRequestTime @1 :UInt64;
  downloadingMaps @2 :Bool;
  downloadingModels @3 :Bool;
  downloadMapsRequestTime @4 :UInt64;
  downloadThemeRequestTime @5 :UInt64;
  flashedPanda @6 :Bool;
  flashingPanda @7 :Bool;
  flashPandaRequestTime @8 :UInt64;
  issueReport @9 :Text;
  issueReportRequestTime @10 :UInt64;
  modelDownloadProgress @11 :Text;
  modelDownloadRequestTime @12 :UInt64;
  settingsBackup @13 :Text;
  settingsBackupRequestTime @14 :UInt64;
  statsSavedCount @15 :UInt64;
  themeDownloadCount @16 :UInt32;
  themeDownloadFailedCount @17 :UInt32;
  themeDownloadProgress @18 :Text;
  themeDownloadSuccessCount @19 :UInt32;
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

struct FrogPilotSignReading @0xe3b5a7c9d1f20486 {
  frameId @0 :Float32;
  mode @1 :Float32;
  nextFrameId @2 :Float32;
  nextMode @3 :Float32;
  signAgreement @4 :Float32;
  signSpeedLimit @5 :Float32;
  speedLimit @6 :Float32;
}

struct FrogPilotUIEvent @0xc2243c65e0340384 {
  union {
    distanceButtonPressed @0 :Bool;
    experimentalModePressed @1 :Void;
    screenRecorderEvent @2 :FrogPilotCarEvent.EventName;
    speedLimitAccepted @3 :Void;
  }
}

struct FrogPilotUIRequest @0xcd96dafb67a082d0 {
  union {
    accountLink @0 :Void;
    accountUrl @1 :Text;
    cancelMapsDownload @2 :Void;
    cancelModelDownload @3 :Void;
    cancelThemeDownload @4 :Void;
    createSettingsBackup @5 :Text;
    downloadAllModels @6 :Void;
    downloadMaps @7 :Void;
    downloadModels @8 :List(Text);
    downloadTheme @9 :List(ThemeDownload);
    flashPanda @10 :Void;
    issueReport @11 :Text;
    lockDoors @12 :Bool;
    refreshTheme @13 :Void;
    restoreSettingsBackup @14 :Text;
    testAlert @15 :Text;
    unlinkAccount @16 :Void;
    updateChecks @17 :Void;
    updateToggles @18 :Void;
  }

  testAlertVolume @19 :Float32;

  struct ThemeDownload {
    component @0 :Text;
    theme @1 :Text;
  }
}

struct ThePondRequest @0xb86e6369214c01c8 {
  request @0 :FrogPilotUIRequest;
}
