// CPirateRagDollLayout.cpp - PirateRagDoll's (module 50) char-layout node lookups, .text
// 0xC2C..0xD80: three functions, the run of adjacent twins at the tail of the module's
// `auto_00_0000010C_text` unit.
//
//   0xC2C fn_50_C2C  0x30  `CQuaternion::BuildInverted()` - the scalar through, the three
//                              imaginary components negated
//   0xC5C fn_50_C5C  0xF0  `CCharLayoutInfo::GetFromParentUnrotated(const CSegId&)`
//   0xD4C fn_50_D4C  0x34  `TSegIdMap<CCharLayoutNode>::ContainsDataFor(const CSegId&)`
//
// Every body is read off `build/G2ME01/PirateRagDoll/asm/auto_00_0000010C_text.s` and every
// name is the module's own, from `config/G2ME01/rels/PirateRagDoll/symbols.txt`, because
// objdiff pairs a unit's functions by name and the module's symbols are `fn_50_*`: a
// `CQuaternion::BuildInverted` member definition here would emit the mangled
// `_ZNK11CQuaternion13BuildInvertedEv`, which objdiff cannot pair with `fn_50_C2C` **and** which
// the DOL's own `MetroidPrime/CAnimData.cpp` already defines. So each of the three is a free
// function under the module's own name, which is the arrangement
// `MetroidPrime/ScriptObjects/CSandBossRelTail.cpp` and `CPirateRagDollCross.cpp` already use in
// this repo.
//
// **What the bodies are, from the twins** (`tools/twin_scan.py --list`, all three byte-identical
// apart from the `bl` target):
//   - `fn_50_C2C` = `BuildInverted__11CQuaternionCFv`, matched in `main/MetroidPrime/CAnimData`
//     (`.text 0x80028E08`, 0x30). Same class, same inline `CQuaternion(float,float,float,float)`
//     constructor, so the same four `lfs` / three `fneg` / four `stfs` and the same r3 = return
//     slot, r4 = receiver.
//   - `fn_50_C5C` = `GetFromParentUnrotated__15CCharLayoutInfoCFRC6CSegId`, matched in
//     `main/MetroidPrime/CRagDoll`. `CCharLayoutInfo::GetFromParentUnrotated` is an inline member
//     of `include/Kyoto/Animation/CCharLayoutInfo.hpp` and this is its code instruction for
//     instruction, with the call retargeted to `fn_50_D4C`.
//   - `fn_50_D4C` = `ContainsDataFor__28TSegIdMap<15CCharLayoutNode>CFRC6CSegId`, also matched
//     in `main/MetroidPrime/CRagDoll`. It is the inline member of
//     `include/Kyoto/Animation/TSegIdMap.hpp`.
//
// **The two mirror structs, and why they are not raw offsets.** Both bodies read exactly one
// member of a class this file cannot reach - `CCharLayoutInfo::mNodes` and
// `TSegIdMap::mIndirectionMap` - and both are `private`. The mirrors below declare those two
// members with their *real* types (`rstl::object_owner<TSegIdMap<CCharLayoutNode>>`,
// `rstl::reserved_vector<rstl::pair<CSegId,CSegId>,100>`) in retail's member order, so every
// expression below them is the tree's own inline code: `TSegIdMap::operator[]`,
// `TSegIdMap::ContainsDataFor`, `rstl::pair::operator!=`, `CCharLayoutNode::GetParent`,
// `CVector3f::operator-`. Only the two private member accesses are spelled through the mirror.
// The layouts are the tree's own and are checked by the header's own
// `CHECK_SIZEOF(TSegIdMapSizeCheck, 0xd8)`: `mIndirectionMap`'s storage lands at +0x8 (count at
// +0x4), which is the `lbzu r0, 0x8(r3)` retail indexes off, and `mNodes` at +0xD0, which is
// the `lwz r5, 0xd0(r5)` of `fn_50_C5C`.
//
// **No dead-strip hazard, and the obvious fix for one is a trap here.**
// `build/G2ME01/PirateRagDoll/ldscript.lcf`'s FORCEACTIVE block holds `fn_50_74`, `fn_50_4F8`,
// `fn_50_DC0`, `fn_50_E10`, `fn_50_1610`, `fn_50_1874`, `fn_50_1938`, `fn_50_2500`, `fn_50_298C`
// and `fn_50_2EE4` - and **not** `fn_50_C2C`, `fn_50_C5C` or `fn_50_D4C`, which reads like the
// ScriptCoin dead-strip case. **Adding a `force_active:` list to module 50 in
// `config/G2ME01/config.yml` does not work and was reverted**: PirateRagDoll then hashes, but
// DarkCommando, CommandoPirate, DarkTrooper and SpacePirate all stop, each by **one** byte -
// `0x38` becomes `0x6C` at file offset 0xAC5B of each - because those four are the modules that
// `bl fn_50_1938`, module 50's only export, and the list shifts the position of that name in
// module 50's string table by 52 bytes. So **`force_active:` is not free in a module other
// modules import from**, which is a case the ScriptCoin measurement did not cover.
// None is needed: dtk's split leaves `auto_00_0000010C_text` (now 0x10C..0xC2C, down from
// 0x10C..0xD80) with **undefined references** to `fn_50_C2C` and `fn_50_C5C` -
// `powerpc-eabi-nm -u` on `build/G2ME01/PirateRagDoll/obj/auto_00_0000010C_text.o` prints both -
// because it is the range *below* 0xC2C that calls them, and `fn_50_4F8` is in FORCEACTIVE, so
// those references are live. `fn_50_D4C` is reached from `fn_50_C5C`'s own `bl`. Nothing is
// stripped; `tools/flip_test.sh MetroidPrime/ScriptObjects/CPirateRagDollLayout.cpp` is what
// proves it, and `tools/audit_rel_claim.py PirateRagDoll` prints the preplf/plf symbol counts.
//
// **The range.** `fn_50_C2C`, `fn_50_C5C` and `fn_50_D4C` are adjacent (0xC2C + 0x30 = 0xC5C,
// 0xC5C + 0xF0 = 0xD4C, 0xD4C + 0x34 = 0xD80) and 0xD80 is where the next claim,
// `MetroidPrime/ScriptObjects/CPirateRagDollCross.cpp`, starts, so one unit claims
// .text 0xC2C..0xD80 and everything below it - `fn_50_10C`, `fn_50_3B4`, `fn_50_4F8` - stays
// unclaimed and is filled from retail by dtk, which is what keeps the module's sha1 against
// `config/G2ME01/config.yml` holding.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only `tools/flip_test.sh` would catch it.

#include "Kyoto/Animation/CCharLayoutInfo.hpp"
#include "Kyoto/Animation/CSegId.hpp"
#include "Kyoto/Animation/TSegIdMap.hpp"
#include "Kyoto/Math/CQuaternion.hpp"
#include "Kyoto/Math/CVector3f.hpp"

#include "rstl/object_owner.hpp"
#include "rstl/pair.hpp"
#include "rstl/reserved_vector.hpp"

/** `TSegIdMap<T>`'s member order (include/Kyoto/Animation/TSegIdMap.hpp), which is private
 *  wholesale. Retail indexes the pair storage at +0x8 and the node array at +0xD0. The header's
 *  own `CHECK_SIZEOF(TSegIdMapSizeCheck, 0xd8)` is the check that this mirror is the same size as
 *  the real thing. */
struct SSegIdMapLayout {
  uchar mBoneCount;
  uchar mCapacity;
  uchar x02_pad[2];
  rstl::reserved_vector< rstl::pair< CSegId, CSegId >, 100 > mIndirectionMap;
  uchar* mNodes;
  CSegId mCurPrevBone;
};
CHECK_SIZEOF(SSegIdMapLayout, 0xd8);

/** `CCharLayoutInfo`'s first member (include/Kyoto/Animation/CCharLayoutInfo.hpp), private; the
 *  only one these three functions reach. Retail loads it with `lwz r5, 0x0(r4)`, and it is
 *  `rstl::object_owner<TSegIdMap<CCharLayoutNode>>`, which is a single `T*`. */
struct SCharLayoutInfoLayout {
  rstl::object_owner< TSegIdMap< CCharLayoutNode > > mNodes;
};

extern "C" {
/** .text 0xD4C, 0x34. `TSegIdMap<T>::ContainsDataFor`:
 *
 *      return mIndirectionMap[id.val()] != rstl::pair<CSegId,CSegId>(CSegId::Null(), CSegId::Null());
 *
 *  which is why retail compares against **0x63** twice - `CSegId::Null()` is `CSegId(99)` - and
 *  why the second compare is `lbz r0, 0x1(r3)` off the `lbzu`'s already-advanced r3 rather than
 *  a second address computation. The receiver is r3 and the `CSegId&` r4. */
bool fn_50_D4C(const SSegIdMapLayout* self, const CSegId& id) {
  return self->mIndirectionMap[id.val()] !=
         rstl::pair< CSegId, CSegId >( CSegId::Null(), CSegId::Null() );
}

/** .text 0xC5C, 0xF0. `CCharLayoutInfo::GetFromParentUnrotated`:
 *
 *      const CCharLayoutNode& node = GetSegmentData(id);
 *      const CSegId parent = node.GetParent();
 *      return !mNodes->ContainsDataFor(parent) ? node.GetReferenceStanceOffset()
 *                                              : node.GetReferenceStanceOffset() -
 *                                                    GetSegmentData(node.GetParent())
 *                                                        .GetReferenceStanceOffset();
 *
 *  r3 is the hidden `CVector3f` return slot (kept in r31), r4 the receiver and r5 the `CSegId&`.
 *  Both `GetSegmentData` lookups index the 0x40-byte node array with `slwi r0, r0, 6`, and the
 *  ternary's two arms are an **address** choice - `addi r5, r30, 0x4` against a 0xC-byte stack
 *  temporary at r1+0xc - with the copy to the return slot common to both, which is what the
 *  `CVector3f` return by value gives.
 *
 *  Two things here are measured rather than stylistic.
 *
 *  1. The `ContainsDataFor` test is a **direct call to `fn_50_D4C`**, not
 *     `self->mNodes->ContainsDataFor(parent)`. Written the second way, mwcceppc declines to inline
 *     the template member and emits an out-of-line **weak** copy under its own mangled name,
 *     `ContainsDataFor__28TSegIdMap<15CCharLayoutNode>CFRC6CSegId`, which the `bl` then targets.
 *     That is a *fourth* 0x34-byte function inside a 0x154-byte claim, so the module's `.text`
 *     came out 0x3500 against retail's 0x34CC - 52 bytes long - and its sha1 stopped matching.
 *     The call is the same call either way: r3 is the map, r4 the stack `CSegId` at r1+0x8, and
 *     `powerpc-eabi-objdump -r -j .text` on the object shows the relocation's target is
 *     `fn_50_D4C`.
 *  2. `mNodes` is spelled `rstl::object_owner<TSegIdMap<CCharLayoutNode>>`, so
 *     `(*self->mNodes)[id]` is `operator*` then `operator[]`, exactly as
 *     `CCharLayoutInfo::GetSegmentData` spells it. With a bare
 *     `TSegIdMap<CCharLayoutNode>*` member instead, the same source matched the other two
 *     functions exactly and left this one's first eight instructions different (0xF0, 31 bytes
 *     out), so the ownership wrapper is load-bearing on the argument-setup schedule. */
CVector3f fn_50_C5C(const SCharLayoutInfoLayout* self, const CSegId& id) {
  const CCharLayoutNode& node = (*self->mNodes)[id];
  const CSegId parent = node.GetParent();
  return !fn_50_D4C( reinterpret_cast< const SSegIdMapLayout* >( self->mNodes.operator->() ), parent )
             ? node.GetReferenceStanceOffset()
             : node.GetReferenceStanceOffset() -
                   (*self->mNodes)[node.GetParent()].GetReferenceStanceOffset();
}

/** .text 0xC2C, 0x30. `CQuaternion::BuildInverted`:
 *
 *      return CQuaternion(w, -imaginary.GetX(), -imaginary.GetY(), -imaginary.GetZ());
 *
 *  `CQuaternion`'s (float,float,float,float) constructor is inline in
 *  include/Kyoto/Math/CQuaternion.hpp and mem-initialises `w` then `imaginary`, which is the
 *  store order retail has: the scalar at +0x0 first, then +0x4, +0x8, +0xC. */
CQuaternion fn_50_C2C(const CQuaternion& q) {
  return CQuaternion( q.GetScalar(), -q.AxisX(), -q.AxisY(), -q.AxisZ() );
}
} // extern "C"
