// `CGraphics::SetUseVideoFilter` - retail `SetUseVideoFilter__9CGraphicsFb`,
// .text 0x802BEC24..0x802BEC6C, 72 bytes:
//
//     802bec24:  94 21 ff f0   stwu  r1,-16(r1)
//     802bec28:  7c 08 02 a6   mflr  r0
//     802bec2c:  3c 80 80 41   lis   r4,-32703      ; r4 = 0x80410000
//     802bec30:  54 65 06 3e   clrlwi r5,r3,24       ; r5 = arg & 0xFF
//     802bec34:  90 01 00 14   stw   r0,20(r1)
//     802bec38:  38 c4 72 64   addi  r6,r4,29284     ; r6 = &mRenderModeObj
//     802bec3c:  7c 05 00 d0   neg   r0,r5
//     802bec40:  98 6d 8d 7f   stb   r3,-29313(r13)  ; lbl_80418AFF = arg
//     802bec44:  7c 00 2b 78   or    r0,r0,r5
//     802bec48:  88 66 00 19   lbz   r3,25(r6)       ; mRenderModeObj.aa
//     802bec4c:  38 86 00 1a   addi  r4,r6,26        ; &...sample_pattern
//     802bec50:  54 05 0f fe   srwi  r5,r0,31        ; r5 = (arg != 0)
//     802bec54:  38 c6 00 32   addi  r6,r6,50        ; &...vfilter
//     802bec58:  48 0a aa 81   bl    803696d8 <GXSetCopyFilter>
//     802bec5c:  80 01 00 14   lwz   r0,20(r1)
//     802bec60:  7c 08 03 a6   mtlr  r0
//     802bec64:  38 21 00 10   addi  r1,r1,16
//     802bec68:  4e 80 00 20   blr
//
// `GXSetCopyFilter`'s real signature is four arguments -
// `void GXSetCopyFilter(GXBool aa, const u8 sample_pattern[12][2], GXBool vf, const u8 vfilter[7])`
// (`include/dolphin/gx/GXFrameBuffer.h:52`) - and **three of the four come out of
// `CGraphics::mRenderModeObj`, whose type in the header is already right and is worth
// stating because it is a rare case in this tree of a header that is correct:
//
//   offset  what                              proof
//   0x19    aa                                `lbz r3,25(r6)`
//   0x1A    sample_pattern[12][2] (24 bytes)  `addi r4,r6,26`, and GXSetCopyFilter reads it
//   0x32    vfilter[7] (7 bytes)              `addi r6,r6,50`
//   0x39..0x3B padding to the align-4 size    `mRenderModeObj__9CGraphics` is `size:0x3C`
//
// which is `GXRenderModeObj` field for field: `viTVmode` (4) + seven `u16`s (14) = 0x12,
// `xFBmode` at 0x14, `field_rendering` at 0x18, `aa` at 0x19, `sample_pattern` at 0x1A,
// `vfilter` at 0x32, 0x39 rounded up to **0x3C**. So the header's
// `static GXRenderModeObj mRenderModeObj;` needs no change, and the three reads are
// field accesses rather than raw offsets.
//
// **The fourth argument is the function's own parameter, not the struct's
// `field_rendering` at 0x18** - retail never reads 0x18 - so this function is the only
// thing that installs a video filter, and it derives the `vf` flag from the caller's
// `bool`. The `neg`/`or`/`srwi` triple is MWCC's `!!`, i.e. the argument is
// re-normalised on the way to the call (`clrlwi` first, so it is `!!(arg & 0xFF)`).
//
// The global is reached twice, two different ways, and both are forced:
//
//  * `mRenderModeObj` is **named in the DOL** - `nm` on `main.elf` gives
//    `80417264 B mRenderModeObj__9CGraphics`, an unmangled name, so the declaration has to
//    be `extern "C"`. It is 0x3C bytes, so it is not small-data eligible and gets
//    `lis`+`addi` in two registers - which is why retail keeps the `lis` result in r4 and
//    does the `addi` into r6, the same way the 48-byte `Carve802BF9C8.cpp` does.
//  * `lbl_80418AFF` is **unnamed** (that is the same word `Carve802BEC1C.cpp` reads), so
//    only the dtk label exists to reference it. `include/Kyoto/Graphics/CGraphics.hpp:459`
//    calls it `mUseVideoFilter` and leaves it undefined, deliberately.
#include "Kyoto/Graphics/CGraphics.hpp"

#include <dolphin/gx/GXFrameBuffer.h>

extern "C" {
extern GXRenderModeObj mRenderModeObj__9CGraphics;
extern uchar lbl_80418AFF;
}

// The host-side storage for these guest globals is in
// `src/Kyoto/Graphics/CGraphicsHostGlobals.cpp`, which is port-only because
// `configure.py` does not claim it. It was `#ifdef TARGET_PC` here, which is wrong:
// `TARGET_PC` reaches `mp_game` only under `MP_SDK_HEADERS_ONLY`, so the ordinary port
// link passed and only `tools/boot_probe.sh` failed to link.

void CGraphics::SetUseVideoFilter(bool filter) {
  // The **pointer**, not a reference, is load-bearing: `&mRenderModeObj__9CGraphics`
  // bound to a `GXRenderModeObj&` compiles the same code except that MWCC schedules
  // `addi r6,r4,0x7264` one slot *later* than retail and puts `neg r0,r5` in its
  // place - two differing instructions out of thirteen, which objdiff scores as
  // 88% and which is exactly the "right length, right arithmetic, wrong registers"
  // case. `tools/try_batch.py` over eight spellings: the pointer is the only one
  // that matches; `filter` passed directly instead of `filter != 0` drops the whole
  // `neg`/`or`/`srwi` triple, and `filter == true` or `filter ? 1 : 0` re-derives
  // it with a completely different instruction set.
  //
  //
  // The `const` on the pointer is **MWCC-only and load-bearing twice over**. It
  // changes the scheduling (dropping it puts `neg r0,r5` before `addi r6,r4,0x7264`
  // again, which is the two-instruction difference this spelling exists to remove),
  // and the GameCube SDK's `GXSetCopyFilter` takes `const u8[12][2]` and
  // `const u8[7]` while Aurora's does not
  // (`extern/aurora/include/dolphin/gx/GXFrameBuffer.h:45`), so a host build of the
  // un-guarded line fails with four `invalid conversion` errors. The `#ifdef
  // TARGET_PC` is therefore not decoration: it is what lets one source satisfy both
  // declarations, and `#ifdef TARGET_PC` is exactly the switch mwcceppc does not
  // take, so the gamecube object is unaffected.
#ifdef TARGET_PC
  GXRenderModeObj* rm = &mRenderModeObj__9CGraphics;
#else
  const GXRenderModeObj* rm = &mRenderModeObj__9CGraphics;
#endif
  lbl_80418AFF = filter;
  GXSetCopyFilter(rm->aa, rm->sample_pattern, filter != 0, rm->vfilter);
}
