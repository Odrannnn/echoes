# progress-prime1-ctargetreticles

`kind: progress`, `target: MetroidPrime/CTargetReticles`. The unit stays `NonMatching`.

## Result, measured

`build/report.json`, `main/MetroidPrime/CTargetReticles`:

| | before | after |
| --- | --- | --- |
| `matched_functions` | 13 | **16** |
| `total_functions` | 44 | 44 |
| `fuzzy_match_percent` | 11.07 | 15.39 |

Whole-build: `All: 32.43% fuzzy, 25.04% matched, 11.94% linked (11265 / 28465 functions)`;
`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`build/gate-diff.log` says `matched 11262 -> 11265   linked 5507 -> 5507   (+3 functions at 100%,
0 units newly linked)` and `no regression`;
`./tools/goal_check.sh build/goal/item.json` -> `PASS`.

Three functions reached an exact match:
`IsGrappleTarget`, `UpdateOrbitZoneGroup`, `InterpolateWithClamp`.

## Per function: before %, after %, and what Prime 1's source needed

Read from `/run/media/odran/Leo/projects/Restored-projects/Chatgpt/prime-ref/src/MetroidPrime/CTargetReticles.cpp`
and adapted to this tree. "small edits" means the body had to be rewritten against Echoes' own
header, member names and callees.

| function | before | after | Prime 1's source |
| --- | --- | --- | --- |
| `IsGrappleTarget__22CCompoundTargetReticleF9TUniqueIdRC13CStateManager` | 8.75% | **100%** | matched unchanged in shape; Echoes drops `kOL_All` (`mgr.GetObjectById(id)`), and `TCastToConstPtr` is needed because this tree's const `GetObjectListById` returns `const CEntity*`. Small edits. |
| `UpdateOrbitZoneGroup__22CCompoundTargetReticleFfRC13CStateManager` | 1.33% | **100%** | `mUnk` is `x294` (+0x294, same slot as Prime 1's `mUnk`); the crosshairs scale is **+0x270** (`mCrosshairsDrawScale`), not `mCrosshairsScale` (+0x268); the tweak read is `gpTweakTargeting->GetCrosshairsFadeInOutTime()` (a real call, 0x80213ECC), not the `mCrosshairsScaleDur` field; and the gate is `CPlayer` bit 5 of the byte at 0x1268 plus `GetCurrentVisor() != kPV_Scan` (visor enum value 2). Small edits, but the fade field name is wrong in our header. |
| `InterpolateWithClamp__25CTargetReticleRenderState...` | 86.48% | **100%** | the float half is byte-identical; **the target-id tail had to go through accessors** - `out.SetTargetId(b.GetTargetId())` etc. Direct `out.mTarget = b.mTarget;` leaves the 32-byte frame and the five dead `sth` stores to +8/+12/+16/+20/+24 that retail has. Prime 1 spells it with the setters, and Prime 1 is Matching, so the setter spelling was already the answer. |
| `Draw__22CCompoundTargetReticleCFRC13CStateManagerb` | 1.16% | 98.84% | small edits: per-player `mgr.GetCameraManager(mPlayerIndex)->...` with the `true` selector, and Echoes has six draw groups (`DrawSeeker`, `DrawCrosshairs`, `DrawScanTargetGroup` do not exist in Prime 1). One instruction short - see the blocker below. |
| `UpdateTargetParameters__22CCompoundTargetReticleFR25CTargetReticleRenderStateRC13CStateManager` | 1.49% | 96.99% | `mPrevState == kRS_XRay \|\| kRS_Thermal` becomes `kRS_Echo \|\| kRS_Dark` (values 2 and 3, which is what retail compares). Small edits; the remaining delta is scheduling only - see the wall. |
| `CalculateOrbitZoneReticlePosition__22CCompoundTargetReticleCFRC13CStateManagerb` | 7.16% | 81.00% | same shape, three Echoes differences: `mgr.GetCameraManager(mPlayerIndex)->GetCurrentCamera(mgr, true)`, `mgr.GetPlayer(mPlayerIndex)->GetTweakPlayer()->GetOrbitZoneHeight(0)` instead of the global tweak, and **`tan(...)` called directly instead of `CMath::SlowTangentR`** (this tree's `SlowTangentR__5CMathFf` is an unresolved DOL global, so it cannot be inlined; retail's body inlines to `bl tan` + `frsp`). `CCast::LtoF` reproduces the int->double->float sequence exactly. |

Not attempted, with the reason: `CalculateClampedScale` (504 B) needs a `{1.f, .8f, .6f}` table at
0x803A7C20 indexed by `CStateManager::fn_80036B6C()`, and its translation reads come off the camera
at 0x30/0x40/0x50, which I could not pin down against this tree's `CGameCamera` layout;
`Draw__17CTargetingManagerCFRC13CStateManagerb` needs `gpRender`'s virtual slot 0x5C fed from two
`.bss` ints at 0x803C9FE8+8/+12, i.e. runtime-initialised globals, not constants;
`DrawOrbitZoneGroup` and the `DrawCurrLockOnGroup` / `DrawNextLockOnGroup` / `DrawSeeker` /
`DrawScanTargetGroup` family are Echoes-specific (quarter-curve texture, scan-target brackets,
seeker-missile lock confirm) and have no Prime 1 counterpart to start from.

## Blocked, with the evidence

**`Draw__22CCompoundTargetReticle` is one instruction from 100%, and getting it breaks the gate.**
Retail's `Draw` does `mr r3,r29; mr r5,r30; addi r4,r1,92; bl DrawCrosshairs` (0x800B0AA4) - it
passes the state manager to a two-parameter function that never reads `r5` (checked: nothing in
0x800AE33C..0x800AE4A8 touches the incoming `r5`). Our object emits the `bl` without the
`mr r5,r30`, which is the whole 4-byte difference. Declaring our `DrawCrosshairs` as
`(const CMatrix3f&, const CStateManager&)` does produce 100% (measured: 17/44), **but it renames
our symbol** to `DrawCrosshairs__22CCompoundTargetReticleCFRC9CMatrix3fRC13CStateManager`, objdiff
then has no partner for retail's `DrawCrosshairs__22CCompoundTargetReticleCFRC9CMatrix3f`, the
report drops it to `None` percent, and `tools/report_diff.py` reports
`GONE main/MetroidPrime/CTargetReticles :: DrawCrosshairs__22CCompoundTargetReticleCFRC9CMatrix3f
(was 1.10%)`, which fails `gate.sh`. I reverted it. So the real answer is upstream: either
`config/G2ME01/symbols.txt`'s two-parameter name for `DrawCrosshairs` is wrong, or retail's source
spelled the call differently from every signature its own linker recorded. `Draw` is left at
98.84% and the gate is clean, which is the right trade.

WALL: UpdateTargetParameters__22CCompoundTargetReticleFR25CTargetReticleRenderStateRC13CStateManager 96.99% - the instruction multiset is identical to retail's; only the two `sth` (to +8/+12) and the `lwz` order differ, and none of the four spellings tried moves both.

## Spellings measured for `UpdateTargetParameters` (all 97%, none 100)

Retail 0x800AD174-0x800AD188: `lhz r0,0(r4); addi r4,r1,12; lwz r3,2064(r5); sth r0,8(r1);
sth r0,12(r1); bl`. Ours, four ways:

1. `GetObjectById(state.GetTargetId())` inline in the `if` - 96.99%. Emits `sth +12` then `sth +8`,
   `addi r4,r1,12`, `lwz` last. Same slots as retail, stores reversed.
2. `const TUniqueId id = state.GetTargetId();` then `GetObjectById(id)` - the compiler folds the
   copy, so only one `sth` remains (structurally worse).
3. `TUniqueId id = state.GetTargetId();` (non-const) then `GetObjectById(id)` - 97.00%. Store order
   matches retail (`sth +8` then `sth +12`) but the compiler passes `addi r4,r1+8`, retail passes
   `r1+12`.
4. `TCastToPtr<CActor>` with a non-const list - does not compile here; `GetObjectListById` only has
   a const overload, and `TCastToConstPtr` emits the same `TCastToPtr<6CActor>__FP7CEntity` call.

## Other changes in the tree, and why

- `include/MetroidPrime/CTargetReticles.hpp`: added `CTargetReticleRenderState`'s getter/setter
  pair for each member. Required - `CCompoundTargetReticle` cannot touch another class's private
  members, and the setter spelling is what takes `InterpolateWithClamp` from 86% to 100%.
  No member added, so `CHECK_SIZEOF(CTargetReticleRenderState, 0x20)` is untouched.
- `include/MetroidPrime/Player/CPlayer.hpp`: added inline `IsCrosshairsOpen() { return
  mDrawCrosshairs; }`. Note `x1268_29_` looks like the right field by name but compiles to
  `rlwinm. r0,r0,30,31,31` (bit 1), while retail emits `26,31,31` (bit 5) - MW packs the first
  declared `bool : 1` at bit 6 of the byte, so this repo's `xNNNN_29_` names are off by one from the
  bit they occupy. `mDrawCrosshairs` is the field at bit 5. Method only, no layout change.
- `docs/research/port_link_gap_list.md` / `port_link_gap.md`: the two new symbols the decompiled
  bodies now reference (`TCastToPtr<CScriptGrapplePoint>`, `CGameCamera::GetFov`) had to be listed,
  or `gate.sh`'s `link_gap.py` check fails with "gap grew". Regenerated with
  `python3 tools/link_gap.py --write-list`; the diff is exactly those two entries and the group
  count 160 -> 162, plus the paragraph in `port_link_gap.md` saying what provides each. Both
  definitions exist on disk (`src/MetroidPrime/TypesMatch.cpp` via `CAST_TO_IMPL`,
  `src/MetroidPrime/Cameras/CGameCamera.cpp:160`) and are missing only because neither file is in
  `files.cmake`.

## Codegen rules learned (not `NEW:` items)

- When retail passes an argument a callee never reads, MW still emits the `mr`; the only way to
  reproduce it from C++ is to have the callee declared with that parameter, which then renames the
  symbol. Weigh that against `report_diff`'s `GONE` rule before doing it.
- A member function returning a class type by value gets a hidden sret pointer in `r3`, so
  `this` is `r4` - worth remembering before reading a register as a parameter.
- `bool : 1` fields here are allocated from bit 6 of their byte downwards; the `xNNNN_24_`,
  `xNNNN_25_`... names in `CPlayer.hpp` are index labels, not bit numbers.
- Dead stores are not eliminated. Retail keeps frames and locals that carry no value; matching
  them means spelling the source the way retail did, not writing tidier code.