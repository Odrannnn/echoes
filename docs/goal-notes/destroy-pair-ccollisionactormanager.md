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

---

# destroy-pair-ccollisionactormanager (lane 1, 2026-10-02, second attempt)

`MetroidPrime/CCollisionActorManager`: **26 -> 27 / 30 functions matched**, unit fuzzy
71.78% -> **71.79%**, `matched_code` 55.03% -> **55.78%** (4116 -> 4164 bytes). `All:` held at
35.32% fuzzy / 29.14% matched / 12.91% linked, **12496 -> 12497** / 28465 functions, `linked`
5872 unchanged. `tools/goal_check.sh build/goal/item.json` -> **PARTIAL** (exit 3): "flip_test
MetroidPrime/CCollisionActorManager.cpp: FAIL, but the target rose; commit it and keep the item",
plus "ok no judge-owned path touched", "ok gate.sh", "ok no asm added". Unit stays `NonMatching`;
nothing was flipped and nothing was committed.

**The wall the last run declared is gone, and it was not a wall.** `fn_8013639C` went
**99.71% -> 100.00%, byte-identical**, and `fn_801363D4` is still 100.00% (unchanged, 96 bytes).
Both are now exact, which is the whole of the `matched_functions` rise.

## What changed: both parameters by value, in BOTH functions

```cpp
extern "C" void fn_801363D4(CJointDescriptionIterator b, CJointDescriptionIterator e) {
  CJointDescriptionIterator cur = b;
  for (; cur != e; ++cur) {
    rstl::destroy(&*cur);
  }
}

extern "C" void fn_8013639C(CJointDescriptionIterator b, CJointDescriptionIterator e) {
  fn_801363D4(b, e);
}
```

The landed spelling was `(It b, It& e)` + `(const It& b, const It& e)` with a named local
`CJointDescriptionIterator ee(e);` and **no local at all** is what matches. `fn_8013639C` becomes a
pure two-line forwarder.

### Why the last run read this backwards

It inferred from `~vector` passing the two ends' **addresses** in r3/r4 (0x8013633C / 0x80136344)
that 9C's parameters must be lvalue references, and from 9C re-copying both into its own frame
that `end` had to be copied into a named local to give the callee a modifiable lvalue. Both
inferences are wrong, and they are wrong for one reason: **`CJointDescriptionIterator` is a class
type** (a `rstl::pointer_iterator`, i.e. one pointer wrapped in a class), and under the SGI ABI
mwcceppc passes such a type **by value as an address**. A by-value parameter and an lvalue
reference receive the identical thing in r3/r4, so *the call site cannot tell you which the
callee was declared with*. `~vector`'s addresses are consistent with either spelling; only 9C's
own frame layout settles it.

### The mechanism: which stack slot the by-value temporary lands in

This is the whole difference, and it is a **frame-slot allocation order**, not codegen. The two
spellings emit the *same* instruction sequence - `lwz / addi / lwz / addi / stw / stw` - and differ
only in the two offsets:

```
retail / landed:  lwz r5,0(r4) ; stw r0,0x14(r1) ; addi r4,r1,8  ; lwz r0,0(r3) ;
                  addi r3,r1,0xc ; stw r5,0xc(r1) ; stw r0,8(r1) ; bl fn_801363D4
```

**With `const&` parameters**, `end` is copied into the *named local* `ee`, which mwcceppc allocates
after the compiler-generated argument temporary for `begin` - so `end` lands in the **higher**
slot (+0xc). **With by-value parameters**, both are compiler-generated call-argument temporaries
laid out in argument order, so `end` lands in the **lower** slot (+8), which is retail's.

So the last run's ~50 body-shape spellings could not have worked: every one of them still had
`ee` as a named local, so every one of them put `end` in the higher slot. The lever was the
*parameter declarations*, and no amount of varying the body touches it.

**For the record, the by-value spelling does not stop D4 re-reading `*end` every pass**, which the
last run attributed to `end` being a non-const `It&`. That was measuring D4 alone with `It&`;
with *both* by value the reload at 0x80136410 is still there and D4 is still 100.00%, because
mwcceppc passes the class-type by value *as an address* and the loop body writes through it.
`It&` (non-const) and `It` by value are interchangeable for D4: both give 100.00%.

### Measured this run (all real builds; 9C score, D4 held at 100.00% throughout)

| `fn_8013639C` parameters | `fn_801363D4` parameters | 9C |
|---|---|---|
| **`It b, It e`, forward straight** | **`It b, It e`** | **100.00** |
| `It b, It e`, `It ee(e); fn_801363D4(b, ee);` | `It b, It e` | 99.71 |
| `const It& b, const It& e` + `It ee(e);` (the last run's landing) | `It b, It& e` | 99.71 |
| `It b, It& e`, forward straight, no local | `It b, It e` | 78.43 |
| `const It& b, const It& e`, `It ee(e); It bb(b);`, call `(bb, ee)` | `It b, It e` | 87.12 |
| `const It& b, const It& e`, `It bb(b); It ee(e);`, call `(bb, ee)` | `It b, It e` | 85.75 |
| `It& b, It& e`, `It ee(e); It bb(b);`, call `(bb, ee)` | `It b, It e` | 74.75 |
| `const It& b, It e`, `It ee(e);` | `It b, It e` | 87.12 |
| `const It b, It e`, 9C const& + `ee` then `bb` | - | 87.12 |
| `It b, It e` (9C by value), `It ee(e); It bb(b);` | `It b, It e` | 87.12 |
| `const It& b, const It& e` + `ee` | `It b, It& e` (two locals) | 92.87 |
| `const It& b, const It& e` + `ee` | `const It b, It e` | 87.12 |

Every 9C spelling that keeps a **named** local in 9C is 74-99%, and every one that passes its
arguments straight through with **no** local is either 100% or a 78% short forwarder that does not
re-copy. Those are the two branches; only the by-value/no-local one is retail.

Two spellings from the last run re-measured and **unchanged**: `It b, It& e` for D4 is still
100.00%, and its `const It& b, const It& e` remains 91.67% for D4. So D4 alone does not need
by-value - it is 9C that does.

### Rejected: the 99.86% trap is still a trap

The last run's `destroy_pair(CJointDescriptionIterator& e, CJointDescriptionIterator b)` shape is
**not** reinstated by this change and is still wrong: its parameters are `(e, b)`, so when the
compiler inlines it D4 is entered with r3 holding the end and r4 the begin, and the loop starts at
`end` and walks forward looking for `begin`. The landed spelling passes the two iterators to D4 in
their own order. The score similarity (99.86% against 99.71%) is a coincidence of frame offsets.

## The other two sub-100% functions: also measured, also walls (new this run)

Neither was touched by the last run, so both were measured from scratch here. Both are **pure
register-numbering differences** against otherwise identical instruction sequences.

**`SetActive` (94.23%, 208 bytes).** Ours and retail differ only in which callee-saved register
holds `this` / `mgr` / `active` / the loop counter / the byte offset, plus one instruction of
prologue ordering (`mr r30,r5 / mr r28,r3 / clrlwi r26,r5,24 / mr r29,r4` against our
`mr r28,r5 / mr r26,r3 / mr r27,r4 / clrlwi r30,r5,24`). Thirteen spellings, none better than the
existing 94.23%: nested `if`s instead of `&&` (94.23), `if (entity == nullptr) continue;`
(94.23), `!(a == b)` (94.23), a named `const bool act` (91.69), `(active ? true : false)` (87.98),
hoisted `const int count` (90.15), hoisted `data()` (88.08), a named `bool wasActive` (93.08),
`const bool cur` + `continue` (93.08), `while` loop (90.15), `GetCollisionDescFromIndex(i)`
(79.85), `if (active)` before `SetActive` (62.46), `(int)size()` (94.23), `CStateManager& smgr = mgr`
(94.23), a named `TUniqueId actorId` (94.17), `mActive = active` after the loop (90.48). The 208-byte
body is right; only the allocation is not.

**`GetCollisionDescIndexFromUniqueId` (96.05%, 76 bytes).** Ours and retail are instruction-for-
instruction identical apart from register numbers: retail uses `r7`/`r8`/`r5`/`r6` where ours uses
`r6`/`r7`/`r5`/`r3`. Twenty spellings, none better: a named `count` local (95.53), `uint`/`size_t`
`i` (93.16), a pointer-arithmetic index (96.05), `while` (96.05), an explicit `static_cast` on
`id` (96.05), a named `wanted` (95.79), `(uint)i < size()` (93.16), `&*begin()` (96.05), every
permutation of the declaration order of `items`/`count`/`i`/`offset` (95.53-96.05), a two-element
`const_iterator` walk (46.53), `begin()` (96.05), `const` pointer (95.53). Note the `(int)sizeof`
casts in the landed source are load-bearing and were kept: the last run measured the uncast form at
46.05%.

**`__ct__22CCollisionActorManager` (30.71%, 3024 bytes)** is the unit's remaining bulk and was not
attempted; it is the next thing with real room.

## What still stops the flip - unchanged, and not caused by this change

`tools/flip_test.sh MetroidPrime/CCollisionActorManager.cpp` -> FAIL:

```
### mwldeppc.exe Linker Error:
#   undefined: 'lbl_804191A8'
```

`extern "C" TAreaId lbl_804191A8[2];` (line ~435) is a bare `extern` **declaration**; retail's
object owns those 8 bytes of `.sbss`, so when this unit is linked our object must *define* them and
nothing in the tree does. **Verified pre-existing** on the clean tree (it reproduces with this
change reverted), and it arrived with `babd7383 progress: progress-unit-ccollisionactormanager`.
A tentative definition (`TAreaId lbl_804191A8[2];`, which mwcceppc puts in `.sbss`) is the likely
fix; it is left alone here because it is an unrelated change to this item's diff and, on its own,
raises no count.

`tools/unit_fit.sh MetroidPrime/CCollisionActorManager.cpp` - the flip needs three further
independent things, none attempted:

```
.text      claimed   7480   ours   7668   over by 188
.ctors     claimed      4   ours      0   SHORT by 4
.rodata    claimed      8   ours      7   SHORT by 1
.sbss      claimed      8   ours      5   SHORT by 3
.sdata2    claimed     36   ours     32   SHORT by 4
.sdata     claimed     -   ours     40   NOT CLAIMED by splits.txt
extra: +  100 destroy<pointer_iterator<CJointCollisionDescription,...>>__4rstl...
       +   92 __dt__26CJointCollisionDescriptionFv
       +   80 __dt__Q24rstl66basic_string<c,...>Fv
```

The `.text` over-run is the same 188 bytes as before this change - the two functions are the same
152 bytes and the header's extra outlined `destroy` is unchanged. `.sdata2` is still 4 bytes short;
the `2.0f` flag remains the most concrete lead into `__ct__`.

## Placement and decl-order gate - unchanged and still fine

Both functions sit between `CCollisionActorManager::CCollisionActorManager` and
`CCollisionActorManager::~CCollisionActorManager`, which is where mwcceppc's reverse emission puts
them in retail's `.text` order. `python3 tools/check_decl_order.py --unit
MetroidPrime/CCollisionActorManager` -> "ok: 1 unit(s) checked, none emits its functions out of
retail order". Changing the parameter *declarations* did not move either function.

## Honest limitation

Neither function is called from our own `~vector`: the header's `rstl::destroy` still outlines its
own 100-byte local copy right after `~vector` in `.text`, exactly where it was, so the object
contains the header's copy **and** the two functions that reproduce retail's. Pointing the header
at these two instead means editing `include/rstl/vector.hpp`, which moves `~vector` in every unit
that has one and has to satisfy "no function anywhere gets worse" across the whole tree. That is a
tree-wide item and is not this one; it is not filed as a `NEW:` line because its own success
condition is not yet known to be reachable.

## Verification actually run

```
./tools/decomp_build.sh main/MetroidPrime/CCollisionActorManager
  All:  35.32% fuzzy, 29.14% matched, 12.91% linked (12497 / 28465 functions)
  main/MetroidPrime/CCollisionActorManager: 71.79% fuzzy, 55.78% matched (27 / 30 functions)
objdiff-cli diff -1 build/G2ME01/src/MetroidPrime/CCollisionActorManager.o
                -2 build/G2ME01/obj/MetroidPrime/CCollisionActorManager.o
  fn_8013639C  OURS match=100.0000 size=56   (RETAIL match=100.0000 size=56)
  fn_801363D4  OURS match=100.0000 size=96   (RETAIL match=100.0000 size=96)
sha1sum build/G2ME01/main.dol                    -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/decomp_build.sh (full)                   -> exit 0, All: line present, sha1 unchanged
./tools/probe_sources.sh                        -> 762 files, 0 failed, 0 errors; LINKED (291 undefined, 0 duplicates)
python3 tools/check_symbol_names.py             -> checked 525 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit MetroidPrime/CCollisionActorManager -> ok
./tools/unit_fit.sh MetroidPrime/CCollisionActorManager.cpp                 -> see above
./tools/flip_test.sh MetroidPrime/CCollisionActorManager.cpp -> FAIL (the lbl_804191A8 link error)
./tools/goal_check.sh build/goal/item.json      -> PARTIAL, "target rose: 26 -> 27 / 30", "no asm added"
```

Note on running the judge: the lane's shell inherits `MP_GOAL_TREE` / `MP_GOAL_JUDGE` /
`MP_GOAL_BASE` / `MP_GOAL_LANE` from a **different** lane's unit, so a bare `goal_check.sh` grades
this tree against another worktree's baseline and reports that other item's id. Run it with those
unset (`env -u MP_GOAL_TREE -u MP_GOAL_JUDGE -u MP_GOAL_BASE -u MP_GOAL_LANE ...`) or it will
report another lane's result.

`docs/HANDOFF.md`'s state block was rewritten by the gate, not by hand; the driver discards it.

WALL: SetActive 94.23% - instruction sequence identical to retail, differs only in which
callee-saved register holds this/mgr/active/i/offset plus one prologue instruction order; 15
spellings tried this run, none moved the register allocation.

WALL: GetCollisionDescIndexFromUniqueId 96.05% - same, a pure register-numbering difference
(`r3`/`r6`/`r7` against retail's `r6`/`r7`/`r8`); 20 spellings tried this run including every
declaration order of the four locals, none changed it.
