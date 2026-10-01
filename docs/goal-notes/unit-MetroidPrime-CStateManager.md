# unit-MetroidPrime-CStateManager

`kind: progress`, target `MetroidPrime/CStateManager` (`main/MetroidPrime/CStateManager`,
`NonMatching`).

## Result

**92 -> 99 of 239 matched functions, +7.** Measured from `build/report.json` at the start of the
run and after it, not recalled. The unit's fuzzy went 14.4457% -> 15.6553%. The unit stays
`NonMatching` and is nowhere near flipping: 99/239, and the five `fn_800379xx` alone are still
96.06% each.

```
./tools/goal_check.sh build/goal/item.json
  goal_check: PASS unit-MetroidPrime-CStateManager
    ok  no judge-owned path touched
    ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
    ok  counts: matched 11918 -> 11925   linked 5727 -> 5727
    ok  check_symbol_names.py
    ok  All:  33.70% fuzzy, 26.85% matched, 12.64% linked (11925 / 28465 functions)
    ok  target rose: main/MetroidPrime/CStateManager: 92 -> 99 / 239 functions
    ok  no asm added
```

`linked 5727 -> 5727` is the right shape for this item: a `progress` item adds functions to a
`NonMatching` unit, so nothing becomes link-complete and the number must not move. It did not.

## What landed

The three destructor families a previous run landed (`fn_800431C4`, `fn_80043510`, `fn_800436CC`
and their forwarders) each sit under a **second, unwritten layer** in retail: a counted array
whose elements are that family's object, and a deleting destructor over the array. All six of
those were unwritten, and they are written here. A fourth family, `fn_800437BC`/`fn_8004380C`, is
new.

| retail | offset | bytes | score |
| --- | --- | --- | --- |
| `fn_800430D0` | 0xCED0 | 80 | **100.00%** |
| `fn_80043120` | 0xCF20 | 96 | **100.00%** |
| `fn_8004341C` | 0xD21C | 80 | **100.00%** |
| `fn_8004346C` | 0xD26C | 96 | **100.00%** |
| `fn_800435D8` | 0xD3D8 | 80 | **100.00%** |
| `fn_80043628` | 0xD428 | 96 | **100.00%** |
| `fn_800437BC` | 0xD5BC | 80 | **100.00%** |
| `fn_8004380C` | 0xD60C | 124 | 95.65% (not counted) |

Seven counted, `fn_8004380C` written but not at 100% so not counted.

Both halves of each pair land together for the reason the earlier note
(`progress-cstatemanager-free-forwarders`) records: `fn_800430D0` calls `fn_80043120`, and a
call to a merely-*declared* function is one more undefined symbol in the host link, which
`probe_sources.sh` gates. Measured, not assumed: **324 undefined before, 324 after**, against
`docs/research/port_link_baseline.txt` (324) and `build/goal/judge/undef.base.count` (324).

## The three spellings that decided the array walks

All three walks (`fn_80043120`, `fn_8004346C`, `fn_80043628`) are the same code with a different
element size, and all three first measured **82.92%**. One change took all three to 100.00%.

**Retail hoists the element pointer out of the loop; the indexed `&m_items[i]` does not.**

```
retail  mr r29,r3 ; addi r31,r29,4 ; b test
        body:  mr r3,r31 ; bl leaf
        next:  addi r31,r31,76 ; addi r30,r30,1
        test:  lwz r0,0(r29) ; cmpw r30,r0 ; blt body

indexed ours (82.92%)
        mr r29,r3 ; mr r31,r29 ; b test
        body:  addi r3,r31,4 ; bl leaf          <- the +4 is INSIDE the loop
```

The element pointer is `self->m_items` (offset +4), and with `&self->m_items[i]` the compiler
hoists `self` into r31 and re-adds 4 on every trip. Spelling it as a walking pointer -

```cpp
SVectorOwner3* p = self->m_items;
for (int i = 0; i < self->m_count; ++i) {
  fn_80043180(p);
  p = reinterpret_cast<SVectorOwner3*>(reinterpret_cast<uchar*>(p) + sizeof(SVectorOwner3));
}
```

- moves the `addi` above the loop and the body becomes retail's `mr r3,r31`. 82.92% -> 100.00%,
  measured once on `fn_80043120` and then applied unchanged to the other two, where it also
  reached 100.00%.

Note the *test* block sits **below** the body in retail (`b` to the test, test branches back), so
these are written as indexed `for` loops and not as pointer-bounded `while`s. The test is
`cmpw r30,r0` on a register pair against the count re-read from `self+0` each trip.

The same three details from the earlier note still hold and are what make the forwarders land:
the flag parameter is a **`short`** (`extsh. r0,r31`, not `cmpwi`), the leaf **returns a
pointer** (`mr r3,r30`), and the forwarder passes **`-1`**.

## `fn_8004380C`: the element array is inline, and the inner loop stores nothing

`fn_800437BC` is the fourth family's forwarder and lands first try at 100.00% - it is
byte-for-byte the same shape as `fn_800430D0`. Its callee `fn_8004380C` (124 B, 0xD60C) is at
**95.65%**, with every instruction accounted for and the residue being register allocation
only. What took it there, in order, all measured:

| spelling | score |
| --- | --- |
| inline `[1]` array of `SByteBuf356`, `*p++ = 0` inner loop | 40.23% |
| ... `memset(buf->m_data, 0, buf->m_length)` | 4.58% |
| ... *descending* dead-counter inner loop | 49.10% |
| ... *ascending* dead-counter inner loop | 90.42% |
| ... pointer stepped at the **top** of the body | 82.71% |
| ... pointer stepped at the **bottom** of the body | 95.65% |
| ... `uchar*` base rather than a typed pointer | 95.65% (same) |
| ... naming the length / `(void)written` / dead pointer increment | 95.65% (same) |

Two things this establishes, both of which cost the earlier attempts:

1. **The elements are inline, not behind a pointer.** Retail seeds the element pointer once with
   `addi r5,r3,4` and then only ever steps it by 356. A `SByteBuf356*` member instead makes the
   compiler emit `lwz r0,4(r3)` and an `add.` to re-derive it, which is 86.26% against 95.65%
   for the inline array. The list is `{ int m_count; SByteBuf356 m_items[1]; }`.

2. **The inner loop stores nothing.** It is MWCC's memset expansion with the stores optimised
   away: `srwi r0,r4,3` for the 8-bytes-per-trip count, `subf r0,r3,r7` + `mtctr` for the
   one-byte remainder, `li r3,0` seeding the byte offset, and **no store instruction in the
   function**. So it cannot be written as `*p++ = 0` (that is 40.23% and gets a plain
   1-byte-per-trip loop) and it cannot be handed to `memset` (4.58%, real stores). It is a loop
   whose body is a **dead counter**, and the counter must be **ascending** for the unroll to
   fire - a descending one is 49.10% and does not unroll at all.

The residue at 95.65% is that the registers are rotated: retail holds count in r6, the element
pointer in r5, the length in r7, the index in r8 and the byte offset in r3; the same code here
holds them in r4, r6, r5, r7, r8. Every opcode, immediate, branch target and displacement
matches. Four further spellings aimed at the allocation (naming the count, naming the length,
`(void)written`, a dead pointer increment instead of an int) all left it at 95.65%. **Not
attempted:** forcing the offset register with a `volatile` or an explicit register variable -
that would be a codegen hack rather than a spelling, and this repo does not use them.

## Everything tried that did not land

- **`fn_8003ABF0` (0x8003ABF0, 248 B) - not written in the end.** It is a second block copy over
  the same 16-byte float elements as `fn_800391E4`, and unlike that one it is index-counted:
  retail unrolls the body 4x per trip (64 bytes) and has a `andi. r4,r4,3` remainder loop. Four
  spellings measured: indexed `for` 21.74%, pointer-bounded `for` 20.60%, explicit 4-way unroll
  2.82%. None is close, and the earlier note's WALL for the sibling `fn_800391E4` (14 spellings)
  suggests the same wall. **Reverted** rather than left in at a score that earns nothing and
  adds a function the reviewer would have to read.
- The `SendScriptMsg` pair (99.58 / 99.52%), the five `fn_800379xx` (96.06%), `fn_800391E4`
  (94.38%), `AllocateUniqueId` (83.31%) and `fn_80037784` (96.67%) were **not touched**. The
  first three are walls an earlier run characterised with exact bytes
  (`docs/history/decomp-findings.md`, and the comment block at `CStateManager.cpp:380`), and the
  last two are the `@stringBase0` wall, which is a section-claim property of the merged rodata
  and not a source problem. Re-running their spellings would have measured nothing new.

## Gates, all re-measured after the change

```
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (unchanged)
./tools/probe_sources.sh        probe: 745 files, 0 failed, 0 errors;
                                link: LINKED (324 undefined, 0 duplicates)   (324 = baseline)
python3 tools/check_symbol_names.py   0 declared names are missing from their object
python3 tools/check_decl_order.py     ok: 978 unit(s) checked, 31 permuted, all 31 accounted for
./tools/decomp_build.sh          All:  33.70% fuzzy, 26.85% matched, 12.64% linked
                                  (11925 / 28465 functions)
```

`check_decl_order.py` reports CStateManager as permuted; that is **pre-existing** (it was in
`docs/research/decl_order.md` before this change) and the rule only bites on a flip, which this
unit is not close to. The new functions are declared descending by retail offset within their
block (0xD60C, 0xD5BC, 0xD428, 0xD3D8, 0xD26C, 0xD21C, 0xCF20, 0xCED0).

The one compiler warning in this unit (`CStateManager.cpp:1345`, "return value expected", in
`GetEditorIdForUniqueId`) is pre-existing and not in the diff.

## Ranges claimed

None. This is a DOL unit: no `splits.txt` change, no `configure.py` change, no carve, no `asm`.

## Files

- `src/MetroidPrime/CStateManager.cpp` - one new block, lines 283-438, sitting between the
  existing family-1/2/3 forwarders and the `fn_800391B4` block. It adds eight functions and the
  local types they walk (`SVectorOwner3Array`, `STokenVectorOwner24`, `SRecordArray488`,
  `SRecordArray488List`, `SByteBuf356`, `SByteBuf356List`), each with a `CHECK_SIZEOF` tying the
  stride to the element size rather than a literal. No header, no `configure.py`, no `splits.txt`.

`docs/HANDOFF.md` is also dirty in this worktree, rewritten by `goal_check.sh`'s own
`gate.sh` run (the judge runs it with `MP_GATE_DOCS_WRITE=1` and derives the state block from
the tree). Not hand-edited; the driver discards it.

## No NEW:

Nothing new is blocked. `fn_8004380C` at 95.65% and `fn_8003ABF0` are spelling questions inside the
unit this item already targets, not separate units, so neither warrants a `NEW:` line. What the
unit needs next is `fn_80042C80` and `fn_80042C28` (the `__dt__13CStateManager` sub-tree, which
calls five out-of-unit symbols - `fn_80234474`, `__dt__14CRumbleManagerFv`, `fn_80042FAC`,
`fn_8004330C`, `fn_80045D6C` - none of which any source file defines, so it is a link-closure
job before it is a decompilation one).
