/**
 * `CIOWinManager::AddIOWin` and `CIOWinManager::IOWinPQNode::IOWinPQNode`.
 *
 * **`NonMatching`, and it is close.** Retail 0x80049BDC (0x17C = 380 bytes) plus the node
 * constructor at 0x80049D58 (0x2C = 44 bytes) - 424 bytes in one claimed range, declared in
 * descending retail offset because mwcceppc emits definitions in reverse source order.
 *
 * The body is instruction-for-instruction retail's, including the exception-safe `new` shape
 * (`r24 = 0 ; p = operator new(16, ?, 0) ; if (p) { construct ; r24 = 1 } ; if (r24) ReleaseData`),
 * the bottom-tested insertion walk, and the inlined two-word `rc_ptr` copy with its AddRef through
 * the *second* word. Two things are left, and neither is the `rc_ptr` layout:
 *
 *  1. **`new`'s operands.** Retail materialises the file string as
 *     `lis r3,-32710 ; addi r4,r3,26592 ; ... ; addi r4,r4,51` - three instructions, with the
 *     `lis` hoisted - where this compiler emits `lis` + one `addi` against relocations. That is one
 *     extra instruction per allocation site, twice in this function, and it is the same wall
 *     `CGameArchitectureSupport::UnloadAudio` hit. It is a property of mwcceppc's constant
 *     materialisation, not of this source, so no spelling of the source removes it.
 *  2. **Register choice.** Retail keeps the insertion cursor in r26 and the previous node in r27;
 *     ours has them the other way round.
 *
 * See docs/research/rc_ptr.md for the layout work that made this writable at all.
 */

#include "MetroidPrime/CIOWin.hpp"

#include "MetroidPrime/CIOWinManager.hpp"

CIOWinManager::IOWinPQNode::IOWinPQNode(rstl::ncrc_ptr<CIOWin> iowin, int prio, IOWinPQNode* next)
    : mIowin(iowin), mPrio(prio), mNext(next) {}

void CIOWinManager::AddIOWin(rstl::ncrc_ptr<CIOWin> iowin, int pumpPrio, int drawPrio) {
  // Retail walks the *pump* list first, at `this+4`, with the second argument as the key, then the
  // *draw* list at `this+0` with the third. `AddIOWin` is declared
  // `(ncrc_ptr<CIOWin>, int, int)`, and the by-value first parameter arrives as a pointer to an
  // 8-byte caller temporary - which is the layout proof in docs/research/rc_ptr.md.
  IOWinPQNode* cur = mPumpRoot;
  IOWinPQNode* prev = nullptr;
  while (cur != nullptr && cur->mPrio > pumpPrio) {
    prev = cur;
    cur = cur->mNext;
  }
  IOWinPQNode* node = new IOWinPQNode(iowin, pumpPrio, cur);
  if (prev == nullptr) {
    mPumpRoot = node;
  } else {
    prev->mNext = node;
  }

  cur = mDrawRoot;
  prev = nullptr;
  while (cur != nullptr && cur->mPrio > drawPrio) {
    prev = cur;
    cur = cur->mNext;
  }
  node = new IOWinPQNode(iowin, drawPrio, cur);
  if (prev == nullptr) {
    mDrawRoot = node;
  } else {
    prev->mNext = node;
  }
}
