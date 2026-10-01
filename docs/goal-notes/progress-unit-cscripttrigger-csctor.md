# progress-unit-cscripttrigger-csctor

`kind: progress`, target `MetroidPrime/ScriptObjects/CScriptTrigger`. The unit stays `NonMatching`;
`flip_test.sh` was not run and is not the acceptance test here. The judge is
`build/report.json`'s per-unit `matched_functions`.

## Result

**20 -> 21 of 44 matched functions.** `./tools/goal_check.sh build/goal/item.json` prints
`PASS progress-unit-cscripttrigger-csctor`; the whole-project matched count went 11949 -> 11950
with `linked` unchanged at 5728 and the `All:` line unmoved at 33.77% fuzzy / 26.96% matched /
12.64% linked. `sha1sum build/G2ME01/main.dol` is `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

| function | before | after |
| --- | --- | --- |
| `ReplaceInhabitant__14CScriptTriggerF9TUniqueId9TUniqueIdR13CStateManager` | 98.39% | **100%** |

Unit fuzzy 34.41% -> 34.46%, matched code 25.41% -> 28.40%.

The item was queued for the **constructor** ("the constructor is at 95.71% and its whole remaining
gap is where the compiler places the `CActor` mem-initializer argument temporaries ... ;
`ReplaceInhabitant` is at 98.39%"). `ReplaceInhabitant` landed. **The constructor did not move** -
see "The constructor is a measured wall" below.

## The change

One line, in `include/MetroidPrime/ScriptObjects/CScriptTrigger.hpp:33` - `CObjectTracker::SetObjectId`:

```cpp
void SetObjectId(const TUniqueId& id) { mId = id; }   // was: SetObjectId(TUniqueId id)
```

**This is not a "take it by reference because it is nicer" change; it is what removes a whole
stack frame's worth of instructions.** mwcceppc materialises every *class-type by-value* argument
into a caller-frame temporary. `TUniqueId` is a 2-byte struct, so the by-value spelling forces a
third and fourth `TUniqueId` temp in `ReplaceInhabitant`, shifting all three of the real ones up by
four bytes and adding one `sth` of its own. Retail's frame has exactly three temps at `r1+16`,
`r1+12`, `r1+8`; ours had four at `r1+20`, `r1+16`, `r1+12`, `r1+8`.

This is the **same rule** the previous run recorded for `rstl::find` in `HasInhabitant`
("the copy is a side effect of calling a function whose parameters are class objects") and for
`RemoveInhabitant` ("need the **const** `mgr.GetObjectById`, not `mgr.ObjectById`"). Generalise it:

> **Any inline accessor mwcceppc might inline away still costs a stack temporary if its parameter
> is a class type passed by value. If retail's frame has one fewer slot than yours, that is the
> first thing to look at.** Read the frame's `sth`/`addi` slot count, not just the body.

`ReplaceInhabitant` is now byte-identical: 276 B, 69 instructions, and the only 6 instructions that
differ from retail are the 6 `bl` relocations (verified with a raw hex compare of both
disassemblies - the 63 non-call instructions match byte for byte, and the call targets are the same
six: `GetObjectById` x2, `TCastToPtr<CActor>` x2, `HasInhabitant`, `do_erase`).

The body itself is unchanged from the previous run - no other spelling was needed, and none of the
13 alternatives tried below beat it.

## Files touched

- `include/MetroidPrime/ScriptObjects/CScriptTrigger.hpp:31-35` - `SetObjectId`'s parameter, plus a
  comment saying why it is by reference.

`src/MetroidPrime/ScriptObjects/CScriptTrigger.cpp` is **unchanged** (`git diff` shows only the
header). No `configure.py`, `splits.txt` or `files.cmake` change: nothing was carved and no symbol
was claimed, so the four-file carve rule does not apply. No asm.

`SetObjectId` has exactly one caller in the tree (`CScriptTrigger.cpp:182`), so the header change
cannot move any other unit. It is included by 9 files, all of which rebuild identically.

## The constructor is a measured wall

Still **95.71%**, unmoved, after **13 spellings tried this run**. The whole gap is 4 instructions
of *scheduling*, and the same multiset of instructions is present in both - only the order of the
`CActor` base-initializer argument temporaries differs:

```
retail  0x80073158  addi r3,r1,176 ; bl CModelDataNull
        0x80073160  lhz  r0,kInvalidUniqueId          <- load the 9th argument
        0x80073164  addi r3,r1,80  ; sth r0,16(r1) ; bl __ct__16CActorParametersFv
        0x80073170  li r0,0 ; lwz r5,@602 ; stw r0,28(r1) ; ... ; bl __shl2i

ours    0xbbb8      addi r3,r1,176 ; bl CModelDataNull
        0xbbc0      addi r3,r1,80 ; bl __ct__16CActorParametersFv   <- ctor first
        0xbbc8      lhz r4,kInvalidUniqueId ; ... ; sth r4,16(r1)    <- load after
```

Retail evaluates the 9th argument (`kInvalidUniqueId`, needed at `r1+16`) *before* the 8th
(`CActorParameters()`, at `r1+80`); ours does the reverse. Both then build the `CMaterialList`
(the `__shl2i` at the end). The frame, the 140 instruction count, the call sequence and all 16
relocations already match; **only the interleaving of two argument temporaries is wrong.**

This is the compiler's choice of evaluation order inside a mem-initializer, not a spelling in the
body, which is what the previous run concluded. It is now measured rather than argued.

## Things measured, so nobody repeats them

Constructor, from the 95.71% baseline (all measured, none reached 100%):

- `kInvalidUniqueId` as a bare argument (pre-existing) -> **95.71%**.
- `TUniqueId(kInvalidUniqueId)` / `static_cast<TUniqueId>(kInvalidUniqueId)` -> 94.82% / 95.71%.
- `TUniqueId(static_cast<ushort>(0xFFFF))` -> 94.68%.
- `CMaterialList(static_cast<uint32>(kMT_Trigger))` -> **86.37%** (much worse; do not).
- `0` -> `0u`, or `static_cast<uint>(0)` -> 95.71%, no change.
- `uid` -> `static_cast<TUniqueId>(uid)` -> 95.71%, no change.
- `CActorParameters()` -> `(CActorParameters())` -> 95.71%, no change.
- `mInhabitants()` added to the mem-init list explicitly -> 95.71%, no change.
- `mAttachedTrigger` moved after `mDamageInfo` in the init list -> 95.71%, no change.
- Swapping the `CMaterialList` and `CActorParameters` argument positions -> **build failure**
  (wrong `CActor` overload), so that is not available as a lever.

`ReplaceInhabitant`, from the 98.39% baseline (the previous run's 12 spellings still stand; these
are new):

- `SetObjectId(const TUniqueId&)` in the header -> **100%**. This is the whole fix.
- `SetObjectId` left by value with a local `const TUniqueId id = newId;` at the call site -> 96.84%.
- `it->mId = newId` (bypassing the accessor) -> build failure, `mId` is private.
- Hoisting `const TUniqueId old = oldId;` -> 98.39%, no change.
- Hoisting `const bool inside = HasInhabitant(newId);` -> 98.39%, no change.
- `while` loops instead of `for` -> 88.61%.
- `(*it).GetObjectId() == oldId` -> 98.39%, no change.
- `oldId == it->GetObjectId()` (operand order) -> 98.25%, slightly worse.
- `TCastToPtr` + non-const `mgr.ObjectById` -> 98.39%, no change.
- `return true` inside the first loop instead of a `replaced` flag -> build failure
  (`replaced` then unused); the flag shape is required.
- trailing `return replaced` -> `return false` -> 94.62%, worse.

## What is still open in this unit (23 functions unmatched, measured)

The constructor at 95.71% (wall, above). The six low-percentage bodies are still `// TODO:` stubs
with no body - real work, not spelling problems: `UpdateInhabitants` 1520 B at 0.26%, `Touch`
1044 B at 0.38%, `AddInhabitant` 944 B at 0.42%, `UpdateCameraInhabitant` 664 B at 0.60%,
`SetPlayerInside` 240 B at 1.67%, `ClearInhabitants` 224 B at 1.79%. Plus 13 unnamed `fn_*` at
0.00%, which the previous run measured as the same dead end (byte-identical COMDAT template
functions our object already emits; renaming them in `symbols.txt` did not pair them).

`ClearInhabitants` is the cheapest of the stubs and its body is **fully determined by retail's
disassembly** (0x80072198, 0xE0 bytes, `./tools/dis.sh 0x80072198 0xE0`). Read out of it:

- a `for` over `mInhabitants` (`lwz r31,348(r3)` / `lwz r31,4(r31)` against `352(r27)`) - the list
  walk is the same one `ReplaceInhabitant` uses, so it is known-good spelling;
- an inner `for (i = 0; i < mgr.GetNumPlayers(); i++)` over `mgr` at `5368(r4)` with a countdown
  (`mtctr` / `bdnz`), comparing `lhz 8(player)` (a `CEntity::m_uid`) against `lhz 8(node)` (the
  tracker's `mId`) and remembering the matching index in `r30`;
- `mgr.ObjectById(id)` + `TCastToPtr<CActor>`, then `if (actor)` { `SetPlayerInside(mgr, false, index)`;
  `NotifyInhabitantExited(*actor, mgr); };
- then `fn_80072278` on `&mInhabitants` - which is the out-of-line `rstl::list::erase(begin, end)`
  (it copies `8(r4)`/`4(r4)` into the frame and tail-calls `fn_800722B8`, itself the
  `do_erase` loop). **Our `mInhabitants.clear()` inlines that walk instead of calling it**, which
  is the previous run's open question, still open.

**The blocker, and it is new:** the index is seeded from a *global*, not a constant:

```
800721c0:	lwz     r30,-27760(r2)      ; 0x80419110, `lbl_80419110`, .sbss, size 0x8
...
800721e8:	mr      r30,r5               ; replaced by the loop index on a match
8007221c:	lwz     r0,-27760(r2)       ; reloaded to test "was it ever set?"
80072220:	cmplw   r30,r0
80072224:	beq     8007223c             ; skip SetPlayerInside if still the seed
```

`lbl_80419110` is **not declared anywhere in the tree**. It is not one of the three named sentinels
(`kInvalidEditorId` 0x80419120, `kInvalidUniqueId` 0x80419124, `kInvalidAreaId` 0x80419128, and the
unnamed -1 at 0x8041912C - all in `.sbss` and all written by the one static initialiser at
0x800E9BF4, per `docs/research/port_globals.md`). It has exactly three references in the whole DOL
(`-27760(r13)` at 0x800E6FE4 read, 0x800E7244 and 0x800E865C written), and the writer at 0x800E865C
is inside `CDecalManager::Initialize`, which stores `-1` there - so it is a **decal-manager** global
that `ClearInhabitants` is borrowing as a "not found" seed. Naming and declaring it means adding a
`.sbss` global to `PortGlobals.cpp`, which shifts the addresses of every `.sbss` symbol after it
and is therefore a much larger and riskier change than this item's target. That is why
`ClearInhabitants` was left as a stub rather than guessed at with a made-up sentinel name.

WALL: __ct__14CScriptTriggerF9TUniqueId...RC9CVector3fUibb 95.71% - the entire remaining gap is
the evaluation order of two `CActor` base-initializer argument temporaries (retail builds the
`kInvalidUniqueId` temp at `r1+16` before the `CActorParameters()` temp at `r1+80`; we do the
reverse); 13 spellings of the argument expressions all measured, none reorders them, and the frame,
instruction count and all 16 relocations already match.

NEW: progress-unit-cscripttrigger-clear | progress | MetroidPrime/ScriptObjects/CScriptTrigger |
`ClearInhabitants` (224 B, 0x80072198) is fully specified by its disassembly - the loop shape, the
inner `m_players[]` search, the callee list and the trailing out-of-line list erase are all read out
above - and is blocked only on naming the undeclared `.sbss` global `lbl_80419110` (0x80419110) it
seeds its player-index search with, plus on getting `mInhabitants.clear()` to call
`rstl::list::erase(begin, end)` out of line instead of inlining the walk.
