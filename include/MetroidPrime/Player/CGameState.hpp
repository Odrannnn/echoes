#ifndef _CGAMESTATE
#define _CGAMESTATE

#include "types.h"

#include "MetroidPrime/TGameTypes.hpp"

#include "MetroidPrime/Player/CGameOptions.hpp"
#include "MetroidPrime/Player/CHintOptions.hpp"
#include "MetroidPrime/Player/CPersistentOptions.hpp"
#include "MetroidPrime/Player/CGameStateBlocks.hpp"

class CGameMode;
class CWorldState;
class CPlayerState;
class CInputStream;
class COutputStream;

// +0x54 .. +0x7F, **0x2C**, the same width as `CPersistentOptions` and built by a function
// that starts the same way: `fn_80145950` (0x80145950, 0x5C - a `Matching` unit, see
// `src/MetroidPrime/Player/CGameStateCardOptsCtor.cpp`) calls `fn_80146154(this, 0)` - the
// `fn_80146154` that constructs `CGameState+0xDC` is called with `r4 = 1` - and then zeroes its
// own `+0x1C`, `+0x20`, `+0x24` and `+0x28` (absolute 0x70, 0x74, 0x78, 0x7C). 0x54 + 0x2C = 0x80,
// which is `gameOptions`, so the extent is exact. It also ends with
// `if (gpMemoryCard) fn_80145628(this)` (`lwz r0,-28356(r13)` at 0x80145980, the same global test
// `fn_801449C8` does at 0x80144C50), so the constructor is more than the shared prefix. It is
// *not* named `CPersistentOptions` because it has four extra zeroed words the class at +0xDC does
// not; the shared part is whatever `fn_80146154` writes, at `+0x00` (a word), `+0x04` and `+0x05`.
struct SGameStateCardOpts {
  u8 x00[0x1C]; //!< +0x00..+0x1B, 0x54..0x6F - `fn_80146154` writes +0x04 and +0x05
  u32 x1c;      //!< +0x1C (0x70)
  u32 x20;      //!< +0x20 (0x74)
  u32 x24;      //!< +0x24 (0x78)
  u32 x28;      //!< +0x28 (0x7C)
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
struct SGameStateWorlds {
  u32 x00;           //!< +0x00 (0x1A0) - `CMainFlow::AdvanceGameState` compares it to 0x949A
  u8 x04;            //!< +0x04 (0x1A4)
  u32 x08;           //!< +0x08 (0x1A8)
  u32 x0c;           //!< +0x0C (0x1AC)
  u32 x10_count;     //!< +0x10 (0x1B0) - 4
  u8 x14_rec[4][16]; //!< +0x14 (0x1B4) .. +0x53 (0x1F3), stride 16, 14 bytes used of each
};
CHECK_SIZEOF(SGameStateWorlds, 0x54)

// +0x204 .. +0x2EB, **0xE8**, and **not** a set of scalars: `fn_80009DBC(this+0x204, 0)`
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
struct SGameStateMemcard {
  u32 x00_size;    //!< +0x00 (0x204), 76
  u8 x04_buf[76];  //!< +0x04 (0x208) .. +0x4F (0x253), filled with `lbl_80417D90` (1)
  u32 x50_size;    //!< +0x50 (0x254), 76 - overwritten with 72 by `fn_800098CC`
  u8 x54_buf[76];  //!< +0x54 (0x258) .. +0x9F (0x2A3), filled with `lbl_80417D91` (0)
  u32 xa0_unk;     //!< +0xA0 (0x2A4) - zeroed by `fn_80009DBC` (`stw r0,160(r31)`)
  u8 xa4_unk[0x40]; //!< +0xA4 (0x2A8) .. +0xE3 (0x2E7), unrecovered: nothing writes it
  int xe4_flag;    //!< +0xE4 (0x2E8) - `fn_80009DBC`'s second argument (`stw r4,228(r31)`)
};
CHECK_SIZEOF(SGameStateMemcard, 0xe8)

class CGameState {
public:
  CGameState();
  CGameState(CInputStream& in, int saveIdx);

  void ReadSystemOptions(CInputStream& in);
  void PutTo(COutputStream& out) const;
  void WriteSystemOptions(COutputStream& out);

  void SetIsDarkWorld(bool);
  CGameMode& GetGameMode();

  CGameOptions& GameOptions() { return gameOptions; }
  // +0xC4. `fn_80180598` is called on it from `CMainFlow::SetGameState` (retail
  // `lwz r3,-28360(r13) ; addi r3,r3,196 ; bl 0x80180598`, 0x8001DDCC) and reads +0x10 of it.
  CHintOptions& HintOptions() { return hintOptions; }

  u64 GetCardSerial() const { return cardSerial; }
  float GetHardModeDamageMultiplier() const;
  bool GetHardModeEnabled() const;

  // **+0x1A0, the first word of the block `fn_80007040` constructs, and the value
  // `CMainFlow::AdvanceGameState` tests.** Retail: `lwz r4,-28360(r13) ; lwz r4,416(r4) ;
  // addis r0,r4,-21326 ; cmplwi r0,18252 ; bne` at 0x8001DEF4, which is the single comparison
  // `x1a0_unk == 0x949A` (mwcceppc materialises 0x949A as `addis`+`cmplwi` against the biased
  // value). Its *meaning* is not recovered: 0x949A also appears nowhere else in the DOL under
  // this reading, and the sibling test in `SetGameState` uses a different constant (0x92A6).
  // Named as unmodelled rather than guessed at.
  int GetX1A0() const { return x1a0.x00; }

  // Retail 0x80142520, 8 bytes, `addi r3,r3,60; blr` - so the whole body is the address of
  // +0x3C. Retail's `rstl::rc_ptr<CWorldState>` there is the pair its constructor writes at
  // 0x80144140: a `new(1200)`'d CWorldState at +0x3C and a separately allocated refcount word,
  // set to 1, at +0x40. Every caller then does one `lwz r3,0(r3)` to get through it before
  // calling a CWorldState method, so this hands out a *reference to the pointer* rather than
  // the pointer: that is what reproduces the pair exactly. Returning `rstl::rc_ptr&` instead
  // would be wrong on this port, whose `rc_ptr` is a single word pointing at a `CRefData` and
  // would need two dependent loads where retail needs one.
  //
  // Deliberately not inline. Retail's definition is in CGameState.cpp, a different translation
  // unit from its only caller, so `CGameArchitectureSupport::Update` (0x80007A14) calls it; an
  // inline accessor folds into `lwz r4,gpGameState; lwz r3,60(r4)` instead and drops that
  // function from 100% to 95.89%. There is no CGameState.cpp in the port, so the definition is
  // at the bottom of src/MetroidPrime/main.cpp under `#pragma inline_max_size(0)`.
  CWorldState*& GetWorldState();

  float GetUnk50() const { return x50_unk; }
  void SetUnk50(float value); // fn_801424EC

  // ---------------------------------------------------------------------------------------
  // The member map. **Every offset below is read out of `build/G2ME01/main.elf`; none is
  // recalled**, and every boundary is pinned by a `stw`/`addi`/`lwz` at a stated address in
  // `CGameState::CGameState(CInputStream&, int)` (retail `fn_80144140`, 0x80144140, 0x684) or in
  // one of the three other functions that touch the same members. The map itself is
  // `docs/research/cgamestate_layout.md`; what is here is the part of it a *C++ member* can
  // carry, and the notes say which measurement pins each row. All 42 of the offsets and sizes
  // are re-measured with mwcceppc's own flags by `tools/probe_gs_offsets.py`.
  //
  // The rule for turning a `lwz`/`stw` field into an address is
  // `field = (address - 0x8041FD80) & 0xFFFF` - the **full** signed displacement, not half of it.
  // That is how `lwz r29,-28356(r13)` is read: 0x8041FD80 - 28356 = 0x80418EBC, which is
  // `gpMemoryCard`, **not** a game state. `docs/research/cgamestate_layout.md` §2 item 3 says the
  // neighbouring `-28376` is `gpTweakGame` at 0x80418EF0; that is wrong by 0x48, and the retail
  // object's own relocation settles it - the object that contains `fn_80144140`
  // (`build/G2ME01/obj/auto_03_80142A30_text.o`, at .text+0x4b8) carries
  // `R_PPC_EMB_SDA21 gpSimplePool`, and so do its `lwz r3,-28356(r13)` sites at 0x801445D0 and
  // 0x801445E0. See the note on `x08_reserve` for what the loop at 0x8014470C is walking.
  //
  // **The members are public**, for the reason `MetroidPrime/Player/CHintOptions.hpp` gives:
  // retail's constructor is unnamed in the symbol table, so the unit that reproduces it is an
  // `extern "C" void fn_80144140(CGameState*, CInputStream&, int)` rather than a C++
  // constructor - a C++ one would mangle to `__ct__9CGameStateFR12CInputStreami` and objdiff
  // would have nothing to pair against. A free function cannot reach private members without a
  // friend declaration, and `friend` on an `extern "C"` function is not something mwcceppc 2.7
  // is known to accept, so the members are public. See
  // `src/MetroidPrime/Player/CGameStateStreamCtor.cpp`.
  // ---------------------------------------------------------------------------------------
public:
  int x00_unk;  //!< +0x00, -1. `stw r5,0(r3)` 0x80144160
  int x04_unk;  //!< +0x04, -1. `stw r5,4(r30)` 0x8014416C

  // +0x08 .. +0x17, **16 bytes**, and the first of four `SGameStateBlock`s. `fn_801466F4`
  // (0x801466F4), called on `this+0x08` with `*(gpMemoryCard+0x10)`, reserves
  // `n * 36`-byte elements and indexes the data as `data + count * 36`; `fn_8014260C`
  // (0x8014260C) walks it as `cursor = base->x0c_data` against `base->x04_count * 36`;
  // `fn_801426E0` (0x801426E0) appends one 36-byte element. So this is the same 16-byte shape as
  // `x178` and `x188`, with a 36-byte element instead of a byte buffer.
  SGameStateBlock x08_reserve;

  // +0x18, the count of the four-entry array at +0x1C. `stw r0,24(r30)` 0x80144184 zeroes it,
  // `stw r0,0(r30) ; slwi r0,r0,3 ; add r3,r28,r0 ; addic. r3,r3,4` (0x80144450-0x8014445C)
  // indexes with it, and `stw r0,24(r30)` after `addi r0,r4,1` (0x8014448C) advances it to 4.
  int x18_playerStates;

  // +0x1C .. +0x3B, four 8-byte `{CPlayerState*, int*}` pairs. Pinned by the stride-8 index
  // above and by `stw r0,0(r3) ; stw r0,4(r3)` (0x80144468/0x80144470) followed by the AddRef
  // through the **second** word (0x80144474-0x80144480) - so the second word is a separately
  // `new(4)`'d refcount set to 1, retail's 8-byte `rstl::rc_ptr`, and not a pointer to a
  // control block. `docs/research/rc_ptr.md` has the rest of that argument.
  CPlayerState* x01c_players[4][2];

  CWorldState* x3c_worldState; //!< +0x3C, retail: x0_ptr of an rc_ptr
  uint* x40_refCount;           //!< +0x40, retail: x4_refCount of the same rc_ptr, `new(4)` with `*refCount = 1`
  uint x44_unk;                 //!< +0x44, 4 bytes nothing in the DOL reads or writes

  // +0x48, a `double` loaded `lfd f1,-25112(r2)` (0x801441CC). The retail object names the
  // constant: `R_PPC_EMB_SDA21 lbl_8041C1A8`, `.sdata2:0x8041C1A8`, size 0x8, and its value is
  // **359999.0** - not a literal, because a literal left every function at 100% and grew
  // `main.dol` in an earlier session. `x48_time` is overwritten twice in the constructor from a
  // `lfd` off the stack (0x801443F8/0x801443FC), which is the two 32-bit halves of the two
  // `ReadBits(32)` results assembled at r1+40.
  double x48_time;

  // +0x50, a `float` loaded `lfs f0,-25096(r2)` (0x801441D0) and then overwritten by
  // `ReadFloat()` (0x80144540). The constant is `R_PPC_EMB_SDA21 lbl_8041C1B8`, `.sdata2:0x8041C1B8`,
  // size 0x4, value **100.0f**.
  float x50_unk;

  // +0x54 .. +0x7F - see `SGameStateCardOpts` above for the measurement.
  SGameStateCardOpts x54;

  CGameOptions gameOptions;             //!< +0x80, 0x44
  CHintOptions hintOptions;             //!< +0xC4, 0x18 - see that header for why 0x18
  CPersistentOptions persistentOptions; //!< +0xDC, 0x2C
  u64 cardSerial;                       //!< +0x108

  // +0x110 and +0x144, two 0x34-byte blocks, written by `fn_80144924` (now
  // `src/MetroidPrime/Player/CGameStateSlotsCtor.cpp`, Matching) with `n = 3` and a
  // default-constructed element each. `+0x110 + 0x34 == +0x144` and `+0x144 + 0x34 == +0x178`,
  // and `fn_80142DD4` (0x80142DD4) confirms the second: `slwi r0,r4,4 ; add r31,r30,r0 ;
  // addi r31,r31,328` is `this + 0x148 + i*16`.
  SGameStateSlots x110;
  SGameStateSlots x144;

  // +0x178 and +0x188, two more 16-byte `SGameStateBlock`s. `+0x178` is pinned by
  // `fn_80142CF8` (0x80142CF8): `addi r3,r31,376 ; bl fn_80142BA4 ; lwz r4,388(r31)` is
  // construct-at-0x178 and read-0x184, so the shape's `x0c_data` is at 0x184 and the block
  // starts at 0x178 - 16 bytes, same as `x08_reserve`. `+0x188` is the same shape one stride
  // later: the constructor zeroes 0x18C, 0x190 and 0x194 (0x80144288-0x80144294), and
  // `main.cpp:723` builds a local out of it.
  SGameStateBlock x178;
  SGameStateBlock x188;

  // +0x198 is `(ptr != nullptr)`: `neg r0,r4 ; or r0,r0,r4 ; srwi r0,r0,31 ; stb r0,408(r30)`
  // (0x801442A8-0x801442B8) is the idiom for `x19c_ptr != nullptr` as a bool. +0x19C is the
  // `new(12)`'d pointer itself: `li r3,12 ; bl __nw__FUlPCcPCc ; mr. r4,r3 ; beq ;
  // bl fn_80193E08` (0x80144278-0x801442A0) - the constructor is only called on a non-null
  // allocation, so it is `if (p) p->ctor();`.
  bool x198_ptrSet;
  void* x19c_ptr;

  // +0x1A0 .. +0x1F3 - see `SGameStateWorlds` above for the measurement.
  SGameStateWorlds x1a0;

  // +0x1F4 .. +0x203, the **fifth** `SGameStateBlock`, immediately after `x1a0`. Its three
  // non-`x00_unk` words are the `stw r0,504/508/512(r30)` at 0x801442CC/0x801442D4/0x801442D8
  // (0x1F8, 0x1FC, 0x200) and nothing in the DOL reads them; the +0x204 member starts on the
  // next word. That is what fixes the two together at 16 bytes each.
  SGameStateBlock x1f4;

  // +0x204 .. +0x2EB - see `SGameStateMemcard` above for the measurement.
  SGameStateMemcard x204;

  // +0x2EC, the flag byte: three `ReadBits(1)` results forced into bits 7, 6 and 5. **The three
  // `rlwimi r0,rX,7,24,24` / `,6,25,25` / `,5,26,26` (0x80144348, 0x8014436C, 0x80144390), each
  // followed by a whole-byte `stb r0,748(r30)`, are mwcceppc's expansion of three one-bit
  // *fields* of a `u8` struct - not of `|=` on a byte**, which is a `stb` of the computed value
  // and nothing else. The fourth write is a bit-1 clear (`rlwimi r0,r6,5,26,26` with `r6 = 0`,
  // 0x8014430C). So this is a bitfield struct, and it is the one member whose type the
  // constructor still needs: `x2ec_flags` is left as a `u8` here because
  // `src/MetroidPrime/Player/CGameStateStreamCtor.cpp` does not reach 0x80144300 yet, and
  // splitting it into named bits is a change with no measured effect until it does.
  u8 x2ec_flags;
  u8 x2ed_pad[3]; //!< 0x2ED..0x2EF
};

// **0x2F0, measured, and it agrees with `operator new(0x2F0)`.** `CGameGlobalObjects`'s
// constructor does `li r3,752` / `bl __nw__FUlPCcPCc` (0x800084C4) and the store
// `stw r4,-28360(r13)` at 0x80008548; 752 is 0x2F0. `tools/size_probe_gs.cpp` re-derives
// sizeof and every offset below with **mwcceppc's own flags** - the host compiler must not be
// used, it is 64-bit and `rstl::string` is 24 bytes there against retail's 0x10.
//
//   sizeof(CGameState) 0x2F0   +0x3C x3c_worldState   +0x40 x40_refCount
//   +0x50 x50_unk             +0x80 gameOptions        +0xC4 hintOptions
//   +0xDC persistentOptions   +0x108 cardSerial
//
// Each of those is independently confirmed by retail's own code, which is what makes this a
// measurement rather than a restatement of the header:
//
//   +0x3C  `stw r0,60(r30)`  0x801441A0  and  +0x40 `stw r3,64(r30)` 0x801441C4
//   +0x50  `stfs f0,80(r30)` 0x801441D8 and  `stfs f1,80(r30)` 0x80144544
//   +0x80  `addi r3,r30,128`  0x801441E0 -> `__ct__12CGameOptionsFv`, and again at
//         0x80142D40 / 0x80142E2C as `PutTo__12CGameOptionsFR16CBitStreamWriter(this+128)`
//   +0xC4  `addi r3,r30,196`  0x801441E8 -> `fn_80180738`, and 0x801444B0 -> `fn_801447C4`
//   +0xDC  `addi r3,r30,220`  0x801441F0 -> `fn_80146154`, and 0x80144558 -> `fn_8000401C`
//   +0x108 src/MetroidPrime/main.cpp:746, "new->x10C = the old x10C, new->x108 = the old x108"
CHECK_SIZEOF(CGameState, 0x2f0)

extern CGameState* gpGameState;

#endif // _CGAMESTATE
