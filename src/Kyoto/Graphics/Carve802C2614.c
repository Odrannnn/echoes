// Carved out of an unclaimed dtk `auto_*` range. Every number here is measured: the address
// and size come from `config/G2ME01/symbols.txt` (`fn_802C2614 = .text:0x802C2614`,
// `size:0x9C`), and the instructions are the ones in `build/G2ME01/main.elf` at that range.
//
// .text 0x802C2614..0x802C26B0, 0x9C = 156 bytes, 1 function:
//
//   802c2614:  94 21 ff f0   stwu  r1,-16(r1)
//   802c2618:  7c 08 02 a6   mflr  r0
//   802c261c:  90 01 00 14   stw   r0,20(r1)
//   802c2620:  88 0d 8d 7d   lbz   r0,-29315(r13)   ; 0x80418AFD
//   802c2624:  28 00 00 00   cmplwi r0,0
//   802c2628:  41 82 00 1c   beq   802c2644
//   802c262c:  3c 60 80 41   lis   r3,-32703
//   802c2630:  3c 80 80 41   lis   r4,-32703
//   802c2634:  38 63 73 30   addi  r3,r3,29488     ; 0x80417330
//   802c2638:  38 84 72 d0   addi  r4,r4,29392     ; 0x804172D0
//   802c263c:  48 0a ad 91   bl    8036d3cc <PSMTXCopy>
//   802c2640:  48 00 00 20   b     802c2660
//   802c2644:  3c 60 80 41   lis   r3,-32703
//   802c2648:  3c 80 80 41   lis   r4,-32703
//   802c264c:  3c a0 80 41   lis   r5,-32703
//   802c2650:  38 63 73 30   addi  r3,r3,29488     ; 0x80417330
//   802c2654:  38 84 6f 74   addi  r4,r4,28532     ; 0x80416F74
//   802c2658:  38 a5 72 d0   addi  r5,r5,29392     ; 0x804172D0
//   802c265c:  48 0a ad a5   bl    8036d400 <PSMTXConcat>
//   802c2660:  3c 60 80 41   lis   r3,-32703
//   802c2664:  38 80 00 00   li    r4,0
//   802c2668:  38 63 72 d0   addi  r3,r3,29392     ; 0x804172D0
//   802c266c:  48 0a a0 5d   bl    8036c6c8 <GXLoadPosMtxImm>
//   802c2670:  88 0d 8d 7c   lbz   r0,-29316(r13)   ; 0x80418AFC
//   802c2674:  28 00 00 00   cmplwi r0,0
//   802c2678:  41 82 00 28   beq   802c26a0
//   802c267c:  3c 60 80 41   lis   r3,-32703
//   802c2680:  3c 80 80 41   lis   r4,-32703
//   802c2684:  38 63 72 d0   addi  r3,r3,29392     ; 0x804172D0
//   802c2688:  38 84 73 00   addi  r4,r4,29440     ; 0x80417300
//   802c268c:  48 0a ae 41   bl    8036d4cc <PSMTXInvXpose>
//   802c2690:  3c 60 80 41   lis   r3,-32703
//   802c2694:  38 80 00 00   li    r4,0
//   802c2698:  38 63 73 00   addi  r3,r3,29440     ; 0x80417300
//   802c269c:  48 0a a0 ad   bl    8036c748 <GXLoadNrmMtxImm>
//   802c26a0:  80 01 00 14   lwz   r0,20(r1)
//   802c26a4:  7c 08 03 a6   mtlr  r0
//   802c26a8:  38 21 00 10   addi  r1,r1,16
//   802c26ac:  4e 80 00 20   blr
//
// **It is CGraphics' "push the matrices to the GX pipe" step, and it is reached from both
// matrix setters.** `SetModelMatrix` (0x802C24AC) ends in `bl fn_802C2614` and
// `SetViewPointMatrix` (0x802C2534) ends in the same `bl`, so this is the shared tail of the
// pair: neither setter talks to the GX pipe itself.
//
// The two flags are the *identity latches* from the same header - `include/Kyoto/Graphics/
// CGraphics.hpp` names 0x80418AFD `mIsGXModelMatrixIdentity` and 0x80418AFC
// `mIsGXNormalMatrixIdentity` (this file's sibling `Carve802C24AC.cpp` reads the first one by
// that name). Retail skips work it already knows is a no-op:
//   - if the model matrix was installed from the `sIdentity` sentinel, the concatenated
//     matrix cannot be anything but the identity, so the `PSMTXConcat` with the model is
//     replaced by a `PSMTXCopy` of the view matrix alone. That is the whole difference
//     between the two arms, and it is why the identity arm is 4 instructions shorter.
//   - if the normal-matrix flag is clear, `PSMTXInvXpose` + `GXLoadNrmMtxImm` are skipped
//     entirely. `PSMTXInvXpose` is the only expensive call in the function, so this is the
//     flag that matters for a static scene.
//
// `0x804172D0` is the composed GX position matrix (model*view, in GX's row-vector order) and
// `0x80417300` its inverse-transpose, uploaded as GX_PNMTX0 (`GXLoadPosMtxImm`/`GXLoadNrmMtxImm`
// are called with id 0). `0x80417330` is what `SetViewPointMatrix` composes, and `0x80416F74`
// is the model matrix `SetModelMatrix` stores - both `size:0x30` in `symbols.txt`, and `Mtx`
// is `f32[3][4]` = 0x30 bytes (`include/dolphin/mtx/GeoTypes.h:24`), so they are Mtx and not
// CTransform4f-sized-by-coincidence.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% and a broken DOL. Only `tools/flip_test.sh`
// catches that. One function, one discontiguous `.text` range, so the ordering rule is
// trivially satisfied here; it is recorded because every other file in this directory
// carries it and a reader of this one should not have to look it up.
//
// Retail names this `fn_802C2614` and the definition has to reproduce that symbol verbatim,
// so the unit is a `.c`: a C++ one mangles to `_Z13fn_802C2614v`, objdiff pairs nothing, and
// the unit silently scores 0/0. `Carve802BE7E4.c` in this directory is the same rule.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with a link-order cycle), and because 0x802C24AC..0x802C250C and
// 0x802C2534..0x802C2614 are both already claimed by `Carve802C24AC.cpp`'s neighbourhood
// and the next function up, `fn_802C26B0`, is 0xAC bytes.
//
// **This is the symbol that keeps `CGraphics::SetModelMatrix` out of the port's link.**
// `Carve802C24AC.cpp` is `Matching` at 100.00% and still unlisted, because it relocates
// against `fn_802C2614` and nothing defined that. This file is what lets both be listed.
#include <dolphin/gx/GXTransform.h>
#include <dolphin/mtx.h>

// Host-side storage for the guest globals these are is `src/Kyoto/Graphics/
// CGraphicsHostGlobals.cpp` for the ones it already had; the four Mtx objects are added
// there, because `CGraphics` has no `.cpp` that could define a C++-named static and a
// `Matching` object is linked into the DOL where only the retail names exist.
extern Mtx lbl_80416F74;
extern Mtx lbl_804172D0;
extern Mtx lbl_80417300;
extern Mtx lbl_80417330;
extern u8 lbl_80418AFD;
extern u8 lbl_80418AFC;

void fn_802C2614(void) {
  if (lbl_80418AFD) {
    PSMTXCopy(lbl_80417330, lbl_804172D0);
  }
  else {
    PSMTXConcat(lbl_80417330, lbl_80416F74, lbl_804172D0);
  }
  GXLoadPosMtxImm(lbl_804172D0, 0);
  if (lbl_80418AFC) {
    PSMTXInvXpose(lbl_804172D0, lbl_80417300);
    GXLoadNrmMtxImm(lbl_80417300, 0);
  }
}
