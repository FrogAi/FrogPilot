from hypothesis import settings, given, strategies as st

import pytest

from cereal import car
from opendbc.can.packer import CANPacker
from opendbc.can.parser import CANParser
from openpilot.selfdrive.car import gen_empty_fingerprint
from openpilot.selfdrive.car.car_helpers import interfaces
from openpilot.selfdrive.car.fw_versions import build_fw_dict
from openpilot.selfdrive.car.hyundai.values import CAMERA_SCC_CAR, CANFD_CAR, CAN_GEARS, CAR, CHECKSUM, DATE_FW_ECUS, \
                                         HYBRID_CAR, EV_CAR, FW_QUERY_CONFIG, LEGACY_SAFETY_MODE_CAR, CANFD_FUZZY_WHITELIST, \
                                         UNSUPPORTED_LONGITUDINAL_CAR, PLATFORM_CODE_ECUS, HYUNDAI_VERSION_REQUEST_LONG, \
                                         get_platform_codes, DBC
from openpilot.selfdrive.car.hyundai.fingerprints import FW_VERSIONS
from openpilot.selfdrive.car.hyundai.hyundaicanfd import CanBus
from openpilot.selfdrive.car.tests.test_car_interfaces import TOGGLES
from openpilot.selfdrive.pandad import can_list_to_can_capnp
from panda import ALTERNATIVE_EXPERIENCE

Ecu = car.CarParams.Ecu
ECU_NAME = {v: k for k, v in Ecu.schema.enumerants.items()}

# Some platforms have date codes in a different format we don't yet parse (or are missing).
# For now, assert list of expected missing date cars
NO_DATES_PLATFORMS = {
  # CAN FD
  CAR.KIA_SPORTAGE_5TH_GEN,
  CAR.HYUNDAI_SANTA_CRUZ_1ST_GEN,
  CAR.HYUNDAI_TUCSON_4TH_GEN,
  # CAN
  CAR.HYUNDAI_ELANTRA,
  CAR.HYUNDAI_ELANTRA_GT_I30,
  CAR.KIA_CEED,
  CAR.KIA_FORTE,
  CAR.KIA_OPTIMA_G4,
  CAR.KIA_OPTIMA_G4_FL,
  CAR.KIA_SORENTO,
  CAR.HYUNDAI_KONA,
  CAR.HYUNDAI_KONA_EV,
  CAR.HYUNDAI_KONA_EV_2022,
  CAR.HYUNDAI_KONA_HEV,
  CAR.HYUNDAI_SONATA_LF,
  CAR.HYUNDAI_VELOSTER,
}

CANFD_EXPECTED_ECUS = {Ecu.fwdCamera, Ecu.fwdRadar}


class TestHyundaiFingerprint:
  def test_can_features(self):
    # Test no EV/HEV in any gear lists (should all use ELECT_GEAR)
    assert set.union(*CAN_GEARS.values()) & (HYBRID_CAR | EV_CAR) == set()

    # Test CAN FD car not in CAN feature lists
    can_specific_feature_list = set.union(*CAN_GEARS.values(), *CHECKSUM.values(), LEGACY_SAFETY_MODE_CAR, UNSUPPORTED_LONGITUDINAL_CAR, CAMERA_SCC_CAR)
    for car_model in CANFD_CAR:
      assert car_model not in can_specific_feature_list, "CAN FD car unexpectedly found in a CAN feature list"

  def test_hybrid_ev_sets(self):
    assert HYBRID_CAR & EV_CAR == set(), "Shared cars between hybrid and EV"
    assert CANFD_CAR & HYBRID_CAR == set(), "Hard coding CAN FD cars as hybrid is no longer supported"

  def test_canfd_ecu_whitelist(self):
    # Asserts only expected Ecus can exist in database for CAN-FD cars
    for car_model in CANFD_CAR:
      ecus = {fw[0] for fw in FW_VERSIONS[car_model].keys()}
      ecus_not_in_whitelist = ecus - CANFD_EXPECTED_ECUS
      ecu_strings = ", ".join([f"Ecu.{ECU_NAME[ecu]}" for ecu in ecus_not_in_whitelist])
      assert len(ecus_not_in_whitelist) == 0, \
                       f"{car_model}: Car model has unexpected ECUs: {ecu_strings}"

  def test_blacklisted_parts(self, subtests):
    # Asserts no ECUs known to be shared across platforms exist in the database.
    # Tucson having Santa Cruz camera and EPS for example
    for car_model, ecus in FW_VERSIONS.items():
      with subtests.test(car_model=car_model.value):
        if car_model == CAR.HYUNDAI_SANTA_CRUZ_1ST_GEN:
          pytest.skip("Skip checking Santa Cruz for its parts")

        for code, _ in get_platform_codes(ecus[(Ecu.fwdCamera, 0x7c4, None)]):
          if b"-" not in code:
            continue
          part = code.split(b"-")[1]
          assert not part.startswith(b'CW'), "Car has bad part number"

  def test_correct_ecu_response_database(self, subtests):
    """
    Assert standard responses for certain ECUs, since they can
    respond to multiple queries with different data
    """
    expected_fw_prefix = HYUNDAI_VERSION_REQUEST_LONG[1:]
    for car_model, ecus in FW_VERSIONS.items():
      with subtests.test(car_model=car_model.value):
        for ecu, fws in ecus.items():
          assert all(fw.startswith(expected_fw_prefix) for fw in fws), \
                          f"FW from unexpected request in database: {(ecu, fws)}"

  @settings(max_examples=100)
  @given(data=st.data())
  def test_platform_codes_fuzzy_fw(self, data):
    """Ensure function doesn't raise an exception"""
    fw_strategy = st.lists(st.binary())
    fws = data.draw(fw_strategy)
    get_platform_codes(fws)

  def test_expected_platform_codes(self, subtests):
    # Ensures we don't accidentally add multiple platform codes for a car unless it is intentional
    for car_model, ecus in FW_VERSIONS.items():
      with subtests.test(car_model=car_model.value):
        for ecu, fws in ecus.items():
          if ecu[0] not in PLATFORM_CODE_ECUS:
            continue

          # Third and fourth character are usually EV/hybrid identifiers
          codes = {code.split(b"-")[0][:2] for code, _ in get_platform_codes(fws)}
          if car_model == CAR.HYUNDAI_PALISADE:
            assert codes == {b"LX", b"ON"}, f"Car has unexpected platform codes: {car_model} {codes}"
          elif car_model == CAR.HYUNDAI_KONA_EV and ecu[0] == Ecu.fwdCamera:
            assert codes == {b"OE", b"OS"}, f"Car has unexpected platform codes: {car_model} {codes}"
          else:
            assert len(codes) == 1, f"Car has multiple platform codes: {car_model} {codes}"

  # Tests for platform codes, part numbers, and FW dates which Hyundai will use to fuzzy
  # fingerprint in the absence of full FW matches:
  def test_platform_code_ecus_available(self, subtests):
    # TODO: add queries for these non-CAN FD cars to get EPS
    no_eps_platforms = CANFD_CAR | {CAR.KIA_SORENTO, CAR.KIA_OPTIMA_G4, CAR.KIA_OPTIMA_G4_FL, CAR.KIA_OPTIMA_H,
                                    CAR.KIA_OPTIMA_H_G4_FL, CAR.HYUNDAI_SONATA_LF, CAR.HYUNDAI_TUCSON, CAR.GENESIS_G90, CAR.GENESIS_G80, CAR.HYUNDAI_ELANTRA}

    # Asserts ECU keys essential for fuzzy fingerprinting are available on all platforms
    for car_model, ecus in FW_VERSIONS.items():
      with subtests.test(car_model=car_model.value):
        for platform_code_ecu in PLATFORM_CODE_ECUS:
          if platform_code_ecu in (Ecu.fwdRadar, Ecu.eps) and car_model == CAR.HYUNDAI_GENESIS:
            continue
          if platform_code_ecu == Ecu.eps and car_model in no_eps_platforms:
            continue
          assert platform_code_ecu in [e[0] for e in ecus]

  def test_fw_format(self, subtests):
    # Asserts:
    # - every supported ECU FW version returns one platform code
    # - every supported ECU FW version has a part number
    # - expected parsing of ECU FW dates

    for car_model, ecus in FW_VERSIONS.items():
      with subtests.test(car_model=car_model.value):
        for ecu, fws in ecus.items():
          if ecu[0] not in PLATFORM_CODE_ECUS:
            continue

          codes = set()
          for fw in fws:
            result = get_platform_codes([fw])
            assert 1 == len(result), f"Unable to parse FW: {fw}"
            codes |= result

          if ecu[0] not in DATE_FW_ECUS or car_model in NO_DATES_PLATFORMS:
            assert all(date is None for _, date in codes)
          else:
            assert all(date is not None for _, date in codes)

          if car_model == CAR.HYUNDAI_GENESIS:
            pytest.skip("No part numbers for car model")

          # Hyundai places the ECU part number in their FW versions, assert all parsable
          # Some examples of valid formats: b"56310-L0010", b"56310L0010", b"56310/M6300"
          assert all(b"-" in code for code, _ in codes), \
                          f"FW does not have part number: {fw}"

  def test_platform_codes_spot_check(self):
    # Asserts basic platform code parsing behavior for a few cases
    results = get_platform_codes([b"\xf1\x00DH LKAS 1.1 -150210"])
    assert results == {(b"DH", b"150210")}

    # Some cameras and all radars do not have dates
    results = get_platform_codes([b"\xf1\x00AEhe SCC H-CUP      1.01 1.01 96400-G2000         "])
    assert results == {(b"AEhe-G2000", None)}

    results = get_platform_codes([b"\xf1\x00CV1_ RDR -----      1.00 1.01 99110-CV000         "])
    assert results == {(b"CV1-CV000", None)}

    results = get_platform_codes([
      b"\xf1\x00DH LKAS 1.1 -150210",
      b"\xf1\x00AEhe SCC H-CUP      1.01 1.01 96400-G2000         ",
      b"\xf1\x00CV1_ RDR -----      1.00 1.01 99110-CV000         ",
    ])
    assert results == {(b"DH", b"150210"), (b"AEhe-G2000", None), (b"CV1-CV000", None)}

    results = get_platform_codes([
      b"\xf1\x00LX2 MFC  AT USA LHD 1.00 1.07 99211-S8100 220222",
      b"\xf1\x00LX2 MFC  AT USA LHD 1.00 1.08 99211-S8100 211103",
      b"\xf1\x00ON  MFC  AT USA LHD 1.00 1.01 99211-S9100 190405",
      b"\xf1\x00ON  MFC  AT USA LHD 1.00 1.03 99211-S9100 190720",
    ])
    assert results == {(b"LX2-S8100", b"220222"), (b"LX2-S8100", b"211103"),
                               (b"ON-S9100", b"190405"), (b"ON-S9100", b"190720")}

  def test_fuzzy_excluded_platforms(self):
    # Asserts a list of platforms that will not fuzzy fingerprint with platform codes due to them being shared.
    # This list can be shrunk as we combine platforms and detect features
    excluded_platforms = {
      CAR.GENESIS_G70,            # shared platform code, part number, and date
      CAR.GENESIS_G70_2020,
    }
    excluded_platforms |= CANFD_CAR - EV_CAR - CANFD_FUZZY_WHITELIST  # shared platform codes
    excluded_platforms |= NO_DATES_PLATFORMS  # date codes are required to match

    platforms_with_shared_codes = set()
    for platform, fw_by_addr in FW_VERSIONS.items():
      car_fw = []
      for ecu, fw_versions in fw_by_addr.items():
        ecu_name, addr, sub_addr = ecu
        for fw in fw_versions:
          car_fw.append({"ecu": ecu_name, "fwVersion": fw, "address": addr,
                         "subAddress": 0 if sub_addr is None else sub_addr})

      CP = car.CarParams.new_message(carFw=car_fw)
      matches = FW_QUERY_CONFIG.match_fw_to_car_fuzzy(build_fw_dict(CP.carFw), CP.carVin, FW_VERSIONS)
      if len(matches) == 1:
        assert list(matches)[0] == platform
      else:
        platforms_with_shared_codes.add(platform)

    assert platforms_with_shared_codes == excluded_platforms


# FrogPilot variables
class TestAlwaysOnLateral:
  def test_lane_keep_dashboard(self):
    can_fingerprint = gen_empty_fingerprint()
    can_fingerprint[2][0x485] = 4

    for candidate, fingerprint, enabled, lat_active, expected in (
      (CAR.HYUNDAI_SANTA_FE_2022, can_fingerprint, False, True,
       {("LKAS11", "CF_Lkas_FcwOpt_USM"): 2, ("LKAS11", "CF_Lkas_LdwsSysState"): 3, ("LFAHDA_MFC", "LFA_Icon_State"): 2}),
      (CAR.HYUNDAI_SANTA_FE_2022, can_fingerprint, True, False,
       {("LKAS11", "CF_Lkas_FcwOpt_USM"): 2, ("LKAS11", "CF_Lkas_LdwsSysState"): 3, ("LFAHDA_MFC", "LFA_Icon_State"): 2}),
      (CAR.HYUNDAI_SANTA_FE_2022, can_fingerprint, False, False,
       {("LKAS11", "CF_Lkas_FcwOpt_USM"): 1, ("LKAS11", "CF_Lkas_LdwsSysState"): 4, ("LFAHDA_MFC", "LFA_Icon_State"): 0}),
      (CAR.HYUNDAI_TUCSON_4TH_GEN, gen_empty_fingerprint(), False, True,
       {("LFA", "LKA_ICON"): 2, ("LFAHDA_CLUSTER", "HDA_ICON"): 0, ("LFAHDA_CLUSTER", "LFA_ICON"): 2}),
      (CAR.HYUNDAI_TUCSON_4TH_GEN, gen_empty_fingerprint(), True, False,
       {("LFA", "LKA_ICON"): 2, ("LFAHDA_CLUSTER", "HDA_ICON"): 1, ("LFAHDA_CLUSTER", "LFA_ICON"): 2}),
      (CAR.HYUNDAI_TUCSON_4TH_GEN, gen_empty_fingerprint(), False, False,
       {("LFA", "LKA_ICON"): 1, ("LFAHDA_CLUSTER", "HDA_ICON"): 0, ("LFAHDA_CLUSTER", "LFA_ICON"): 0}),
    ):
      CarInterface, CarController, CarState = interfaces[candidate]
      CP = CarInterface.get_params(candidate, fingerprint, [], False, TOGGLES, docs=False)
      FPCP = CarInterface.get_frogpilot_params(candidate, fingerprint, [], CP, TOGGLES)
      CI = CarInterface(CP, FPCP, CarController, CarState)

      CC = car.CarControl.new_message(enabled=enabled, latActive=lat_active)
      CC.hudControl.leftLaneVisible = True
      CC.hudControl.rightLaneVisible = True

      CI.update(CC, [], TOGGLES)
      _, can_sends = CI.apply(CC.as_reader(), 0, TOGGLES)

      messages = {message for message, _ in expected}
      parser = CANParser(DBC[candidate]["pt"], [(message, 0) for message in messages], 0)
      parser.update_strings([can_list_to_can_capnp(can_sends, msgtype="sendcan")], sendcan=True)

      for (message, signal), value in expected.items():
        assert parser.vl[message][signal] == value, (candidate, enabled, lat_active, message, signal)

  def test_lkas_button_forced(self):
    lfa_button_fingerprint = gen_empty_fingerprint()
    lfa_button_fingerprint[0][0x391] = 8

    for candidate, fingerprint, experimental_long, forced in (
      (CAR.HYUNDAI_IONIQ_5, gen_empty_fingerprint(), False, True),
      (CAR.HYUNDAI_IONIQ_5, gen_empty_fingerprint(), True, True),
      (CAR.HYUNDAI_SANTA_FE_2022, gen_empty_fingerprint(), False, False),
      (CAR.HYUNDAI_SANTA_FE_2022, gen_empty_fingerprint(), True, False),
      (CAR.HYUNDAI_SANTA_FE_2022, lfa_button_fingerprint, False, True),
      (CAR.HYUNDAI_SONATA, lfa_button_fingerprint, False, False),
      (CAR.HYUNDAI_SONATA, lfa_button_fingerprint, True, True),
    ):
      CarInterface, _, _ = interfaces[candidate]
      CP = CarInterface.get_params(candidate, fingerprint, [], experimental_long, TOGGLES, docs=False)
      FPCP = CarInterface.get_frogpilot_params(candidate, fingerprint, [], CP, TOGGLES)

      assert FPCP.alwaysOnLateralLKASButton == forced, (candidate, experimental_long)

  def test_main_cruise_dashboard(self):
    for always_on_lateral, main_cruise_on, main_mode in ((False, False, 1), (True, False, 0), (True, True, 1)):
      CarInterface, CarController, CarState = interfaces[CAR.HYUNDAI_SONATA]
      CP = CarInterface.get_params(CAR.HYUNDAI_SONATA, gen_empty_fingerprint(), [], True, TOGGLES, docs=False)
      FPCP = CarInterface.get_frogpilot_params(CAR.HYUNDAI_SONATA, gen_empty_fingerprint(), [], CP, TOGGLES)
      if always_on_lateral:
        CP.alternativeExperience |= ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL
      CI = CarInterface(CP, FPCP, CarController, CarState)

      CC = car.CarControl.new_message()
      CI.update(CC, [], TOGGLES)
      CI.CS.main_cruise_on = main_cruise_on
      _, can_sends = CI.apply(CC.as_reader(), 0, TOGGLES)

      parser = CANParser(DBC[CAR.HYUNDAI_SONATA]["pt"], [("SCC11", 0)], 0)
      parser.update_strings([can_list_to_can_capnp(can_sends, msgtype="sendcan")], sendcan=True)

      assert parser.vl["SCC11"]["MainMode_ACC"] == main_mode, (always_on_lateral, main_cruise_on)

  def test_main_cruise_reset(self):
    CarInterface, CarController, CarState = interfaces[CAR.HYUNDAI_SONATA]
    CP = CarInterface.get_params(CAR.HYUNDAI_SONATA, gen_empty_fingerprint(), [], True, TOGGLES, docs=False)
    FPCP = CarInterface.get_frogpilot_params(CAR.HYUNDAI_SONATA, gen_empty_fingerprint(), [], CP, TOGGLES)
    CS = CarState(CP, FPCP)

    assert not CS.update_main_cruise([0, 1])

    CS.car_safety_mode_set = True
    assert not CS.update_main_cruise([1])
    assert CS.update_main_cruise([0, 1, 0])

    CS.rx_checks_invalid = True
    assert not CS.update_main_cruise([0])
    assert CS.update_main_cruise([1])

    CS.rx_checks_invalid = False
    assert CS.update_main_cruise([0])

    CS.car_safety_mode_set = False
    assert not CS.update_main_cruise([0])

  def test_main_cruise_toggle(self):
    for candidate, experimental_long, always_on_lateral, toggles in (
      (CAR.HYUNDAI_SONATA, True, True, True),
      (CAR.HYUNDAI_SONATA, True, False, False),
      (CAR.HYUNDAI_SONATA, False, True, False),
      (CAR.HYUNDAI_IONIQ_5, True, True, True),
    ):
      CarInterface, CarController, CarState = interfaces[candidate]
      CP = CarInterface.get_params(candidate, gen_empty_fingerprint(), [], experimental_long, TOGGLES, docs=False)
      FPCP = CarInterface.get_frogpilot_params(candidate, gen_empty_fingerprint(), [], CP, TOGGLES)
      if always_on_lateral:
        CP.alternativeExperience |= ALTERNATIVE_EXPERIENCE.ALWAYS_ON_LATERAL
      CI = CarInterface(CP, FPCP, CarController, CarState)
      CI.CS.car_safety_mode_set = True

      packer = CANPacker(DBC[candidate]["pt"])
      if candidate in CANFD_CAR:
        message, bus, signal = CI.CS.cruise_btns_msg_canfd, CanBus(CP).ECAN, "ADAPTIVE_CRUISE_MAIN_BTN"
      else:
        message, bus, signal = "CLU11", 0, "CF_Clu_CruiseSwMain"

      available = []
      for main_button in (0, 1, 1, 0, 1, 0):
        msgs = [packer.make_can_msg(message, bus, {signal: main_button})]
        if candidate not in CANFD_CAR:
          msgs.append(packer.make_can_msg("SCC11", 0, {"MainMode_ACC": 1}))
        CS, _ = CI.update(car.CarControl.new_message(), [can_list_to_can_capnp(msgs)], TOGGLES)
        available.append(CS.cruiseState.available)

      expected = [False, True, True, True, False, False] if toggles else [True] * 6
      assert available == expected, (candidate, experimental_long, always_on_lateral)
