#include "common/params.h"

#include <dirent.h>
#include <sys/file.h>

#include <algorithm>
#include <cassert>
#include <csignal>
#include <unordered_map>

#include "common/queue.h"
#include "common/swaglog.h"
#include "common/util.h"
#include "system/hardware/hw.h"

namespace {

volatile sig_atomic_t params_do_exit = 0;
void params_sig_handler(int signal) {
  params_do_exit = 1;
}

int fsync_dir(const std::string &path) {
  int result = -1;
  int fd = HANDLE_EINTR(open(path.c_str(), O_RDONLY, 0755));
  if (fd >= 0) {
    result = fsync(fd);
    close(fd);
  }
  return result;
}

bool create_params_path(const std::string &param_path, const std::string &key_path) {
  // Make sure params path exists
  if (!util::file_exists(param_path) && !util::create_directories(param_path, 0775)) {
    return false;
  }

  // See if the symlink exists, otherwise create it
  if (!util::file_exists(key_path)) {
    // 1) Create temp folder
    // 2) Symlink it to temp link
    // 3) Move symlink to <params>/d

    std::string tmp_path = param_path + "/.tmp_XXXXXX";
    // this should be OK since mkdtemp just replaces characters in place
    char *tmp_dir = mkdtemp((char *)tmp_path.c_str());
    if (tmp_dir == NULL) {
      return false;
    }

    std::string link_path = std::string(tmp_dir) + ".link";
    if (symlink(tmp_dir, link_path.c_str()) != 0) {
      return false;
    }

    // don't return false if it has been created by other
    if (rename(link_path.c_str(), key_path.c_str()) != 0 && errno != EEXIST) {
      return false;
    }
  }

  return true;
}

std::string ensure_params_path(const std::string &prefix, const std::string &path = {}) {
  std::string params_path = path.empty() ? Path::params() : path;
  if (!create_params_path(params_path, params_path + prefix)) {
    throw std::runtime_error(util::string_format(
        "Failed to ensure params path, errno=%d, path=%s, param_prefix=%s",
        errno, params_path.c_str(), prefix.c_str()));
  }
  return params_path;
}

class FileLock {
public:
  FileLock(const std::string &fn) {
    fd_ = HANDLE_EINTR(open(fn.c_str(), O_CREAT, 0775));
    if (fd_ < 0 || HANDLE_EINTR(flock(fd_, LOCK_EX)) < 0) {
      LOGE("Failed to lock file %s, errno=%d", fn.c_str(), errno);
    }
  }
  ~FileLock() { close(fd_); }

private:
  int fd_ = -1;
};

std::unordered_map<std::string, uint32_t> keys = {
    {"AccessToken", CLEAR_ON_MANAGER_START | DONT_LOG},
    {"AlwaysOnDM", PERSISTENT},
    {"ApiCache_Device", PERSISTENT},
    {"ApiCache_NavDestinations", PERSISTENT | DONT_LOG},
    {"AssistNowToken", PERSISTENT},
    {"AthenadPid", PERSISTENT},
    {"AthenadUploadQueue", PERSISTENT},
    {"AthenadRecentlyViewedRoutes", PERSISTENT},
    {"BootCount", PERSISTENT},
    {"CalibrationParams", PERSISTENT},
    {"CameraDebugExpGain", CLEAR_ON_MANAGER_START},
    {"CameraDebugExpTime", CLEAR_ON_MANAGER_START},
    {"CarBatteryCapacity", PERSISTENT},
    {"CarParams", CLEAR_ON_MANAGER_START | CLEAR_ON_ONROAD_TRANSITION},
    {"CarParamsCache", CLEAR_ON_MANAGER_START},
    {"CarParamsPersistent", PERSISTENT},
    {"CarParamsPrevRoute", PERSISTENT},
    {"CarVin", CLEAR_ON_MANAGER_START | CLEAR_ON_ONROAD_TRANSITION},
    {"CompletedTrainingVersion", PERSISTENT},
    {"ControlsReady", CLEAR_ON_MANAGER_START | CLEAR_ON_ONROAD_TRANSITION},
    {"CurrentBootlog", PERSISTENT},
    {"CurrentRoute", CLEAR_ON_MANAGER_START | CLEAR_ON_ONROAD_TRANSITION},
    {"DisableLogging", CLEAR_ON_MANAGER_START | CLEAR_ON_ONROAD_TRANSITION},
    {"DisablePowerDown", PERSISTENT},
    {"DisableUpdates", PERSISTENT},
    {"DisengageOnAccelerator", PERSISTENT},
    {"DmModelInitialized", CLEAR_ON_ONROAD_TRANSITION},
    {"DongleId", PERSISTENT},
    {"DoReboot", CLEAR_ON_MANAGER_START},
    {"DoShutdown", CLEAR_ON_MANAGER_START},
    {"DoUninstall", CLEAR_ON_MANAGER_START},
    {"ExperimentalLongitudinalEnabled", PERSISTENT},
    {"ExperimentalMode", PERSISTENT},
    {"ExperimentalModeConfirmed", PERSISTENT},
    {"FirmwareQueryDone", CLEAR_ON_MANAGER_START | CLEAR_ON_ONROAD_TRANSITION},
    {"ForcePowerDown", PERSISTENT},
    {"GitBranch", PERSISTENT},
    {"GitCommit", PERSISTENT},
    {"GitCommitDate", PERSISTENT},
    {"GitDiff", PERSISTENT},
    {"GithubSshKeys", PERSISTENT},
    {"GithubUsername", PERSISTENT},
    {"GitRemote", PERSISTENT},
    {"GsmApn", PERSISTENT},
    {"GsmMetered", PERSISTENT},
    {"GsmRoaming", PERSISTENT},
    {"HardwareSerial", PERSISTENT},
    {"HasAcceptedTerms", PERSISTENT},
    {"IMEI", PERSISTENT | DONT_LOG},
    {"InstallDate", PERSISTENT},
    {"IsDriverViewEnabled", CLEAR_ON_MANAGER_START},
    {"IsEngaged", PERSISTENT},
    {"IsLdwEnabled", PERSISTENT},
    {"IsMetric", PERSISTENT},
    {"IsOffroad", CLEAR_ON_MANAGER_START},
    {"IsOnroad", PERSISTENT},
    {"IsRhdDetected", PERSISTENT},
    {"IsReleaseBranch", CLEAR_ON_MANAGER_START},
    {"IsTakingSnapshot", CLEAR_ON_MANAGER_START},
    {"IsTestedBranch", CLEAR_ON_MANAGER_START},
    {"JoystickDebugMode", CLEAR_ON_MANAGER_START | CLEAR_ON_OFFROAD_TRANSITION},
    {"LanguageSetting", PERSISTENT},
    {"LastAthenaPingTime", CLEAR_ON_MANAGER_START},
    {"LastGPSPosition", PERSISTENT},
    {"LastManagerExitReason", CLEAR_ON_MANAGER_START},
    {"LastOffroadStatusPacket", CLEAR_ON_MANAGER_START | CLEAR_ON_OFFROAD_TRANSITION},
    {"LastPowerDropDetected", CLEAR_ON_MANAGER_START},
    {"LastUpdateException", CLEAR_ON_MANAGER_START},
    {"LastUpdateTime", PERSISTENT},
    {"LiveDelay", PERSISTENT},
    {"LiveParameters", PERSISTENT},
    {"LiveTorqueParameters", PERSISTENT | DONT_LOG},
    {"LongitudinalPersonality", PERSISTENT},
    {"NavDestination", CLEAR_ON_MANAGER_START | CLEAR_ON_OFFROAD_TRANSITION | DONT_LOG},
    {"NavDestinationWaypoints", CLEAR_ON_MANAGER_START | CLEAR_ON_OFFROAD_TRANSITION},
    {"NavPastDestinations", PERSISTENT | DONT_LOG},
    {"NavSettingLeftSide", PERSISTENT},
    {"NavSettingTime24h", PERSISTENT},
    {"NetworkMetered", PERSISTENT},
    {"ObdMultiplexingChanged", CLEAR_ON_MANAGER_START | CLEAR_ON_ONROAD_TRANSITION},
    {"ObdMultiplexingEnabled", CLEAR_ON_MANAGER_START | CLEAR_ON_ONROAD_TRANSITION},
    {"Offroad_BadNvme", CLEAR_ON_MANAGER_START},
    {"Offroad_CarUnrecognized", CLEAR_ON_MANAGER_START | CLEAR_ON_ONROAD_TRANSITION},
    {"Offroad_ConnectivityNeeded", CLEAR_ON_MANAGER_START},
    {"Offroad_ConnectivityNeededPrompt", CLEAR_ON_MANAGER_START},
    {"Offroad_IsTakingSnapshot", CLEAR_ON_MANAGER_START},
    {"Offroad_NeosUpdate", CLEAR_ON_MANAGER_START},
    {"Offroad_NoFirmware", CLEAR_ON_MANAGER_START | CLEAR_ON_ONROAD_TRANSITION},
    {"Offroad_Recalibration", CLEAR_ON_MANAGER_START | CLEAR_ON_ONROAD_TRANSITION},
    {"Offroad_StorageMissing", CLEAR_ON_MANAGER_START},
    {"Offroad_TemperatureTooHigh", CLEAR_ON_MANAGER_START},
    {"Offroad_UnofficialHardware", CLEAR_ON_MANAGER_START},
    {"Offroad_UnsupportedDevice", CLEAR_ON_MANAGER_START},
    {"Offroad_UpdateFailed", CLEAR_ON_MANAGER_START},
    {"OpenpilotEnabledToggle", PERSISTENT},
    {"PandaHeartbeatLost", CLEAR_ON_MANAGER_START | CLEAR_ON_OFFROAD_TRANSITION},
    {"PandaSomResetTriggered", CLEAR_ON_MANAGER_START | CLEAR_ON_OFFROAD_TRANSITION},
    {"PandaSignatures", CLEAR_ON_MANAGER_START},
    {"PrimeType", PERSISTENT},
    {"RecordFront", PERSISTENT},
    {"RecordFrontLock", PERSISTENT},  // for the internal fleet
    {"ReplayControlsState", CLEAR_ON_MANAGER_START | CLEAR_ON_ONROAD_TRANSITION},
    {"RouteCount", PERSISTENT},
    {"SecOCKey", PERSISTENT | DONT_LOG},
    {"SnoozeUpdate", CLEAR_ON_MANAGER_START | CLEAR_ON_OFFROAD_TRANSITION},
    {"SshEnabled", PERSISTENT},
    {"TermsVersion", PERSISTENT},
    {"Timezone", PERSISTENT},
    {"TrainingVersion", PERSISTENT},
    {"UbloxAvailable", PERSISTENT},
    {"UpdateAvailable", CLEAR_ON_MANAGER_START | CLEAR_ON_ONROAD_TRANSITION},
    {"UpdateFailedCount", CLEAR_ON_MANAGER_START},
    {"UpdaterAvailableBranches", PERSISTENT},
    {"UpdaterCurrentDescription", CLEAR_ON_MANAGER_START},
    {"UpdaterCurrentReleaseNotes", CLEAR_ON_MANAGER_START},
    {"UpdaterFetchAvailable", CLEAR_ON_MANAGER_START},
    {"UpdaterNewDescription", CLEAR_ON_MANAGER_START},
    {"UpdaterNewReleaseNotes", CLEAR_ON_MANAGER_START},
    {"UpdaterState", CLEAR_ON_MANAGER_START},
    {"UpdaterTargetBranch", CLEAR_ON_MANAGER_START},
    {"UpdaterLastFetchTime", PERSISTENT},
    {"Version", PERSISTENT},

    // FrogPilot parameters
    {"AccelerationPath", PERSISTENT},
    {"AccelerationProfile", PERSISTENT},
    {"AdjacentLeadsUI", PERSISTENT},
    {"AdjacentPath", PERSISTENT},
    {"AdjacentPathMetrics", PERSISTENT},
    {"AdvancedCustomUI", PERSISTENT},
    {"AdvancedLateralTune", PERSISTENT},
    {"AdvancedLongitudinalTune", PERSISTENT},
    {"AggressiveFollow", PERSISTENT},
    {"AggressiveJerkAcceleration", PERSISTENT},
    {"AggressiveJerkDanger", PERSISTENT},
    {"AggressiveJerkDeceleration", PERSISTENT},
    {"AggressiveJerkSpeed", PERSISTENT},
    {"AggressiveJerkSpeedDecrease", PERSISTENT},
    {"AggressivePersonalityProfile", PERSISTENT},
    {"AlertVolumeControl", PERSISTENT},
    {"AlwaysOnLateral", PERSISTENT},
    {"AlwaysOnLateralLKASButton", PERSISTENT},
    {"AMapKey1", PERSISTENT | DONT_LOG},
    {"ApiCache_DriveStats", PERSISTENT},
    {"ApiCache_DriveStatsKonik", PERSISTENT},
    {"AutomaticallyDownloadModels", PERSISTENT},
    {"AutomaticUpdates", PERSISTENT},
    {"AvailableModelNames", PERSISTENT},
    {"AvailableModels", PERSISTENT},
    {"BackupSettings", CLEAR_ON_MANAGER_START},
    {"BigMap", PERSISTENT},
    {"BlacklistedModels", PERSISTENT},
    {"BlindSpotMetrics", PERSISTENT},
    {"BlindSpotPath", PERSISTENT},
    {"BorderMetrics", PERSISTENT},
    {"CalibratedLateralAcceleration", PERSISTENT},
    {"CalibrationProgress", PERSISTENT},
    {"CameraView", PERSISTENT},
    {"CarMake", PERSISTENT},
    {"CarModel", PERSISTENT},
    {"CarModelName", PERSISTENT},
    {"CECurves", PERSISTENT},
    {"CECurvesLead", PERSISTENT},
    {"CELead", PERSISTENT},
    {"CEModelStopTime", PERSISTENT},
    {"CENavigation", PERSISTENT},
    {"CENavigationIntersections", PERSISTENT},
    {"CENavigationLead", PERSISTENT},
    {"CENavigationTurns", PERSISTENT},
    {"CESignalLaneDetection", PERSISTENT},
    {"CESignalSpeed", PERSISTENT},
    {"CESlowerLead", PERSISTENT},
    {"CESpeed", PERSISTENT},
    {"CESpeedLead", PERSISTENT},
    {"CEStopLights", PERSISTENT},
    {"CEStoppedLead", PERSISTENT},
    {"ClusterOffset", PERSISTENT},
    {"ColorScheme", PERSISTENT},
    {"Compass", PERSISTENT},
    {"ConditionalExperimental", PERSISTENT},
    {"CSLCEnabled", PERSISTENT},
    {"CurvatureData", PERSISTENT | DONT_LOG},
    {"CurveSpeedController", PERSISTENT},
    {"CurveSpeedProfile", PERSISTENT},
    {"CustomAlerts", PERSISTENT},
    {"CustomColors", PERSISTENT},
    {"CustomCruise", PERSISTENT},
    {"CustomCruiseLong", PERSISTENT},
    {"CustomDistanceIcons", PERSISTENT},
    {"CustomIcons", PERSISTENT},
    {"CustomPersonalities", PERSISTENT},
    {"CustomSignals", PERSISTENT},
    {"CustomSounds", PERSISTENT},
    {"CustomThemes", PERSISTENT},
    {"CustomUI", PERSISTENT},
    {"DebugMode", CLEAR_ON_OFFROAD_TRANSITION},
    {"DecelerationProfile", PERSISTENT},
    {"DesiredCurvatureHeadModels", PERSISTENT},
    {"DeveloperMetrics", PERSISTENT},
    {"DeveloperSidebar", PERSISTENT},
    {"DeveloperSidebarMetric1", PERSISTENT},
    {"DeveloperSidebarMetric2", PERSISTENT},
    {"DeveloperSidebarMetric3", PERSISTENT},
    {"DeveloperSidebarMetric4", PERSISTENT},
    {"DeveloperSidebarMetric5", PERSISTENT},
    {"DeveloperSidebarMetric6", PERSISTENT},
    {"DeveloperSidebarMetric7", PERSISTENT},
    {"DeveloperUI", PERSISTENT},
    {"DeveloperWidgets", PERSISTENT},
    {"DeviceManagement", PERSISTENT},
    {"DeviceShutdown", PERSISTENT},
    {"DisableOnroadUploads", PERSISTENT},
    {"DisableOpenpilotLongitudinal", PERSISTENT},
    {"DiscordUsername", PERSISTENT | DONT_LOG},
    {"DisengageVolume", PERSISTENT},
    {"DistanceButtonControl", PERSISTENT},
    {"DistanceIconPack", PERSISTENT},
    {"DoToggleReset", PERSISTENT},
    {"DoToggleResetStock", PERSISTENT},
    {"DownloadableColors", PERSISTENT},
    {"DownloadableDistanceIcons", PERSISTENT},
    {"DownloadableIcons", PERSISTENT},
    {"DownloadableSignals", PERSISTENT},
    {"DownloadableSounds", PERSISTENT},
    {"DownloadableWheels", PERSISTENT},
    {"DriverCamera", PERSISTENT},
    {"DynamicPathWidth", PERSISTENT},
    {"DynamicPedalsOnUI", PERSISTENT},
    {"EngageVolume", PERSISTENT},
    {"Fahrenheit", PERSISTENT},
    {"FavoriteDestinations", PERSISTENT | DONT_LOG},
    {"ForceAutoTune", PERSISTENT},
    {"ForceAutoTuneOff", PERSISTENT},
    {"ForceFingerprint", PERSISTENT},
    {"ForceMPHDashboard", PERSISTENT},
    {"ForceOffroad", CLEAR_ON_MANAGER_START},
    {"ForceOnroad", CLEAR_ON_MANAGER_START},
    {"ForceStops", PERSISTENT},
    {"ForceTorqueController", PERSISTENT},
    {"FPSCounter", PERSISTENT},
    {"FrogPilotApiToken", PERSISTENT | DONT_LOG},
    {"FrogPilotCarParams", CLEAR_ON_MANAGER_START | CLEAR_ON_ONROAD_TRANSITION},
    {"FrogPilotCarParamsPersistent", PERSISTENT},
    {"FrogPilotDongleId", PERSISTENT | DONT_LOG},
    {"FrogPilotDrives", PERSISTENT},
    {"FrogPilotKilometers", PERSISTENT},
    {"FrogPilotMinutes", PERSISTENT},
    {"FrogPilotRegistration", PERSISTENT | DONT_LOG},
    {"FrogPilotStats", PERSISTENT | DONT_LOG},
    {"FrogPilotTelemetry", PERSISTENT},
    {"FrogPilotTelemetryConfirmed", PERSISTENT},
    {"FrogPilotToggles", CLEAR_ON_MANAGER_START},
    {"FrogPilotTuningLevels", CLEAR_ON_MANAGER_START},
    {"FrogsGoMoosTweak", PERSISTENT},
    {"FullMap", PERSISTENT},
    {"GoatScream", PERSISTENT},
    {"GreenLightAlert", PERSISTENT},
    {"HideAlerts", PERSISTENT},
    {"HideLeadMarker", PERSISTENT},
    {"HideMap", PERSISTENT},
    {"HideMapIcon", PERSISTENT},
    {"HideMaxSpeed", PERSISTENT},
    {"HideSpeed", PERSISTENT},
    {"HideSpeedLimit", PERSISTENT},
    {"HigherBitrate", PERSISTENT},
    {"HolidayThemes", PERSISTENT},
    {"HondaAltTune", PERSISTENT},
    {"HondaLowSpeedPedal", PERSISTENT},
    {"HondaMaxBrake", PERSISTENT},
    {"HumanAcceleration", PERSISTENT},
    {"HumanFollowing", PERSISTENT},
    {"HumanLaneChanges", PERSISTENT},
    {"IconPack", PERSISTENT},
    {"IncreasedStoppedDistance", PERSISTENT},
    {"IncreasedStoppedDistanceLowVisibility", PERSISTENT},
    {"IncreasedStoppedDistanceRain", PERSISTENT},
    {"IncreasedStoppedDistanceRainStorm", PERSISTENT},
    {"IncreasedStoppedDistanceSnow", PERSISTENT},
    {"IncreaseFollowingLowVisibility", PERSISTENT},
    {"IncreaseFollowingRain", PERSISTENT},
    {"IncreaseFollowingRainStorm", PERSISTENT},
    {"IncreaseFollowingSnow", PERSISTENT},
    {"IncreaseThermalLimits", PERSISTENT},
    {"InstantReplay", PERSISTENT},
    {"KonikDongleId", PERSISTENT | DONT_LOG},
    {"KonikMinutes", PERSISTENT},
    {"LaneChanges", PERSISTENT},
    {"LaneChangeTime", PERSISTENT},
    {"LaneDetectionWidth", PERSISTENT},
    {"LaneLinesWidth", PERSISTENT},
    {"LastMapsUpdate", PERSISTENT},
    {"LateralTune", PERSISTENT},
    {"LeadDepartingAlert", PERSISTENT},
    {"LeadDetectionThreshold", PERSISTENT},
    {"LeadInfo", PERSISTENT},
    {"LKASButtonControl", PERSISTENT},
    {"LockDoors", PERSISTENT},
    {"LockDoorsTimer", PERSISTENT},
    {"LongDistanceButtonControl", PERSISTENT},
    {"LongitudinalActuatorDelay", PERSISTENT},
    {"LongitudinalActuatorDelayStock", PERSISTENT},
    {"LongitudinalTune", PERSISTENT},
    {"LongPitch", PERSISTENT},
    {"LoudBlindspotAlert", PERSISTENT},
    {"LowVoltageShutdown", PERSISTENT},
    {"MapAcceleration", PERSISTENT},
    {"MapboxPublicKey", PERSISTENT | DONT_LOG},
    {"MapBoxRequests", PERSISTENT | DONT_LOG},
    {"MapboxSecretKey", PERSISTENT | DONT_LOG},
    {"MapDeceleration", PERSISTENT},
    {"MapdLogLevel", CLEAR_ON_MANAGER_START},
    {"MapGears", PERSISTENT},
    {"MapMatchedWay", CLEAR_ON_MANAGER_START},
    {"MapSpeedLimit", CLEAR_ON_MANAGER_START},
    {"MapsSelected", PERSISTENT},
    {"MapStyle", PERSISTENT},
    {"MaxDesiredAcceleration", PERSISTENT},
    {"MaxLateralAcceleration", PERSISTENT},
    {"MinimumBackupSize", PERSISTENT},
    {"MinimumLaneChangeSpeed", PERSISTENT},
    {"Model", PERSISTENT},
    {"ModelDrivesAndScores", PERSISTENT},
    {"ModelLatSmoothSeconds", PERSISTENT},
    {"ModelRandomizer", PERSISTENT},
    {"ModelUI", PERSISTENT},
    {"ModelVersions", PERSISTENT},
    {"NavigationUI", PERSISTENT},
    {"NewLongAPI", PERSISTENT},
    {"NNFF", PERSISTENT},
    {"NNFFLite", PERSISTENT},
    {"NNFFModelName", CLEAR_ON_OFFROAD_TRANSITION},
    {"NoLogging", PERSISTENT},
    {"NoUploads", PERSISTENT},
    {"NudgelessLaneChange", PERSISTENT},
    {"NumericalTemp", PERSISTENT},
    {"Offset1", PERSISTENT},
    {"Offset2", PERSISTENT},
    {"Offset3", PERSISTENT},
    {"Offset4", PERSISTENT},
    {"Offset5", PERSISTENT},
    {"Offset6", PERSISTENT},
    {"Offset7", PERSISTENT},
    {"OneLaneChange", PERSISTENT},
    {"OnroadDistanceButton", PERSISTENT},
    {"openpilotMinutes", PERSISTENT},
    {"OSMDownloadBounds", PERSISTENT},
    {"OSMDownloadLocations", PERSISTENT},
    {"OSMDownloadProgress", CLEAR_ON_MANAGER_START},
    {"PathEdgeWidth", PERSISTENT},
    {"PathWidth", PERSISTENT},
    {"PauseAOLOnBrake", PERSISTENT},
    {"PauseLateralOnSignal", PERSISTENT},
    {"PauseLateralSpeed", PERSISTENT},
    {"PedalsOnUI", PERSISTENT},
    {"PersonalizeOpenpilot", PERSISTENT},
    {"PreferredSchedule", PERSISTENT},
    {"PreviousSpeedLimit", PERSISTENT},
    {"PromptDistractedVolume", PERSISTENT},
    {"PromptVolume", PERSISTENT},
    {"QOLLateral", PERSISTENT},
    {"QOLLongitudinal", PERSISTENT},
    {"QOLVisuals", PERSISTENT},
    {"RadarTracksUI", PERSISTENT},
    {"RainbowPath", PERSISTENT},
    {"RandomEvents", PERSISTENT},
    {"RandomizedModel", PERSISTENT},
    {"RandomThemes", PERSISTENT},
    {"RandomThemesHolidays", PERSISTENT},
    {"ReduceAccelerationLowVisibility", PERSISTENT},
    {"ReduceAccelerationRain", PERSISTENT},
    {"ReduceAccelerationRainStorm", PERSISTENT},
    {"ReduceAccelerationSnow", PERSISTENT},
    {"ReduceLateralAccelerationLowVisibility", PERSISTENT},
    {"ReduceLateralAccelerationRain", PERSISTENT},
    {"ReduceLateralAccelerationRainStorm", PERSISTENT},
    {"ReduceLateralAccelerationSnow", PERSISTENT},
    {"RefuseVolume", PERSISTENT},
    {"RelaxedFollow", PERSISTENT},
    {"RelaxedJerkAcceleration", PERSISTENT},
    {"RelaxedJerkDanger", PERSISTENT},
    {"RelaxedJerkDeceleration", PERSISTENT},
    {"RelaxedJerkSpeed", PERSISTENT},
    {"RelaxedJerkSpeedDecrease", PERSISTENT},
    {"RelaxedPersonalityProfile", PERSISTENT},
    {"RestartMapd", CLEAR_ON_MANAGER_START},
    {"RestoreSettings", CLEAR_ON_MANAGER_START},
    {"ReverseCruise", PERSISTENT},
    {"RoadEdgesWidth", PERSISTENT},
    {"RoadName", CLEAR_ON_MANAGER_START},
    {"RoadNameUI", PERSISTENT},
    {"RotatingWheel", PERSISTENT},
    {"ScreenBrightness", PERSISTENT},
    {"ScreenBrightnessOnroad", PERSISTENT},
    {"ScreenManagement", PERSISTENT},
    {"ScreenRecorder", PERSISTENT},
    {"ScreenTimeout", PERSISTENT},
    {"ScreenTimeoutOnroad", PERSISTENT},
    {"SearchInput", PERSISTENT},
    {"SecOCKeys", PERSISTENT | DONT_LOG},
    {"SetSpeedLimit", PERSISTENT},
    {"SetSpeedOffset", PERSISTENT},
    {"ShowCEMStatus", PERSISTENT},
    {"ShowCPU", PERSISTENT},
    {"ShowCSCStatus", PERSISTENT},
    {"ShowGPU", PERSISTENT},
    {"ShowIP", PERSISTENT},
    {"ShowMemoryUsage", PERSISTENT},
    {"ShownToggleDescriptions", PERSISTENT},
    {"ShowSLCOffset", PERSISTENT},
    {"ShowSpeedLimits", PERSISTENT},
    {"ShowSteering", PERSISTENT},
    {"ShowStoppingPoint", PERSISTENT},
    {"ShowStoppingPointMetrics", PERSISTENT},
    {"ShowStorageLeft", PERSISTENT},
    {"ShowStorageUsed", PERSISTENT},
    {"Sidebar", PERSISTENT},
    {"SignalAnimation", PERSISTENT},
    {"SignalMetrics", PERSISTENT},
    {"SLCConfirmation", PERSISTENT},
    {"SLCConfirmationHigher", PERSISTENT},
    {"SLCConfirmationLower", PERSISTENT},
    {"SLCFallback", PERSISTENT},
    {"SLCLookaheadHigher", PERSISTENT},
    {"SLCLookaheadLower", PERSISTENT},
    {"SLCMapboxFiller", PERSISTENT},
    {"SLCOverride", PERSISTENT},
    {"SLCPriority1", PERSISTENT},
    {"SLCPriority2", PERSISTENT},
    {"SLCPriority3", PERSISTENT},
    {"SLCPriority4", PERSISTENT},
    {"SNGHack", PERSISTENT},
    {"SoundPack", PERSISTENT},
    {"SpeedLimitChangedAlert", PERSISTENT},
    {"SpeedLimitController", PERSISTENT},
    {"SpeedLimitFiller", PERSISTENT},
    {"SpeedLimitFillerShareData", PERSISTENT},
    {"SpeedLimitFillerUploadedHash", PERSISTENT | DONT_LOG},
    {"SpeedLimitSources", PERSISTENT},
    {"StandardFollow", PERSISTENT},
    {"StandardJerkAcceleration", PERSISTENT},
    {"StandardJerkDanger", PERSISTENT},
    {"StandardJerkDeceleration", PERSISTENT},
    {"StandardJerkSpeed", PERSISTENT},
    {"StandardJerkSpeedDecrease", PERSISTENT},
    {"StandardPersonalityProfile", PERSISTENT},
    {"StandbyMode", PERSISTENT},
    {"StartAccel", PERSISTENT},
    {"StartAccelStock", PERSISTENT},
    {"StartupMessageBottom", PERSISTENT},
    {"StartupMessageTop", PERSISTENT},
    {"StaticPedalsOnUI", PERSISTENT},
    {"SteerDelay", PERSISTENT},
    {"SteerDelayStock", PERSISTENT},
    {"SteerFriction", PERSISTENT},
    {"SteerFrictionStock", PERSISTENT},
    {"SteerKP", PERSISTENT},
    {"SteerKPStock", PERSISTENT},
    {"SteerLatAccel", PERSISTENT},
    {"SteerLatAccelStock", PERSISTENT},
    {"SteerRatio", PERSISTENT},
    {"SteerRatioStock", PERSISTENT},
    {"StockDongleId", PERSISTENT | DONT_LOG},
    {"StopAccel", PERSISTENT},
    {"StopAccelStock", PERSISTENT},
    {"StoppedTimer", PERSISTENT},
    {"StoppingDecelRate", PERSISTENT},
    {"StoppingDecelRateStock", PERSISTENT},
    {"SubaruSNG", PERSISTENT},
    {"TacoTune", PERSISTENT},
    {"TacoTuneHacks", PERSISTENT},
    {"TetheringEnabled", PERSISTENT},
    {"ThemesDownloaded", PERSISTENT},
    {"ThePondPairedBrowsers", PERSISTENT | DONT_LOG},
    {"ThePondPairing", PERSISTENT},
    {"ThePondPairingCode", PERSISTENT | DONT_LOG},
    {"ToyotaDoors", PERSISTENT},
    {"ToyotaDSUBypass", PERSISTENT},
    {"TuningLevel", PERSISTENT},
    {"TuningLevelConfirmed", PERSISTENT},
    {"TurnDesires", PERSISTENT},
    {"UnlockDoors", PERSISTENT},
    {"Updated", PERSISTENT},
    {"UpdaterCopySeconds", PERSISTENT},
    {"UpdaterDownloadProgress", CLEAR_ON_MANAGER_START},
    {"UseKonikServer", PERSISTENT},
    {"UseSI", PERSISTENT},
    {"UseVienna", PERSISTENT},
    {"VEgoStarting", PERSISTENT},
    {"VEgoStartingStock", PERSISTENT},
    {"VEgoStopping", PERSISTENT},
    {"VEgoStoppingStock", PERSISTENT},
    {"VeryLongDistanceButtonControl", PERSISTENT},
    {"VoltSNG", PERSISTENT},
    {"WarningImmediateVolume", PERSISTENT},
    {"WarningSoftVolume", PERSISTENT},
    {"WeatherPresets", PERSISTENT},
    {"WeatherToken", PERSISTENT | DONT_LOG},
    {"WheelIcon", PERSISTENT},
    {"WheelSpeed", PERSISTENT},
};

} // namespace


Params::Params(const std::string &path) {
  params_prefix = "/" + util::getenv("OPENPILOT_PREFIX", "d");
  params_path = ensure_params_path(params_prefix, path);

  // FrogPilot variables
  cache_path = "/cache/params" + params_prefix + "/";
}

Params::~Params() {
  if (future.valid()) {
    future.wait();
  }
  assert(queue.empty());
}

std::vector<std::string> Params::allKeys() const {
  std::vector<std::string> ret;
  for (auto &p : keys) {
    ret.push_back(p.first);
  }
  return ret;
}

bool Params::checkKey(const std::string &key) {
  return keys.find(key) != keys.end();
}

ParamKeyType Params::getKeyType(const std::string &key) {
  return static_cast<ParamKeyType>(keys[key]);
}

int Params::put(const char* key, const char* value, size_t value_size) {
  // Information about safely and atomically writing a file: https://lwn.net/Articles/457667/
  // 1) Create temp file
  // 2) Write data to temp file
  // 3) fsync() the temp file
  // 4) rename the temp file to the real name
  // 5) fsync() the containing directory
  std::string tmp_path = params_path + "/.tmp_value_XXXXXX";
  int tmp_fd = mkstemp((char*)tmp_path.c_str());
  if (tmp_fd < 0) return -1;

  int result = -1;
  do {
    // Write value to temp.
    ssize_t bytes_written = HANDLE_EINTR(write(tmp_fd, value, value_size));
    if (bytes_written < 0 || (size_t)bytes_written != value_size) {
      result = -20;
      break;
    }

    // fsync to force persist the changes.
    if ((result = fsync(tmp_fd)) < 0) break;

    FileLock file_lock(params_path + "/.lock");

    // Move temp into place.
    if ((result = rename(tmp_path.c_str(), getParamPath(key).c_str())) < 0) break;

    // fsync parent directory
    result = fsync_dir(getParamPath());
  } while (false);

  close(tmp_fd);
  if (result != 0) {
    ::unlink(tmp_path.c_str());
  }
  return result;
}

int Params::remove(const std::string &key) {
  FileLock file_lock(params_path + "/.lock");
  int result = unlink(getParamPath(key).c_str());

  // FrogPilot variables
  unlink((cache_path + key).c_str());

  if (result != 0) {
    return result;
  }
  return fsync_dir(getParamPath());
}

std::string Params::get(const std::string &key, bool block) {
  if (!block) {
    return util::read_file(getParamPath(key));
  } else {
    // blocking read until successful
    params_do_exit = 0;
    void (*prev_handler_sigint)(int) = std::signal(SIGINT, params_sig_handler);
    void (*prev_handler_sigterm)(int) = std::signal(SIGTERM, params_sig_handler);

    std::string value;
    while (!params_do_exit) {
      if (value = util::read_file(getParamPath(key)); !value.empty()) {
        break;
      }
      util::sleep_for(100);  // 0.1 s
    }

    std::signal(SIGINT, prev_handler_sigint);
    std::signal(SIGTERM, prev_handler_sigterm);
    return value;
  }
}

std::map<std::string, std::string> Params::readAll() {
  FileLock file_lock(params_path + "/.lock");
  return util::read_files_in_dir(getParamPath());
}

void Params::clearAll(ParamKeyType key_type) {
  FileLock file_lock(params_path + "/.lock");

  // 1) delete params of key_type
  // 2) delete files that are not defined in the keys.
  if (DIR *d = opendir(getParamPath().c_str())) {
    struct dirent *de = NULL;
    while ((de = readdir(d))) {
      if (de->d_type != DT_DIR) {
        auto it = keys.find(de->d_name);
        if (it == keys.end() || (it->second & key_type)) {
          unlink(getParamPath(de->d_name).c_str());

          // FrogPilot variables
          unlink((cache_path + de->d_name).c_str());
        }
      }
    }
    closedir(d);
  }

  fsync_dir(getParamPath());
}

void Params::putNonBlocking(const std::string &key, const std::string &val) {
   queue.push(std::make_pair(key, val));
  // start thread on demand
  if (!future.valid() || future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
    future = std::async(std::launch::async, &Params::asyncWriteThread, this);
  }
}

void Params::asyncWriteThread() {
  // TODO: write the latest one if a key has multiple values in the queue.
  std::pair<std::string, std::string> p;
  while (queue.try_pop(p, 0)) {
    // Params::put is Thread-Safe
    put(p.first, p.second);
  }
}
