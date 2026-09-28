/**
 * `.text 0x8027D844..0x8027D84C`, 0x8 = 8 bytes, two functions, each a bare `blr`:
 *
 * ```
 * 8027D844  OnActivate__10CGuiWidgetFv
 * 8027D848  OnVisible__10CGuiWidgetFv
 * ```
 *
 * Both are declared in `include/GuiSys/CGuiWidget.hpp` and defined nowhere in the tree, so these
 * are their first bodies: the base class's empty `OnActivate`/`OnVisible`, which every widget that
 * does not override them inherits.
 *
 * **Source order is descending by address** (`OnVisible` first) because mwcceppc emits function
 * definitions in *reverse* source order; the file as written emits `OnActivate` at offset 0 and
 * `OnVisible` at +4, which is what retail has. Measured with the unit's own MWCC flags: an 8-byte
 * `.text` and no other section, so the class's vtable is not emitted here and the claim stays
 * `.text`-only.
 *
 * Upstream's `config/G2ME01/splits.txt` left 0x8027D844..0x8027D84C as a gap between
 * `Kyoto/Text/CGuiTextSupport.cpp` (which ends at 0x8027D7B0) and `MetroidPrime/Carve8027E404.c`;
 * this unit fills the first half of that gap and nothing overlaps.
 */
#include "GuiSys/CGuiWidget.hpp"

void CGuiWidget::OnVisible() {}

void CGuiWidget::OnActivate() {}
