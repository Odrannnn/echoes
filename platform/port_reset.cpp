/**
 * The port's reset-request flag, in a translation unit that is actually built.
 *
 * **This file exists because of a latent link bug, and the bug is worth stating precisely
 * because every instrument the project owns missed it.**
 *
 * `platform/sdk_stubs.cpp` is in `mp_platform` and calls `PortDebug::RequestReset()`. The only
 * definition of it was in `platform/debug_ui.cpp`, which **is not in `CMakeLists.txt`** - it is
 * the debug UI, and pulling it in would drag imgui and the whole menu into the shipping binary.
 * So the reference had no definition.
 *
 * Why nothing caught it: `MP_SDK_HEADERS_ONLY=ON` - the configuration `tools/link_check.sh` and
 * therefore `tools/gate.sh` link - uses a *different source list*, and in that configuration the
 * reference does not exist. The configuration the port will actually ship in,
 * `MP_SDK_HEADERS_ONLY=OFF`, has it. And `tools/boot_probe.sh` is the only thing that builds that
 * configuration, so the failure appeared there and **nowhere else**: `port link gap` passed,
 * `port link dups` reported 0, and the undefined count did not include it.
 *
 * That is the same shape as two other failures this session, and it is worth noticing they are
 * the same shape: **a configuration-dependent gap that only one instrument builds.** The three
 * were the `TARGET_PC` host definitions in the `CGraphics` carves, the `PortReachStubs.cpp`
 * staleness, and this. The general check is cheap: build the *shipping* configuration, not only
 * the one the gate happens to use.
 *
 * Both functions and the flag live here rather than in `debug_ui.cpp` so that adding the debug UI
 * to the build later cannot produce a duplicate definition. `debug_ui.cpp` reads the flag through
 * the `extern` below.
 */
#include "port_debug.h"

namespace PortDebug {

bool sResetRequested = false;

void RequestReset() { sResetRequested = true; }

bool ConsumeResetRequest() {
  const bool requested = sResetRequested;
  sResetRequested = false;
  return requested;
}

} // namespace PortDebug
