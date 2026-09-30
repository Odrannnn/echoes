# progress-prime1-cguncontroller — `MetroidPrime/Weapons/GunController/CGunController`

`kind: progress`. The unit stays `NonMatching`; I did not run `flip_test.sh`. Baseline re-measured
on a clean tree before I touched anything: **6 / 12 functions matched**, unit fuzzy 82.23%,
`All: 31.23% fuzzy, 23.55% matched, 11.83% linked (10263 / 28465)`.

**Result: 10 / 12 matched** (`All:` 31.24% / 23.57% / 10267). Four of the five functions the
item named are now 100%, three of them from Prime 1's source with no change beyond the member
names already in this tree. `tools/goal_check.sh build/goal/item.json` → **PASS**, whole gate
including `main.dol` sha1, 86 RELs, `check_docs_claims.py` and the port probe.

## Per function, before → after

| function | before | after | what it took |
|---|---|---|---|
| `LoadFidgetAnimAsync` | 0.00% (no body at all) | **100.00%** | Prime 1 verbatim, 1 line |
| `EnterFreeLook` | 78.59% | **100.00%** | Prime 1's inverted condition |
| `EnterComboFire` | 79.47% | **100.00%** | Prime 1's inverted condition |
| `Update` | 74.61% | **100.00%** | three things, see below |
| `EnterStruck` | 91.87% | 91.87% | nothing tried that helped; see the wall |
| `fn_801DC820` | 0.00% | 0.00% | depends on `EnterStruck` |

`__ct__`, `EnterFidget`, `EnterIdle`, `ReturnToDefault`, `Reset` and `ReturnToBasePosition` were
already 100% and stayed there.

## What each change was, and the evidence for it

### `LoadFidgetAnimAsync` — the symbol was declared and had no body

`CGunController.hpp:37` declares it; the `.cpp` had nothing. Prime 1's body is one call and
transfers unchanged:

```cpp
void CGunController::LoadFidgetAnimAsync(CStateManager& mgr, int type, int gunId, int animSet) {
  mFidget.LoadAnimAsync(*mModelData.AnimationData(), type, gunId, animSet, mgr);
}
```

0.00% → 100.00%, 48 bytes, retail 0x801DC7F0 size 0x30. `CGSFidget::LoadAnimAsync` already has the
identical signature in this tree's header, so nothing outside the unit changed.

**It has to be declared between `EnterStruck` and `Update`** — retail order is
`Update` 0x801DC614, `LoadFidgetAnimAsync` 0x801DC7F0, `fn_801DC820` 0x801DC820, `EnterStruck`
0x801DC908, and mwcceppc emits definitions in reverse source order. The unit's `.text` came out in
ascending retail order (`nm -n` on `build/G2ME01/src/.../CGunController.o`), so the reverse-declared
rule holds.

### `EnterFreeLook` / `EnterComboFire` — MW lays out the two arms in source order

Both were ~79% with an otherwise identical instruction stream. The diff was only the branch shape:

```
retail   cmpwi r0,3 ; beq a18   ... SetAnim ... b a20 ; a18: lwz/stw (SetLoopState) ; a20: mGunState=2
ours    cmpwi r0,3 ; beq END   ... SetAnim ...     ; SetLoopState ; b END ; END:
```

Retail jumps **forward** past the `SetAnim` arm into the `SetLoopState` arm, i.e. the *then* arm is
laid out second. That is the shape MW gives `if (A && B) X else Y`, not `if (A || B) Y else X`.
Prime 1 writes both with the condition inverted:

```cpp
if (mGunState != kGS_ComboFire && !mEnteredComboFire) { mCurAnimId = ...SetAnim(...); }
else { mFreeLook.SetLoopState(mComboFire.GetLoopState()); }
```

Swapping to Prime 1's spelling took both to **100.00%** in one build. **When a switch or an
if/else only differs in which arm comes first in the object, read the branch direction, not the
percentage: `beq` to a *later* address means the taken arm is the second one in the source.**

### `Update` — three separate things, worth 25 points

1. **The jump table.** Retail does `cmplwi r0,8 ; bgt default ; lis/slwi/lwzx/mtctr/bctr` over
   `jumptable_803B73D0`; ours emitted a compare chain. Dumping the table's nine `R_PPC_ADDR32`
   relocations showed cases 0, 1 and 4 all pointing at the shared empty arm, which is only
   possible if they are **explicit** `case` labels rather than `default`. Adding
   `case kGS_Inactive: case kGS_Default: case kGS_Idle: break;` in place of a bare `default:` gave
   the jump table. Its entries are the record of the switch's real shape: 0, 1, 4 → end; 2 →
   FreeLook; 3 → ComboFire; 5 → Fidget; 6 → Strike; 7 **and 8** → BigStrike. So `kGS_Unknown8`
   really does share `BigStrike`'s body, which this tree already had right.
2. **Prime 1's return spelling**, `if (mAnimDone) { ...; return true; } return false;` instead of
   `return mAnimDone;`. Retail's tail is `li r3,1` / `li r3,0` on two paths.
3. **The last 6 points, and the reason to trust Prime 1's source even where it looks redundant.**
   After 1 and 2 the function was 0x1E0 against retail's 0x1DC: one instruction too many, and one
   callee-saved register too many (ours saved r28–r31 and kept `CAnimData& data` in r28; retail
   saved r29–r31, kept it in r29, and *re-derived* it in the Strike arm with
   `lwz r4,0(this) ; lwz r4,16(r4)`). Prime 1's Strike arm passes
   `*mModelData.AnimationData()` — a fresh expression — where every other arm passes the named
   local `animData`. That redundancy is what lets the register die and be rematerialised. Copying
   it verbatim: **100.00%**, size 0x1DC, three saved registers.

**So: a 4-byte size mismatch in a function that is otherwise instruction-for-instruction identical
is a live-range problem, and the fix is usually in the source's expression, not its order. Compare
`nm -S` sizes first.**

## The wall: `EnterStruck` 91.87% and `fn_801DC820` 0%

Both are the same 232 bytes, and I could not derive them.

Retail's `EnterStruck` calls a function at 0x801DC820 that this tree has no body for, and that
function is not what its name suggests. Disassembled, it is a whole-object copy of a
`CPASAnimParmData`: it stores words 0 and 1 of the destination, reads the count back out of
**word 1**, and then copies `count` 8-byte elements from +8 in 64-byte chunks with an 8-byte
remainder loop. Word 0 is `pas::EAnimationState`, word 1 is
`rstl::reserved_vector<CPASAnimParm,8>::mCount`, and `CPASAnimParm` is `CHECK_SIZEOF(..., 0x8)`
in `include/Kyoto/Animation/CPASAnimParm.hpp` — so the layout is exactly
`{mStateId, mCount, mData[64]}` = 72 bytes, and the call site's
`addi r3, r1,196 ; addi r4, r1,124` passes whole-`CPASAnimParmData` pointers, 72 bytes apart.

`EnterStruck` constructs the parmdata at `r1+124`, copies it to `r1+196`, and passes
**`r1+196`** as `FindBestAnimation`'s first argument. The parameter is a const reference — the
relocation is `FindBestAnimation__12CPASDatabaseCFRC16CPASAnimParmDataR9CRandom16i`, and
`config/G2ME01/symbols.txt:11745` confirms the const-ref overload is the real one — so the copy
is not argument passing. It is an explicit assignment, and **Prime 1's `EnterStruck` has no such
copy** (Prime 1 matches 100% without one, and its `EnterStruck` is 0x2B4 against Echoes' 0x1B8).
So the assignment is an Echoes-only edit whose source I cannot recover from the Prime 1 clone, and
guessing at it would be inventing source rather than recovering it. Everything else in
`EnterStruck` is already identical modulo that call: the same four `NoParameter()` calls, the same
eight constructor arguments, the same `CAnimPlaybackParms` tail. The 91.87% is the 36 bytes the
copy's presence shifts — retail's frame is 0x140 and it saves r27–r31, ours 0xF0 and r26–r31.

I did **not** try hand-writing the copy, because with no source construct to call it from, a
`fn_801DC820` that exists only to make a percentage go up is exactly the kind of thing the
reviewer rejects.

WALL: EnterStruck 91.87% - the remaining 36 bytes need a `CPASAnimParmData` copy-assignment that retail's source performs and Prime 1's does not; fn_801DC820 is that copy and its only caller is EnterStruck.

## Link-gap bookkeeping (two files, both measured consequences)

`tools/goal_check.sh` failed `gate.sh` twice on this, and both fixes are real:

1. `docs/research/port_link_gap_list.md` ratchets **both ways** — a listed symbol that is no
   longer missing fails. `LoadFidgetAnimAsync` left the list, so its entry is deleted and the
   `other game methods` header count follows.
2. Defining `LoadFidgetAnimAsync` reached its only callee,
   `CGSFidget::LoadAnimAsync` (retail 0x801DD05C, 0xF0 bytes), which nothing had needed before
   because its only caller was itself a reach stub. So the gap grew by one and the entry is added,
   with a dated paragraph in `docs/research/port_link_gap.md` saying what provides it and what
   does not. It is **not** closable from this tree: retail's body ends in
   `NWeaponTypes::get_token_vector(CAnimData&, int, rstl::vector<CToken>&, bool)`, and
   `include/MetroidPrime/Weapons/WeaponCommon.hpp` declares no such function — Prime 1's is two
   overloads in `WeaponTypes.cpp` (0xFC and 0xE0 bytes) over token-loading machinery the port does
   not have. So it stays a logged reach stub and **the boot is unchanged**: the call now stops one
   level deeper, inside a body that was already a stub. Net gap 246 → 246.

`check_docs_claims.py` → "docs claims agree with the tree".

## Files changed

- `src/MetroidPrime/Weapons/GunController/CGunController.cpp` — the only source change. Added
  `LoadFidgetAnimAsync`; inverted the conditions in `EnterFreeLook`/`EnterComboFire`; rewrote
  `Update`'s switch (explicit `kGS_Inactive`/`kGS_Default`/`kGS_Idle` arm, Prime 1's break shapes,
  fresh `*mModelData.AnimationData()` in the Strike arm, explicit `return true/false`).
- `docs/research/port_link_gap_list.md` — one entry deleted, one added, group header count.
- `docs/research/port_link_gap.md` — the `other game methods` row, plus the dated paragraph above.

No header, `configure.py`, `splits.txt`, `files.cmake`, `tools/` or `build/goal/` change, no `asm`.
`check_symbol_names.py`: 0 missing. `main.dol` sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
`git status` shows those three files and nothing else, so no function in any other unit can have
moved.

NEW: progress-cgsfidget-loadanimasync | progress | MetroidPrime/Weapons/GunController/CGSFidget | LoadAnimAsync is 240 bytes and unwritten; closing it needs NWeaponTypes::get_token_vector, which this tree does not declare at all, so it is one item of its own (1 of 6 functions in the unit).
