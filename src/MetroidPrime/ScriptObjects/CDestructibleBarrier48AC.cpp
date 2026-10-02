// CDestructibleBarrier48AC.cpp - DestructibleBarrier's (module 13) two small class destructors,
// .text 0x48AC..0x49BC (0x110 = 272 bytes, two functions). A second unit in the same module, the
// same arrangement as `CAtomicAlpha7E0.cpp`: the head, `CDestructibleBarrierRel.cpp`, claims
// `.text 0x0..0xA0`, everything between the two ranges stays unclaimed, so dtk fills it from
// retail and the module's sha1 against `config/G2ME01/config.yml` still holds.
//
//   0x48AC  fn_13_48AC  0x64  the twin - a two-word `rstl::auto_ptr<COBBTree>` deleting
//                              destructor
//   0x4910  fn_13_4910  0xAC  `COBBTree`'s own deleting destructor, the twin of
//                              `__dt__Q28COBBTree10SIndexDataFv` (src/WorldFormat/COBBTree.cpp)
//
// **The first is a twin.** `tools/twin_scan.py` pairs fn_13_48AC with
// `__dt__Q24rstl32auto_ptr<20CScannableObjectInfo>Fv` in `src/MetroidPrime/Factories/
// CScannableObjectInfo.cpp` (100 bytes, matched) - the same instructions apart from the two `bl`
// targets. It is *not* that function: the shape is the deleting destructor of a two-word
// `rstl::auto_ptr<T>`, so the T differs. This copy's own `.rela.text` names its callee
// `__dt__8COBBTreeFv`, so its T is `COBBTree`, which is what the declaration below says. The
// identical shape built inside another module is `CAtomicAlpha7E0.cpp` (module 2), whose T is
// `CAnimData`.
//
// Range from `config/G2ME01/rels/DestructibleBarrier/symbols.txt`. The neighbours left retail are
// fn_13_4770 (0x4770, 0x13C) above and fn_13_49BC (0x49BC, 0x54) below.
//
// What fn_13_48AC's bytes are, read off
// `build/G2ME01/DestructibleBarrier/asm/auto_00_000000A0_text.s:5195`:
//
//   +0x00  mHas    `lbz r0,0(r30) / cmplwi r0,0 / beq` guards the delete
//   +0x04  mItem   `lwz r3,4(r30) / li r4,1 / bl __dt__8COBBTreeFv` - `delete mItem`
//   then `extsh. r0,r31 / ble` on the flag and `mr r3,r30 / bl Free__7CMemoryFPCv` - the
//   destructor's own `delete this` half, MWCC's `operator delete` being `CMemory::Free`
//   (`include/Kyoto/Alloc/CMemory.hpp:46`) - with the receiver returned in r3 for the caller.
//
// The declaration is the one `CAtomicAlpha7E0.cpp` measured, under the module's own flags
// (`GC/1.3.2`, `-O4,p -inline auto -inline deferred,noauto`):
//     if (self != 0) {
//       if (self->mHas) { __dt__8COBBTreeFv(self->mItem, 1); }
//       if (flag > 0)  { Free__7CMemoryFPCv(self); }
//     }
//     return self;
//
//   - **The flag parameter has to be a `short`.** Written `int` - which is what a hand-written
//     free function reaches for - the flag test compiles to `cmpwi r31,0` where retail has
//     `extsh. r0,r31`. That one instruction is the whole difference (measured both ways); the
//     register choice, the `mr r31,r4` *before* the `mr. r30,r3`, and the two exits are identical
//     either way.
//   - **The definition keeps the C name** `fn_13_48AC`, because this range's `symbols.txt` entry is
//     the dtk placeholder `fn_13_48AC`, while an out-of-line member destructor would emit the
//     *mangled* `__dt__<whatever the class is called>Fv` and pair with nothing in objdiff. That is
//     the rule `src/MetroidPrime/Cameras/Carve801E7C14.c` states for the same situation.
//   - **`COBBTree` stays forward-declared and the callee is named.** `delete self->mItem` reads
//     better, but it needs `WorldFormat/COBBTree.hpp`, and with that include the object grows
//     bytes its claim does not cover. The explicit call is what `delete` compiles to
//     (`lwz r3,4(r30) / li r4,1 / bl`), and the object then holds `.text` and `.comment` only.
//
// fn_13_4910 is the same statement sequence with eight member destructors in front of the flag
// test (see its own comment). Both functions are in one claim because they are adjacent, which
// is the "one contiguous range per unit" rule `docs/RUNNING_THE_DECOMP.md` states; the 0xA0..0x48AC
// and 0x49BC..0x7358 bytes around them stay unclaimed.
//
// This file is listed in `files.cmake`, and **its host branch defines nothing at all** - the same
// arrangement `CAtomicAlpha7E0.cpp` uses, and the reason is the one `tools/check_files_cmake.py`
// enforces: a `Matching` object has to be either in `files.cmake` or in the checker's judge-owned
// EXCLUDED set, and the only automatic exemption is a unit that defines RELMain/RELExit, which
// this one deliberately does not. An empty host branch keeps the port's undefined count where it
// is while the MWCC branch is the retail source and reproduces retail's bytes.

class COBBTree;

extern "C" {

// The DOL's `COBBTree` deleting destructor, `symbols.txt`'s `__dt__8COBBTreeFv`: the call
// `delete mItem` makes, with the delete flag in r4. Declared, never defined here.
void __dt__8COBBTreeFv(COBBTree* self, int flag);

// Retail's `CMemory::Free(void const*)`; the delete half of this destructor. Declared, never
// defined here. `Kyoto/Alloc/CMemory.hpp:46` is where MWCC reaches it from `operator delete`.
void Free__7CMemoryFPCv(const void* ptr);

// The module's own member destructors, named as `config/G2ME01/rels/DestructibleBarrier/
// symbols.txt` names them, so the eight `bl` relocations resolve against the module. Each takes the
// member's address and the delete flag in r4. Declared, never defined here: their ranges stay
// unclaimed, so dtk fills them from retail.
void fn_13_4B0C(void* self, int flag);

void fn_13_4AB8(void* self, int flag);

void fn_13_4A64(void* self, int flag);

void fn_13_4A10(void* self, int flag);

void fn_13_49BC(void* self, int flag);

#ifdef __MWERKS__

// One 0x10 member of the object fn_13_4910 walks. The bytes give the offsets and the sizes
// (COBBTree is 0x80 per `include/WorldFormat/COBBTree.hpp:116`, and the walk reaches +0x10, +0x20,
// +0x30, +0x40, +0x50, +0x60 and +0x70), not what each member holds; the classes those members are
// really are declared in `include/WorldFormat/COBBTree.hpp` and are not needed to name the
// offsets, so they are not included here.
struct CDBBTreeMember {
  unsigned char mBytes[0x10];
};

// The 0x80-byte object fn_13_4910 destroys, in the order the walk visits it: the member at +0x00
// (fn_13_49BC), then +0x10, +0x20 and +0x30 (fn_13_4A10, three of them), +0x40 (fn_13_4A64), +0x50
// and +0x60 (fn_13_4AB8, two of them), and +0x70 (fn_13_4B0C). Three of the five destructors are
// called on more than one member, so this is the layout the seven `addi r3,r30,N` / `bl` pairs
// spell out.
struct CDestructibleBarrierCOBBTree {
  CDBBTreeMember mAt00;
  CDBBTreeMember mAt10;
  CDBBTreeMember mAt20;
  CDBBTreeMember mAt30;
  CDBBTreeMember mAt40;
  CDBBTreeMember mAt50;
  CDBBTreeMember mAt60;
  CDBBTreeMember mAt70;
};

// .text 0x4910, 0xAC = 172 bytes. `COBBTree`'s own deleting destructor
// (`symbols.txt`'s `fn_13_4910`; `include/WorldFormat/COBBTree.hpp:116` sizes the class at
// 0x80). Same 0x10 frame and same `mr r31,r4` before `mr. r30,r3` as fn_13_48AC below, and the
// same `extsh. r0,r31` on the flag; what it adds is the member-destruction walk, which MWCC emits
// from the highest member down to the base. Each member's destructor is named here rather than
// left to a real member class, for the reason the flag argument note gives: this range's
// `symbols.txt` entry is the placeholder `fn_13_4910`, not the mangled `__dt__8COBBTreeFv`.
void* fn_13_4910(CDestructibleBarrierCOBBTree* self, short flag) {
  if (self != 0) {
    fn_13_4B0C(&self->mAt70, -1);
    fn_13_4AB8(&self->mAt60, -1);
    fn_13_4AB8(&self->mAt50, -1);
    fn_13_4A64(&self->mAt40, -1);
    fn_13_4A10(&self->mAt30, -1);
    fn_13_4A10(&self->mAt20, -1);
    fn_13_4A10(&self->mAt10, -1);
    fn_13_49BC(&self->mAt00, -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

// The two words the bytes show: the flag at +0 the destructor tests, the owned pointer at +4 the
// destructor deletes. `rstl::auto_ptr<T>`'s own layout, with T = `COBBTree` for this copy.
struct CDestructibleBarrierCOBBTreePtr {
  bool mHas;
  COBBTree* mItem;
};

// .text 0x48AC, 0x64 = 100 bytes. `self` in r3, the delete flag in r4; returns `self`.
void* fn_13_48AC(CDestructibleBarrierCOBBTreePtr* self, short flag) {
  if (self != 0) {
    if (self->mHas) {
      __dt__8COBBTreeFv(self->mItem, 1);
    }
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

#endif
}
