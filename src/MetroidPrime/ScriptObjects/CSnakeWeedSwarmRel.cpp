// CSnakeWeedSwarmRel.cpp - SnakeWeedSwarm's (module 71) head, .text 0x0..0xDC: the four
// functions above the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CScriptPlayerProxy.cpp` and
// `MetroidPrime/ScriptObjects/CMetareeSwarmRel.cpp`, and the ranges come from
// `config/G2ME01/rels/SnakeWeedSwarm/symbols.txt`:
//
//   0x00  fn_71_0   0x2C   the CActor `GetHealthInfo` slot, calling vtable slot 0x38
//   0x2C  RELExit   0x24   li r3,0 / bl SetLoader_SnakeWeedSwarm
//   0x50  RELMain   0x20   bl fn_71_70
//   0x70  fn_71_70   0x6C   lbl_71_bss_40 = {fn_71_DC, lbl_71_data_18, lbl_71_data_24}
//
// Everything else in the module is left unclaimed, so dtk fills it from retail and the module's
// sha1 against `config/G2ME01/config.yml` still holds. The first neighbour left retail is
// fn_71_DC (0xDC, 0x544), the module's own entity loader: behavioural class code, and it needs the
// CActor/CPatterned hierarchy this tree does not model.
//
// The two callees are named by what they are, not invented:
//   - `SetLoader_SnakeWeedSwarm` is the DOL's 0x8021BB08, two instructions, `stw r3,
//     gLoader_SnakeWeed; blr`. So it stores the *address* of the record, not a loader.
//     `LoadSnakeWeedSwarm` at 0x8021BADC reads it as `lwz r6, gLoader_SnakeWeed;
//     lwz r12, 0(r6); mtctr r12; bctrl` and `SnakeWeedAlt_8021BA94` at 0x8021BA94 as
//     `lwz r7, gLoader_SnakeWeed; addi r12, r7, 0x4; bl __ptmf_scall`, which is why the store
//     below hands it `&lbl_71_bss_40`.
//   - `lbl_71_bss_40` is `.bss:0x40`, `size:0x1C`: the module's own copy of the record. This
//     unit's split claims .text only, so dtk's `.bss` object has to define it, and a second
//     definition under MWCC is what produced mwldeppc's internal linker error on
//     ScriptPlayerProxy. Hence extern under MWCC and a host definition.
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
// which are 12 bytes each** - `__ptmf_scall` (build/G2ME01/asm/Runtime/ptmf.s:0x80345454) reads all
// three words, the `this` adjustment, a vtable offset and the address, and `__ptmf_null` there is
// `0xC` bytes for the same reason. So the two 12-byte objects at `.data:0x18` and `.data:0x24`
// (`auto_04_00000000_data.s`: 0 / 0xFFFFFFFF / fn_71_1AF8 and 0 / 0xFFFFFFFF / fn_71_1B3C) are
// non-virtual member-function pointers, vtable offset -1 meaning "call it directly", and the
// registration below copies them word for word rather than assigning a function to them.
//
// The type is spelled here rather than taken from `MetroidPrime/ScriptLoaderRel.hpp`, which models
// only the first two members: the module fills seven words, and the third (`fn_71_1B3C`) has no
// reader in the DOL, so the header's version is short a member. Correcting the header is a port-side
// model change and belongs to a different item.
struct SSnakeWeedSwarm_FuncPtrs {
  FScriptLoader swarm;
  // The DOL's `SnakeWeedAlt_8021BA94` calls this one through `__ptmf_scall` with a CVector3f by
  // value and a CDamageInfo, and `fn_71_1AF8` reads `*(u16*)(info + 0)` and `*(float*)(info + 0xC)`.
  void (CEntity::*damage)(CVector3f, const CDamageInfo&, CStateManager&);
  void (CEntity::*alt)(CVector3f, const CDamageInfo&, CStateManager&);
};
void SetLoader_SnakeWeedSwarm(SSnakeWeedSwarm_FuncPtrs* loader);

// The module's own copies of the two member-function pointers, in `.data` and not claimed by this
// unit, so they are extern here: dtk's data object defines them.
extern void (CEntity::*lbl_71_data_18)(CVector3f, const CDamageInfo&, CStateManager&);
extern void (CEntity::*lbl_71_data_24)(CVector3f, const CDamageInfo&, CStateManager&);

// **fn_71_0 is a vtable entry, not a free function.** `lbl_71_data_30` (`.data:0x30`, 0x7C bytes)
// is CSnakeWeedSwarm's own vtable and stores it at offset 0x3C, and that table is CActor's
// (`__vt__6CActor`, 29 entries after two zero words) with one slot replaced: the 14th virtual,
// `GetHealthInfo__6CActorCFv`, is `fn_71_0` here, and the entry before it - the 13th, at vtable
// offset 0x38, which is the one the call below dispatches on - is `HealthInfo__6CActorFv`, and
// that is what the DOL's own `GetHealthInfo__6CActorCFv` at 0x8000B900 calls in exactly these
// instructions.
//
// So the call is a member call, and that spelling is measured rather than guessed: loading the
// vtable by hand - `void* const* vt = *(void* const* const*)self;` and calling `vt[14]` -
// compiles to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, and that one register is 99.09%
// on the function (measured 2026-09-29 on CIngPuddleRel.cpp, the same call). `mwcceppc` only
// reaches for r12 on its own virtual-dispatch path, and it lays a class's virtuals out the way
// retail's vtable is laid out: two leading words (offset-to-top, then the RTTI pointer, both
// zero in this REL) and then one word per virtual, so the twenty-ninth virtual is the last word
// of `lbl_71_data_30`. The slots are named by position because no header here models a CActor
// virtual; none of them is defined or called from this file, because the only object that carries
// this vtable is the module's own retail bytes.
class CSnakeWeedSwarmVTable {
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
};

extern "C" {
CEntity* fn_71_DC(CStateManager&, CInputStream&, const CEntityInfo&);

#ifdef __MWERKS__
extern SSnakeWeedSwarm_FuncPtrs lbl_71_bss_40;
#else
SSnakeWeedSwarm_FuncPtrs lbl_71_bss_40 = {0, 0, 0};
#endif

// .text 0x70, 0x6C bytes: the module's loader registration. Retail loads all six words out of
// `.data` into r9/r8/r7 and r5/r4/r0, stores the loader through `stwu` so r3 walks the record,
// stores the six words back, and only then calls the setter - so the two member-function pointers
// are copied out of `.data` rather than being built here.
void fn_71_70() {
  lbl_71_bss_40.swarm = fn_71_DC;
  lbl_71_bss_40.damage = lbl_71_data_18;
  lbl_71_bss_40.alt = lbl_71_data_24;
  SetLoader_SnakeWeedSwarm(&lbl_71_bss_40);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the matching
// build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMetareeSwarmRel.cpp` and `CIngPuddleRel.cpp`: listing this file in `files.cmake` would make
// the port link `fn_71_DC` and `SetLoader_SnakeWeedSwarm`, which it cannot, and
// `tools/link_check.sh --strict` fails on a growing undefined count. So the port keeps reading
// `SnakeWeedSwarm.rel` off the disc through `platform/rel.cpp`, and the `#else` branch exists
// only so the file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain() { fn_71_70(); }

void RELExit() { SetLoader_SnakeWeedSwarm(0); }
#else
void mp_relmain_snakeweedswarm() { fn_71_70(); }

void mp_relexit_snakeweedswarm() { SetLoader_SnakeWeedSwarm(0); }
#endif

// .text 0x0, 0x2C bytes. Vtable entry 0x3C of CSnakeWeedSwarm: CActor's `GetHealthInfo` slot,
// returning the result of the `HealthInfo` slot above it. Retail's CActor spelling of the same
// function is `return const_cast<CActor*>(this)->HealthInfo();`; the const_cast is invisible in
// the bytes - r3 is already the address that gets passed on - so the record's pointer is passed
// straight through.
CHealthInfo* fn_71_0(CSnakeWeedSwarmVTable* self) { return self->Slot12(); }
}
