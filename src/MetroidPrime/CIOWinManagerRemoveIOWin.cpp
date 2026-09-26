/**
 * `CIOWinManager::RemoveIOWin`, retail 0x80049A98, 0x144 = 324 bytes.
 *
 * **99.63% fuzzy, `NonMatching`, on 14 instructions - all one register choice.** The second
 * walk below is retail's byte for byte, register for register; the first walk's `prev` and its
 * comparison result hold r29/r27 where retail holds r27/r29, and `cur` is r28 in both. About
 * seventy spellings of the declaration order, the delete, the null tests and the comparison
 * were measured (tools/try_batch.py's metric, reproduced in the lane notes) and 14 is the
 * floor. **This is the same wall `CIOWinManagerAddIOWin.cpp` documents in the same class** -
 * "retail keeps the insertion cursor in r26 and the previous node in r27; ours has them the
 * other way round" - so it is a property of mwcceppc's allocator here, not of the source.
 *
 * ## What it does
 *
 * The same two-list walk as `RemoveAllIOWins` (0x80049A18), with a match test and an unlink,
 * and over the **pump** list (`x4_pumpRoot`; retail reads `4(this)`) first and the **draw** list
 * second - the opposite order from the header's member order, and the reason the first walk's
 * cursor is `4(this)` while the second's is `0(this)`. Each iteration copy-constructs an
 * `rstl::rc_ptr<CIOWin>` **out of line** from the node's own `x0_iowin`
 * (`bl CopyInto__Q24rstl10CRcPtrDataFPQ24rstl10CRcPtrDataRCQ24rstl10CRcPtrData`), compares its
 * first word against `*chIow` with `subf`/`cntlzw`/`srwi ,5`, releases the copy, and on a match
 * unlinks the node, releases the node's own copy and frees it.
 *
 * ## The three things in the source that are load-bearing
 *
 * 1. **The inner `bool same;` block.** `win` and the result live in a block that ends *before*
 *    the `if`, so `win`'s destructor is emitted once, at the end of that block and therefore
 *    ahead of the branch - which is retail's `addi r3,r1,16 ; bl ReleaseData ; clrlwi. ; beq`
 *    at 0x80049ac8-0x80049ae8. Spell it as one body-scope local instead and mwcceppc emits the
 *    release on *both* paths: measured 332 bytes, 105 differing instructions.
 * 2. **`~IOWinPQNode()` declared and defined here, marked `inline`.** Retail's unlink is
 *    `cmplwi r28,0 ; beq ; beq ; beq ; mr r3,r28 ; bl ReleaseData ; mr r3,r28 ; bl Free`, and
 *    the two dead `beq`s are mwcceppc's own: they are what it emits in front of an implicit
 *    member destructor, and they appear in the out-of-line destructor it generates
 *    (`mr. r30,r3 ; beq ; beq ; bl ReleaseData`). The body is empty because the whole of the
 *    destructor is `x0_iowin.~rc_ptr()`. Marking the out-of-class definition `inline` is what
 *    makes mwcceppc expand it; without that word it emits a strong out-of-line
 *    `__dt__Q213CIOWinManager11IOWinPQNodeFv` and calls it, and the unit is 308 bytes.
 * 3. **The argument's first word is read through a `volatile` lvalue.** Retail reloads
 *    `0(r31)` on every iteration and keeps only the *address* of the argument in a register.
 *    Given a plain `chIow.x0_ptr` mwcceppc hoists the loop-invariant load into the prologue,
 *    which costs a sixth callee-saved register, and the whole allocation flips: `this` moves
 *    off r30 and the prologue stops being `stmw r27,28 ; mr r30,r3 ; mr r31,r4`. Measured 76
 *    differing instructions against 14. A `volatile` read of a `const` object is
 *    observationally identical to a plain one; it is spelled this way because it is the only
 *    thing measured that stops the hoist.
 *
 * ## Why the name is `RemoveIOWin` and not `fn_80049A98`
 *
 * `config/G2ME01/symbols.txt` now carries MWCC's mangling for this address,
 * `RemoveIOWin__13CIOWinManagerFRCQ24rstl15rc_ptr<6CIOWin>`, in place of the `fn_80049A98`
 * placeholder. That is what let **`CIOWinManagerRemoveAllIOWins.cpp` become `Matching`**
 * (`flip_test` PASS): its own call site emits the *mangled* name, and the DOL's link was
 * missing that symbol, not missing the range - the range had been byte-exact all along. With
 * the placeholder still in `symbols.txt` objdiff pairs nothing here and the unit scores 0/0
 * (measured), so the rename is what makes this unit measurable at all.
 *
 * `rstl::rc_ptr<CIOWin>`'s `ReleaseData` and its own destructor are instantiated here as COMDAT
 * weak copies, 180 bytes that the link discards; `CIOWinManagerRemoveAllIOWins.cpp` has the
 * same pair, so this is the project's existing state and not something this unit adds.
 */

#include "MetroidPrime/CIOWin.hpp"

#include "MetroidPrime/CIOWinManager.hpp"

// Above `RemoveIOWin` in the file on purpose: mwcceppc emits function definitions in reverse
// source order, and `RemoveIOWin` has to be the first thing in the object's `.text` to sit at
// 0x80049A98. The body is empty because the whole of the destructor is the implicit
// `x0_iowin.~rc_ptr()`, which is the `bl ReleaseData` in the unlink below. The `inline` is what
// makes mwcceppc expand it into the two dead `beq`s retail has; see point 2 in the header.
inline CIOWinManager::IOWinPQNode::~IOWinPQNode() {}

void CIOWinManager::RemoveIOWin(const rstl::rc_ptr<CIOWin>& chIow) {
  IOWinPQNode* prev = nullptr;
  IOWinPQNode* cur = x4_pumpRoot;
  while (cur != nullptr) {
    bool same;
    {
      rstl::rc_ptr<CIOWin> win(rstl::CRcPtrData::OutOfLine(), cur->x0_iowin);
      // The `volatile` lvalue is deliberate; see point 3 in the header.
      same = win.x0_ptr == *(void* const volatile*)(&chIow);
    }
    if (same) {
      if (prev == nullptr) {
        x4_pumpRoot = cur->xc_next;
      } else {
        prev->xc_next = cur->xc_next;
      }
      if (cur != nullptr) {
        cur->~IOWinPQNode();
        ::operator delete(cur);
      }
      break;
    }
    prev = cur;
    cur = cur->xc_next;
  }

  IOWinPQNode* draw = x0_drawRoot;
  IOWinPQNode* prev2 = nullptr;
  while (draw != nullptr) {
    bool same2;
    {
      rstl::rc_ptr<CIOWin> win(rstl::CRcPtrData::OutOfLine(), draw->x0_iowin);
      same2 = win.x0_ptr == *(void* const volatile*)(&chIow);
    }
    if (same2) {
      if (prev2 == nullptr) {
        x0_drawRoot = draw->xc_next;
      } else {
        prev2->xc_next = draw->xc_next;
      }
      if (draw != nullptr) {
        draw->~IOWinPQNode();
        ::operator delete(draw);
      }
      break;
    }
    prev2 = draw;
    draw = draw->xc_next;
  }
}
