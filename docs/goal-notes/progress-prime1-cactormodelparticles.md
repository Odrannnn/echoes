# progress-prime1-cactormodelparticles

Trial of porting Prime 1's `CActorModelParticles` source into Echoes' unit, as the item asked.

## Result

`main/MetroidPrime/CActorModelParticles` **20 / 77 -> 34 / 77** matched functions.
Unit fuzzy 24.86% -> **33.75%**, matched code 16.76% -> **26.62%**.
`All:` 9440 -> **9454** matched functions (28465 total). The unit stays `NonMatching`; the
other 43 functions in it are untouched stubs.

**14 of the 16 functions the item listed reached 100%.** This is the item's real
measurement: Prime 1's source, adapted only to Echoes' member names and Echoes'
`CSkinnedModel`-based point-generator interface, reproduces Echoes' bytes for this class
essentially unchanged. Echoes' `CItem` really is Prime 1's plus the implosion fields.

## Per function (the item asked for this table)

| function | before | after | Prime 1 source |
|---|---|---|---|
| `AddRef__Q220CActorModelParticles7CSystemFv` | 78.57% | **100%** | needed a small edit: split `++mRefCount` out of the `if` condition |
| `DelRef__Q220CActorModelParticles7CSystemFv` | 81.43% | **100%** | same small edit |
| `Update__Q220CActorModelParticles7CSystemFv` | 79.26% | **96.11%** | needed a small edit: two separate early returns, and `loading = true; break;` instead of `return` |
| `UpdateRainSplash__Q220CActorModelParticles5CItemFfPC6CActorR13CStateManager` | 4.83% | **100%** | matched unchanged |
| `UpdateIce__Q220CActorModelParticles5CItemFfPC6CActorR13CStateManager` | 1.51% | **100%** | matched unchanged, except `it->get()` for Prime 1's `it->get()` on a vector iterator |
| `Update__20CActorModelParticlesFfR13CStateManager` | 2.13% | **100%** | matched unchanged |
| `StartAsh__20CActorModelParticlesFR6CActor` | 6.67% | **100%** | matched unchanged |
| `DoFirePop__20CActorModelParticlesFR6CActor` | 6.67% | **100%** | matched unchanged |
| `StartElectric__20CActorModelParticlesFR6CActor` | 2.94% | **100%** | matched unchanged |
| `StopElectric__20CActorModelParticlesFR6CActor` | 3.12% | **100%** | matched unchanged, after adding two `CActor` accessors (see below) |
| `LightDudeOnFire__20CActorModelParticlesFR6CActor` | 4.17% | **100%** | matched unchanged |
| `StopFire__20CActorModelParticlesFR6CActor` | 2.33% | **100%** | matched unchanged |
| `SetupHook__20CActorModelParticlesCF9TUniqueId` | 4.17% | **100%** | adapted: Echoes' `PointGenerator` takes `(model, workspace, context)`, so the call is `SetPointGeneratorFunc(&*it, PointGenerator)` rather than Prime 1's `reinterpret_cast<CItem*>(context)->GeneratePoints(...)` |
| `FindOrCreateSystem__20CActorModelParticlesFR6CActor` | 6.07% | **100%** | matched unchanged |
| `AddTypeRef__Q220CActorModelParticlesFQ220CActorModelParticles12ESystemTypes` | 51.96% | **100%** | needed a small edit: `const uchar mask` must be computed **before** `mDgrps[type].AddRef()`, not after |
| `GetAshyTexture__20CActorModelParticlesCFRC6CActor` | 85.00% | 85.00% | **did not help** - no change, see below |

**Unchanged at 85%:** `GetAshyTexture`. Rewriting it from Prime 1's single-condition `if`
made no difference, because the stub's nested `if` already produced the same shape. The
remaining 15% is a **call-offset artifact, not a source error**: `tools/bytescmp`-style
comparison of the 160 bytes shows 30 differing bytes, and every one of them is inside a
`bl`/`li` immediate or a `mr` that a different symbol layout moved
(`build/binutils/powerpc-eabi-nm` on our object vs `config/G2ME01/symbols.txt` on retail).
The control flow, register usage and every load/store offset are identical. Do not spend
another run on it.

**96.11% `CSystem::Update`:** 4 of 108 bytes differ, and they are one branch. Retail emits
`bnelr` for the `mLoaded` test; we emit `bne +8 / blr` (an extra 4-byte pair). I tried four
spellings of the two early returns (`== 0`, `!mRefCount`, `<= 0`, a blank line) and all four
produced byte-identical output. This looks like a mwcceppc tail-duplication decision that
the source spelling does not reach.

## Files changed

- `src/MetroidPrime/CActorModelParticles.cpp` - the 16 functions above.
- `include/MetroidPrime/CActor.hpp` - two accessors that did not exist:
  `bool GetPointGeneratorParticles() const` and `void SetPointGeneratorParticles(bool)`.
  The member already existed (`CActor.hpp:313`) and retail reads it directly at `0x152` bit 6
  from `FindOrCreateSystem`, `StopFire`, `StopElectric` and `CActorModelParticles::Update`;
  only the getter/setter were missing. **No layout change**: they are inline accessors.
- `src/Kyoto/Animation/CSkinnedModel.cpp` - **required by my change, see below.**

## The one thing that was not in the item and cost real time

`SetupHook` calling `CSkinnedModel::SetPointGeneratorFunc` **added an undefined symbol to the
port link** (254 -> 255) and `tools/gate.sh` failed on it:

```
link_check: STRICT FAIL - regression gate: 255 undefined against a baseline of 254 (GREW)
  NEW  CSkinnedModel::SetPointGeneratorFunc(void*, void (*)(CSkinnedModel const&, SSkinningWorkspace const&, void*))
gap grew: _ZN13CSkinnedModel21SetPointGeneratorFuncEPvPFvRKS_RK18SSkinningWorkspaceS0_E is not in port_link_gap_list.md
```

The setter was declared in the header and implemented only in `DolphinCSkinnedModel.cpp`,
which is **not in `files.cmake`** - so the port had no definition for it and only no caller
had ever exposed the hole. Retail's body is two stores, `.text:0x8030F054`, size 0xC
(`stw r4,-25032(r13); stw r3,-25028(r13); blr`), so I wrote it plus the
`sPointGenData` definition into `src/Kyoto/Animation/CSkinnedModel.cpp` - the port-side TU,
which `files.cmake:1051` does name. This is **not an unrelated fix smuggled in**: the
decompilation's own call is what created the undefined symbol, and the port's count may not
rise. It is retail behaviour transcribed from the disassembly, not a stub.

Generalisable rule for the next lane: **a `progress` item that fills in a call the port has
never had a caller for must define the callee, or the port link grows.** The gate catches it
even though the decompilation side is perfect.

## What I did not do, and why

The remaining 43 functions in the unit are the `Update*` effect bodies, `GeneratePoints`,
`Render`, `AddStragglersToRenderer` and the 19 unnamed `fn_8014*` gaps. `tools/unit_fit.sh`
reports **40 functions (4220 bytes) in our object that retail's unit object does not define**
- all `rstl` template/destructor COMDATs. That is the known harmless cause the tool itself
names, but it means the unit is not close to a flip and the item is right to be a `progress`
item, not a `match` one. I did not attempt a flip.

## Verification

```
./tools/decomp_build.sh        All: 29.11% fuzzy, 21.25% matched, 11.37% linked (9454 / 28465)
sha1sum build/G2ME01/main.dol  6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (unchanged)
python3 tools/check_symbol_names.py   checked 484 units; 0 declared names are missing
python3 tools/report_diff.py build/report.base.json build/report.json   no regression
MP_GATE_DOCS_WRITE=1 ./tools/gate.sh build/report.base.json   GATE PASS
python3 tools/check_decl_order.py --unit MetroidPrime/CActorModelParticles   ok
```

`report_diff.py` also reports a `SPLIT` in `IngSnatchingSwarm` and `1 units newly linked`
(`CIngSnatchingSwarmGenAccessors`). **That is not mine** - it is a pre-existing uncommitted
carve in this worktree (`configure.py`, `files.cmake`,
`config/G2ME01/rels/IngSnatchingSwarm/splits.txt`, plus an untracked
`src/MetroidPrime/ScriptObjects/CIngSnatchingSwarmGenAccessors.cpp`). I left it alone. It is
why the docs-claims check needed `MP_GATE_DOCS_WRITE=1`; the judge runs the gate that way.

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` were rewritten by that gate run and I
reverted both with `git checkout` - the driver discards edits to them.

No `asm` added. No `tools/`, `build/goal/` or `docs/research/` path touched. Not committed.

---

# Run 2 (2026-10-01, lane 5)

## STALE (partly) — the earlier run's work had landed

The clean tree already measured **35 / 77** matched (report.base.json == report.json at the start),
not the 20 or 34 the run-1 notes record. Run 1's commit `9a9fbf5e progress:
progress-prime1-cactormodelparticles` is upstream, so run 1's 14 functions were already at 100%
before this run started. **Re-measure first is the whole point; do not re-do what the notes
already reached.** This run therefore attacked the functions run 1 never touched: the ones whose
retail offsets the notes describe as "untouched stubs".

## Result

`main/MetroidPrime/CActorModelParticles` **35 -> 42 / 77** matched functions.
Unit fuzzy 33.88% -> **61.53%**, matched code 27.49% -> **38.99%**.
`All:` 11520 -> **11527** matched functions (28465 total). Linked count unchanged at 5590.
The unit stays `NonMatching`.

`tools/goal_check.sh build/goal/item.json` -> **PASS** (measured, not asserted).

## Per function (measured by `tools/decomp_build.sh main/MetroidPrime/CActorModelParticles`)

| function | before | after |
|---|---|---|
| `IsMediumOrLarge__FRC6CActor` | 6.09% | **100%** |
| `StopImplosion__20CActorModelParticlesFR6CActor` | 2.70% | **100%** |
| `StartImplosion__20CActorModelParticlesFR6CActorRC9CVector3fb` | 3.23% | **100%** |
| `StopRainSplashes__20CActorModelParticlesFR6CActor` | 4.17% | **100%** |
| `StartRainSplashes__20CActorModelParticlesFR6CActorR13CStateManageriif` | 1.30% | **100%** |
| `UpdateElectric__Q220CActorModelParticles5CItemFfPC6CActorR13CStateManager` | 0.83% | **100%** |
| `UpdateAshGen__Q220CActorModelParticles5CItemFfPC6CActorR13CStateManager` | 0.74% | **99.97%** |
| `Update__Q220CActorModelParticles5CItemFfR13CStateManager` | 0.82% | **100%** |
| `UpdateFirePop__Q220CActorModelParticles5CItemFfPC6CActor` | 0.78% | **89.82%** |
| `UpdateIcePop__Q220CActorModelParticles5CItemFfPC6CActor` | 0.78% | **89.82%** |
| `UpdateOnFire__Q220CActorModelParticles5CItemFfP6CActorR13CStateManager` | 0.48% | **86.13%** (kept; real code, see the wall below) |

Seven reached 100%; three are 86-100%. Nothing got worse.

## The spellings that reached 100% (so the next run does not repeat them)

**`IsMediumOrLarge`** is not Prime 1's `patterned->GetKnockBackCtrl().GetCreatureSize() !=
kCS_Small`. Retail's 0x8014FD14 is 0x5C bytes and reads:

```
bl 80097584 <TCastToPtr<CPatterned>(CEntity*)>
cmplwi r3,0 ; beq ->player-path
lwz  r3,856(r3)          # CPatterned + 0x358
neg r0,r3 ; or r0,r0,r3 ; srwi r3,r0,31   # !!x
b end
player-path: bl 8009a000 <TCastToPtr<CPlayer>(CEntity*)> ; !!r3
```

So Echoes' spelling is a plain `!= 0` on `CPatterned::mCreatureSize` (which this tree's header
already had at the right offset, as a *private* member with no accessor) plus a **CPlayer** cast as
the fallback, which Prime 1 does not have at all. The source that matched:

```cpp
if (const CPatterned* patterned = TCastToConstPtr< CPatterned >(&actor)) {
  return patterned->GetCreatureSize() != 0;
}
return TCastToConstPtr< CPlayer >(&actor) != nullptr;
```

`GetCreatureSize()` is a new inline accessor on `CPatterned` (no layout change). `TCastToPtr<CPlayer>`
was already a reachstub (`PortReachStubs.cpp` reachstub_100/101), so it costs no new link name.

**`StartImplosion`** (124 bytes) is three statements: `FindOrCreateSystem`, store the point at
offset 0xF8 (`mImplosionPoint`), `UseType(blackHole ? kST_BlackHole : kST_Imploder)`. Note the
`li r4,7` is emitted *before* the `blackHole` test, so the source order matters.

**`StopImplosion`** (148 bytes) is guarded by `actor.GetPointGeneratorParticles()` (bit 6 of byte
0x152, already in the header), then `FindSystem(GetUniqueId())`, then `SetParticleEmission(false)`
on `mImplosionGen` and **two stores at 0xEC/0xF0** - `mImplosionMaxParticles` and
`mImplosionQueuedParticles`, **not** the point iterator. Getting those two fields wrong costs the
match; the disassembly is unambiguous.

**`StopRainSplashes`** / **`StartRainSplashes`**: Prime 1's `RemoveRainSplashGenerator` /
`AddRainSplashGenerator` are Echoes' `StopRainSplashes` / `StartRainSplashes` under new names, and
the bodies are unchanged **except** Echoes' `StartRainSplashes` uses `FindOrCreateSystem` and tests
`it->mRainSplashGen.null() && actor.HasModelData()` - written as a positive `if`, not as two early
returns. Two early returns produce the wrong branch at +0x88 and 98.57%; the positive `if` is 100%.

**`UpdateElectric`** is Prime 1's function unchanged. 672/672 bytes, only `bl`/`lis` relocations
differ.

**`UpdateAshGen`** is Prime 1's plus the Echoes-only `mAshQueuedParticles` clamp, which appears
**twice**: once in the "gen exists" arm and once in the "just created the generator" arm. The clamp
is `rstl::min_val(16, mAshMaxParticles)` then `mAshMaxParticles -= mAshQueuedParticles`, and it is
guarded by `if (mAshMaxParticles > 0)`. 752/752 bytes; the residual 0.03% is two float constants
(`0.3f`/`1.f`) landing in a different pool slot.

`CElementGen::mMAXP` **already exists** in this tree's header (`include/Kyoto/Particles/CElementGen.hpp:204`);
only the `GetMaxParticles()` accessor was missing. Prime 1 declares it, Echoes' header had dropped it.

**`Update__CItem`** is Prime 1's `CItem::Update` plus Echoes' implosion, and the extra field is
`mAshQueuedParticles` (an extra `= 0` next to `mAshMaxParticles = 0`) **and** a
`mImplosionGen` block that calls `SetParticleEmission(false)` and then zeroes 0xE4 and 0xF0 inside
the same `if`. There is **no** `mImplosionPointIterator = -1` in that block - adding one is 4 bytes
over and drops the match. The effect order is OnFire, AshGen, Ice, FirePop, Electric, **Implosion**,
RainSplash, Burn, IcePop - Implosion sits after Electric, not last.

## The one thing that cost real time, and the general rule

`tools/bytesdiff.sh` / `tools/probe_cc.sh` **do not work on this unit.** They fail with
`the file 'musyx/musyx.h' cannot be opened` because they omit `-i extern/musyx/include`, which the
real build has. Use the unit's own `cflags` from `build.ninja` instead. The helper I wrote and used
is left at **`.tmp/opencode/probe.sh`** - it compiles
`src/MetroidPrime/CActorModelParticles.cpp` with the ninja rule's flags (including
`-i extern/musyx/include` and the three `-DMUSY_*` defines) and then calls `tools/bytescmp.py`. It is
untracked, so the driver will clean it; re-create it from this description if needed.

Its output format is misleading in one way worth remembering: `bytescmp.py` aligns ours and retail
instruction by instruction from the top, so **one missing or extra instruction desynchronises the
whole listing** and every later line looks wrong. Read `SIZE:` first. When ours and retail agree on
size and only `bl`/`b`/`lis` immediates differ, that is a **match** - relocations are not compared.

## Measured walls (spellings tried, none reached 100%)

**`UpdateFirePop` / `UpdateIcePop` (both 716/716 bytes, 89.82%).** The control flow, calls and every
member offset are right; 32 instructions differ *only in stack-slot numbers*. Retail's
`CModelData::GetBounds` result lives at `r1+72` and its `GetCenterPoint` output at `r1+36`; ours put
the `bounds` temp at `r1+48`. The frame size (128) and the total size (716) are identical, so this
is purely where mwcceppc chose to put one 24-byte temporary. Tried, all measured:

| spelling | ours / retail | result |
|---|---|---|
| `const CAABox bounds = ...; gen->SetGlobalTranslation(bounds.GetCenterPoint());` | 716 / 716 | **89.82%** - best; slot at +48 |
| same but `bounds` written before `gen` | 716 / 716 | identical to above |
| ternary `cond ? A.GetBounds(xf).GetCenterPoint() : B.GetCenterPoint()` | 640 / 716 | worse |
| `const CVector3f center = ...GetCenterPoint(); gen->SetGlobalTranslation(center);` | 692 / 716 | worse |
| hoisted `CVector3f center;` filled in both arms | 740 / 716 | worse |
| `const CVector3f center` per arm (two of them) | 764 / 716 | worse |
| `const CAABox& bounds = ...;` (a reference) | 660 / 716 | much worse |
| `CAABox bounds(CAABox::MakeMaxInvertedBox());` assigned in the arms | 768 / 716 | worse |
| `const CAABox bounds = cond ? A : B;` hoisted before `gen` | 712 / 716 | close; one insn short |

The winning shape also needs `GetOtherBounds()`, **not** `GetRenderBoundsCached()`: retail reads
`actor+0xCC`, and this tree's `mOtherBounds` is at 0xCC while `mRenderBounds` is at 0xE4. Using the
wrong accessor is a 100-point error on its own.

**`UpdateOnFire` (1160 bytes, 86.13%, NOT committed as a guess).** Prime 1's body compiles to
1056 bytes against retail's 1160. The missing ~104 bytes are Echoes' **multiplayer sfx selection**:
retail calls `fn_80036F10__13CStateManagerCFv` (a `CStateManager` predicate) and then branches on
`TCastToPtr<CPlayer>(actor) != nullptr` and a field at `CPlayer+0x38C` before choosing between
sfx ids built as `addi r0,r4,7522` / `addi r0,r4,9866` (i.e. `IsMediumOrLarge ? base : base+1`).
The same three-way selection appears in `StartBurnDeath`. Working out which `CStateManager` method
`fn_80036F10` is, and what `CPlayer+0x38C` is, needs a disassembly pass I did not finish. **Do not
assume Prime 1's `SFXeff_x_fire_lp_00/01` and `SFXeff_x_ash_00/01` are the ids** - the retail
immediates are 7522/7523 in `StartBurnDeath` and 7394/7395 plus 9602/9603 in `UpdateOnFire`.

WALL: UpdateFirePop 89.82% - mwcceppc allocates the `bounds` temp at r1+48 where retail uses r1+72; nine source shapes tried, all worse or equal, frame and total size already identical.
WALL: UpdateIcePop 89.82% - same function body as UpdateFirePop, same stack-slot-only difference; every spelling tried for one was tried for the other.
WALL: UpdateOnFire 86.13% - Echoes' multiplayer sfx selection (fn_80036F10 predicate + CPlayer+0x38C) is unidentified; Prime 1's two-id spelling is 104 bytes short.

## Files changed

- `src/MetroidPrime/CActorModelParticles.cpp` - `IsMediumOrLarge`, `CItem::Update`,
  `UpdateElectric`, `UpdateFirePop`, `UpdateIcePop`, `UpdateAshGen`, `UpdateOnFire`,
  `StartImplosion`, `StopImplosion`, `StartRainSplashes`, `StopRainSplashes`.
- `include/MetroidPrime/CActor.hpp` - `CVector3f GetModelScale() const` (Prime 1 has it, this
  header did not; inline, no layout change).
- `include/MetroidPrime/Enemies/CPatterned.hpp` - `int GetCreatureSize() const` (inline; the
  member already existed at 0x358).
- `include/Kyoto/Particles/CElementGen.hpp` - `int GetMaxParticles() const` returning `mMAXP`
  (inline; `mMAXP` already existed at line 204, only the accessor was missing).
- `src/MetroidPrime/PortGlobals.cpp` - **required by this change, see below**:
  `TCastToPtr<CPatterned>(CEntity*)`. `IsMediumOrLarge` calls it, and it had no port definition, so
  `tools/probe_sources.sh` reported the link growing 250 -> 251 undefined. Retail's body is at
  0x80097584 and is **not** the type-id wrapper the five `PORT_CAST_TO_PTR` entries above are: it is
  a null test plus `rlwinm. r0,r0,30,29,29` on the byte at 0x20, which is cast-flag bit 2, the same
  test `src/MetroidPrime/TypesMatch.cpp` spells `(entity->GetCastFlags() & 4) != 0`. That file is
  unlisted in `files.cmake` for the layout reason its own comment records, so the definition goes
  here. Transcribed from the disassembly, not a stub, and not an unrelated fix: the decompilation's
  own call created the hole.

Generalisable rule, same as run 1's: **a `progress` item that fills in a call the port has never
had a caller for must define the callee, or the port link grows.** Two different holes this run
(`TCastToPtr<CPatterned>`) and last run (`CSkinnedModel::SetPointGeneratorFunc`).

## Not attempted, and why

- `UpdateImplosion` (1224 bytes, 0.46%) - Echoes-only, no Prime 1 source. The disassembly is 0x4CC
  bytes of `CTransform4f::GetInverse` / `Rotate` / `__ml__` / `CUnitVector3f` /
  `SetGlobalOrientAndTrans` / three `SetExternalParam` calls and a `TCastToPtr<CPatterned>`; a
  faithful port needs the plane construction worked out member by member. Cheapest single remaining
  win in the unit, but a whole item.
- `GeneratePoints` (1676 bytes, 0.24%), `AddStragglersToRenderer` (420), `Render` (548),
  `PointGenerator` (60), `GetNextBestPt` (280) - all Echoes-only signatures (skinned-model
  workspace instead of Prime 1's vertex arrays; no thermal-visor branches).
- `__ct__CItem` (508 bytes, 42.04%), `__ct__CSystem` (296, 75.53%), `__dt__CItem` (568, 97.89%),
  `CSystem::Update` (108, 96.11%) - run 1's notes call `CSystem::Update` a wall after four spellings
  and this run did not re-measure it. `__dt__CItem` at 97.89% is 12 bytes from a match and is the
  cheapest thing left in the unit; **`NEW:` candidate for the next run.**

## Verification (all measured on this tree)

```
./tools/decomp_build.sh main/MetroidPrime/CActorModelParticles
  All:  33.08% fuzzy, 25.92% matched, 12.18% linked (11527 / 28465 functions)
  main/MetroidPrime/CActorModelParticles: 61.53% fuzzy, 38.99% matched (42 / 77 functions)
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (unchanged)
./tools/probe_sources.sh   probe: 753 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py   checked 515 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit MetroidPrime/CActorModelParticles   ok
./tools/goal_check.sh build/goal/item.json   PASS
```

The judge also reported `linked 5590 -> 5590` and `no asm added`.

`docs/HANDOFF.md` was rewritten by `goal_check.sh`'s gate run and reverted with `git checkout`; the
driver discards edits to it. No `tools/`, `build/goal/` or `docs/research/` path touched, no
`configure.py`/`config/`/`files.cmake` change, no `.asm`, not committed.
