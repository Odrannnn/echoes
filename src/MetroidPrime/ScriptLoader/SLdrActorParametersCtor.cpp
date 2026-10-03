// `__ct__19SLdrActorParametersFv` - `SLdrActorParameters`'s constructor, DOL
// `.text:0x8023F534`, 0xB4 = 180 bytes, 45 instructions (`config/G2ME01/symbols.txt:10167`).
//
// **Carved out of the unclaimed dtk `auto_*` range, in four files in one change**: this source,
// the `Object(...)` line in `configure.py`, the claim in `config/G2ME01/splits.txt`, and the
// `files.cmake` line. It is the function immediately after the destructor the unit
// `MetroidPrime/ScriptLoader/SLdrActorParameters.cpp` claims (0x8023F4C4..0x8023F534), so the two
// are contiguous and this unit's claim, 0x8023F534..0x8023F5E8, is exactly what the destructor's
// unit ends at. Claiming it splits `auto_03_8023F534_text` (0x8023F534..0x80241C90, dtk's own
// object, 35 functions) into this unit and `auto_03_8023F5E8_text` (0x8023F5E8..0x80241C90, 34);
// `tools/gate.sh`'s per-function diff scores a split as a split, not a loss, and
// `total_functions` stays 28465 (measured before and after).
//
// **The generated header is already retail's layout, read off the destructor.** `+0x00`
// `lighting` (`bl fn_802405CC` on the receiver), `+0x40` `scannable` (`addi r3,r31,64`),
// `+0x44..+0x50` the four `CAssetId`s (`stw r0,68(r31)`..`stw r0,80(r31)`), `+0x54`
// `useGlobalRenderTime` (`stb r6,84(r31)`), `+0x58`/`+0x5C` the two floats (`stfs`), `+0x60`
// `visor` (`addi r3,r31,96`) and `+0x70`/`+0x74` the two volume caps. So the source passes
// `&self->lighting`, `&self->scannable` and `&self->visor` rather than a stand-in struct.
//
// **The four callees are called by retail's names, not as C++ member constructors.**
// `symbols.txt` gives `SLdrLightParameters`' and `SLdrVisorParameters'` constructors the `fn_`
// placeholders (`fn_802405CC` at 0x802405CC and `fn_8023F704` at 0x8023F704), so neither
// `__ct__22SLdrLightParametersFv` nor `__ct__22SLdrVisorParametersFv` exists in the binary and
// writing `lighting.SLdrLightParameters()` would emit a name mwldeppc cannot resolve. Only
// `SLdrScannableParameters`' (`__ct__23SLdrScannableParametersFv`, 0x8024156C) and `CColor`'s
// (`__ct__6CColorFffff`, 0x803207FC) are named; of those two only the `CColor` one is claimed, by
// the `Matching` unit `Kyoto/Graphics/DolphinCColor.cpp`. All four are declared here and none is
// defined here for the matching build.
//
// **The return type is `SLdrActorParameters*` and that is load-bearing, together with
// `return self;`.** Retail's bytes carry `mr r3,r31` at 0x8023F5A4, in the middle of the tail
// store block and not in the epilogue - MWCC hoisted the return-value move above the last stores
// because `r3` is otherwise dead there. Measured, all three spellings: as `void` the body is
// 176 bytes (4 short of the claim) and allocates `r5`/`r4`/`r3` to `true`/`15`/`false`; with the
// pointer return but no `return` (mwcceppc warns "return value expected") it is *still* 176,
// because a non-void type alone emits no move; with `return self;` it is 180 and `r3` is taken,
// so the constants move to `r6`/`r5`/`r4` and every following instruction matches.
//
// **The float is retail's own pool entry, not a literal this unit emits.** Both `lfs` are
// `lbl_8041DC88@sda21(r2)` (`-18232(r2)`, `_SDA2_BASE_` = 0x804223C0 minus 0x8041DC88), the
// `.float 1` dtk recorded at `build/G2ME01/asm/auto_11_8041DAC8_sdata2.s:517-519` and named in
// `symbols.txt:24598`. Letting mwcceppc emit its own `.sdata2` copy would add a second
// definition of an address retail already has inside dtk's own `auto_11_8041DAC8_sdata2.o` and
// move every `.sdata2` address above it - the same reason `Carve8023E5A8.c` declares
// `lbl_8041DCB0` instead of writing `0.f`.
//
// **Two of the stores look redundant and are not removable.** `fn_8023F704` is an opaque call
// that already wrote 15 at `visor + 4`, and `fn_802405CC` already wrote `CColor::Green()` at
// `lighting + 0x14`, yet retail writes 15 at `+0x64` and a `CColor(1.f,1.f,1.f,1.f)` at `+0x14`
// after both calls. MWCC cannot see through either callee, so it keeps both stores, and it needs
// both to order them exactly as retail does: the `CColor` goes through a stack temporary at
// `sp+8` and is copied back with one `lwz`/`stw` pair, the 15 goes straight into `r5`.
//
// Source order is **descending by address** and load-bearing (mwcceppc emits definitions in
// reverse source order and mwldeppc keeps the object's `.text` verbatim). This unit has one
// function, so the point does not arise here; `tools/check_decl_order.py --unit
// main/main/MetroidPrime/ScriptLoader/SLdrActorParametersCtor` is the cheap check.

#include "Kyoto/Graphics/CColor.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrActorParameters.hpp"

/** 0x8041DC88, `symbols.txt:24598`, `.sdata2`, 4 bytes: dtk recorded `.float 1`
 *  (`build/G2ME01/asm/auto_11_8041DAC8_sdata2.s:517-519`).  Retail's `__ct__` reads it twice, into
 *  `f1` for the `CColor` arguments and into `f0` for the two fade times.  The matching build takes
 *  the real bytes from dtk's own `auto_11_8041DAC8_sdata2.o`; declared, never defined here. */
extern const float lbl_8041DC88;

/** 0x802405CC, `symbols.txt:10188`, 0xC4 = 196 bytes: retail's
 *  `SLdrLightParameters::SLdrLightParameters()`, which `symbols.txt` carries as the `fn_`
 *  placeholder `fn_802405CC`.  Unclaimed, so dtk's own object defines it for this link;
 *  declared, never defined here for the matching build. */
extern "C" void fn_802405CC(SLdrLightParameters* self);

/** 0x8024156C, `symbols.txt:10206`, 0xC = 12 bytes: retail's own
 *  `__ct__23SLdrScannableParametersFv`.  Unclaimed; declared, never defined here. */
extern "C" void __ct__23SLdrScannableParametersFv(SLdrScannableParameters* self);

/** 0x8023F704, `symbols.txt:10170`, 0x14 = 20 bytes: retail's
 *  `SLdrVisorParameters::SLdrVisorParameters()`, the `fn_` placeholder again.  Unclaimed;
 *  declared, never defined here. */
extern "C" void fn_8023F704(SLdrVisorParameters* self);

/** 0x803207FC, `symbols.txt:14638`, 0x60 = 96 bytes: retail's own
 *  `CColor::CColor(float,float,float,float)`.  Claimed by the `Matching` unit
 *  `Kyoto/Graphics/DolphinCColor.cpp`; declared, never defined here. */
extern "C" void __ct__6CColorFffff(CColor* self, float r, float g, float b, float a);

/** `__ct__19SLdrActorParametersFv` - retail `.text:0x8023F534`, 0xB4 = 180 bytes: the three
 *  member constructors, the four `CAssetId` defaults, and the `useGlobalRenderTime` /
 *  `takesProjectedShadow` / `maxVolume` / `maxEchoVolume` defaults.
 *  `extern "C"` is what keeps `__ct__19SLdrActorParametersFv` unmangled, which is the name
 *  `symbols.txt:10167` carries, so objdiff pairs it against retail's own. */
extern "C" SLdrActorParameters* __ct__19SLdrActorParametersFv(SLdrActorParameters* self) {
  fn_802405CC(&self->lighting);
  __ct__23SLdrScannableParametersFv(&self->scannable);
  self->darkModel = kInvalidAssetId;
  self->darkSkin = kInvalidAssetId;
  self->echoModel = kInvalidAssetId;
  self->echoSkin = kInvalidAssetId;
  fn_8023F704(&self->visor);
  self->lighting.ambientColor = CColor(lbl_8041DC88, lbl_8041DC88, lbl_8041DC88, lbl_8041DC88);
  self->useGlobalRenderTime = true;
  self->fadeInTime = lbl_8041DC88;
  self->fadeOutTime = lbl_8041DC88;
  self->visor.visorFlags = 15;
  self->isHighlightedInDarkVisor = false;
  self->forceRenderUnsorted = false;
  self->takesProjectedShadow = true;
  self->unknown_0xf07981e8 = false;
  self->unknown_0x6df33845 = false;
  self->maxVolume = 127;
  self->maxEchoVolume = 127;
  return self;
}

#ifndef __MWERKS__
// Four host-only stand-ins, empty bodies, and they **are** empty: nothing in the port calls them
// except the constructor above, which nothing calls either. Three of the four - `fn_802405CC`,
// `fn_8023F704`, `__ct__23SLdrScannableParametersFv` - are unclaimed retail functions no unit of
// ours implements at all; the fourth, `__ct__6CColorFffff`, is implemented by the `Matching` unit
// `Kyoto/Graphics/DolphinCColor.cpp` but under its C++ name, so the *retail* name this unit
// references still has no definition in the port's flat link. None of the four is in
// `docs/research/port_link_gap_list.md`, so defining them keeps the port's undefined count where it
// is instead of adding four (measured: 287 undefined, 0 duplicates, same as the branch head).
// `__dt__23SLdrScannableParametersFv` needs no stand-in: the destructor unit
// `MetroidPrime/ScriptLoader/SLdrActorParameters.cpp` defines that one for the host, and a second
// definition would be a duplicate.
// The guard is `__MWERKS__`, not `TARGET_PC`, for the reason
// `src/MetroidPrime/Cameras/Carve801E7C14.c` records: the matching build must take all four from
// elsewhere - three from dtk's own `auto_03_8023F5E8_text.o` (the range above this claim) and the
// `CColor` from `Kyoto/Graphics/DolphinCColor.cpp` - and a second definition there would be a
// duplicate.
// They are deliberately not logged at run time: they are not reachability stubs, they are the
// host's stand-ins for four functions this tree has not decompiled under retail's own names yet.
void fn_802405CC(SLdrLightParameters* self) {
  (void)self;
}

void fn_8023F704(SLdrVisorParameters* self) {
  (void)self;
}

void __ct__23SLdrScannableParametersFv(SLdrScannableParameters* self) {
  (void)self;
}

void __ct__6CColorFffff(CColor* self, float r, float g, float b, float a) {
  (void)self;
  (void)r;
  (void)g;
  (void)b;
  (void)a;
}

/** Retail's `.float 1` pool entry at 0x8041DC88, `symbols.txt:24598`.  Retail's own value, dtk's
 *  own `.sdata2` object in the matching build; the port's flat link carries our sources and not
 *  dtk's objects, so without this the linker asks for a name nothing defines.  Same arrangement
 *  as `lbl_8041DCB0` in `MetroidPrime/ScriptLoader/Carve8023E5A8.c`. */
const float lbl_8041DC88 = 1.f;
#endif