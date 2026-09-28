/**
 * `CGameState::CGameState(CInputStream&, int)` - retail `fn_80144140`, .text:0x80144140,
 * `size:0x684` = 1,668 bytes, 417 instructions.
 *
 * This is the **only** writer of `gpGameState` on the boot path. `main.cpp:704`
 * (`CMain::StreamNewGameState`) is its one caller in the DOL, and boot-path step 17
 * (`docs/research/boot_path.md`) dereferences `gpGameState` at 0x800081A4 with no null test -
 * so this function is the single symbol the project measured as reached before a frame.
 *
 * The member map it needs is in `include/MetroidPrime/Player/CGameState.hpp`, and every one of
 * the 42 offsets and sizes in that header is measured with mwcceppc's own flags by
 * `tools/probe_gs_offsets.py` (not by the host compiler, which is 64-bit). This file is the
 * other half of that claim: the **shape**.
 *
 * ## Nothing about the callees blocks a `Matching` unit
 *
 * A `Matching` unit's object only needs *relocations*. `dtk dol split` writes a *filled*
 * `build/G2ME01/obj/<unit>.o` carrying retail's bytes for the functions a unit does not match,
 * and that filled object - not ours - is what the DOL link uses for a `NonMatching` unit. So an
 * unwritten callee is this function's problem only if it is claiming that callee's bytes. Every
 * call below is a relocation. The claim is measured, not assumed: the retail object that
 * contains this function is `build/G2ME01/obj/auto_03_80142A30_text.o`, and
 * `tools/dump_fn_relocs.sh 0x80144140` lists its relocations, which is where the named
 * constants in the notes below come from.
 *
 * ## The three things that are not logic
 *
 * 1. **`operator new`'s file operand is one merged `.rodata` object.** All five `new` sites pass
 *    `lis r3,0x803B ; addi r4,r3,-28152` = 0x803A9208, and the retail object's relocations name
 *    it: `R_PPC_ADDR16_HA/LO lbl_803A9208` at .text+0x26, +0x5e, +0x12e, +0x296 and +0x546.
 *    0x803A9208 is the seven bytes `"??(??)"` and the same pool object continues with
 *    `"InitialWorld"`, `"FrontEnd"`, `"Results"`, `"Coin"`, `"Deathmatch"`, `"%s%s%d"` and the
 *    two long `Cannot find World Asset(%x) ...` messages. The two `CBasics::Stringize` calls
 *    take **`lbl_803A9208 + 60`** (0x803A9244, the empty string - the NUL that terminates
 *    `"%s%s%d"`) and **`lbl_803A9208 + 134`** (0x803A928E, `"Save game did not contain World
 *    Asset(%x).  Created..."`), and retail materialises each as `lis` + the **-28152** `addi`
 *    (a relocation against `lbl_803A9208`) + a second plain `addi` of the constant.
 *    A string *literal* cannot reproduce that: the compiler would emit its own `.rodata`, and a
 *    `Matching` unit may not own one. So the pool object is named and the `new` is routed
 *    through a local throwing `operator new`, exactly as
 *    `src/Kyoto/CResLoaderAddPakFileAsync.cpp` does for `lbl_803AFAA0`. See
 *    `docs/research/rc_ptr.md`'s last section for the measurement.
 * 2. **The two floating constants are named retail objects, not literals.** `lfd f1,-25112(r2)`
 *    and `lfs f0,-25096(r2)` are `R_PPC_EMB_SDA21 lbl_8041C1A8` and `lbl_8041C1B8`
 *    (`.sdata2:0x8041C1A8`, size 0x8, value **359999.0**; `.sdata2:0x8041C1B8`, size 0x4, value
 *    **100.0f**). Writing the literals instead left every function at 100% and grew `main.dol`
 *    by 32 bytes in an earlier session, because the per-function diff cannot see a `.sdata2`
 *    claim change.
 * 3. **`gpSimplePool`'s vtable slot 3 is called through a global**, with a `CToken` built on the
 *    stack. `lwz r4,-28376(r13)` is 0x80418EA8 - `gpSimplePool`, *not* a game state and *not*
 *    the "gpTweakGame at 0x80418EF0" that `docs/research/cgamestate_layout.md` §2 item 3 says:
 *    0x8041FD80 - 28376 = 0x80418EA8. The retail object settles it independently, with
 *    `R_PPC_EMB_SDA21 gpSimplePool` at .text+0x4b8. `lwz r12,0(r4) ; lwz r12,12(r12) ; mtctr ;
 *    bctrl` is vtable slot 3.
 *
 * ## The full retail relocation list, recorded here because it is no longer recoverable
 *
 * **Once this unit claims 0x80144140, `fn_80144140`'s retail relocations are gone from
 * `build/G2ME01/obj/`.** `dtk dol split` cuts a claimed range out of the `auto_*` objects into a
 * *filled* `obj/<unit>.o` that carries **our** bytes and **our** relocations, and
 * `tools/dump_fn_relocs.sh` then reports the unit's own object. The tool says so when it sees one
 * (that check is the point of adding it), but the data is only available before the
 * `splits.txt` entry exists. It was captured from
 * `build/G2ME01/obj/auto_03_80142A30_text.o` (that object spans 0x80142A30..0x80144924 and holds
 * `fn_80144140` at .text+0x1710) and is reproduced here so the next lane does not have to
 * re-derive it. `.text+0xNN` is relative to 0x80144140:
 *
 *   +0x026/+0x02A  ADDR16_HA/LO  lbl_803A9208        +0x048  REL24 __nw__FUlPCcPCc
 *   +0x054         REL24         fn_8015C34C         +0x05E/+0x066 HA/LO lbl_803A9208
 *   +0x070         REL24         __nw__FUlPCcPCc      +0x08C  EMB_SDA21 lbl_8041C1A8
 *   +0x090         EMB_SDA21     lbl_8041C1B8        +0x09C  REL24 fn_80145950
 *   +0x0A4         REL24         __ct__12CGameOptionsFv   +0x0AC REL24 fn_80180738
 *   +0x0B8         REL24         fn_80146154         +0x0EC  REL24 fn_80144924
 *   +0x0F8         REL24         fn_80004A4C         +0x118  REL24 fn_80144924
 *   +0x124         REL24         fn_80004A4C         +0x12E/+0x136 HA/LO lbl_803A9208
 *   +0x154         REL24         __nw__FUlPCcPCc      +0x160  REL24 fn_80193E08
 *   +0x180         REL24         fn_80007040         +0x19C  REL24 fn_80009DBC
 *   +0x1D4 +0x1E0 +0x1EC +0x210 +0x234 +0x258  REL24 ReadBits__16CBitStreamReaderFUi
 *   +0x26C         REL24         SomethingWorldId_80005698
 *   +0x278 +0x288  REL24         ReadBits__16CBitStreamReaderFUi
 *   +0x296/+0x2A2  HA/LO         lbl_803A9208        +0x2CC  REL24 __nw__FUlPCcPCc
 *   +0x2E0         REL24         __ct__12CPlayerStateFiR16CBitStreamReader
 *   +0x2F8         REL24         __nw__FUlPCcPCc      +0x354  REL24 fn_8000934C
 *   +0x36C         REL24         fn_801805EC         +0x378  REL24 fn_801447C4
 *   +0x384         REL24         fn_800045A0         +0x390  REL24 fn_80144D70
 *   +0x39C         REL24         fn_80003BE8         +0x3F4  EMB_SDA21 gpMemoryCard
 *   +0x3FC         REL24         GetInputStream__16CBitStreamReaderFv
 *   +0x400         REL24         ReadFloat__12CInputStreamFv
 *   +0x414         REL24         fn_80146068         +0x420  REL24 fn_8000401C
 *   +0x42C         REL24         fn_80004678         +0x438  REL24 fn_801466F4
 *   +0x440 +0x460 +0x478  REL24 GetInputStream__16CBitStreamReaderFv
 *   +0x490         EMB_SDA21     gpMemoryCard        +0x494  REL24 fn_80176B48
 *   +0x4A0         EMB_SDA21     gpMemoryCard        +0x4A8  REL24 fn_80176A4C
 *   +0x4B8         EMB_SDA21     gpSimplePool        +0x4E8  REL24 __ct__6CTokenFRC6CToken
 *   +0x4F0         REL24         GetObj__6CTokenFv   +0x504  REL24 __dt__6CTokenFv
 *   +0x518         REL24         fn_80145068         +0x524  REL24 fn_801426E0
 *   +0x530         REL24         fn_8000447C         +0x53C  REL24 __dt__6CTokenFv
 *   +0x546/+0x54E  HA/LO         lbl_803A9208        +0x558  REL24 Stringize__7CBasicsFPCce
 *   +0x56C         REL24         __ct__Q24rstl66basic_string<...>FPCciRCQ24rstl17rmemory_allocator
 *   +0x588         REL24         ReadBits__16CBitStreamReaderFUi
 *   +0x59C         REL24         internal_dereference__Q24rstl66basic_string<...>Fv
 *   +0x5B4         REL24         GetInputStream__16CBitStreamReaderFv
 *   +0x5D8         REL24         fn_8014260C         +0x5EA/+0x5F2 HA/LO lbl_803A9208
 *   +0x5FC         REL24         Stringize__7CBasicsFPCce
 *   +0x610         REL24         __ct__Q24rstl66basic_string<...>FPCciRCQ24rstl17rmemory_allocator
 *   +0x618         REL24         internal_dereference__Q24rstl66basic_string<...>Fv
 *   +0x63C         REL24         fn_801437DC         +0x644  REL24 fn_8014306C
 *   +0x654         REL24         fn_80142DD4         +0x668  REL24 fn_80142CF8
 *
 * Three of those are things this file already owns or a `Matching` unit already defines -
 * `fn_80144924` (`CGameStateSlotsCtor.cpp`), `fn_80180738` (`CHintOptionsCtor.cpp`) - so they
 * are links, not gaps. Everything else is retail's own bytes.
 *
 * ## What is *not* here, and why it is not a blocker
 *
 * ~~`fn_80146154` (0x80146154, 0x58) is called as `fn_80146154(this+0xDC, 1)` - `r3` and `r4` only -
 * and it does `lbz r5,8(r1)` and `lbz r4,12(r1)`, which are **its own** outgoing parameter save
 * area, never written by the caller, and stores both into the object at +4 and +5. No C++ can
 * express "pass two garbage bytes on the stack", so that one callee cannot be written as source.
 * It is a *callee* problem: the caller only needs the relocation, and the bytes stay retail's.~~
 *
 * **Superseded (2026-09-26): `fn_80146154` is now a `Matching` unit at 100%**
 * (`src/MetroidPrime/Player/CPersistentOptionsCtor.cpp`). The mechanism above was wrong: with
 * `stwu r1,-32(r1)`, LR saved at 36(r1) and r31 at 28(r1), 8(r1) and 12(r1) are this frame's own
 * local area rather than the caller's parameter save area, so the two bytes are uninitialised
 * *locals* of the callee, and one 8-byte `volatile` local read as two bytes four apart reproduces
 * all 22 instructions. The other block, the uninitialised length at 0x801444E0, is a different
 * shape - a runtime byte count, not a byte of a local - and is still not expressible.
 *
 * ## The two loops, which is where the percentages stop moving
 *
 * 0x801443DC-0x801444A0 builds four `CPlayerState`s into `x01c_players` (stride 8, `slwi
 * r0,r0,3 ; add r3,r28,r0 ; addic. r3,r3,4`) and 0x8014470C-0x80144774 walks
 * `gpMemoryCard`'s 112-byte-element array with the loop-invariant `r29 = gpMemoryCard` and the
 * same `addic.` overflow probe. Those are register-allocation questions, not logic ones.
 */

// `#define _CMEMORY` keeps `Kyoto/Alloc/CMemory.hpp`'s own throwing `operator new` out, so the
// one below is the only `new` in this translation unit - see the long comment on it.
#define _CMEMORY

#include "types.h"

#include "rstl/string.hpp"

#include "MetroidPrime/Player/CGameState.hpp"

#include "Kyoto/CToken.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/IObj.hpp"
#include "Kyoto/Streams/CInputStream.hpp"
#include "Kyoto/Streams/CBitStreamReader.hpp"

#include "MetroidPrime/CMemoryCard.hpp"
#include "MetroidPrime/Player/CGameOptions.hpp"
#include "MetroidPrime/Player/CHintOptions.hpp"
#include "MetroidPrime/Player/CPersistentOptions.hpp"
#include "MetroidPrime/CWorldTransManagerView.hpp"

extern "C" void __ct__12CGameOptionsFv(CGameOptions* self);

// ---------------------------------------------------------------------------------------------
// The merged `.rodata` pool object. See the file comment, item 1.
extern "C" const char lbl_803A9208[];

// The two named `.sdata2` constants. See the file comment, item 2.
extern "C" const double lbl_8041C1A8;
extern "C" const float lbl_8041C1B8;

// The throwing `operator new`, so that all five `new` sites in this file pass retail's
// `lbl_803A9208` as the file argument instead of a literal of their own. This is
// `Kyoto/Alloc/CMemory.hpp:28-37` with one word changed, and the header is not included here so
// its version does not collide. The `_CMEMORY` define above is what keeps it out, and it has to
// come before *any* include, because `rstl/rmemory_allocator.hpp` and `rstl/string.hpp` both
// pull `Kyoto/Alloc/CMemory.hpp` in. With the header's `"??(?)?"` literal instead, every byte of
// the object matches except the `addi` - the whole of the `main.dol` sha1 difference - and the
// object would also carry a 7-byte `.rodata` a `Matching` unit may not own.
#if defined(__MWERKS__) || defined(CLANGD)
void* operator new(size_t sz, const char*, const char*);
void* operator new[](size_t sz, const char*, const char*);
inline void* operator new(size_t sz) { return operator new(sz, lbl_803A9208, nullptr); }
inline void* operator new[](size_t sz) { return operator new[](sz, lbl_803A9208, nullptr); }
inline void* operator new(size_t n, void* ptr) { return ptr; }
#endif

// ---------------------------------------------------------------------------------------------
// The 42 named callees. Retail's symbol table has no name for any of them, so each is called by
// its dtk label; the relocations in `build/G2ME01/obj/auto_03_80142A30_text.o` are what confirms
// the list (that is the whole value of `tools/dump_fn_relocs.sh`). Declarations only - no body
// here - so every one of them is an ordinary undefined reference the DOL link resolves to
// retail's own bytes.
//
// `__nw__FUlPCcPCc` is the throwing `operator new` declared above, and
// `Stringize__7CBasicsFPCce` is `CBasics::Stringize(const char*, ...)`, so its varargs are
// written as a call through a pointer type rather than spelled as varargs: retail's own
// `crclr 4*cr1+eq` before each call is the condition register being cleared for the FP compare
// the callee does, which is what a call with a `double` argument would do, and a plain C++ call
// with no FP argument leaves it alone.
extern "C" {
// `fn_8015C34C` - CWorldTransManagerView's default constructor, 0x8015C34C, 0x114 = 276 bytes.
void fn_8015C34C(CWorldTransManagerView* self);
// `fn_80180738` - CHintOptions' default constructor; a `Matching` unit already defines it
// (src/MetroidPrime/Player/CHintOptionsCtor.cpp), so this is a link, not a gap.
void fn_80180738(CHintOptions* self);
// The +0x54 block's constructor. Calls `fn_80146154(this, 0)` and zeroes its own +0x1C..+0x28.
void fn_80145950(SGameStateCardOpts* self);
// CPersistentOptions' constructor. **Cannot be written as source**: it reads two uninitialised
// words of its own outgoing parameter save area (`lbz r5,8(r1) ; lbz r4,12(r1)`) and stores them
// at +4 and +5. It is called, never defined, here.
void fn_80146154(CPersistentOptions* self, int flag);
// The two 0x34 blocks' `(count, element)` fill constructor - src/MetroidPrime/Player/
// CGameStateSlotsCtor.cpp, `Matching`, so this is a link to our own object.
void fn_80144924(SGameStateSlots* self, int n, const SGameStateBlock* src);
// The `{?, ..., void* heap}` destructor: `if (this) { Free(x0c); if ((short)flag > 0) Free(this); }`
void fn_80004A4C(void* self, int flag);
// The 16-byte block element's copy constructor - src/MetroidPrime/Player/CGameStateBlockCopy.cpp.
void fn_80142A10(SGameStateBlock* self, const SGameStateBlock* src);
// `new(12)`'s constructor.
void fn_80193E08(void* self);
// The +0x1A0 block's constructor.
void fn_80007040(SGameStateWorlds* self);
// The +0x204 block's constructor: writes 76 at +0x00, fills +0x04..+0x4F from a 4-byte global 19
// times four bytes at a time, then the same at +0x50 and +0x54..+0x9F.
void fn_80009DBC(SGameStateMemcard* self, int flag);
// CPlayerState's stream constructor.
void __ct__12CPlayerStateFiR16CBitStreamReader(CPlayerState* self, int index,
                                                CBitStreamReader& reader);
// `rc_ptr<CPlayerState>::ReleaseData` - one of the ~30 per-`T` release functions
// `docs/research/rc_ptr.md` §7 lists. It takes the rc_ptr itself (r3), not a pointer to it.
void fn_8000934C(void* self);
// The hint options' stream constructor and the +0xC4 block's copy assignment.
void fn_801805EC(CHintOptions* self, CBitStreamReader& reader);
void fn_801447C4(CHintOptions* self, const CHintOptions& rhs);
// Three destructors of 0x14-byte stack temporaries: `~T(t, -1)`.
void fn_800045A0(void* self, int flag);
void fn_8000447C(void* self, int flag);
void fn_80004678(void* self, int flag);
// The 36-byte element's stream constructor and the block's copy assignment.
void fn_80144D70(void* self, CInputStream& in);
void fn_80003BE8(SGameStateWorlds* self, const void* rhs);
// Read a length-prefixed name: `fn_80145068(&local, reader, id, obj)`.
void fn_80145068(void* self, CBitStreamReader& reader, int id, void* obj);
// Append one 36-byte element to the +0x08 block.
void fn_801426E0(SGameStateBlock* self, const void* elem);
// Look one 36-byte element up.
void fn_8014260C(CGameState* self, int id);
// Reserve `n` 36-byte elements in the +0x08 block.
void fn_801466F4(SGameStateBlock* self, int n);
// Set up a CBitStreamReader over a CInputStream.
CInputStream& GetInputStream__16CBitStreamReaderFv(CBitStreamReader* self);
float ReadFloat__12CInputStreamFv(CInputStream* self);
uint ReadBits__16CBitStreamReaderFUi(CBitStreamReader* self, uint n);
// The persistent options' stream constructor, and the block it fills's copy assignment.
void fn_80146068(void* self, int flag, CBitStreamReader& reader);
void fn_8000401C(CPersistentOptions* self, const void* rhs);
// Two gpMemoryCard methods, both with the object id in r4.
int fn_80176B48(CMemoryCard* self, int id);
void* fn_80176A4C(CMemoryCard* self, int id);
} // extern "C"

// `SomethingWorldId_80005698` and `Stringize__7CBasicsFPCce` - a world id, and
// `CBasics::Stringize(const char*, ...)`.
extern "C" int SomethingWorldId_80005698(int id);
extern "C" const char* Stringize__7CBasicsFPCce(const char* fmt, ...);

// Declared, not defined, and named in `config/G2ME01/symbols.txt` under its mwcceppc name. The
// spellings are the ones `nm` reports, and a wrong one is a link error rather than a silent
// mismatch - which is what `tools/check_symbol_names.py` says about the DOL units.
extern "C" {
void __dt__6CTokenFv(CToken* self, int flag = 0);
void __ct__6CTokenFRC6CToken(CToken* self, const CToken& rhs);
void* GetObj__6CTokenFv(CToken* self);
}

// `CSimplePool`'s third virtual. **The identification is the one thing in this file that is not
// a measurement**, and it is written down rather than hidden: what is measured is that retail
// loads the vptr out of `gpSimplePool` and calls `*(vtbl+3)`. `CSimplePool` declares nine
// virtuals and retail's own vtable is an unnamed object with no symbol for it anywhere in
// `main.elf`, so which of the nine sits at slot 3 is not derivable from the tree. Naming it
// `CToken* GetObj(const void* tag, int n)` is a **placeholder** that reproduces the byte
// pattern - hidden return pointer in r3, `this` in r4, two arguments in r5 and r6 - and the
// declaration is commented as provisional. `docs/research/cgamestate_layout.md` should carry
// this as an open question, not as a solved one.
class CGameStateStreamPool {
public:
  virtual CToken* GetObj(const void* tag, int n);
};

// The +0x19C member: a 12-byte object whose constructor is `fn_80193E08`, and which is
// allocated with a null test (`li r3,12 ; bl new ; mr. r4,r3 ; beq ; bl fn_80193E08`), so the
// constructor is only reached on a non-null allocation. `CGameState::x198_ptrSet` is the
// `(p != nullptr)` of that.
class SGameStateMarker {
public:
  u32 x0, x4, x8;
};
CHECK_SIZEOF(SGameStateMarker, 0xc)

extern "C" CMemoryCard* gpMemoryCard;

// The block element's default constructor, inlined at 0x80144220/0x80144250: it zeroes `x04`,
// `x08` and `x0c` and **leaves `x00` alone**, which is the same six-store pattern as
// `CHintOptions` (`src/MetroidPrime/Player/CHintOptionsCtor.cpp`). It is spelled here as three
// assignments rather than as a constructor because retail's is unnamed and a C++ constructor
// would mangle to something objdiff has no retail symbol to pair with.
static inline void block_default_ctor(SGameStateBlock* self) {
  self->x04_count = 0;
  self->x08_cap = 0;
  self->x0c_data = nullptr;
}

// ---------------------------------------------------------------------------------------------
// The body. Declared `extern "C"` and named for its address because **retail's symbol table has
// no name for it** (`config/G2ME01/symbols.txt:5397` calls it `fn_80144140`) - a C++
// `CGameState::CGameState(CInputStream&, int)` would mangle to
// `__ct__9CGameStateFR12CInputStreami` and objdiff would have nothing to pair it against. The
// same reason as every other unnamed function in this tree.
//
// **How far this goes, and why.** Straight-line code from the prologue through the +0x204
// member's constructor is written. The four `CPlayerState`s loop (0x801443DC-0x801444A0) and
// the 112-byte-element loop (0x8014470C-0x80144774) are **not** written: they are
// register-allocation problems, and a loop written the obvious way lands in the wrong registers
// in a way that is invisible until the whole function is compared. The block at
// 0x801444E0-0x80144530 is also not written, and it is **not expressible**: it is a
// `memset`-shaped fill whose length is read out of an uninitialised stack word
// (`addic. r3,r1,224 ; lwz r5,0(r3)`) that nothing in the function ever stores to. It used to be
// described here as "the same class of thing as `fn_80146154`" - it is not: `fn_80146154` read two
// *bytes of a local* and is now a Matching unit, while this reads a *word used as a length*, and
// no C++ says "clear a runtime number of bytes from a length read out of nowhere".
//
// The CToken round trip and the `gpSimplePool` slot-3 dispatch are not written either; see the
// note on `CGameStateStreamPool` above. So this unit is **`NonMatching` and claims its range
// only so objdiff measures it**, which is safe: a `NonMatching` object is not in the DOL link.
// Measured: **24.33%** of `fn_80144140`, and the `All:` line and the DOL's sha1 are unchanged by
// it.
//
// `CGameState::x2ec_flags` is **not** a plain `u8` with three `|=` in the source either: the
// three `rlwimi r0,rX,7,24,24` / `,6,25,25` / `,5,26,26` (0x80144348, 0x8014436C, 0x80144390),
// each followed by a whole-byte `stb`, are what mwcceppc emits for **three one-bit fields of a
// `u8` struct**, not for `|=` on a byte. So the flag is
// `{ u8 b7:1; u8 b6:1; u8 b5:1; u8 rest:5; }`, and naming it that is the one member whose type
// the constructor still needs and this header does not yet have.
extern "C" void fn_80144140(CGameState* self, CInputStream& in, int saveIdx) {
  (void)in;
  (void)saveIdx;

  // 0x80144160-0x80144184. Four zeroed words and two -1s, then `x08_reserve`'s three
  // non-`x00_unk` words: the same three the block's copy constructor writes, which is why they
  // are zeroed here rather than left to a constructor call.
  self->x00_unk = -1;
  self->x04_unk = -1;
  self->x08_reserve.x04_count = 0;
  self->x08_reserve.x08_cap = 0;
  self->x08_reserve.x0c_data = nullptr;
  self->x18_playerStates = 0;

  // 0x80144170-0x801441A0. `new CWorldTransManagerView()` expanded the way mwcceppc expands it: the
  // allocation, a null test, and then the class's **out-of-line** constructor as a call. The
  // pointer is live across the test (`mr. r0,r3 ; beq`), so it is written as one statement and
  // stored afterwards, not as an initialiser.
  CWorldTransManagerView* worldState = static_cast< CWorldTransManagerView* >(::operator new(sizeof(CWorldTransManagerView)));
  if (worldState) {
    fn_8015C34C(worldState);
  }
  self->x3c_worldState = worldState;

  // 0x801441AC-0x801441C4. The rc_ptr's **second** word: a separate four-byte allocation with
  // `*refCount = 1`, which is what makes retail's `rstl::rc_ptr` eight bytes
  // (`docs/research/rc_ptr.md`).
  uint* refCount = static_cast< uint* >(::operator new(4));
  if (refCount) {
    *refCount = 1;
  }
  self->x40_refCount = refCount;

  // 0x801441C8-0x801441DC. `r3 = this + 0x54` is set up *first*, then the two FP constants are
  // loaded and stored, then the call - so the source order is the stores before the call and
  // the compiler hoisted the argument. The two constants are retail's named `.sdata2` objects,
  // **not literals**: see the file comment, item 2.
  self->mTotalPlayTime = lbl_8041C1A8;
  self->mEscapeTime = lbl_8041C1B8;
  fn_80145950(reinterpret_cast< SGameStateCardOpts* >(&self->mSystemOptions));

  // 0x801441E0-0x801441F8. Three member constructors, each a bare call on `this + offset`.
  __ct__12CGameOptionsFv(&self->gameOptions);
  fn_80180738(&self->hintOptions);
  fn_80146154(&self->persistentOptions, 1);

  // 0x801441FC-0x80144238. Six zeroed words - three of them the last words of
  // `persistentOptions` and two of them `cardSerial` - and then the first of the two 0x34
  // blocks, built by filling three elements with a default-constructed temporary that is
  // destroyed immediately afterwards. The temporary's `x00_unk` is **not** stored, which is why
  // `block_default_ctor` writes three words and not four.
  // `CPersistentOptions` friends only `fn_801449C8`, so the three words go through the overlay.
  reinterpret_cast< SGameStateCardOpts* >(&self->persistentOptions)->x1c = 0;
  reinterpret_cast< SGameStateCardOpts* >(&self->persistentOptions)->x20 = 0;
  reinterpret_cast< SGameStateCardOpts* >(&self->persistentOptions)->x24 = 0;
  // One `u64` zero, upstream's `cardSerialA`/`cardSerialB` pair; see `CGameStateCtor.cpp`.
  *reinterpret_cast< u64* >(&self->cardSerialA) = 0;
  SGameStateBlock block110;
  block_default_ctor(&block110);
  fn_80144924(&self->x110, 3, &block110);
  fn_80004A4C(&block110, -1);

  // 0x8014423C-0x80144264. The same, for the second block at +0x144.
  SGameStateBlock block144;
  block_default_ctor(&block144);
  fn_80144924(&self->x144, 3, &block144);
  fn_80004A4C(&block144, -1);

  // 0x80144268-0x801442BC. `x178` and `x188` get their three words zeroed, then a 12-byte
  // object is allocated and constructed, and the result is recorded **twice**: as a bool
  // `(p != nullptr)` at +0x198 and as the pointer at +0x19C. The bool is
  // `neg r0,r4 ; or r0,r0,r4 ; srwi r0,r0,31`, which is how mwcceppc spells `!= nullptr` here.
  self->x178.x04_count = 0;
  self->x178.x08_cap = 0;
  self->x178.x0c_data = nullptr;
  self->x188.x04_count = 0;
  self->x188.x08_cap = 0;
  self->x188.x0c_data = nullptr;
  SGameStateMarker* marker = static_cast< SGameStateMarker* >(::operator new(sizeof(SGameStateMarker)));
  if (marker) {
    fn_80193E08(marker);
  }
  // +0x198/+0x19C are upstream's `rstl::auto_ptr< CGameMode > mGameMode` (`mHas`, `mItem`).
  reinterpret_cast< bool* >(&self->mGameMode)[0] = marker != nullptr;
  reinterpret_cast< void** >(&self->mGameMode)[1] = marker;

  // 0x801442C0. The +0x1A0 block's constructor.
  fn_80007040(reinterpret_cast< SGameStateWorlds* >(&self->mGameModeType));

  // 0x801442C4-0x801442DC. `x1f4`'s three words, then the +0x204 member's constructor with
  // `r4 = 0`.
  self->x1f4.x04_count = 0;
  self->x1f4.x08_cap = 0;
  self->x1f4.x0c_data = nullptr;
  fn_80009DBC(reinterpret_cast< SGameStateMemcard* >(&self->mControlMapper), 0);
}

