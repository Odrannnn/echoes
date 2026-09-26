#ifndef _CPERSISTENTOPTIONSMAP
#define _CPERSISTENTOPTIONSMAP

#include "types.h"

#include "rstl/string.hpp"

#include "MetroidPrime/Player/SPersistentOptionsValue.hpp"

/**
 * The `rstl::map< rstl::string, SPersistentOptionsValue >` at `CPersistentOptions+0x04`, spelled
 * out from `CPersistentOptions`'s own bytes.
 *
 * **These are local shapes, not `rstl::map`, and deliberately so.** `include/rstl/map.hpp` and
 * `include/rstl/red_black_tree.hpp` are another lane's, and the four units that need this header
 * all need the *exact* offsets below, not a container that happens to work. Nothing outside the
 * four units includes this file.
 *
 * Everything here is measured off the three functions that touch it:
 *
 * | offset | read by | what it is |
 * | --- | --- | --- |
 * | `+0x00` | - | four bytes nothing in the chain reads. **The compare at `fn_800273B4` is called with `tree+1` as its `this`**, so the address of the container's byte 1 is part of the ABI as the DOL has it |
 * | `+0x04` | `fn_80146338` `addi r3,4(r3)` , `lwz 4(r31)` | the node count |
 * | `+0x08` | `fn_80146338` `rbtree_rebalance(tree+8, ...)`, `fn_80145B90` `addi r0,r31,8` | the tree header, and **the value an `end()` iterator holds in its second word** |
 * | `+0x0C` | `fn_80146338` `stw r3,12(r31)` | the right-most node |
 * | `+0x10` | `fn_80145BDC` `lwz r31,16(r3)`, `fn_80146338` `stw r3,16(r31)` | the root node |
 *
 * `rstl::string` is **0x10**, not 0x0C - `CHECK_SIZEOF(string, 0x10)` in `rstl/string.hpp`, and
 * `__ct__Q24rstl66basic_string...` at 0x802FF134 copies only the first three words. That fourth
 * word is what puts the map's value at `node+0x20` rather than `node+0x1C`, and it is why a node
 * is 0x2C and not 0x28.
 *
 * The node is 0x2C, measured from `fn_80008CE0` (`li r3,44` at 0x80008CE8, the node constructor
 * `fn_80146338` calls): four tree words, the 16-byte key string, then the 12-byte value.
 */
struct SMapNode {
  SMapNode* x00_left;                 //!< +0x00
  SMapNode* x04_right;                //!< +0x04
  SMapNode* x08_parent;               //!< +0x08
  u32 x0c_colour;                     //!< +0x0C
  rstl::string x10_key;               //!< +0x10 (0x10 bytes: ptr, cow, size, empty allocator)
  SPersistentOptionsValue x20_value;  //!< +0x20
};
CHECK_SIZEOF(SMapNode, 0x2c)

struct SMap {
  u8 x00_unk;              //!< +0x00
  u8 x01_compare_this;     //!< +0x01 - `&x01` is the `this` `fn_800273B4` is called with
  u16 x02_unk;             //!< +0x02
  u32 x04_count;           //!< +0x04
  SMapNode* x08_header;    //!< +0x08 - the tree header, and `end()`'s second word
  SMapNode* x0c_rightmost;
  SMapNode* x10_root;
  u32 x14_unk;
};
CHECK_SIZEOF(SMap, 0x18)

//! The 8-byte iterator: a node, plus the tree's header so `end()` is representable.
struct SMapIter {
  SMapNode* x00_node;
  SMapNode* x04_end;
};
CHECK_SIZEOF(SMapIter, 8)

//! `rstl::pair< iterator, bool >` as `fn_80146338` returns it: by value, through `r3`.
struct SMapInsert {
  SMapIter x0_iter;
  bool x8_inserted;
};
CHECK_SIZEOF(SMapInsert, 0xc)

//! The map's `value_type`. 0x1C bytes, with the value at +0x10 - a hole at +0x0C..+0x0F, which
//! is retail's own shape and is what `fn_80146338`'s node copy reproduces (it copies the entry's
//! +0x10..+0x1B to the node's +0x20 and its +0x00..+0x0B to the node's +0x10).
struct SMapEntry {
  rstl::string key;
  SPersistentOptionsValue value;
  SMapEntry(const rstl::string& k, const SPersistentOptionsValue& v) : key(k), value(v) {}
};
CHECK_SIZEOF(SMapEntry, 0x1c)

//! `fn_800273B4` - retail 0x800273B4, `size:0x28`: `a < b` for two `rstl::string`s. It takes a
//! third argument it never reads and forwards its second and third to `fn_800273DC`, which calls
//! `fn_80021074` and normalises the result with `srwi r3,r3,31`; it is the tree's comparator, and
//! the `this` it is handed is `tree+1`.
extern "C" bool fn_800273B4(const void* self, const rstl::string& a, const rstl::string& b);

#endif // _CPERSISTENTOPTIONSMAP
