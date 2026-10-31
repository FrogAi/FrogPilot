#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"

#include <sys/mman.h>

#include <cstring>
#include <vector>

#include "frogpilot/ui/qt/egl_wayland_patch.h"

namespace {
const size_t IMAGE_SIZE = 0x17000;

struct Word {
  uint32_t offset;
  uint32_t patched;
  uint32_t stock;
};

// Every word that differs between the stock libeglSubDriverWayland.so (SHA-256 34b01105...) and agnos-builder's patched build (SHA-256 14d6f475...).
const Word CHANGED_WORDS[] = {
  {0x15c84, 0x9101e002, 0x3941e002},
  {0x15c88, 0x52800023, 0x121f7843},
  {0x15c8c, 0x38e31044, 0x3901e003},
  {0x15fac, 0x9101e26b, 0x3941e26b},
  {0x15fbc, 0x5280002c, 0x3200016c},
  {0x15fc0, 0x38ec3169, 0x3901e26c},
  {0x161ac, 0x9101e260, 0x3941e260},
  {0x161b8, 0x52800050, 0x121e7810},
  {0x161bc, 0x38f01001, 0x3901e270},
  {0x167e8, 0x9101e295, 0x3941e295},
  {0x167f0, 0x52800056, 0x321f02b6},
  {0x167f4, 0x38f632a1, 0x3901e296},
};

uint8_t *mapStockDriver() {
  uint8_t *image = static_cast<uint8_t *>(mmap(nullptr, IMAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
  memset(image, 0xa5, IMAGE_SIZE);

  for (const Word &word : CHANGED_WORDS) {
    memcpy(image + word.offset, &word.stock, sizeof(word.stock));
  }

  return image;
}
}

TEST_CASE("patchEglWayland/stock driver") {
  uint8_t *image = mapStockDriver();
  mprotect(image, IMAGE_SIZE, PROT_READ | PROT_EXEC);

  std::vector<uint8_t> expected(image, image + IMAGE_SIZE);
  for (const Word &word : CHANGED_WORDS) {
    memcpy(expected.data() + word.offset, &word.patched, sizeof(word.patched));
  }

  bool patched = patchEglWayland(reinterpret_cast<uintptr_t>(image));

  REQUIRE(patched);
  REQUIRE(memcmp(image, expected.data(), IMAGE_SIZE) == 0);
}

TEST_CASE("patchEglWayland/unknown driver") {
  for (const Word &word : CHANGED_WORDS) {
    uint8_t *image = mapStockDriver();
    image[word.offset] ^= 1;
    mprotect(image, IMAGE_SIZE, PROT_READ | PROT_EXEC);

    std::vector<uint8_t> before(image, image + IMAGE_SIZE);

    bool patched = patchEglWayland(reinterpret_cast<uintptr_t>(image));

    REQUIRE_FALSE(patched);
    REQUIRE(memcmp(image, before.data(), IMAGE_SIZE) == 0);
  }
}
