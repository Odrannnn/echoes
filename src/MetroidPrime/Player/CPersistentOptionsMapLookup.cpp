/**
 * `fn_80145B90` and `fn_80145BDC` - retail 0x80145B90 (`size:0x4C`) and 0x80145BDC (`size:0xBC`),
 * the contiguous pair 0x80145B90..0x80145C98. One unit because they are contiguous in retail and
 * `fn_80145B90` is a three-instruction wrapper whose only callee is `fn_80145BDC`.
 *
 * ## STATUS: `NonMatching`. Both functions are **one instruction out of a hundred**, and both
 * differences are instruction *scheduling*, not logic. This is a measured blocker, not a guess.
 *
 * | function | objdiff | what differs |
 * | --- | --- | --- |
 * | `fn_80145B90` | 99.05% (1 of 16 instructions in the epilogue) | retail restores LR first - `lwz r0,20(r1) ; lwz r31,12(r1) ; lwz r30,8(r1)` - and mwcceppc restores it last. The **body is byte-identical**, including the `stw` order. |
 * | `fn_80145BDC` | 95.74% (1 of 47 instructions in the prologue) | retail loads the root where the local is created - `stw r31,28(r1) ; lwz r31,16(r3) ; stw r30,24(r1)` - and mwcceppc sinks the load to the end of the prologue, after `mr r28,r3`. Everything from `b` into the loop to the `blr` is instruction-for-instruction identical, epilogue included. |
 *
 * Sixty-one body variants were ranked with `tools/try_batch.py` (the sweep is in this lane's
 * report): eight loop shapes, six spellings of the closing test, ten spellings of the compare's
 * `this` argument, eight spellings of the two local initialisers, `volatile` in five positions.
 * **The compare's `this` is the one that mattered and it is not scheduling** - see below - and
 * nothing moved either of the two remaining differences off zero. Retrying this needs a
 * different idea, not another spelling.
 *
 * **The two shapes that are load-bearing, both measured:**
 *
 *  * **The compare's `this` must be a member address, not pointer arithmetic.** Passing
 *    `reinterpret_cast<const u8*>(tree) + 1` gets the address hoisted out of the loop into a
 *    callee-saved register - 28 differing instructions, and the whole allocation shifts
 *    (r28=key, r30=tree+1 instead of r28=tree, r30=best). Passing `&tree->x01_compare_this`,
 *    with the map's `+0x01` named as the `u8` it is, keeps MWCC recomputing it at each call site
 *    and lands at 2. 28 -> 2 is the single largest step in this file, and the map is declared
 *    byte-wise (`u8`/`u8`/`u16`) in `CPersistentOptionsMap.hpp` for that reason.
 *  * **The loop's condition is written inverted** - `if (!cmp) { best; left } else { right }` -
 *    and that is 5 differing instructions against 6 for the direct spelling, because retail
 *    branches `bne` to the *right* arm.
 *
 * ## `fn_80145B90` - the search, plus the second word of the iterator
 *
 *     0x80145BA0  mr   r31,r4
 *     0x80145BA4  mr   r4,r5        the key moves up to make room for the call
 *     0x80145BB0  mr   r3,r31
 *     0x80145BB4  bl   80145bdc
 *     0x80145BB8  stw  r3,0(r30)    the node
 *     0x80145BBC  addi r0,r31,8
 *     0x80145BC0  stw  r0,4(r30)    and &tree->x08
 *
 * Both stores are *after* the call, which is why the body is two assignments and not a
 * constructor with the search inlined into an initialiser list. The epilogue's restore order
 * matches the prologue's save order, which is what makes the LR load's position a scheduling
 * question rather than a layout one.
 *
 * ## `fn_80145BDC` - an exact-key lookup, returning 0 rather than `end()`
 *
 * The descent is a `lower_bound`'s: go **right** while the node's key is less than the search key,
 * otherwise remember the node and go left. The loop's compare is
 * `fn_800273B4(tree+1, node->x10_key, key)` - "the node's key is less than the search key".
 *
 * **The closing test has its operands the other way round, and reading it as `a < b` is what
 * makes the function a `find` and not a no-op.** It is
 * `fn_800273B4(tree+1, key, best->x10_key)` at 0x80145C48/0x80145C50 - the *search* key first -
 * against `node->x10_key` first in the loop at 0x80145C0C/0x80145C14. And the flag is built as
 *
 *     0x80145C3C  cmplwi r30,0
 *     0x80145C40  li      r31,0
 *     0x80145C44  beq     80145c60      !best  ->  r31 = 1
 *     0x80145C54  bl      80146273b4
 *     0x80145C5C  beq     80145c64      cmp == 0 -> keep r31 = 0
 *     0x80145C60  li      r31,1
 *     0x80145C68  beq     80145c74      r31 == 0 -> return best
 *     0x80145C6C  li      r3,0
 *     0x80145C74  mr      r3,r30
 *
 * i.e. **`absent = !best || cmp(key, best->key)`**, and the function returns `absent ? 0 : best`.
 * The loop leaves `best` on a node with `key <= best->key`, so the return is the node whose key
 * is **equal** to the search key - which is exactly what `fn_80145ACC` wants, because it is a
 * "set if absent". Measured: `!best || cmp(...)` is 2 differing instructions and
 * `best ? cmp(...) : true` is 7, because only the first spelling produces the `li r31,0` /
 * `li r31,1` pair at 0x80145C40/0x80145C60.
 *
 * ## The register assignment, which the source controls
 *
 * Retail spills the cursor to **r31** and the best-so-far to **r30**, which is the *opposite* of
 * the order the two locals are needed in, and MWCC hands out r31, r30, r29 in declaration order
 * (`rstl/algorithm.hpp`'s `lower_bound` notes the same). So the cursor is declared first even
 * though `best` has to be initialised first - which is also the order retail's own code does them
 * in (`lwz r31,16(r3)` at 0x80145BEC, then `li r30,0` at 0x80145BF4). Declaring `best` first
 * moves both and costs 17 instructions. The parameters then land in r29 (key) and r28 (tree),
 * which is what the loop's `addi r3,r28,1` needs.
 */

#define _CMEMORY

#include "types.h"

#include "MetroidPrime/Player/CPersistentOptionsMap.hpp"

extern "C" {
// The exact-key lookup, spelled as described in the header.
SMapNode* fn_80145BDC(SMap* tree, const rstl::string& key);
} // extern "C"

extern "C" SMapNode* fn_80145BDC(SMap* tree, const rstl::string& key) {
  SMapNode* node = tree->x10_root;
  SMapNode* best = 0;
  while (node) {
    if (!fn_800273B4(&tree->x01_compare_this, node->x10_key, key)) {
      best = node;
      node = node->x00_left;
    } else {
      node = node->x04_right;
    }
  }
  bool absent = !best || fn_800273B4(&tree->x01_compare_this, key, best->x10_key);
  return absent ? 0 : best;
}

// Retail 0x80145B90: the search, * the second word of the iterator. The declaration sits
// directly above its definition and inside its own `extern "C"` block, so that
// `tools/try_batch.py` - which takes the first line matching the name and then the next `{` after
// it - finds *this* function's body and not `fn_80145BDC`'s, which is the other way about.
extern "C" {
void fn_80145B90(SMapIter* out, SMap* tree, const rstl::string& key);
} // extern "C"

extern "C" void fn_80145B90(SMapIter* out, SMap* tree, const rstl::string& key) {
  out->x00_node = fn_80145BDC(tree, key);
  out->x04_end = reinterpret_cast< SMapNode* >(&tree->x08_header);
}
