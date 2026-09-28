/**
 * `.text 0x8027DC1C..0x8027DC28`, 0xC = 12 bytes, three functions, each a bare `blr`:
 *
 * ```
 * 8027DC1C  Update__10CGuiWidgetFf
 * 8027DC20  ProcessUserInput__10CGuiWidgetFRC11CFinalInput
 * 8027DC24  Draw__10CGuiWidgetCFRC19CGuiWidgetDrawParms
 * ```
 *
 * All three are declared in `include/GuiSys/CGuiWidget.hpp` and defined nowhere in the tree, so
 * these are their first bodies: the base class's empty `Update`, `ProcessUserInput` and `Draw`.
 * The base is a real base - `CGuiHeadWidget`, `CGuiModel`, `CGuiLight`, `CGuiCamera` and `CGuiPane`
 * all derive from it - and a widget that does not draw or take input inherits these.
 *
 * `ProcessUserInput` returns `void` here, not the `EMessageReturn` that `CCredits` and
 * `CSlideShow` return for their own: the vtable slot is the same and retail's four bytes are a
 * bare `blr`, so whatever the return register holds is what the base class does.
 *
 * **Source order is descending by address** (`Draw`, `ProcessUserInput`, `Update`) because mwcceppc
 * emits function definitions in *reverse* source order; the file as written emits `Update` at
 * offset 0, `ProcessUserInput` at +4 and `Draw` at +8, which is what retail has. Measured with
 * the unit's own MWCC flags: a 12-byte `.text` and no other section, so no vtable is emitted here.
 *
 * Upstream's `config/G2ME01/splits.txt` left 0x8027DC1C..0x8027DC28 as a gap between
 * `Kyoto/Text/CGuiTextSupport.cpp` (which ends at 0x8027D7B0) and `MetroidPrime/Carve8027E404.c`;
 * this unit fills the second half of that gap, after `MetroidPrime/Carve8027D844.cpp`, and
 * nothing overlaps.
 */
#include "GuiSys/CGuiWidget.hpp"

void CGuiWidget::Draw(const CGuiWidgetDrawParms& parms) const {}

void CGuiWidget::ProcessUserInput(const CFinalInput& input) {}

void CGuiWidget::Update(float dt) {}
