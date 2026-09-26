// `CCubeRenderer::~CCubeRenderer`, carved out of dtk's `auto_03_8026FE10_text` range. Retail .text
// 0x80270848..0x80270A64, 0x21C = 540 bytes, one function: `__dt__13CCubeRendererFv`.
//
// ## Why this unit exists: it is the key function
//
// `~CCubeRenderer` is the first virtual `CCubeRenderer` declares that is neither pure nor inline,
// so **the translation unit that defines it is the one that emits `__vt__13CCubeRenderer`** -
// under mwcceppc, and under the host's Itanium ABI as `vtable for CCubeRenderer`. Before this
// file nothing defined it, so no compiler anywhere emitted the table, and the constructor's
// vptr stores had nothing to point at: `src/MetaRender/Carve80271238.cpp` used to define the
// three tables as arrays of zeros under `TARGET_PC`, which is what made every host `gpRender->`
// virtual a jump to address 0. A vtable the linker can see is one that stops being asked for.
//
// ## The body is the inventory
//
// Retail's destructor is also the most complete statement of the class's members, because it
// destroys each non-trivial one explicitly. In source order, which is the reverse of the
// member order the implicit destructions run in:
//
//   0x8027086C-0x8027087C  both vptrs rewritten to `__vt__13CCubeRenderer` and `+0x140`
//   0x80270880             `lbl_80419748 = 0` - the live-renderer word the constructor set
//   0x80270884             `fn_80272624()` - the same four-word clear the constructor ends with
//   0x80270888-0x80270894  `if (x4f8) fn_802C420C(x4f8)` - the explicit body statement
//   0x80270898-0x802708A8  x550 `single_ptr<CGraphicsPalette>`: `fn_802C3F50(p, 1)`
//   0x802708AC-0x8027096C  seven `TLockedToken`s, each two nested inline null tests + `~CToken`
//   0x80270970-0x80270980  x4F8 `single_ptr<CTexture>`: `~CTexture(p, 1)`
//   0x80270984             x360 vector: `fn_80038F0C(this + 0x360, -1)`
//   0x80270990             x330 list: `fn_802738F0(this + 0x330, -1)`
//   0x8027099C-0x802709D4  x2C4, x25C, x1F4, x18C, x124 `CTexture`s
//   0x802709D8-0x802709E8  x120 `single_ptr<CTexture>`
//   0x802709EC             xB8 `CTexture`
//   0x802709F8             x1C list: `fn_80273770(this + 0x1C, -1)`
//   0x80270A04             x10 `CFont`: `fn_802BAD30(this + 0x10, -1)`
//   0x80270A10-0x80270A34  `IWeaponRenderer` then `IRenderer` vptrs restored (`lbl_803B8B70`,
//                          `lbl_803B0C1C`), each behind the base-conversion null test
//   0x80270A38-0x80270A44  `if (flag > 0) fn_80272988(this)` - the deleting path
//
// ## `NonMatching`, and the measured reasons
//
// Four relocations name symbols this tree spells differently, and each is a rename rather
// than a code change: `fn_802C3F50` is `CGraphicsPalette::~CGraphicsPalette` (it tests +0x4
// against `sCurrentFrameCount` and frees the `single_ptr` at +0xC); `fn_80038F0C`,
// `fn_802738F0` and `fn_80273770` are the three container destructors, out of line in retail
// and weak instantiations here; `fn_802BAD30` is `CFont::~CFont`, and
// `src/MetroidPrime/CConsoleOutputWindowCtor.cpp` calls its constructor by the placeholder name
// `fn_802BAD6C`, so renaming either is that file's change as well. And the deleting path calls
// `fn_80272988`, which only decrements a counter (`fn_802729C0()`'s word) and never frees -
// a class-specific `operator delete` paired with `fn_80272958`, the allocator `AllocateRenderer`
// calls. Neither is declared here, because naming them is a claim about two more functions.

#include "MetaRender/CCubeRenderer.hpp"

extern "C" {
/** Clears four `.sbss` words; the constructor's last call. */
void fn_80272624();
/** The `CTexture` release `BeginScene` also calls before freeing +0x4F8. */
void fn_802C420C(CTexture* tex);
/** `.sbss:0x80419748`, the live renderer. Defined beside the constructor. */
extern CCubeRenderer* lbl_80419748;
}

CCubeRenderer::~CCubeRenderer() {
  lbl_80419748 = nullptr;
  fn_80272624();
  if (x4f8_phazonSuitMask.get() != nullptr) {
    fn_802C420C(x4f8_phazonSuitMask.get());
  }
}
