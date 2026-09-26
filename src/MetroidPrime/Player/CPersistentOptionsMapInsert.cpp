/**
 * `fn_80145ACC` - retail 0x80145ACC, `size:0xC4` = 196 bytes, 0x80145ACC..0x80145B90: the
 * option map's **set-if-absent**, and the one function of the six on the port's critical path
 * that `fn_80145C98` cannot be linked without.
 *
 *     0x80145ACC  stwu r1,-80(r1)    LR at 84(r1); r31/r30/r29 at 76/72/68
 *     0x80145ADC  mr   r31,r5        the value, by reference
 *     0x80145AE4  mr   r30,r4        the name
 *     0x80145AF0  mr   r29,r3        `self`
 *     0x80145AF4  addi r3,r1,16 ; addi r4,r29,4
 *     0x80145AFC  bl   80145b90      the find, into the local at 16(r1)
 *     0x80145B0C  stw  r3,8(r1)      0
 *     0x80145B18  stw  r0,12(r1)     self+12
 *     0x80145B2C  clrlwi. r0,r3,24 ; beq -+0x2C
 *     0x80145B34  mr   r4,r30 ; addi r3,r1,36
 *     0x80145B3C  bl   802ff134     the key's copy constructor
 *     0x80145B58  stw  r7,52(r1) ; stw r5,56(r1) ; stw r0,60(r1)   the value, copied
 *     0x80145B64  lwz  r5,20(r29)   the map's root
 *     0x80145B68  bl   80146338     the node insert
 *     0x80145B6C  addi r3,r1,36
 *     0x80145B70  bl   802fe9b8     ~the key
 *
 * ## The test is `it == end()`, and `end()` is a stored value
 *
 * `fn_80145B90` returns an 8-byte `{node, &map->x08}`, and the two comparisons after it are
 * `node == 0` **and** `end == self+12` - i.e. `self+4+8`, the map's header word, which is what
 * `fn_80145B90` stores in the iterator's second field. So the source keeps an `end()` as an
 * ordinary 8-byte local at 8(r1), writes it **after** the search (both `stw`s are at 0x80145B0C
 * and 0x80145B18, past the `bl`), and compares the two locals field by field. That is why the
 * body cannot be `if (it.x00_node == 0)`: the second half of the comparison is not implied.
 *
 * ## The 28-byte local at 36(r1) is the map's `value_type`, and the value is at +0x10
 *
 * `rstl::string` is **0x10** (`CHECK_SIZEOF(string, 0x10)`), not 0x0C, so
 * `pair<string, SPersistentOptionsValue>` is 0x1C with the value at +0x10 - and the three
 * `stw`s at 0x80145B58/5C/60 write 52(r1) = 36(r1)+16, which is the value and not the next
 * field. `fn_80146338` then copies that same entry twice into the node: the key to node+0x10
 * and the value to node+0x20, which is why `fn_80008CE0` (its node constructor) copies
 * `entry+0x10..0x1B` to `node+0x20` *and* constructs a string at `node+0x10` from `entry+0x00`.
 *
 * The key is built by its **copy constructor** (`bl 802ff134`), not assigned, and the entry is a
 * local whose scope ends at the closing brace - which is the `bl 802fe9b8`
 * (`internal_dereference`, i.e. `~rstl::string`) at 0x80145B70, the last thing the function does
 * on that path.
 *
 * ## Status: written, and it is the one of the four that is not `Matching`
 *
 * It cannot be a `Matching` unit, and the reason is not in this file: its only callee that lives
 * in source, `fn_80145B90`, is in `CPersistentOptionsMapLookup.cpp`, which is **`NonMatching` at
 * 99.05% / 95.74% - one instruction out on each function, both a pure scheduling difference**
 * (see that file's header). A `Matching` unit may not reference a symbol no linked object
 * defines, so the whole map front-end is parked until that unit lands. The four pieces are
 * therefore: this function, `fn_80145B90`, `fn_80145BDC` and `fn_80146338` - **three written, the
 * rbtree insert not attempted.**
 */

#define _CMEMORY

#include "types.h"

#include "MetroidPrime/Player/CPersistentOptions.hpp"
#include "MetroidPrime/Player/CPersistentOptionsMap.hpp"

#include "rstl/string.hpp"

#if !defined(__MWERKS__)
#include <new>
#endif

extern "C" {
// The find. `fn_80145B90(&it, self+4, name)` - the map is the class's second word.
void fn_80145B90(SMapIter* out, SMap* tree, const rstl::string& name);
} // extern "C"

// Retail 0x80145B90's partner: the node insert, `fn_80146338`, `size:0x1B8`. It is a relocation
// here, and it is retail's own bytes - nothing in this unit spells it.
extern "C" void fn_80146338(SMapInsert* out, SMap* tree, SMapNode* root, const SMapEntry* entry);

// Declared above its own definition, and inside its own `extern "C"` block, so that
// `tools/try_batch.py` - which matches the first line carrying the name and then takes the next
// `{` after it - finds this body and not `fn_80146338`'s.
extern "C" {
void fn_80145ACC(CPersistentOptions* self, const rstl::string& name,
                 const SPersistentOptionsValue& value);
} // extern "C"

extern "C" void fn_80145ACC(CPersistentOptions* self, const rstl::string& name,
                            const SPersistentOptionsValue& value) {
  SMap* map = reinterpret_cast< SMap* >(reinterpret_cast< u8* >(self) + 4);

  SMapIter it;
  fn_80145B90(&it, map, name);

  SMapIter end;
  end.x00_node = 0;
  end.x04_end = reinterpret_cast< SMapNode* >(reinterpret_cast< u8* >(self) + 12);

  if (it.x00_node == end.x00_node && it.x04_end == end.x04_end) {
    SMapEntry entry(name, value);
    SMapInsert result;
    fn_80146338(&result, map, map->x10_root, &entry);
  }
}
