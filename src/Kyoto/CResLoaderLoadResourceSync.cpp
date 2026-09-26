/**
 * `fn_802FC420` - retail `.text:0x802FC420`, `size:0xB8` = 184 bytes, 48-byte frame, `stmw r26`
 * rather than four separate `stw`s. `CResLoader::LoadResourceSync`: the **uncompressed**
 * synchronous load, and the first of the three that allocate.
 *
 * ```
 * 802fc420:  stwu r1,-48(r1) ; mflr r0 ; stw r0,52(r1) ; stmw r26,24(r1)
 * 802fc430:  mr   r28,r3                      <- this
 * 802fc434:  mr   r26,r5                      <- arg3, the void** out-parameter
 * 802fc438:  mr   r27,r6                      <- arg4, the uint* out-parameter
 * 802fc43c:  bl   fn_802FCEEC
 * 802fc440:  lwz  r30,104(r28)                <- this->x68_curRes
 * 802fc444:  mr   r31,r3                      <- the pak
 * 802fc448:  bl   GetSize__Q28CPakFile8SResInfoCFv
 * 802fc450:  addi r0,r3,31
 * 802fc454:  lis  r5,lbl_803AFAA0
 * 802fc458:  lis  r4,kUnknownType__10CCallStack
 * 802fc45c:  addi r3,r1,8                     <- the CCallStack, built on the stack
 * 802fc460:  addi r6,r4,0
 * 802fc464:  addi r5,r5,0
 * 802fc468:  clrrwi r29,r0,5                   <- the 32-byte-rounded length
 * 802fc46c:  li   r4,-1
 * 802fc470:  bl   __ct__10CCallStackFUiPCcPCc
 * 802fc474:  mr   r7,r3
 * 802fc478:  mr   r3,r29 ; li r4,2 ; li r5,1 ; li r6,0
 * 802fc488:  bl   Alloc__7CMemoryFUlQ210IAllocator5EHintQ210IAllocator6EScopeQ210IAllocator5ETypeRC10CCallStack
 * 802fc494:  mr   r0,r3 ; mr r3,r30 ; mr r28,r0      <- r28 is REUSED: this -> buf
 * 802fc4a0:  bl   GetOffset__Q28CPakFile8SResInfoCFv
 * 802fc4a4:  mr   r7,r3
 * 802fc4a8:  mr   r3,r31 ; mr r4,r28 ; mr r5,r29 ; li r6,0
 * 802fc4b8:  bl   SyncSeekRead__8CDvdFileFPvUi11ESeekOrigini
 * 802fc4bc:  stw  r28,0(r26)                   <- *outBuf = buf
 * 802fc4c0:  bl   GetSize__Q28CPakFile8SResInfoCFv
 * 802fc4c8:  stw  r3,0(r27)                    <- *outSize = GetSize(), the UNROUNDED size
 * 802fc4cc:  <the epilogue>
 * ```
 *
 * Six things in that listing are not guesses, and three of them are the whole difficulty:
 *
 *  1. **`CMemory::Alloc` takes a `const CCallStack&` in r7, and the `CCallStack` is a stack
 *     object at `r1+8`** built by an explicit call to `CCallStack::CCallStack(uint, const char*,
 *     const char*)` - retail's ctor is only 12 bytes (`stw r5,0(r3)` / `stw r6,4(r3)` / `blr`,
 *     0x8028BFE8) and is in a dtk object, so it is *called* and not inlined. Writing
 *     `CMemory::Alloc(len, hint, scope, type)` with the header's default argument instead does not
 *     reproduce the `lis`/`addi` pair pair at all, because the default is a temporary and the
 *     compiler folds it away.
 *  2. **The `lineStr` is retail's own merged `lbl_803AFAA0` (0x803AFAA0) and *not* a literal.**
 *     That 16-byte object is `"??(??)\0.pak\0\0\0\0"` - two strings the linker merged - and
 *     `src/Kyoto/CResLoaderAddPakFileAsync.cpp` already declares it for the same reason. A literal
 *     here compiles to a `lis` plus a *different* `addi`, the per-function diff sees nothing
 *     wrong, and the hash fails: the file this is writing is exactly where
 *     `docs/research/rc_ptr.md`'s correction came from.
 *  3. **`type` is `kUnknownType__10CCallStack` (0x803AEAB8, `"UnknownType\0"`)**, the class's own
 *     static, and it is passed **explicitly** - the compiler has to emit a `lis`+`addi` for it
 *     even though it is the declared default. `CCallStack`'s declaration has the default
 *     `= kUnknownType`, so the call below spells it out; dropping the third argument is a
 *     measurable difference, not a no-op.
 *  4. **The read length is the *rounded* size and the second out-parameter is the *unrounded*
 *     one.** `clrrwi r29,r0,5` produces the `(size + 31) & ~31` that goes into `SyncSeekRead`
 *     in r5, and `*outSize` is a *second* `GetSize()` call, not `r29`. That asymmetry is the
 *     caller's contract - the buffer is padded, the reported size is not - and writing
 *     `*outSize = alignedSize` gives a different object.
 *  5. **r28 is `this` and then the buffer.** `this` dies at the `lwz r30,104(r28)`, and MWCC
 *     coalesces the buffer into the same register, which is why the listing shows `mr r28,r0`
 *     *after* the `Alloc` call. The source order below is what produces it.
 *  6. **The frame is `stmw r26,24(r1)`, not four `stw`s**, which is what four saved
 *     non-volatile registers in a row emits.
 *
 * The hint/scope/type triple is `2, 1, 0` = `kHI_RoundUpLen`, `kSC_Unk1`, `kTP_Heap`.
 *
 * Unnamed in `config/G2ME01/symbols.txt` and referenced by `auto_03_802F8EB0_text.o`, so it
 * keeps its dtk name with C linkage. Its range is 0x802FC420..0x802FC4D8 and `fn_802FC4D8` is
 * next, so this is its own unit: the two differ in what they do with the buffer and claiming
 * both would put `fn_802FC4D8`'s bytes in a unit that does not reproduce them.
 */
#include "types.h"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Alloc/CCallStack.hpp"
#include "Kyoto/Alloc/IAllocator.hpp"
#include "Kyoto/CPakFile.hpp"
#include "Kyoto/CDvdFile.hpp"
#include "Kyoto/CResLoader.hpp"

// `lbl_803AFAA0` - `.rodata:0x803AFAA0`, `size:0x10`, owned by no unit:
//
//   803afaa0  3f 3f 28 3f 3f 29 00 2e 70 61 6b 00 00 00 00 00   "??(??)..pak"
//
// Two strings the linker merged: bytes 0..6 are the `??(??)?` that stands in for `__FILE__` in
// this throwaway `CCallStack`, and byte 7 starts the `".pak"` that `CResLoaderAddPakFileAsync`
// appends. This unit's object references it once, as `R_PPC_ADDR16_HA` + `R_PPC_ADDR16_LO` at
// 0x802fc454/0x802fc45c; the *other* unit materialises the same address twice. A `Matching` unit
// may not own a `.rodata` byte, so this is a declaration; the definition is
// `src/MetroidPrime/PortGlobals.cpp`'s business, exactly as for the other retail read-only data.
extern "C" const char lbl_803AFAA0[];

extern "C" void fn_802FC420(void* resLoader, const SObjectTag& tag, void** outBuf,
                            uint* outSize) {
  CResLoader* const self = static_cast< CResLoader* >(resLoader);
  CPakFile* const pak = static_cast< CPakFile* >(fn_802FCEEC(resLoader, tag));
  CPakFile::SResInfo* const res = self->x68_curRes;
  const uint paddedSize = (res->GetSize() + 31) & ~static_cast< uint >(31);
  // The `CCallStack` is a **temporary passed straight into the call**, not a named local. That is
  // what produces retail's `addi r3,r1,8` / `bl <ctor>` / `mr r7,r3` - the reference is the value
  // r3 already holds, because the ctor (`stw r5,0(r3)` / `stw r6,4(r3)` / `blr`) leaves r3 alone.
  // Naming the object (`const CCallStack callstack = ...;` and passing that) gives `addi r7,r1,8`
  // and a trailing `li r3,0` instead, and the unit reads 91.70% instead of 100.00%. Measured, both
  // ways, same object.
  //
  // **`lbl_803AFAA0` has to be named**, not written as the `"??(??"` literal `CCallStack`'s own
  // signature and `CMemory.hpp`'s inline `operator new` both use. mwcceppc emits a literal as a
  // local `@stringBase0` and the linker lands that somewhere else, so the `addi` differs and only
  // the DOL hash sees it. Same correction as `docs/research/rc_ptr.md`.
  void* const buf =
      CMemory::Alloc(paddedSize, IAllocator::kHI_RoundUpLen, IAllocator::kSC_Unk1,
                     IAllocator::kTP_Heap, CCallStack(-1, lbl_803AFAA0));
  pak->DvdFile().SyncSeekRead(buf, paddedSize, kSO_Set, res->GetOffset());
  *outBuf = buf;
  *outSize = res->GetSize();
}
