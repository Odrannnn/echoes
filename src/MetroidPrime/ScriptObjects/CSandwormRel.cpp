// CSandwormRel.cpp - Sandworm's (module 56) head, .text 0x0..0xDC: the four functions above the
// module's class code. Same arrangement as `MetroidPrime/ScriptObjects/CSnakeWeedSwarmRel.cpp`,
// `MetroidPrime/ScriptObjects/CFishCloudRel.cpp` and
// `MetroidPrime/ScriptObjects/CMetareeSwarmRel.cpp`, and the ranges come from
// `config/G2ME01/rels/Sandworm/symbols.txt`:
//
//   0x00  fn_56_0   0x2C   the CActor `GetHealthInfo` slot, calling vtable slot 0x38
//   0x2C  RELExit   0x24   li r3,0 / bl SetLoader_Sandworm
//   0x50  RELMain   0x20   bl fn_56_70
//   0x70  fn_56_70  0x6C   lbl_56_bss_38 = {fn_56_DC, lbl_56_data_6E0, lbl_56_data_6EC}
//
// **This head is shorter than every sibling's in the family, and that was measured before
// anything was written, not inferred from the `fn_<id>_<off>` names.** Unlike Ing, Splinter,
// MinorIng and AtomicAlpha, whose heads open with a block of member-address accessors, Sandworm's
// `.text` begins with `fn_56_0` itself (`build/G2ME01/Sandworm/asm/auto_00_00000000_text.s`), so
// the claim is four functions over 0xDC, not eighteen over 0x170. Diffing that dump's 0x0..0xDC
// against `build/G2ME01/SnakeWeedSwarm/asm/auto_00_00000000_text.s` over the same range with every
// identifier masked: **55 instructions each, 53 identical in opcode and operands, and the two that
// differ are the `bl` at 0x3C and the `bl` at 0xC8** - i.e. the module's own setter name and
// nothing else. Every body below is therefore one `CSnakeWeedSwarmRel.cpp` already reproduces at
// 100.00%, and no spelling had to be discovered for this unit.
//
// Everything else in the module is left unclaimed, so dtk fills it from retail and the module's
// sha1 against `config/G2ME01/config.yml` still holds. The first neighbour left retail is
// `fn_56_DC` (0xDC, 0xF6C), the module's own entity loader - three arguments, `stw r3`/`mr r27,
// r4`/`stw r5` and `lwz r4, 0x8(r27)`, the same shape as `fn_71_DC` - and it plus the 371
// functions above `auto_fn_56_14FA8_text` need the CActor/CPatterned/CAi hierarchy this tree does
// not model.
//
// The two callees are named by what they are, not invented:
//   - `SetLoader_Sandworm` is the DOL's 0x8021887C, two instructions, `stw r3,
//     gLoader_Sandworm@sda21(r0); blr`, immediately after `LoadSandworm__FR13CStateManagerR12C-
//     InputStreamRC11CEntityInfo` at 0x80218850, which is 0x2C bytes and so ends exactly there.
//     So it stores the *address* of the record, not a loader. **It has no C++ body in this tree**,
//     unlike `SetLoader_FishCloud` and `SetLoader_SnakeWeedSwarm`, whose definitions live in
//     `src/MetroidPrime/ScriptLoaderRel.cpp` - so unlike them it also has no mangled name in
//     `config/G2ME01/symbols.txt` yet, and the declaration below is what mwcceppc mangles. The one
//     token `fn_8021887C__FP18SSandworm_FuncPtrs` in that file exists only so the call resolves;
//     the unit holding 0x8021887C is unclaimed, so no DOL byte moves.
//   - `lbl_56_bss_38` is `.bss:0x38`, `size:0x1C` (`build/G2ME01/Sandworm/asm/
//     auto_05_00000000_bss.s`): the sixth of the module's seven `.bss` objects, and the one
//     `fn_56_70` stores through, so the offset has to be read off the dump rather than assumed to
//     be 0x0 the way `CFishCloudRel.cpp`'s is. This unit's split claims `.text` only, so dtk's
//     `.bss` object has to define it, and a second definition under MWCC is what produced
//     mwldeppc's internal linker error on ScriptPlayerProxy. Hence extern under MWCC and a host
//     definition.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100%.

#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

class CDamageInfo;
class CEntity;
class CHealthInfo;
class CVector3f;

// **The record is 0x1C bytes: an FScriptLoader and two CodeWarrior pointer-to-member-functions,
// which are 12 bytes each** - `__ptmf_scall` (build/G2ME01/asm/Runtime/ptmf.s:0x80345454) reads
// all three words, the `this` adjustment, a vtable offset and the address, and `__ptmf_null` there
// is 0xC bytes for the same reason. So the two 12-byte objects at `.data:0x6E0` and `.data:0x6EC`
// (`auto_04_00000000_data.s`: 0 / 0xFFFFFFFF / fn_56_5938 and 0 / 0xFFFFFFFF / fn_56_5910, both
// 0xC) are non-virtual member-function pointers, vtable offset -1 meaning "call it directly", and
// the registration below copies them word for word rather than assigning a function to them.
// The struct is spelled here rather than added to `MetroidPrime/ScriptLoaderRel.hpp`, which models
// only the first two members of the family's records; extending that header is a port-side model
// change and belongs to a different item.
//
// **Which class the two members hang off is not determined by these bytes, and the file does not
// pretend otherwise.**
struct SSandworm_FuncPtrs {
  FScriptLoader swarm;
  // **Both members are typed the same way on purpose.** What is measured about them is the
  // 12-byte size each occupies and the offset each is read at; the argument list below is the
  // +0x4 reader's shape and is not a claim about the +0x10 one. `fn_80218818` (0x80218818) reads
  // `gLoader_Sandworm`, adds 0x4 and calls `__ptmf_scall4` (0x8034547C, which adjusts **r4**,
  // not r3, so the object is the second value passed) with a stack local, r23 and a loop
  // counter. `fn_802187EC` (0x802187EC) adds 0x10 and calls `__ptmf_scall` (0x80345454, which
  // adjusts **r3**) with only r3 set up, and `fn_8012F3C0` uses its return value as the loop
  // bound (`bl fn_802187EC; cmpw r24, r3; blt`). Both thunks are called only from there, next to
  // `TCastToPtr<12CSandwormEye>__FP7CEntity`. The member names are the reader offsets, not an
  // invented class, and codegen depends on none of it - only the two sizes and the two offsets are
  // load-bearing in `fn_56_70`.
  void (CEntity::*update)(CVector3f*, CEntity*, int);
  void (CEntity::*eyeCount)(CVector3f*, CEntity*, int);
};
// Declared in C++, not inside the `extern "C"` block below: mwcceppc mangles it to
// `SetLoader_Sandworm__FP18SSandworm_FuncPtrs`, which is the name
// `config/G2ME01/symbols.txt` gives the DOL after the one-token rename. Putting the declaration
// inside `extern "C"` would drop the mangling and dtk would look for the bare name; a
// `fn_8021887C`-shaped alias is a different symbol and the call would resolve to nothing.
void SetLoader_Sandworm(SSandworm_FuncPtrs* loader);

// The module's own copies of the two member-function pointers, in `.data` and not claimed by this
// unit, so they are extern here: dtk's data object defines them.
extern void (CEntity::*lbl_56_data_6E0)(CVector3f*, CEntity*, int);
extern void (CEntity::*lbl_56_data_6EC)(CVector3f*, CEntity*, int);

// **fn_56_0 is a vtable entry, not a free function, and its bytes are identical to
// `CSnakeWeedSwarmRel.cpp`'s `fn_71_0`** - both `.text:0x0`, `size:0x2C`, eleven instructions, word
// for word. That is CActor's `GetHealthInfo`, returning the result of the `HealthInfo` slot above
// it. It is stored in *two* of this module's vtables, so nothing here is a dead-stripping hazard:
// `build/G2ME01/Sandworm/asm/auto_04_00000000_data.s` shows `lbl_56_data_6F8` (`.data:0x6F8`,
// 0x98 bytes = 38 words) and `lbl_56_data_884` (`.data:0x884`, 0x240 bytes = 144 words), each
// holding two leading zero words (offset-to-top, then the RTTI pointer, both zero in this REL) and
// then one word per virtual, so 36 virtuals in the first and 142 in the second. In both, word 14 -
// vtable offset 0x38, the 13th virtual, the one the call below dispatches on - is a `HealthInfo`
// slot (`HealthInfo__6CActorFv` in the first, `HealthInfo__3CAiFv` in the second, so the two
// tables are CSandwormEye's and CSandworm's) and word 15, offset 0x3C, is `fn_56_0` in both; each
// table is its class's with that one slot replaced.
//
// So the call is a member call, and that spelling is measured rather than guessed: loading the
// vtable by hand - `void* const* vt = *(void* const* const*)self;` and calling `vt[14]` -
// compiles to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, and that one register is 99.09%
// on the function (measured 2026-09-29 on CIngPuddleRel.cpp, the same call). `mwcceppc` only
// reaches for r12 on its own virtual-dispatch path, and it lays a class's virtuals out the way
// retail's vtable is laid out, so the 13th virtual is at 0x38. The slots are named by position
// because no header here models a CActor virtual; none of them is defined or called from this
// file, because the only objects that carry these vtables are the module's own retail bytes. The
// class is sized to the smaller of the two tables, which is a choice, not a measurement - only
// `Slot12`'s position is load-bearing, and it is the same in both.
class CSandwormEyeVTable {
public:
  virtual void Slot0();
  virtual void Slot1();
  virtual void Slot2();
  virtual void Slot3();
  virtual void Slot4();
  virtual void Slot5();
  virtual void Slot6();
  virtual void Slot7();
  virtual void Slot8();
  virtual void Slot9();
  virtual void Slot10();
  virtual void Slot11();
  virtual CHealthInfo* Slot12();
  virtual void Slot13();
  virtual void Slot14();
  virtual void Slot15();
  virtual void Slot16();
  virtual void Slot17();
  virtual void Slot18();
  virtual void Slot19();
  virtual void Slot20();
  virtual void Slot21();
  virtual void Slot22();
  virtual void Slot23();
  virtual void Slot24();
  virtual void Slot25();
  virtual void Slot26();
  virtual void Slot27();
  virtual void Slot28();
  virtual void Slot29();
  virtual void Slot30();
  virtual void Slot31();
  virtual void Slot32();
  virtual void Slot33();
  virtual void Slot34();
  virtual void Slot35();
};

extern "C" {
CEntity* fn_56_DC(CStateManager&, CInputStream&, const CEntityInfo&);

#ifdef __MWERKS__
extern SSandworm_FuncPtrs lbl_56_bss_38;
#else
SSandworm_FuncPtrs lbl_56_bss_38 = {0, 0, 0};
#endif

// .text 0x70, 0x6C bytes: the module's loader registration. Retail loads all six words out of
// `.data` into r9/r8/r7 and r5/r4/r0, stores the loader through `stwu` so r3 walks the record,
// stores the six words back, and only then calls the setter - so the two member-function pointers
// are copied out of `.data` rather than being built here.
void fn_56_70() {
  lbl_56_bss_38.swarm = fn_56_DC;
  lbl_56_bss_38.update = lbl_56_data_6E0;
  lbl_56_bss_38.eyeCount = lbl_56_data_6EC;
  SetLoader_Sandworm(&lbl_56_bss_38);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names that platform/compiled_modules.cpp could call. The MWCC branch is the
// retail source token for token, so the matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMetareeSwarmRel.cpp`, `CIngPuddleRel.cpp`, `CSnakeWeedSwarmRel.cpp` and `CFishCloudRel.cpp`:
// listing this file in `files.cmake` would make the port link `fn_56_DC` and `SetLoader_Sandworm`,
// which it cannot, and `tools/link_check.sh --strict` fails on a growing undefined count. So the
// port keeps reading `Sandworm.rel` off the disc through `platform/rel.cpp`, and the `#else` branch
// exists only so the file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain() { fn_56_70(); }

void RELExit() { SetLoader_Sandworm(nullptr); }
#else
void mp_relmain_sandworm() { fn_56_70(); }

void mp_relexit_sandworm() { SetLoader_Sandworm(nullptr); }
#endif

// .text 0x0, 0x2C bytes. Vtable entry 0x3C of both of the module's vtables: CActor's
// `GetHealthInfo` slot, returning the result of the `HealthInfo` slot above it at 0x38. Retail's
// CActor spelling of the same function is `return const_cast<CActor*>(this)->HealthInfo();`; the
// const_cast is invisible in the bytes - r3 is already the address that gets passed on - so the
// record's pointer is passed straight through. These eleven instructions are `fn_71_0`'s byte for
// byte, so this line is the same one `CSnakeWeedSwarmRel.cpp` already measures.
CHealthInfo* fn_56_0(CSandwormEyeVTable* self) { return self->Slot12(); }
}
