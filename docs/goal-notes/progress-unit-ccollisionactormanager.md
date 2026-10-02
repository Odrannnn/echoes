# progress-unit-ccollisionactormanager (lane 2, 2026-10-02)

`MetroidPrime/CCollisionActorManager`: **24 -> 25 / 30 functions matched** at 100%, unit fuzzy
69.39% -> **69.75%**, `matched_code` 53.58% -> **53.74%**. `All:` held at 35.11% fuzzy /
28.82% matched / 12.90% linked, 12423 -> 12424 functions. `tools/goal_check.sh
build/goal/item.json` -> **PASS** (gate.sh, DOL sha1 `6ef9b491...`, 86 RELs, report diff,
wiring, docs claims, port probe, `check_symbol_names.py`, "target rose: 24 -> 25", "no asm
added"). Unit stays `NonMatching`; nothing was flipped and nothing was committed.

## What landed

**1. `fn_801358B4` -> 100%** (0.00% -> 100.00%, 12 bytes). This is the whole of the
`matched_functions` rise. Retail 0x801358B4 is three instructions -
`lwz r0,-27736(r13) ; stw r0,-27608(r13) ; blr` - a copy of `kInvalidAreaId` (`.sbss`
0x80419128) into this unit's own zeroed `.sbss` word at 0x804191A8. It is the **first**
definition in retail's `.text` for this unit, so with mwcceppc's reverse emission order it is
the **last** definition in retail's source file; declaring it last here puts it at `.text`
offset 0 where retail has it.

Three things had to be true at once, all measured:

- **The name must be retail's.** A `static void ClearAreaId()` emitting the identical three
  instructions at `.text` offset 0 changed **nothing** (unit stayed 69.39%, 24/30). objdiff
  pairs the base and target symbol tables **by name**; it does not pair an unnamed 12-byte
  function positionally. Declaring it `extern "C" void fn_801358B4()` (retail's symbol is
  C-linkage) is what makes it pair, and the unit went straight to 25/30.
- **The destination must be named for retail too.** The target store is an `R_PPC_EMB_SDA21`
  against `lbl_804191A8`, and the relocation has to carry the same symbol name or the word
  does not compare equal. Declared as `extern "C" TAreaId lbl_804191A8[2];` - two `TAreaId`s
  is exactly the 8 bytes of `.sbss` retail's object owns for this unit
  (`build/report.json` -> this unit's sections), so the object's `.sbss` size is right too.
  Nothing in the DOL reads or writes the second word.
- **A `static` function is emitted even when nothing calls it.** mwcceppc keeps
  `static void ClearAreaId()` and its `static TAreaId`; that was not the blocker.

What the function is *for* is not recoverable from the bytes - `grep 'bl 801358b4'` over
`build/G2ME01/main.elf` finds no caller, and it is the only reference to `lbl_804191A8` in
retail's object. The body does exactly what retail's three instructions do and adds no work.

**2. `GetCollisionDescIndexFromUniqueId` 75.79% -> 96.05%** (76 bytes). This one does **not**
move `matched_functions`; it is the fuzzy gain only, and it is in the diff because the item's
reason names this function. Retail (0x80135A88) reads `mItems` **once, before the loop**
(`lwz r5,12(r3)`) and then uses **two** induction variables: a byte offset `r6` for the
address and an element counter `r8` for the return value, fetching with
`addi r3,r6,60 ; lhzx r3,r5,r3`. Ours re-read `12(r3)` inside the loop. Spellings measured
(this run, all with `tools/fast_try.sh`, objdiff `fuzzy_match_percent`):

| spelling | score |
|---|---|
| `mJointDescriptions[i]` (unchanged baseline) | 75.79% |
| `const rstl::vector<..>& v = mJointDescriptions;` then `v[i]` | 75.79% |
| `vector* v = &mJointDescriptions;` then `(*v)[i]` | 75.79% |
| `const int count = size();` then `mJointDescriptions[i]` | 75.79% |
| `mJointDescriptions.data()[i]` inline | 75.79% |
| `mJointDescriptions.at(i)` | 75.79% |
| `items[i]`, `items` a `data()` local | 83.95% - MW strength-reduces to a **pointer** IV, 17 instructions, 2 short of retail's 19 |
| `items[i]`, `items` a `data()` local, `const int count` too | 83.95% |
| `items` + explicit byte offset, `++i, offset += ..` | 95.95% |
| `items` + explicit byte offset, increment order swapped | **96.05%** |
| same, `offset` declared before the `for` | 96.05% |
| same, `const int count` instead of `size()` in the test | 96.05% |
| same, `const TUniqueId target = id;` hoisted first | 74.47% |
| same, `const CJointCollisionDescription* const items` | 95.53% |
| same, `uint offset` | build error (`uint`/`int` mix) |
| same, **no `(int)` casts** on `sizeof` | 46.05% - the division's type changes and the offset lands elsewhere |
| explicit byte offset divided out of a `const char*` cast | mwcceppc `expression syntax error` on the C-style cast |

The landed form is the "increment order swapped" row. Still 18 of 19 instructions: every one
is present and in retail's order except the `li <offset>,0`, which ours places before the
`lhz r0,0(r4)` and retail after, and the register **numbering** - retail `r7` count / `r8`
index / `r5` base / `r6` offset / `r3` temp against ours `r6` / `r7` / `r5` / `r3` / `r4`.
Note the `(int)` cast is load-bearing: the identical source without it is 46.05%, so this is
not a spelling to "clean up" later without re-measuring.

**3. `SetActive` left alone.** It is 94.23% (208 bytes) and the whole difference is register
allocation - the instruction sequence is identical instruction for instruction, and the six
callee-saved values are the same six under a cyclic shift by two (retail `r28`=this,
`r29`=mgr, `r30`=active, `r26`=active?1:0, `r31`=i, `r27`=byte offset; ours `r26`, `r27`,
`r28`, `r30`, `r29`, `r31`). Measured, none of which moved it past 94.23%: Prime 1's nested
`if (entity) { if (active != act->GetActive()) { ... } }` form; `if (active != false)`;
`const bool isActive = active;` rewritten through the local; `uint i`;
`for (int i = 0, offset = 0; ...)` with `mJointDescriptions[offset / sizeof(...)]`
(94.38%, i.e. +0.15, and it breaks the increment order retail has - offset then index - so it
is not worth the contortion); the same with the increments swapped (79.00%, much worse);
`const bool newActive = active;` (91.69%).

WALL: GetCollisionDescIndexFromUniqueId 96.05% - 19 of 19 instructions present and in retail's
order but for the `li <offset>,0` placement; what is left is the register the byte offset
lands in (ours `r3`, retail `r6`), and 16 spellings this run all put it in `r3` or drop two
instructions.
WALL: SetActive 94.23% - identical instruction sequence, cyclic-by-two relabelling of the six
callee-saved values; eight spellings (nesting, `!= false`, bool/uint locals, `at()`, an
explicit byte offset both ways) did not reach it.

## What is left in this unit, and why it is not a flip

`unit_fit.sh MetroidPrime/CCollisionActorManager.cpp`: `.text` over by 36 bytes, `.ctors` short
4, `.rodata` short 1, `.sbss` short 3, `.sdata2` short 4, `.sdata` 40 bytes not claimed, and
three functions in ours that retail's object does not define (`destroy<pointer_iterator<...>>`
100 B, `__dt__CJointCollisionDescription` 92 B, `__dt__basic_string` 80 B). The flip is far
away; this item only asked for the count.

`fn_8013639C` (56 B) + `fn_801363D4` (96 B) are retail's out-of-line `rstl::destroy` chain
for `vector<CJointCollisionDescription>`, and the shape is now fully characterised:

- `fn_801363D4` is `destroy_impl` with **`const It&` parameters** - it does `lwz r31,0(r3)`
  once for the first element and then **re-loads `0(r30)` every iteration** for the end
  (0x80136410), because the loop body calls `internal_dereference` and MW cannot prove the
  reference is unchanged. Ours loads both ends once into `r30`/`r31`, spills them to the frame
  and compares registers - 26 instructions to retail's 24.
- `fn_8013639C` is the `destroy` that forwards: it copies `*first`/`*last` into **its own**
  frame (`stw r5,8(r1) ; stw r0,12(r1)`, 0x801363B8-0x801363BC) and passes their addresses,
  which is what you get when `destroy` takes its iterators **by value** and `destroy_impl`
  takes them **by reference**. `~vector` likewise passes addresses of its own temporaries
  (0x80136350-0x80136364).
- The loop body destroys through `internal_dereference__Q24rstl66basic_string<...>Fv` at
  `elem + 44` (the `mName`), stride 104, **inlined** - retail's object defines no
  `__dt__CJointCollisionDescription` at all, while ours emits one.

Two independent blockers, and the second is the expensive one:

1. objdiff needs the literal names `fn_8013639C` / `fn_801363D4`, and these are *template
   instantiations*. Getting them means either hand-writing two `extern "C"` functions with
   retail's names in this `.cpp`, or changing `rstl/construct.hpp`'s
   `destroy`/`destroy_impl` so the instantiations come out outlined and unnamed.
2. Changing `include/rstl/construct.hpp` changes `~vector` in **every** unit that has one,
   which is exactly the "no function anywhere gets worse" condition. It is a
   whole-tree change, not a one-unit change, and out of scope for a `progress` item on this
   unit.

## Smaller things measured, not acted on

- Retail's `.sdata2` for this unit is 36 bytes against our 32, and the extra constant is
  **`2.0f`** (`0x40000000` at offset 0x18; ours has `0x3f000000` at 0x04 where retail has it
  at 0x08, so the order differs too). `__ct__` is at 30.71% and this is the most concrete
  lead into it - some `2.0f` the constructor needs and this source does not have.
- `__ct__` is also where the other 2000-odd bytes are; its 30.71% is not a register-allocation
  problem and was not attempted beyond the above.
- The 8-byte `.sbss`: `lbl_804191A8` is 8 bytes and only its first word is referenced
  anywhere. Whether retail had one 8-byte object there or two 4-byte ones is not
  distinguishable from the bytes; the array declaration matches the size either way.

## Verification actually run

```
./tools/decomp_build.sh main/MetroidPrime/CCollisionActorManager
  All:  35.11% fuzzy, 28.82% matched, 12.90% linked (12424 / 28465 functions)
  main/MetroidPrime/CCollisionActorManager: 69.75% fuzzy, 53.74% matched (25 / 30 functions)
sha1sum build/G2ME01/main.dol        -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py  -> checked 525 units; 0 declared names are missing
./tools/unit_fit.sh MetroidPrime/CCollisionActorManager.cpp
./tools/goal_check.sh build/goal/item.json -> PASS
```

`docs/HANDOFF.md`'s state block was rewritten by the gate, not by hand; the driver discards
it.

NEW: destroy-pair-ccollisionactormanager | match | MetroidPrime/CCollisionActorManager | retail's
fn_8013639C (56 B) + fn_801363D4 (96 B) are the outlined rstl::destroy/destroy_impl pair for
vector<CJointCollisionDescription>; matching them needs `destroy` to take its iterators by
value and `destroy_impl` by const reference (so the end pointer is re-loaded each iteration)
in include/rstl/construct.hpp, which is a shared-header change that moves ~vector in every
unit - budget it as a tree-wide item, not a one-unit one.
