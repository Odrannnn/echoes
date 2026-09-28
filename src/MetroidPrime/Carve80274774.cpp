/**
 * `.text 0x80274774..0x80274784`, 0x10 = 16 bytes, two functions.
 *
 * ```
 * 80274774  fn_80274774                                    li r3,0 ; blr
 * 8027477C  AddWorkerWidget__10CGuiWidgetFP10CGuiWidget   li r3,0 ; blr
 * ```
 *
 * `CGuiWidget::AddWorkerWidget` is declared in `include/GuiSys/CGuiWidget.hpp` and defined
 * nowhere in the tree - upstream left the whole class unwritten - so this is its first body, and
 * it is the base class's "a widget has no workers" answer. `fn_80274774` is a different
 * function that retail leaves unnamed, so `config/G2ME01/symbols.txt` carries the `fn_<addr>`
 * placeholder for it; it stays `extern "C"` because a C++ one would mangle and objdiff would
 * pair nothing.
 *
 * **Source order is descending by address** (`AddWorkerWidget` first, then `fn_80274774`) and that
 * is load-bearing: mwcceppc emits function definitions in *reverse* source order, so the file as
 * written puts `fn_80274774` at offset 0 and `AddWorkerWidget` at +8, which is what retail has.
 * Measured with the unit's own MWCC flags: a 0x10-byte `.text` and no other section.
 *
 * Upstream's `config/G2ME01/splits.txt` left 0x80274774..0x80274784 as a gap between
 * `MetroidPrime/Carve8027409C.c` and `MetroidPrime/Carve80274C1C.c`; this unit fills it and
 * nothing overlaps.
 */
#include "GuiSys/CGuiWidget.hpp"

bool CGuiWidget::AddWorkerWidget(CGuiWidget* worker) { return false; }

extern "C" int fn_80274774() { return 0; }
