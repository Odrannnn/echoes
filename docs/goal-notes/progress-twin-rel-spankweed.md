# progress-twin-rel-spankweed - `module:SpankWeed`, +3 functions

## What landed

Two new `Matching` units carve the run of `rstl` template instantiations out of SpankWeed's
(module 73) unclaimed `auto_00_000003F4_text` range, and both are byte-exact:

| unit | claim | functions |
| --- | --- | --- |
| `SpankWeed/MetroidPrime/ScriptObjects/SpankWeedCopyFloat.cpp` | `.text 0x000039E8..0x00003ABC` (0xD4 = 212 B) | `fn_73_39E8`, `fn_73_3A80` |
| `SpankWeed/MetroidPrime/ScriptObjects/SpankWeedCopyDesc.cpp` | `.text 0x00003B7C..0x00003BE4` (0x68 = 104 B) | `fn_73_3B7C` |

What they are, measured off `build/G2ME01/SpankWeed/asm/auto_00_000003F4_text.s` and confirmed
against the DOL twins the item listed:

* `fn_73_39E8` - `rstl::vector<SConnection, rmemory_allocator>::reserve(int)`, 0x98 = 152 bytes,
  38 instructions. Instruction-for-instruction the DOL's
  `reserve__Q24rstl48vector<11SConnection,Q24rstl17rmemory_allocator>Fi` (0x800485E4,
  `config/G2ME01/symbols.txt:1356`), which `src/MetroidPrime/CEntity.cpp` already matches at 100%.
* `fn_73_3A80` - the `rstl::uninitialized_copy` it calls, 0x3C = 60 bytes, 15 instructions; the
  DOL's is at 0x8004867C. Three `lfs`/`stfs` pairs and a 12-byte stride, no call.
* `fn_73_3B7C` - `rstl::uninitialized_copy` over this module's 0x68-byte
  `CJointCollisionDescription`, 0x68 = 104 bytes, 26 instructions, copy-constructing through the
  module's own `fn_73_25EC` (`.text 0x25EC`). The same shape and the same declaration as
  `src/MetroidPrime/ScriptObjects/CElitePirateVecCopy.cpp` (module 15, `Matching`, 100%).

All three were written as plain `extern "C"` functions under the module's own `fn_73_<off>` names,
as `CElitePirateVecCopy.cpp` is, because dtk could not name these TU-local weak instantiations and
renaming them in `config/G2ME01/rels/SpankWeed/symbols.txt` would change the module's relocations.
`rstl::pointer_iterator` is one word wide (`include/rstl/pointer_iterator.hpp:59`), so each
iterator argument is a one-word class passed by value - that is what the `addi r3,r1,0x14` /
`addi r4,r1,0xc` temporaries and the `lwz r3,0x0(r3)` at the head of each copy are.

Both units carry `mw_version="GC/2.7"`: REL objects default to GC/1.3.2, and that generator
schedules the by-value iterator load differently (`CElitePirateVecCopy.cpp` records the same).

## Measured

```
module:SpankWeed              19 / 104  ->  22 / 104 functions
project matched_functions  13308       ->  13311      (linked 6356 -> 6359)
total_functions            28465       ->  28465      (unchanged, as splits.txt edits require)
main.dol                   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
SpankWeed.rel              08c018c8c4b584857c99154348432ffa1bee4364  == config.yml, cmp-equal to
                           orig/G2ME01/files/RelProd/SpankWeed.rel
all 86 RELs                 0 differ from config.yml
```

Supporting checks, all clean: `unit_fit.sh` on both units (`.text claimed 212 ours 212 retail 212,
fits` and `claimed 104 ours 104 retail 104, fits`, **no extra functions** on either);
`check_symbol_names.py` 0 missing; `check_decl_order.py` 1126 units, 37 permuted, neither of these
among them; `check_files_cmake.py` 0 dead; `check_module_wiring.py` 126 units of our own code in
80 modules, SpankWeed among them; `check_raw_offsets.py` 185 sites in 79 files, unchanged;
`probe_sources.sh` 855 -> **857** files, 0 failures, and the port link still 287 undefined /
0 duplicates, because both files are behind `#ifdef __MWERKS__` and so compile to nothing on the
host; `tools/goal_check.sh build/goal/item.json` -> **PASS**.

**Our objects really are in the link**, which is the one rule: `build.ninja`'s
`build/G2ME01/SpankWeed/SpankWeed.plf` rule lists
`build/G2ME01/src/MetroidPrime/ScriptObjects/SpankWeedCopyFloat.o` and `.../SpankWeedCopyDesc.o`,
and no `obj/` counterpart exists - dtk split the two ranges out of the module's own bytes into
`auto_00_00003ABC_text.o` (1 function) and `auto_00_00003BE4_text.o` (4), and
`auto_00_000003F4_text` fell 81 -> 73 functions. The claimed bytes therefore have exactly one
possible source, and the module still hashes to `config.yml`.

## Not claimed: the other four functions of the same run

All four are written and measured; none is exact, so none is claimed. Left in place for the next
run, with the spellings, because re-trying them blind is what the note is for.

* **`fn_73_3ABC`** (`.text 0x3ABC..0x3B7C`, 0xC0) - `rstl::vector<CJointCollisionDescription,
  rmemory_allocator>::reserve(int)`. **48 instructions against retail's 48, 0xC0 = 192 bytes
  exactly, and the only difference is four register numbers**: retail keeps `newData` in **r29**
  and the destroy loop's bound in **r31**; ours keeps `newData` in **r31** and the bound in **r29**.
  Everything else - the iterator temporaries, the `addic.`/`beq` guard, the `cmplwi r30,0` element
  test, the `addi r3,r30,0x2c` into `internal_dereference`, the stores - is byte-identical. Twelve
  spellings tried, all of them landing on the same four: plain `void* newData` with a `char*`
  cursor/bound; `const int newSize`; `const SVec* self`; `char* const newData`; `char* const last`;
  `SJointDesc*` cursor and bound with `++elem`; `void* last`; `void*` for both cursor and bound;
  the bound computed before the cursor; a second `self->mItems` read in the destroy loop; named
  `const SIter begin/end` temporaries (47 instructions, worse); and the header's own
  `rstl::rmemory_allocator::allocate/deallocate` instead of `extern "C"` calls (byte-identical to
  the plain spelling). The 12-byte twin `fn_73_39E8` is 38/38 and exact, so this is the destroy
  loop's extra live value, not the source.
* **`fn_73_3838`** (0x3838, 0xB8) - `red_black_tree<pair<uint,int>,...>::copy_from`. Two source
  traps: retail's `create_node` is a **member**, so r3 is the class pointer and r4..r8 the five
  arguments - an `extern "C"` spelling without the leading `self` silently loses three of them -
  and the two `set_parent` stores then land 0x18 bytes off because the node's `mParent` is at +0x8
  while the value starts at +0x10. Reaching the node through a struct rather than through raw
  `reinterpret_cast` is what fixes the offsets.
* **`fn_73_38F0`** (0x38F0, 0x78) - the `create_node` `copy_from` calls. **30 instructions against
  retail's 30**; the only differences are where `li r3,0x24` sits (retail index 2, right after
  `mflr`, ours index 9, immediately before the call) and the resulting argument shuffles.
* **`fn_73_3968`** (0x3968, 0x80) - `free_node_and_sub_nodes`. Ours is **33 instructions against
  retail's 32**: one redundant `beq` in the `if (node) node->~node()` null test. Retail emits
  `cmplwi r31,0 / beq / addic. / beq / addic. / beq`, ours emits `cmplwi r31,0 / beq / beq /
  addic. / beq / addic. / beq`.

No `WALL:` or `NEW:` line is filed: the item passes, and the guidance is that a measured wall
belongs in this file rather than in the queue.

## Two things this cost, worth the next run's time

* **mwcceppc will not let you force a class-template member into an object.** `template void
  rstl::vector< SConnection, rstl::rmemory_allocator >::reserve( int );` is rejected with
  `Error: illegal explicit template instantiation`. `template class rstl::vector<...>;` *is*
  accepted and emits eight symbols - and it emits the two this item wanted adjacent and in the
  right order (`reserve` then its `uninitialized_copy`, caller before callee, `nm` offsets 0x8AC
  and 0x944) - but six of the eight are symbols retail's object does not define, so it is not a
  carve. Explicit instantiation of a *free* function template compiles and emits; of the
  `static inline rstl::uninitialized_copy` it compiles and emits **nothing** (internal linkage).
  The hand-written `extern "C"` route is the only one that works, and `CElitePirateVecCopy.cpp` is
  the proof it reaches 100%.
* **The claim must be a contiguous range, so a matched function cannot skip its unmatched
  neighbours.** `fn_73_3A80` (0x3A80) and `fn_73_3B7C` (0x3B7C) are both exact, but
  `fn_73_3ABC` sits between them, so they are two units and two files, not one. Taking the run as
  one unit is right when the run matches and wrong when one member of it does not.

## Files

* `src/MetroidPrime/ScriptObjects/SpankWeedCopyFloat.cpp` (new) - 2 functions
* `src/MetroidPrime/ScriptObjects/SpankWeedCopyDesc.cpp` (new) - 1 function
* `configure.py` - two `Object(Matching, ..., mw_version="GC/2.7")` entries in `Rel("SpankWeed", ...)`
* `config/G2ME01/rels/SpankWeed/splits.txt` - the two claim ranges
* `files.cmake` - the two sources, with the reason `CElitePirateVecCopy.cpp` gives