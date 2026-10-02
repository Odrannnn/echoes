# progress-unit-cscriptdock

`kind: progress`, target `MetroidPrime/ScriptObjects/CScriptDock`. The unit stays `NonMatching`
(it does not flip: `fn_800B63DC` alone is 0% and `AcceptScriptMsg` is 84%, so the unit cannot
reach `Matching` in one item). No `asm`, no config change, no other unit touched.

## Result: 8/18 -> 11/18 matched functions (measured, `./tools/goal_check.sh build/goal/item.json` PASS)

```
goal_check: PASS progress-unit-cscriptdock
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11754 -> 11757   linked 5727 -> 5727
  ok    target rose: main/MetroidPrime/ScriptObjects/CScriptDock: 8 -> 11 / 18 functions
```
`All: 33.41% fuzzy, 26.42% matched, 12.64% linked (11757 / 28465 functions)`, unit
`81.73% fuzzy, 28.17% matched code`. `sha1sum build/G2ME01/main.dol` =
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged), `check_symbol_names.py` 0 missing.

## Per function (before -> after, objdiff fuzzy, retail size)

| function | before | after | what changed |
|---|---|---|---|
| `CWorld::PropogateAreaChain` | 98.55% | **100.00%** (304 B) | two separate early `return`s instead of one `||` |
| `CScriptDock::GetCurrentConnectedAreaId` | 97.44% | **100.00%** (156 B) | `GetAreaAlways(GetAreaId())` instead of `GetAreaAlways(mArea)` |
| `CScriptDock::UpdateAreaActivateFlags` | 52.52% | **100.00%** (252 B) | Prime 1's loop shape: `const int count`, `const bool active` computed before the `GetConnectedAreaId` call |
| `CScriptDock::Think` | 87.92% | 99.91% (664 B) | ternary `SendScriptMsgs(...)` -> `if/else` with two separate calls |
| `CScriptDock::SetLoadConnected` | 83.42% | 99.64% (276 B) | the connected-area test calls `GetConnectedAreaId(GetReferenceCount())` twice instead of caching it in a local |
| `CScriptDock::HasPointCrossedDock` | 96.17% | 99.71% (164 B) | `dock.GetPlaneVertices().data()` + named `const CPlane plane` local |
| `CScriptDock::GetPlane` | 61.80% | 99.66% (140 B) | same, plus `return plane;` instead of returning the temporary |
| `CScriptDock::__ct__` | 78.48% | 78.48% (580 B) | unchanged, see below |
| `CScriptDock::AcceptScriptMsg` | 83.98% | 84.02% (1456 B) | one `const int count = area->GetDockCount(); if (count <= mDock)`; rest is register allocation |
| `fn_800B63DC` | 0.00% | 0.00% (636 B) | not attempted, see below |

Everything above is in `src/MetroidPrime/ScriptObjects/CScriptDock.cpp` only.

## Spellings tried and rejected (so the next run does not repeat them)

Measured with `tools/fast_try.sh` / `tools/try_edit.py` (all on the unit, per-function fuzzy).

**`GetPlane` / `HasPointCrossedDock` - the CPlane ctor argument order.** The CPlane temporary has
to be built in a named local and returned (`return CPlane(...)` is 65%/96%; `CPlane plane(...);
return plane;` is 96.2%). The remaining 1 instruction is the *order of the two `addi`s* that form
`&vertices[1]` and `&vertices[2]`: retail does `addi r6,r4,24` (arg 3) before `addi r5,r4,12`
(arg 2), ours the reverse. Every spelling below produced the identical 99.66% / 99.71%, i.e. the
order is decided by the CPlane ctor call itself, not by how the arguments are spelled:

`&vertices[0]` then `v[0],v[1],v[2]` 99.66 | `dock.GetPlaneVertices().data()` 99.66 |
`vertices.data()[i]` 96.2 | `*(v+0),*(v+1),*(v+2)` 99.66 | `v[0],v[1],v[2]` with `v` non-const 99.66 |
declared `p2,p1,p0` refs in that order 99.66 | `p0,p1,p2` refs 99.66 | `p2-then-p1` pointer locals
99.66 | `p1-then-p2` 99.66 | `&vertices[2]` with `v[-1],v[0],v[1]` 98.8 | `CPlane plane(p0,p1,p2);
return CPlane(plane);` 95.86 | `CPlane plane(p0,p1,p2); plane; return plane;` 99.66 |
`return CPlane(vertices[0],vertices[1],vertices[2])` (no local) 65.26 | `return CPlane(v[0],v[1],v[2])`
65.26. Passing by value (`CVector3f a=v[0]; ...`) drops to 33.66% (extra copies).

**`__ct__` - a real wall, not a spelling problem.** Retail's 580 B is 16 bytes *smaller* in frame
(`stwu r1,-416` vs our `-432`), it calls `CModelDataNull__10CModelDataFv` and
`__ct__10SMoverDataFfRC9CVector3fRC10CAxisAngleRC9CVector3fRC10CAxisAngle` out of line (after two
`Identity__10CAxisAngleFv`), it loads the extent components in the order x, **y, z** and negates
three separate values, and it computes `loadConnected && !isVirtual` as
`neg / srawi / or / srawi` on the argument registers. Tried, all 78.48% or worse:
`CAABox(-(0.5f*extent), 0.5f*extent)` 78.48 | `CAABox(0.5f*extent, -(0.5f*extent))` 78.52 |
Prime 1's explicit-component `CAABox(CVector3f(-(0.5f*GetX())), ...)` 78.52 | the 6-float
`CAABox` overload 74.39 | `CAABox(CVector3f(extent * -0.5f), extent * 0.5f)` 73.61 |
`(CModelData = CModelData::CModelDataNull())` 73.61 | `CModelData()` instead of
`CModelDataNull()` no change | `SMoverData(1.f, CVector3f::Zero(), CAxisAngle::Identity(),
CVector3f::Zero(), CAxisAngle::Identity())` (the 5-arg out-of-line form) 78.48 |
`SMoverData(1.f, CVector3f::Zero(), CAxisAngle::Identity())` 78.48. The frame-size difference means
our `CPhysicsActor` base-ctor argument list is not spelling the same shape, not the dock's own
init-list; this needs a look at `CPhysicsActor`'s ctor, which is a different unit.

**`AcceptScriptMsg` - the switch is register allocation, not logic.** Retail keeps the message in
`r31` and the compare constants in `r5`/`r6`; ours keeps it in `r6` and reloads the constant base.
322 instructions differ but the source-level differences I could find are worth ~0.04%:
Prime 1's `GetMaterialFilter().GetIncludeList().Union(CMaterialList(kMT_AIBlock))` 83.98 (no
change) | dropping the `door != nullptr` check 83.98 (no change) | a `while` loop over the door
list instead of the range-for 83.98 (no change) | `mDock >= area->GetDockCount()` -> `const int
count = area->GetDockCount(); if (count <= mDock)` **84.02 (kept)** | hoisting
`const int numAreas` out of the virtual-dock loop 81.23 (worse - do not hoist it).
The `kSM_XWLD` case is the one real difference left: retail builds the include list with
`__shl2i(0,1)` and a table load for `kMT_AIBlock` and then calls `MakeIncludeExclude`, while ours
materialises the `CMaterialList` and copies it; that is a `CMaterialList`/`CMaterialFilter` header
question, not a CScriptDock one.

**`fn_800B63DC` (636 B, 0.00%)** is the CScriptDock `CInputStream` constructor - it reads
`SLdrEditorProperties` from the stream through a `switch` on eight property hashes and calls
`LdrToEntityInfo` / `LoadTypedefSLdrEditorProperties` / `fn_800420D4`. It is 0% because the class has
no such constructor at all in this repo's source; the 0% is a *missing function*, not a bad
spelling, and writing it needs `CEntityInfo`'s stream loader which does not exist here.

## Lessons (general, not CScriptDock-specific)

- **`X(a, b) != k` written twice is what retail compiled.** Spelling
  `if (dock.GetConnectedAreaId(dock.GetReferenceCount()) != kInvalidAreaId) { Area(dock.GetConnectedAreaId(dock.GetReferenceCount()))... }`
  (a repeated pure call) took `SetLoadConnected` 83.42 -> 99.64; introducing the named local we had
  before was worth 2%. Prime 1's source has the local, and in *this* unit retail does not. The
  same shape is why `Think` wants the ternary expanded to two `if`/`else` calls, and why
  `UpdateAreaActivateFlags` wants `active` computed *before* `GetConnectedAreaId`.
- **`GetAreaId()` vs the member `mArea` changes codegen** in a `const` method: `GetCurrentConnectedAreaId`
  went 97.44 -> 100.00 from `GetAreaAlways(mArea)` -> `GetAreaAlways(GetAreaId())` even though the
  inlines are identical text. A unit can sit at 97% purely on which spelling of the same value a
  header accessor uses.
- **Two separate early `return`s beat `if (a || b) return;`** in `PropogateAreaChain` (98.55 ->
  100.00). MWCC materialises the `||` as a shared compare-and-branch that retail does not have.
- **A 99.6% that will not move is usually the callee's argument setup, not the caller's source.**
  Eleven spellings of the `CPlane` ctor arguments all produced the *same* two-instruction `addi`
  swap, so the ordering is a property of the call. Do not spend a run on it; it needs the ctor
  itself, in `src/Kyoto/Math/CPlane.cpp`.
- **`tools/try_edit.py` silently does nothing on a constructor.** It replaces from a marker to the
  next `\n}\n`, and a mem-init list means the closing brace is not on its own line, so every
  "variant" scored identically and I lost one round on it. For a ctor, replace the whole
  `CScriptDock::CScriptDock(...) ... {}` block between the declaration and `CScriptDock::~CScriptDock`.
- **`fast_try.sh`/`objdiff` on one unit is a valid per-function measurement loop** - a few seconds
  per variant against a multi-minute full build - but it does not rebuild the DOL, so a score
  improvement is not yet a link result. The full `decomp_build.sh` + `goal_check.sh` is what
  decides.

## Not filed as `NEW:`

`fn_800B63DC` needs a stream-loading `CEntityInfo`, which is absent from the tree - that is a
missing-symbol port item for `CScriptDock`'s own `CInputStream` ctor, but its success would not
raise any *count* on its own (it is currently counted as 0/18 already), so per the rules I am not
filing it. The `CPhysicsActor` constructor shape (behind `__ct__`) and `CMaterialList`/`CMaterialFilter`
argument materialisation (behind `AcceptScriptMsg`) are both other units' headers; file them
against those units if a lane wants them.

WALL: CScriptDock::__ct__ 78.48% - retail's 580 B calls CModelDataNull/SMoverData out of line and
uses a 16-byte-smaller frame; 13 source spellings of the CAABox, model-data and mover-data
arguments all measured 78.48% or lower, and the remaining difference is in CPhysicsActor's
constructor, not this unit's init list.
WALL: CScriptDock::GetPlane 99.66% - one `addi` ordering inside the CPlane ctor call; 11 argument
spellings all measured 99.66% and the call is now byte-identical apart from that pair.

---

# Second run (lane 6, 2026-10-02) - 11/18 -> 13/18, PASS

Re-measured the clean tree first: 11/18 matched, unit 81.73% fuzzy, 28.17% matched code, the
same seven unmatched functions the first run listed. Nothing had landed upstream, so the item
was not `STALE:`.

```
goal_check: PASS progress-unit-cscriptdock
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12304 -> 12306   linked 5863 -> 5863
  ok    target rose: main/MetroidPrime/ScriptObjects/CScriptDock: 11 -> 13 / 18 functions
  ok    no asm added
```
`All: 34.78% fuzzy, 28.34% matched, 12.90% linked (12306 / 28465 functions)`; the unit goes
`81.73% -> 84.51% fuzzy, 28.17% -> 45.41% matched code`. `sha1sum build/G2ME01/main.dol` =
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged). Everything is in
`src/MetroidPrime/ScriptObjects/CScriptDock.cpp`; no header, config, tool or other unit touched.

## Per function (before -> after, objdiff fuzzy)

| function | before | after | what changed |
|---|---|---|---|
| `SetLoadConnected` | 99.64% | **100.00%** (276 B) | the `GetShouldLoadOther` result gets a named `const bool other` local before the `!=` |
| `Think` | 99.91% | **100.00%** (664 B) | `SendScriptMsgs(kSS_X, mgr, kInvalidUniqueId, kSM_None)` -> `SendScriptMsgs(kSS_X, mgr)` - the header's own defaults |
| `AcceptScriptMsg` | 84.02% | 94.31% (1456 B) | four changes, all measured individually below |
| `__ct__` | 78.48% | 78.48% (580 B) | unchanged; a new experiment, see below |
| `HasPointCrossedDock` | 99.71% | 99.71% | unchanged, same wall as `GetPlane` |
| `GetPlane` | 99.66% | 99.66% | unchanged, wall re-measured with 7 new spellings |
| `fn_800B63DC` | 0.00% | 0.00% (636 B) | not attempted, see below |

### The three changes, each measured on its own

- `SetLoadConnected`: `const bool other = dock.GetShouldLoadOther(dock.GetReferenceCount());`
  then `if (loadConnected != other)` - **99.64 -> 100.00**. Retail emits
  `clrlwi r4,r28,24; clrlwi r0,r3,24; cmplw r4,r0` (the parameter converted first, into r4) and
  we emitted `clrlwi r3,r3,24; clrlwi r0,r28,24; cmplw r0,r3`. Measured spellings that did **not**
  work: reversed operands `other != loadConnected` 99.64, `int(...) != int(...)` 98.91, `==` + early
  `return` 99.64, `static_cast<bool>` on both 99.64, and a non-const `IGameArea::Dock&` used for
  both calls 79.78.
- `Think`: dropping the two spelled-out trailing arguments so the header defaults apply -
  **99.91 -> 100.00**. Retail gives both call sites the *same* outgoing slot (`r1+16`); spelling
  `kInvalidUniqueId, kSM_None` out gives each site its own (`r1+20` and `r1+16`), and the extra
  4-byte temporary shifted every other spill in the function by 4. `include/Kyoto/.../CEntity.hpp:25-31`
  says exactly this and the comment is right. Only the *two-argument* form works:
  `SendScriptMsgs(kSS_MaxReached, mgr, kInvalidUniqueId)` (three args) measured 99.91 - unchanged.
- `AcceptScriptMsg`, in the order the wins came:
  1. `include.Add(kMT_AIBlock)` -> `GetMaterialFilter().GetIncludeList() | CMaterialList(lbl_80418054)`,
     with `extern "C" const EMaterialTypes lbl_80418054;` at file scope: **84.02 -> 86.04**.
     `lbl_80418054` is this unit's own `.sdata` word and holds 48 = `kMT_AIBlock` (read out of
     `build/G2ME01/main.elf`); retail loads it and calls `__shl2i`, we folded the shift to `lis r0,1`.
     Same idiom and same precedence as `CMaterialList(lbl_80417E54)` in `CGameProjectile.cpp:226`.
     Using the constant but keeping a named local (`CMaterialList include; include.Add(lbl_80418054);`)
     also reaches 86.04; the *inline* form is what reaches 89.04.
  2. same block written as the call's argument instead of through a local: **86.04 -> 89.04**.
  3. `const EScriptObjectMessage message = msg.GetMessage(); switch (message)` and
     `message == kSM_Increment` in the `kSM_Increment`/`kSM_Decrement` fallthrough: **89.04 -> 92.15**.
     Switching on a local alone is worth nothing (89.04); the win is reusing that local in the
     *other* test at the end of the function, which stops the second `msg.GetMessage()`.
  4. `DoesAreaExist(areaId) && IsAreaValid(areaId)` -> the three conditions spelled out:
     `areaId.Value() >= 0 && areaId.Value() < mgr.GetWorld()->GetNumAreas() &&
     mgr.GetWorld()->GetAreaAlways(areaId).IsLoaded()`: **92.15 -> 94.31**. This is the same test
     (`CWorld.hpp:122-124`: `IsAreaValid` is `mAreas[id]->IsLoaded()`, `DoesAreaExist` is
     `id >= 0 && id < mAreas.size()`, `GetAreaAlways` is `*mAreas[id]`, `GetNumAreas` is
     `mAreas.size()`) and nothing is dropped; mwcc was materialising the nested `&&` as a bool
     (`li r3,0 / li r3,1 / clrlwi. r0,r3,24; beq`) where retail short-circuits into two branches.
     `DoesAreaExist(areaId) && GetAreaAlways(areaId).IsLoaded()`, `both via GetArea`,
     a `CWorld* world` local, and `DoesAreaExist(areaId) && IsAreaValid(areaId)` all measured
     92.15; only the fully spelled-out form wins.

### What is left in `AcceptScriptMsg` (94.31%)

`difflib` on the two objects' instruction streams, ignoring branch targets, leaves 120 differing
lines and 6 extra instructions in ours. Almost all of it is stack-slot address, from three places:
retail spills the `kSM_XALD` `GetCurrentAreaId()` result to `r1+28`/`r1+20` and we use `r1+88`/`r1+92`;
the `kSM_SetToMax` `FindConnectedObject` 2-byte temp is at `r1+8` in retail and `r1+12` in ours;
and the `CPortalTransition` block uses `r1+52` in retail and `r1+48` in ours. We also keep the
`msg` *pointer* in r31 (`mr r31,r5` at the top, `mr r5,r31` before the `CActor::AcceptScriptMsg`
call) where retail never spills it - retail keeps the msg *value* in r31 and the `lis` scratch in
r6, we keep the value in r6 and the scratch in r5. Five spellings of the `kSM_XALD` statement were
measured and all were worse than the base: `CGameArea* area = mgr.World()->Area(GetCurrentAreaId());`
92.46, `CWorld* world = mgr.World();` 93.04, `const TAreaId id = GetCurrentAreaId();` 91.95,
`mgr.GetWorld()->GetAreaAlways(GetCurrentAreaId()).AddDock(...)` does not compile
(`AddDock` is non-const). Dropping the braces from `case kSM_XWLD` is exactly neutral (89.04).
Switching on a local *without* reusing it in the increment test is exactly neutral (89.04).

## New measured results on the first run's two walls

**`GetPlane` / `HasPointCrossedDock` - the `addi` order, re-measured and still a wall.** Retail
builds the `CPlane` arguments as `addi r6,r4,24` (arg 3) then `addi r5,r4,12` (arg 2); ours emits
them in the other order, and that is the *entire* diff (2 instructions of 35 and of 41). Seven
further spellings, all measured, none better than the 99.66 the tree already had:
`dock.GetPlaneVertices().data()` with no vector local 99.66, `const CVector3f* const v = &vertices[0]`
99.66, `CPlane plane(vertices[0], vertices[1], vertices[2])` indexed straight off the
`reserved_vector` 96.20, three `const CVector3f& a/b/c` refs 96.20, the same refs declared `c,b,a`
96.20, `CPlane plane = CPlane(v[0], v[1], v[2]);` 81.91. Together with the eleven in the first run
that is eighteen spellings with no effect on the order, which is a property of the call itself -
`CPlane(const CVector3f&, const CVector3f&, const CVector3f&)` is out of line in
`src/Kyoto/Math/CPlane.cpp`, a `Matching` unit, so it cannot be touched from here.

**`__ct__` - the first run's frame-size reason is right, and I found the one thing it missed.**
Retail's 580 B reads *three* more `.sdata` shift amounts, `lbl_80418048` (34), `lbl_8041804C` (43)
and `lbl_80418050` (48) - the three flags of `CMaterialList(kMT_Trigger, kMT_Immovable, kMT_AIBlock)` -
and `__shl2i`-es each, the same way `kSM_XWLD` does. I declared all three and used them: the object
does emit three `bl __shl2i`, and the function measures **78.48% either way**, with or without
`CModelData::CModelDataNull()` in place of `CModelData()` (four combinations, all 78.48). So the
constants are not what is wrong there. The remaining 16-byte frame difference and the fact that
retail stores six floats where we store twelve are in the `CPhysicsActor` base-constructor argument
setup, which is `CPhysicsActor`'s unit, not this one.

WALL: CScriptDock::GetPlane 99.66% - one `addi` ordering inside the out-of-line `CPlane` ctor call;
7 further argument spellings measured this run (all 99.66% or lower, listed above) join the 11 from
the previous run and none moves it, and the ctor lives in a `Matching` unit.

## `fn_800B63DC` (636 B, 0.00%) - the first run's blocker description is partly wrong

It is the `CScriptDock(CStateManager&, const CEntityInfo&, CInputStream&)` stream constructor, and
it does **not** need a stream-loading `CEntityInfo`. `objdump -dr` on the retail object gives its
whole call list: `__ct__20SLdrEditorPropertiesFv`, `LoadTypedefSLdrEditorProperties__FR20SLdrEditorPropertiesR12CInputStream`,
`__nw__FUlPCcPCc` (size 744, file `lbl_803A7EF8`, line 0), `AllocateUniqueId__13CStateManagerFv`,
`LdrToEntityInfo__FRC11CEntityInfoRC20SLdrEditorProperties`, `__dt__20SLdrEditorPropertiesFv`, and
this unit's own `__ct__`. Every one of those already exists in the tree
(`include/MetroidPrime/ScriptLoader/Structs/SLdrEditorProperties.hpp`,
`include/MetroidPrime/CEntityInfo.hpp:315`), and `CScriptForgottenObject.cpp` shows the shape.
So the blocker is not a missing symbol - it is that 636 bytes of eight-hash-case loader code has to
be written and matched byte for byte, including the debug filename string that `__nw__FUlPCcPCc`
takes. I did not start it: with the name our object would emit
(`__ct__11CScriptDockFR13CStateManagerRC11CEntityInfoR12CInputStream`) not matching the retail
symbol `fn_800B63DC`, objdiff may not pair them at all, so the item would gain nothing. Not filed as
`NEW:` for the same reason as before: its success cannot be shown to raise any count from here.

## Lessons (general, not CScriptDock-specific)

- **A 99.6% that is two instructions of register order may be a *default argument*, not a callee.**
  `SendScriptMsgs` declares `TUniqueId uid = kInvalidUniqueId, EScriptObjectMessage msg = kSM_None`
  and `CEntity.hpp:25-28` says the defaults are load-bearing because "MWCC fills the arguments in
  after the frame layout, so every call site in one function shares a single outgoing stack slot".
  We spelled the defaults out anyway, which is invisible in the diff (same call, same registers,
  same constants) and cost one 4-byte temporary and a 4-byte shift on every spill in the function.
  **Grep the tree for call sites that spell out a defaulted argument before believing a
  sub-100% is the callee's fault.**
- **`value |= 1 << k` on a `CMaterialList` has two spellings and retail picks the slower one.**
  A literal shift constant is folded by mwcc to a `lis`/`addi` pair; the same shift read from a
  `.sdata` word is a real `__shl2i` call. Where retail calls `__shl2i` with a literal-pool `lwz`,
  the source read a **named `.sdata` constant**, not the enum: `extern "C" const EMaterialTypes
  lbl_804NNNN;` and use that. Values are readable out of `build/G2ME01/main.elf` by mapping the
  address through `.sdata`/`.sdata2`/`.data` in `readelf -S`. This was worth +5.0% on
  `AcceptScriptMsg` in one step, and `docs/goal-notes/progress-cgp-doorbranch.md` already records
  the same trick for `lbl_80417E54`.
- **Reusing one local in two places is worth more than introducing it.** `switch (msg.GetMessage())`
  -> `const EScriptObjectMessage message = ...; switch (message)` moved `AcceptScriptMsg` 0.00%;
  adding `message == kSM_Increment` in the fallthrough case moved it 89.04 -> 92.15. A `const` local
  that a function reads once is free; mwcc still needs the expression twice otherwise.
- **`a && b` on two inlined accessors is not the same code as the conditions written out.** With
  `DoesAreaExist(x) && IsAreaValid(x)`, mwcc materialises the inner `&&` as a 0/1 bool and tests
  it (`li r3,0 / li r3,1 / clrlwi. r0,r3,24; beq`); retail short-circuits each comparison into its
  own branch. Spelling the three comparisons out is worth +2.2% and deletes nothing.
- **`tools/try_edit.py` scores a variant that failed to compile as if it had run.** It ignores
  ninja's exit status, so a variant naming an undeclared symbol silently reports the *previous*
  variant's score - six identical numbers, all wrong. `.tmp/opencode/try.py` (this lane, not
  committed) checks ninja's status and prints `BUILD FAIL`, and also builds each variant body from
  the file's *real* current text instead of a retyped copy - a retyped body cost me one round by
  changing `mgr.World()` into `mgr.GetWorld()` in a line I was not even testing.
- **A percentage that does not move is a real answer.** The ctor emits three `__shl2i` calls with
  the right constants and stays at 78.48%, exactly like thirteen other spellings. That is worth
  as much as a win: it says the difference is not in this file.

## Not filed as `NEW:`

Same three as the previous run, unchanged: `fn_800B63DC` (its blocker is the 636-byte loader body,
not a missing symbol, and it cannot be shown to raise a count), the `CPhysicsActor` constructor
argument setup behind `__ct__`, and the `CPlane` out-of-line constructor behind `GetPlane`. All
three are other units' code; file them against `MetroidPrime/Actors/CPhysicsActor` and
`Kyoto/Math/CPlane` if a lane wants them - both are `Matching`, so any fix has to be in their
*callers*, and this unit is one caller that has now been pushed as far as the callee allows.
