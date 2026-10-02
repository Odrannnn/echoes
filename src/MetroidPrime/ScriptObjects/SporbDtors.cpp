// SporbDtors.cpp - a carve of Sporb's (module 76) destructor chain and the three functions above
// its neighbour, .text 0x00011B34..0x00011D14: seven adjacent functions and nothing else. Sizes and
// names from `config/G2ME01/rels/Sporb/symbols.txt`:
//
//   0x11B34 fn_76_11B34 0x7C  the base class's deleting destructor: stores `__vt__15CGameProjectile`
//                              over the object, destroys +0x238 through the imported
//                              `__dt__17CProjectileWeaponFv`, +0x1F8 through this module's
//                              unclaimed `fn_76_5334`, then `__dt__7CWeaponFv` with flag 0.
//   0x11BB0 fn_76_11BB0 0x70  three members at +0xA0, +0x5C and +0x18, each through fn_76_11C20.
//   0x11C20 fn_76_11C20 0x58  one member at +0x8 through fn_76_11C78.
//   0x11C78 fn_76_11C78 0x54  `CMemory::Free` of the pointer at +0xC, then itself on flag > 0.
//   0x11CCC fn_76_11CCC 0x1C  the three floats at +0x584..+0x58F, copied to the hidden return
//                              pointer.
//   0x11CE8 fn_76_11CE8 0x0C  the flag byte at +0xC cleared.
//   0x11CF4 fn_76_11CF4 0x20  `CEnergyProjectile::Render`, reached through the module's own
//                              thunk and calling the imported DOL implementation.
//
// The four destructors are the shape `MetroidPrime/ScriptObjects/CFogOverlayRel.cpp` (fn_23_0) and
// `MetroidPrime/ScriptObjects/CFlyerSwarmRelTail.cpp` (fn_21_192C, fn_21_18D4) already reproduce at
// 100% in other modules, so the three load-bearing details are theirs and not measured again here:
// the flag is a **`short`** (`int` gives `cmpwi r31,0` where retail has `extsh. r0,r31`), the
// return type is a **pointer** (a `void` leaf loses the trailing `mr r3,r30`), and the flag handed
// to a member/base teardown is the literal `0` or `-1` rather than a computed value. The class
// hierarchy is not modelled: no header here declares these types, and the vptr stores are written
// by hand, because a class whose first non-inline virtual is defined in this translation unit is
// the one that emits a `__vt__`, and an emitted `.data` object the split does not claim moves the
// module's sha1. This is the arrangement `CFogOverlayRel.cpp` records for the same reason.
//
// `fn_76_5334` (0x5334, 0x54) is outside this claim and stays retail: named in
// `config/G2ME01/rels/Sporb/symbols.txt`, declared and never defined here. Three more callees are
// imported from the DOL and are spelled with the names the module's own import table carries
// (`build/G2ME01/Sporb/Sporb.preplf`): `__vt__15CGameProjectile`, `__dt__17CProjectileWeaponFv`,
// `__dt__7CWeaponFv`, `Render__17CEnergyProjectileCFRC13CStateManager` and `Free__7CMemoryFPCv`
// (reached as `CMemory::Free`). No rename in `symbols.txt` is needed: the names this object exports
// and the names the module already had are the same strings.
//
// `fn_76_11B34` stores a vtable that is **not** the module's own and defines no virtual, so it
// emits no `.data` of its own and nothing here is a dead-stripping hazard: retail already reaches
// all seven functions from `fn_76_11A50` and `fn_76_11AB0`, which stay in `auto_00_0000FE64_text`
// and are in the module's `ldscript.lcf` FORCEACTIVE list. No `force_active:` entry is needed in
// `config/G2ME01/config.yml`.
//
// Definitions are in descending retail text order, because mwcceppc emits definitions in reverse
// source order and mwldeppc keeps the object's `.text` order verbatim; ascending would permute the
// module's bytes with objdiff still at 100%. `python3 tools/check_decl_order.py --unit` does not
// resolve a path out of a `Rel(...)` block and `flip_test.sh` cannot find a REL unit's source, so
// the module's sha1 against `config/G2ME01/config.yml` is the acceptance test here.
//
// The file is listed in `files.cmake`, and **its host branch defines nothing at all** - the same
// arrangement `CFlyerSwarmRelTail.cpp` uses. `__dt__17CProjectileWeaponFv`, `__dt__7CWeaponFv`,
// `fn_76_5334` and `Render__17CEnergyProjectileCFRC13CStateManager` are names the flat host link
// cannot resolve, so a host definition would grow the port's undefined count for no gain.

#include "Kyoto/Alloc/CMemory.hpp"
#include "types.h"

#ifdef __MWERKS__

extern "C" {
// The imported vtable, by the name the module's import table carries.
extern char __vt__15CGameProjectile[];

// The imported DOL functions, likewise. `fn_76_5334` (0x5334) is this module's own and is left
// retail: declared, never defined.
void __dt__17CProjectileWeaponFv(void* self, short flag);
void __dt__7CWeaponFv(void* self, short flag);
void Render__17CEnergyProjectileCFRC13CStateManager(void* self, const void* mgr);
void* fn_76_5334(void* self, short flag);

// This unit's own three chain links below, in the order the calls reach them.
void* fn_76_11BB0(void* self, short flag);
void* fn_76_11C20(void* self, short flag);
void* fn_76_11C78(void* self, short flag);

// .text 0x11CF4, 0x20 bytes. `CEnergyProjectile::Render` reached through this module's own thunk:
// the frame, one forwarded call, and the epilogue. Retail's callee is the DOL's
// `Render__17CEnergyProjectileCFRC13CStateManager`, which is in the module's import table, so the
// call is direct and the arguments pass straight through in r3/r4 - there is no virtual dispatch
// here and the class is not modelled.
//
//   00000000  94 21 FF F0  stwu r1,-0x10(r1)
//   00000010  7C 08 02 A6  mflr  r0
//   00000014  90 01 00 14  stw  r0,0x14(r1)
//   00000018  48 00 07 35  bl   Render__17CEnergyProjectileCFRC13CStateManager
//   0000001C  80 01 00 14  lwz  r0,0x14(r1)
//   00000020  7C 08 03 A6  mtlr  r0
//   00000024  38 21 00 10  addi r1,r1,0x10
//   00000028  4E 80 00 20  blr
void fn_76_11CF4(void* self, const void* mgr) {
  Render__17CEnergyProjectileCFRC13CStateManager(self, mgr);
}

// .text 0x11CE8, 0x0C bytes. The flag byte at +0xC cleared.
//
//   00000000  38 00 00 00  li   r0,0x0
//   00000004  98 03 00 0C  stb  r0,0xC(r3)
//   00000008  4E 80 00 20  blr
//
// The zero is materialised in r0 and not in r3, which is what a store of a literal through a
// `void*` receiver gives here.
void fn_76_11CE8(void* self) {
  *reinterpret_cast< unsigned char* >(static_cast< char* >(self) + 0xC) = 0;
}

// .text 0x11CCC, 0x1C bytes. The three floats at +0x584, +0x588 and +0x58F copied to the hidden
// return pointer. **The same shape as `fn_76_370`**, this module's already-`Matching` accessor at
// 0x000370 (`MetroidPrime/ScriptObjects/SporbAccessors.cpp`), which is the reference for the
// spelling: `out` is the hidden return pointer, `self` arrives in r4, and each value is read and
// stored as a float.
//
//   00000000  C0 04 05 84  lfs  f0,0x584(r4)
//   00000004  D0 03 00 00  stfs f0,0x0(r3)
//   00000008  C0 04 05 88  lfs  f0,0x588(r4)
//   0000000C  D0 03 00 04  stfs f0,0x4(r3)
//   00000010  C0 04 05 8C  lfs  f0,0x58C(r4)
//   00000014  D0 03 00 08  stfs f0,0x8(r3)
//   00000018  4E 80 00 20  blr
void fn_76_11CCC(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x584);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0x11C78, 0x54 bytes. The leaf of the chain: `Free` of the pointer the record holds at
// +0xC, then itself when the flag is positive.
//
//   00000000  94 21 FF F0  stwu r1,-0x10(r1)
//   00000010  7F E3 FB 78  mr   r30,r3
//   00000014  41 82 00 1C  beq  <epilogue>      ; the `if (this)` guard
//   00000018  80 7E 00 0C  lwz  r3,0xC(r30)      ; the member pointer
//   0000001C  48 00 07 99  bl   Free__7CMemoryFPCv
//   00000020  7F E0 07 35  extsh. r0,r31        ; `if (flag > 0)`, the SHORT flag
//   00000024  40 81 00 0C  ble  <epilogue>
//   00000028  7F C3 F3 78  mr   r3,r30
//   0000002C  48 00 07 89  bl   Free__7CMemoryFPCv
//   ...                      epilogue, with `mr r3,r30` before `lwz r31` / `lwz r30`
void* fn_76_11C78(void* self, short flag) {
  if (self != nullptr) {
    CMemory::Free(*reinterpret_cast< void** >(reinterpret_cast< char* >(self) + 0xC));
    if (flag > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// .text 0x11C20, 0x58 bytes. One member, at +0x8, through `fn_76_11C78` with the "destroy, do not
// free" flag; itself on a positive flag. The same two-call shape as
// `CFlyerSwarmRelTail.cpp`'s `fn_21_18D4`, with this module's offset in place of the receiver.
void* fn_76_11C20(void* self, short flag) {
  if (self != nullptr) {
    fn_76_11C78(reinterpret_cast< char* >(self) + 0x8, -1);
    if (flag > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// .text 0x11BB0, 0x70 bytes. Three members at +0xA0, +0x5C and +0x18, each through `fn_76_11C20`
// with flag -1, in descending offset order; itself on a positive flag. `fn_76_11C20`'s own guard
// is what absorbs the null check for each of them, so no test appears here.
void* fn_76_11BB0(void* self, short flag) {
  if (self != nullptr) {
    fn_76_11C20(reinterpret_cast< char* >(self) + 0xA0, -1);
    fn_76_11C20(reinterpret_cast< char* >(self) + 0x5C, -1);
    fn_76_11C20(reinterpret_cast< char* >(self) + 0x18, -1);
    if (flag > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// .text 0x11B34, 0x7C bytes. The base class's deleting destructor: `__vt__15CGameProjectile` over
// the object, the member at +0x238 through the imported `__dt__17CProjectileWeaponFv`, the member
// at +0x1F8 through this module's `fn_76_5334`, then `__dt__7CWeaponFv` with flag 0.
//
//   00000000  3C 80 00 00  lis  r4,__vt__15CGameProjectile@ha
//   00000004  38 7E 02 38  addi r3,r30,0x238
//   00000008  38 04 00 00  addi r0,r4,__vt__15CGameProjectile@l
//   0000000C  38 80 FF FF  li   r4,-1
//   00000010  90 1E 00 00  stw  r0,0x0(r30)     ; the derived vptr, by hand
//   00000014  48 00 08 CD  bl   __dt__17CProjectileWeaponFv
//   ...                       +0x1F8 / fn_76_5334, then this / __dt__7CWeaponFv / 0
//   00000058  7F E0 07 35  extsh. r0,r31        ; `if (flag > 0)`, the SHORT flag
//   0000005C  40 81 00 0C  ble  <epilogue>
//   00000060  7F C3 F3 78  mr   r3,r30
//   00000064  48 00 08 A5  bl   Free__7CMemoryFPCv
//   ...                       epilogue, with `mr r3,r30` before `lwz r31` / `lwz r30`
//
// `mwcceppc` lays the two halves out the way `~CIOWin` does: the `lis` in one register, and the
// first member's address computed in between, so the `lis` is hoisted above the argument setup and
// the `addi ...@l` lands after it. The vtable's high half shares r4 with the flag argument, which
// is only materialised after the store.
void* fn_76_11B34(void* self, short flag) {
  if (self != nullptr) {
    *reinterpret_cast< void** >(self) = __vt__15CGameProjectile;
    __dt__17CProjectileWeaponFv(reinterpret_cast< char* >(self) + 0x238, -1);
    fn_76_5334(reinterpret_cast< char* >(self) + 0x1F8, -1);
    __dt__7CWeaponFv(self, 0);
    if (flag > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}
}

#endif // __MWERKS__