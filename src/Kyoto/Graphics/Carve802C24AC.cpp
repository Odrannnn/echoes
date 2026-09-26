// `CGraphics::SetModelMatrix` - retail `SetModelMatrix__9CGraphicsFRC12CTransform4f`,
// .text 0x802C24AC..0x802C250C, 96 bytes:
//
//     802c24ac:  94 21 ff f0   stwu  r1,-16(r1)
//     802c24b0:  7c 08 02 a6   mflr  r0
//     802c24b4:  3c 80 80 41   lis   r4,-32703
//     802c24b8:  90 01 00 14   stw   r0,20(r1)
//     802c24bc:  38 04 73 d4   addi  r0,r4,29652     ; 0x804173D4 sIdentity
//     802c24c0:  7c 03 00 40   cmplw r3,r0            ; &xf == &sIdentity ?
//     802c24c4:  40 82 00 1c   bne   802c24e0
//     802c24c8:  88 0d 8d 7d   lbz   r0,-29315(r13)   ; 0x80418AFD
//     802c24cc:  28 00 00 00   cmplwi r0,0
//     802c24d0:  40 82 00 2c   bne   802c24fc        ; already identity: do nothing
//     802c24d4:  38 00 00 01   li    r0,1
//     802c24d8:  98 0d 8d 7d   stb   r0,-29315(r13)   ; flag = true
//     802c24dc:  48 00 00 0c   b     802c24e8
//     802c24e0:  38 00 00 00   li    r0,0
//     802c24e4:  98 0d 8d 7d   stb   r0,-29315(r13)   ; flag = false
//     802c24e8:  3c a0 80 41   lis   r5,-32703
//     802c24ec:  7c 64 1b 78   mr    r4,r3
//     802c24f0:  38 65 6f 74   addi  r3,r5,28532     ; 0x80416F74
//     802c24f4:  48 00 6b 2d   bl    802c9020 <CTransform4f::operator=>
//     802c24f8:  48 00 01 1d   bl    802c2614 <fn_802C2614>
//     802c24fc:  80 01 00 14   lwz   r0,20(r1)
//     ... epilogue
//
// **It is a "make the model matrix the identity exactly once" latch, and the identity
// is recognised by *address*, not by value.** The argument is compared with
// `sIdentity__12CTransform4f` (`.bss:0x804173D4, size:0x34`) using `cmplw` - a pointer
// compare - so a caller's own copy of an identity matrix is *not* the sentinel and
// does install. When the sentinel arrives and the flag is already set, the function
// returns having done nothing at all: the caller's `SetModelMatrix(sIdentity)` is
// being used to assert that the current matrix is the identity, and re-uploading it
// would be a redundant GX call. The flag is `lbl_80418AFD`, which
// `include/Kyoto/Graphics/CGraphics.hpp:457` calls `mIsGXModelMatrixIdentity`.
//
// The matrix is stored at 0x80416F74, which `config/G2ME01/symbols.txt` gives as
// `lbl_80416F74 = .bss:0x80416F74; size:0x30` - a 0x30-byte `CTransform4f`, immediately
// after `mViewMatrix__9CGraphics` (0x80416F44, 0x30). **It is not `mGXModelMatrix`**,
// which the header declares as a `Mtx`: an `Mtx` is 64 bytes, so it cannot be this
// 48-byte object. The header's `static Mtx mGXModelMatrix;` names a different thing
// again, and nothing in the tree references either spelling, so the `lbl_` one is used
// here - the same rule as the rest of this class, for the same reason (`CGraphics` has
// no `.cpp` that could define a C++-named static, and a `Matching` object is linked
// into the DOL where only the retail names exist).
//
// The trailing `fn_802C2614` (0x802C2614, 0x9C) is unnamed in retail, in an unclaimed
// `auto_03_*` range, so it is defined by dtk's fill in the DOL link. Per the
// correction in `docs/research/boot_path.md`, that is a *relocation* and not a
// precondition: nothing has to be written for a `Matching` unit to call it.
#include "Kyoto/Graphics/CGraphics.hpp"

#include "Kyoto/Math/CTransform4f.hpp"
#include "Kyoto/Math/CVector3f.hpp"

extern "C" {
extern CTransform4f sIdentity__12CTransform4f;
extern uchar lbl_80418AFD;
extern CTransform4f lbl_80416F74;
extern void fn_802C2614();
}

// The host-side storage for these guest globals is in
// `src/Kyoto/Graphics/CGraphicsHostGlobals.cpp`, which is port-only because
// `configure.py` does not claim it. It was `#ifdef TARGET_PC` here, which is wrong:
// `TARGET_PC` reaches `mp_game` only under `MP_SDK_HEADERS_ONLY`, so the ordinary port
// link passed and only `tools/boot_probe.sh` failed to link.

void CGraphics::SetModelMatrix(const CTransform4f& xf) {
  if (&xf == &sIdentity__12CTransform4f) {
    if (lbl_80418AFD) {
      return;
    }
    lbl_80418AFD = 1;
  } else {
    lbl_80418AFD = 0;
  }
  lbl_80416F74 = xf;
  fn_802C2614();
}
