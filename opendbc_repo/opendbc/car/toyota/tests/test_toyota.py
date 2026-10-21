from hypothesis import given, settings, strategies as st
from parameterized import parameterized

from opendbc.can import CANPacker, CANParser
from opendbc.car import Bus, gen_empty_fingerprint
from opendbc.car.structs import CarControl, CarParams, CarState
from opendbc.car.fw_versions import build_fw_dict
from opendbc.car.tests.test_car_interfaces import TOGGLES
from opendbc.car.toyota.fingerprints import FW_VERSIONS
from opendbc.car.toyota.interface import CarInterface
from opendbc.car.toyota.values import CAR, DBC, TSS2_CAR, ANGLE_CONTROL_CAR, RADAR_ACC_CAR, SECOC_CAR, \
                                                  FW_QUERY_CONFIG, PLATFORM_CODE_ECUS, FUZZY_EXCLUDED_PLATFORMS, \
                                                  get_platform_codes

Ecu = CarParams.Ecu


def check_fw_version(fw_version: bytes) -> bool:
  # TODO: just use the FW patterns, need to support all chunks
  return b'?' not in fw_version and b'!' not in fw_version


class TestToyotaInterfaces:
  def test_car_sets(self):
    assert len(ANGLE_CONTROL_CAR - TSS2_CAR) == 0
    assert len(RADAR_ACC_CAR - TSS2_CAR) == 0

  def test_lta_platforms(self):
    # At this time, only RAV4 2023 is expected to use LTA/angle control
    assert ANGLE_CONTROL_CAR == {CAR.TOYOTA_RAV4_TSS2_2023}

  def test_tss2_dbc(self):
    # We make some assumptions about TSS2 platforms,
    # like looking up certain signals only in this DBC
    for car_model, dbc in DBC.items():
      if car_model in TSS2_CAR and car_model not in SECOC_CAR:
        assert dbc[Bus.pt] == "toyota_nodsu_pt_generated"

  def test_essential_ecus(self, subtests):
    # Asserts standard ECUs exist for each platform
    common_ecus = {Ecu.fwdRadar, Ecu.fwdCamera}
    for car_model, ecus in FW_VERSIONS.items():
      with subtests.test(car_model=car_model.value):
        present_ecus = {ecu[0] for ecu in ecus}
        missing_ecus = common_ecus - present_ecus
        assert len(missing_ecus) == 0

        # Some exceptions for other common ECUs
        if car_model not in (CAR.TOYOTA_ALPHARD_TSS2,):
          assert Ecu.abs in present_ecus

        if car_model not in (CAR.TOYOTA_MIRAI,):
          assert Ecu.engine in present_ecus

        if car_model not in (CAR.TOYOTA_PRIUS_V, CAR.LEXUS_CTH):
          assert Ecu.eps in present_ecus


class TestToyotaFingerprint:
  def test_non_essential_ecus(self, subtests):
    # Ensures only the cars that have multiple engine ECUs are in the engine non-essential ECU list
    for car_model, ecus in FW_VERSIONS.items():
      with subtests.test(car_model=car_model.value):
        engine_ecus = {ecu for ecu in ecus if ecu[0] == Ecu.engine}
        assert (len(engine_ecus) > 1) == (car_model in FW_QUERY_CONFIG.non_essential_ecus[Ecu.engine]), \
          f"Car model unexpectedly {'not ' if len(engine_ecus) > 1 else ''}in non-essential list"

  def test_valid_fw_versions(self, subtests):
    # Asserts all FW versions are valid
    for car_model, ecus in FW_VERSIONS.items():
      with subtests.test(car_model=car_model.value):
        for fws in ecus.values():
          for fw in fws:
            assert check_fw_version(fw), fw

  # Tests for part numbers, platform codes, and sub-versions which Toyota will use to fuzzy
  # fingerprint in the absence of full FW matches:
  @settings(max_examples=100)
  @given(data=st.data())
  def test_platform_codes_fuzzy_fw(self, data):
    fw_strategy = st.lists(st.binary())
    fws = data.draw(fw_strategy)
    get_platform_codes(fws)

  def test_platform_code_ecus_available(self, subtests):
    # Asserts ECU keys essential for fuzzy fingerprinting are available on all platforms
    for car_model, ecus in FW_VERSIONS.items():
      with subtests.test(car_model=car_model.value):
        for platform_code_ecu in PLATFORM_CODE_ECUS:
          if platform_code_ecu == Ecu.eps and car_model in (CAR.TOYOTA_PRIUS_V, CAR.LEXUS_CTH,):
            continue
          if platform_code_ecu == Ecu.abs and car_model in (CAR.TOYOTA_ALPHARD_TSS2,):
            continue
          assert platform_code_ecu in [e[0] for e in ecus]

  def test_fw_format(self, subtests):
    # Asserts:
    # - every supported ECU FW version returns one platform code
    # - every supported ECU FW version has a part number
    # - expected parsing of ECU sub-versions

    for car_model, ecus in FW_VERSIONS.items():
      with subtests.test(car_model=car_model.value):
        for ecu, fws in ecus.items():
          if ecu[0] not in PLATFORM_CODE_ECUS:
            continue

          codes = dict()
          for fw in fws:
            result = get_platform_codes([fw])
            # Check only one platform code and sub-version
            assert 1 == len(result), f"Unable to parse FW: {fw}"
            assert 1 == len(list(result.values())[0]), f"Unable to parse FW: {fw}"
            codes |= result

          # Toyota places the ECU part number in their FW versions, assert all parsable
          # Note that there is only one unique part number per ECU across the fleet, so this
          # is not important for identification, just a sanity check.
          assert all(code.count(b"-") > 1 for code in codes), f"FW does not have part number: {fw} {codes}"

  def test_platform_codes_spot_check(self):
    # Asserts basic platform code parsing behavior for a few cases
    results = get_platform_codes([
      b"F152607140\x00\x00\x00\x00\x00\x00",
      b"F152607171\x00\x00\x00\x00\x00\x00",
      b"F152607110\x00\x00\x00\x00\x00\x00",
      b"F152607180\x00\x00\x00\x00\x00\x00",
    ])
    assert results == {b"F1526-07-1": {b"10", b"40", b"71", b"80"}}

    results = get_platform_codes([
      b"\x028646F4104100\x00\x00\x00\x008646G5301200\x00\x00\x00\x00",
      b"\x028646F4104100\x00\x00\x00\x008646G3304000\x00\x00\x00\x00",
    ])
    assert results == {b"8646F-41-04": {b"100"}}

    # Short version has no part number
    results = get_platform_codes([
      b"\x0235870000\x00\x00\x00\x00\x00\x00\x00\x00A0202000\x00\x00\x00\x00\x00\x00\x00\x00",
      b"\x0235883000\x00\x00\x00\x00\x00\x00\x00\x00A0202000\x00\x00\x00\x00\x00\x00\x00\x00",
    ])
    assert results == {b"58-70": {b"000"}, b"58-83": {b"000"}}

    results = get_platform_codes([
      b"F152607110\x00\x00\x00\x00\x00\x00",
      b"F152607140\x00\x00\x00\x00\x00\x00",
      b"\x028646F4104100\x00\x00\x00\x008646G5301200\x00\x00\x00\x00",
      b"\x0235879000\x00\x00\x00\x00\x00\x00\x00\x00A4701000\x00\x00\x00\x00\x00\x00\x00\x00",
    ])
    assert results == {b"F1526-07-1": {b"10", b"40"}, b"8646F-41-04": {b"100"}, b"58-79": {b"000"}}

  def test_fuzzy_excluded_platforms(self):
    # Asserts a list of platforms that will not fuzzy fingerprint with platform codes due to them being shared.
    platforms_with_shared_codes = set()
    for platform, fw_by_addr in FW_VERSIONS.items():
      car_fw = []
      for ecu, fw_versions in fw_by_addr.items():
        ecu_name, addr, sub_addr = ecu
        for fw in fw_versions:
          car_fw.append(CarParams.CarFw(ecu=ecu_name, fwVersion=fw, address=addr,
                                        subAddress=0 if sub_addr is None else sub_addr))

      CP = CarParams(carFw=car_fw)
      matches = FW_QUERY_CONFIG.match_fw_to_car_fuzzy(build_fw_dict(CP.carFw), CP.carVin, FW_VERSIONS)
      if len(matches) == 1:
        assert list(matches)[0] == platform
      else:
        # If a platform has multiple matches, add it and its matches
        platforms_with_shared_codes |= {str(platform), *matches}

    assert platforms_with_shared_codes == FUZZY_EXCLUDED_PLATFORMS, (len(platforms_with_shared_codes), len(FW_VERSIONS))


# FrogPilot variables
class TestToyotaDashboard:
  @parameterized.expand([(True, False, True), (False, True, True), (False, False, False)])
  def test_lane_keeping_shown(self, enabled, lat_active, shown):
    CP = CarInterface.get_non_essential_params(CAR.LEXUS_ES_TSS2)
    FPCP = CarInterface.get_frogpilot_params(CAR.LEXUS_ES_TSS2, gen_empty_fingerprint(), [], CP, TOGGLES)
    CI = CarInterface(CP, FPCP)
    parser = CANParser(DBC[CAR.LEXUS_ES_TSS2][Bus.pt], [("LKAS_HUD", float("nan"))], 0)

    CC = CarControl(enabled=enabled, latActive=lat_active)
    CC.hudControl.leftLaneVisible = True

    CI.update([], TOGGLES)
    _, can_sends = CI.apply(CC.as_reader(), 0, TOGGLES)
    parser.update([(0, can_sends)])

    lane_keeping = (parser.vl["LKAS_HUD"]["BARRIERS"], parser.vl["LKAS_HUD"]["LEFT_LINE"], parser.vl["LKAS_HUD"]["RIGHT_LINE"])

    if shown:
      assert lane_keeping == (1, 1, 2)
    else:
      assert lane_keeping == (0, 0, 0)


class TestToyotaLkasButton:
  @parameterized.expand([(CAR.LEXUS_ES_TSS2,), (CAR.TOYOTA_PRIUS,)])
  def test_lkas_presses(self, car_model):
    CP = CarInterface.get_non_essential_params(car_model)
    FPCP = CarInterface.get_frogpilot_params(car_model, gen_empty_fingerprint(), [], CP, TOGGLES)
    CI = CarInterface(CP, FPCP)
    packer = CANPacker(DBC[car_model][Bus.pt])

    def pressed(lkas_hud, cruise_active=0):
      can_packets = [(0, [packer.make_can_msg("LKAS_HUD", 2, lkas_hud), packer.make_can_msg("PCM_CRUISE", 0, {"CRUISE_ACTIVE": cruise_active})])]
      CS, _ = CI.update(can_packets, TOGGLES)

      return any(be.pressed and be.type == CarState.ButtonEvent.Type.lkas for be in CS.buttonEvents)

    assert not pressed({"LKAS_STATUS": 0})
    assert not pressed({"LKAS_STATUS": 1})
    assert pressed({"LKAS_STATUS": 0})
    assert pressed({"LDA_ON_MESSAGE": 1, "LKAS_STATUS": 1})
    assert not pressed({"LDA_ON_MESSAGE": 1, "LKAS_STATUS": 2})

    for camera_turned_off in ("LDA_MALFUNCTION", "LDA_UNAVAILABLE", "TWO_BEEPS"):
      assert not pressed({camera_turned_off: 1, "LKAS_STATUS": 0}), camera_turned_off
      assert not pressed({"LKAS_STATUS": 1}), camera_turned_off

    assert not pressed({"LKAS_STATUS": 0}, cruise_active=1)
    assert pressed({"LDA_ON_MESSAGE": 2, "LKAS_STATUS": 1}, cruise_active=1)
