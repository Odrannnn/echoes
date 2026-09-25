/**
 * `CIOWinManager::RemoveAllIOWins`, retail 0x80049A18, 0x80 = 128 bytes.
 *
 * **`NonMatching`, blocked on one instruction.** The body is retail's and the layout is now
 * retail's: two bottom-tested walks, one over `x0_drawRoot` and one over `x4_pumpRoot`, each
 * copy-constructing an `rstl::rc_ptr<CIOWin>` **from the node's own `x0_iowin` member** and
 * passing it to `RemoveIOWin` by reference. That the copy is taken from the node's address rather
 * than from a `rc_ptr*` is itself the proof that the member is at offset 0 of a 16-byte node.
 *
 * What is left is that retail calls `bl fn_80049010` for the copy - an **out-of-line copy
 * constructor** - while this compiler inlines the identical eight instructions. That is 5 extra
 * instructions per loop, 10 in all, and 138 bytes here against retail's 128.
 *
 * Retail's own compiler makes both choices: it *inlines* the same eight instructions in
 * `IOWinPQNode::IOWinPQNode` (0x80049D58), in `fn_80049034` and in four more places, and calls out
 * only here and in `RemoveIOWin`, `fn_80049244`, `fn_8004935C`, `fn_80049764` and `fn_80049884` -
 * all 15 call sites are `CIOWinManager` methods, and `main.elf` has no other caller. So retail's
 * definition was not visible in a header, and it was not a weak template instantiation either
 * (`nm` reports `80049010 T`, a strong global, as it does for
 * `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv`).
 *
 * The way to get it is to move the two words out of the template into a non-template base whose
 * copy constructor is defined in one .cpp - then one out-of-line symbol serves every `T` and the
 * call appears, and `fn_80049010` in symbols.txt is renamed to whatever that symbol is called. It
 * is not done here because mwcceppc 2.7 rejects both spellings of explicit instantiation
 * (`template rc_ptr<T>::rc_ptr(const rc_ptr<T>&);` and `template class rc_ptr<T>;` both give
 * "declaration syntax error"), so the base-class split is the only route and it changes every
 * rc_ptr user's mangled names. See docs/research/rc_ptr.md.
 *
 * It is also the *only* one of the four frame-loop functions this blocks. `AddIOWin`,
 * `PumpMessages` and `CInputGenerator::Update` never call 0x80049010; they inline the copy.
 */

#include "MetroidPrime/CIOWin.hpp"

#include "MetroidPrime/CIOWinManager.hpp"

void CIOWinManager::RemoveAllIOWins() {
  while (x0_drawRoot) {
    rstl::rc_ptr<CIOWin> win(x0_drawRoot->x0_iowin);
    RemoveIOWin(win);
  }
  while (x4_pumpRoot) {
    rstl::rc_ptr<CIOWin> win(x4_pumpRoot->x0_iowin);
    RemoveIOWin(win);
  }
}
