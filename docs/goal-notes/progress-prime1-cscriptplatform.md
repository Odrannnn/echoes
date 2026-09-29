# progress-prime1-cscriptplatform

`MetroidPrime/ScriptObjects/CScriptPlatform` — `progress` item, unit stays `NonMatching`.

## Result

| | before | after |
|---|---|---|
| unit `matched_functions` | **10** / 60 | **20** / 60 |
| unit `matched_code` | 476 B | 1916 B (of 18000) |
| unit `fuzzy_match_percent` | 15.83% | 21.83% |
| tree `matched_functions` | 9678 / 28465 | 9688 / 28465 |

**+10 functions at 100%, 0 worse, 0 asm added** (that is `tools/gate.sh`'s own per-function
diff line, quoted).

## What I did

Read every retail body out of `build/G2ME01/obj/MetroidPrime/ScriptObjects/CScriptPlatform.o`
with `objdump -d` plus `objdump -r` (the relocation at each `bl` names the callee, which is what
turns a guess into a measurement), and wrote the ten that Prime 1's decomp also covers, plus the
five Echoes-only ones Prime 1 has no analogue for. `.tmp/opencode/fdiff2.py` (gitignored helper)
prints, per function, the instructions whose raw bytes differ between the retail object and ours;
`tools/fast_try.sh` gives the per-function percentages. Loop used: `./tools/fast_try.sh
MetroidPrime/ScriptObjects/CScriptPlatform` (~3 s), so every number below is measured, not
recalled.

### Per function: before% → after%, and whether Prime 1's source was usable

| function | before | after | Prime 1 (`prime-ref/src/MetroidPrime/ScriptObjects/CScriptPlatform.cpp`) |
|---|---|---|---|
| `IsRider__…CF9TUniqueId` | 0.00 | **100.00** | structure used unchanged; needed Echoes edits (below) |
| `GetCollisionPrimitive__…CFv` | 89.62 | **100.00** | used unchanged, but the branch had to be **inverted** (`if (mTreeGroup.null()) return base;` first) |
| `GetPrimitiveTransform__…CFv` | 98.06 | **100.00** | `xf.AddTranslation(GetPrimitiveOffset())` — ours had `SetTranslation(GetTranslation() + …)`, a one-line edit |
| `IsInMovedList__…RCQ24rstl24reserved_vector<Us,1024>` | 55.28 | **100.00** | Prime 1's loop, but **Echoes masks the id**: `id.Value() & 0x3ff` |
| `GetAimPosition__…CFRC13CStateManagerf` | 25.45 | **100.00** | used unchanged |
| `StopMotion__…Fv` | 95.43 | **100.00** | Echoes-only; one missing statement (below) |
| `TranslateMotion__…FRC9CVector3f` | 54.34 | **100.00** | Echoes-only (Prime 1 has waypoints, not splines) |
| `RotateMotion__…FRC11CQuaternionRC9CVector3f` | 68.28 | **100.00** | Echoes-only |
| `fn_800a1df8__…Fv` | 51.83 | **100.00** | Echoes-only (Prime 1's analogue is the `kSM_Reset` case of `AcceptScriptMsg`) |
| `AddRider__…F9TUniqueIdR13CStateManagerRCQ24rstl18optional_object<f>` | 62.32 | **100.00** | Echoes-only overload; retail copies the timer to the stack before forwarding it |
| `IsSlave__…CF9TUniqueId` | 23.19 | 95.29 | Prime 1's two `rstl::find`s, same Echoes edits; only a register-number swap left |
| `GetTouchBounds__…CFv` | 46.90 | 93.10 | Prime 1 unchanged except Echoes' `mTreeGroup` is a `CCollisionPrimitive*`, not a `CCollidableOBBTreeGroup*` |
| `BuildNearListFromRiders__…` | 87.03 | 98.01 | Prime 1's iterator loop; needed `end` hoisted (below) |
| `GetSortingBounds__…CFRC13CStateManager` | 32.70 | 92.12 | Prime 1 unchanged (identical to `CScriptActor::GetSortingBounds`, which is already in the tree) |

The Echoes edits Prime 1's rider/slave helpers need, measured from the relocations at `IsRider`
(`+0xfd4 __ct__7SRidersF9TUniqueIdRC12CTransform4fRCQ24rstl18optional_object<f>`): Echoes' `SRiders`
has **no one-argument constructor**, so the temporary Prime 1 writes as `SRiders(riderId)` is
`SRiders(id, CTransform4f::Identity(), rstl::optional_object< float >())` here, and Echoes' retail
`IsRider` contains **no** vector-erase instantiation, so Prime 1's `if (false) { riders.erase(…) }`
trick must be left out. `IsSlave` is the same spelling twice.

## Codegen rules measured here (worth keeping)

- **`rstl::find` is what retail's rider/slave searches are.** The retail loop is
  `cursor = begin; while (cursor != end && !(cursor->mUid == temp.mUid)) ++cursor; return cursor != end;`
  — including the `subf/or/srwi` return, which is `it != end` on a pointer iterator. A hand-written
  `for (int i…)` produces a completely different body.
- **GCC strength-reduces a `for (it = v.begin(); it != v.end(); ++it)` loop into an index loop**
  (`addi r28,r28,1; cmpw r28,r31; blt` instead of `cmplw r30,r31; bne`). **Hoisting the end iterator
  into a local first defeats it** and reproduces retail exactly: `BuildNearListFromRiders` went
  87.03% → 98.01% on that one change, and nothing else changed. A raw `const SRiders*` loop does *not*
  defeat it.
- `CTransform4f::AddTranslation(v)` is `m0i += v.GetXi()` in that order, which is what retail's
  three `fadds`/`stfs` pairs in `GetPrimitiveTransform` look like; `SetTranslation(GetTranslation() + v)`
  allocates the same three registers in the wrong order and cannot match.
- Retail's `AddRider(TUniqueId, CStateManager&, const optional_object<float>&)` forwarder **copies**
  the timer onto the stack (8 bytes at `r1+12`) before passing `&copy`, even though the callee also
  takes it by const reference. Spelling the argument as a copy-constructed temporary reproduces the
  88 bytes exactly.
- `fn_800a1df8` is `x48d_25_ = true; if (mMotionFlags & 8) { mMotionActive = true; } else
  { StopMotion(); } mDead = false; mHealth = mInitialHealth;` — the `mDead = false` and the health
  copy are **outside** the if/else (retail's `b` skips only the `bl StopMotion`). Getting that
  scope wrong costs the last two instructions and the function sits at 99.89%.
- Echoes' bitfield packing in this class is 8-per-byte, declared order, **bit 7 first** (measured:
  `mDead` = byte `0x48c` bit 7, `mMotionActive` = bit 3, `mMotionTransformed` = byte `0x48d` bit 5,
  `x48d_25_` = bit 6). `mMotionTransformed` and `mMotionActive` land where retail has them, so the
  header's bitfield block is right.
- The member offsets this unit's *matching* functions use are already correct and are confirmed by
  the matches themselves: `mDragDelta` 0x2ec, `mRotationDelta` 0x2f8, `mPreviousRotation` 0x308,
  `mCurrentRotation` 0x338, `mInitialHealth` 0x368, `mHealth` 0x388, `mTreeGroup` 0x3e8,
  `mRiders` 0x3ec, `mStaticSlaves` 0x3fc, `mDynamicSlaves` 0x40c, `mBoundsTrigger` 0x424,
  `mMotionFlags` 0x434.

## Header change, and why

One line of `include/MetroidPrime/ScriptObjects/CScriptPlatform.hpp`: the member at offset 0x42c was
declared `CPlatformSplineController*`, an unscaffolded forward declaration. Retail's
`TranslateMotion`/`RotateMotion` call `PositionSpline__11CGameSplineFv` on it (relocation at
`TranslateMotion+0x28` and `RotateMotion+0x30`) and then `CMotionSpline::Translate` /
`CMotionSpline::Rotate` on the result, and `CGameSpline::PositionSpline()` / `CMotionSpline::Translate`
/ `CMotionSpline::Rotate` are exactly those symbols in this tree. It is now `CGameSpline*`. That is a
pointer-to-pointer type change — **no layout change**, `CHECK_SIZEOF` still passes, and no other file
includes this header (`grep -rn 'CScriptPlatform.hpp' src/ include/` returns only itself), so no
other unit was recompiled or moved.

## What is left, and why

Two functions are one instruction-scheduling decision away and are recorded here rather than as
`NEW:` items, because they are a wall, not new work:

- `GetSortingBounds` 92.12%, `GetTouchBounds` 93.10% — in both, retail and we emit the *same*
  instructions; ours hoists one load across a register move that retail keeps in place. In
  `GetSortingBounds` the only difference is that retail loads `kInvalidUniqueId` before
  `mBoundsTrigger` and we load `mBoundsTrigger` first. **`CScriptActor::GetSortingBounds` is already
  at exactly 92.12% in this tree with the same source**, so this is a known, shared wall for that
  shape, not something specific to this unit. Spelling tried: hoisting the tree-group pointer into a
  local for `GetTouchBounds` (93.10%, no change).
- `IsSlave` 95.29% and `BuildNearListFromRiders` 98.01% — remaining diff is a cyclic shift of the
  five callee-saved registers (`[mgr, result, cursor, end, sret]` vs retail's
  `[sret, mgr, result, cursor, end]`). Declaring `end` before `result` instead of after moved
  `BuildNearListFromRiders` from 98.01% down to 92.19%; that is the only spelling I tried.

`__dt__` (77.78%), `RemoveRider` (35.22%), `DecayRiders` (1.33%), `MoveRiders` (0.45%), `PreThink`
(0.24%), `Think` (0.75%), `Move` (1.58%), `AcceptScriptMsg` (1.98%) and the fifteen `fn_800A*`
helpers are blocked on the two `NEW:` items below.

## Verified

```
sha1sum build/G2ME01/main.dol      -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/decomp_build.sh            -> All: 29.92% fuzzy, 21.74% matched, 11.74% linked (9688 / 28465 functions)
./tools/probe_sources.sh           -> probe: 744 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py-> checked 502 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptPlatform -> ok, none out of retail order
./tools/gate.sh build/goal/judge/report.base.json
```

`gate.sh` is **ok on every step except `docs claims`**, which reports only:

```
missing: 'matched    9688 / 28465 functions'  (HANDOFF state block: total matched)
missing: 'DOL units  8277 / 16726 functions'  (HANDOFF state block: DOL matched)
```

Those are the derived state-block numbers this change moved. The judge rewrites them itself —
`tools/gate.sh:115` runs `check_docs_claims.py ${MP_GATE_DOCS_WRITE:+--write}` and `goal_check.sh:105`
invokes the gate with `MP_GATE_DOCS_WRITE=1` — so per the brief I did not hand-edit
`docs/HANDOFF.md`. Its own `per-function diff` line reads
`matched 9678 -> 9688 linked 4894 -> 4894 (+10 functions at 100%, 0 units newly linked)`.

Diff is two files, `src/MetroidPrime/ScriptObjects/CScriptPlatform.cpp` and
`include/MetroidPrime/ScriptObjects/CScriptPlatform.hpp`; no `.s`, no `tools/`, no `docs/`, no
`build/goal/`. Not committed.

## The retail-only helper chain in this unit (measured, `objdump -r` call map)

We define none of these; retail's object puts each of them *after* its callers, and knowing the call
graph is most of the work. Sizes and callers, from `build/G2ME01/obj/.../CScriptPlatform.o`:

| symbol | size | called from |
|---|---|---|
| `fn_800A1004` | 76 B | `RemoveRider+0xb0`, `DecayRiders+0x80`, `MoveRiders+0x244`, `DragSlaves+0x1a8` — the out-of-line `rstl::vector<SRiders>::erase(iterator)` |
| `fn_800A1050` | 248 B | only `fn_800A1004+0x34` |
| `fn_800A1148` | 56 B | `fn_800A1050+0x38`, `fn_800A31A0+0x4c` |
| `fn_800A1180` | 28 B | only `fn_800A1148+0x24` |
| `fn_800A31A0` | 132 B | `PreThink+0x10c/0x17c/0x1c0/0x668`, `__dt__+0xb4/0xc0/0xcc` — a `rstl::vector` destructor |
| `fn_800A14DC` | 100 B | `AddSlave+0x144`, `BuildSlaveList+0xfc`, `AddRider(vector…)+0x22c` |
| `fn_800A4038` / `fn_800A4090` | 88 B each | `__dt__+0xa8` / `__dt__+0x34/0x40/0x4c` |

So the chain is `1004 → 1050 → 1148 → 1180`, reachable only from the four rider/slave vector
functions, with `31A0` separate. Note for whoever takes it: our `RemoveRider` *does* reach an erase
(our object instantiates `rstl::vector<SRiders>::erase`), so the fix there is not "call something
missing" but "make our erase's bytes equal retail's out-of-line helper" — the `rstl::vector::erase`
in this repo does not match yet.

## NEW:

NEW: progress-prime1-cscriptplatform-dtor | progress | MetroidPrime/ScriptObjects/CScriptPlatform | __dt__ is 77.78% and cannot be written until CScriptPlatform's member order above 0x424 is resolved: retail's dtor deletes 0x42c and 0x440 through vtable[2] and frees 0x444/0x448/0x44c with fn_800A4090, while its bitfields sit at 0x48c/0x48d, so mInitialTransform must start at 0x44c and the three offsets the dtor frees cannot all be spline pointers - the header's order contradicts the measured dtor and needs re-deriving from the dtor and the ctor before the dtor (and probably the 42.52% ctor) can match
NEW: progress-prime1-cscriptplatform-ridervec | progress | MetroidPrime/ScriptObjects/CScriptPlatform | the erase chain fn_800A1004 -> fn_800A1050 -> fn_800A1148 -> fn_800A1180 does not exist in our object, and retail's RemoveRider, DecayRiders, MoveRiders and DragSlaves all call into it (RemoveRider+0xb0, DecayRiders+0x80, MoveRiders+0x244, DragSlaves+0x1a8), so those four cannot match until the out-of-line rstl::vector<SRiders>::erase reproduces it
NEW: progress-prime1-cscriptplatform-slavevec | progress | MetroidPrime/ScriptObjects/CScriptPlatform | fn_800A14DC (100 B) is undefined in our object and is called from AddSlave+0x144, BuildSlaveList+0xfc and AddRider(vector)+0x22c, so those three functions cannot match until it exists
