// CEmperorIngStage1469C.cpp - EmperorIngStage1's (module 16) own copy of
// `rstl::vector<CJointCollisionDescription, rstl::rmemory_allocator>::push_back_unsafe`,
// `.text` 0x0000469C..0x000046D4: one function, 0x38 bytes.
//
// ## What this copy really is
//
// `twin_scan.py rel_example` paired this with the matched DOL function
// `push_back_unsafe__Q24rstl63vector<26CJointCollisionDescription,Q24rstl17rmemory_allocator>FRC26CJointCollisionDescription`
// (0x80137004, `build/G2ME01/asm/MetroidPrime/CCollisionActorManager.s:1647`) because the
// 56 bytes of instruction sequence agree. **It is not a twin of it: it is the same template
// instantiation, emitted into the module because the module's own object needs it**, and the
// module's asm says so four times over.
//
// 1. The element is `CJointCollisionDescription`, 0x68 bytes - `include/MetroidPrime/Collision/CJointCollisionDescription.hpp`
//    already asserts it (`CHECK_SIZEOF(CJointCollisionDescription, 0x68)`), and the module's own
//    copy constructor `fn_16_471C` (0x471C, 0xC0 bytes, `asm/auto_00_00000000_text.s:5035`)
//    walks exactly that header's member list: int +0x00, int +0x04, byte +0x08, byte +0x09
//    (the two `CSegId`), eight floats +0x0C..+0x2B (`CVector3f mBounds`, `CVector3f mPivotPoint`,
//    `mRadius`, `mMaxSeparation`), `rstl::string` +0x2C, half +0x3C (`mActorId`), float +0x40
//    (`mMass`), `CMatrix3f` +0x44 (`mOrientation`). Same 0xC0 bytes, same offsets as the DOL's
//    `__ct__26CJointCollisionDescriptionFRC26CJointCollisionDescription` at 0x801370F4.
// 2. The vector layout is retail's `rstl::vector`: allocator +0x00, `mCount` +0x04,
//    `mCapacity` +0x08, `mItems` +0x0C - which is what `fn_16_469C` reads
//    (`lwz r5,0x4(r3) / lwz r6,0xc(r3)`) and what `include/rstl/vector.hpp:95` reads.
// 3. The callee is this module's **own** three-level `rstl` construct chain, not the DOL's:
//      0x46D4 fn_16_46D4 0x20  `bl fn_16_46F4`                                 rstl::construct<CJointCollisionDescription>
//      0x46F4 fn_16_46F4 0x28  `cmplwi r3,0 / beq` then `bl fn_16_471C`        rstl::construct_impl<CJointCollisionDescription>
//      0x471C fn_16_471C 0xC0  the copy body above                           CJointCollisionDescription(const CJointCollisionDescription&)
//    The DOL's chain is 0x8013703C / 0x8013705C / 0x801370F4, same three shapes, same sizes.
//    The module keeps its own copies because it imports the *static* factories from the DOL
//    (`SphereCollision__26CJointCollisionDescription...` at 0x3F70 and 0x4148,
//    `OBBAutoSizeCollision__...` at 0x4420) and then has to copy their results
//    (`bl fn_16_471C` at 0x3F7C) into a local vector that is finally handed to
//    `__ct__22CCollisionActorManagerFR13CStateManager9TUniqueId7TAreaIdRCQ24rstl63vector<26CJointCollisionDescription,...>`
//    at 0x3FF4 / 0x41C0 / 0x44F0 as a `const rstl::vector<CJointCollisionDescription>&`.
//    Those three `CCollisionActorManager` ctor calls are also what proves the element type and
//    the container: nothing else in the module has a `push_back_unsafe` with a 0x68 stride.
//
// ## Why a local class and not the instantiation itself
//
// **Measured on 2026-10-02:** an explicit specialisation of a member of a class template -
//
//     namespace rstl { template <> void vector<CJointCollisionDescription, rmemory_allocator>::push_back_unsafe(
//                        const CJointCollisionDescription& in) { construct(mItems + mCount++, in); } }
//
// compiles clean under mwcceppc 1.3.2 and produces an **object with no `.text` at all** (a
// 408-byte `.o` whose only section is `.comment`). MWCC accepts the syntax and instantiates
// nothing, so there is no way to *ask* for this one template instantiation from this
// translation unit, and the out-of-line definition of `vector<T,Alloc>::push_back_unsafe` itself
// is only emitted when something calls it. The class below is therefore declared locally, with
// retail's own member offsets, and the body is retail's own body. It is the same arrangement
// `EmperorIngStage1Accessors.cpp` records for this module's accessors - state the offsets
// literally so the unit is layout-immune and nothing here is evidence about a class declaration
// that does not exist.
//
// The declaration that produced the shape, for the next module that has one of these:
//
//     class CEmperorIngStage1JointVector {
//       rstl::rmemory_allocator mAllocator;   // +0x00
//       int mCount;                           // +0x04
//       int mCapacity;                        // +0x08
//       CJointCollisionDescription* mItems;   // +0x0C
//     public:
//       void push_back_unsafe(const CJointCollisionDescription& in);
//     };
//     void CEmperorIngStage1JointVector::push_back_unsafe(const CJointCollisionDescription& in) {
//       fn_16_46D4(mItems + mCount++, in);
//     }
//
// The three things that are load-bearing, all measured on the 56 bytes:
//   - the callee is named, not inlined: `rstl::construct` is not reachable without also emitting
//     `construct<T>` and `construct_impl<T>`, which are the module's 0x46D4 and 0x46F4 and are
//     **not** in this claim. `fn_16_46D4` is that same function, and it is the only name the
//     module's own object can use.
//   - `mItems + mCount++` in that order. MWCC loads the count once into r5, multiplies by the
//     0x68 stride into r0, increments r5, stores it, then adds r0 into `mItems` and calls. It is
//     that schedule, not just the arithmetic, that the 56 bytes are.
//   - the element pointer arithmetic has to be through the member, so the stride comes from
//     `sizeof(CJointCollisionDescription)`. Hard-coding 0x68 with raw offsets compiles to
//     something else.
//
// **No dead-strip hazard, and that is measured.** The renamed symbol is not in
// `build/G2ME01/EmperorIngStage1/ldscript.lcf`'s FORCEACTIVE list, but `auto_00_00000000_text.o`
// calls it three times (+0x3F9C, +0x4174, +0x4470, each inside an unclaimed function), so dtk's
// own object holds the reference and this unit's `.text` survives the link - the situation
// `CIngBoostBallGuardianA91C.cpp` records for IngBoostBallGuardian. No `force_active:` needed.
//
// `config/G2ME01/rels/EmperorIngStage1/symbols.txt` renames `fn_16_469C` to the name mwcceppc
// gives the definition above, replacing its line rather than being inserted beside it (a dtk parse
// error). One rename, and no callee rename: `fn_16_46D4` keeps its retail name and the call
// matches the reference relocation by name. The three `bl fn_16_469C` sites in the auto object
// follow the rename, so they resolve against this unit's definition.
//
// Definitions are in descending retail text order (one function): mwcceppc emits definitions in
// reverse source order and mwldeppc keeps the object's `.text` order verbatim, so ascending would
// permute the module's bytes with objdiff still at 100%. `python3 tools/check_decl_order.py --unit
// CEmperorIngStage1469C` says ok.
//
// In `files.cmake` with an empty host branch, as `CIngBoostBallGuardianA91C.cpp` explains: the
// callee `fn_16_46D4` is module 16's `rstl::construct<CJointCollisionDescription>` and does not
// exist on the host, so listing this file without the `#ifdef` would add an undefined name to the
// port's link.

#ifdef __MWERKS__

#include "MetroidPrime/Collision/CJointCollisionDescription.hpp"
#include "rstl/rmemory_allocator.hpp"

// .text 0x46D4, 0x20 bytes: this module's `rstl::construct<CJointCollisionDescription>`, still
// retail, still carrying its dtk name. See the note above.
extern "C" void fn_16_46D4(void* dest, const CJointCollisionDescription& src);

class CEmperorIngStage1JointVector {
  rstl::rmemory_allocator mAllocator;
  int mCount;
  int mCapacity;
  CJointCollisionDescription* mItems;

public:
  void push_back_unsafe(const CJointCollisionDescription& in);
};

// .text 0x469C, 0x38 bytes. The module's `vector<CJointCollisionDescription>::push_back_unsafe`.
void CEmperorIngStage1JointVector::push_back_unsafe(const CJointCollisionDescription& in) {
  fn_16_46D4(mItems + mCount++, in);
}

#endif