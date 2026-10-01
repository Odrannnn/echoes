// One unit, three functions, and both of `CMainFlow`'s `.data` objects:
//
//   .text  0x8001DAF4-0x8001DF48  (1108 bytes)  ~CMainFlow, SetGameState, AdvanceGameState
//   .data  0x803B1770-0x803B17D0  (  96 bytes)  vtable for CMainFlow, jumptable_803B178C
//
// ## Why the destructor and the state machine are in one unit: `.data` is 8-byte aligned
//
// `AdvanceGameState` has to be a `Matching` unit, and a `Matching` unit's object is what the
// linker places, so it has to carry its own switch jumptable. But:
//
//  * **mwcceppc 2.7 puts `.data` in an 8-byte-aligned section** no matter what is in it -
//    `-align powerpc`, `-align 4` and `-align off` were all tried and all give `2**3`, measured -
//    so a section cannot start at 0x803B178C, which is 4-byte aligned. Claiming
//    `.data 0x803B178C..0x803B17D0` makes mwldeppc insert four bytes of padding, every address
//    above it moves, and the DOL sha1 fails with all 86 RELs. dtk warns first:
//    "Alignment for MetroidPrime/CMainFlowAdvanceGameState.cpp .data expected 8, but starts at
//    0x803B178C". `align:4` on the split line silences the warning and does **not** change the
//    padding - measured, not assumed.
//  * **the jumptable is emitted as a local `@N` symbol, not as `jumptable_803B178C`**, so
//    `dtk dol diff` cannot find the symbol `symbols.txt` names even when the bytes are right.
//  * **one unit cannot claim two discontiguous ranges** - `splits.txt` takes it and
//    `dtk dol split` then fails with a cyclic-dependency error - which rules out keeping
//    `SetGameState` (0x8001DB54..0x8001DE68, between the other two) in a unit of its own.
//
// So the only 8-aligned `.data` range that can hold the jumptable is the one that starts with
// the vtable, and the vtable is emitted by the unit that defines the class's key function, which
// is `~CMainFlow`. All three functions therefore had to land together. It also happens to be
// the arrangement that works: mwcceppc emits the vtable at `.data+0` and the jumptable at
// `.data+0x1C`, which is exactly retail's layout, and the `.text` comes out in the right order
// because the definitions below descend by retail offset.
//
// # `CMainFlow::~CMainFlow` - retail 0x8001DAF4, 96 bytes
//
//   8001daf4  94 21 ff f0   stwu   r1,-16(r1)
//   8001daf8  7c 08 02 a6   mflr   r0
//   8001df00 ...                      epilogue shape identical to CIOWin's
//   8001db14  3c a0 80 3b   lis    r5,0x803b       ; &vtable for CMainFlow
//   8001db18  38 80 00 00   li     r4,0             ; the base is destroyed, not deleted
//   8001db1c  38 05 17 70   addi   r0,r5,0x1770
//   8001db20  90 1e 00 00   stw    r0,0(r30)
//   8001db24  48 02 c3 0d   bl     0x80049e30      ; CIOWin::~CIOWin
//   8001db28  7f e0 07 35   extsh. r0,r31          ; the deleting flag
//   8001db2c  40 81 00 0c   ble    +0x44
//   8001db30  7f c3 f3 78   mr     r3,r30
//   8001db34  4b 2b 08 55   bl     0x802ce388      ; CMemory::Free
//
// `mGameState` is a plain enum, so the body is `{}`: vptr, base destructor, and the deleting
// tail. Nothing in the source names the vtable - mwcceppc derives it, and the seven words it emits
// are retail's. The vtable's `OnMessage` slot relocates against
// `OnMessage__9CMainFlowFRC20CArchitectureMessageR18CArchitectureQueue`, and **that symbol is
// still retail's own bytes**: retail's `OnMessage` is unnamed in the DOL (`fn_8001DF54`) and no
// unit here reproduces it, so `symbols.txt` is renamed to give dtk's fill object the name the
// vtable needs. Without that rename this unit does not link - which is the honest state of the
// port: the vtable is defined and `OnMessage` is still a hole, and the port link says so by name
// instead of failing on a vtable that names nothing. See docs/research/port_link_stubs.md: this
// is not a stub, it is retail's own code.
//
// # The jumptable, read out of the DOL rather than inferred
//
// `AdvanceGameState`'s dispatch is `lwz r4,20(r3) ; addi r0,r4,1 ; cmplwi r0,16 ; bgt default ;
// slwi r0,r0,2 ; lwzx r0,r4,r0 ; mtctr r0 ; bctr`, so the table is indexed by
// `mGameState + 1` and covers states -1..15. The seventeen words were read with
// `objdump -s -j .data --start-address=0x803B178C` (and out of dtk's fill object
// `auto_07_803B178C_data.o` before this range was claimed):
//
//   [0]      0x8001DF20   case kCFS_Unspecified (-1)
//   [1]..[7] 0x8001DF30   default  -> states 0..6  (None, WinBad, WinGood, WinBest,
//                                          LoseGame, Default, StateSetter)
//   [8]      0x8001DEBC   case kCFS_PreFrontEnd (7)
//   [9]      0x8001DECC   case kCFS_FrontEnd (8)
//   [10..14] 0x8001DF30   default  -> states 9..13 (unnamed in the header)
//   [15]     0x8001DEAC   case kCFS_Game (14)
//   [16]     0x8001DEDC   case kCFS_GameExit (15)
//
// **Only five of the seventeen entries are cases and the other twelve are the default**, which is
// what pins the source: a `switch` whose case labels are exactly {kCFS_Unspecified,
// kCFS_PreFrontEnd, kCFS_FrontEnd, kCFS_Game, kCFS_GameExit} makes a compiler build a table
// spanning min..max of the *labels*, -1..15, i.e. 17 entries with the gaps defaulting. Reading
// the arms instead gives the opposite conclusion, because four of the five bodies are the same
// two instructions with a different constant.
//
// **The body order in the source is retail's, and it is not the enum's.** mwcceppc emits a
// switch's cases in *source* order (measured: reordering the arms permutes the bytes and nothing
// else notices), and retail's offsets run 0x8001DEAC (kCFS_Game), 0x8001DEBC (kCFS_PreFrontEnd),
// 0x8001DECC (kCFS_FrontEnd), 0x8001DEDC (kCFS_GameExit), 0x8001DF20 (kCFS_Unspecified). So the
// arms are written in that order, and `kCFS_GameExit` **falls through** into `kCFS_Unspecified` -
// that is what makes retail emit one shared copy of `SetGameState(kCFS_PreFrontEnd, queue)` at
// 0x8001DF20 with the game-exit case branching into it (`beq 8001df20` twice, `b 8001df20` once)
// instead of emitting it twice. With an explicit `break` there is no merge and the function is
// 4 bytes longer.
//
// Three shapes in `AdvanceGameState` worth naming:
//
// 1. `gpMain->mRestartMode` is `+0x58` and is an `ERestartMode`, compared against 0 and 7 here.
//    `CMain.hpp`'s `GetRestartMode()` is the accessor; the layout is measured with mwcceppc
//    (restartMode 0x58, mFrameTimeIdx 0x8C, sizeof(CMain) 0x94).
// 2. **`gpMain->SetGameExitReset(true)` reproduces four instructions of which three are a no-op.**
//    Retail: `lbz r0,144(r3) ; li r4,1 ; rlwimi r0,r4,1,30,30 ; stb r0,144(r3)`. The mask is word
//    bit 30, which lives in byte **0x93**, and the store is to byte **0x90**. mwcceppc allocates
//    that `bool : 1` at word bit 30 and still addresses the containing byte as 0x90 - and does so
//    for all nine of CMain's bitfields (measured one at a time). `= true` and not `= false`,
//    because `= false` makes MWCC fold the constant away and emit `li r4,0`. Written out, the
//    assignment cannot change byte 0x90; it is reproduced rather than "fixed" because the bytes
//    are the specification.
// 3. **`gpGameState->GetGameModeType() == 0x534E474C` is the constant retail's bytes imply, and
//    0x949A is the constant a reader would expect.** mwcceppc canonicalises a 32-bit
//    equality compare whose constant does not fit a `cmplwi` as `addis rD,rS,-(K>>16)` +
//    `cmplwi rD,K&0xFFFF` - measured
//    over nine spellings: `K = 0x949A` gives `addis rD,rS,0` + `cmplwi rD,0x949A`, and retail's
//    `addis r0,r4,-21326` + `cmplwi r0,18252` pins `K = 0x534E474C`. The *meaning* is not
//    recovered; `CGameState.hpp` says so at the accessor. The same shape appears in
//    `SetGameState` with `K = 0x46524E44`.
//
// # `CMainFlow::SetGameState` - retail 0x8001DB54, 788 bytes
//
// `mGameState = state;` and then a `switch` on **the member, read back** (`stw r4,20(r3)` then
// `lwz r0,20(r3)`), which is why the store is not folded away. The four cases are
// kCFS_GameExit(15), kCFS_PreFrontEnd(7), kCFS_FrontEnd(8), kCFS_Game(14) **in that source
// order** - mwcceppc emits a switch's bodies in source order and retail's offsets are
// 0x8001DB9C, 0x8001DCD4, 0x8001DD40, 0x8001DDE4. The dispatch itself
// (`cmpwi 14 / beq / bge / cmpwi 8 / beq / bge / cmpwi 7 / bge / b / cmpwi 16 / bge`) is a binary
// search over the label set and comes out of any spelling of those four cases.
//
// Four things about it are not obvious and each cost the function to find:
//
// 1. **The message is pushed as a temporary, not through a named local.**
//    `queue.Push(msg)` where `msg` is a local gives a *copy-initialised* local plus an AddRef
//    plus two `ReleaseData` calls - 40 extra bytes (see the note in
//    `CInputGeneratorUpdate.cpp` about mwcceppc not eliding the copy out of a return value).
//    `queue.Push(fn_80048EA4(...))` gives retail's fourteen instructions exactly: build into the
//    sret slot at `r1+52`, `Push` it, then the temporary's destructor - which is the dead
//    `addic. r3,r1,60 ; beq` (the address of the message's `rc_ptr`, never zero) and one
//    `bl ReleaseData__Q24rstl34rc_ptr<24IArchitectureMessageParm>Fv`.
// 2. **The window pointer is passed by address.** `fn_80048EA4` reads its 2nd, 3rd and 4th
//    operands (`lwz r4,0(r29) ; lwz r5,0(r30) ; lwz r6,0(r31)`), so all three are pointers to
//    values. Retail materialises the 4th as `addi r7,r1,16` - the address of the local - which is
//    also what keeps that local in memory instead of in a register, and getting it wrong costs
//    16 bytes of frame and the `mr. r0,r3` in four places.
// 3. **`new` is spelled by hand, with a register temporary, because the spelling is the bytes.**
//    `p = __nw__FUlPCcPCc(212, lbl_803A60A0, nullptr); if (p) p = fn_80022C74(p, 6); win = p;`
//    gives `bl new ; mr. r0,r3 ; beq L ; li r4,6 ; bl ctor ; mr r0,r3 ; L: stw r0,16(r1)`, which
//    is retail. Assigning straight into the address-taken `win` instead stores twice per site
//    (`cmplwi r3,0 ; stw r3,16(r1)`) and is 8 instructions longer. `fn_80022C74`, `fn_80020478`,
//    `fn_800214A0`, `fn_80192808`, `fn_80193E08` and `fn_801F47F4` are retail's *constructors* -
//    they all call `__ct__6CIOWinF...` and are 0x194/0x118/0x6F4/0x5C/0x2C/0x2D4 bytes - but
//    retail's symbol table gives none of them a C++ name, so they are called through `extern "C"`
//    and the size is written as a literal. A real `new CIOWinSubclass(n)` cannot be written
//    because the constructor's mangled name is not retail's.
// 4. **`StreamNewGameState` is called through an `extern "C"` declaration with retail's mangled
//    name spelled out and its third parameter dropped.** Retail writes `li r4,0` (a **null**
//    `CInputStream&`) and never writes r5 at all - the second argument is whatever the virtual
//    call above it left behind. No C++ source can express "pass an uninitialised int", and every
//    spelling that does express it (`int saveIdx;` uninitialised) makes mwcceppc allocate a
//    callee-saved register for it, which costs `stw r30,72(r1)` in the prologue, `mr r5,r30` at
//    the call and `lwz r30,72(r1)` in the epilogue - three instructions that move every branch
//    displacement in the function. `extern "C"` suppresses mangling, so the declaration's
//    parameter list does not change the symbol the call refers to, and r5 is left alone.
//
// The two unnamed constants: `0x46524E44` is compared against `CGameMode`'s sixteenth virtual
// (`lwz r12,0(r3) ; lwz r12,68(r12)`, i.e. vtable word 17, which is `CGameMode::GetGameModeType()`
// - the only one of the twenty-three that returns `int`), and it is the same `addis`+`cmplwi` bias
// as `0x534E474C` above and for the same reason. `fn_80143884` ignores its argument (its first act
// is `lwz r3,-28360(r13)`), so r3 still holds the compared value and the `bl` is retail's either
// way; upstream's `CMainFlow.cpp` therefore calls it with no argument and so does this file.
//
// # No `.rodata`, no `.sdata`
//
// Every `operator new` in this unit takes retail's `lbl_803A60A0` as its `file` operand - the
// seven bytes `"??(??)\0"` that `CMainFlowCtor.cpp` also uses - rather than a string literal of
// this file's own, and the three `.sdata` pairs are referenced by symbol
// (`lbl_80417DE0`..`lbl_80417DF4`) rather than declared here. So the only data this object
// contributes is the 0x60 bytes above.
#include "MetroidPrime/CMainFlow.hpp"

#include "MetroidPrime/CArchitectureMessage.hpp"
#include "MetroidPrime/CArchitectureQueue.hpp"
#include "MetroidPrime/CMain.hpp"
#include "MetroidPrime/Player/CGameMode.hpp"
#include "MetroidPrime/Player/CGameState.hpp"

// Retail's own constructors, its own message factory, its own message queue and its own
// `CMain::StreamNewGameState`. None of these carries a C++ name in
// `config/G2ME01/symbols.txt`, so each is declared `extern "C"` and the parameter list is the
// port's reading of the body, not a guess at a class.
//
//   fn_80048EA4  builds a CArchitectureMessage on the stack from (const char*, const int*,
//                const int*, void*) and dereferences all three pointers; r3 is the return slot,
//                which is why the class return type is what makes the caller pass `addi r3,r1,52`.
//   __nw__FUlPCcPCc  `operator new(size, file, line)`, and `lbl_803A60A0` is the `file`.
//   lbl_80417DE0/DE4  12 and 11 - the pair the kCFS_GameExit message is built from.
//   lbl_80417DE8/DEC  12 and 11 - the pair the kCFS_PreFrontEnd message is built from.
//   lbl_80417DF0/DF4  10 and 1000 - the pair the kCFS_Game message is built from.
//   StreamNewGameState__5CMainFR12CInputStreami  0x800053B8. `CMain.hpp` declares the member with
//                its two real parameters; this declaration is the *call site* and is spelled out
//                because of point 4 above.
extern "C" const char lbl_803A60A0[];
extern "C" void* __nw__FUlPCcPCc(unsigned long size, const char* file, const char* line);
extern "C" void* fn_800214A0(void* self);
extern "C" void* fn_80020478(void* self);
extern "C" void* fn_80022C74(void* self, int a);
extern "C" void* fn_80193E08(void* self);
extern "C" void* fn_801F47F4(void* self);
extern "C" CArchitectureMessage fn_80048EA4(const char* file, const int* a, const int* b, void* obj);
extern "C" void fn_801423A8(CGameState* self, void* p);
extern "C" void fn_80180598(void* hintOptions);
extern "C" void StreamNewGameState__5CMainFR12CInputStreami(CMain* self, CInputStream& in);
extern "C" int lbl_80417DE0;
extern "C" int lbl_80417DE4;
extern "C" int lbl_80417DE8;
extern "C" int lbl_80417DEC;
extern "C" int lbl_80417DF0;
extern "C" int lbl_80417DF4;

// PORT NOTE - the `new` sizes below are retail's **guest** `sizeof`s, and the port's objects are
// bigger (8-byte vtable pointer, host `rstl::string`). Placement-constructing a host object into
// a guest-sized block overruns the heap chunk, and glibc aborts later in an unrelated `free`
// ("free(): invalid size" from `CArchitectureMessage`'s `rc_ptr` in `CIOWinManager::PumpMessages`,
// measured 2026-09-29, the first frame `kCFS_PreFrontEnd` was reached). So the port build asks for
// the host size of every window whose constructor it actually has; mwcceppc still sees retail's
// literal. The windows whose constructors are still reach stubs keep the guest size until theirs
// are written - their object is not constructed either way.
#if defined(__MWERKS__)
#define PREFRONTEND_SIZE 20
#else
#include "MetroidPrime/CPreFrontEnd.hpp"
#define PREFRONTEND_SIZE sizeof(CPreFrontEnd)
#endif

// PORT NOTE - `CMain::ERestartMode`. Upstream renamed values 1..5 and shifted 6 and 7, and the two
// enumerations are **value-identical for 0..7**, which is what makes the renaming below a rename
// and not a change of behaviour. The evidence, all of it a measured constant:
//
//   * the end-of-game window sizes are unchanged, so 1..5 map 1:1. This file allocates 212/100/212/
//     104 bytes at `kRM_WinBest`(3)/`kRM_LoseGame`(4)/`kRM_Default`(5)/default; upstream's
//     `CMainFlow.cpp` allocates `CPlayMovie(4)`, `CAutoSave`, `CPlayMovie(6)`, `CCredits` at
//     `kRM_EndMovie1`/`kRM_EndAutoSave`/`kRM_EndMovie2`/default - the same four in the same order,
//     and both guard the range with `>= 1 && <= 5`.
//   * **`docs/research/boot_path.md:176`**: `li r0,6 ; stw r0,88(r31)` at 0x800063E0 stores 6 for
//     the "start a new game" state, which upstream calls `kRM_Default` and this file called
//     `kRM_StateSetter`. Upstream's `CMain.hpp` also has a 7-valued `kRM_StateSetter` and no 7 for
//     "pre front end", which cannot both be right.
//   * the two `!= `/`== ` tests at 0x8001DF08/0x8001DF2C are against **7** (this file's comment
//     above says so), which is `kRM_StateSetter` upstream and `kRM_PreFrontEnd` here.
//
// So: `kRM_WinBad`->`kRM_Credits1`, `kRM_WinGood`->`kRM_Credits2`, `kRM_WinBest`->`kRM_EndMovie1`,
// `kRM_LoseGame`->`kRM_EndAutoSave`, `kRM_Default`->`kRM_EndMovie2`, `kRM_StateSetter`->
// `kRM_Default` and `kRM_PreFrontEnd`->`kRM_StateSetter`. This file used none of the three values
// (8, 9, 10) that only the old naming had, so no enumerator has to be added. **The two naming
// schemes should be reconciled in `CMain.hpp`**: upstream's own `main.cpp` writes
// `restartMode(kRM_Default)`, which is 6, i.e. this file's `kRM_StateSetter`, and
// `boot_path.md` says the same. The names are the only thing that is wrong; the values are not.
//
// PORT NOTE - `StartGameFromFrontEnd` (retail `fn_80143884`) and `fn_80143E88` are declared by
// `include/MetroidPrime/Player/CGameState.hpp` (with C++ linkage; `PortReachStubs.cpp` gives them
// their retail names through `asm`), so a local `extern "C"` declaration of either is a hard
// error - "conflicting declaration with 'C' linkage". They are used from the header here.

// Descending by retail offset: 0x8001DE68, then 0x8001DB54, then 0x8001DAF4. mwcceppc emits
// definitions in reverse source order, so the other way round permutes this unit's .text and
// only `flip_test` notices.
void CMainFlow::AdvanceGameState(CArchitectureQueue& queue) {
  switch (mGameState) {
  case kCFS_Game:
    SetGameState(kCFS_GameExit, queue);
    break;
  case kCFS_PreFrontEnd:
    SetGameState(kCFS_FrontEnd, queue);
    break;
  case kCFS_FrontEnd:
    SetGameState(kCFS_Game, queue);
    break;
  case kCFS_GameExit:
    if (gpMain->GetRestartMode() != CMain::kRM_None
        && gpMain->GetRestartMode() != CMain::kRM_StateSetter) {
      if (gpGameState->GetGameModeType() == 0x534E474C) {
        gpMain->SetGameExitReset(true);
      } else {
        gpMain->ResetGameState();
      }
    }
  // fallthrough - see the note above: this is what shares one `SetGameState` body.
  case kCFS_Unspecified:
    SetGameState(kCFS_PreFrontEnd, queue);
    break;
  }
}

void CMainFlow::SetGameState(EClientFlowStates state, CArchitectureQueue& queue) {
  mGameState = state;
  switch (mGameState) {
  case kCFS_GameExit: {
    CMain::ERestartMode mode = gpMain->GetRestartMode();
    // The `bool` local is load-bearing: written as `if (mode >= .. && mode <= ..)` mwcceppc emits
    // the two compares and a direct branch, and retail materialises the predicate as
    // `li r0,0 / ... / li r0,1 / clrlwi. r0,r0,24 / beq`, which is how it tests a `bool`.
    bool valid = mode >= CMain::kRM_Credits1 && mode <= CMain::kRM_EndMovie2;
    if (valid) {
      void* win = nullptr;
      switch (mode) {
      case CMain::kRM_EndMovie2: {
        void* p = __nw__FUlPCcPCc(212, lbl_803A60A0, nullptr);
        if (p != nullptr) {
          p = fn_80022C74(p, 6);
        }
        win = p;
        break;
      }
      case CMain::kRM_EndAutoSave: {
        void* p = __nw__FUlPCcPCc(100, lbl_803A60A0, nullptr);
        if (p != nullptr) {
          p = fn_80020478(p);
        }
        win = p;
        break;
      }
      case CMain::kRM_EndMovie1: {
        void* p = __nw__FUlPCcPCc(212, lbl_803A60A0, nullptr);
        if (p != nullptr) {
          p = fn_80022C74(p, 4);
        }
        win = p;
        break;
      }
      default: {
        void* p = __nw__FUlPCcPCc(104, lbl_803A60A0, nullptr);
        if (p != nullptr) {
          p = fn_800214A0(p);
        }
        win = p;
        break;
      }
      }
      queue.Push(fn_80048EA4(nullptr, &lbl_80417DE0, &lbl_80417DE4, &win));
    }
    break;
  }
  case kCFS_PreFrontEnd:
    if (gpMain->GetRestartMode() != CMain::kRM_None) {
      void* win;
      void* p = __nw__FUlPCcPCc(PREFRONTEND_SIZE, lbl_803A60A0, nullptr);
      if (p != nullptr) {
        p = new (p) CPreFrontEnd();
      }
      win = p;
      queue.Push(fn_80048EA4(nullptr, &lbl_80417DE8, &lbl_80417DEC, &win));
    }
    break;
  case kCFS_FrontEnd: {
    CMain::ERestartMode mode = gpMain->GetRestartMode();
    int gameMode = gpGameState->GetGameMode().GetGameModeType();
    if (gameMode == 0x46524E44) {
      StartGameFromFrontEnd();
    } else if (mode != CMain::kRM_None) {
      if (gpMain->GetRestartMode() == CMain::kRM_StateSetter) {
        gpMain->SetRestartMode(CMain::kRM_Default);
        CInputStream* nullStream = nullptr;
        StreamNewGameState__5CMainFR12CInputStreami(gpMain, *nullStream);
        void* obj = __nw__FUlPCcPCc(12, lbl_803A60A0, nullptr);
        if (obj != nullptr) {
          obj = fn_80193E08(obj);
        }
        fn_801423A8(gpGameState, obj);
        fn_80180598(&gpGameState->HintOptions());
      } else {
        fn_80143E88();
      }
    }
    break;
  }
  case kCFS_Game:
    gpGameState->GameOptions().EnsureOptions();
    {
      void* win;
      void* p = __nw__FUlPCcPCc(44, lbl_803A60A0, nullptr);
      if (p != nullptr) {
        p = fn_801F47F4(p);
      }
      win = p;
      gpMain->SetRestartMode(CMain::kRM_Default);
      queue.Push(fn_80048EA4(nullptr, &lbl_80417DF0, &lbl_80417DF4, &win));
    }
    break;
  }
}

CMainFlow::~CMainFlow() {}
