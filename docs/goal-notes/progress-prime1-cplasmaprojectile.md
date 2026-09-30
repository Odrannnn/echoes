# progress-prime1-cplasmaprojectile

`kind: progress`, target `MetroidPrime/Weapons/CPlasmaProjectile`. Unit stays `NonMatching`; the
result is the per-function exact-match count in `build/report.json`.

## Result

`tools/goal_check.sh build/goal/item.json` -> **PASS**.
`main/MetroidPrime/Weapons/CPlasmaProjectile`: **1 -> 11 / 21 matched functions**,
unit fuzzy 39.61% -> 80.58%, matched code 0.07% -> 26.82%.
Whole build: `All: 30.05% fuzzy, 21.90% matched, 11.74% linked (9747 / 28465 functions)`
(measured; base was 29.98% / 21.86% / 9737). DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`,
`probe_sources.sh` 744 files 0 failed, `check_symbol_names.py` 0 missing.

Only `src/MetroidPrime/Weapons/CPlasmaProjectile.cpp` is touched (plus the includes it needed).
No header, no `configure.py`, no `splits.txt`, no `asm`.

## Per function: before% -> after%, and what it took

`before%` is what `build/report.json` said when the item was queued; `after%` is the measured
value now. "Prime 1 verbatim" means Prime 1's `src/MetroidPrime/Weapons/CPlasmaProjectile.cpp`
body compiles here unchanged apart from the names/headers this repo forces.

| function | before | after | how |
|---|---|---|---|
| SetInitialDamage | 75.00 | **100.00** | swapped the two statements: retail stores the bitfield **before** the float. Prime 1 is the same order. |
| UpdateLights | 1.02 | **100.00** | Prime 1 verbatim, but `AUTO(it, ...)` -> `rstl::vector<TUniqueId>::iterator it` (this repo has no `AUTO`). |
| DeletePlasmaLights | 2.38 | **100.00** | Prime 1 verbatim, same `AUTO` -> explicit-iterator edit. Emits the `fn_801197E8` vector-assign helper as a side effect. |
| CreatePlasmaLights | 1.22 | **100.00** | Prime 1 verbatim; two fixes: `GetAreaId()` -> `GetAreaIdForPersistence()`, and `mLights.push_back(id)` -> `push_back_unsafe(id)` (retail has no capacity check; `reserve(3)` precedes it). |
| SetLightsActive | 46.05 | **100.00** | the index loop had to become an iterator loop; the index form does not match retail's pointer walk. |
| RenderMotionBlur | 0.81 | **100.00** | Prime 1 verbatim; needed `#include "Kyoto/Graphics/CGX.hpp"` and `CGraphics.hpp` (both exist here). |
| UpdateEnergyPulse | 11.72 | **100.00** | Prime 1 verbatim. |
| ResetBeam | 87.25 | **100.00** | Prime 1 verbatim: `mFiring = false` moves **inside** each branch, and `mBeamAngle = 0.f` is written **twice** (retail really stores 1536 twice). |
| AddToRenderer | 47.88 | **100.00** | Prime 1 body, but the gate is `mBeamAttributes & 2` (not `& 1`) and the contact gen is null-checked first. Needed `#include "MetaRender/CCubeRenderer.hpp"` for `gpRender`. |
| CanRenderUnsorted | 100.00 | 100.00 | already matched at queue time. |
| UpdateBeamState | 94.12 | **100.00** | only the particle-count test: `!get() \|\| == 0` -> `get() ? GetParticleCountAll() <= 0 : true`. `<= 0`, not `== 0` - `<=` is what makes retail's `cntlzw/srwi` pair. |
| Fire | 70.49 | 99.72 | Prime 1 verbatim, plus the cache loop must be `for (i = 0; i < 8; ++i)` over a `PointCache()` reference hoisted **before** the flag writes. The last 0.28% is a register choice (`li r3,1` vs retail `li r4,1`) after the virtual `SetActive` call; three instructions, not a real difference. |
| UpdateFx | 52.12 | 98.39 | Prime 1 verbatim plus the two TODO blocks (contact-gen orientation/pulse, muzzle gen). Two things had to be right: the point-cache shift is `for (i = 1; i < 8; ++i) { idx = 8 - i; ... }` (not a descending loop), and the muzzle gen is `Update; SetGlobalTranslation(xf.GetColumn(kDY)); SetParticleEmission(true); SetGlobalScale(mMuzzleScale); Update` - that exact call order is what took 93.72 -> 98.39. |
| Render | 0.67 | 91.75 | Prime 1 verbatim, adapted: no visor flag in Echoes, and the four layers are gated on `mBeamAttributes & 0x10 / 0x20 / 0x40 / 0x80` rather than unconditional. Needed `CCameraManager.hpp` + `CRelAngle.hpp`; `mgr.GetCameraManager()` takes an index here, so it is `GetCameraManager(0)`. |
| RenderBeam | 0.41 | 90.59 | Prime 1 verbatim. |
| AcceptScriptMsg | 5.57 | 75.28 | see below. |
| __sinit_CPlasmaProjectile_cpp | 42.67 | 42.67 | not started; see "left" below. |
| ctor | 93.60 | 93.60 | not started. |
| UpdatePlayerEffects | 0.34 | 0.34 | not started. |
| MakeBillboardEffect | 1.75 | 1.75 | not started. |
| fn_801197E8 | (none) | (none) | retail's `rstl::vector<TUniqueId>::operator=`; now emitted by `DeletePlasmaLights`, but objdiff gives it no score. |

`fn_801197E8` is counted by `total_functions` and never scores, so 11 of 21 is really 11 of 20
scored.

## AcceptScriptMsg: what Prime 1 does not tell you

Prime 1's `AcceptScriptMsg(EScriptObjectMessage, TUniqueId, CStateManager&)`; Echoes' is
`(CStateManager&, const CScriptMsg&)`, so the switch is on `msg.GetMessage()`. The two retail
message codes are **`kSM_XCRT` (0x58435254) and `kSM_XDelete` (0x5844454c)** - not `kSM_Registered`
/ `kSM_Deleted`, which do not exist in this repo's `EScriptObjectMessage` at all.

The desc access needs a spelling this repo cannot express directly: `mProjectile.GetWeaponDescription()`
returns `TLockedToken<CWeaponDescription>`, and neither `TLockedToken::GetTag()` nor a
`->` on it exists. `TToken< CWeaponDescription > desc = mProjectile.GetWeaponDescription();` gives
both (`desc->mAPSM` and `desc.GetTag()`) and is what the code now uses.

Left at 75%: retail's `kSM_XCRT` arm first builds a locked `CToken` copy of the description
(`fn_80033B6C`) and tests `desc->mAPSM`'s loaded byte; ours calls `GetWeaponDescription()` +
`__ct__6CTokenFRC6CToken` + `__dt__6CTokenFv` + `GetObj` instead - four calls where retail has one.
This needs `CProjectileWeapon` to expose its `mWeaponDesc` as a `TToken`, which is a header
change, so it is out of scope for this item.

## Left, and what the next run should not repeat

- **`UpdatePlayerEffects` (1188 B, 0.34%)** - the port-side blockers are real, not cosmetic:
  `CPlayer::IncrementEnvironmentDamage` / `DecrementEnvironmentDamage`, `SetFrozenState`,
  `TryToBreakOrbit` and `CPlayer::kOB_ActivateOrbitSource` **do not exist** in this repo's headers
  (grepped `include/`). Echoes also renamed Prime 1's `mActivePlayerPhazon` to something else - the
  constructor initialises `mSustainedDamagePlayerId` and `AcceptScriptMsg` retail calls
  `PopSustainedDamage__7CPlayerFv`, so the ownership moved from a bool to a player id. Do not
  re-derive this from Prime 1; read `0x8011add4..0x8011b278` and work from the members Echoes has.
- **`MakeBillboardEffect` (228 B, 1.75%)** - there is no `CHUDBillboardEffect` in this repo
  (`include/MetroidPrime/ScriptObjects/` has only `CScriptHUDMemo.hpp`), and Echoes' signature has
  a fifth `uint playerMask` argument Prime 1 lacks.
- **`__sinit_CPlasmaProjectile_cpp` (60 B, 42.67%)** - small, but the shape is wrong: retail loads
  `f1` and `f0` from `.rodata` and divides (`fdivs f0,f1,f0`) for `kInvMaxPlasmaLights`, and ours
  emits an extra `lq r0,-24576(r3)` pair plus the same divide. Retail's is 15 instructions to
  ours' 17. Likely `kMaxPlasmaLights` must be a compile-time constant the compiler can fold
  (e.g. an enum or a literal in the `.cpp`), not a `static const int` in the class.
- **ctor (2700 B, 93.60%)** - untouched. The prologue already differs (retail `-0x3b0` frame,
  ours `-0x3a0`), so the register allocation is off from the first instruction; that is a
  parameter-passing-shape question, not a body question.
- **`unit_fit.sh` reports `.sbss` over by 13 bytes** (ours 21, retail 8). This is **pre-existing** -
  verified by stashing the change and re-running: the same 13 bytes over on the base commit. The
  extras are COMDAT weak template copies (dtors, `reserve`, `operator=`), which per the tool's own
  note are usually harmless. The `__sinit` work above is the most likely real cause of part of it.

Do **not** re-try: `push_back` vs `push_back_unsafe` in `CreatePlasmaLights` (the checked form
emits 5 extra instructions and cannot match); `== 0` vs `<= 0` in `UpdateBeamState` (`== 0` gives
`cntlzw`+`srwi` where retail has `cntlzw`+`srwi` too but the operand path differs - `<= 0` is the
one that matches); index loops over `mLights` in `SetLightsActive` / `DeletePlasmaLights` /
`UpdateLights` (retail walks a pointer; the iterator loop is the only spelling that matches).
