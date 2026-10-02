# destroy-pair-ccollisionactormanager (lane 6, 2026-10-02)

`MetroidPrime/CCollisionActorManager`: **25 -> 26 / 30 functions matched**, unit fuzzy
69.75% -> **71.78%**, `matched_code` 53.74% -> **55.03%** (4020 -> 4116 bytes). `All:` held at
35.18% fuzzy / 28.92% matched / 12.90% linked, **12446 -> 12447** / 28465 functions. Report diff
against `build/goal/judge/report.base.json`: **0 functions worse** anywhere in the tree.
`tools/goal_check.sh build/goal/item.json` -> **PARTIAL** (exit 3): "flip_test
MetroidPrime/CCollisionActorManager.cpp: FAIL, but the target rose; commit it and keep the item".
Unit stays `NonMatching`; nothing was flipped and nothing was committed.

The item asked for retail's outlined `rstl::destroy` / `destroy_impl` pair for
`vector<CJointCollisionDescription>` - `fn_8013639C` (56 B) and `fn_801363D4` (96 B). The previous
run's `NEW:` line said this needed a change to the shared `include/rstl/construct.hpp`. **It does
not.** Both functions can be matched inside this one unit, which is what this run did; the header
is untouched, so no other unit moved.

## What landed

`src/MetroidPrime/CCollisionActorManager.cpp`, one `extern "C"` block inserted between
`CCollisionActorManager::CCollisionActorManager` and `~CCollisionActorManager` (the source
position that puts them in retail's `.text` slot - see "Placement" below):

* **`fn_801363D4` 0.00% -> 100.00%**, 96 bytes, **byte-identical to retail** (verified by
  `powerpc-eabi-objcopy -O binary --only-section=.text` on both objects and comparing the slices).
* **`fn_8013639C` 0.00% -> 99.71%**, 56 bytes. 13 of its 14 instructions match; the whole body is
  retail's except which of its two frame slots holds `end`.

The two together are the whole of the `matched_functions` rise; `fn_801363D4` alone is the count.

### The one thing that made `fn_801363D4` match: `end` is a **non-const** `It&`

Retail keeps the *address* of the end iterator in the callee-saved r30 (`mr r30,r4`) and re-reads
`0(r30)` on every pass of the loop (0x80136410), while `begin` is dereferenced once into r31 and
becomes the induction variable. The previous run's notes read that as "destroy_impl with `const
It&` parameters"; measured, that is not it. Written with `const It&` - or with `end` by value, or
behind a `const` pointer, or `const volatile` - mwcceppc **hoists `*end` out of the loop** into
r31, the test becomes a register compare, and the function is 92 bytes with 22 of retail's 24
instructions at 91.67%. A **non-const `It&`** is what stops the hoist: the loop body can write
through it, so the load stays in the loop and the reference stays in a callee-saved register.
`begin` may be `It` by value or `const It&` - both give 100.00%. `begin` as a non-const `It&`
puts its `lwz r31,0(r3)` one instruction later than retail has it and drops to 91.67%.

```cpp
extern "C" void fn_801363D4(CJointDescriptionIterator b, CJointDescriptionIterator& e) {
  CJointDescriptionIterator cur = b;
  for (; cur != e; ++cur) {
    rstl::destroy(&*cur);   // -> internal_dereference<basic_string> at cur+0x2c, inlined
  }
}
```

`rstl::destroy(&*cur)` is the header's own template and produces retail's five-instruction loop
body (`cmplwi r31,0 / beq`, `addic. r0,r31,0x2c / beq`, `addi r3,r31,0x2c`, `bl
internal_dereference<...>`, `addi r31,r31,0x68`) exactly - it already did, inside the 100-byte
local `destroy<...>` outline the header emits. Spelling the body `cur->~CJointCollisionDescription()`
instead, or as `destroy_impl(&*cur)`, makes no difference.

### `fn_8013639C`'s remaining difference is one frame offset

Retail: `lwz r5,0(r4)` (end) -> `stw r5,0x8(r1)`; `lwz r0,0(r3)` (begin) -> `stw r0,0xc(r1)`, with
`addi r4,r1,0x8` and `addi r3,r1,0xc` in that order, so **end at +8 and begin at +0xc**. Ours puts
**begin at +8 and end at +0xc** with the same instruction order and the same frame. The two
`addi`/`stw` pairs are the only difference; everything else, including the frame size (-0x10) and
the `bl`, is identical. The code is semantically the same either way - this is a frame-slot
assignment, not a logic difference - and about fifty spellings (below) all put the end at the
higher offset.

`fn_8013639C`'s parameters are `const&`, not by value: retail's `~vector` computes the two ends,
stores them at +0xc and +0x10 of its own frame and calls with `addi r3,r1,0x14` / `addi r4,r1,0xc`
(0x8013633C / 0x80136344), i.e. **addresses**, and 9C dereferences both. Taking them by value
produces a 32-byte forwarder that forwards r3/r4 straight through (57.07%).

## Spellings measured this run

Every row is a real build (`tools/fast_try.sh`); the harness reported a build failure rather than
scoring a stale object, which matters - `fast_try.sh` prints `build FAILED` and still regenerates
the report, so a failed variant silently scores whatever was on disk before it.

`fn_801363D4`, paired with a fixed `fn_8013639C` where noted (D4 score first):

| `fn_801363D4` parameters | 9C | D4 |
|---|---|---|
| `It b, It& e` (**landed**) | 99.71 | **100.00** |
| `const It& b, It& e` | 57.07 | **100.00** |
| `It b, const It& e` | 78.43 | 91.67 |
| `const It& b, const It& e` (the spelling the last run settled on) | 57.07 | 91.67 |
| `It& b, It& e` | 57.07 | 91.67 |
| `It& b, const It& e` | - | 91.67 |
| `const volatile It& e` | 57.07 | 91.67 |
| `It b, const It* e` | 78.43 | 91.67 |
| `const It& b, CJointDescriptionIterator* e` | - | 91.67 |
| `const It* b, const It* e` | 57.07 | 91.67 |
| `const It* b, CJointDescriptionIterator* e` | 57.07 | 100.00 |
| `It& b, CJointDescriptionIterator& e`, `while` instead of `for` | - | build error |
| `for (It cur = b; ...)`, `e != cur` instead of `cur != e` | 57.07 | 91.88 |
| `cur->~CJointCollisionDescription()` instead of `rstl::destroy(&*cur)` | 57.07 | 91.67 |

`fn_8013639C`, paired with the landed D4:

| `fn_8013639C` body | 9C |
|---|---|
| `(const It& b, const It& e) { It ee(e); fn_801363D4(b, ee); }` (**landed**) | **99.71** |
| `(It b, It e) { It ee(e); fn_801363D4(b, ee); }` (by value) | 99.71 |
| `(It b, It e) { It ee(e); It* const pb = &b; fn_801363D4(*pb, ee); }` | 99.71 |
| `(It b, It e) { It bb(b); It ee(e); fn_801363D4(bb, ee); }` | 92.36 |
| `(const It& b, const It& e) { It ee(e); It bb(b); fn_801363D4(bb, ee); }` | 92.36 |
| `(It b, It e) { It ee(e); fn_801363D4(b, ee); }` with `e`'s slot written through a pointer | 99.71 |
| `(It b, It e) { It bb(b); fn_801363D4(bb, e); }` (end never copied) | 85.14 |
| `(It b, It e) { fn_801363D4(b, e); }` (no frame copies at all) | 57.07 |
| a local `struct { It end; It begin; }` written `slots.end = e; slots.begin = b;` | 70.21 |
| the same as a two-element local array | 70.21 |
| `(It* b, It* e)` parameters, two local copies | 77.36 |

### A spelling that scores 99.86% and is WRONG - do not take it

```cpp
static void destroy_pair(CJointDescriptionIterator& e, CJointDescriptionIterator b) {
  fn_801363D4(b, e);
}
extern "C" void fn_8013639C(CJointDescriptionIterator b, CJointDescriptionIterator e) {
  CJointDescriptionIterator ee(e);
  destroy_pair(ee, b);
}
```
scores **99.86%** - the best number this run produced - and is a trap. `destroy_pair`'s own
parameters are `(e, b)`, so when the compiler inlines it, `fn_801363D4` is entered with **r3
holding the end and r4 the begin**: `fn_801363D4` would start its loop at `end` and walk forward
looking for `begin`. The higher score comes entirely from the two `addi` immediates lining up with
retail's; the values behind them are exchanged. It was measured, recognised and dropped. The
reviewer-facing rule this is an instance of: a fuzzy number is not a result, and a shape that
matches while the data does not is worse than 99.71% of correct code.

## Placement, and the decl-order gate

`tools/check_decl_order.py` (run by `gate.sh`) compares the order our object emits against
retail's, so the two functions have to be *declared* where mwcceppc's reverse emission puts them.
Placing the block at the end of the file, next to the other reproduced `fn_` statics, put them at
`.text` 0 and failed the gate with `GATE FAIL: decl-order`; placing it after
`CJointCollisionDescription::ScaleAllBounds` emitted them after `__ct__` and also failed. They
belong between `CCollisionActorManager::CCollisionActorManager` and
`CCollisionActorManager::~CCollisionActorManager`, which is where they are now:

```
ours   __dt__22CCollisionActorManagerFv  destroy<...>__4rstl...  fn_8013639C  fn_801363D4  __ct__22CCollision...
retail __dt__22CCollisionActorManagerFv  __dt__vector              fn_8013639C  fn_801363D4  __ct__22CCollision...
```
(`__dt__vector` is weak and `destroy<...>` is not in retail, so both are outside what the check
compares; `python3 tools/check_decl_order.py --unit MetroidPrime/CCollisionActorManager` -> "ok:
1 unit(s) checked, none emits its functions out of retail order".)

`~vector` (0x80136318, 132 B) and `~CCollisionActorManager` (0x801362C4, 84 B) were at 100% before
this run and still are. `~vector`'s `bl` to the local `destroy<...>` outline is still +0x38, the
same offset as retail's `bl fn_8013639C`, which is why it is untouched.

## What is left, and why the unit still cannot flip

`tools/unit_fit.sh MetroidPrime/CCollisionActorManager.cpp`:

```
.text      claimed   7480   ours   7668   over by 188      (was over by 36; +152 = the two functions)
.ctors     claimed      4   ours      0   SHORT by 4
.rodata    claimed      8   ours      7   SHORT by 1
.sbss      claimed      8   ours      5   SHORT by 3
.sdata2    claimed     36   ours     32   SHORT by 4
.sdata     claimed      -   ours     40   NOT CLAIMED by splits.txt
extra: + 100 destroy<pointer_iterator<CJointCollisionDescription,...>>__4rstl...
       +  92 __dt__26CJointCollisionDescriptionFv
       +  80 __dt__Q24rstl66basic_string<c,...>Fv
```

So the flip needs three independent things this item did not attempt: the three extra functions
gone, the four short sections exact, and the `undefined` below fixed. The `2.0f` in `.sdata2`
that the last run's notes flagged is still the most concrete lead into `__ct__` (30.71%, the
unit's remaining bulk).

## A pre-existing link error, not from this change

`tools/flip_test.sh` fails, but on something this run did not touch:

```
### mwldeppc.exe Linker Error:
#   undefined: 'lbl_804191A8'
```

`extern "C" TAreaId lbl_804191A8[2];` (line 371) is a bare `extern` **declaration**; retail's
object owns those 8 bytes of `.sbss`, so when this unit is linked our object has to *define* them
and nothing in the tree does. **Verified pre-existing**: `git stash` + `./tools/flip_test.sh
MetroidPrime/CCollisionActorManager.cpp` on the clean HEAD `c8cea705` prints the identical error.
It arrived with `babd7383 progress: progress-unit-ccollisionactormanager`, not with this change, and
it is left alone here because fixing it is an unrelated change to this item's diff. A tentative
definition (`TAreaId lbl_804191A8[2];`, which mwcceppc puts in `.sbss`) is the likely fix; it is
not filed as a `NEW:` item because on its own it raises no count - the unit still could not flip
with the three extra functions and the four short sections.

## Honest limitation

Neither new function is called from our own `~vector`. The header's `rstl::destroy` still outlines
its own 100-byte local copy (`destroy<pointer_iterator<...>>__4rstl...`) right after `~vector` in
`.text`, exactly where it was before, so the object now contains the header's copy **and** the two
functions that reproduce retail's. Pointing the header at `fn_8013639C` instead means editing
`include/rstl/vector.hpp`, which moves `~vector` in every unit that has one and has to satisfy
"no function anywhere gets worse" across the whole tree. That is the tree-wide item the previous
run's `NEW:` line described, and it is still worth filing - but it is not this one, and this item
does not need it: `fn_801363D4` is a byte-exact 100% match without touching a shared header.

## Verification actually run

```
./tools/decomp_build.sh main/MetroidPrime/CCollisionActorManager
  All:  35.18% fuzzy, 28.92% matched, 12.90% linked (12447 / 28465 functions)
  main/MetroidPrime/CCollisionActorManager: 71.78% fuzzy, 55.03% matched (26 / 30 functions)
sha1sum build/G2ME01/main.dol                    -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh                        -> 753 files, 0 failed, 0 errors; LINKED (291 undefined, 0 duplicates)
python3 tools/check_symbol_names.py             -> checked 525 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit MetroidPrime/CCollisionActorManager -> ok
./tools/unit_fit.sh MetroidPrime/CCollisionActorManager.cpp
./tools/flip_test.sh MetroidPrime/CCollisionActorManager.cpp -> FAIL (the `lbl_804191A8` link error above)
./tools/goal_check.sh build/goal/item.json      -> PARTIAL (exit 3), "target rose: 25 -> 26 / 30", "no asm added"
report diff vs build/goal/judge/report.base.json: 0 functions worse
```

`docs/HANDOFF.md`'s state block was rewritten by the gate, not by hand; the driver discards it.

WALL: fn_8013639C 99.71% - 13 of 14 instructions, and the only difference from retail is which of
the two frame slots holds `end` (+8 against +0xc); about fifty spellings this run, all correct
code, all put `end` at the higher offset.
