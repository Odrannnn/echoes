// `CGraphics::SetViewPointMatrix` - retail `SetViewPointMatrix__9CGraphicsFRC12CTransform4f`,
// .text 0x802C2534..0x802C2614, 0xE0 = 224 bytes:
//
//     802c2534:  94 21 ff c0   stwu  r1,-64(r1)
//     802c2538:  7c 08 02 a6   mflr  r0
//     802c253c:  3c 80 80 41   lis   r4,-32703
//     802c2540:  90 01 00 44   stw   r0,68(r1)
//     802c2544:  93 e1 00 3c   stw   r31,60(r1)
//     802c2548:  7c 7f 1b 78   mr    r31,r3
//     802c254c:  38 64 6f 44   addi  r3,r4,28484   ; 0x80416F44 mViewMatrix__9CGraphics
//     802c2550:  7f e4 fb 78   mr    r4,r31
//     802c2554:  48 00 6a cd   bl    802c9020 <CTransform4f::operator=>
//     ... 12 loads from (r31), 12 fneg, 12 stores to (r4) = 0x804172A0 ...
//     802c25a4:  38 61 00 08   addi  r3,r1,8        ; the stack Mtx
//     802c25e0:  48 0a af b5   bl    8036d594 <PSMTXTrans>
//     802c25e4:  3c 60 80 41   lis   r3,-32703
//     802c25e8:  3c a0 80 41   lis   r5,-32703
//     802c25ec:  38 63 72 a0   addi  r3,r3,29344     ; 0x804172A0
//     802c25f0:  38 81 00 08   addi  r4,r1,8
//     802c25f4:  38 a5 73 30   addi  r5,r5,29488     ; 0x80417330
//     802c25f8:  48 0a ae 09   bl    8036d400 <PSMTXConcat>
//     802c25fc:  48 00 00 19   bl    802c2614 <fn_802C2614>
//     802c2600:  80 01 00 44   lwz   r0,68(r1)
//     802c2604:  83 e1 00 3c   lwz   r31,60(r1)
//     802c2608:  7c 08 03 a6   mtlr  r0
//     802c260c:  38 21 00 40   addi  r1,r1,64
//     802c2610:  4e 80 00 20   blr
//
// **It is a view matrix, and a GX `Mtx` is only 3x4, so the transpose is done by hand here
// and the translation is a *second* matrix rather than a fourth column.** The twelve stores
// to 0x804172A0 are the transposed 3x3 with a **zero** last column:
//
//     [0][0]=m[0][0]  [0][1]=m[1][0]  [0][2]=m[2][0]   [0][3]=0
//     [1][0]=m[0][2]  [1][1]=m[1][2]  [1][2]=m[2][2]   [1][3]=0
//     [2][0]=-m[0][1] [2][1]=-m[1][1] [2][2]=-m[2][1]  [2][3]=0
//
// and `PSMTXTrans` builds the translation on the stack, its three `fneg`ed arguments being
// `-m[i][3]` - the same sign as row 2, because the view matrix's translation column is the
// negated eye position. `PSMTXConcat` multiplies the two into 0x80417330 and the result goes
// to `fn_802C2614` (0x802C2614, 0x9C), the same shared tail `CGraphics::SetModelMatrix`
// calls, which owns the identity latches.
//
// **The zero column is a literal `0.f` in retail's source, and that is load-bearing - but
// the literal cannot be used here, and the reason is the reason this unit is `NonMatching`.**
// MWCC allocates float temporaries f12, f11, f10, ... in *creation* order, and it creates a
// read of a *global* ahead of everything else. Retail's allocation is f12/f11/f10 to
// `m[i][0]` and **f9** to the zero, which is the zero being the *fourth* temporary - i.e. it
// was a pooled literal, not a variable read. Measured with `tools/probe_cc.sh`, over all 55
// instructions the two spellings differ in exactly four register names:
//
//     = 0.f            zero f9   m[i][0] f12,f11,f10   <- retail
//     = lbl_8041E508   zero f12  m[i][0] f11,f10,f9    <- 10 wrong bytes in the DOL
//
// (`lfs f9,-16056(r2)` with `_SDA2_BASE_` = 0x804223C0 puts the load at 0x8041E508, and
// `tools/dol_read.py 0x8041E508 0x10` reads `00000000`, so retail's constant is a real
// zero and dtk's `auto_11_8041E278_sdata2` gap object does define `lbl_8041E508` as `.float 0`.)
//
// **Claiming the 4-byte `.sdata2` at 0x8041E508 does not work either**, and that is the
// wall. MWCC pools a float literal into an *unnamed* 4-byte `.sdata2` COMDAT, so a unit can
// place it at a claimed address - but claiming 0x8041E508..0x8041E50C removes that address
// from dtk's gap object, and with it the *name* `lbl_8041E508`, which five other retail
// functions in unclaimed ranges reference by name: `fn_802C229C`, `fn_802C27C4`,
// `fn_802C2B38`, `fn_802C2CC0` and `CGraphics::LoadDolphinSpareTexture`. `mwldeppc` then
// reports `undefined: 'lbl_8041E508'` six times and the DOL does not link at all. Measured,
// not argued: with the claim the link fails; without it the link succeeds and the DOL is
// retail everywhere except the ten bytes above.
//
// So this is `NonMatching` at 99.11%, and the ten wrong bytes are the four float-register
// fields. Two named ways forward, both outside this file: a `lbl_8041E508` **definition**
// that MWCC places in `.sdata2` rather than `.sbss` (it puts `float x = 0.f` in `.sbss`,
// because the all-zero bit pattern is indistinguishable from uninitialised) so the claim
// can keep the name alive; or MWCC treating a global read like any other temporary.
//
// The three f-registers left over from those negations (f1, f2, f3 at 0x802c2588,
// 0x802c2594, 0x802c259c) are the `PSMTXTrans` arguments and are never reloaded: that is
// the scheduler keeping them live across twelve stores rather than the source computing
// `-m[i][3]` twice.
//
// `mViewMatrix__9CGraphics` is 0x80416F44, `size:0x30` in `config/G2ME01/symbols.txt`, and
// `CTransform4f` is 16 floats, so the header's `static CTransform4f mViewMatrix` is that
// object. It is reached by its dtk name for the reason the rest of this class is:
// `CGraphics` has no `.cpp` that could define a C++-named static, and a `Matching` object is
// linked into the DOL where only the retail names exist.
//
// A named C++ member, so a `.cpp` is right here (an anonymous carve would have to be a `.c`).
// One discontiguous `.text` range, one function, so the descending-source-order rule is
// trivially met - recorded because every other file in this directory carries it.
#include "Kyoto/Graphics/CGraphics.hpp"

#include "Kyoto/Math/CTransform4f.hpp"

#include <dolphin/mtx.h>

extern "C" {
extern CTransform4f mViewMatrix__9CGraphics;
extern Mtx lbl_804172A0;
extern Mtx lbl_80417330;
extern void fn_802C2614();
extern float lbl_8041E508;
}

void CGraphics::SetViewPointMatrix(const CTransform4f& xf) {
  mViewMatrix__9CGraphics = xf;
  const ConstMtxPtr m = xf.GetCStyleMatrix();
  Mtx trans;
  lbl_804172A0[0][0] = m[0][0];
  lbl_804172A0[0][1] = m[1][0];
  lbl_804172A0[0][2] = m[2][0];
  lbl_804172A0[0][3] = lbl_8041E508;
  lbl_804172A0[1][0] = m[0][2];
  lbl_804172A0[1][1] = m[1][2];
  lbl_804172A0[1][2] = m[2][2];
  lbl_804172A0[1][3] = lbl_8041E508;
  lbl_804172A0[2][0] = -m[0][1];
  lbl_804172A0[2][1] = -m[1][1];
  lbl_804172A0[2][2] = -m[2][1];
  lbl_804172A0[2][3] = lbl_8041E508;
  PSMTXTrans(trans, -m[0][3], -m[1][3], -m[2][3]);
  PSMTXConcat(lbl_804172A0, trans, lbl_80417330);
  fn_802C2614();
}
