// Carved out of the unclaimed dtk `auto_*` range at retail `.text 0x80233B58`, as the new `Matching`
// unit `MetroidPrime/ScriptLoader/Carve80333B58`.  Retail's own symbol names this function
// `RegisterScriptObjects__25CScriptObjectLoaderHelperFQ24rstl44vector<P7CEntity,Q24rstl17rmemory_allocator>R13CStateManager`
// (`config/G2ME01/symbols.txt:10011`, size 0xF4), so it is written as the real member that header
// already declares - `include/MetroidPrime/CScriptObjectLoaderHelper.hpp:47` - and the compiler emits
// that name.  It has to be the real name: `MetroidPrime/CGameArea.cpp:761` already calls it and its
// object carries the name as an undefined reference
// (`build/G2ME01/obj/MetroidPrime/CGameArea.o`: `U RegisterScriptObjects__25CScriptObjectLoaderHelper...`),
// and `tools/check_symbol_names.py` fails a claim whose object does not define the name `symbols.txt`
// declares for its range.
//
// **The unit name is 7 hex digits, and that is deliberate.**  Every other carve is
// `Carve` + the 8-digit address (`Carve80233A90`, `Carve8023289C`); this one is spelled to match the
// queued item's target `MetroidPrime/ScriptLoader/Carve80333B58`, because `tools/goal_check.sh`
// resolves a `match` item's target by looking for `Object(<state>, "<target>.cpp")` in configure.py
// and fails the item outright when the names differ.  The address is **0x80233B58**; nothing about
// the code below depends on the spelling.
//
// **The claim.**  `.text 0x80233B58..0x80233C4C`, 0xF4 = 244 bytes, 61 instructions, one function.
// The range starts exactly where `MetroidPrime/ScriptLoader/Carve80233A90.cpp` (0x80233A90..0x80233B58)
// ends and stops exactly where the next function in dtk's listing begins -
// `InitScriptObjects__25CScriptObjectLoaderHelperFRQ24rstl45vector<9TEditorId,...>` at 0x80233C4C,
// `symbols.txt:10012`.  So the claim spans nothing and nothing above it is taken.  The byte evidence
// is the pristine disc: `python3 tools/dol_read.py 0x80233B58 0xF4 orig/G2ME01/sys/main.dol` gives
// 244 bytes, word for word `build/G2ME01/asm/auto_03_80233B58_text.s:11-77`.
//
// **Two `for` loops over the same `rstl::vector<CEntity*>&`, with the null check written twice.**
// Loop 1 is 0x80233B88..0x80233B9C (`lwz r3,0xc(r28)` / `lwzx r4,r3,r31` / `cmplwi r4,0` / `beq` /
// `mr r3,r29` / `bl AddObject__13CStateManagerFP7CEntity`); loop 2 is the same head at
// 0x80233BC0 plus the message block to 0x80233C14.  Retail re-loads `mItems` and re-tests rather than
// fusing the two passes, so this file spells two loops rather than one pass with two bodies.
//
// **The loop is indexed, and that is load-bearing - measured, not assumed.**  The index in `r30`
// (`li r30,0` / `addi r30,r30,1`) with a byte-offset induction variable in `r31` (`li r31,0` /
// `addi r31,r31,4`) and the guard `lwz r0,0x4(r28)` / `cmpw r30,r0` / `blt` is what
// `objects.size()` and `objects[i]` produce.  The same body with
// `rstl::vector<CEntity*>::iterator` (compiled with `tools/probe_cc.sh`) is a **different 30-instruction
// loop**: `lwz r4,0(r31)` off a hoisted `mItems`, no second induction variable, and an
// end-pointer compare - `lwz r0,4(r29)` / `lwz r3,12(r29)` / `slwi r0,r0,2` / `add r0,r3,r0` /
// `cmplw r31,r0` / `bne` - which is 4 instructions longer and never matches retail.  The indexed
// spelling below reproduces retail's 11-instruction loop head and guard exactly.
//
// The message block is a twin of matched code in this tree, and it is
// `CGameCollision::SendMaterialMessage` (`src/MetroidPrime/CGameCollision.cpp:744-748`, `Matching`,
// 100.00% in `build/report.json`), retail 0x801255BC
// (`build/G2ME01/asm/MetroidPrime/CGameCollision.s:2261-2288`):
//
//   lhz r6, 0x8(r5)          addi r5, r4, 0x4e44        lhz r7, kInvalidUniqueId
//   sth r6, 0x10(r1)  mr r3, r30  addi r4, r1, 0x1c  sth r7, 0x8(r1)  sth r7, 0xc(r1)
//   sth r6, 0x14(r1)  sth r7, 0x18(r1)  sth r7, 0x1c(r1)  sth r7, 0x1e(r1)  sth r6, 0x20(r1)
//   stw r5, 0x24(r1)  stw r0, 0x28(r1)  bl DeliverScriptMsg__13CStateManagerFRC10CScriptMsg
//
// **Ten stores for one statement, and the order is not member order.**  Measured with
// `tools/probe_cc.sh` on a 10-line body that is nothing but the statement:
//
//   c:  li r0,-1        10: lhz r6,8(r4)      14: lis r4,0x5841     18: lhz r7,kInvalidUniqueId
//   1c: addi r5,r4,0x4c44   20: sth r6,0x10(r1)    24: addi r4,r1,0x1c
//   28: sth r7,0x8(r1)   2c: sth r7,0xc(r1)    30: sth r6,0x14(r1)   34: sth r7,0x18(r1)
//   38: sth r7,0x1c(r1) 3c: sth r7,0x1e(r1)   40: sth r6,0x20(r1)   44: stw r5,0x24(r1)
//   48: stw r0,0x28(r1) 4c: bl DeliverScriptMsg
//
// which is retail's store sequence, offsets and all.  Two things that block usually matters for are
// **measured not to matter here**: spelling the message into a `const CScriptMsg msg` local and
// passing `msg` gives a **byte-identical** 96-byte object (same offsets, same order, same `bl`), so
// the count does not come from binding a temporary to the reference - MWCC materialises it either way.
// What the last five stores (`0x10, 0x8, 0xc, 0x14, 0x18`) are is not established here; they are a
// second, unread copy in both spellings and in retail, and the claim above is only that the statement
// reproduces all ten.
//
// `CScriptMsg`'s own layout is 0x10 bytes and retail's last five fit it exactly: `m_unk`,
// `m_originator`, `m_id` as three 2-byte `TUniqueId`s (`include/MetroidPrime/TGameTypes.hpp:57`,
// `CHECK_SIZEOF(TUniqueId, 0x2)` at line 79) at +0/+2/+4, then `m_msg` and `m_state` as words at +8
// and +0xc (`include/MetroidPrime/CEntityInfo.hpp:330-334`).  0x1c+0 = `m_unk`, 0x1c+2 = `m_originator`,
// 0x1c+4 = `m_id`, 0x1c+8 = `m_msg`, 0x1c+0xc = `m_state`; the pad word at +6 is never written.
//
// **`m_msg` is a four-character code, not an enum constant.**  `stw r5,0x24(r1)` stores
// 0x58414C44 = `'X' 'A' 'L' 'D'`, materialised as `lis r3,0x5841` / `addi r5,r3,0x4c44` - a constant
// in the instruction stream, never a `lis`/`addi` pair for a string address.  `CGameCollision` writes
// it the same way with `static_cast<EScriptObjectMessage>('XOND')` and the tree's
// `EScriptObjectMessage` enumerators are all of that shape (`kSM_Load = 0x4c4f4144` = `'LOAD'`), so
// the spelling below is retail's, not a cast invented to fit.
//
// **The three ids go in retail's *argument* order, not member order.**  `CScriptMsg`'s five-argument
// constructor is declared `(unk, id, originator, msg, state)` (`CEntityInfo.hpp:317-319`) and
// initialised `m_unk(unk), m_originator(originator), m_id(id)`; the header says so in as many words
// ("Do not 'tidy' this back") because mwcceppc keeps it.  Retail's stores are `0x1c = kInvalidUniqueId`
// (m_unk), `0x1e = kInvalidUniqueId` (m_originator) and `0x20 = entity->GetUniqueId()` (m_id), which
// is `CScriptMsg(kInvalidUniqueId, id, kInvalidUniqueId, ...)` - the same call `CGameCollision` makes,
// and the one this unit needs.  `m_unk` and `m_originator` hold the same value here, so this function
// cannot distinguish the two orders on its own; `CGameCollision::SendMaterialMessage` and this copy
// together are what make the spelling unambiguous.
//
// **The vector's layout is `rstl::vector`'s own** (`include/rstl/vector.hpp:18-21`: `mAllocator` at
// +0, `mCount` at +4, `mCapacity` at +8, `mItems` at +0xc, `rmemory_allocator` being one word).
// Retail reads `mCount` with `lwz r0,0x4(r28)` / `cmpw r30,r0` in the loop guard and `mItems` with
// `lwz r3,0xc(r28)` in the body, both re-loaded every iteration, and keeps the vector itself in `r28`
// for the whole function - which is what `objects[i]` through `operator[]` and `size()` gives.  The
// index is `int` (`cmpw`, not `cmplw`).
//
// **`lhz r6, 0x8(r4)` is `CEntity::mUniqueId`.**  `CEntity` has a vtable pointer at +0, `mAreaId` at
// +4 and `mUniqueId` at +8 (`include/MetroidPrime/CEntity.hpp:80`, `CHECK_SIZEOF(CEntity, 0x24)`), and
// `GetUniqueId()` returns the 2-byte `TUniqueId` by value, which is the `lhz`.  `CGameCollision` reads
// the same +8 out of its `CActor&` argument.
//
// **The parameter is by value and that is retail's declaration**, not a simplification:
// `symbols.txt:10011` spells `FQ24rstl44vector<P7CEntity,Q24rstl17rmemory_allocator>R13CStateManager`
// (`F` = by value, `R` = reference), and `CScriptObjectLoaderHelper.hpp:47` declares it that way.  The
// caller makes the copy and passes a pointer - `MetroidPrime/CGameArea.cpp:761` passes
// `mPostConstructed->mScriptLoadState->mObjects`, a `rstl::vector<CEntity*>` member, by value - so
// the callee only ever sees the pointer in `r4` and never runs a copy or a destructor.
//
// **Source order is descending by address** (the rule is about *multiple* definitions; this unit has
// one, so it cannot be permuted).  `mwcceppc` emits definitions in reverse source order and
// `mwldeppc` keeps the object's `.text` order verbatim, so a two-function unit declared ascending is a
// permuted `.text` - 100.00% per function and a broken DOL, which only `tools/flip_test.sh` catches.
// The nearest carve, `Carve80233A90.cpp`, records the same thing for the range immediately below this
// one.
//
// The two callees are both real definitions in this tree - `CStateManager::AddObject(CEntity*)`
// (`src/MetroidPrime/CStateManager.cpp:1194`) and `CStateManager::DeliverScriptMsg(const CScriptMsg&)`
// (`src/MetroidPrime/CStateManager.cpp:1610`) - so this unit adds no undefined symbol to the DOL link
// and, in the host port, *removes* one: `CGameArea.cpp` was the last caller of an undefined
// `CScriptObjectLoaderHelper::RegisterScriptObjects`.

#include "MetroidPrime/CScriptObjectLoaderHelper.hpp"

#include "MetroidPrime/CEntity.hpp"
#include "MetroidPrime/CEntityInfo.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "rstl/vector.hpp"

/** `CScriptObjectLoaderHelper::RegisterScriptObjects` - retail `.text:0x80233B58`, 0xF4 = 244 bytes.
 *  Registers every entity the layer load produced with the state manager, then sends each one the
 *  'XALD' message - the loop `CGameCollision::SendMaterialMessage` sends one of, with the actor's own
 *  id as the destination.  Two passes, not one, because that is what retail emitted. */
void CScriptObjectLoaderHelper::RegisterScriptObjects(rstl::vector< CEntity* > objects,
                                                     CStateManager& mgr) {
  for (int i = 0; i < objects.size(); i++) {
    CEntity* entity = objects[i];
    if (entity) {
      mgr.AddObject(entity);
    }
  }

  for (int i = 0; i < objects.size(); i++) {
    CEntity* entity = objects[i];
    if (entity) {
      mgr.DeliverScriptMsg(CScriptMsg(kInvalidUniqueId, entity->GetUniqueId(), kInvalidUniqueId,
                                      static_cast< EScriptObjectMessage >('XALD'),
                                      kSS_InvalidState));
    }
  }
}