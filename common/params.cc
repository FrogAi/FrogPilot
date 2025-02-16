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
    {"ApiCache_NavDestinations", PERSISTENT},
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
    {"ExperimentalLongitudinalEnabled", PERSISTENT | DEVELOPMENT_ONLY},
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
    {"IMEI", PERSISTENT},
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
    {"LiveParameters", PERSISTENT},
    {"LiveTorqueParameters", PERSISTENT | DONT_LOG},
    {"LongitudinalPersonality", PERSISTENT},
    {"NavDestination", CLEAR_ON_MANAGER_START | CLEAR_ON_OFFROAD_TRANSITION},
    {"NavDestinationWaypoints", CLEAR_ON_MANAGER_START | CLEAR_ON_OFFROAD_TRANSITION},
    {"NavPastDestinations", PERSISTENT},
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
    {"AccelerationPath", PERSISTENT | FROGPILOT_VISUALS},
    {"AccelerationProfile", PERSISTENT | FROGPILOT_CONTROLS},
    {"AdjacentLeadsUI", PERSISTENT | FROGPILOT_CONTROLS},
    {"AdjacentPath", PERSISTENT | FROGPILOT_VISUALS},
    {"AdjacentPathMetrics", PERSISTENT | FROGPILOT_VISUALS},
    {"AdvancedCustomUI", PERSISTENT | FROGPILOT_VISUALS},
    {"AdvancedLateralTune", PERSISTENT | FROGPILOT_CONTROLS},
    {"AggressiveFollow", PERSISTENT | FROGPILOT_CONTROLS},
    {"AggressiveJerkAcceleration", PERSISTENT | FROGPILOT_CONTROLS},
    {"AggressiveJerkDanger", PERSISTENT | FROGPILOT_CONTROLS},
    {"AggressiveJerkDeceleration", PERSISTENT | FROGPILOT_CONTROLS},
    {"AggressiveJerkSpeed", PERSISTENT | FROGPILOT_CONTROLS},
    {"AggressiveJerkSpeedDecrease", PERSISTENT | FROGPILOT_CONTROLS},
    {"AggressivePersonalityProfile", PERSISTENT | FROGPILOT_CONTROLS},
    {"AlertVolumeControl", PERSISTENT | FROGPILOT_VISUALS},
    {"AlwaysOnLateral", PERSISTENT | FROGPILOT_CONTROLS},
    {"AlwaysOnLateralLKAS", PERSISTENT | FROGPILOT_CONTROLS},
    {"AlwaysOnLateralMain", PERSISTENT | FROGPILOT_CONTROLS},
    {"AMapKey1", PERSISTENT},
    {"AMapKey2", PERSISTENT},
    {"ApiCache_DriveStats", PERSISTENT},
    {"AutomaticallyUpdateModels", PERSISTENT | FROGPILOT_CONTROLS},
    {"AutomaticUpdates", PERSISTENT | FROGPILOT_OTHER},
    {"AvailableModels", PERSISTENT},
    {"AvailableModelNames", PERSISTENT},
    {"BigMap", PERSISTENT | FROGPILOT_VISUALS},
    {"BlacklistedModels", PERSISTENT | FROGPILOT_CONTROLS},
    {"BlindSpotMetrics", PERSISTENT | FROGPILOT_VISUALS},
    {"BlindSpotPath", PERSISTENT | FROGPILOT_VISUALS},
    {"BorderMetrics", PERSISTENT | FROGPILOT_VISUALS},
    {"BrakeSignal", PERSISTENT | FROGPILOT_VISUALS},
    {"CameraView", PERSISTENT | FROGPILOT_VISUALS},
    {"CancelModelDownload", CLEAR_ON_MANAGER_START},
    {"CancelThemeDownload", CLEAR_ON_MANAGER_START},
    {"CarMake", PERSISTENT},
    {"CarModel", PERSISTENT},
    {"CarModelName", PERSISTENT},
    {"CECurves", PERSISTENT | FROGPILOT_CONTROLS},
    {"CECurvesLead", PERSISTENT | FROGPILOT_CONTROLS},
    {"CELead", PERSISTENT | FROGPILOT_CONTROLS},
    {"CEModelStopTime", PERSISTENT | FROGPILOT_CONTROLS},
    {"CENavigation", PERSISTENT | FROGPILOT_CONTROLS},
    {"CENavigationIntersections", PERSISTENT | FROGPILOT_CONTROLS},
    {"CENavigationLead", PERSISTENT | FROGPILOT_CONTROLS},
    {"CENavigationTurns", PERSISTENT | FROGPILOT_CONTROLS},
    {"CertifiedHerbalistDrives", PERSISTENT | FROGPILOT_CONTROLS},
    {"CertifiedHerbalistScore", PERSISTENT | FROGPILOT_CONTROLS},
    {"CESignalSpeed", PERSISTENT | FROGPILOT_CONTROLS},
    {"CESignalLaneDetection", PERSISTENT | FROGPILOT_CONTROLS},
    {"CESlowerLead", PERSISTENT | FROGPILOT_CONTROLS},
    {"CESpeed", PERSISTENT | FROGPILOT_CONTROLS},
    {"CESpeedLead", PERSISTENT | FROGPILOT_CONTROLS},
    {"CEStatus", CLEAR_ON_OFFROAD_TRANSITION},
    {"CEStoppedLead", PERSISTENT | FROGPILOT_CONTROLS},
    {"ClusterOffset", PERSISTENT | FROGPILOT_VEHICLES},
    {"ColorToDownload", CLEAR_ON_MANAGER_START},
    {"Compass", PERSISTENT | FROGPILOT_VISUALS},
    {"ConditionalExperimental", PERSISTENT | FROGPILOT_CONTROLS},
    {"CurveSensitivity", PERSISTENT | FROGPILOT_CONTROLS},
    {"CurveSpeedControl", PERSISTENT | FROGPILOT_CONTROLS},
    {"CustomAlerts", PERSISTENT | FROGPILOT_VISUALS},
    {"CustomColors", PERSISTENT | FROGPILOT_VISUALS},
    {"CustomCruise", PERSISTENT | FROGPILOT_CONTROLS},
    {"CustomCruiseLong", PERSISTENT | FROGPILOT_CONTROLS},
    {"CustomDistanceIcons", PERSISTENT | FROGPILOT_CONTROLS},
    {"CustomIcons", PERSISTENT | FROGPILOT_VISUALS},
    {"CustomPersonalities", PERSISTENT | FROGPILOT_CONTROLS},
    {"CustomSignals", PERSISTENT | FROGPILOT_VISUALS},
    {"CustomSounds", PERSISTENT | FROGPILOT_VISUALS},
    {"CustomUI", PERSISTENT | FROGPILOT_VISUALS},
    {"DecelerationProfile", PERSISTENT | FROGPILOT_CONTROLS},
    {"DeveloperUI", PERSISTENT | FROGPILOT_VISUALS},
    {"DeviceManagement", PERSISTENT | FROGPILOT_CONTROLS},
    {"DeviceShutdown", PERSISTENT | FROGPILOT_CONTROLS},
    {"DisableOnroadUploads", PERSISTENT | FROGPILOT_CONTROLS},
    {"DisableOpenpilotLongitudinal", PERSISTENT | FROGPILOT_VEHICLES},
    {"DiscordUsername", PERSISTENT},
    {"DissolvedOxygenDrives", PERSISTENT | FROGPILOT_CONTROLS},
    {"DissolvedOxygenScore", PERSISTENT | FROGPILOT_CONTROLS},
    {"DistanceIconToDownload", CLEAR_ON_MANAGER_START},
    {"DisengageVolume", PERSISTENT | FROGPILOT_VISUALS},
    {"DoToggleReset", PERSISTENT},
    {"DownloadableColors", PERSISTENT},
    {"DownloadableDistanceIcons", PERSISTENT},
    {"DownloadableIcons", PERSISTENT},
    {"DownloadableSignals", PERSISTENT},
    {"DownloadableSounds", PERSISTENT},
    {"DownloadableWheels", PERSISTENT},
    {"DownloadAllModels", CLEAR_ON_MANAGER_START},
    {"DragonRiderDrives", PERSISTENT | FROGPILOT_CONTROLS},
    {"DragonRiderScore", PERSISTENT | FROGPILOT_CONTROLS},
    {"DriverCamera", PERSISTENT | FROGPILOT_VISUALS},
    {"DuckAmigoDrives", PERSISTENT | FROGPILOT_CONTROLS},
    {"DuckAmigoScore", PERSISTENT | FROGPILOT_CONTROLS},
    {"DynamicPathWidth", PERSISTENT | FROGPILOT_VISUALS},
    {"DynamicPedalsOnUI", PERSISTENT | FROGPILOT_VISUALS},
    {"EngageVolume", PERSISTENT | FROGPILOT_VISUALS},
    {"ExperimentalGMTune", PERSISTENT | FROGPILOT_VEHICLES},
    {"ExperimentalModeActivation", PERSISTENT | FROGPILOT_CONTROLS},
    {"ExperimentalModels", PERSISTENT},
    {"ExperimentalModeViaDistance", PERSISTENT | FROGPILOT_CONTROLS},
    {"ExperimentalModeViaLKAS", PERSISTENT | FROGPILOT_CONTROLS},
    {"ExperimentalModeViaTap", PERSISTENT | FROGPILOT_CONTROLS},
    {"Fahrenheit", PERSISTENT | FROGPILOT_VISUALS},
    {"FlashPanda", CLEAR_ON_MANAGER_START},
    {"ForceAutoTune", PERSISTENT | FROGPILOT_CONTROLS},
    {"ForceAutoTuneOff", PERSISTENT | FROGPILOT_CONTROLS},
    {"ForceFingerprint", PERSISTENT | FROGPILOT_VEHICLES},
    {"ForceMPHDashboard", PERSISTENT | FROGPILOT_CONTROLS},
    {"ForceOffroad", CLEAR_ON_MANAGER_START},
    {"ForceOnroad", CLEAR_ON_MANAGER_START},
    {"ForceStandstill", PERSISTENT | FROGPILOT_CONTROLS},
    {"ForceStops", PERSISTENT | FROGPILOT_CONTROLS},
    {"FPSCounter", PERSISTENT | FROGPILOT_VISUALS},
    {"FrogPilotDrives", PERSISTENT | FROGPILOT_TRACKING},
    {"FrogPilotKilometers", PERSISTENT | FROGPILOT_TRACKING},
    {"FrogPilotMinutes", PERSISTENT | FROGPILOT_TRACKING},
    {"FrogPilotToggles", CLEAR_ON_MANAGER_START},
    {"FrogPilotTogglesUpdated", CLEAR_ON_MANAGER_START},
    {"FrogPilotTuningLevels", CLEAR_ON_MANAGER_START},
    {"FrogsGoMoosTweak", PERSISTENT | FROGPILOT_VEHICLES},
    {"FullMap", PERSISTENT | FROGPILOT_VISUALS},
    {"GasRegenCmd", PERSISTENT | FROGPILOT_VEHICLES},
    {"GMapKey", PERSISTENT},
    {"GoatScream", PERSISTENT | FROGPILOT_VISUALS},
    {"GreenLightAlert", PERSISTENT | FROGPILOT_VISUALS},
    {"HideAlerts", PERSISTENT | FROGPILOT_VISUALS},
    {"HideCSCUI", PERSISTENT | FROGPILOT_CONTROLS},
    {"HideLeadMarker", PERSISTENT | FROGPILOT_VISUALS},
    {"HideMapIcon", PERSISTENT | FROGPILOT_VISUALS},
    {"HideMaxSpeed", PERSISTENT | FROGPILOT_VISUALS},
    {"HideSpeed", PERSISTENT | FROGPILOT_VISUALS},
    {"HideSpeedLimit", PERSISTENT | FROGPILOT_VISUALS},
    {"HolidayThemes", PERSISTENT | FROGPILOT_VISUALS},
    {"HumanAcceleration", PERSISTENT | FROGPILOT_CONTROLS},
    {"HumanFollowing", PERSISTENT | FROGPILOT_CONTROLS},
    {"HKGBraking", PERSISTENT},
    {"HKGtuning", PERSISTENT | FROGPILOT_VEHICLES},
    {"IconToDownload", CLEAR_ON_MANAGER_START},
    {"IncreasedStoppedDistance", PERSISTENT | FROGPILOT_CONTROLS},
    {"IncreaseThermalLimits", PERSISTENT | FROGPILOT_CONTROLS},
    {"IssueReported", CLEAR_ON_MANAGER_START},
    {"JerkInfo", PERSISTENT | FROGPILOT_VISUALS},
    {"LaneChangeCustomizations", PERSISTENT | FROGPILOT_CONTROLS},
    {"LaneChangeTime", PERSISTENT | FROGPILOT_CONTROLS},
    {"LaneDetectionWidth", PERSISTENT | FROGPILOT_CONTROLS},
    {"LaneLinesWidth", PERSISTENT | FROGPILOT_VISUALS},
    {"LastMapsUpdate", PERSISTENT | FROGPILOT_OTHER},
    {"LateralMetrics", PERSISTENT | FROGPILOT_VISUALS},
    {"LateralTune", PERSISTENT | FROGPILOT_CONTROLS},
    {"LeadDepartingAlert", PERSISTENT | FROGPILOT_VISUALS},
    {"LeadDetectionThreshold", PERSISTENT | FROGPILOT_CONTROLS},
    {"LeadInfo", PERSISTENT | FROGPILOT_VISUALS},
    {"LockDoors", PERSISTENT | FROGPILOT_VEHICLES},
    {"LockDoorsTimer", PERSISTENT | FROGPILOT_VEHICLES},
    {"LongitudinalMetrics", PERSISTENT | FROGPILOT_VISUALS},
    {"LongitudinalTune", PERSISTENT | FROGPILOT_CONTROLS},
    {"LongPitch", PERSISTENT | FROGPILOT_VEHICLES},
    {"LosAngelesDrives", PERSISTENT | FROGPILOT_CONTROLS},
    {"LosAngelesScore", PERSISTENT | FROGPILOT_CONTROLS},
    {"LoudBlindspotAlert", PERSISTENT | FROGPILOT_VISUALS},
    {"LowVoltageShutdown", PERSISTENT | FROGPILOT_CONTROLS},
    {"ManualUpdateInitiated", CLEAR_ON_MANAGER_START},
    {"MapAcceleration", PERSISTENT | FROGPILOT_CONTROLS},
    {"MapboxPublicKey", PERSISTENT},
    {"MapboxSecretKey", PERSISTENT},
    {"MapDeceleration", PERSISTENT | FROGPILOT_CONTROLS},
    {"MapGears", PERSISTENT | FROGPILOT_CONTROLS},
    {"MapsSelected", PERSISTENT | FROGPILOT_OTHER},
    {"MapSpeedLimit", CLEAR_ON_MANAGER_START},
    {"MapStyle", PERSISTENT | FROGPILOT_VISUALS},
    {"MapTargetVelocities", CLEAR_ON_MANAGER_START},
    {"MapTurnControl", PERSISTENT | FROGPILOT_CONTROLS},
    {"MaxDesiredAcceleration", PERSISTENT | FROGPILOT_CONTROLS},
    {"MinimumBackupSize", PERSISTENT},
    {"MinimumLaneChangeSpeed", PERSISTENT | FROGPILOT_CONTROLS},
    {"Model", PERSISTENT | FROGPILOT_CONTROLS},
    {"ModelDrivesAndScores", PERSISTENT | FROGPILOT_CONTROLS},
    {"ModelDownloadProgress", CLEAR_ON_MANAGER_START},
    {"ModelRandomizer", PERSISTENT | FROGPILOT_CONTROLS},
    {"ModelToDownload", CLEAR_ON_MANAGER_START},
    {"ModelUI", PERSISTENT | FROGPILOT_VISUALS},
    {"ModelVersion", PERSISTENT | FROGPILOT_CONTROLS},
    {"ModelVersions", PERSISTENT | FROGPILOT_CONTROLS},
    {"MTSCCurvatureCheck", PERSISTENT | FROGPILOT_CONTROLS},
    {"NavigationUI", PERSISTENT | FROGPILOT_VISUALS},
    {"NextMapSpeedLimit", CLEAR_ON_MANAGER_START},
    {"NewLongAPI", PERSISTENT | FROGPILOT_VEHICLES},
    {"NewLongAPIGM", PERSISTENT | FROGPILOT_VEHICLES},
    {"NNFF", PERSISTENT | FROGPILOT_CONTROLS},
    {"NNFFLite", PERSISTENT | FROGPILOT_CONTROLS},
    {"NNFFModelName", CLEAR_ON_OFFROAD_TRANSITION},
    {"NoLogging", PERSISTENT | FROGPILOT_CONTROLS},
    {"NorthDakotaDrives", PERSISTENT | FROGPILOT_CONTROLS},
    {"NorthDakotaScore", PERSISTENT | FROGPILOT_CONTROLS},
    {"NotreDameDrives", PERSISTENT | FROGPILOT_CONTROLS},
    {"NotreDameScore", PERSISTENT | FROGPILOT_CONTROLS},
    {"NoUploads", PERSISTENT | FROGPILOT_CONTROLS},
    {"NudgelessLaneChange", PERSISTENT | FROGPILOT_CONTROLS},
    {"NumericalTemp", PERSISTENT | FROGPILOT_VISUALS},
    {"OfflineMode", PERSISTENT | FROGPILOT_CONTROLS},
    {"Offset1", PERSISTENT | FROGPILOT_CONTROLS},
    {"Offset2", PERSISTENT | FROGPILOT_CONTROLS},
    {"Offset3", PERSISTENT | FROGPILOT_CONTROLS},
    {"Offset4", PERSISTENT | FROGPILOT_CONTROLS},
    {"OneLaneChange", PERSISTENT | FROGPILOT_CONTROLS},
    {"OnroadDistanceButton", PERSISTENT | FROGPILOT_CONTROLS},
    {"OnroadDistanceButtonPressed", CLEAR_ON_MANAGER_START},
    {"openpilotMinutes", PERSISTENT},
    {"OSMDownloadBounds", PERSISTENT},
    {"OSMDownloadLocations", PERSISTENT},
    {"OSMDownloadProgress", CLEAR_ON_MANAGER_START},
    {"PathEdgeWidth", PERSISTENT | FROGPILOT_VISUALS},
    {"PathWidth", PERSISTENT | FROGPILOT_VISUALS},
    {"PauseAOLOnBrake", PERSISTENT | FROGPILOT_CONTROLS},
    {"PauseLateralOnSignal", PERSISTENT | FROGPILOT_CONTROLS},
    {"PauseLateralSpeed", PERSISTENT | FROGPILOT_CONTROLS},
    {"PedalsOnUI", PERSISTENT | FROGPILOT_VISUALS},
    {"PersonalizeOpenpilot", PERSISTENT | FROGPILOT_VISUALS},
    {"PreferredSchedule", PERSISTENT | FROGPILOT_OTHER},
    {"PreviousSpeedLimit", PERSISTENT},
    {"PromptDistractedVolume", PERSISTENT | FROGPILOT_VISUALS},
    {"PromptVolume", PERSISTENT | FROGPILOT_VISUALS},
    {"QOLLateral", PERSISTENT | FROGPILOT_CONTROLS},
    {"QOLLongitudinal", PERSISTENT | FROGPILOT_CONTROLS},
    {"QOLVisuals", PERSISTENT | FROGPILOT_VISUALS},
    {"RadicalTurtleDrives", PERSISTENT | FROGPILOT_CONTROLS},
    {"RadicalTurtleScore", PERSISTENT | FROGPILOT_CONTROLS},
    {"RainbowPath", PERSISTENT | FROGPILOT_VISUALS},
    {"RandomEvents", PERSISTENT | FROGPILOT_VISUALS},
    {"RecertifiedHerbalistDrives", PERSISTENT | FROGPILOT_CONTROLS},
    {"RecertifiedHerbalistScore", PERSISTENT | FROGPILOT_CONTROLS},
    {"RefuseVolume", PERSISTENT | FROGPILOT_VISUALS},
    {"RelaxedFollow", PERSISTENT | FROGPILOT_CONTROLS},
    {"RelaxedJerkAcceleration", PERSISTENT | FROGPILOT_CONTROLS},
    {"RelaxedJerkDanger", PERSISTENT | FROGPILOT_CONTROLS},
    {"RelaxedJerkDeceleration", PERSISTENT | FROGPILOT_CONTROLS},
    {"RelaxedJerkSpeed", PERSISTENT | FROGPILOT_CONTROLS},
    {"RelaxedJerkSpeedDecrease", PERSISTENT | FROGPILOT_CONTROLS},
    {"RelaxedPersonalityProfile", PERSISTENT | FROGPILOT_CONTROLS},
    {"ReverseCruise", PERSISTENT | FROGPILOT_CONTROLS},
    {"RoadEdgesWidth", PERSISTENT | FROGPILOT_VISUALS},
    {"RoadName", CLEAR_ON_MANAGER_START},
    {"RoadNameUI", PERSISTENT | FROGPILOT_VISUALS},
    {"RotatingWheel", PERSISTENT | FROGPILOT_VISUALS},
    {"ScreenBrightness", PERSISTENT | FROGPILOT_VISUALS},
    {"ScreenBrightnessOnroad", PERSISTENT | FROGPILOT_VISUALS},
    {"ScreenManagement", PERSISTENT | FROGPILOT_VISUALS},
    {"ScreenRecorder", PERSISTENT | FROGPILOT_VISUALS},
    {"ScreenTimeout", PERSISTENT | FROGPILOT_VISUALS},
    {"ScreenTimeoutOnroad", PERSISTENT | FROGPILOT_VISUALS},
    {"SearchInput", PERSISTENT | FROGPILOT_OTHER},
    {"SecretGoodOpenpilotDrives", PERSISTENT | FROGPILOT_CONTROLS},
    {"SecretGoodOpenpilotScore", PERSISTENT | FROGPILOT_CONTROLS},
    {"SetSpeedLimit", PERSISTENT | FROGPILOT_CONTROLS},
    {"SetSpeedOffset", PERSISTENT | FROGPILOT_CONTROLS},
    {"ShowCEMStatus", PERSISTENT | FROGPILOT_VISUALS},
    {"ShowCPU", PERSISTENT | FROGPILOT_VISUALS},
    {"ShowGPU", PERSISTENT | FROGPILOT_VISUALS},
    {"ShowIP", PERSISTENT | FROGPILOT_VISUALS},
    {"ShowMemoryUsage", PERSISTENT | FROGPILOT_VISUALS},
    {"ShowSLCOffset", PERSISTENT | FROGPILOT_CONTROLS},
    {"ShowSpeedLimits", PERSISTENT | FROGPILOT_VISUALS},
    {"ShowSteering", PERSISTENT | FROGPILOT_VISUALS},
    {"ShowStoppingPoint", PERSISTENT | FROGPILOT_VISUALS},
    {"ShowStoppingPointMetrics", PERSISTENT | FROGPILOT_VISUALS},
    {"ShowStorageLeft", PERSISTENT | FROGPILOT_VISUALS},
    {"ShowStorageUsed", PERSISTENT | FROGPILOT_VISUALS},
    {"Sidebar", PERSISTENT | FROGPILOT_OTHER},
    {"SidebarMetrics", PERSISTENT | FROGPILOT_VISUALS},
    {"SignalMetrics", PERSISTENT | FROGPILOT_VISUALS},
    {"SignalToDownload", CLEAR_ON_MANAGER_START},
    {"SLCConfirmation", PERSISTENT | FROGPILOT_CONTROLS},
    {"SLCConfirmationHigher", PERSISTENT | FROGPILOT_CONTROLS},
    {"SLCConfirmationLower", PERSISTENT | FROGPILOT_CONTROLS},
    {"SLCLookaheadHigher", PERSISTENT | FROGPILOT_CONTROLS},
    {"SLCLookaheadLower", PERSISTENT | FROGPILOT_CONTROLS},
    {"SLCFallback", PERSISTENT | FROGPILOT_CONTROLS},
    {"SLCOverride", PERSISTENT | FROGPILOT_CONTROLS},
    {"SLCPriority1", PERSISTENT | FROGPILOT_CONTROLS},
    {"SLCPriority2", PERSISTENT | FROGPILOT_CONTROLS},
    {"SLCPriority3", PERSISTENT | FROGPILOT_CONTROLS},
    {"SmartTurnControl", PERSISTENT | FROGPILOT_CONTROLS},
    {"SNGHack", PERSISTENT | FROGPILOT_VEHICLES},
    {"SoundToDownload", CLEAR_ON_MANAGER_START},
    {"SpeedLimitAccepted", CLEAR_ON_MANAGER_START},
    {"SpeedLimitChangedAlert", PERSISTENT | FROGPILOT_CONTROLS},
    {"SpeedLimitController", PERSISTENT | FROGPILOT_CONTROLS},
    {"SpeedLimitSources", PERSISTENT | FROGPILOT_VISUALS},
    {"StandardFollow", PERSISTENT | FROGPILOT_CONTROLS},
    {"StandardJerkAcceleration", PERSISTENT | FROGPILOT_CONTROLS},
    {"StandardJerkDanger", PERSISTENT | FROGPILOT_CONTROLS},
    {"StandardJerkDeceleration", PERSISTENT | FROGPILOT_CONTROLS},
    {"StandardJerkSpeed", PERSISTENT | FROGPILOT_CONTROLS},
    {"StandardJerkSpeedDecrease", PERSISTENT | FROGPILOT_CONTROLS},
    {"StandardPersonalityProfile", PERSISTENT | FROGPILOT_CONTROLS},
    {"StandbyMode", PERSISTENT | FROGPILOT_VISUALS},
    {"StartupMessageBottom", PERSISTENT | FROGPILOT_VISUALS},
    {"StartupMessageTop", PERSISTENT | FROGPILOT_VISUALS},
    {"StaticPedalsOnUI", PERSISTENT | FROGPILOT_VISUALS},
    {"SteerFriction", PERSISTENT | FROGPILOT_CONTROLS},
    {"SteerFrictionStock", PERSISTENT},
    {"SteerLatAccel", PERSISTENT | FROGPILOT_CONTROLS},
    {"SteerLatAccelStock", PERSISTENT},
    {"SteerKP", PERSISTENT | FROGPILOT_CONTROLS},
    {"SteerKPStock", PERSISTENT},
    {"SteerRatio", PERSISTENT | FROGPILOT_CONTROLS},
    {"SteerRatioStock", PERSISTENT},
    {"StoppedTimer", PERSISTENT | FROGPILOT_VISUALS},
    {"TacoTune", PERSISTENT | FROGPILOT_CONTROLS},
    {"TestingSound", CLEAR_ON_MANAGER_START},
    {"TetheringEnabled", PERSISTENT | FROGPILOT_OTHER},
    {"ThemeDownloadProgress", CLEAR_ON_MANAGER_START},
    {"ToyotaDoors", PERSISTENT | FROGPILOT_VEHICLES},
    {"TrafficFollow", PERSISTENT | FROGPILOT_CONTROLS},
    {"TrafficJerkAcceleration", PERSISTENT | FROGPILOT_CONTROLS},
    {"TrafficJerkDanger", PERSISTENT | FROGPILOT_CONTROLS},
    {"TrafficJerkDeceleration", PERSISTENT | FROGPILOT_CONTROLS},
    {"TrafficJerkSpeed", PERSISTENT | FROGPILOT_CONTROLS},
    {"TrafficJerkSpeedDecrease", PERSISTENT | FROGPILOT_CONTROLS},
    {"TrafficPersonalityProfile", PERSISTENT | FROGPILOT_CONTROLS},
    {"TuningInfo", PERSISTENT | FROGPILOT_VISUALS},
    {"TuningLevel", PERSISTENT},
    {"TuningLevelConfirmed", PERSISTENT},
    {"TurnAggressiveness", PERSISTENT | FROGPILOT_CONTROLS},
    {"TurnDesires", PERSISTENT | FROGPILOT_CONTROLS},
    {"UnlimitedLength", PERSISTENT | FROGPILOT_VISUALS},
    {"UnlockDoors", PERSISTENT | FROGPILOT_VEHICLES},
    {"Updated", PERSISTENT},
    {"UpdateWheelImage", CLEAR_ON_MANAGER_START},
    {"UserCurvature", PERSISTENT},
    {"UserLogged", CLEAR_ON_MANAGER_START},
    {"UseSI", PERSISTENT | FROGPILOT_VISUALS},
    {"UseStockColors", CLEAR_ON_MANAGER_START},
    {"UseVienna", PERSISTENT | FROGPILOT_CONTROLS},
    {"VisionTurnControl", PERSISTENT | FROGPILOT_CONTROLS},
    {"VoltSNG", PERSISTENT | FROGPILOT_VEHICLES},
    {"WarningImmediateVolume", PERSISTENT | FROGPILOT_VISUALS},
    {"WarningSoftVolume", PERSISTENT | FROGPILOT_VISUALS},
    {"WD40Drives", PERSISTENT | FROGPILOT_CONTROLS},
    {"WD40Score", PERSISTENT | FROGPILOT_CONTROLS},
    {"WheelIcon", PERSISTENT | FROGPILOT_VISUALS},
    {"WheelSpeed", PERSISTENT | FROGPILOT_VISUALS},
    {"WheelToDownload", CLEAR_ON_MANAGER_START},
};

} // namespace


Params::Params(const std::string &path) {
  params_prefix = "/" + util::getenv("OPENPILOT_PREFIX", "d");
  params_path = ensure_params_path(params_prefix, path);
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
