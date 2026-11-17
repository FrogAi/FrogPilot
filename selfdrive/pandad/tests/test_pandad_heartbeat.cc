#define CATCH_CONFIG_MAIN

#include "catch2/catch.hpp"
#include "selfdrive/pandad/pandad.h"

TEST_CASE("heartbeat engaged") {
  REQUIRE(heartbeat_engaged(true, false, false));
  REQUIRE(heartbeat_engaged(true, false, true));
  REQUIRE(heartbeat_engaged(true, true, true));
  REQUIRE(heartbeat_engaged(false, true, false));
  REQUIRE_FALSE(heartbeat_engaged(false, false, false));
}

TEST_CASE("mismatch exit kept") {
  REQUIRE_FALSE(heartbeat_engaged(false, true, true));
  REQUIRE_FALSE(heartbeat_engaged(false, false, true));
}
