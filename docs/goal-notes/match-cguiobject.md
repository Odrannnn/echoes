# match-cguiobject — `GuiSys/CGuiObject` 19/21 -> **20/21** matched (2026-10-01, lane 7)

**Result: `GetWorldTransform` reached 100.00% (was 79.17%), `AddChildObject` 99.15% -> 99.58%.
0 functions anywhere got worse, 2 got better, no `asm`, judge PARTIAL.**
The unit stays `NonMatching` because `AddChildObject` is 4 registers short, so `flip_test.sh`
FAILs; the judge accepted the partial result (`ok target rose: main/GuiSys/CGuiObject: 19 -> 20 / 21`).

Measured, not recalled (`build/report.json`, after the change):

```
main/GuiSys/CGuiObject: 99.89% fuzzy, 74.73% matched, 20 / 21 functions
All:  32.81% fuzzy, 25.61% matched, 12.03% linked (11423 / 28465 functions)
main.dol sha1 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010 (unchanged)
probe_sources.sh -> 752 files, 0 failed, 0 errors; check_symbol_names.py -> 0 missing
unit_fit.sh -> .text claimed 3008 ours 3008 retail 3008 "fits"; no extra functions
check_decl_order.py --unit GuiSys/CGuiObject -> ok
```

Per-function diff from the judge's own baseline (`build/goal/judge/report.base.json` vs
`build/report.json`, all 28465 functions compared): **WORSE 0**, BETTER 2, no function added or lost.

## Files changed (only this one)

`src/GuiSys/CGuiObject.cpp` — two source edits plus their comments. No `configure.py` change, no
`config/` change, no `asm`, no header change.

## 1. `GetWorldTransform`: 79.17% -> **100.00%** — the store moves out of the `if` arm

This finishes the block-layout question the previous run left open (its notes: *"the thing to look
for is a source form where `mWorldTransformValid = true` is not textually in the same statement
group as the assignment"*). It is exactly that, and it is one line:

```cpp
if (mParent != nullptr) {
  mWorldXF = mParent->GetWorldTransform() * mLocalXF;
} else {
  return mLocalXF;
}
mWorldTransformValid = true;      // <-- outside the if, after both arms
```

Retail ends each of its ten inlined levels with `bl __as__ ; b <setvalid_i>` and places the
`li r0,1 ; stb r0,100(this)` block **after** the no-parent arm, so the multiply falls through into
a block the no-parent path jumps *over*. With the store textually inside the `if` arm, mwcceppc
keeps it in the same basic block as the `__as__` call and never splits it: the instruction sequence
is identical, only the `b` position and the branch displacements differ (8 bytes x 10 levels).
Hoisting the store out of the arm is what makes mwcceppc split the block.

Measured: 79.17% (108 of 696 bytes wrong) -> **100.00%**. `tools/bytescmp.py` now reports
**19 differing instructions of 174, every one of them a `bl` relocation** - objdiff ignores the
relocated field, hence 100%. No other change was needed; `inline` on the declaration and
`inline_max_size(450)` (previous run) are still required and still correct.

This also settles the item's own `reason` ("worst GetWorldTransform at 79.17%").

## 2. `AddChildObject`: 99.15% -> 99.58% — subscript with `EDim`, not `.GetX()`

One change, same elements, different spelling: in the twelve-argument `CTransform4f` constructor,
each matrix value is subscripted instead of accessored, and `pos` likewise:

```cpp
worldLocalXf = CTransform4f(
  tmpMtx.GetColumn(kDX)[kDX], tmpMtx.GetColumn(kDY)[kDX], tmpMtx.GetColumn(kDZ)[kDX], pos[kDX],
  tmpMtx.GetColumn(kDX)[kDY], tmpMtx.GetColumn(kDY)[kDY], tmpMtx.GetColumn(kDZ)[kDY], pos[kDY],
  tmpMtx.GetColumn(kDX)[kDZ], tmpMtx.GetColumn(kDY)[kDZ], tmpMtx.GetColumn(kDZ)[kDZ], pos[kDZ]);
```

`GetColumn(kDY)[kDX]` and `GetColumn(kDY).GetX()` are the same float, but they are not the same
code. `operator[](EDim)` and `GetX()` differ in how mwcceppc orders the twelve loads and which FPR
each gets.

* `.GetX()` everywhere -> 48 differing instructions (99.15%)
* `operator[](EDim)` on the nine matrix values -> 36 (99.36%)
* plus `operator[](EDim)` on `pos` -> **32 (99.58%)** — kept

**The `GetColumn(EDim)` call is what matters, not the subscript.** `GetColumn(kDY)[kDX]` and
`GetRow(kDX)[kDY]` are the same float and both are a strided read, yet replacing all nine with the
`GetRow` form collapses the function to 111 differing instructions (and `Get00()`..`Get22()` does
the same). Substituting **one at a time** shows why: only the `[kDX]` subscript (the `GetRow(kDX)`
equivalents, which are the first column) is interchangeable, because `GetColumn(kC)[kC]` and
`GetRow(kC)[kC]` are literally the same expression; the other eight lose the strided read that
makes mwcceppc build the frame slots retail builds.

### What is left: four registers, and it is a whole-block allocator decision

32 differing instructions, of which 19 are `bl`/`lis` relocations objdiff ignores. The other 13
are one contiguous register renumbering in the twelve-load block: **the load order, the store
order, the frame offsets and every mnemonic already match exactly.** Retail hands f1..f9 to the
nine matrix values and f0/f10/f11 to `pos`; ours hands f3..f11 to the matrix and f0/f1/f2 to
`pos`. The values and the offsets are identical — only the physical register number differs, for the
block as a whole.

Spellings tried **this run**, all measured, none reached 0:

* `pos` as `const CVector3f&` 57 | by value 36 | `pos[0]`/`[1]`/`[2]` 36 | `.GetX/.GetY/.GetZ` 36
  | bound to named `float` locals 182 | inline `(tmpMtx * position)[kD?]` 188
* `worldLocalXf = CTransform4f(...)` replaced by a named `CTransform4f built` temporary 84
* named `CVector3f c0,c1,c2` columns for the constructor 111
* all nine as `GetRow(kD)[kC]` 111 | all nine as `GetNN()` 111
* `scale[kDX]`/`[kDY]`/`[kDZ]` 134 | `scale[0]/[1]/[2]` 32 | `CMatrix3f tmpMtx` by value 32
* mixing the two subscript styles per argument: `GetRow(kDX)[kD?]` 32, `.GetX()` 32, `GetNN()` 32
* `GetRow` for the twelve values 111; a per-column `GetColumn(kX).GetX()` mix 32 each
* swapping two **argument positions** (m01/m02, m12/m22) gives 31 and 32, but that computes a
  *different transform* (it reorders matrix elements), so it is not a match and was rejected
* `tmpMtx * position` moved before the `CMatrix3f` construction: does not compile (`pos` undefined)

This is the same wall the previous run reached from the other side: it reported *"About thirty
spellings of the 12-argument constructor did not move it... so this is very likely not reachable
from source with the same twelve arguments."* That conclusion still holds, and the two runs'
spellings now overlap only slightly, which is why the gap shrank to four registers. **The previous
run's `WALL:` lines on the two recursive functions are superseded** — `GetWorldTransform` is 100%
and `RecalculateTransforms` was already 100%; both are finished, not walls.

WALL: CGuiObject::AddChildObject 99.58% - last 13 instructions are one contiguous FPR renumbering of the twelve-argument CTransform4f load block (identical order, offsets and values); ~25 spellings tried this run did not move it

## Codegen facts added this run (measured)

* **A statement group that ends a basic block does not need a `goto`.** Moving a store out of an
  `if` arm and placing it after both arms is enough to make mwcceppc split the block so the
  inlined recursion's levels branch over it. This is the general form of the previous run's
  `for(;;)` + `break` finding in `AddChildObject`: *where the store sits in the source decides
  whether it shares a block with the call before it.*
* `operator[](EDim)` and `GetX()`/`GetY()`/`GetZ()` are not interchangeable for register
  allocation even when they name the same member: 99.15% vs 99.58% in the same function.
* An `EDim` overload of an *inline* accessor (`operator[](EDim)`) inlines and keeps retail's
  strided frame layout; the same element reached via `GetRow(kD)[kC]` or `GetNN()` does not
  (36 vs 111 differing instructions).

## Not filed as `NEW:`

`AddChildObject`'s last four registers are a measured wall, not a queueable target: the twelve
loads, their order, their frame offsets and their values are already retail's, so no source
spelling that keeps the same twelve arguments can be expected to change the allocator's choice,
and filing it would cost a lane an hour to re-derive this. The spellings and scores are recorded
above instead.