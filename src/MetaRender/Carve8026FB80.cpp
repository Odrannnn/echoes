// `CCubeRenderer::EndScene`, carved out of dtk's `auto_03_8026F628_text` range. Retail .text
// 0x8026FB80..0x8026FBFC, 0x7C = 124 bytes, one function: `EndScene__13CCubeRendererFv`.
//
// **This is the other half of the frame's begin/end pair, 0x7C bytes before `BeginScene`**
// (`src/MetaRender/Carve8026FBFC.cpp`, 0x8026FBFC, slot 35). `BeginScene` is slot 35 and this is
// slot 36, and both are reached from the same dispatcher. **Nothing is drawn here and nothing
// here can be**: retail's body is four stores, a bit-field write, two calls and a branch. The
// claim that landing it "renders a frame" would be false - the draw methods and the model data
// are still retail's, and this file is not a rendering.
//
// The retail length is measured, not assumed: the body runs from the `stwu` at 0x8026FB80 to the
// `blr` at 0x8026FBF8 inclusive, so 0x8026FBF8 + 4 = 0x8026FBFC, which is exactly where
// `BeginScene` starts. `config/G2ME01/symbols.txt` says `size:0x7C` and the disassembly agrees.
// The claim therefore **abuts** `Carve8026FBFC.cpp` and does not overlap it; the two units are
// adjacent, not nested, which is the only arrangement a `splits.txt` claim can take here.
//
// ## What retail's 31 instructions are
//
// ```
//   stwu r1,-32(r1) ; mflr r0 ; stw r0,36(r1) ; stw r31,28(r1)   frame, this in r31
//   mr   r31,r3                                                    r31 = this
//   lbz  r4,-29340(r13)                       .sdata2 0x8041B124    the frame's filter flag
//   lbz  r0,1364(r3)                         this+0x554           the eight-bit flag byte
//   cntlzw r3,r4
//   rlwimi r0,r3,28,30,30                                        x554_30_ = (flag == 0)
//   stb  r0,1364(r31)
//   bl   fn_802C1658                          0x5BC               `CGraphics::EndScene()`
//   lwz  r3,840(r31)                          this+0x348           x348_
//   cmpwi r3,2 ; blt +0x50
//   lwz  r3,288(r31) ; li r4,1 ; bl __dt__8CTextureFv ; stw 0     x120_ = nullptr
//   addi r0,r3,1 ; stw r0,840(r31)                                ++x348_
//   li r0,0 ; addi r3,r1,8 ; stw r0,8(r1) ; bl fn_802C1F5C         SetClearColor(CColor(0))
// ```
//
// **The `rlwimi` shift 28 is not a mistake and must not be "corrected".** The mask field is 30,30
// and the value is taken from **bit 26** of the source, which is bit 5 - not bit 0. That is
// because the source is a `cntlzw` result, and `cntlzw` returns **32** (not 1) for a zero
// argument, so MWCC's boolean is bit 5 of the register and the shift is chosen to move bit 5
// onto mask field 30. This is the `rlwimi` trap recorded in
// `docs/RUNNING_THE_DECOMP.md` ("MWCC 2.7's bit-field granularity, and the `rlwimi` trap"):
// **a 1-bit field's shift is `31 - p` only when the source is a `li 0/1`, and it is not when the
// source is a computed value.** Writing the comparison the natural way reproduces it; do not
// hand-tune the shift.
//
// **`fn_802C1658` is `CGraphics::EndScene` and is reached here by dtk's own name, not a mangled
// one.** `CGraphics` has no `.cpp` anywhere in this tree, so a call to `CGraphics::EndScene()`
// mangles to `_ZN9CGraphics9EndSceneEv`, which nothing defines and which is not in the DOL's
// symbol table - see `src/Kyoto/Graphics/Carve802BEC1C.cpp` for the same constraint on a data
// member. It is the mirror of `fn_802C1E60`, the one-call scene *begin* forwarder that
// `Carve8026FBFC.cpp` ends with, and `RsMain` (0x800063F8) calls that pair in this order, so the
// identification is by call site and by the `GXCopyDisp` / `GXSetCopyFilter` tail at
// 0x802C1B68, not by guesswork.
//
// **The flag is `lbl_80418AE4`, in `.sdata`, and the non-`const` declaration is load-bearing.**
// `r13` here is **`_SDA_BASE_` = 0x8041FD80, not `_SDA2_BASE_`**, and that was measured rather
// than assumed: `fn_802C1F5C`, three instructions into this very function, does
// `lwz r4,-29324(r13)`, and the same `-29313` displacement is
// `CGraphics::GetUseVideoFilter`'s `lbl_80418AFF` (see `src/Kyoto/Graphics/Carve802BEC1C.cpp`),
// which lives in `.sdata`. Against the SDA2 base the same displacement would give 0x8041B111,
// which is inside the `.sdata2` float constant pool and has no byte writer anywhere. So
// `-29340` is `0x80418AE4` = `lbl_80418AE4`, `size:0x1 data:byte`, and
// `symbols.txt` describes it exactly.
//
// **It must be declared non-`const`, and that is the same rule as
// `CGameStateSlotDefaults.cpp`**: a retail `.sdata` object's address needs a non-`const`
// declaration, because `const` puts the object in the read-only small-data area and mwcceppc
// then emits `lis`+`addi` with `R_PPC_ADDR16_HA`/`LO` where retail has a single
// `R_PPC_EMB_SDA21`. The symbol is also **written** - `fn_802BE8E8` (0x802BE8E8) is an 8-byte
// `stb r3,-29340(r13); blr` setter called with `li r3,1` from `RsMain` at 0x800063F8, between
// `CGraphics::BeginScene` and `CGraphics::EndScene` - so it is mutable and a `const` object
// would be a lie as well as a codegen error.
//
// `x348_` is the header's own `int x348_` (constructor 0x802713EC stores 2), and the branch is
// `cmpwi r3,2 ; blt`, so the delete-and-null happens at **2 or above** and the increment below it.
// That is the opposite of the reading "count down to zero" that the BeginScene member
// `x4f4_phazonSuitMaskCountdown` invites, and it is what the two instructions say.

#include "MetaRender/CCubeRenderer.hpp"

#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Graphics/CTexture.hpp"

// `CGraphics` has no `.cpp` in this tree; the DOL names these two by dtk's labels only.
extern "C" {
void fn_802C1658();
void fn_802C1F5C(const CColor& color);
}

// The frame's per-scene filter flag, `.sdata` 0x80418AE4. See the header note for why r13 is
// `_SDA_BASE_` here and why the declaration is deliberately not `const`.
extern "C" uchar lbl_80418AE4;

void CCubeRenderer::EndScene() {
  // 0x8026FB94-0x8026FBA4. `cntlzw` + `rlwimi` is MWCC's spelling of this comparison; the
  // shift lands MWCC's bit-5 boolean on mask field 30, which is `x554_30_`.
  x554_30_ = (lbl_80418AE4 == 0);

  // 0x8026FBA8. The EFB copy that closes the scene, and the reason a frame needs closing.
  fn_802C1658();

  // 0x8026FBAC-0x8026FBD4. Retail branches with **`blt` to 0x8026FBD0**, which is the
  // *increment*, so the delete-and-null is the fall-through and the condition is the
  // **positive** `>=`: written the other way round, MWCC emits `bge` over the delete block and
  // permutes the two blocks. `x120_ = nullptr` is one statement, not two:
  // `rstl::single_ptr::operator=` is `delete x0_ptr; x0_ptr = ptr;`, which is exactly the
  // `lwz / li r4,1 / bl dtor / stw 0` at 0x8026FBB8-0x8026FBC8.
  if (x348_ >= 2) {
    x120_ = nullptr;
  } else {
    ++x348_;
  }

  // 0x8026FBD8-0x8026FBE4. A single `stw 0`, not four `stb` - so the `uint` spelling, which is
  // why this is not the same expression `Carve8026FBFC.cpp` uses for its clear colour.
  fn_802C1F5C(CColor(static_cast< uint >(0)));
}
