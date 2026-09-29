// CIngSnatchingSwarmGenAccessors.cpp - three leaf accessors of the CParticleGen subclass that
// IngSnatchingSwarm (module 33) carries as a member, .text 0x4F28..0x4F44.
//
// From `config/G2ME01/rels/IngSnatchingSwarm/symbols.txt` and the module's own data section
// `build/G2ME01/IngSnatchingSwarm/asm/auto_04_00000000_data.s`:
//
//   .data:0x2E0  lbl_33_data_2E0, 0x8C = 35 words
//       words 0,1   0, 0                     (offset-to-top, RTTI - both zero in this REL)
//       word  14    fn_33_4F28               -> vtable index 12
//       word  22    fn_33_4F30               -> vtable index 20
//       word  24    fn_33_4F3C               -> vtable index 22
//       word  33    AddModifier__12CParticleGenFP5CWarp  -> vtable index 31
//
// **That table is `CParticleGen`'s, and the index arithmetic is what identifies the three
// functions.** `include/Kyoto/Particles/CParticleGen.hpp` declares, in order:
//
//   0 ~CParticleGen            11 SetGeneratorRate    22 GetDrawFlags
//   1 Update                  12 SetDrawFlags        23 ShouldDraw
//   2 Render                  13 GetOrientation      24 GetSystemCount
//   3 SetOrientation          14 GetTranslation      25 IsSystemDeletable
//   4 SetTranslation          15 GetGlobalOrientation 26 GetBounds
//   5 SetGlobalOrientation    16 GetGlobalTranslation 27 GetParticleCount
//   6 SetGlobalTranslation    17 GetGlobalScale      28 SystemHasLight
//   7 SetGlobalScale          18 GetParticleEmission 29 GetLight
//   8 SetLocalScale           19 GetModulationColor  30 DestroyParticles
//   9 SetParticleEmission     20 GetGeneratorRate    31 AddModifier
//  10 SetModulationColor      21 GetEmitterTime      32 Get4CharId
//
// Three of the four table entries this file claims line up with a header member exactly:
//
//   index 12 SetDrawFlags     header `virtual void SetDrawFlags(uint flags) { mDrawFlags = flags; }`
//   index 20 GetGeneratorRate header `virtual float GetGeneratorRate() const { return 1.f; }`
//   index 22 GetDrawFlags     header `virtual uint GetDrawFlags() const { return mDrawFlags; }`
//   index 31 AddModifier      named by the DOL's own symbol - the class really does derive from
//                             CParticleGen, and it is this module's own concrete subclass of it
//                             (the module instantiates it at `this + 0x158` in `fn_33_41CC`, and
//                             `fn_33_4AC4` at .text 0x4A1C is its copy constructor).
//
// The bodies are then forced by the header, not guessed:
//
//   fn_33_4F28  0x4F28  0x08  stw r4, 0x1c(r3) / blr                    -> mDrawFlags = flags
//   fn_33_4F30  0x4F30  0x0C  lis r3, lbl_33_rodata_30@ha /
//                          lfs f1, lbl_33_rodata_30@l(r3) / blr         -> return 1.f
//   fn_33_4F3C  0x4F3C  0x08  lwz r3, 0x1c(r3) / blr                    -> return mDrawFlags
//
// so the one fact the disassembly has to supply is the **member offset 0x1C**, and both the store
// and the load use it. `lbl_33_rodata_30` is the module's own `.rodata:0x30`, `size:0x4`,
// `.float 1` (`auto_03_00000000_rodata.s`) - the single float `GetGeneratorRate` returns, and it
// is in the ldscript's FORCEACTIVE list, so the reference resolves against the unclaimed
// `.rodata` object.
//
// **This unit claims .text only, and no class is instantiated**, so the object emits exactly the
// three functions and no vtable: the vtable itself is the retail bytes in the unclaimed
// `.data:0x2E0` object, and this file only has to provide the three symbols it names. That is why
// the class below is a stand-in carrying one member: a class with a defined virtual would emit a
// `__vt__` into `.data` and the module's sha1 would move. Same arrangement as
// `CIngSnatchingSwarmRel.cpp`'s `CIngSnatchingSwarmVTable`.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100%. Only `flip_test.sh` catches that.
//
// `tools/audit_rel_claim.py IngSnatchingSwarm` prints 0x00004F28..0x00004F44 3/3, and all three
// symbols are in `build/G2ME01/IngSnatchingSwarm/ldscript.lcf`'s FORCEACTIVE block, so nothing
// here is a dead-stripping hazard.

#include "types.h"

#ifdef __MWERKS__
// The float `GetGeneratorRate` returns is the module's `.rodata:0x30`, which this unit does not
// claim, so the reference has to be to the symbol the unclaimed `.rodata` object defines and not
// to a constant of our own - a literal would put a `.rodata` section in this object and move the
// module. `extern` under MWCC and a host definition, exactly as `lbl_33_bss_0` is handled in
// `CIngSnatchingSwarmRel.cpp`.
extern "C" {
extern const float lbl_33_rodata_30;
}
#else
extern "C" const float lbl_33_rodata_30 = 1.0f;
#endif

// The CParticleGen subclass this module defines, reduced to the one member the three accessors
// below touch. Its real declaration is this module's own and is not in the tree; naming the
// offset is what makes these three functions writable, and `0x1C` is measured from the store and
// the load in the disassembly above.
//
// **Member order is load bearing, and getting it wrong is what a first attempt did.** This class
// *derives from* CParticleGen, so the 0x1C bytes in front of `mDrawFlags` are CParticleGen's own
// layout - its vtable pointer, `rstl::list<CWarp*> mModifiersList` and the padding to the
// derived class's first member. Declaring `mDrawFlags` first and the padding after it put the
// member at offset 0, the object still compiled and linked, objdiff still saw the three
// functions, and the module's sha1 moved: `cmp -l` against retail showed `stw r4, 0x0(r3)`
// against `stw r4, 0x1c(r3)` and `lwz r3, 0x0(r3)` against `lwz r3, 0x1c(r3)` - two bytes, at
// 0x4FE7 and 0x4FFB of the `.rel` - and `dtk shasum` reported `IngSnatchingSwarm.rel: FAILED`
// with `86 files OK`. `char x_pad0[...]` is padding, which `tools/check_raw_offsets.py`
// deliberately does not count as a raw offset.
class CIngSnatchingSwarmGen {
private:
  char x_pad0[0x1C];

public:
  uint mDrawFlags;
};

extern "C" {
// .text 0x4F3C, 0x08 bytes - vtable index 22, `CParticleGen::GetDrawFlags()`.
uint fn_33_4F3C(CIngSnatchingSwarmGen* self) { return self->mDrawFlags; }

// .text 0x4F30, 0x0C bytes - vtable index 20, `CParticleGen::GetGeneratorRate()`.
float fn_33_4F30() { return lbl_33_rodata_30; }

// .text 0x4F28, 0x08 bytes - vtable index 12, `CParticleGen::SetDrawFlags()`.
void fn_33_4F28(CIngSnatchingSwarmGen* self, uint flags) { self->mDrawFlags = flags; }
}
