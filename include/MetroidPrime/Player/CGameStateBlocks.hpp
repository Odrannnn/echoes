#ifndef _CGAMESTATEBLOCKS
#define _CGAMESTATEBLOCKS

#include "types.h"

// The member shapes `CGameState`'s `+0x110` and `+0x144` blocks are built out of, named so
// that the functions that touch them can be written as C++ instead of as raw offsets - and,
// since the merge to upstream PrimeDecomp/echoes, the three further overlays at `+0x54`,
// `+0x1A0` and `+0x204` that used to live in `include/MetroidPrime/Player/CGameState.hpp`.
//
// **Upstream's `CGameState` models the three of those three ranges as classes of its own**, so the
// overlays below are views onto bytes that a named member already owns and none of them adds one.
// Each is at the same offset, with the same total size, as the member it views; `SGameState.hpp`
// says which member that is for each. They exist because the port's retail-named functions
// (`fn_80145950`, `fn_80007040`, `fn_80009DBC`, `fn_80009898`, `fn_800098CC`, `fn_80009AC0`) are
// written against these shapes and are not allowed to be renamed into upstream's classes: retail's
// symbol table gives them no name, so the port's `extern "C"` definitions are what objdiff pairs.
//
// Every offset and size here is measured out of `build/G2ME01/main.elf`; nothing is recalled.
// The evidence, and what each row is pinned by:
//
//   SGameStateBlock, **16 bytes**: `+0x00` unknown, `+0x04` the element count, `+0x08` the
//   capacity, `+0x0C` the data pointer.
//     * `fn_801466F4` (0x801466F4), called on `CGameState+0x08`, indexes its data as
//       `data + count * 36` and grows it with `allocate__Q24rstl17rmemory_allocatorFi(8(this))`
//       - so the base pointer's `+0x04` is the count, `+0x08` the capacity and `+0x0C` the data.
//     * `fn_8014260C` (0x8014260C), also on `CGameState+0x08`, walks it as
//       `cursor = *(void**)(base+0x0C)` against `*(uint*)(base+0x04) + *(uint*)(base+0x04)*36`.
//     * `fn_80004AA0` (0x80004AA0), the block's own copy, reads `4(src)`, `8(src)` and
//       `12(src)`, writes the same three words on the destination and then memcpy's the bytes -
//       with `4(dst)` as a **byte** size (`srwi. r0,3` for the eight-bytes-at-a-time loop) and
//       `8(dst)` as a **byte** capacity.
//     * `fn_80142DD4` (0x80142DD4) reads `12(this + 0x148 + i*16)` for i = 0, 1, 2, which is the
//       `+0x0C` data pointer of the second block's third element.
//   So `x04_count` is an element count in the 36-byte case and a **byte** size in the byte-buffer
//   case; both are the same word and the difference is in what the code does with it, not in the
//   layout. `x00_unk` is read by nothing and written by nothing in the DOL.
//
//   `SGameStateSlots`, **0x34 = 52 bytes**: `x00_count` then three 16-byte elements.
//     * `fn_80144924` (0x80144924) is its constructor: `stw r4,0(r3)` writes the count and the
//       fill loop strides by 16 (`addi r31,r31,16` at 0x8014499C), so the element is 16 bytes.
//     * `fn_80142DD4` (0x80142DD4) confirms the second instance's extent: `slwi r0,r4,4;
//       add r31,r30,r0; addi r31,r31,328` is `this + 0x148 + i*16`, i = 0, 1, 2 - so the block at
//       `CGameState+0x144` runs `0x144 .. 0x178`, which is 0x34.
//     * `CGameState+0x110 + 0x34 == CGameState+0x144` exactly, so the first instance is the same
//       size without needing a second measurement.
struct SGameStateBlock {
  u32 x00_unk;    //!< +0x00: read and written by nothing in the DOL
  u32 x04_count;  //!< +0x04: the element count, or a byte size in the byte-buffer instance
  u32 x08_cap;    //!< +0x08: the capacity, in the same unit as x04_count
  void* x0c_data; //!< +0x0C: the data block
};
CHECK_SIZEOF(SGameStateBlock, 0x10)

struct SGameStateSlots {
  int x00_count;            //!< +0x00
  SGameStateBlock x04_blk[3]; //!< +0x04, stride 16
};
CHECK_SIZEOF(SGameStateSlots, 0x34)

// +0x54 .. +0x7F, **0x2C**, and the same shape as `CPersistentOptions`, which is what upstream's
// `mSystemOptions` at `+0x54` is. `fn_80145950` (0x80145950, 0x5C - a `Matching` unit, see
// `src/MetroidPrime/Player/CGameStateCardOptsCtor.cpp`) calls `fn_80146154(this, 0)` - the
// `fn_80146154` that constructs `CPersistentOptions` at `CGameState+0xDC` is called with `r4 = 1`
// - and then zeroes its own `+0x1C`, `+0x20`, `+0x24` and `+0x28` (absolute 0x70, 0x74, 0x78,
// 0x7C). 0x54 + 0x2C = 0x80, which is `gameOptions`, so the extent is exact. It also ends with
// `if (gpMemoryCard) fn_80145628(this)` (`lwz r0,-28356(r13)` at 0x80145980, the same global test
// `fn_801449C8` does at 0x80144C50), so the constructor is more than the shared prefix.
//
// **It is not a different class, and upstream is right that it is not:** the four words it zeroes
// are the same four `CPersistentOptions` at `+0xDC` has. It keeps the name it had before the merge
// because that is what every call site in the port spells, and because it names the four named
// words the upstream header still leaves as one `char x0_[0x28]` padding array - `mSaveIdx` is
// `+0x28` and is named there, so `x28` below is that member seen under the pre-merge name.
struct SGameStateCardOpts {
  u8 x00[0x1C]; //!< +0x00..+0x1B, 0x54..0x6F - `fn_80146154` writes +0x04 and +0x05
  u32 x1c;      //!< +0x1C (0x70)
  u32 x20;      //!< +0x20 (0x74)
  u32 x24;      //!< +0x24 (0x78)
  u32 x28;      //!< +0x28 (0x7C) - `CPersistentOptions::mSaveIdx` in upstream's header
};
CHECK_SIZEOF(SGameStateCardOpts, 0x2c)

// +0x1A0 .. +0x1F3, **0x54**, from `fn_80007040` (0x80007040), which the constructor calls on
// `CGameState+0x1A0` at 0x801442C0 and which `CMainFlow::AdvanceGameState` reads at 0x8001DEF4
// (`lwz r4,416(r4)`). It zeroes +0x00, a byte at +0x04, +0x08 and +0x0C, then calls
// `fn_800070A4(this+0x1B0, 4, temp)` - which writes 4 at 0x1B0 and copies four 14-byte records
// at stride 16 from 0x1B4, so 0x1B0 + 4*16 = 0x1F0 and the tail 0x1F0..0x1F3 is four bytes the
// constructor does not touch. `fn_80003BE8(this+0x1A0, local)` (0x801444DC) is its copy
// assignment, and `fn_80003F08(new, local)` in `src/MetroidPrime/main.cpp` is the same shape.
//
// **The next member therefore starts at 0x1F4, not 0x1F0**: 0x1F4 is the `x00_unk` word of the
// next `SGameStateBlock`, whose `x04_count`, `x08_cap` and `x0c_data` are the three words the
// constructor zeroes at 0x1F8, 0x1FC and 0x200 (`stw r0,504(r30)`, `508`, `512` - 0x801442CC,
// 0x801442D4, 0x801442D8) and whose data word lands exactly on 0x200, four bytes below the
// `addi r3,r30,516` that starts the memcard block. That is what pins 0x54 here rather than
// 0x50 or 0x58.
//
// **After the merge this is a view, not a member.** Upstream declares `int mGameModeType` at
// `+0x1A0` - retail's `mGameModeType` is the `lwz r4,416(r4)` at 0x8001DEF4 - and one
// `char x1a4_[0x50]` for the rest of it, so the port reaches this range through
// `&CGameState::mGameModeType`. `SGameState.hpp` spells that out.
struct SGameStateWorlds {
  u32 x00;           //!< +0x00 (0x1A0) - `CMainFlow::AdvanceGameState` compares it to 0x949A
  u8 x04;            //!< +0x04 (0x1A4)
  u32 x08;           //!< +0x08 (0x1A8)
  u32 x0c;           //!< +0x0C (0x1AC)
  u32 x10_count;     //!< +0x10 (0x1B0) - 4
  u8 x14_rec[4][16]; //!< +0x14 (0x1B4) .. +0x53 (0x1F3), stride 16, 14 bytes used of each
};
CHECK_SIZEOF(SGameStateWorlds, 0x54)

// +0x204 .. +0x2EB, **0xE8**, and not a set of scalars: `fn_80009DBC(this+0x204, 0)`
// (0x801442DC) writes 76 at +0x00, then fills +0x04..+0x4F with a 4-byte global repeated 19
// times four bytes at a time, then writes 76 at +0x50 and fills +0x54..+0x9F the same way. So it
// is one object with two 76-byte buffers, and it reaches to 0x204+0x9F = 0x2A3 at least.
// **0xE8 is exact, not a guess**: 0x204 + 0xE8 = 0x2EC, which is the flag byte the constructor's
// last three `rlwimi`/`stb` pairs write, and nothing past 0x2EC exists in the object.
//
// **Corrected 2026-09-26, from `fn_80009DBC` itself** (now
// `src/MetroidPrime/Player/CGameStateMemcardCtor.cpp`): the two ends of the tail are written, and
// only the middle is unrecovered. `stw r0,160(r31)` at 0x80009E50 zeroes +0xA0, and
// `stw r4,228(r31)` at 0x80009E54 stores the constructor's second argument at +0xE4 - so
// +0x2A4..+0x2E7, 0x40 bytes, is what nothing writes (the flag lands at 0x2E8). The fill byte
// is *not* one global: the first buffer is filled from `lbl_80417D90` and the second from
// `lbl_80417D91` (`.sdata:0x80417D90` and `+0x91`, both one-byte objects, values 1 and 0), read
// as `lbz r0,-32752(r13)` and `lbz r0,-32751(r13)`. And `fn_80009DBC`'s own last act is
// `fn_80009898(this)`, whose inner `fn_800098CC` writes 72 at +0x50 and fills +0x54..+0x9B from
// a third byte at `lbl_80417D8D` - so the 76 this constructor stores at +0x50 does not survive
// its own call.
//
// **After the merge every one of those seven rows is upstream's `CControlMapper`, and each offset
// agrees**: `CControlMapper` is `CHECK_SIZEOF(..., 0xe8)` upstream, and its three
// `rstl::reserved_vector`s are `{ int mCount; uchar mData[N]; }` with `N` = 76, 76 and 8 pairs -
// `mCount` at `+0x00`/`+0x50`/`+0xA0`, the bytes at `+0x04`/`+0x54`/`+0xA4` - and
// `int mControlScheme` at `+0xE4`. `fn_80009DBC` is upstream's
// `CControlMapper::CControlMapper(int controlScheme)` (76 enabled, 76 overridden, then `Reset()`),
// and `fn_80009898` is its `Reset`. This struct stays because the port's four `extern "C"`
// definitions of those four retail-named functions are not allowed to be renamed into the class,
// and because the type a `reinterpret_cast` needs has to exist somewhere. It is reached through
// `&CGameState::ControlMapper()`; see `SGameState.hpp`.
struct SGameStateMemcard {
  u32 x00_size;    //!< +0x00 (0x204), 76 - `CControlMapper::mCommandEnabled`'s count
  u8 x04_buf[76];  //!< +0x04 (0x208) .. +0x4F (0x253), filled with `lbl_80417D90` (1)
  u32 x50_size;    //!< +0x50 (0x254), 76 - overwritten with 72 by `fn_800098CC`
  u8 x54_buf[76];  //!< +0x54 (0x258) .. +0x9F (0x2A3), filled with `lbl_80417D91` (0)
  u32 xa0_unk;     //!< +0xA0 (0x2A4) - zeroed by `fn_80009DBC` (`stw r0,160(r31)`)
  u8 xa4_unk[0x40]; //!< +0xA4 (0x2A8) .. +0xE3 (0x2E7), unrecovered: nothing writes it
  int xe4_flag;    //!< +0xE4 (0x2E8) - `fn_80009DBC`'s second argument (`stw r4,228(r31)`)
};
CHECK_SIZEOF(SGameStateMemcard, 0xe8)

// `fn_80142A10` - retail .text:0x80142A10, size:0x20 = 32 bytes. The copy constructor of the
// 16-byte element, and the only thing the fill loop calls. It forwards `r3` unchanged to
// `fn_80004D5C` and touches nothing itself, so its own 8 instructions are a frame and a tail call.
//
// `fn_80004D5C` (0x80004D5C, 0x24) is `if (this != nullptr) fn_80004AA0(this, src);` - the null
// test is on the *pointer*, and `fn_80004AA0` is the copy that reads `4/8/12` of the source. So
// the element copy is retail's `if (this)` copy constructor and the shape above is what it copies.
extern "C" void fn_80142A10(SGameStateBlock* self, const SGameStateBlock* src);

#endif // _CGAMESTATEBLOCKS
