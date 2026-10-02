// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address and
// size come from `config/G2ME01/symbols.txt:9830` (`fn_80229EE8 = .text:0x80229EE8;
// type:function size:0xA8`), the 42 instructions are retail's own, as dtk listed them in
// `build/G2ME01/asm/auto_03_80229EE8_text.s` before this claim existed, and the body below is the
// C++ those bytes are the compilation of.
//
// .text 0x80229EE8..0x80229F90, 0xA8 = 168 bytes, 42 instructions, 1 function:
//
//   fn_80229EE8   0x80229EE8  0xA8  42  the four-argument initialiser of the data class that
//                                        `include/MetroidPrime/Enemies/CSwarmBasics.hpp:15`
//                                        forward-declares as `CBasicSwarmData`: it copies a
//                                        `CDamageInfo` member-wise, copy-constructs a
//                                        `CHealthInfo` at +0x1C and a `CDamageVulnerability` at
//                                        +0x3C, stores one word at +0xA8 and returns the object
//                                        (`mr r3, r29`, 0x80229F70).
//
// **Who calls it, and what the four arguments are.**  The caller is
// `LdrToBasicSwarmData__FRC24SLdrBasicSwarmProperties` (`symbols.txt:10094`, 0x80239C68, in dtk's
// `auto_03_802399F4_text`), the generated loader converter for `SLdrBasicSwarmProperties`
// (`include/MetroidPrime/ScriptLoader/Structs/SLdrBasicSwarmProperties.hpp`).  Its body is
// `build/G2ME01/asm/auto_03_802399F4_text.s:232-244`: it builds three locals - a `CDamageInfo` at
// r1+0x28 (`bl LdrToDamageInfo`, 0x8023B2F8, `symbols.txt:10113`), a `CHealthInfo` at r1+0x8
// (`bl LdrToHealthInfo`, 0x8023B2D0, `symbols.txt:10112`) and a `CDamageVulnerability` at r1+0x44
// (`bl LdrToDamageVulnerability`, line 232) - reads `lwz r7, 0x1bc(r31)` out of its properties
// block (line 239) and then calls this function (line 244) with `r3 = &the object under
// construction` (r1+0x74), `r4/r5/r6` those three locals and `r7` as read.  It then fills the rest
// of the object itself (retail +0x6C..+0xDC, lines 246-323) and copy-constructs its return value
// from it with the same class's copy constructor, `fn_80239E94` (0x80239E94, 0x190, line 324),
// which is this function's pattern extended to the whole class: the same member-wise `CDamageInfo`
// copy, the same two out-of-line copies at +0x1C and +0x3C, and then the members from +0x6C on.
//
// The class is the one the module loaders pass as the eighth argument of `CSwarmBasics`'s
// constructor (`CSwarmBasics.hpp:78-80`, `const CBasicSwarmData& data`): `FlyerSwarm`'s own loader
// builds it with `bl LdrToBasicSwarmData` and leaves its address in the stack slot that argument
// goes in (`build/G2ME01/FlyerSwarm/asm/auto_00_000000D8_text.s:208,241`), and `MetareeSwarm`,
// `PlantScarabSwarm`, `IngBlobSwarm`, `BacteriaSwarm` and `EmperorIngStage3` do the same.  So the
// layout below is the object, not a guess at one:
//
//   +0x00  CDamageInfo, 0x1C bytes  - copied member-wise because `CDamageInfo.hpp:81`'s
//                                     `CHECK_SIZEOF(CDamageInfo, 0x1c)` class declares no copy
//                                     constructor that fits, so MWCC inlines the copy: `lwz/stw`
//                                     for the 4-byte `CWeaponMode` at +0x00, four `lfs/stfs`
//                                     float pairs at +0x04/+0x08/+0x0C/+0x10, three `lhz/sth`
//                                     ushort pairs at +0x14/+0x16/+0x18 and `lbz/stb` for the two
//                                     flag bits sharing byte +0x1A.
//   +0x1C  CHealthInfo, 0x20     - `addi r3, r29, 0x1c` / `mr r4, r5` /
//                                  `bl __ct__11CHealthInfoFRC11CHealthInfo`: retail's copy
//                                  constructor at 0x80070D60, 0x54 bytes (`symbols.txt:2082`),
//                                  whose bytes sit inside `ScriptObjects/CScriptActor.cpp`'s
//                                  claim, so no unit of this carve defines them.
//   +0x3C  CDamageVulnerability, - `addi r3, r29, 0x3c` / `mr r4, r30` /
//          0x30                    `bl __ct__20CDamageVulnerabilityFRC20CDamageVulnerability`:
//                                  retail's copy at 0x8001C634, 0x5C bytes (`symbols.txt:510`),
//                                  inside `Player/CPlayer.cpp`'s claim
//                                  (0x8000B8E8..0x8001D0CC, `splits.txt`).
//   +0xA8  one 4-byte field      - `stw r31, 0xa8(r29)`, the fifth argument - the same
//                                  `lwz 0x1bc(r31)` the loader read out of its properties block,
//                                  which `LdrToBasicSwarmData` passes straight through.
//
// **The return value is measured, not assumed.**  The last instruction before the epilogue is
// `mr r3, r29` (0x80229F70), i.e. the function hands back the object it just initialised, which is
// why the definition below returns `CBasicSwarmData&` and ends `return *self;` - without the
// return statement MWCC emits the same 41 other instructions in the same order and omits exactly
// that `mr`.  The loader call site ignores the result (`auto_03_802399F4_text.s:245` loads r3
// again at once).
//
// The class below models the object only as far as this function touches it, the way the
// neighbouring `Carve*.c` units model theirs.  `mDamage` is the real `CDamageInfo` (0x1C in both
// builds, `CHECK_SIZEOF` above) and is copied by assignment, which is the member-wise copy retail's
// bytes are the compilation of.  `mHealthInfo` and `mVulnerability` are byte arrays of the
// measured sizes rather than their real types so that every offset is the same on the 64-bit host
// as in the 32-bit object: `CHealthInfo` is 0x20 on both, but `CDamageVulnerability` is 0x38 there
// rather than 0x30, because `rstl::vector` holds pointers.  This function only ever hands those two
// to their out-of-line copy constructors, and never reads a member of either.  `x6c_` is the run of
// members this function does not initialise (the loader writes them after the call) and is here
// only to put `xA8_` at retail's +0xA8.
//
// **The two callees need one host definition between them.**  `Matching` units reach the
// out-of-line copies by their mangled names through `extern "C"`, because that is what reproduces
// the `bl`; on the host those names bind to nothing.  `Carve8016FD4C.c` records the same for the
// `CHealthInfo` copy, and `src/Kyoto/Alloc/PortMwccNew.cpp` defines it for the host's link.
// `__ct__20CDamageVulnerabilityFRC20CDamageVulnerability` had no host definition until this carve,
// which adds one there next to it: a reference to a name nothing defines is a new undefined symbol,
// and `tools/link_check.sh --strict`, whose verdict `tools/probe_sources.sh` reports, fails on one.
//
// Source order is **descending by retail address** and that is load-bearing: mwcceppc emits
// function definitions in *reverse* source order and mwldeppc keeps the object's `.text` order
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function, a broken DOL and a
// module hash that fails on a few bytes.  Only `tools/flip_test.sh` catches that.  A one-function
// file cannot get it wrong.
//
// Retail names none of these.  `symbols.txt:9830` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim - `extern "C"`, so it is not mangled - because a C++ definition
// would come out as `_Z<len>fn_80229EE8...` and objdiff would pair nothing.  That is also why the
// unit is a `.cpp` with `extern "C"` rather than a plain C++ member definition.
//
// **Why its own unit, and what the split does.**  The dtk auto unit this comes out of is
// `auto_03_80229EE8_text` (`# 0x80229EE8..0x80229F90 | size: 0xA8`, one function - the 8 bytes in
// front of it were taken by the carve `Carve80229EE0.c`, and a claim may not span an unclaimed
// gap).  This carve claims all of what is left, so the auto unit disappears rather than being
// shortened.  Both neighbours are already claimed: `Carve80229EE0.c` ends exactly at 0x80229EE8
// (`splits.txt:1731-1732`) and `FlyerSwarm.cpp` starts exactly at 0x80229F90 (`splits.txt:1737-1739`).
//
// The directory is retail's own, taken from the nearest claimed range: `IngPuddle.cpp` owns
// 0x80229EB4..0x80229EE0 and this address is the code immediately after the 8-byte setter that
// follows it, so the code is that unit's neighbourhood.

#include "MetroidPrime/CDamageInfo.hpp"

/** Retail's own mangled name for `CHealthInfo`'s copy constructor (0x80070D60, 0x54 bytes).
 *  Declared `extern "C"` so the name stays unmangled and the `bl` is emitted against it; declared,
 *  never defined here - the bytes are inside `ScriptObjects/CScriptActor.cpp`'s claim, which is
 *  `NonMatching`, so dtk's own object supplies them in the DOL link. */
extern "C" void __ct__11CHealthInfoFRC11CHealthInfo(void* self, const void* src);

/** Retail's own mangled name for `CDamageVulnerability`'s copy constructor (0x8001C634, 0x5C
 *  bytes), reached the same way.  Declared, never defined here - the bytes are inside
 *  `Player/CPlayer.cpp`'s claim.  `src/Kyoto/Alloc/PortMwccNew.cpp` defines this one for the host's
 *  link, which is the only thing that keeps this `bl` from being a new undefined symbol there. */
extern "C" void __ct__20CDamageVulnerabilityFRC20CDamageVulnerability(void* self, const void* src);

/** The object's head, as far as this function initialises it - see the layout table above. */
struct CBasicSwarmData {
  CDamageInfo mDamage;                // +0x00, 0x1C bytes
  unsigned char mHealthInfo[0x20];    // +0x1C, CHealthInfo
  unsigned char mVulnerability[0x30]; // +0x3C, CDamageVulnerability
  unsigned char x6c_[0x3C];           // +0x6C..+0xA8, not touched here
  unsigned int xA8_;                  // +0xA8
};

extern "C" CBasicSwarmData& fn_80229EE8(CBasicSwarmData* self, const CDamageInfo* damage,
                                        const void* health, const void* vulnerability,
                                        unsigned int x);

extern "C" CBasicSwarmData& fn_80229EE8(CBasicSwarmData* self, const CDamageInfo* damage,
                                        const void* health, const void* vulnerability,
                                        unsigned int x) {
  self->mDamage = *damage;
  __ct__11CHealthInfoFRC11CHealthInfo(self->mHealthInfo, health);
  __ct__20CDamageVulnerabilityFRC20CDamageVulnerability(self->mVulnerability, vulnerability);
  self->xA8_ = x;
  return *self;
}
