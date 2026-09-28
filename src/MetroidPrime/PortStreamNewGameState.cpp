/**
 * `StreamNewGameState__5CMainFR12CInputStreami` - retail `.text:0x800053B8`, `0x214` = 532 bytes.
 *
 * **This file is port-only.** `configure.py` does not declare it, so mwcceppc never sees it, no
 * `splits.txt` range claims anything for it and the DOL is byte-identical with or without it -
 * the same arrangement as `src/MetroidPrime/PortBoot.cpp`. It exists for one reason, and the
 * reason is a name:
 *
 *   `CMain::StreamNewGameState` compiles to `_ZN5CMain18StreamNewGameStateER12CInputStreami`
 *   under the host's Itanium mangling and to `StreamNewGameState__5CMainFR12CInputStreami` under
 *   mwcceppc's. `src/MetroidPrime/CMainFlowDtor.cpp:208` calls the *second* one, spelled out in
 *   an `extern "C"` declaration, and its header comment (point 4) says why that call site may not
 *   be changed: retail passes a **null** `CInputStream&` and never writes r5 at all, and no C++
 *   spelling reproduces "an uninitialised int" without costing three instructions in the middle
 *   of a `Matching` unit. So the port needs a definition under retail's own MWCC-mangled name,
 *   which no host compiler will ever emit for a member function.
 *
 * `src/MetroidPrime/main.cpp:380` still holds `CMain::StreamNewGameState`'s body for the DOL, and
 * on the host that Itanium-named function now has no caller - `CMainFlowDtor.cpp` is the only
 * thing in the port that reaches this function, and it reaches it by retail's name. The two are
 * not kept in step by anything; this one is written from the disassembly below, block by block,
 * because the `main.cpp` body is a `// TODO` inherited from upstream PrimeDecomp/echoes and does
 * not do what retail does (it hands `in` straight to `CGameState`, and every retail caller passes
 * a null reference - see 0x80005474).
 *
 * # What retail does, and what this file does
 *
 * Every address below is `build/G2ME01/main.elf`, read with
 * `powerpc-eabi-objdump -d --start-address=0x800053B8 --stop-address=0x800055CC`. The block map
 * in `docs/research/paks.md` is the same disassembly; where this file departs from it, the reason
 * is given. The two `CGameState` parameters retail is declared with are `(CInputStream& in,
 * int saveIdx)`, and **the third is never read**: `StreamNewGameState` saves r26..r31 in its
 * prologue and never r5, and `fn_80144140` overwrites r5 with `li r5,-1` at 0x80144148 before
 * touching anything else. This definition therefore takes the two arguments `CMainFlowDtor.cpp`
 * actually passes, and passing an uninitialised register is not something a host can express.
 *
 * | retail | what | here |
 * | --- | --- | --- |
 * | 0x800053D0 | local r1+0xB0 = copy of `gpGameState+0x54` (`fn_80005108`) | not built - see below |
 * | 0x800053E8 | r27 = that local's `+0x28`, the save-slot index | read straight off the member |
 * | 0x800053EC/0x800053F4 | r30/r29 = `+0x108`/`+0x10C`, the card serial | read straight off the member |
 * | 0x800053F8 | local r1+0x7C = copy of `gpGameState+0x110` (`fn_80004C90`) | `CopySlots` |
 * | 0x80005400 | local r1+0x24 = copy of `gpGameState+0x188` (`fn_80004AA0`) | `fn_80004AA0` |
 * | 0x8000540C-0x80005424 | `flag = local.r1+0x28 != 0 && (u8)in == 0` | `liveRecord` |
 * | 0x80005434 | local r1+0x48 = copy of `gpGameState+0x144` (`fn_80004C90`) | `CopySlots` |
 * | 0x80005444 | local r1+0x14 = copy of `gpGameState+0x178` (`fn_80004AA0`) | `fn_80004AA0` |
 * | 0x80005454 | local r1+0xDC = copy of `gpGameState+0x80` (`fn_80004E84`) | `CGameOptions` copy |
 * | 0x80005458 | release the old `CGameState` (`fn_80004154(slot, 0)`) | `GameState() = nullptr` |
 * | 0x80005470 | `gpGameState = 0` | same |
 * | 0x80005474-0x80005484 | pick the record: r1+0x24 if flag, else `r1+0x80 + 16*idx` | `record` |
 * | 0x80005494 | `CMemoryInStream(record.data, record.size)` | same |
 * | 0x800054A0 | `CBitStreamReader(that)` | same |
 * | 0x800054B4 | `::operator new(752, "??(??)..", 0)` | `::operator new(sizeof(CGameState))` |
 * | 0x800054C4 | `fn_80144140(&reader)` - the `CGameState` constructor | `fn_80144140` |
 * | 0x800054D4 | publish into `gameGlobalObjects+0x130` | `GameState() = ...` |
 * | 0x800054E0/0x800054F8 | destroy the bit reader, then the memory stream | RAII, same points |
 * | 0x80005500 | `gpGameState = the new one` | same |
 * | 0x80005518 | `fn_80142FA4` - copy the slots local into `+0x110` | `AssignSlots` |
 * | 0x80005524 | `fn_80142920` - copy the slots local into `+0x144` | `AssignSlots` |
 * | 0x80005530 | `fn_801427DC` - copy the block local into `+0x178` | `AssignBlock` |
 * | 0x80005538 | `fn_80003D00` - copy the options local into `+0x80` | `operator=` |
 * | 0x80005544 | *(the copy into `+0x54` is at 0x8000550C, `fn_80003F08`)* | **not reproduced** |
 * | 0x80005550 | `CGameOptions::EnsureOptions()` on the new `+0x80` | same |
 * | 0x8000555C/0x80005560 | `new->x10C = old->x10C`, `new->x108 = old->x108` | `cardSerial` assign |
 * | 0x80005568 | `fn_80142FEC(new)`, only when flag | **not reproduced** |
 * | 0x80005570-0x800055B4 | six destructors, reverse order | same, see below |
 *
 * ## The two blocks that are not reproduced, and why
 *
 * Both are named rather than silently dropped, and both cost the same thing: a symbol the port's
 * link does not define.
 *
 * 1. **The `SGameStateCardOpts` copy at `CGameState+0x54` and its carry-over into the new state**
 *    (`fn_80005108`, 0x80005108, 0x50; `fn_80003F08`, 0x80003F08, 0x50;
 *    `__dt__PersistentOptions_800050A4`, 0x800050A4, 0x64). The type is not a POD copy:
 *    `fn_80005108` is `fn_800052A0(dst, src)` + `fn_80005158(dst+0x18, src+0x18)` + one word, and
 *    `__dt__PersistentOptions_800050A4` destroys the sub-objects at `+0x00` and `+0x18` with
 *    `fn_80004678`/`fn_80004744`, so the member owns two heap things the header's
 *    `u8 x00[0x1C]` does not model. A `SGameStateCardOpts a = b;` here would be a shallow copy of
 *    both, and a double free once either side is destroyed - so it is left out rather than faked.
 *    **The one thing that local is read for survives**: retail loads the save-slot index out of
 *    it at 0x800053E8, and that word is `gpGameState->x54.x28`, read here before the release.
 * 2. **`fn_80142FEC(new)`** (0x80142FEC, 0x80), called only when `flag` is set. Unnamed in
 *    `config/G2ME01/symbols.txt`, unwritten, and calling it would add a symbol the port does not
 *    define. The condition and the call site are recorded here so the next lane does not have to
 *    re-derive them: `80005564 beq 80005570` tests the same `flag` byte.
 *
 * What is spent to get the rest: **one** new undefined symbol, `fn_80144140`. That is the whole
 * budget - `tools/goal_check.sh` requires the port's unique undefined count not to rise, resolving
 * `StreamNewGameState__5CMainFR12CInputStreami` frees exactly one, and `link_undefined.txt` was
 * 322 before this file existed. Every other callee is either defined by the port already
 * (`fn_80004AA0`, `fn_80004A4C`, `CMemoryInStream`, `CBitStreamReader`, `EnsureOptions`,
 * `rstl::single_ptr::operator=`) or written here from those two primitives, with retail's own
 * loop shapes measured above.
 */

#include "MetroidPrime/CMain.hpp"

#include "Kyoto/Streams/CBitStreamReader.hpp"
#include "Kyoto/Streams/CMemoryInStream.hpp"

#include "MetroidPrime/CGameGlobalObjects.hpp"
#include "MetroidPrime/Player/CGameOptions.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CGameStateBlocks.hpp"

#include <stddef.h>
#include <stdint.h>

extern "C" {
// Retail `.text:0x80004AA0`, `0xFC` - the deep copy of one `SGameStateBlock`, count and capacity
// copied and `allocate(capacity)` + byte copy otherwise. Defined by the port in
// `src/MetroidPrime/Player/CGameStateBlockCopyCtor.cpp`.
SGameStateBlock* fn_80004AA0(SGameStateBlock* self, const SGameStateBlock* src);
// Retail `.text:0x80004A4C`, `0x54` - `Free(x0c_data)`, and `Free(self)` only when the flag is
// positive. Defined by the port in `src/MetroidPrime/Player/CGameStateBlockDtor.cpp`.
SGameStateBlock* fn_80004A4C(SGameStateBlock* self, int flag);
// Retail `.text:0x80144140`, `0x684` - `CGameState`'s stream constructor, the only writer of the
// members this function copies into afterwards. **Not defined by the port**:
// `src/MetroidPrime/Player/CGameStateStreamCtor.cpp` is in `tools/check_files_cmake.py`'s
// `EXCLUDED` list because listing it opens twenty-one symbols and closes none.
//
// The second parameter is a `CBitStreamReader&`, which is not what
// `include/MetroidPrime/Player/CGameState.hpp:94` says (`CGameState(CInputStream&, int)`) and not
// what `CGameStateStreamCtor.cpp:343` declares either. Both are the tree's reading; retail's
// bytes settle it: `StreamNewGameState` passes `&r1+8`, the `CBitStreamReader` built at
// 0x80005498, and `fn_80144140` then calls `ReadBits__16CBitStreamReaderFUi` on that pointer
// (0x80144314, 0x80144320, ...), `GetInputStream__16CBitStreamReaderFv` on it (0x801443FC) and
// passes it to `__ct__12CPlayerStateFiR16CBitStreamReader` (0x80144420). `CBitStreamReader` has
// no vtable - its constructor only stores `x0_stream` at `+0x00` (0x80342F58) - so it is not a
// `CInputStream` and cannot be passed as one. Declared `extern "C"`, so this disagrees with no
// other object at link time; `CGameStateStreamCtor.cpp` is not in the port's build.
void fn_80144140(CGameState* self, CBitStreamReader& in, int saveIdx);
} // extern "C"

namespace {

// Retail `fn_80004C90` (0x80004C90, 0x44) copies `x00_count` and then calls `fn_80004CD4`
// (0x80004CD4, 0x68), a loop of `src->count` iterations that advances both pointers by 16 and
// calls `fn_80004D3C` (0x80004D3C, 0x20) -> `fn_80004D5C` (0x80004D5C, 0x24) -> `fn_80004AA0`.
// Neither `fn_80004C90` nor `fn_80004CD4` is in the port, so the same two steps are written from
// the primitive that is. Neither retail loop is bounded by 3 either: `fn_80004CD4` stops on the
// counter alone, and `x00_count` is written by `fn_80144924(this, 3, ...)` (0x80144924), which is
// what makes it 3.
void CopySlots(SGameStateSlots* dst, const SGameStateSlots* src) {
  dst->x00_count = src->x00_count;
  const int count = src->x00_count;
  for (int i = 0; i < count; ++i) {
    fn_80004AA0(&dst->x04_blk[i], &src->x04_blk[i]);
  }
}

// Retail `__dt__80004B9C` (0x80004B9C, 0x50): null test, then `fn_80004BEC` (0x80004BEC, 0x60) -
// a loop of `self->x00_count` iterations over `self+4`, stride 16, calling `fn_80004C4C`
// (0x80004C4C, 0x20) each time - then `extsh. r0,r31` / `ble`, so the `Free(self)` tail runs only
// for a positive flag and the `-1` every caller here passes skips it. `fn_80004C4C` ->
// `fn_80004C6C` -> `fn_80004A4C(blk, -1)`, so all three spell the loop below.
void ReleaseSlots(SGameStateSlots* slots) {
  const int count = slots->x00_count;
  for (int i = 0; i < count; ++i) {
    fn_80004A4C(&slots->x04_blk[i], -1);
  }
}

// Retail `fn_80142944` (0x80142944, 0x68), reached from `StreamNewGameState` through
// `fn_80142FA4` (0x80142FA4, 0x24: `addi r3,r3,272` then tail-call) and `fn_80142920`
// (0x80142920, 0x24, `addi r3,r3,324`): self-assign check, `fn_80004BEC(dst)` to release the
// destination, a range copy-construct of `src->x00_count` elements, then `dst->x00_count =
// src->x00_count`. That is `ReleaseSlots` followed by `CopySlots`, which is why both exist.
void AssignSlots(SGameStateSlots* dst, const SGameStateSlots* src) {
  if (dst == src) {
    return;
  }
  ReleaseSlots(dst);
  CopySlots(dst, src);
}

// Retail `fn_80142800` (0x80142800, 0x114), reached from `StreamNewGameState` through
// `fn_801427DC` (0x801427DC, 0x24: `addi r3,r3,376` then tail-call): self-assign check,
// `fn_80142914(dst)` (`x04_count = 0`), then either `Free(dst->x0c_data)` when the source is
// empty or a reserve-and-byte-copy. Releasing and re-copying instead of reusing the buffer is the
// same result in the block: `fn_80004AA0` writes `x04_count`, `x08_cap` and a fresh `x0c_data`
// before it copies, so nothing is leaked, nothing is freed twice and the bytes are the source's.
void AssignBlock(SGameStateBlock* dst, const SGameStateBlock* src) {
  if (dst == src) {
    return;
  }
  fn_80004A4C(dst, -1);
  fn_80004AA0(dst, src);
}

} // namespace

extern "C" void StreamNewGameState__5CMainFR12CInputStreami(CMain* self, CInputStream& in) {
  CGameGlobalObjects* objects = self->GetGameGlobalObjects();
  CGameState* oldState = gpGameState;

  // 0x800053E8 / 0x800053EC / 0x800053F4, all read before the release at 0x80005458. Retail
  // reads them out of the `fn_80005108` local it built from `gpGameState+0x54` a few
  // instructions earlier; the local is a copy of that member, so the same three words are read
  // off the member itself. `x28` is the save-slot index that picks the record at 0x80005474.
  //
  // **After the merge to upstream PrimeDecomp/echoes, two of the three go through an accessor.**
  // `+0x54` is upstream's `mSystemOptions`, whose `+0x28` is `CPersistentOptions::mSaveIdx` and
  // is private, so the word is read through the `SGameStateCardOpts` overlay in
  // `CGameStateBlocks.hpp` - the same 0x2C shape, the same `+0x28`. `+0x108` is upstream's
  // `cardSerialA`/`cardSerialB` pair, and `GetCardSerial()` reassembles it as
  // `(u64(cardSerialA) << 32) | cardSerialB`, which is the value the old 0x108 `u64` held.
  const int slotIdx = reinterpret_cast< const SGameStateCardOpts* >(&oldState->SystemOptions())->x28;
  const u64 cardSerial = oldState->GetCardSerial();

  SGameStateSlots local110; // 0x800053F8, r1+0x7C, from gpGameState+0x110
  SGameStateBlock local188; // 0x80005400, r1+0x24, from gpGameState+0x188
  SGameStateSlots local144; // 0x80005434, r1+0x48, from gpGameState+0x144
  SGameStateBlock local178; // 0x80005444, r1+0x14, from gpGameState+0x178
  {
    // The two blocks retail builds first, in its order.
    CopySlots(&local110, &oldState->x110);
    fn_80004AA0(&local188, &oldState->x188);

    // 0x8000540C-0x80005424. `lwz r0,40(r1)` is `local188.x04_count`, and `clrlwi. r0,r26,24`
    // is the **low byte of the `CInputStream&` itself**, not a dereference: `r26 = r4` at
    // 0x800053CC and nothing reassigns it. All three retail callers pass 0 or 1 there -
    // `li r4,0` at 0x8001dd90, `li r4,1` at 0x8002055c, `li r4,0` at 0x80143a38 - so the
    // conjunct is "the caller passed address 0", and every one of them passes a reference
    // anyway; `&in` is that address.
    const bool liveRecord = local188.x04_count != 0 && (reinterpret_cast< uintptr_t >(&in) & 0xff) == 0;

    CopySlots(&local144, &oldState->x144);
    fn_80004AA0(&local178, &oldState->x178);

    // 0x80005454, r1+0xDC. Retail's is `fn_80004E84` (0x80004E84, 0xD8) - a real copy of a
    // class that owns an `rstl::vector<SObjectTag>` and an `rstl::reserved_vector` - and its
    // destructor is `fn_80004D84` (0x80004D84, 0xAC), which retail's symbol table does not name
    // and this tree's does: it is `CGameOptions::~CGameOptions()`, already one of the port's
    // undefined symbols (referenced by `CGameGlobalObjectsCtor.cpp.o` and `main.cpp.o`), so
    // running it here costs no new one. The copy and the assignment are the implicit member-wise
    // pair, whose `rstl` members are templates defined in their headers.
    CGameOptions local80(oldState->GameOptions());

    // 0x80005458-0x80005470. `fn_80004154(&ggo->x130, 0)` is retail's out-of-line
    // `rstl::single_ptr<CGameState>::operator=(T*)`, and `gameStateSlot` in
    // `CMainResetGameState.cpp` spells the same offset there for the same reason - that file is
    // a mwcceppc unit reproducing 32-bit bytes, this one is not, so `GameState()` is the
    // accessor instead of `+0x130`.
    objects->GameState() = nullptr;
    gpGameState = nullptr;

    // 0x80005474-0x80005484. The record is 16 bytes read as `{u32 count, void* data, u32 cap,
    // u32 size}` only in the sense that `lwz r4,12(r5)` feeds `CMemoryInStream`'s length and
    // `lwz r5,4(r5)` its pointer: `x04_count` is the **byte** size and `x0c_data` the buffer,
    // which is the word `CGameStateBlocks.hpp` documents for the byte-buffer instance. The
    // alternative source is `r1+0x80 + 16*slotIdx`, i.e. `local110.x04_blk[slotIdx]`.
    const SGameStateBlock& record = liveRecord ? local188 : local110.x04_blk[slotIdx];

    // 0x80005494 and 0x800054A0, then 0x800054E0/0x800054F8 destroy them in the other order.
    CMemoryInStream memStream(record.x0c_data, record.x04_count);
    CBitStreamReader reader(memStream);

    // 0x800054B4 is `::operator new(752, "??(??)..", 0)` and 0x800054C4 the constructor; 752 is
    // retail's `sizeof(CGameState)`, and this is the host's, which is what `new CGameState(...)`
    // in `main.cpp` allocates too. The null test is retail's own (`mr. r4,r3` / `beq`).
    CGameState* newState = static_cast< CGameState* >(::operator new(sizeof(CGameState)));
    if (newState != nullptr) {
      fn_80144140(newState, reader, 0);
    }

    // 0x800054D4 publishes, 0x80005500 reads it back into `gpGameState`.
    objects->GameState() = newState;
    gpGameState = objects->GameState().get();

    // 0x80005518-0x80005538, retail's four surviving carry-overs: the new state's `+0x110` and
    // `+0x144` slots, its `+0x178` block and its `+0x80` options all come from the old state
    // rather than from the stream. The fifth, `fn_80003F08(&new->x54, ...)`, is the one not
    // reproduced - see the header.
    AssignSlots(&gpGameState->x110, &local110);
    AssignSlots(&gpGameState->x144, &local144);
    AssignBlock(&gpGameState->x178, &local178);
    gpGameState->GameOptions() = local80;

    // 0x80005550, on `gpGameState+0x80` - `CGameOptions` is at `+0x80` in
    // `include/MetroidPrime/Player/CGameOptions.hpp`'s own row of `CGameState.hpp`.
    gpGameState->GameOptions().EnsureOptions();

    // 0x8000555C/0x80005560: `stw r29,268(r3)` then `stw r30,264(r3)`, both loaded from the old
    // state at 0x800053EC/0x800053F4 - the two halves of `cardSerial` at `+0x108`, which are
    // upstream's `cardSerialB` and `cardSerialA`. `SetCardSerial` splits the value the same way.
    gpGameState->SetCardSerial(cardSerial);

    // 0x80005568 `fn_80142FEC(new)` when `liveRecord` - not reproduced, see the header.
  } // ~local80, retail's first destructor at 0x80005570

  // Retail's remaining five, reverse order: 0x8000557C r1+0x14, 0x80005588 r1+0x48,
  // 0x80005594 r1+0x24, 0x800055A0 r1+0x7C. The sixth, 0x800055AC r1+0xB0, is the
  // `SGameStateCardOpts` local this file does not build. The POD locals have no destructor of
  // their own, which is why they are released by hand, and the `CGameOptions` one above is why
  // they are in this exact order rather than at the end of the scope.
  fn_80004A4C(&local178, -1);
  ReleaseSlots(&local144);
  fn_80004A4C(&local188, -1);
  ReleaseSlots(&local110);
}
