# progress-prime1-cscriptdoor

`kind: progress`, target `MetroidPrime/ScriptObjects/CScriptDoor`. Unit stays `NonMatching`; not a
flip, so `flip_test.sh` was not used to decide anything. All figures below come from
`build/report.json` after `./tools/decomp_build.sh` (not recalled).

## Result

| | before | after |
|---|---|---|
| unit `matched_functions` | **11 / 35** | **12 / 35** |
| unit `fuzzy_match_percent` | 64.86% | 65.77% |
| unit `matched_code` | 948 B | 1172 B |
| DOL `matched_functions` | 8465 | 8466 |
| `All:` line | 30.44% fuzzy, 22.36% matched, 11.74% linked | identical (unchanged) |

`SetDoorAnimation__11CScriptDoorFQ211CScriptDoor13EDoorAnimType` is now **byte-identical** to retail
(verified with `objdump -s` on both objects: `IDENTICAL`).

## The five functions the item named

Prime 1's `src/MetroidPrime/ScriptObjects/CScriptDoor.cpp` was read for each. Echoes' engine is a
fork, so Prime 1's bodies mostly do not apply; what did transfer was *shape* of expression, not code.

### 1. `SetDoorAnimation` - 89.89% -> **100%** (matched)

Prime 1's version is `mDoorState = state;` with a `static_cast<int>(state)` animation id - the whole
`switch` over the four `m*Animation` members is Echoes-only, so Prime 1's source could not be reused.
Two edits, both forced by the measured bytes:

* The switch temp must live in **r0**, so it has to die at `mAnimationId = animationId` and the
  `CAnimPlaybackParms` must be built from the **member** `mAnimationId`, not the local. Passing the
  local kept the value live across the `HasAnimation()` test and the allocator picked **r8**; then
  retail's `li r0,0` position and the `lwz r4,944(r3)` reload of `mAnimationId` before
  `SetAnimation` were both missing. With the local used only for the store, all of it appears.
* **Case order in the source is the code layout order.** Retail emits the case bodies
  `mClosingAnimation, mOpeningAnimation, mClosedAnimation, mOpenAnimation`, i.e. the source order
  `kDAT_Closing, kDAT_Opening, kDAT_Closed, kDAT_Open` (enum values 1, 2, 3, 0). Our source listed
  them Open/Closing/Opening/Closed, which reproduced the *same* instructions in a different order -
  99.57%, not 100%. Reordering the `case` labels made it exact. (Same rule as the `flip` note about
  declaration order, applied to `switch` bodies.)

Attempted and discarded: assigning `mAnimationId` directly in each case (no `li r0,0`, store
duplicated per branch, 88.41%); keeping the local and its current use (r8, 89.89%).

### 2. `Think` - 92.06% -> 98.82% (still not matched)

Three measured fixes, each visible in the disassembly:

* **`mgr.World()` is called four times in retail, not once.** Retail has four `lwz rX,5636(r29)`
  (one per use); we had one, held in a callee-save register across the whole `kDS_WaitingForArea`
  block. Deleting the `CWorld* world` local (calling `mgr.World()` at each of the four sites) also
  fixed the callee-save register set: after it, both objects use r27-r31 with identical roles
  (r28=this, r29=mgr, r30=dock, r31=chain-head base, r27=iterator/loop). Before it ours saved
  r26-r31 and shifted every named register by one. 92.06% -> 95.69%.
* **The `kDS_CloseDelay` branch is written the other way round.** Retail tests
  `mOpenRequestCount == 0` with `beq` over the `if` body and *falls through* into
  `SetDoorState(mgr, kDS_Open)`; ours tested with `bne` over the `else`. Rewriting
  `if (==0) { if (<=0) Closing; } else { Open; }` as
  `if (!=0) { Open; } else if (<=0) { Closing; }` is semantically identical and reproduces retail's
  layout. 95.69% -> 98.82%.
* **Remaining delta is one instruction in the final `for (i < mgr.GetNumPlayers())` loop.** Retail
  strength-reduces `&mgr->m_playerStates[i]` to an induction pointer seeded with `mgr`
  (`mr r30,r29` ... `lwz r5,5388(r30)` ... `addi r30,r30,4`); ours seeds the IV with 0 and rebuilds
  the address (`li r30,0` ... `addi r0,r30,5388` ... `lwzx r5,r29,r0`), one instruction longer
  (1296 B vs 1292 B). This is an instruction-selection difference, not expressible from the source.

Attempted for that last instruction: hoisting `mgr.GetNumPlayers()` into a local (98.59%, worse);
hoisting `mgr.GetPlayerState(i)` into a local (no change); iterating an explicit
`const CPlayerState* state` alongside the index (99.42% but **wrong** - the compiler then assumes
`CPlayerState` objects are contiguous and emits `addi rPtr,rPtr,1588`; discarded, not kept).

### 3. `IsConnectedToArea` - 76.86% -> 80.39% (still not matched)

The trailing `return areaDock.GetConnectedAreaId(...) == area;` compiled to the branchless
`subf`/`cntlzw`/`srwi` idiom. Retail (and the earlier `== area` test in the same function) uses
`cmpw` / `bne` / `li r3,1`. Writing the last comparison as
`if (... == area) { return true; }` inside the existing `if (dock) { ... }` makes the tail
instruction-for-instruction identical to retail. 76.86% -> 80.39%.

Remaining delta: one instruction. Retail hoists the `lhz r0,1136(r3)` / `stw r0,8(r1)` copy of the
by-reference argument for `GetObjectById` to *after* the callee-save prologue; 2.7 emits it before.

Prime 1's `IsConnectedToArea` is the same algorithm written with three named locals
(`world` / `area` / `dock`) instead of one chained expression. Adopting that spelling changed
nothing in our object - the chain already inlines identically - so it was not kept (it would have
been diff noise for no measured gain).

### 4. `GetTouchBounds` - 90.48% (unchanged)

Same single-instruction problem: ours hoists `lbz r0,32(r4)` (the `GetActive()` byte load) above
`stw r31,44(r1)` / `mr r31,r3`; retail emits the whole prologue first. Both objects are 168 bytes
with identical instruction multisets. Tried nested `if (GetActive()) { if (HasMaterial(...)) ... }`
instead of `&&`: byte-identical output, no change. Prime 1's body differs only in the material
constant (`kMT_Solid` vs Echoes' `kMT_Unknown59`), so it cannot move the schedule.

### 5. `GetOrbitPosition` - 84.62% (unchanged)

Prime 1's source is character-for-character what we had (`return GetTranslation() + mOrbitPos;`),
so there is nothing to port; the delta is that retail hoists the `lfs f3,1144(r4)` load of
`mOrbitOffset.y` above the `fadds` for `.x`, and 2.7 does not. Register allocation already matches
exactly (f1/f0/f4/f3/f2). Tried and measured: `operator+=` on a local (80.00%, worse); three named
`float` locals + `CVector3f(x,y,z)` (84.62%, byte-identical to `operator+`, so reverted to keep the
diff free of no-op churn). `CVector3f::operator+` in this repo's `Kyoto/Math/CVector3f.hpp` uses
`GetX()` where Prime 1 uses `mX`; that header is shared with already-matched units so it was left
alone.

## Also measured, not kept

* `UpdateShield` (98.05%, 228 B, not in the item's list): the two case bodies are already identical;
  retail's dispatch has one extra `cmpwi r0,0` / `beq` (it tests value 0 explicitly where 2.7 tests
  `1` directly). Tree-shape difference, not source-expressible.
* No other unit, and no other function in this unit, changed. The only rebuilt object is
  `build/G2ME01/src/MetroidPrime/ScriptObjects/CScriptDoor.o`; the unit is `NonMatching`, so it is
  not in the link.

## Gates

```
sha1sum -c config/G2ME01/build.sha1   # 87 OK, 0 failed (main.dol + 86 RELs)
                                       # main.dol = 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh              # 749 files, 0 failed, 0 errors
python3 tools/check_symbol_names.py   # 503 units, 0 declared names missing
./tools/decomp_build.sh               # All: 30.44% fuzzy, 22.36% matched, 11.74% linked - unchanged
python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptDoor
                                      # ok: none emits its functions out of retail order
```

Every edit is a re-spelling of code that was already there; no initialisation, call or check was
removed. The two re-shaped conditionals (`kDS_CloseDelay`, `IsConnectedToArea`'s tail) are
equivalent to the originals for every input.

## Lesson worth keeping

MWCC 2.7 hoists the first body load of a function above the callee-save prologue and schedules one
float add earlier than 1.3.2 did, on identical source. That single reordering is the whole delta in
`GetTouchBounds`, `GetOrbitPosition` and the last instruction of `Think`, and it is not reachable
from the source. Do not spend a run on those three again - check the *instruction multiset* first; if
it already matches and only the order differs, the function is blocked on the compiler version.

Two source-level rules that did pay for themselves here, both from reading the bytes rather than the
C++:

* a `switch` emits its case bodies in **source order**, so the order of the `case` labels is part of
  the match - the same trap as `flip`'s reverse-declaration rule;
* a value assigned to a member and then *read back* through that member lets the allocator drop the
  local to r0; using the local in a later expression keeps it alive and costs a whole register class.

## NEW items

None filed. The three remaining functions are one instruction away and blocked on compiler-version
scheduling; per the item's rules a measured wall belongs in this file, not in the queue.
