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
