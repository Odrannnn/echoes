# cobbtree-ccollisionedge-stream-ctor

`kind: progress`, `target: WorldFormat/COBBTree`. The unit stays `NonMatching`; no `flip_test` was
run to decide anything. This is the `NEW:` item `progress-unit-cobbtree` filed, done.

## Measured result

Re-measured the clean tree first (`./tools/decomp_build.sh WorldFormat/COBBTree`): **35 / 37**,
`matched_code` 5600 / 6428, `matched_code_percent` = `fuzzy_match_percent` = 87.12%, `fn_8024F0FC`
and `fn_8024F3A8` the only two at 0.00%.

| | before | after |
|---|---|---|
| `matched_functions` | **35 / 37** | **36 / 37** |
| `matched_code` | 5600 / 6428 | **5784 / 6428** |
| `matched_code_percent` / `fuzzy_match_percent` | 87.12% | **89.98%** |
| `All:` matched | 12412 | **12413** |

`python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`:

```
matched  12412 -> 12413   linked 5863 -> 5863   (+1 functions at 100%, 0 units newly linked)
  +100%    main/WorldFormat/COBBTree :: __ct__Q24rstl51vector<14CCollisionEdge,Q24rstl17rmemory_allocator>FR12CInputStreamRCQ24rstl17rmemory_allocator
  RENAMED  main/WorldFormat/COBBTree :: fn_8024F0FC -> __ct__Q24rstl51vector<14CCollisionEdge,...>FR12CInputStreamRCQ24rstl17rmemory_allocator (0.00% -> 100.00%)
no regression
```

The `RENAMED` line here is real, unlike the last run's on this unit where the tool paired names by
position in a list. `build/report.json` agrees: the function at 0x8024F0FC is 100.00%, and the only
function still at 0.00% is `fn_8024F3A8` (644 B).

## The change

One spelling, and it worked first time: **define `CCollisionEdge(CInputStream&)` out of class.**

- `include/WorldFormat/CCollisionEdge.hpp:15-20` - the constructor becomes a declaration. The body
  (two `in.Get<ushort>()`) is unchanged, just moved.
- `src/WorldFormat/COBBTree.cpp:8-22` - the out-of-class definition, with the measurement that
  justifies the split.
- `config/G2ME01/symbols.txt:10388` - the rename below.

The vector stream constructor is the generic `rstl::vector<T>::vector(CInputStream&, const Alloc&)`
at `include/Kyoto/Streams/CInputStream.hpp:196-203`, whose element loop is
`push_back_unsafe(in.Get<T>())`. With `T`'s stream constructor written in the class, mwcceppc inlines
it, no symbol survives, and the loop unrolls four elements per iteration:

```
before   build/G2ME01/src/WorldFormat/COBBTree.o  0x17d4 .. 0x19a4   0x1D0 = 464 bytes
after    build/G2ME01/src/WorldFormat/COBBTree.o  0x17d4 .. 0x188c   0x0B8 = 184 bytes
retail   fn_8024F0FC, 0x8024F0FC, symbols.txt size:0xB8              184 bytes
```

Both are instruction-for-instruction identical to retail, including the shape that identifies the
fix: retail's loop is `mr r4,r29 / addi r3,r1,8 / bl <ctor>` - destination is the stack temporary at
`r1+8` - then `lhz r0,8(r1) / sth r0,0(r3) / lhz r0,10(r1) / sth r0,2(r3)`. Ours now emits the same
`bl`; before, the two reads were inlined straight into the slot and the loop carried a
`srwi./mtctr/bdnz` four-way unroll retail has not got.

The out-of-line definition is itself byte-exact with retail:

```
CCollisionEdge::CCollisionEdge(CInputStream&)  ours 0x1d04..0x1d30  0x2C bytes
fn_8024747C, 0x8024747C, symbols.txt size:0x2C                      44 bytes
```

same 11 instructions, same order.

**Why this .cpp and not its own.** `COBBTree.cpp` is the only unit that constructs a
`CCollisionEdge` from a stream - `rstl::vector<CCollisionEdge>`'s stream constructor is
instantiated only by `COBBTree::SIndexData`'s initialiser list (`include/WorldFormat/COBBTree.hpp:81`),
and the other two units that include the header (`CAreaOctTree.cpp`, `CCollisionSurface.cpp`) only
ever hold `const CCollisionEdge*`. Retail puts the helper at 0x8024747C, which is the start of the
**unclaimed** gap 0x8024747C..0x802474B4 (`splits.txt:1528` ends CCollisionSurface at 0x8024747C;
CMetroidModelInstance starts at 0x802474B4), so retail has no unit for it either and this is not
worth a carve.

**Blast radius, measured.** `main.dol`'s sha1 is byte-identical (below) - COBBTree is `NonMatching`,
so the link takes retail's object for it and our definition is not in the DOL at all. The two units
that include the header and *are* linked, `CCollisionSurface.cpp` and `CCollisionPrimitiveData.cpp`,
never call the stream constructor, so their objects did not move.

## Config change, as a list of intended changes

```
config/G2ME01/symbols.txt:10388
-fn_8024F0FC = .text:0x8024F0FC; // type:function size:0xB8 align:4
+__ct__Q24rstl51vector<14CCollisionEdge,Q24rstl17rmemory_allocator>FR12CInputStreamRCQ24rstl17rmemory_allocator = .text:0x8024F0FC; // type:function size:0xB8 align:4
```

No address gained a second name and none was dropped. **`symbols.txt` takes one `=` per line**:
`fn_8024F0FC = <mangled> = .text:...` is rejected by dtk with
`Failed to parse symbol line` and the `-r` configure fails outright - replace the name, do not chain
them. A rename needs `./tools/decomp_build.sh -r`; `decomp_build.sh` alone leaves the retail-side
objects (`build/G2ME01/obj/`) carrying the old name, objdiff then pairs nothing, and the report does
not move even though the bytes are right.

That is the second half of this item and worth restating: **objdiff pairs functions by name, so a
byte-exact function still scores 0.00% while retail's address is `fn_`-named.** The bytes were
already right for one build before the rename and the report did not budge.

## Gates, all run in this worktree after the change

```
$ ./tools/goal_check.sh build/goal/item.json
goal_check: PASS cobbtree-ccollisionedge-stream-ctor
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12412 -> 12413   linked 5863 -> 5863
  ok    check_symbol_names.py
  ok    All:  35.08% fuzzy, 28.78% matched, 12.90% linked (12413 / 28465 functions)
  ok    target rose: main/WorldFormat/COBBTree: 35 -> 36 / 37 functions
  ok    no asm added

$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  build/G2ME01/main.dol

$ python3 tools/check_symbol_names.py
checked 525 units; 0 declared names are missing from their object

$ python3 tools/check_decl_order.py --unit WorldFormat/COBBTree
ok: 1 unit(s) checked, none emits its functions out of retail order

$ ./tools/unit_fit.sh WorldFormat/COBBTree.cpp
   .text over the claimed range by 2069 bytes; 12 function(s) in ours but not the retail unit
   object, 2672 bytes total   (was 2305 / 12 / 3092 before this change)
```

`unit_fit`'s byte total fell by 420 = 464 - 44: the vector stream constructor stopped being one of
the "ours but not retail's" extras by becoming byte-identical to retail's, and the new 44-byte
helper is not counted as extra because dtk put retail's bytes at 0x8024747C in the neighbour's
auto-text object. Both numbers improved; no initialisation was dropped, `CHECK_SIZEOF` untouched, no
`asm`, and `docs/HANDOFF.md` (which `gate.sh` rewrites when `MP_GATE_DOCS_WRITE=1`) was reverted
out of the diff.

## Left

`fn_8024F3A8` (0x8024F3A8, 644 B, `rstl::vector<u64>::vector(CInputStream&, const Alloc&)`) is the
unit's last 0.00% function. It was not touched or measured in this run - the previous run's note
records the spellings already tried - so no `WALL:` line here.