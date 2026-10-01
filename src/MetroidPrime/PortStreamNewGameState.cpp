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
 * | 0x800053F8 | local r1+0x7C = copy of `gpGameState+0x110` (`fn_80004C90`) | `reserved_vector` copy |
 * | 0x80005400 | local r1+0x24 = copy of `gpGameState+0x188` (`fn_80004AA0`) | `vector` copy |
 * | 0x8000540C-0x80005424 | `flag = local.r1+0x28 != 0 && (u8)in == 0` | `liveRecord` |
 * | 0x80005434 | local r1+0x48 = copy of `gpGameState+0x144` (`fn_80004C90`) | `reserved_vector` copy |
 * | 0x80005444 | local r1+0x14 = copy of `gpGameState+0x178` (`fn_80004AA0`) | `vector` copy |
 * | 0x80005454 | local r1+0xDC = copy of `gpGameState+0x80` (`fn_80004E84`) | `CGameOptions` copy |
 * | 0x80005458 | release the old `CGameState` (`fn_80004154(slot, 0)`) | `GameState() = nullptr` |
 * | 0x80005470 | `gpGameState = 0` | same |
 * | 0x80005474-0x80005484 | pick the record: r1+0x24 if flag, else `r1+0x80 + 16*idx` | `record` |
 * | 0x80005494 | `CMemoryInStream(record.data, record.size)` | same |
 * | 0x800054A0 | `CBitStreamReader(that)` | same |
 * | 0x800054B4 | `::operator new(752, "??(??)..", 0)` | `::operator new(sizeof(CGameState))` |
 * | 0x800054C4 | `fn_80144140(&reader)` - the `CGameState` constructor | `CGameState(reader)` |
 * | 0x800054D4 | publish into `gameGlobalObjects+0x130` | `GameState() = ...` |
 * | 0x800054E0/0x800054F8 | destroy the bit reader, then the memory stream | RAII, same points |
 * | 0x80005500 | `gpGameState = the new one` | same |
 * | 0x80005518 | `fn_80142FA4` - copy the slots local into `+0x110` | `operator=` |
 * | 0x80005524 | `fn_80142920` - copy the slots local into `+0x144` | `operator=` |
 * | 0x80005530 | `fn_801427DC` - copy the block local into `+0x178` | `operator=` |
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
 * 1. **The `CPersistentOptions` copy at `CGameState+0x54` and its carry-over into the new state**
 *    (`fn_80005108`, 0x80005108, 0x50; `fn_80003F08`, 0x80003F08, 0x50;
 *    `__dt__PersistentOptions_800050A4`, 0x800050A4, 0x64). Left out when the host modelled the
 *    member as opaque bytes, because a shallow copy of its two heap-owning sub-objects would have
 *    been a double free. Since 2026-10-01 the member is the real class and the copy is
 *    expressible; it is still not written, because nothing has measured it on the port.
 *    **The one thing that local is read for survives**: retail loads the save-slot index out of
 *    it at 0x800053E8, and that is `SystemOptions().GetSaveIdx()`, read here before the release.
 * 2. **`fn_80142FEC(new)`** (0x80142FEC, 0x80), called only when `flag` is set. Unnamed in
 *    `config/G2ME01/symbols.txt`, unwritten, and calling it would add a symbol the port does not
 *    define. The condition and the call site are recorded here so the next lane does not have to
 *    re-derive them: `80005564 beq 80005570` tests the same `flag` byte.
 *
 * Until 2026-10-01 the host kept an opaque `CGameState` layout and this file copied the four
 * compressed blocks by hand through `fn_80004AA0`/`fn_80004A4C` and built the new state through
 * the undefined `fn_80144140`. The host now uses upstream's layout, so the blocks are the real
 * `rstl` members, their copies and assignments are the containers' own, and the constructor is
 * `CGameState(CBitStreamReader&)` from `Player/CGameState.cpp` (`CGameState.hpp` befriends this
 * function for the four private members).
 */

#include "MetroidPrime/CMain.hpp"

#include "Kyoto/Streams/CBitStreamReader.hpp"
#include "Kyoto/Streams/CMemoryInStream.hpp"

#include "MetroidPrime/CGameGlobalObjects.hpp"
#include "MetroidPrime/Player/CGameOptions.hpp"
#include "MetroidPrime/Player/CGameState.hpp"

#include <stddef.h>
#include <stdint.h>

extern "C" void StreamNewGameState__5CMainFR12CInputStreami(CMain* self, CInputStream& in) {
  CGameGlobalObjects* objects = self->GetGameGlobalObjects();
  CGameState* oldState = gpGameState;

  // 0x800053E8 / 0x800053EC / 0x800053F4, all read before the release at 0x80005458. Retail
  // reads them out of the `fn_80005108` local it built from `gpGameState+0x54` a few
  // instructions earlier; the local is a copy of that member, so the same three words are read
  // off the member itself. `x28` is the save-slot index that picks the record at 0x80005474.
  //
  // `+0x54` is `mSystemOptions`, whose `+0x28` is `CPersistentOptions::mSaveIdx`; `+0x108` is the
  // `cardSerialA`/`cardSerialB` pair, which `GetCardSerial()` reassembles.
  const int slotIdx = oldState->SystemOptions().GetSaveIdx();
  const u64 cardSerial = oldState->GetCardSerial();

  // Since 2026-10-01 the host uses upstream's `CGameState` layout, so the four blocks are the
  // real `rstl` members and their copies, assignments and destructors are the containers' own.
  // 0x800053F8, r1+0x7C, from gpGameState+0x110, and 0x80005400, r1+0x24, from gpGameState+0x188.
  rstl::reserved_vector< rstl::vector< uchar >, 3 > local110(oldState->mCompressedGameStates);
  rstl::vector< uchar > local188(oldState->mCheckpointGameState);
  {

    // 0x8000540C-0x80005424. `lwz r0,40(r1)` is `local188.x04_count`, and `clrlwi. r0,r26,24`
    // is the **low byte of the `CInputStream&` itself**, not a dereference: `r26 = r4` at
    // 0x800053CC and nothing reassigns it. All three retail callers pass 0 or 1 there -
    // `li r4,0` at 0x8001dd90, `li r4,1` at 0x8002055c, `li r4,0` at 0x80143a38 - so the
    // conjunct is "the caller passed address 0", and every one of them passes a reference
    // anyway; `&in` is that address.
    const bool liveRecord = local188.size() != 0 && (reinterpret_cast< uintptr_t >(&in) & 0xff) == 0;

    // 0x80005434, r1+0x48, from gpGameState+0x144, and 0x80005444, r1+0x14, from +0x178.
    rstl::reserved_vector< rstl::vector< uchar >, 3 > local144(oldState->mCompressedGameOptions);
    rstl::vector< uchar > local178(oldState->mCompressedMultiplayerOptions);

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
    // `lwz r5,4(r5)` its pointer - an `rstl::vector< uchar >`'s size and data. The alternative
    // source is `r1+0x80 + 16*slotIdx`, i.e. `local110[slotIdx]`.
    const rstl::vector< uchar >& record = liveRecord ? local188 : local110[slotIdx];

    // 0x80005494 and 0x800054A0, then 0x800054E0/0x800054F8 destroy them in the other order.
    CMemoryInStream memStream(record.data(), record.size());
    CBitStreamReader reader(memStream);

    // 0x800054B4 is `::operator new(752, "??(??)..", 0)` and 0x800054C4 the constructor; 752 is
    // retail's `sizeof(CGameState)`, and this is the host's, which is what `new CGameState(...)`
    // in `main.cpp` allocates too. The null test is retail's own (`mr. r4,r3` / `beq`).
    CGameState* newState = static_cast< CGameState* >(::operator new(sizeof(CGameState)));
    if (newState != nullptr) {
      new (newState) CGameState(reader);
    }

    // 0x800054D4 publishes, 0x80005500 reads it back into `gpGameState`.
    objects->GameState() = newState;
    gpGameState = objects->GameState().get();

    // 0x80005518-0x80005538, retail's four surviving carry-overs: the new state's `+0x110` and
    // `+0x144` slots, its `+0x178` block and its `+0x80` options all come from the old state
    // rather than from the stream. The fifth, `fn_80003F08(&new->x54, ...)`, is the one not
    // reproduced - see the header.
    gpGameState->mCompressedGameStates = local110;
    gpGameState->mCompressedGameOptions = local144;
    gpGameState->mCompressedMultiplayerOptions = local178;
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
  // 0x80005594 r1+0x24, 0x800055A0 r1+0x7C - the containers' own destructors here. The sixth,
  // 0x800055AC r1+0xB0, is the `CPersistentOptions` local this file does not build.
}
