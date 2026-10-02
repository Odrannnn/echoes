// `LdrToEntityInfo`, both overloads, read off retail's own instructions:
// `tools/dis.sh 0x80239BD4 0x38`, which is the whole of
// `LdrToEntityInfo__FRC11CEntityInfoRC20SLdrEditorProperties` in `config/G2ME01/symbols.txt`.
//
// ## Which of the two is the forwarder - asked and answered before either was written
//
// Retail has ONE of them, not two. `symbols.txt` lists exactly one `LdrToEntityInfo` (the
// non-const `FR11CEntityInfoRC20SLdrEditorProperties` at 0x80239BD4, 0x38), and every `bl`
// to that address in `objdump -d build/G2ME01/main.elf` lands there - 91 of them, counting
// the ones inside the loaders whose `info` parameter is spelled `const CEntityInfo&`:
// `LoadPickup` (call at 0x800B3FA4), `LoadHUDMemo`, `LoadSequenceTimer`, `LoadStreamedAudio`
// and `LoadAreaProperties`. Those four are exactly the objects that bind to the *const*
// overload in this tree (`CScriptPickup.cpp.o`, `CScriptStreamedMusic.cpp.o`,
// `CScriptSequenceTimer.cpp.o`, `CScriptHUDMemo.cpp.o` - `build/goal/judge/undef.base.txt:159`),
// because they pass their const `info` through without a `const_cast`, which is what the other
// five objects in the port's link do (`undef.base.txt:160`).
//
// So retail's non-const is the body, and the const overload is the port's own: a forwarder
// over a `const_cast`, which is the same call retail's four loaders make. A `const_cast`
// costs no instruction, so `LoadPickup` passing a `const CEntityInfo&` straight into a
// `CEntityInfo&` parameter *is* this function - the cast is invisible at 0x800B3FA4.
//
// ## The body, instruction by instruction (`tools/dis.sh 0x80239BD4 0x38`)
//
//   lbz r0,52(r4)            props.active, the bool at SLdrEditorProperties +0x34
//   lbz r5,24(r3)            CEntityInfo's flags byte at +0x18
//   rlwimi r5,r0,7,24,24     r0's bit 0 -> the byte's bit 7: `info.active`
//   stb r5,24(r3)            store 1 of three read-modify-writes of that byte
//   lwz r0,56(r4)            props.unknown_0x5d298a43, the word at +0x38
//   clrlwi r5,r0,31          GNU's spelling of `rlwinm r5,r0,0,31,31`: r0 & 1
//   rlwinm r4,r0,31,31,31    = (r0 >> 1) & 1
//   lbz r0,24(r3)
//   rlwimi r0,r5,6,25,25     -> bit 6: `info.scriptingBlocked`
//   stb r0,24(r3)            store 2
//   lbz r0,24(r3)
//   rlwimi r0,r4,5,26,26     -> bit 5: `info.unk`
//   stb r0,24(r3)            store 3
//   blr                      r3 is still `info`, which is the return value
//
// The three read-modify-write pairs of the same byte are mwcceppc's code for three
// independent field assignments in source order, so retail's source order *is* `active`,
// `scriptingBlocked`, `unk` - the same rule `docs/research/real_loaders.md` item 5 used for
// the stores in `LoadAreaProperties`. The bit positions are MWCC's `uchar` bitfield layout,
// MSB first, and the constructor at 0x800484D4 proves it independently: its `bool active`
// parameter goes to bit 7 with the same `rlwimi ...,7,24,24`, and its two `li r4,1` initial
// `scriptingBlocked` and `unk` to bits 6 and 5.
//
// The offsets are retail's struct, read off that constructor rather than assumed:
// `areaId` 0x00, `conns` 0x04..0x14 (`rstl::vector` is allocator + count + capacity + items,
// four words), `editorId` 0x14 (`stw r0,20(r29)`), flags byte 0x18 (`lbz r0,24(r29)`).
// `SLdrEditorProperties` is `name` 0x00, `transform` 0x10 (three CVector3f), `active` 0x34,
// `unknown_0x5d298a43` 0x38 - the declaration order of
// `include/MetroidPrime/ScriptLoader/Structs/SLdrEditorProperties.hpp`.
//
// Nothing else runs. The name suggests a conversion, but there is no area id, no connection
// list and no call in the 0x38 bytes: the "conversion" is these three bits, which is why the
// function is a friend of `CEntityInfo` and not a setter - `symbols.txt` names no setter.
//
// Two host/retail differences that are not logic, recorded so nobody re-derives them:
// retail masks `props.active` to its bit 0 (`rlwimi r5,r0,7,24,24` reads r0's bit 0 only)
// where the host compiler ORs the whole `bool` byte in, which is the same value for the 0/1
// that `LoadTypedefEditorProperties`' `ReadBool()` arm writes (`ReadUint8() != 0`, retail
// `0x8023F014`) and differs only on a byte that is not a valid `bool` at all; and the flags
// byte sits at +0x24 on the host rather than retail's +0x18, because `rstl::vector`'s item
// pointer is eight bytes here and the struct grows with it. Nothing in this body reads a raw
// offset, so the field names are its whole interface.
//
// ## Why this file is port-only (in `files.cmake`, absent from `configure.py`)
//
// 0x80239BD4 sits in the same unclaimed `.text` range as `LoadTypedefEditorProperties`
// (0x8023EF3C): the nearest split blocks are `MetroidPrime/ScriptLoader/RubiksPuzzle.cpp`
// ending 0x802399F4 and `MetroidPrime/ScriptLoader.cpp` starting 0x80242894. No unit owns
// these bytes, so there is nothing for `configure.py` to mark `Matching` and nothing for
// `tools/flip_test.sh` to flip; carving the range would be four files in one change and a
// different job. The file is listed in `files.cmake` only, so the port's `mp_game` compiles
// it and both symbols leave the port's undefined list. Precedent:
// `src/MetroidPrime/ScriptLoader/Structs/SLdrEditorProperties_Load.cpp`.

#include "MetroidPrime/CEntityInfo.hpp"

#include "MetroidPrime/ScriptLoader/Structs/SLdrEditorProperties.hpp"

CEntityInfo& LdrToEntityInfo(CEntityInfo& info, const SLdrEditorProperties& props) {
  info.mActive = props.active;
  info.mUpdateWhileOccluded = props.unknown_0x5d298a43 & 1;
  info.mUpdateDuringCinematicSkip = (props.unknown_0x5d298a43 >> 1) & 1;
  return info;
}

const CEntityInfo& LdrToEntityInfo(const CEntityInfo& info, const SLdrEditorProperties& props) {
  return LdrToEntityInfo(const_cast<CEntityInfo&>(info), props);
}
