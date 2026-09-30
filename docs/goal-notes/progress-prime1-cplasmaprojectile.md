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

---

# Second run (lane-2 worktree, 2026-09-30). Base commit 07774a1f, tree was clean.

## Result

`./tools/goal_check.sh build/goal/item.json` -> **PASS**.
`main/MetroidPrime/Weapons/CPlasmaProjectile`: **12 -> 14 / 21 matched functions**
(unit fuzzy 80.90% -> 82.33%, matched code 27.38% -> 39.00%).
Whole build **11222 -> 11224** matched functions; `All: 32.36% fuzzy, 24.93% matched,
11.94% linked (11224 / 28465 functions)`. DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
Only `src/MetroidPrime/Weapons/CPlasmaProjectile.cpp` is touched. No header, no
`configure.py`, no `splits.txt`, no `asm`.

`__sinit_CPlasmaProjectile_cpp` was already at 100% when this run started (the tree is newer
than the note above), so 14 of 20 scorable functions match; `fn_801197E8` still never scores.

## Per function: before -> after, and what it took

| function | before | after | how |
|---|---|---|---|
| RenderBeam | 90.59 | **100.00** | **restore Prime 1's operator precedence.** See below. |
| Fire | 99.72 | **100.00** | one word: `const bool flag` on the *definition's* third parameter. |
| UpdateFx | 98.40 | 98.62 | the muzzle-gen block's first call is `SetGlobalOrientation(xf)`, not `Update(dt)`. |
| AcceptScriptMsg | 75.28 | 85.84 | added the Echoes sustained-damage pop tail after the switch. |
| ctor / Render / UpdatePlayerEffects / MakeBillboardEffect | 93.60 / 91.75 / 0.34 / 1.75 | unchanged | see "still open". |

### 1. RenderBeam: the previous run "fixed" a real bug in Prime 1's source, and that cost the match

Prime 1 (and Echoes retail) write

    const float uvY1 = uvY0 + ((flags & 3) == 3) ? 2.f : 0.5f * GetCurrentLength();

`+` binds tighter than `?:`, so this is `(uvY0 + ((flags&3)==3)) ? 2.f : 0.5f*len` - a
comparison of the float `uvY0+bool` against zero, not a conditional addend. The previous run
silently re-parenthesised it to `uvY0 + (((flags&3)==3) ? 2.f : 0.5f*len)`, which is what the
code *should* say, and lost the function. Restoring Prime 1's exact text took it 90.59 -> 100.00
with nothing else changed. General rule for this port: **when Prime 1 is `Matching`, its source
is the specification - do not tidy it.** Check the operator precedence before assuming a line is
what it looks like.

Retail's codegen for that line is the tell: `lis r5,17200` / `xoris r0,r4,32768` / `stw r0,44(r1)`
/ `stw r5,40(r1)` / `clrlwi r0,r27,30 / subfic r0,r0,3` / `cntlzw` / `rlwinm` / `stw r0,52(r1)`,
then `lfd f0,40(r1)` / `fsubs f4,f0,f2` / `fcmpu` / `beq`. The `stw r5,48(r1)` / `lfd f1,48(r1)`
pair is retail materialising a double out of `{176.0f, <the bool>}` on the stack; it is not
something you can write directly, it falls out of the expression.

### 2. Fire: top-level `const` on a by-value parameter changes MWCC's temp allocation

Retail uses `r4` for the `1` it materialises after the `SetLightsActive` call; MWCC here picks
`r3`. The only difference in the whole function is those three instructions. Adding `const` to
the parameter **in the definition** (Prime 1 has `const bool b`) moves the temp to `r4` and
matches the function. The header is untouched, so the signature is unchanged.
Worth trying on other units whose only diff is a `li r3`/`li r4` pair.

### 3. UpdateFx: the muzzle gen's first call

CElementGen's vtable in this repo (measured from the calls that already match):
0x0C `Update(double)`, 0x14 `SetOrientation`, 0x18 `SetTranslation`, 0x1C
`SetGlobalOrientation`, 0x20 `SetGlobalTranslation`, 0x24 `SetGlobalScale`, 0x2C
`SetParticleEmission`. Retail's muzzle block is
`SetGlobalOrientation(xf); SetGlobalTranslation(xf.GetTranslation()); SetParticleEmission(true);
SetGlobalScale(mMuzzleScale); Update(dt);` - the previous run had `Update(dt)` first (98.40 ->
98.62). Prime 1 has no muzzle block at all; this block is Echoes-only and had to be read out of
the object.

### 4. AcceptScriptMsg: the sustained-damage pop

Echoes' `AcceptScriptMsg` ends, after the `switch` and before
`CGameProjectile::AcceptScriptMsg`, with (retail `0x6d0..0x708`):

    if (mSustainedDamagePlayerId != kInvalidUniqueId) {
      if (CPlayer* player = TCastToPtr<CPlayer>(mgr.ObjectById(mSustainedDamagePlayerId))) {
        player->PopSustainedDamage();
      }
      mSustainedDamagePlayerId = kInvalidUniqueId;   // reached from both arms of the inner test
    }

`CPlayer::PopSustainedDamage()` and `mSustainedDamagePlayerId` both already exist here; only
`#include "MetroidPrime/Player/CPlayer.hpp"` was needed. 75.28 -> 85.84.

## Still open, measured on this tree

- **Render (91.75%) - blocked on CStateManager's layout.** Retail loads the camera manager with
  `lwz r4,5632(r29)`; our `mgr.GetCameraManager(0)` compiles to `lwz r4,5404(r30)`. That member
  (`CStateManager::m_cameraManagers`, the last member of the class in
  `include/MetroidPrime/CStateManager.hpp`) is 228 bytes earlier in our CStateManager than in
  retail's, so this one instruction cannot match without changing CStateManager's layout - which
  would move every offset in every function that touches it. Not this item's job. Retail also
  holds `mgr` in `r29` and hoists `&mInnerColor`/`&mOuterColor` in `r30`/`r29` across the
  `RenderBeam` calls; we keep `mgr` in `r30` and recompute the addresses.
- **AcceptScriptMsg (85.84%), two remaining gaps.** (a) retail reads the third word of
  `GetWeaponDescription()`'s return slot directly; we emit
  `__ct__6CTokenFRC6CToken` + `__dt__6CTokenFv` + `GetObj` because
  `include/Weapons/CProjectileWeapon.hpp:56` returns `TLockedToken<CWeaponDescription>` **by
  value**, and the source then binds it to a `TToken<>` copy. Fixing it means changing that
  getter's return type, which also feeds `CEnergyProjectile.cpp:147,154,194,291` and
  `CGameProjectile.cpp:105,107,199`. (b) retail, before `mgr.AddWeaponId`, has an extra
  `if (mInitialDamageEnabled) { <halfword at 298> = GetOwnerId(); }` (retail `0x660..0x678`).
  298 = 0x12A is inside `CEntity`, and I did not find a member of ours at that offset that this
  class can name; do not guess it.
- **ctor (93.60%)** - retail's frame is `-432` with `r22..r31` saved at 392 and **no** FPR save;
  ours is `-416` with `r22..r31` at 360 plus `f31` at 400. Ours holds a float in `f31` across a
  call that retail does not, and has 32 fewer bytes of locals. Parameter/temporary shape, not
  body.
- **UpdatePlayerEffects (0.34%) / MakeBillboardEffect (1.75%)** - unchanged stubs; the notes
  above still hold (missing `CPlayer::Increment/DecrementEnvironmentDamage`, `SetFrozenState`,
  `TryToBreakOrbit`, no `CHUDBillboardEffect`). Both are far too big to bring to 100% in one
  item, and neither would raise `matched_functions` on its own.
- `unit_fit.sh` still reports `.sbss` 13 bytes over (pre-existing, COMDAT weak template copies).

WALL: CPlasmaProjectile::UpdateFx 98.62% - the four remaining diffs are MWCC's temp register (it
picks r3 where retail picks r4) plus one int->bool `clrlwi` on the `CauseDamage` argument;
ten spellings tried (below) all scored lower.

Do **not** re-try on `UpdateFx`, all measured lower than 98.62:
`CauseDamage(A | B)` 90.47; `CauseDamage(A + B)` 91.20; `CauseDamage(A ? true : B)` 96.79;
`CauseDamage((A || B) ? 1 : 0)` 97.70; two named `const bool` temporaries for the `&&` 95.80;
`(A && B) ? true : false` 96.83; `&` instead of `&&` 95.98; `const bool contact` moved inside
the `if (mContactGen.get())` 96.39; `cache` declared inside `if (mBeamAttributes & 1)` 97.68;
`const float dt` on `UpdateFx` no change. Also do not re-try hoisting `PointCache()` past the
flag writes in `Fire` (the previous run's finding) - moving it back inside the `if` changes
nothing, and `const bool flag` is what actually fixed `Fire`.
