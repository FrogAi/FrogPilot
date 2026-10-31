#include "frogpilot/ui/qt/egl_wayland_patch.h"

#include <sys/mman.h>
#include <unistd.h>

#include <cassert>

#ifdef QCOM2
#include <link.h>

#include <cstring>

#include "common/swaglog.h"
#endif

namespace {
struct Instruction {
  uint32_t offset;
  uint32_t original;
  uint32_t replacement;
};

// From agnos-builder's userspace/patch_egl_wayland.py, without its CommitBuffer null-pointer fix, which the installed library already has.
// File offsets equal virtual addresses in this library's .text section.
const Instruction PATCHES[] = {
  // Callback: atomically clear callback-pending bit 0 at surface + 0x78.
  {0x15c84, 0x3941e002, 0x9101e002},  // ldrb w2, [x0, #0x78] -> add x2, x0, #0x78
  {0x15c88, 0x121f7843, 0x52800023},  // and w3, w2, #0xfffffffe -> mov w3, #1
  {0x15c8c, 0x3901e003, 0x38e31044},  // strb w3, [x0, #0x78] -> ldclralb w3, w4, [x2]

  // CommitBuffer: atomically set callback-pending bit 0.
  {0x15fac, 0x3941e26b, 0x9101e26b},  // ldrb w11, [x19, #0x78] -> add x11, x19, #0x78
  {0x15fbc, 0x3200016c, 0x5280002c},  // orr w12, w11, #1 -> mov w12, #1
  {0x15fc0, 0x3901e26c, 0x38ec3169},  // strb w12, [x19, #0x78] -> ldsetalb w12, w9, [x11]

  // SwapBuffers: atomically clear buffer-acquired bit 1.
  {0x161ac, 0x3941e260, 0x9101e260},  // ldrb w0, [x19, #0x78] -> add x0, x19, #0x78
  {0x161b8, 0x121e7810, 0x52800050},  // and w16, w0, #0xfffffffd -> mov w16, #2
  {0x161bc, 0x3901e270, 0x38f01001},  // strb w16, [x19, #0x78] -> ldclralb w16, w1, [x0]

  // DequeueBuffer: atomically set buffer-acquired bit 1.
  {0x167e8, 0x3941e295, 0x9101e295},  // ldrb w21, [x20, #0x78] -> add x21, x20, #0x78
  {0x167f0, 0x321f02b6, 0x52800056},  // orr w22, w21, #2 -> mov w22, #2
  {0x167f4, 0x3901e296, 0x38f632a1},  // strb w22, [x20, #0x78] -> ldsetalb w22, w1, [x21]
};

#ifdef QCOM2
int patchLoadedDriver(dl_phdr_info *object, size_t, void *) {
  if (!strstr(object->dlpi_name, "libeglSubDriverWayland")) {
    return 0;
  }

  if (patchEglWayland(object->dlpi_addr)) {
    LOG("egl wayland patch: made libeglSubDriverWayland's surface flag updates atomic");
  } else {
    LOGW("egl wayland patch: libeglSubDriverWayland differs from the known build, left unchanged");
  }

  return 1;
}
#endif
}

bool patchEglWayland(uintptr_t base) {
  for (const Instruction &instruction : PATCHES) {
    if (*reinterpret_cast<const uint32_t *>(base + instruction.offset) != instruction.original) {
      return false;
    }
  }

  size_t page_size = getpagesize();

  for (const Instruction &instruction : PATCHES) {
    uintptr_t address = base + instruction.offset;
    void *page = reinterpret_cast<void *>(address & ~(page_size - 1));

    int ret = mprotect(page, page_size, PROT_READ | PROT_WRITE | PROT_EXEC);
    assert(ret == 0);

    *reinterpret_cast<uint32_t *>(address) = instruction.replacement;

    ret = mprotect(page, page_size, PROT_READ | PROT_EXEC);
    assert(ret == 0);

    __builtin___clear_cache(reinterpret_cast<char *>(address), reinterpret_cast<char *>(address + sizeof(uint32_t)));
  }

  return true;
}

void patchEglWayland() {
#ifdef QCOM2
  if (dl_iterate_phdr(patchLoadedDriver, nullptr) == 0) {
    LOGW("egl wayland patch: libeglSubDriverWayland isn't loaded, left unchanged");
  }
#endif
}
