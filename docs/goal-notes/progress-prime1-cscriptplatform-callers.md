# progress-prime1-cscriptplatform-callers

`MetroidPrime/ScriptObjects/CScriptPlatform` — progress item; the unit remains `NonMatching`.

## Result (first run)

The unit rose from **29/60 to 30/60 matched functions**; tree matched count rose **10313 -> 10314**, linked stayed **5048**. The judge reported `+1` target function and no regressions.

`BuildSlaveList` (0x800A2934, 468 B) is now **100.00%**. It reserves static-slave storage, resolves `PLAY/ACTV` connections into actors and their relative translations, and records `IBND/ACTV` trigger IDs. Retail's existing `fn_800A46F0`/`fn_800A14DC` helpers are used.

`AddRider(vector)` (0x800A38D0, 660 B) is implemented with duplicate lookup/timer update, rider transform and `XONP` notification, and reserve/push. It measures **97.73%**, so it does not yet count. Remaining differences are compiler output: iterator register allocation, one FP instruction scheduling/store order, and message/optional-timer register allocation. Variants measured this run: the `rideePos` nested transform scored 92.71%; direct transform temporary scored 96.43%; adding the explicit second ridee-null check scored 97.73% (kept). A hand-written search loop scored 92.94% and was discarded.

## Verification (first run)

- `./tools/goal_check.sh build/goal/item.json` -> PASS: matched **10313 -> 10314**, linked **5048 -> 5048**, target **29 -> 30/60**, no asm added; gate, symbol-name check, and All count all passed.
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptPlatform` -> no functions out of retail order.
- `./tools/fast_try.sh MetroidPrime/ScriptObjects/CScriptPlatform` measured `BuildSlaveList` 100.00% and `AddRider(vector)` 97.73%.

---

# Second run (2026-10-01, lane 4)

## Re-measured first, and the item's `reason` is stale

The unit was **already 41/60** on this tree, not the 30/60 the first run left it at: four later
items landed on it (`...-dtor`, `...-ridervec`, `...-slavevec`, `...-erase-pair`), taking it
41 -> 42 with this change. Both functions `reason` names as "still the TODO bodies" were already
written when I started: `BuildSlaveList` is at **100.00%** and `AddRider(vector)` at **99.61%**.
The item was still live, so I did the rest rather than writing `STALE:`.

Unmatched on entry, all from `./tools/fast_try.sh`: the constructor (42.52%, 1388 B),
`AdvanceMotionTime` 0.92%, `AddRider(vector)` 99.61%, `DecayRiders` 1.33%, `MoveRiders` 0.45%,
`fn_800A31A0` 93.45%, `PreThink` 0.24%, `DragSlave` 0.54%, `DragSlaves` 0.83%, `Think` 0.75%,
`SetMotionTime` 2.49%, `TeleportToWaypoint` 2.50%, `fn_800A1D4C` 0.00%, `fn_800A1CE8` 0.00%,
`AcceptScriptMsg` 1.98%, `UpdateSlaveTransforms` 1.79%, `Move` 1.58%, `fn_800a0200` 1.72%.
**`AddSlave` at 99.86% was the only one that was a near miss rather than a TODO body.**

## Result: +1 function

`AddSlave` (0x800A1330, 428 B) is now **100.00%**. Unit **41 -> 42 / 60**; tree matched
**11346 -> 11347**; linked **5507 -> 5507**. `./tools/goal_check.sh build/goal/item.json` ->
**PASS**, every check green, no asm added.

The change is two lines in the `else` branch: bind the element the find loop returned to a named
reference, then assign the timer through it.

```c
    SRiders& found = *slave;
    found.mDecayTimer = decayTimer;
```

Why it is the register allocation and not the semantics: the `optional_object<float>` copy
assignment emits a self-assignment guard, `addi rX,rX,4` / `cmplw rX,r31` - `&lhs != &rhs`. Written
inline, MWCC uses one register for both the guard temporary and the left-hand side, so the found
pointer has to be reloaded after the find loop's exit test. Ours was `lwz r0,32(r1)` at the exit
test and then a *second* `lwz r3,32(r1)` at the top of the found block; retail loads it once, into
r3, at 0x800A13C8 and keeps it in r3 across the branch at 0x800A13D0. Naming the element gives the
allocator the second live range it needs.

Spellings measured this run on this body, differing instructions against the retail-derived object
with branch targets and `bl` operands normalised (`tools/bytescmp.py` does not normalise, so it
reports more; these counts are the ones that decide the match):

| spelling | differing instrs |
| --- | --- |
| `SRiders& found = *slave; found.mDecayTimer = ...` (kept) | **0** |
| `SRiders* found = &*slave; found->mDecayTimer = ...` | **0** |
| `(*slave).mDecayTimer = decayTimer;` (the old body) | 10 |
| `slave->mDecayTimer = decayTimer;` | 17 |
| `optional_object<float>& slot = slave->mDecayTimer; slot = ...` | 10 |
| `SRiders& found = *slave; CTransform4f xf = ...; rider.mTransform = xf;` (AddRider shape) | - |
| early-return shape of the whole function | 40 |
| `else if (CActor* ...)` instead of the nested `if` | 40 |
| hand-written search loop over `mDynamicSlaves` | 72 |
| hoisting `mDynamicSlaves.end()` into a local and comparing against that | 50 |

**This does not transfer to `AddRider`.** Its `else` branch is the same assignment, but there
*retail* reuses r3 for the guard exactly as we do (`addi r3,r3,4` / `cmplw r3,r31` at 0x800A3B04),
so the inline form is already the right one there. Measured on `AddRider`: the named reference 21
differing (worse), a named pointer 21, `it->` 17, the inline form 14 (kept). Do not change the two
branches together.

## `AddRider(vector)` - still 99.61%, and why it did not move

The remaining 14 differing instructions are all register swaps inside the two `DeliverScriptMsg`
blocks and nothing else. Block 1 (0x800A3A50): retail `lhz r7,8(r28)` + `lhz r8,<const>`, we do
`lhz r8,8(r28)` + `lhz r7,<const>`; block 2 (0x800A3AA0) the same with r6/r7. The values, the
store offsets and the instruction count all agree - only which of r6/r7/r8 holds which of the two
sources differs. Spellings tried, all worse or equal: naming the `CScriptMsg` in a local (32),
naming the message id (14, same as baseline), hoisting the three ids into locals (73), building one
`CScriptMsg` before the branch and assigning into it (71), binding the found element to a reference
or pointer (21). The first run's spellings (92.71%, 96.43%, 92.94%) are still the record for the
rest of the function.

WALL: AddRider(vector) 99.61% - the last 14 differences are register swaps in the two
DeliverScriptMsg argument blocks; 10 spellings of the message construction were measured this run
and none reached 0.

## `fn_800A31A0` - the four-store shape, and the wall

Retail (0x800A31A0, 132 B) stores **four** words before its `bl fn_800A1148`: `[12]` and `[8]` both
hold the end pointer, `[16]` and `[20]` both hold `mItems`, and the call passes `r3 = r1+20`
(`&mItems` copy), `r4 = r1+12` (`&end` copy). The comment in the file already claimed this shape
was produced by passing both ends by value; on this tree it was not, and the body in the tree emits
only two stores (`r3 = r1+8`, `r4 = r1+12`).

Getting four stores needs the *callee's* by-value parameters to be materialised in the frame, which
MWCC only does when their addresses are taken - and then only in the layer that takes them. Two
spellings reach four stores (the rest emit 0-3): an `inline` wrapper taking `SRiders* const*` and
copying into fresh locals, called with the two addresses; and the same wrapper taking `SRiders*`
**by value** and taking `&param` inside. Best measured, 5 differing instructions of 33:

```c
static inline void fn_800A31A0_destroy(SRiders* const* last, SRiders* const* first) {
  SRiders* localFirst;
  SRiders* localLast;
  localLast = *last;
  localFirst = *first;
  fn_800A1148(&localFirst, &localLast);
}
// caller: SRiders* firstItems; SRiders* lastItems;
//         lastItems = self->mItems + self->mCount; firstItems = self->mItems;
//         fn_800A31A0_destroy(&lastItems, &firstItems);
```

That is 5 from retail and no closer. The five are all slot and register assignment: ours puts
`mItems` at `[8]` and `[20]` and the end at `[12]` and `[16]`, retail puts the end at `[8]`/`[12]`
and `mItems` at `[16]`/`[20]`, and retail computes the end into r5 (`add r5,r5,r0`) where we use r0.
MWCC allocates the four stack objects in **source declaration order**, and no ordering of two
caller locals plus two wrapper locals yields retail's interleaving - that needs the end's two slots
allocated before the first's two. Also measured and worse: a by-value wrapper with the two
parameters reversed (7), with the locals declared in the other order (6/7/8), three chained layers
(7), a `SRiders* p[2]` or a two-word struct inside the wrapper (17 - the wrapper stops being
inlined and becomes an extra symbol), the old `static` forwarder (6), and a direct
`fn_800A1148(&firstItems, &lastItems)` (6, two stores).

WALL: fn_800A31A0 93.45% - four stores are reachable but the slot assignment is not; 20 spellings
of the wrapper and its call were measured this run and the best is 5 differing instructions.

## What is left (measured, for whoever picks this up)

Sixteen functions are still unwritten TODO bodies: the constructor (42.52%, 1388 B), `PreThink`
(0.24%, 1688 B), `Move` (1.58%, 2088 B), `AcceptScriptMsg` (1.98%, 1608 B), `MoveRiders` (0.45%,
888 B), `DragSlave` (0.54%, 736 B), `Think` (0.75%, 536 B), `DragSlaves` (0.83%, 484 B),
`AdvanceMotionTime` (0.92%, 436 B), `DecayRiders` (1.33%, 300 B), `SetMotionTime` (2.49%, 304 B),
`fn_800a0200` (1.72%, 232 B), `UpdateSlaveTransforms` (1.79%, 224 B), `TeleportToWaypoint` (2.50%,
160 B), and the two retail functions that are **absent from our source entirely**:

- `fn_800A1CE8` (0x800A1CE8, 100 B) and `fn_800A1D4C` (0x800A1D4C, 172 B) sit between `fn_800A1CA0`
  and `fn_800a1df8` in retail's object, so they belong in the source between this file's
  `fn_800a1df8` and `fn_800A1CA0`, in the order `fn_800A1D4C` then `fn_800A1CE8` (mwcceppc emits in
  reverse source order). Nothing in the DOL calls either - they are reached from a REL - so their
  owning class is still unknown. What the bytes say: `fn_800A1D4C(r3 = self, r4 = flag)` is a
  `vector<T>::~vector(int)` whose element size is 0x50 and whose element has a **virtual**
  destructor (vtable slot 1 via `mtctr`/`bctrl` at 0x800A1DA8); `fn_800A1CE8` is the destructor of
  a class deriving from a polymorphic base - it stores a vtable pointer (0x800332B0) at
  `self[0]` and destroys a second `vector` of 0x50-byte polymorphic elements at `self+4`. Both are
  `vector<SRiders>`-shaped (`mCapacity`@0, `mCount`@4, `mItems`@0xc) and both need the same
  four-store `destroy(begin, end)` shape as `fn_800A31A0`, so **the `fn_800A31A0` wall above blocks
  them too** - worth attacking as one problem, not three. Recovering the element type is the other
  half; I did not get far enough to name it, so this is a characterisation, not a recipe.

No `NEW:` filed: every function left in this unit is either a TODO body in a unit the queue already
targets, or blocked on the `fn_800A31A0` slot-assignment wall, which is a measured wall rather than
a new piece of work.

## Verification (second run)

- `./tools/goal_check.sh build/goal/item.json` -> **PASS**: matched **11346 -> 11347**, linked
  **5507 -> 5507**, target **41 -> 42/60**, no judge-owned path touched, no asm added; gate
  (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe), `check_symbol_names.py` and
  the `All:` line all green.
- `./tools/fast_try.sh MetroidPrime/ScriptObjects/CScriptPlatform` -> 42/60, `AddSlave` no longer in
  the sub-100% list.
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptPlatform` -> ok, none
  out of retail order.
- `./tools/unit_fit.sh MetroidPrime/ScriptObjects/CScriptPlatform.cpp` -> the 32 "extra" functions
  are the pre-existing weak `__dt__`/`__ct__` template instantiations; the change adds no symbol.

## One lesson worth carrying (not a `NEW:`)

When a member assignment through a found iterator is short by a few instructions, look at the
`rstl::optional_object<T>` copy-assignment guard `addi rX,rX,4` / `cmplw rX,rB` - `&lhs != &rhs`
before suspecting the search loop. Binding the *found element* to a named reference or pointer on
the line before the assignment is a one-line change that can be worth a whole function, and it
costs nothing when retail reuses the register instead (check which way retail went first: in
`AddSlave` retail keeps the pointer in r3, in `AddRider` it does not).
