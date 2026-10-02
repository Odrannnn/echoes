// CSandBossRelTail3.cpp - SandBoss's (module 55) projectile-destructor chain, .text
// 0x10AE8..0x10C78: the four deleting destructors that sit above `fn_55_108E4`, the module's copy
// of `CPlasmaProjectile::~CPlasmaProjectile()`. They are one contiguous run of the module's
// unclaimed code and are written as free functions over offsets, the spelling
// `Carve80032774.cpp`/`TypesMatch.cpp` use for the same destructors in the DOL.
//
//   0x10AE8 fn_55_10AE8  0x60  `CBeamProjectile::~CBeamProjectile()`: stores the (imported) DOL
//                               vtable `__vt__15CBeamProjectile`, destroys its base through
//                               fn_55_10B48, then frees behind the flag
//   0x10B48 fn_55_10B48  0x7C  `CGameProjectile::~CGameProjectile()`: its own DOL vtable, the
//                               `CProjectileWeapon` subobject at +0x238 through the imported
//                               `__dt__17CProjectileWeaponFv`, the member at +0x1F8 through this
//                               module's own copy `fn_55_480C`, `CWeapon` at +0 by
//                               `__dt__7CWeaponFv`, then the flag's free
//   0x10BC4 fn_55_10BC4  0x54  the `rstl::vector<int>` member destructor at +0x598: the pointer
//                               at +0xC released, then the receiver behind the flag
//   0x10C18 fn_55_10C18  0x60  the module's own class's destructor: stores the module's own
//                               vtable `lbl_55_data_9AC`, destroys its base through fn_55_108E4
//                               (which stays retail's, below this claim), then the flag's free
//
// `fn_55_10B48` is the one function here whose *callees* are not all DOL imports: the member at
// +0x1F8 is a class whose destructor retail emitted out of line **inside this module** at
// 0x480C - the twin shape `__dt__19CStaticInterferenceFv` - so the call has to name the module's
// own copy or the bytes would carry a DOL address retail does not use. It is still given by the
// auto object (`build/G2ME01/SandBoss/obj/auto_00_00000178_text.o` defines `T fn_55_480C`) and
// stays unclaimed, so the name resolves; the same is true of `fn_55_108E4` for fn_55_10C18 and of
// the module's `.data` label `lbl_55_data_9AC` (the module's own vtable, 0x98 bytes), both defined
// by the auto objects at 0x108E4 and `.data:0x9AC`.
//
// Every body is read off `build/G2ME01/SandBoss/asm/auto_00_0001073C_text.s`, and every name is
// either the module's own from `config/G2ME01/rels/SandBoss/symbols.txt` or the DOL's, which the
// auto object already lists as undefined (`nm -u`: `__vt__15CBeamProjectile`,
// `__vt__15CGameProjectile`, `__dt__17CProjectileWeaponFv`, `__dt__7CWeaponFv`), so the REL has
// the imports.
//
// **The bodies are inside `#ifdef __MWERKS__` and the host branch is empty, so listing this file
// in `files.cmake` adds no undefined reference** - the arrangement `CSandBossRelTail.cpp` uses.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim.

#ifdef __MWERKS__

/** 0x802CE388, `symbols.txt`: `CMemory::Free(void const*)`. Declared under retail's own emitted
 *  spelling so the call needs no header. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** The two DOL vtables this chain stores, imported by the module. Retail materialises their
 *  address with `lis`/`addi`, so the declaration's element type is irrelevant. */
extern "C" void* __vt__15CGameProjectile[];
extern "C" void* __vt__15CBeamProjectile[];

/** The module's own class vtable at `.data:0x9AC`, defined by `auto_04_00000000_data.o`. */
extern "C" void* lbl_55_data_9AC[];

/** 0x108E4, 0x204: `CPlasmaProjectile::~CPlasmaProjectile()`, the module's copy, below this
 *  claim and still retail's. fn_55_10C18 destroys its base through it with the flag `0`. */
extern "C" void* fn_55_108E4(void* self, short flag);

/** 0x480C, 0x54: the module's own out-of-line destructor of the member at +0x1F8, defined by
 *  `auto_00_00000178_text.o`. Retail's `bl` in fn_55_10B48 names this copy, not a DOL one. */
extern "C" void* fn_55_480C(void* self, short flag);

/** The two imported base destructors fn_55_10B48 calls. */
extern "C" void* __dt__17CProjectileWeaponFv(void* self, short flag);
extern "C" void* __dt__7CWeaponFv(void* self, short flag);

/** 0x10C18, 0x60. The module's own class's deleting destructor: its vtable, its base through
 *  fn_55_108E4 with the flag `0`, then the receiver behind the positive deleting flag. */
struct SFn55_10C18 {
  void* x00_vtable;
};

extern "C" void* fn_55_10C18(SFn55_10C18* self, short flag) {
  if (self) {
    self->x00_vtable = (void*)lbl_55_data_9AC;
    fn_55_108E4(self, 0);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** 0x10BC4, 0x54. The `rstl::vector<int>` member destructor at +0x598; word for word
 *  `fn_55_4E40` of `CSandBossRelTail2.cpp` and the DOL's `fn_80032854`. */
extern "C" void* fn_55_10BC4(void* self, short flag) {
  if (self) {
    Free__7CMemoryFPCv(*(const void**)((unsigned char*)self + 0xC));
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** 0x10B48, 0x7C. `CGameProjectile::~CGameProjectile()`: the DOL vtable, then the three
 *  subobjects in retail's order - `CProjectileWeapon` at +0x238 (imported destructor), the
 *  member at +0x1F8 through this module's own `fn_55_480C`, `CWeapon` at +0 through the imported
 *  `__dt__7CWeaponFv` and the flag `0` - and the receiver behind the positive deleting flag. */
struct SFn55_10B48 {
  void* x00_vtable;
};

extern "C" void* fn_55_10B48(SFn55_10B48* self, short flag) {
  if (self) {
    self->x00_vtable = (void*)__vt__15CGameProjectile;
    __dt__17CProjectileWeaponFv((unsigned char*)self + 0x238, -1);
    fn_55_480C((unsigned char*)self + 0x1F8, -1);
    __dt__7CWeaponFv(self, 0);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** 0x10AE8, 0x60. `CBeamProjectile::~CBeamProjectile()`: the DOL vtable, its base through
 *  fn_55_10B48 with the flag `0`, then the receiver behind the positive deleting flag. */
struct SFn55_10AE8 {
  void* x00_vtable;
};

extern "C" void* fn_55_10AE8(SFn55_10AE8* self, short flag) {
  if (self) {
    self->x00_vtable = (void*)__vt__15CBeamProjectile;
    fn_55_10B48(reinterpret_cast< SFn55_10B48* >(self), 0);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

#endif
