// CAtomicAlpha7E0.cpp - AtomicAlpha's (module 2) `rstl::auto_ptr<CAnimData>` deleting
// destructor, .text 0x7E0..0x844 (0x64 = 100 bytes). A second unit in the same module, the same
// arrangement as `CLumiteRelTail.cpp`: the head, `CAtomicAlphaRel.cpp`, claims `.text
// 0x0..0x13C`, everything between the two ranges stays unclaimed, so dtk fills it from retail and
// the module's sha1 against `config/G2ME01/config.yml` still holds.
//
// **This is the twin answer.** `tools/twin_scan.py` pairs this function with
// `__dt__Q24rstl32auto_ptr<20CScannableObjectInfo>Fv` in `src/MetroidPrime/Factories/
// CScannableObjectInfo.cpp` (100 bytes, matched) - the same instructions apart from the two `bl`
// targets and the data addresses. It is *not* that function: the shape is the deleting destructor
// of a two-word `rstl::auto_ptr<T>`, so the T differs. **Of the 63 copies of this shape that sit
// in REL modules, 59 call `__dt__9CAnimDataFv`** and 2 call `__dt__12CActorLightsFv` (measured;
// `python3 tools/twin_scan.py --list` lists the copies, and each copy's two relocation targets
// are in its object's `.rela.text`). This copy calls `__dt__9CAnimDataFv`, so its T is
// `CAnimData`, which is what the declaration below says.
//
// Range from `config/G2ME01/rels/AtomicAlpha/symbols.txt`. The neighbours left retail are
// fn_2_720 (0x720, 0x6C), fn_2_78C (0x78C, 0x54) above and fn_2_844 (0x844, 0x60), fn_2_8A4
// (0x8A4, 0x7C) below - the module's other small class destructors, each also a
// `__dt(self, flag)` deleting destructor with its own member at its own offset.
//
// What the bytes are, read off `build/G2ME01/AtomicAlpha/asm/auto_00_0000013C_text.s`:
//
//   +0x00  mHas    `lbz r0,0(r30) / cmplwi r0,0 / beq` guards the delete
//   +0x04  mItem   `lwz r3,4(r30) / li r4,1 / bl __dt__9CAnimDataFv` - `delete mItem`
//   then `extsh. r0,r31 / ble` on the flag and `mr r3,r30 / bl Free__7CMemoryFPCv` - the
//   destructor's own `delete this` half, MWCC's `operator delete` being `CMemory::Free`
//   (`include/Kyoto/Alloc/CMemory.hpp:46`) - with the receiver returned in r3 for the caller.
//
// **The declaration that produced this shape, measured in this run** under the module's own flags
// (`GC/1.3.2`, `-O4,p -inline auto -inline deferred,noauto`): this file's object
// `build/G2ME01/src/MetroidPrime/ScriptObjects/CAtomicAlpha7E0.o` and the unit's retail-derived
// target object `build/G2ME01/AtomicAlpha/obj/MetroidPrime/ScriptObjects/CAtomicAlpha7E0.o` are
// both `.text` 0x64 and byte-identical once the two `R_PPC_REL24` words at +0x34 and +0x44 are
// masked - and those words are the same two symbols in both objects, so the whole 0x64 matches:
//
//     if (self != 0) {
//       if (self->mHas) { __dt__9CAnimDataFv(self->mItem, 1); }
//       if (flag > 0)  { Free__7CMemoryFPCv(self); }
//     }
//     return self;
//
//   - **The flag parameter has to be a `short`.** Written `int` - which is what a hand-written
//     free function reaches for - the flag test compiles to `cmpwi r31,0` where retail has
//     `extsh. r0,r31`. That one instruction is the whole difference (measured both ways); the
//     register choice, the `mr r31,r4` *before* the `mr. r30,r3`, and the two exits are identical
//     either way. `extsh.` is the `short` type, so the flag is a `short` - the same reading
//     `src/MetroidPrime/Cameras/Carve801E7C14.c` records for its own chain.
//   - **The same body as an out-of-line member destructor is byte-identical too** (`class X { bool
//     mHas; CAnimData* mItem; ~X(); };` with `~X() { if (mHas) { delete mItem; } }` measured at
//     100 bytes, same instructions), but it emits the *mangled* `__dt__<class>Fv` for whatever
//     the class is called, and this range's `symbols.txt` entry is the dtk placeholder
//     `fn_2_7E0`: a mangled definition would pair with nothing in objdiff. That is the rule
//     `Carve801E7C14.c` states for the same situation ("retail names none of these, so
//     `symbols.txt` carries the `fn_<addr>` placeholder and this file reproduces that symbol
//     verbatim"), and it is why the definition below keeps the C name.
//   - **The template's own mangled name is not reachable from a unit that claims one function.**
//     MWCC does not emit `rstl::auto_ptr<CAnimData>`'s out-of-line deleting destructor from a
//     declaration: `template class rstl::auto_ptr<CAnimData>;` and an explicit member
//     specialization (`template <> rstl::auto_ptr<CAnimData>::~auto_ptr() { ... }`) both emit no
//     `.text` at all (measured), and a "use" that forces the call site to inline the body
//     (`void use(rstl::auto_ptr<CAnimData>* p) { delete p; }`) emits no
//     `__dt__Q24rstl20auto_ptr<9CAnimData>Fv` either - it inlines the body and leaves only the
//     two callees undefined. `-inline deferred` therefore leaves nothing to name, and carrying an extra
//     function to provoke the instantiation would put bytes in the object that this claim does
//     not cover. The class is declared here instead, as the two words the bytes show.
//   - **`CAnimData` stays forward-declared and the callee is named.** `delete self->mItem` reads
//     better, but it needs `MetroidPrime/CAnimData.hpp`, and with that include this object grows
//     0x24 bytes of `.data` and 5 of `.bss` (measured) - bytes its claim does not cover, in the
//     same family as the trap `docs/RUNNING_THE_DECOMP.md` records as "a non-`Matching` object
//     can contribute bytes the split does not claim". The explicit call is what `delete` compiles
//     to (`lwz r3,4(r30) / li r4,1 / bl`), and the object then holds `.text` and `.comment` only,
//     like the module's other units.
//
// This file is listed in `files.cmake`, and **its host branch defines nothing at all** - the same
// arrangement `CLumiteRelTail.cpp` uses, and the reason is the one `tools/check_files_cmake.py`
// enforces: a `Matching` object has to be either in `files.cmake` or in the checker's judge-owned
// EXCLUDED set, and the only automatic exemption is a unit that defines RELMain/RELExit, which
// this one deliberately does not. An empty host branch keeps the port's undefined count where it
// is while the MWCC branch is the retail source and reproduces retail's bytes.
//
// Definitions are in descending retail text order (mwcceppc emits definitions in reverse source
// order); `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CAtomicAlpha7E0.cpp`.

class CAnimData;

extern "C" {

// The DOL's `CAnimData` deleting destructor, `symbols.txt`'s `__dt__9CAnimDataFv`: the call
// `delete mItem` makes, with the delete flag in r4. Declared, never defined here.
void __dt__9CAnimDataFv(CAnimData* self, int flag);

// Retail's `CMemory::Free(void const*)`; the delete half of this destructor. Declared, never
// defined here. `Kyoto/Alloc/CMemory.hpp:46` is where MWCC reaches it from `operator delete`.
void Free__7CMemoryFPCv(const void* ptr);

#ifdef __MWERKS__

// The two words the bytes show: the flag at +0 the destructor tests, the owned pointer at +4 the
// destructor deletes. `rstl::auto_ptr<T>`'s own layout, with T = `CAnimData` for this copy.
struct CAtomicAlphaAnimDataPtr {
  bool mHas;
  CAnimData* mItem;
};

// .text 0x7E0, 0x64 = 100 bytes. `self` in r3, the delete flag in r4; returns `self`.
void* fn_2_7E0(CAtomicAlphaAnimDataPtr* self, short flag) {
  if (self != 0) {
    if (self->mHas) {
      __dt__9CAnimDataFv(self->mItem, 1);
    }
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

#endif
}
