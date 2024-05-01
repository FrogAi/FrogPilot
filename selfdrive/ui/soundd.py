import math
import numpy as np
import os
import time
import wave


from cereal import car, messaging
from openpilot.common.basedir import BASEDIR
from openpilot.common.filter_simple import FirstOrderFilter
from openpilot.common.params import Params
from openpilot.common.realtime import Ratekeeper
from openpilot.common.retry import retry
from openpilot.common.swaglog import cloudlog

from openpilot.system import micd

SAMPLE_RATE = 48000
SAMPLE_BUFFER = 4096 # (approx 100ms)
MAX_VOLUME = 1.0
MIN_VOLUME = 0.1
CONTROLS_TIMEOUT = 5 # 5 seconds
FILTER_DT = 1. / (micd.SAMPLE_RATE / micd.FFT_SAMPLES)

AMBIENT_DB = 30 # DB where MIN_VOLUME is applied
DB_SCALE = 30 # AMBIENT_DB + DB_SCALE is where MAX_VOLUME is applied

AudibleAlert = car.CarControl.HUDControl.AudibleAlert


sound_list: dict[int, tuple[str, int | None, float]] = {
  # AudibleAlert, file name, play count (none for infinite)
  AudibleAlert.engage: ("engage.wav", 1, MAX_VOLUME),
  AudibleAlert.disengage: ("disengage.wav", 1, MAX_VOLUME),
  AudibleAlert.refuse: ("refuse.wav", 1, MAX_VOLUME),

  AudibleAlert.prompt: ("prompt.wav", 1, MAX_VOLUME),
  AudibleAlert.promptRepeat: ("prompt.wav", None, MAX_VOLUME),
  AudibleAlert.promptDistracted: ("prompt_distracted.wav", None, MAX_VOLUME),

  AudibleAlert.warningSoft: ("warning_soft.wav", None, MAX_VOLUME),
  AudibleAlert.warningImmediate: ("warning_immediate.wav", None, MAX_VOLUME),

  # Random Events
  AudibleAlert.angry: ("angry.wav", 1, MAX_VOLUME),
  AudibleAlert.doc: ("doc.wav", 1, MAX_VOLUME),
  AudibleAlert.fart: ("fart.wav", 1, MAX_VOLUME),
  AudibleAlert.firefox: ("firefox.wav", 1, MAX_VOLUME),
  AudibleAlert.nessie: ("nessie.wav", 1, MAX_VOLUME),
  AudibleAlert.noice: ("noice.wav", 1, MAX_VOLUME),
  AudibleAlert.uwu: ("uwu.wav", 1, MAX_VOLUME),

  # Other
  AudibleAlert.goat: ("goat.wav", None, MAX_VOLUME),
}

def check_controls_timeout_alert(sm):
  controls_missing = time.monotonic() - sm.recv_time['controlsState']

  if controls_missing > CONTROLS_TIMEOUT:
    if sm['controlsState'].enabled and (controls_missing - CONTROLS_TIMEOUT) < 10:
      return True

  return False


class Soundd:
  def __init__(self):
    # FrogPilot variables
    self.params = Params()
    self.params_memory = Params("/dev/shm/params")

    self.previous_sound_directory = None
    self.random_events_directory = BASEDIR + "/selfdrive/frogpilot/assets/random_events/sounds/"

    self.random_events_map = {
      AudibleAlert.angry: MAX_VOLUME,
      AudibleAlert.doc: MAX_VOLUME,
      AudibleAlert.fart: MAX_VOLUME,
      AudibleAlert.firefox: MAX_VOLUME,
      AudibleAlert.nessie: MAX_VOLUME,
      AudibleAlert.noice: MAX_VOLUME,
      AudibleAlert.uwu: MAX_VOLUME,
    }

    self.update_frogpilot_params()

    self.current_alert = AudibleAlert.none
    self.current_volume = MIN_VOLUME
    self.current_sound_frame = 0

    self.controls_timeout_alert = False

    self.spl_filter_weighted = FirstOrderFilter(0, 2.5, FILTER_DT, initialized=False)

  def load_sounds(self):
    self.loaded_sounds: dict[int, np.ndarray] = {}

    # Load all sounds
    for sound in sound_list:
      if sound == AudibleAlert.goat and not self.goat_scream:
        continue

      filename, play_count, volume = sound_list[sound]

      if sound in self.random_events_map:
        wavefile = wave.open(self.random_events_directory + filename, 'r')
      else:
        try:
          wavefile = wave.open(self.sound_directory + filename, 'r')
        except FileNotFoundError:
          wavefile = wave.open(BASEDIR + "/selfdrive/assets/sounds/" + filename, 'r')

      assert wavefile.getnchannels() == 1
      assert wavefile.getsampwidth() == 2
      assert wavefile.getframerate() == SAMPLE_RATE

      length = wavefile.getnframes()
      self.loaded_sounds[sound] = np.frombuffer(wavefile.readframes(length), dtype=np.int16).astype(np.float32) / (2**16/2)

  def get_sound_data(self, frames): # get "frames" worth of data from the current alert sound, looping when required

    ret = np.zeros(frames, dtype=np.float32)

    if self.current_alert != AudibleAlert.none:
      num_loops = sound_list[self.current_alert][1]
      sound_data = self.loaded_sounds[self.current_alert]
      written_frames = 0

      current_sound_frame = self.current_sound_frame % len(sound_data)
      loops = self.current_sound_frame // len(sound_data)

      while written_frames < frames and (num_loops is None or loops < num_loops):
        available_frames = sound_data.shape[0] - current_sound_frame
        frames_to_write = min(available_frames, frames - written_frames)
        ret[written_frames:written_frames+frames_to_write] = sound_data[current_sound_frame:current_sound_frame+frames_to_write]
        written_frames += frames_to_write
        self.current_sound_frame += frames_to_write

    return ret * self.current_volume

  def callback(self, data_out: np.ndarray, frames: int, time, status) -> None:
    if status:
      cloudlog.warning(f"soundd stream over/underflow: {status}")
    data_out[:frames, 0] = self.get_sound_data(frames)

  def update_alert(self, new_alert):
    current_alert_played_once = self.current_alert == AudibleAlert.none or self.current_sound_frame > len(self.loaded_sounds[self.current_alert])
    if self.current_alert != new_alert and (new_alert != AudibleAlert.none or current_alert_played_once):
      self.current_alert = new_alert
      self.current_sound_frame = 0

  def get_audible_alert(self, sm):
    if sm.updated['controlsState']:
      new_alert = sm['controlsState'].alertSound.raw
      self.update_alert(new_alert)
    elif check_controls_timeout_alert(sm):
      self.update_alert(AudibleAlert.warningImmediate)
      self.controls_timeout_alert = True
    elif self.controls_timeout_alert:
      self.update_alert(AudibleAlert.none)
      self.controls_timeout_alert = False

  def calculate_volume(self, weighted_db):
    volume = ((weighted_db - AMBIENT_DB) / DB_SCALE) * (MAX_VOLUME - MIN_VOLUME) + MIN_VOLUME
    return math.pow(10, (np.clip(volume, MIN_VOLUME, MAX_VOLUME) - 1))

  @retry(attempts=7, delay=3)
  def get_stream(self, sd):
    # reload sounddevice to reinitialize portaudio
    sd._terminate()
    sd._initialize()
    return sd.OutputStream(channels=1, samplerate=SAMPLE_RATE, callback=self.callback, blocksize=SAMPLE_BUFFER)

  def soundd_thread(self):
    # sounddevice must be imported after forking processes
    import sounddevice as sd

    sm = messaging.SubMaster(['controlsState', 'microphone'])

    with self.get_stream(sd) as stream:
      rk = Ratekeeper(20)

      cloudlog.info(f"soundd stream started: {stream.samplerate=} {stream.channels=} {stream.dtype=} {stream.device=}, {stream.blocksize=}")
      while True:
        sm.update(0)

        if sm.updated['microphone'] and self.current_alert == AudibleAlert.none and not self.alert_volume_control: # only update volume filter when not playing alert
          self.spl_filter_weighted.update(sm["microphone"].soundPressureWeightedDb)
          self.current_volume = self.calculate_volume(float(self.spl_filter_weighted.x))

        elif self.alert_volume_control and self.current_alert in self.volume_map:
          self.current_volume = self.volume_map[self.current_alert] / 100.0

        elif self.current_alert in self.random_events_map:
          self.current_volume = self.random_events_map[self.current_alert]

        self.get_audible_alert(sm)

        rk.keep_time()

        assert stream.active

        # Update FrogPilot parameters
        if self.params_memory.get_bool("FrogPilotTogglesUpdated"):
          self.update_frogpilot_params()

  def update_frogpilot_params(self):
    self.alert_volume_control = self.params.get_bool("AlertVolumeControl")

    self.volume_map = {
      AudibleAlert.engage: self.params.get_int("EngageVolume"),
      AudibleAlert.disengage: self.params.get_int("DisengageVolume"),
      AudibleAlert.refuse: self.params.get_int("RefuseVolume"),

      AudibleAlert.prompt: self.params.get_int("PromptVolume"),
      AudibleAlert.promptRepeat: self.params.get_int("PromptVolume"),
      AudibleAlert.promptDistracted: self.params.get_int("PromptDistractedVolume"),

      AudibleAlert.warningSoft: self.params.get_int("WarningSoftVolume"),
      AudibleAlert.warningImmediate: self.params.get_int("WarningImmediateVolume"),

      AudibleAlert.goat: self.params.get_int("PromptVolume"),
    }

    custom_theme = self.params.get_bool("CustomTheme")
    custom_sounds = self.params.get_int("CustomSounds") if custom_theme else 0
    self.goat_scream = custom_sounds == 1 and self.params.get_bool("GoatScream")

    theme_configuration = {
      1: "frog_theme",
      2: "tesla_theme",
      3: "stalin_theme"
    }

    holiday_themes = custom_theme and self.params.get_bool("HolidayThemes")
    current_holiday_theme = self.params_memory.get_int("CurrentHolidayTheme") if holiday_themes else 0

    holiday_theme_configuration = {
      1: "april_fools",
      2: "christmas",
      3: "cinco_de_mayo",
      4: "easter",
      5: "fourth_of_july",
      6: "halloween",
      7: "new_years_day",
      8: "st_patricks_day",
      9: "thanksgiving",
      10: "valentines_day",
      11: "world_frog_day",
    }

    if current_holiday_theme != 0:
      theme_name = holiday_theme_configuration.get(current_holiday_theme)
      self.sound_directory = BASEDIR + ("/selfdrive/frogpilot/assets/holiday_themes/" + theme_name + "/sounds/")
      self.goat_scream = False
    else:
      theme_name = theme_configuration.get(custom_sounds)
      self.sound_directory = BASEDIR + ("/selfdrive/frogpilot/assets/custom_themes/" + theme_name + "/sounds/" if custom_sounds != 0 else "/selfdrive/assets/sounds/")

    if self.sound_directory != self.previous_sound_directory:
      self.load_sounds()

    self.previous_sound_directory = self.sound_directory

def main():
  s = Soundd()
  s.soundd_thread()


if __name__ == "__main__":
  main()
