# `rstl/rstl_map.cpp` — `Matching`, and how the last 0.08% went

Measured 2026-09-26 in a lane worktree. The unit claims
`.text 0x802FD3CC..0x802FDAB8` (0x7CC = 1996 bytes) and holds five functions:

| retail | size | before | after |
|---|---|---|---|
| `rstl::rbtree_traverse_forward` | 0x9C | 100.00% | 100.00% |
| `rstl::rbtree_rebalance_for_erase` | 0x41C (1052) | **99.92%** | **100.00%** |
| `rstl::rbtree_rebalance` | 0x174 | 100.00% | 100.00% |
| `rstl::rbtree_rotate_right` | 0x60 | 100.00% | 100.00% |
| `rstl::rbtree_rotate_left` | 0x60 | 100.00% | 100.00% |

`flip_test.sh rstl/rstl_map.cpp` → `PASS -> kept as Matching`; DOL sha1 unchanged
(`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`).

## The mismatch, and why the inherited diagnosis was wrong

Four instructions differed, all of them one value in the wrong register. At retail +0x24:

```
  bc:  mr    r28,r3          ; the header parameter, saved
  c0:  lwz   r3,0(r4)        ; node->mLeft
  c4:  cmplwi r3,0
  ...
  e0:  mr    r30,r3          ; replacement = node->mLeft
  ...
 10c:  stw   r31,8(r3)       ; node->mLeft->mParent = successor
```

and ours had `r5` in all four places. The previous lane's note says "r3 is a dead *first
parameter*". It is not dead: `mr r28,r3` at +0x20 keeps it, and `lwz r0,8(r28)` at +0xBC reads
`header->mRootNode` back out of it. r3 is the **live** header parameter, and it is r3's *register
slot* that is free, because the allocator moved the value to r28 immediately.

## What actually fixed it

The source had

```cpp
fake_header* header = static_cast< fake_header* >(header_void);
```

That local is a temporary. `static_cast` produces one, it is created first (it is declared first),
and it claims the first free register — r3. `node->get_left()`, the next temporary, is then pushed
to r5 by the rule already in `FACTS.md` ("mwcceppc holds r3 … the first temporary after it dies is
pushed to r5").

**Delete the local and cast `header` at each point of use.** The cast is then created at the point
of use, after `node->get_left()`, and `node->get_left()` gets r3. Byte-exact.

```cpp
void* rbtree_rebalance_for_erase(void* header, void* node_void) {
  ...
  if (static_cast< fake_header* >(header)->get_root() == node) {
    static_cast< fake_header* >(header)->set_root(successor);
  } else { ... }
  ...
}
```

A local that is *declared but never used* also matches, so the mechanism is confirmed to be the
use, not the declaration.

## What was tried and did not help (all 4 differing instructions, i.e. no change)

Both parameter orders in the signature; naming the value `left`; naming it `leftmost_child`;
`const` on the `header` local; initialising `header` late; no `header` local but the casts written
as `header_of(header)` through a free inline; the `header` local declared *before* `node`;
`fake_node* successor;` assigned rather than initialised; a `dead_first` local to shift temporary
numbering; the `tmp` local in the inner `else` removed; the outer `if` inverted; `node` not given a
local; accessors replaced by direct `->mLeft` / `->mRight` members; comparison operand order
reversed (`nullptr == x`, `!x`) on the outer and inner `if`; a `static` spelling of the function.

So: **re-ordering comparison operands and the `static` choice were genuinely dead ends here**, and
so was everything that re-spelled the value. The only axis that moved was *deleting a temporary*.

## Also in this unit

`include/rstl/red_black_tree.hpp`'s declaration was renamed
`rbtree_rebalance_for_erase(void* header, void* node_void)` to match. That is a parameter name in a
declaration and cannot change emitted code; `red_black_tree.hpp` is reachable from `set.hpp`,
`multimap.hpp`, `map.hpp` and `hash_map.hpp`, and the gate's per-function diff confirms no other unit
moved.
