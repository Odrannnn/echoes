// `CGraphics::SetTevOp`, retail `SetTevOp__9CGraphicsF12ERglTevStageRCQ213CTevCombiners8CTevPass`,
// .text 0x802BFA18..0x802BFA38, 32 bytes - and the `CTevCombiners` closure behind it.
//
// `src/MetroidPrime/CRainSplashGenerator.cpp:238` and `src/MetroidPrime/CSimpleShadow.cpp:76`
// are listed in `files.cmake` and both call `CGraphics::SetTevOp`, but retail's body lives in
// `src/Kyoto/Graphics/DolphinCGraphics.cpp`, a `configure.py` `NonMatching` unit that
// `files.cmake` does not list (PORT_NOTES.md records why), so the symbol was undefined in the
// port's link. This file gives it that body and the five functions it needs, all read off
// `build/G2ME01/main.elf`:
//
//   CGraphics::SetTevOp(ERglTevStage, CTevPass const&)  SetTevOp__9CGraphics...  0x802BFA18  0x20
//   CTevCombiners::SetupPass(int, CTevPass const&)      fn_802BE47C               0x802BE47C  0x5C
//   CTevCombiners::DeletePass(int)                       fn_802BE4D8               0x802BE4D8  0x44
//   CTevCombiners::SetPassCombiners(int, CTevPass const&) fn_802BE44C              0x802BE44C  0x30
//   CTevCombiners::RecomputePasses()                     fn_802BE588               0x802BE588  0x40
//   CTevCombiners::CTevPass::Execute(int) const          fn_802BE398               0x802BE398  0xB4
//
// `CTevCombiners::Init` (retail `fn_802BE51C`, 0x802BE51C, 0x6C) is the sixth member of the same
// family. It is **not** reachable from `SetTevOp`, so nothing in this file's first five functions
// needs it - but `CGraphicsHostStartup.cpp:449` calls it on the boot path, and every callee it has
// (`DeletePass`, `RecomputePasses`) is defined here, so its body is here too. See the section on
// `Init` below for what retail's 0x6C bytes say.
//
// ## SetTevOp is a thunk, and the thunk is the whole function
//
//     802bfa18:  94 21 ff f0   stwu  r1,-16(r1)
//     802bfa1c:  7c 08 02 a6   mflr  r0
//     802bfa20:  90 01 00 14   stw   r0,20(r1)
//     802bfa24:  4b ff ea 59   bl    802be47c <fn_802BE47C>
//     802bfa28:  80 01 00 14   lwz   r0,20(r1)
//     802bfa2c:  7c 08 03 a6   mtlr  r0
//     802bfa30:  38 21 00 10   addi  r1,r1,16
//     802bfa34:  4e 80 00 20   blr
//
// r3 (`ERglTevStage`) and r4 (the `CTevPass&`) reach `fn_802BE47C` untouched, which fixes both
// that it is `CTevCombiners::SetupPass(int, const CTevPass&)` - the declaration the header
// already has at `include/Kyoto/Graphics/CTevCombiners.hpp:179` - and that the stage needs no
// conversion on the way (retail passes the enum straight through, so a `static_cast` to
// `GXTevStageID` here changes no value).
//
// ## SetupPass: an address comparison, then the valid-pass bookkeeping
//
//     802be47c:  94 21 ff f0   stwu  r1,-16(r1)
//     802be480:  7c 08 02 a6   mflr  r0
//     802be484:  3c a0 80 41   lis   r5,-32703        ; r5 = 0x80410000
//     802be488:  90 01 00 14   stw   r0,20(r1)
//     802be48c:  38 05 6b 48   addi  r0,r5,27464      ; r0 = 0x80416B48 = lbl_80416B48
//     802be490:  7c 04 00 40   cmplw r4,r0            ; &pass == &lbl_80416B48 ?
//     802be494:  93 e1 00 0c   stw   r31,12(r1)
//     802be498:  7c 7f 1b 78   mr    r31,r3           ; r31 = stage
//     802be49c:  40 82 00 0c   bne   802be4a8
//     802be4a0:  48 00 00 39   bl    802be4d8 <fn_802BE4D8>   ; DeletePass(stage)
//     802be4a4:  48 00 00 20   b     802be4c4
//     802be4a8:  4b ff ff a5   bl    802be44c <fn_802BE44C>   ; SetPassCombiners(stage, pass)
//     802be4ac:  54 60 06 3f   clrlwi. r0,r3,24              ; if (result & 0xFF)
//     802be4b0:  41 82 00 14   beq   802be4c4
//     802be4b4:  38 00 00 01   li    r0,1
//     802be4b8:  38 6d 8d 58   addi  r3,r13,-29352    ; r3 = &sValidPasses
//     802be4bc:  7c 03 f9 ae   stbx  r0,r3,r31        ; sValidPasses[stage] = true
//     802be4c0:  48 00 00 c9   bl    802be588 <fn_802BE588>   ; RecomputePasses()
//     802be4c4:  80 01 00 14   lwz   r0,20(r1)
//     802be4c8:  83 e1 00 0c   lwz   r31,12(r1)
//     802be4cc:  7c 08 03 a6   mtlr  r0
//     802be4d0:  38 21 00 10   addi  r1,r1,16
//     802be4d4:  4e 80 00 20   blr
//
// `stbx r0,r3,r31` with `r3 = &sValidPasses` and `r31 = stage` is `sValidPasses[stage] = 1`,
// i.e. the two-byte small-data object that `CTevCombiners::ResetStates` and
// `CTevCombiners::Init` also write is `static bool sValidPasses[2]`, which is what
// `include/Kyoto/Graphics/CTevCombiners.hpp:189` declares.
//
// ## `lbl_80416B48` is the reset pass, and it is the one object the comparison names
//
// `lbl_80416B48 = .bss:0x80416B48; size:0x4C` (config/G2ME01/symbols.txt 19364). 0x4C is exactly
// `sizeof(CTevPass)` as the header lays it out - `uint mId` (4) + `ColorPass` 4 x `ColorVar`
// (16) + `AlphaPass` 4 x `AlphaVar` (16) + two `CTevOp` (20 each) - and `CTevCombiners::ResetStates`
// (retail `ResetStates__13CTevCombinersFv`, 0x802BE31C, 0x4C) loads that address into r3, zeroes
// the two valid-pass bytes, and then calls `fn_802BE398(r3, 0)`: the same call
// `CTevPass::Execute` is. So `lbl_80416B48` is a statically allocated `CTevPass` that the reset
// path executes on stage 0, and it is what `SetupPass` tests for. Being `.bss`, all of its fields
// are zero, which is `kCS_PreviousColor` / `kAS_PreviousAlpha` / `kTO_Add` / `kTB_Zero` /
// `kTS_Scale1` / `kTO_Previous` - passthrough, the pass a reset stage wants.
//
// It has no upstream name (it is not `CTevCombiners::kEnvPassthru`, which is a `const` member and
// lives in `.sdata2`), so this file names it `sResetPass` and gives it retail's zero-field value.
// Nothing in the port can hold its address - `lbl_80416B48` is on no undefined-symbol list - so
// the `DeletePass` branch is not taken by any port caller today, exactly as the file header says
// rather than pretending otherwise. What the port *does* need is that the comparison is by
// address and not by value: `kEnvPassthru` and `sResetPass` have identical fields, and retail
// distinguishes them.
//
// ## DeletePass and SetPassCombiners
//
// `fn_802BE4D8` (0x44) is `SetPassCombiners(stage, lbl_80416B48); sValidPasses[stage] = false;
// RecomputePasses();` - r3 = stage is moved to r31 before the call and the store is `stbx r0,r3,r31`
// with `r0 = 0` afterwards, so it is the same two-byte store as above with a zero. `fn_802BE44C`
// (0x30) swaps its two arguments into r3/r4, calls `fn_802BE398`, and returns 1 unconditionally -
// `li r3,1` on the way out - so `SetPassCombiners` always succeeds and retail's `if` around
// `sValidPasses[stage] = true` in `SetupPass` never fails. It is written as the real `if` rather
// than unfolded, because retail emits one.
//
// ## Init: the odd sequence is retail's, and it is not a bug in the transcription
//
// `fn_802BE51C` (0x6C) is the only function of the five that writes both statics and then
// immediately overwrites what it wrote:
//
//     802be51c:  94 21 ff f0   stwu  r1,-16(r1)
//     802be520:  7c 08 02 a6   mflr  r0
//     802be524:  38 60 00 01   li    r3,1
//     802be528:  90 01 00 14   stw   r0,20(r1)
//     802be52c:  38 00 00 02   li    r0,2
//     802be530:  93 e1 00 0c   stw   r31,12(r1)
//     802be534:  3b ed 8d 58   addi  r31,r13,-29352    ; r31 = &sValidPasses
//     802be538:  93 c1 00 08   stw   r30,8(r1)
//     802be53c:  3b c0 00 00   li    r30,0
//     802be540:  90 0d 8d 5c   stw   r0,-29348(r13)    ; sNumEnabledPasses = 2
//     802be544:  98 6d 8d 58   stb   r3,-29352(r13)    ; sValidPasses[0] = 1
//     802be548:  98 7f 00 01   stb   r3,1(r31)         ; sValidPasses[1] = 1
//     802be54c:  7f c3 f3 78   mr    r3,r30
//     802be550:  4b ff ff 89   bl    802be4d8 <fn_802BE4D8>   ; DeletePass(i)
//     802be554:  3b de 00 01   addi  r30,r30,1
//     802be558:  2c 1e 00 02   cmpwi r30,2
//     802be55c:  41 80 ff f0   blt   802be54c         ; while (i < 2)
//     802be560:  38 00 00 00   li    r0,0
//     802be564:  98 1f 00 00   stb   r0,0(r31)        ; sValidPasses[0] = 0
//     802be568:  98 1f 00 01   stb   r0,1(r31)        ; sValidPasses[1] = 0
//     802be56c:  48 00 00 1d   bl    802be588 <fn_802BE588>   ; RecomputePasses()
//
// So the two `stb r3` stores and the `stw r0,-29348(r13)` are real assignments that the loop then
// clobbers: `DeletePass(0)` sets `sValidPasses[0] = false` and `DeletePass(1)` sets
// `sValidPasses[1] = false`, and the two `stb r0` after the loop set them to false again. They are
// kept here rather than dropped, because dropping them is exactly the "delete an initialisation to
// gain percent" move the repo's rules forbid, and because the sequence is what makes the store
// *count* and the `CGX::SetNumTevStages` call sequence identical to retail's.
//
// `sNumEnabledPasses = 2` is the interesting one: it is dead after the loop, because both
// `DeletePass` calls run `RecomputePasses` and the last one leaves `sNumEnabledPasses == 1`.
// Retail wrote it, so retail wrote it.
//
// The loop counter is `r30`, tested with `cmpwi r30,2` / `blt` and incremented after the call, so
// it is `for (int i = 0; i < 2; ++i) DeletePass(i);` - not a range-for over `sValidPasses` and not
// an unrolled pair of calls.
//
// The net effect is that both stages end up holding the reset pass with both valid-pass bytes
// clear, i.e. one TEV stage enabled, which is what `CGX::SetNumTevStages(1)` at the end sets.
// `Init` is `CGraphics::InitGraphicsDefaults`' last graphics call before `DisableAllLights`, so on
// the host this runs once at start-up and the port gets one TEV stage until something calls
// `SetupPass` with a real pass.
//
// ## RecomputePasses
//
//     802be588:  3c 60 80 41   lis   r3,-32703        ; r3 = 0x80410000
//     802be590:  38 6d 8d 58   addi  r3,r13,-29352     ; r3 = &sValidPasses
//     802be598:  88 63 00 01   lbz   r3,1(r3)          ; sValidPasses[1]
//     802be59c:  7c 03 00 d0   neg   r0,r3
//     802be5a0:  7c 00 1b 78   or    r0,r0,r3
//     802be5a4:  54 03 0f fe   srwi  r3,r0,31          ; !!sValidPasses[1]
//     802be5a8:  38 63 00 01   addi  r3,r3,1
//     802be5ac:  54 60 06 3e   clrlwi r0,r3,24
//     802be5b0:  90 0d 8d 5c   stw   r0,-29348(r13)    ; sNumEnabledPasses = r3 & 0xFF
//     802be5b4:  4b ff fa fd   bl    802be0b0 <SetNumTevStages__3CGXFUc>
//
// `sNumEnabledPasses` is the word four bytes after `sValidPasses` (29348 vs 29352), which is the
// private `static uint` the header declares at line 190, and only **stage 1** decides the enabled
// count: `neg`/`or`/`srwi` is MWCC's `!!`, so it is `sValidPasses[1] ? 1 : 0`, and the
// `addi r3,r3,1` at 0x802BE5A8 then makes it `sValidPasses[1] ? 2 : 1` - retail always keeps
// stage 0, so the host gets 1 or 2 TEV stages, never 0 or 1. `CGX::SetNumTevStages` is retail's own
// `SetNumTevStages__3CGXFUc` and `src/Kyoto/Graphics/CGX.cpp:79` already has that body, so the
// call needs nothing from this file.
//
// ## Execute: the eight CGX calls, in retail's order
//
// `fn_802BE398` (0xB4) loads the pass's fields at these offsets and calls, in order:
//
//   +0x00 uint  mId           (never passed to GX - it is the pass-cache key)
//   +0x04..+0x10 ColorVar mA..mD  -> CGX::SetTevColorIn(stage, a, b, c, d)
//   +0x14..+0x20 AlphaVar mA..mD  -> CGX::SetTevAlphaIn(stage, a, b, c, d)
//   +0x24 bool  mColorOp.mClamp, +0x28 op, +0x2C bias, +0x30 scale, +0x34 mOutput
//                                -> CGX::SetTevColorOp(stage, op, bias, scale, clamp, out)
//   +0x38 bool  mAlphaOp.mClamp, +0x3C op, +0x40 bias, +0x44 scale, +0x48 mOutput
//                                -> CGX::SetTevAlphaOp(stage, op, bias, scale, clamp, out)
//   `li r4,0` after each          -> CGX::SetTevKColorSel(stage, GX_TEV_KCSEL_8_8)
//   `li r4,0` after that         -> CGX::SetTevKAlphaSel(stage, GX_TEV_KASEL_8_8)
//
// The offsets are the ones `CTevOp`'s member list in the header produces - `bool` first at +0x24
// and +0x38 (`lbz` reads exactly those two bytes) and the four enums after it at 4-byte strides -
// and the calls take the four values **unconverted**: retail loads a field with `lwz` and hands
// it to a `GXTevColorArg` parameter, which only works because `CTevCombiners::EColorSrc` and
// `GXTevColorArg` are the same fifteen values in the same order, and likewise the eight
// `EAlphaSrc`/`GXTevAlphaArg`, the two `ETevOp`/`GXTevOp`, the three `ETevBias`/`GXTevBias`, the
// four `ETevScale`/`GXTevScale` and the four `ETevOutput`/`GXTevRegID`. So the casts below are
// `static_cast`s of an already-correct value, not a translation.
//
// All eight callees are `CGX` members with bodies already in `src/Kyoto/Graphics/CGX.cpp`
// (lines 79, 93, 117, 141, 165, 189, 197), which is listed in `files.cmake`; that is why this
// closure is closed and the port's undefined count falls rather than merely holding.
//
// ## `sNextUniquePass` is here because the reset pass's constructor needs it
//
// `CTevPass`'s constructor (inline in the header, `mId(sNextUniquePass++)`) is the only reader of
// `CTevCombiners::sNextUniquePass`, and no file in the tree defines it. `sResetPass` is
// dynamically initialised, so without this definition the new object would open one undefined
// symbol. Retail's is `.bss`, so zero is its own initial value; any future port file that wants a
// `CTevPass` of its own should use this one rather than define it twice.
#include "Kyoto/Graphics/CGraphics.hpp"

#include "Kyoto/Graphics/CGX.hpp"
#include "Kyoto/Graphics/CTevCombiners.hpp"

#include <dolphin/gx/GXEnum.h>

// Retail's `CTevCombiners::sNextUniquePass`; see the header's last section.
int CTevCombiners::sNextUniquePass = 0;

// Retail's `sValidPasses[2]` and `sNumEnabledPasses`: the two-byte small-data object at
// r13-29352 and the word four bytes after it (29348). They are the private statics the header
// declares at lines 189-190, so they are defined **as class members** - a file-scope pair with
// the same names would leave the members undefined and open two new symbols instead.
// `static` data members may be defined out of class whether or not they are private.
bool CTevCombiners::sValidPasses[2];
uint CTevCombiners::sNumEnabledPasses;

// `ColorVar`/`AlphaVar` are one-enum wrappers and retail never emits them as symbols: retail's
// `CTevPass` constructor (0x8025AA1C) inlines both into a flat run of `lwz`/`stw` pairs - see the
// header's section on `Execute` for the same `lwz`/`stw` shape. So these are the one-field stores
// that expansion already is, spelled out because the header declares the two constructors and no
// file in the tree defined them; `sResetPass`'s construction is the port's first caller.
CTevCombiners::ColorVar::ColorVar(EColorSrc src) : mSrc(src) {}
CTevCombiners::AlphaVar::AlphaVar(EAlphaSrc src) : mSrc(src) {}

// Retail's `lbl_80416B48`, the reset pass; see the header's third section. All fields zero in
// retail's `.bss`, which is passthrough on every field.
//
// Both `CTevOp`s are spelled out because the header's default constructor is `clamp = true`,
// while retail's `.bss` leaves `mClamp` 0. Defaulting the arguments here would hand
// `clamp = true` to `CGX::SetTevColorOp`/`SetTevAlphaOp` the moment `CTevCombiners::Init`
// executes this pass; every other field's default already matches zero.
static CTevCombiners::CTevPass sResetPass(
    CTevCombiners::ColorPass(CTevCombiners::kCS_PreviousColor, CTevCombiners::kCS_PreviousColor,
                             CTevCombiners::kCS_PreviousColor, CTevCombiners::kCS_PreviousColor),
    CTevCombiners::AlphaPass(CTevCombiners::kAS_PreviousAlpha, CTevCombiners::kAS_PreviousAlpha,
                             CTevCombiners::kAS_PreviousAlpha, CTevCombiners::kAS_PreviousAlpha),
    CTevCombiners::CTevOp(CTevCombiners::kTO_Add, CTevCombiners::kTB_Zero,
                          CTevCombiners::kTS_Scale1, false, CTevCombiners::kTO_Previous),
    CTevCombiners::CTevOp(CTevCombiners::kTO_Add, CTevCombiners::kTB_Zero,
                          CTevCombiners::kTS_Scale1, false, CTevCombiners::kTO_Previous));

void CTevCombiners::RecomputePasses() {
  // Only stage 1 decides the count; `neg`/`or`/`srwi` in retail is MWCC's `!!`, and the
  // `addi r3,r3,1` at 0x802BE5A8 adds one on top - so retail passes 1 or 2, never 0 or 1.
  sNumEnabledPasses = 1 + (sValidPasses[1] != 0);
  CGX::SetNumTevStages(static_cast< uchar >(sNumEnabledPasses));
}

bool CTevCombiners::SetPassCombiners(int stage, const CTevPass& pass) {
  pass.Execute(stage);
  // Retail returns 1 unconditionally (`li r3,1` at 0x802BE46C), so `SetupPass`'s `if` never fails.
  return true;
}

void CTevCombiners::DeletePass(int stage) {
  // Retail applies the reset pass to the stage, then clears the flag and recomputes.
  SetPassCombiners(stage, sResetPass);
  sValidPasses[stage] = false;
  RecomputePasses();
}

void CTevCombiners::SetupPass(int stage, const CTevPass& pass) {
  // Retail compares **addresses** (`cmplw r4,r0` at 0x802BE490), not contents: the reset pass and
  // `CGraphics::kEnvPassthru` have identical fields and must not be confused.
  if (&pass == &sResetPass) {
    DeletePass(stage);
  } else if (SetPassCombiners(stage, pass)) {
    sValidPasses[stage] = true;
    RecomputePasses();
  }
}

void CTevCombiners::Init() {
  // Retail's two `stb r3` (0x802BE544, 0x802BE548) and its `stw r0,-29348(r13)` (0x802BE540).
  // The loop below and the two stores after it clear every one of them again, so all three are
  // dead stores on their own; they are retail's, and removing them would change the store count
  // and the call sequence. See the header's section on `Init`.
  sNumEnabledPasses = 2;
  sValidPasses[0] = true;
  sValidPasses[1] = true;
  // `mr r3,r30` / `bl` / `addi r30,r30,1` / `cmpwi r30,2` / `blt`: a counted loop over both
  // stages, not two unrolled calls and not a range-for.
  for (int i = 0; i < 2; ++i) {
    DeletePass(i);
  }
  sValidPasses[0] = false;
  sValidPasses[1] = false;
  RecomputePasses();
}

void CTevCombiners::CTevPass::Execute(int stage) const {
  const GXTevStageID stageId = static_cast< GXTevStageID >(stage);
  // Retail hands each field straight to the GX wrapper (`lwz` + `bl`), so every cast below is of
  // an already-correct value: the two enum families are identical term for term.
  CGX::SetTevColorIn(stageId, static_cast< GXTevColorArg >(mColorPass.GetA().GetSource()),
                     static_cast< GXTevColorArg >(mColorPass.GetB().GetSource()),
                     static_cast< GXTevColorArg >(mColorPass.GetC().GetSource()),
                     static_cast< GXTevColorArg >(mColorPass.GetD().GetSource()));
  CGX::SetTevAlphaIn(stageId, static_cast< GXTevAlphaArg >(mAlphaPass.GetA().GetSource()),
                     static_cast< GXTevAlphaArg >(mAlphaPass.GetB().GetSource()),
                     static_cast< GXTevAlphaArg >(mAlphaPass.GetC().GetSource()),
                     static_cast< GXTevAlphaArg >(mAlphaPass.GetD().GetSource()));
  CGX::SetTevColorOp(stageId, static_cast< GXTevOp >(mColorOp.GetOp()),
                     static_cast< GXTevBias >(mColorOp.GetBias()),
                     static_cast< GXTevScale >(mColorOp.GetScale()),
                     static_cast< GXBool >(mColorOp.GetClamp()),
                     static_cast< GXTevRegID >(mColorOp.GetOutput()));
  CGX::SetTevAlphaOp(stageId, static_cast< GXTevOp >(mAlphaOp.GetOp()),
                     static_cast< GXTevBias >(mAlphaOp.GetBias()),
                     static_cast< GXTevScale >(mAlphaOp.GetScale()),
                     static_cast< GXBool >(mAlphaOp.GetClamp()),
                     static_cast< GXTevRegID >(mAlphaOp.GetOutput()));
  CGX::SetTevKColorSel(stageId, GX_TEV_KCSEL_8_8);
  CGX::SetTevKAlphaSel(stageId, GX_TEV_KASEL_8_8);
}

void CGraphics::SetTevOp(ERglTevStage stage, const CTevCombiners::CTevPass& pass) {
  // Retail's body is the 32-byte thunk at the top of this file: `bl fn_802BE47C` with r3 and r4
  // untouched, which is `SetupPass(stage, pass)`.
  CTevCombiners::SetupPass(static_cast< int >(stage), pass);
}