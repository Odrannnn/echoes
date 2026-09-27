/* CMain layout probe.  Sizes and offsets go into .data so `objdump -s` can read them back:
 * the host compiler is 64-bit and rstl::reserved_vector is 64-bit there, so a host sizeof()
 * says nothing about the GameCube object.  See docs/RUNNING_THE_DECOMP.md.
 *
 * `__builtin_offsetof` does not exist in mwcceppc 2.7 (measured: "'(' expected"), so the
 * offsets come from the null-pointer-address idiom, which is what forces a real .data word.
 * CMain's members are private, so the probe opens the class before including it.
 */
#include "Kyoto/SObjectTag.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "rstl/reserved_vector.hpp"

#define private public
#include "MetroidPrime/CMain.hpp"
#undef private

#define OFF(m) (unsigned int)(size_t) & (((CMain*)0)->m)

extern "C" const unsigned int g_cmain_probe[] = {
  (unsigned int)sizeof(CMain),
  (unsigned int)sizeof(CMain::SFrameTimeHistory),
  OFF(osContext),
  OFF(x4_unk1),
  OFF(memorySys),
  OFF(xc_unk2),
  OFF(x10_unk),
  OFF(x18_frameTimeHistory),
  OFF(x2c_frameTimeHistory),
  OFF(x40_frameTimeTotal),
  OFF(x44_frameTimeTotal),
  OFF(frameTimeMinimum),
  OFF(x4c),
  OFF(x50),
  OFF(gameGlobalObjects),
  OFF(restartMode),
  OFF(x5c),
  OFF(frameTimes),
  OFF(frameTimeIdx),
  OFF(x164_),
};
