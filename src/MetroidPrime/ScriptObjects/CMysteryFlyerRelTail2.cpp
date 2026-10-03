// CMysteryFlyerRelTail2.cpp - MysteryFlyer's (module 45) out-of-line
// `rstl::optional_object<CAABox>` converting constructor, .text 0x2BBC..0x2BF8 (0x3C bytes, one
// function: `fn_45_2BBC`). A second unit in the same module, the arrangement `CLumiteRelTail.cpp`,
// `CFlyerSwarmRelTail.cpp` and `CBacteriaSwarmRelTail3.cpp` use - the head, `CMysteryFlyerRel.cpp`,
// claims `.text 0x0..0x170`, and the bytes between the two claims stay unclaimed, so dtk fills them
// from retail and the module's sha1 against `config/G2ME01/config.yml` still holds. One contiguous
// range per unit, as `RUNNING_THE_DECOMP.md` requires.
//
// What retail calls, read off `build/G2ME01/MysteryFlyer/asm/auto_00_00000170_text.s`:
//
//   0x2BBC fn_45_2BBC  0x3C  six words copied from the source to the destination, with the
//                          destination's valid flag set to 1 between the first load and the rest
//
// `CMysteryFlyerRel.cpp`'s `fn_45_10` (0x10) is the only caller in the module and it calls this by
// its dtk name, so the name here has to be `fn_45_2BBC`; that file declares it and must not define
// it. Its header records why the module's head cannot simply instantiate the template instead -
// doing so puts a trailing pool in the object and its own hash fails - so the layout is mirrored
// here in `COptionalAabox`: `uchar m_data[sizeof(T)]` followed by `bool m_valid ATTRIBUTE_ALIGN(4)`
// from `include/rstl/optional_object.hpp`, which for `sizeof(CAABox) == 0x18` (two `CVector3f`)
// puts the flag at +0x18, the offset retail's `stb` uses, and makes the object 0x1C bytes.
//
// **The body is two statements, and their order is what the compiler version then has to schedule
// around.** Measured here, one statement order per version, all with the module's own cflags and
// nothing else changed:
//
//   m_valid first, m_value second   1.3 / 1.3.2  `li; stb;` then the copy - 15 instructions, the
//                                                        `stb` in slot 1
//                                   2.0 / 2.5 / 2.6 / 2.7  `li; lwz; stb;` - retail's 15
//                                                        instructions, byte for byte
//                                   3.0a5.2  all six loads, then all seven stores - not retail
//   m_value first, m_valid second   1.3.2  the copy, then the `stb` in the last slot - and the copy
//                                    is memberwise (two `CVector3f`), so it is four loads into
//                                    r6/r5/r4 rather than retail's two alternating temporaries
//
// So `mw_version="GC/2.7"` on the `Rel("MysteryFlyer", ...)` entry in `configure.py`, which is the
// per-object override `CLumiteRelTail.cpp` and `CSandBossRelTail.cpp` already use, and 2.0/2.5/2.6
// would do the same for these bytes. This is a scheduling difference between two builds of the
// compiler family, the same class of thing `configure.py` records for `CGameOptions.cpp`: no source
// spelling reaches 1.3.2's order. A POD stand-in for `CAABox` (`float m_min[3]; float m_max[3];`)
// makes no difference, so the 24-byte copy is not what is version-sensitive - only where the flag
// store lands is.
//
// **The bodies are inside `#ifdef __MWERKS__` and the host branch is empty, so listing this file in
// `files.cmake` adds no undefined reference** - the arrangement `CLumiteRelTail.cpp` uses. There are
// no calls at all: the copy and the flag store are the whole of `fn_45_2BBC`.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only tools/flip_test.sh would catch it.
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CMysteryFlyerRelTail2.cpp`.

#include "Kyoto/Math/CAABox.hpp"

#ifdef __MWERKS__

// `rstl::optional_object<CAABox>`'s storage, spelled out so the constructor can be written here
// under retail's name. See `include/rstl/optional_object.hpp`: `uchar m_data[sizeof(T)]` then
// `bool m_valid ATTRIBUTE_ALIGN(4)`, so for `CAABox` (two `CVector3f`, 0x18 bytes) the flag lands
// at +0x18 and the object is 0x1C bytes.
class COptionalAabox {
public:
  CAABox m_value;
  bool m_valid;
};

extern "C" {
// .text 0x2BBC, 0x3C bytes. `optional_object(const CAABox&)`: mark it valid, then copy the box in.
void fn_45_2BBC(COptionalAabox* out, const CAABox& box) {
  out->m_valid = true;
  out->m_value = box;
}
}

#endif