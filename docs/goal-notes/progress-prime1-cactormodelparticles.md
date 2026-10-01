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

---

# Run 3 (2026-10-01, lane 6)

## Result

`main/MetroidPrime/CActorModelParticles` **44 -> 45 / 77** matched functions (re-measured on
this tree; run 2's 42 was stale, and the clean tree here already had run 1+2's 44 landed).
Unit fuzzy 63.20% -> **64.56%**, matched code 42.39% -> **42.97%**.
`All:` 11956 -> **11957** matched functions (28465 total). Linked count unchanged at 5728.
The unit stays `NonMatching`; no flip attempted.

`./tools/goal_check.sh build/goal/item.json` -> **PASS** (measured, not asserted).

| function | before | after | what did it |
|---|---|---|---|
| `Update__Q220CActorModelParticles7CSystemFv` | 96.11% | **100%** | two early returns -> one `\|\|` (see below) |
| `StartBurnDeath__20CActorModelParticlesFR6CActorR13CStateManager` | 1.61% | **89.90%** | written from the disassembly; 248/248 bytes |
| `GetNextBestPt__FiRC13CSkinnedModelRC18SSkinningWorkspaceiR9CRandom16` | 87.14% | **97.14%** | `delta` as a reference + `MagSquared` inlined |

One function reached 100% - that is the item's result. The other two are large real gains kept
for the next run. **Nothing regressed** (`tools/report_diff.py`: `no regression`).

## THE FINDING THAT UNBLOCKED STARTBURNDEDEATH (run 2 called this function unidentified)

Run 2 recorded "`fn_80036F10` predicate + `CPlayer+0x38C` is unidentified". Both are now named, and
**no new header was needed** - this tree already declares them:

- `fn_80036F10__13CStateManagerCFv` is `CStateManager::fn_80036F10()`, already declared at
  `include/MetroidPrime/CStateManager.hpp:245` with the comment *Maybe_CheckIsMultiplayer*. It
  returns `gpGameState->GetGameMode()->GetType()` != two of the mode ids. It is what
  `CRumbleManager.cpp:18` and `CKnockBackMgr.cpp:79` already call. **The name was there; run 2
  did not grep for it.**
- `CPlayer+0x38C` is `EPlayerMorphBallState mMorphBallState`
  (`include/MetroidPrime/Player/CPlayer.hpp:548`), already readable through
  `GetMorphballTransitionState()` (`:212`).

So `StartBurnDeath`'s whole sfx selection is:

```cpp
ushort sfx = static_cast<ushort>(IsMediumOrLarge(actor) ? 7521 : 7522);
if (mgr.fn_80036F10()) {
  if (CPlayer* player = TCastToPtr<CPlayer>(&actor)) {
    sfx = static_cast<ushort>(
        player->GetMorphballTransitionState() == CPlayer::kMS_Unmorphed ? 9602 : 9601);
  } else {
    sfx = static_cast<ushort>(IsMediumOrLarge(actor) ? 9601 : 9602);
  }
}
CSfxManager::AddEmitter(sfx, actor.GetTranslation(), actor.GetCurrentAreaId().Value(), true, false,
                        CSfxManager::kMedPriority);
it->mAshy.Lock();
```

**The ids are 7521/7522 and 9601/9602, not the 7522/7523 that run 2's notes assumed.** See the
sign-mask rule below - the ternary is *inverted* from what the immediates look like.

## The MWCC 2.7 codegen rule this item turned up (generalisable, costs nothing to reuse)

Retail writes `clrlwi rX,r3,24; neg r0,rX; or r0,r0,rX; srawi rX,r0,31; addi r0,rX,K`.
MWCC 2.7 will only produce that **`srawi`** - rather than `srwi` + `subfic`, which is the same value
- from a ternary **narrowed to `ushort` at its point of definition**:

```cpp
int    s = cond ? A : B;  ushort u = static_cast<ushort>(s);  // srawi + addi  <- retail
ushort u = cond ? A : B;                                      // srwi + subfic
ushort u = 7522 - (cond ? 1 : 0);                             // srwi + subfic
ushort u = 7522 + (cond ? 1 : 0);                             // srwi + subfic
ushort u = static_cast<ushort>(cond ? A : B);                 // srwi + subfic   (!!)
```

So the narrowing has to be a **`static_cast<ushort>` wrapping the whole ternary**, and the ternary
has to be a **signed `int` expression** (an explicit `int` temporary alone is not enough - it gets
re-narrowed per-arm and you get `mr r29,r0` where retail has `clrlwi r29,r0,16`).

**And `srawi rX,r0,31` yields -1/0, not 0/1.** `neg/or` sets every bit when the input is
non-zero, so the mask is all-ones, so `K + mask` is `K-1` on the true branch. That is why the ids
are `7521` on the true branch: the *addi immediate* is the **false** value. Reading
`addi r0,r4,7522` as "7522 when medium" inverts the whole ternary and costs hours.

Measured in isolation at `.tmp/opencode/vt2.py` (re-create from this paragraph if gone; untracked).
Its `tern_neg1` / `signed_tern` rows are the shapes that emit `srawi`.

## CSystem::Update: the wall run 1 recorded, broken by *un-nesting* the guard

Run 1 tried four spellings of the two early returns and all four were byte-identical, then wrote
it off as an unreachable tail-duplication decision. **It was reachable: the two guards must be one
`if`.** Two separate `if (...) { return; }` blocks make MWCC emit `beqlr` (a single conditional
return); retail emits `bnelr / ... / bne` - i.e. it tested `mLoaded` to *skip* the whole body and
`mRefCount` to *fall into* the loop. One `||` reproduces both:

```cpp
if (mLoaded || mRefCount == 0) { return; }        // 100%
if (!mLoaded && mRefCount != 0) { ...body... }    // 96.11% - same value, wrong branch shape
```

`mRefCount <= 0` gives 99.81% (the compare becomes `ble`, retail has `bne`).
**Generalisable lesson: when a function is stuck on `bnelr` vs `beqlr`, the fix is almost never in
the spelling of the return - it is in whether the two guards are one expression.**

## GetNextBestPt: `MagSquared()` must be inlined, and `delta` must be a reference

87.14% -> 97.14%, and the **loop body is now instruction-for-instruction identical to retail**.
Two edits, both about the same thing - retail *spills* `delta` to the stack (`stfs f2,12(r1)` /
`stfs f3,8(r1)` / `stfs f4,16(r1)`, frame 144) where we kept it in registers (frame 128):

```cpp
const CVector3f& delta = startVec - point;                        // reference -> forces the spill
const float distance = delta.GetX()*delta.GetX() + delta.GetY()*delta.GetY() + delta.GetZ()*delta.GetZ();
```

`delta.MagSquared()` (the inline at `include/Kyoto/Math/CVector3f.hpp:59`) compiles to three
`fmadds`, which is the same arithmetic but never touches memory. Spelling the three products out
gives retail's three `fmuls` + two `fadds` and the three stores.

The last 2.86% is `lfs f28,-24920(r2)` (retail) vs `lfs f28,0(0)` (ours) for `maxDistance = 0.f`:
retail keeps 0.0f in `.sdata2` at **0x8041E4E8** and loads it, we fold the literal. Same value, and
`.sdata2:0x8041E4E8` is unnamed in `config/G2ME01/symbols.txt` (`lbl_8041E4E8`, 4-byte float, 0.0f),
so there is nothing to declare - it is a pool-slot difference, like run 2's `0.3f`/`1.f` note on
`UpdateAshGen`.

## What I measured and rejected (so the next run does not repeat it)

- **`IsMediumOrLarge` returning `int`** does NOT fix `StartBurnDeath`. It stays at 100% itself, but
  the caller then loses retail's `clrlwi r4,r3,24` and the function drops to 240 bytes. **It must
  keep returning `bool`.**
- **`StartBurnDeath` morph-ball arm, all measured:** `!= 0 ? 9601 : 9602` 85.89%;
  `!= kMS_Unmorphed ? 9601 : 9602` 85.89%; `== 0 ? 9602 : 9601` 85.89%;
  `== kMS_Unmorphed ? 9602 : 9601` **89.90%** (best, what is committed);
  `> kMS_Unmorphed ? 9601 : 9602` 85.89%; `static_cast<int>(...) != 0` 85.89%;
  an `int state` temporary 85.89%; `9602 - (int)(... != 0)` 86.37%.
  The residual is the morph test itself: retail does
  `lwz r0,908(r3); cntlzw r0,r0; rlwinm r0,r0,27,31,31; neg r3,r0` - the **`cntlzw`/`rlwinm`**
  idiom, which MWCC only emits for a test on a value whose range it cannot prove. Every spelling
  here gives `neg/or/srawi` instead, because `EPlayerMorphBallState` is a 4-value enum and MWCC
  *does* know its range. **Getting there needs a member read as a plain `int` whose range is
  unknown - i.e. probably a different accessor, or `CPlayer+0x38C` is not the field retail reads.**
  This is the one part of `StartBurnDeath` still open.
- **`StartBurnDeath` call-argument order** (retail sets `r3,r9,r4,r5,r7,r8`; we set
  `r9,r4,r6,r3,r5,r7,r8`). Hoisting `area`/`pos` into locals, and replacing `kMedPriority` with
  the literal `127`, both leave the score at 89.90%. Same instructions, different schedule - not
  reachable from the source I tried.
- **`UpdateAshGen` (99.97%, unchanged this run):** the residual is still the `0.3f`/`1.f` pool slot.
  Tried, all measured 99.97%: writing the ternary as `1.0f : 0.30000001192092896f`; hoisting
  `const float scale`. Run 2's note stands - do not spend a run on it.
- **`__ct__CItem` (508 bytes, 42.04%) is a dead end for a source edit.** Retail emits an
  **out-of-line call** to `fn_8014F9CC` to fill the 8-element `mOnFireGens` vector; we inline the
  fill loop. `fn_8014F9CC` (72 bytes) is that vector-fill helper and is itself one of the unit's 0%
  `fn_` gaps. This is a codegen decision (inline vs out-of-line COMDAT), not a spelling, and it is
  the same class of problem as `unit_fit.sh`'s "40 functions retail does not define". **Do not
  attempt it as a `progress` item.**
- **`__ct__CSystem` (296 bytes, 75.53%) is now the cheapest untouched function** - Prime 1's
  source is the same body. Not attempted this run (out of budget).

## The tooling trap that cost the first hour of this run - read this first

**`tools/bytesdiff.sh` and `tools/probe_cc.sh` do not work on this unit** (run 2 already recorded
this: they omit `-i extern/musyx/include` and die on `musyx/musyx.h`). Worse, and new:
**compiling the unit by hand and then running `objdiff-cli report generate` measures nothing.**
objdiff reads the object ninja wrote at
`build/G2ME01/src/MetroidPrime/CActorModelParticles.o`; a hand compile to any other path leaves the
report stale and the unit's percentages do not move no matter what you changed. I lost several
iterations to this. The harness that works is `.tmp/opencode/probe.sh` (and `try.py` for a
one-line score): **`ninja build/G2ME01/src/MetroidPrime/CActorModelParticles.o` first** (0.8s), copy
that object aside, then generate the report. Ninja rebuilds this single unit in under a second, so
there is no reason to hand-compile at all.

Also: `tools/bytescmp.py` aligns ours against retail **instruction by instruction from the top**, so
one extra or missing instruction desynchronises the whole listing and every later line looks wrong
(read the `SIZE:` line first, and a run of consecutive differing `bl`s is almost always just the
desync). `.tmp/opencode/bytediff.py` (untracked) compares raw bytes with our object's relocations
masked out, which is what actually localises a difference; note it still needs the `bl`/`lis`
immediates masked, so a handful of reported runs are relocation noise.

## Files changed

`src/MetroidPrime/CActorModelParticles.cpp` only - three functions, no header change, no new
symbol, no new undefined reference, no layout change. Lines: `CSystem::Update` guard at :82,
`GetNextBestPt` at :617, `StartBurnDeath` at :730.

## Verification (all measured on this tree)

```
./tools/decomp_build.sh main/MetroidPrime/CActorModelParticles
  All:  33.78% fuzzy, 26.98% matched, 12.64% linked (11957 / 28465 functions)
  main/MetroidPrime/CActorModelParticles: 64.56% fuzzy, 42.97% matched (45 / 77 functions)
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (unchanged)
./tools/probe_sources.sh   probe: 747 files, 0 failed, 0 errors; link: LINKED (323 undefined, 0 duplicates)
python3 tools/check_symbol_names.py   checked 516 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit MetroidPrime/CActorModelParticles   ok
python3 tools/report_diff.py build/report.base.json build/report.json
  matched 11956 -> 11957   linked 5728 -> 5728   (+1 functions at 100%, 0 units newly linked)
  no regression
./tools/goal_check.sh build/goal/item.json   PASS
```

`docs/HANDOFF.md` was rewritten by `goal_check.sh`'s gate run and reverted with `git checkout`; the
driver discards edits to it. No `tools/`, `build/goal/` or `docs/research/` path touched, no
`configure.py`/`config/`/`files.cmake` change, no `.asm`, not committed. `.tmp/opencode/` helpers
are untracked and will be cleaned.

NEW: progress-prime1-cactormodelparticles-ctors | progress | MetroidPrime/CActorModelParticles | __ct__CSystem (296B, 75.53%) is the cheapest untouched function in the unit and Prime 1's body is unchanged; __ct__CItem (508B, 42.04%) is NOT worth it, retail calls an out-of-line vector-fill helper there.

## Lane 6: passed, then failed on the moved tip (2026-10-01 19:05:15Z)

The judged change failed goal_check.sh (exit 1) once rebased onto 38068b1312aa; re-do it against the current tip.

---

# Run 4 (2026-10-01, lane 6)

## FIRST: the previous failure was NOT the decompilation - it was a stale gap-list entry

Before touching anything I ran `tools/goal_check.sh` on a tree with no change of mine and it
failed with:

```
  FAIL  gate.sh
        stale: _ZN15CGMSinglePlayerC1Ev is listed but no longer missing - delete the entry
        GATE FAIL: link-gap
```

Commit **38068b13** ("port: real CGMSinglePlayer") defined `CGMSinglePlayer::CGMSinglePlayer()`
but did not delete its entry from `docs/research/port_link_gap_list.md`. `tools/link_gap.py`
fails a *resolved* symbol until its entry is deleted, so **every item on this tip failed the
gate**, whatever it did. That is why the run-3 change was judged a failure. Two files, both
allowed for an agent (`tools/` and `port_link_baseline.txt` are the forbidden ones):

- `docs/research/port_link_gap_list.md` - deleted the `_ZN15CGMSinglePlayerC1Ev` line.
- `docs/research/port_link_gap.md` - `| other game methods | 244 |` -> `243 |`. This was not
  optional: with the entry gone, `tools/check_docs_claims.py` failed the *next* gate step with
  "the gap table says other game methods is 244, the generated list has 243" and "the gap
  table's rows sum to 318, the generated list holds 317". The table is `244+12+58+4`; it is now
  `243+12+58+4` = 317. I did not touch the dated prose figures elsewhere in that file (AGENTS.md
  says annotate stale checkpoints, do not rewrite them).

**Generalisable: a `port` item that defines a symbol must delete that symbol's line from
`docs/research/port_link_gap_list.md` in the same commit, or it breaks the gate for every
concurrent item on the branch.**

## Result

`main/MetroidPrime/CActorModelParticles` **44 -> 46 / 77** matched functions. Re-measured: the
clean tree at 38068b1312aa was **44**, not run 3's 45 - run 3's commit never landed.
Unit fuzzy 63.20% -> **65.51%**, matched code 42.39% -> **44.57%**, data 100.00% (unchanged).
`All:` 11959 -> **11961** matched (28465 total). Linked 5728 -> 5728.

`./tools/goal_check.sh build/goal/item.json` -> **PASS** (measured, exit 0).

| function | before | after | what did it |
|---|---|---|---|
| `Update__Q220CActorModelParticles7CSystemFv` | 96.11% | **100%** | two early returns -> one `\|\|`, re-measured from run 3 |
| `GetNextBestPt__FiRC13CSkinnedModel...` | 87.14% | **97.14%** | `delta` as a reference + `MagSquared` spelled out, re-measured from run 3 |
| `StartBurnDeath__20CActorModelParticlesFR6CActorR13CStateManager` | 1.61% | **89.90%** | `FindOrCreateSystem`, not `FindSystem` (see below) |
| `__ct__Q220CActorModelParticles7CSystemFPCc` | 75.53% | **100%** | `push_back_unsafe` (see below) |
| `UpdateOnFire__Q220CActorModelParticles5CItemFfP6CActorR13CStateManager` | 86.13% | **95.05%** | Echoes' player-count sfx selection, positive `if` |

Two functions reached 100%, three more moved a long way. Nothing regressed.

## `push_back_unsafe` - the cheapest win in the unit, and run 3 called this ctor "unchanged"

Run 3 filed a `NEW:` saying `__ct__CSystem` (75.53%) "is the cheapest untouched function and
Prime 1's body is unchanged". Prime 1's body *was* already in the tree, and it was 24.48% off,
so "unchanged" was the wrong inference: the body is right and the **API call** is wrong.
`include/rstl/vector.hpp:86` has

```cpp
void push_back_unsafe(const T& in) { rstl::construct(mItems + mCount++, in); }
```

and that is byte-for-byte what retail's inlined `push_back` does, because `mItems + mCount++`
stores the new count **before** the construct call, and there is no capacity check:

```
retail 0x8014FC9C: lwz r3,4(r29); lwz r5,12(r29); slwi r0,r3,3; addi r4,r3,1
                  add. r3,r5,r0; stw r4,4(r29); beq; addi r4,r1,8; bl __ct__6CToken
ours  (push_back): + 10 instructions of capacity check, and ++mCount after the ctor
```

**Generalisable: before concluding that a decompiled body "matches Prime 1 unchanged" and is
therefore a codegen wall, check whether the repo has a second, differently-inlined member for
the same operation. `push_back` / `push_back_unsafe`, `at` / `operator[]`, `size` / `capacity`
are the pairs that keep turning up.**

## `StartBurnDeath` calls `FindOrCreateSystem`, and Prime 1's source is right about that

Run 3's spelling had `if (it == mItems.end()) return;` guarding a `FindSystem` call, which is
3 instructions and one call too many. Prime 1's `StartBurnDeath` is `AUTO(it,
FindOrCreateSystem(actor));` with **no** end check, and that is what retail does - which is why
retail has no `cmplw`/`beq` there at all. Do **not** re-derive this the way I did first: the
`bl` at 0x8014B704 is `48 00 08 f1`, and for `bl` the LI field is **already the byte
displacement** (bits 6..29) - do not shift it left by 2 again, or every `bl` in the function
resolves to a mid-function address and you conclude the callee is nonsense. `tools/dis.sh
<addr> <size>` against `build/G2ME01/main.elf` prints resolved symbol names and is the tool to
reach for; `tools/bytescmp.py` prints raw immediates and will not.

With that fixed the function is 244 bytes against retail's 248 and **one instruction** short.

## `UpdateOnFire`'s sfx selection, in full (run 2 and run 3 both had it wrong)

Run 2 measured the immediates (7394/7395, 9602/9603) and could not place the arms; run 3
measured the shape for `StartBurnDeath` only. The whole block, from
`tools/dis.sh 0x8014de6c 0x94`:

```cpp
ushort sfx = static_cast<ushort>(IsMediumOrLarge(*actor) ? 7393 : 7394);
if (mgr.fn_80036F10()) {                                   // TRUE arm falls through
  sfx = static_cast<ushort>(IsMediumOrLarge(*actor) ? 9865 : 9866);
} else if (TCastToPtr<CPlayer>(actor) != nullptr) {
  sfx = static_cast<ushort>(155);
}
mSfx = CSfxManager::AddEmitter(sfx, actor->GetTranslation(), actor->GetCurrentAreaId().Value(),
                               true, true, CSfxManager::kMedPriority);
```

Three things, each worth its own line:

1. **The pairs are inverted by the mask.** `srawi` yields 0/-1, so the `addi` immediate is the
   **false** arm: 7394/7395 means `? 7393 : 7394`, and 9866 means `? 9865 : 9866`. Run 2 read
   them the other way round.
2. **The single-player player value is 155**, not one of the 96xx pair. 155 is only reachable
   in the *non*-multiplayer arm, and only when the cast to `CPlayer` succeeds.
3. **Write the `if` positive.** `if (!c) { A } else { B }` and `if (c) { B } else { A }` are the
   same C++, but MWCC lays the **then** block out inline, so the negative form emits `bne` over
   the wrong block. Measured: 90.88% negative, **95.05%** positive. The same rule as run 3's
   `CSystem::Update` finding, in a different place - *the branch MWCC falls through into is the
   source's `then` block, so pick the polarity that matches retail's layout, not the one that
   reads better.*

Also: the trailing `AddEmitter` arguments are `true, true` (useAcoustics, looped). The old
`1, false, true` spelled the 7-parameter `uchar volume` overload; retail's mangled name is
`AddEmitter__11CSfxManagerFUsRC9CVector3fibbs`, the 6-parameter one, and
`R_PPC_REL24 AddEmitter__11CSfxManagerFUsRC9CVector3fibbs` is what ours now emits.

## What I measured and rejected, so the next run does not repeat it

- **`StartBurnDeath` is one instruction from 100% and no spelling reaches it.** Retail
  `0x8014B6F8`: `lwz r0,908(r3); cntlzw r0,r0; rlwinm r0,r0,27,31,31; neg r3,r0; addi r0,r3,9602`.
  Ours: `cntlzw r0,r0; srwi r3,r0,5; addi r0,r3,9601`. Same value for every value the enum can
  hold (both give 9602 at 0 and 9601 otherwise); retail's `rlwinm` takes bit 4 of `cntlzw`, i.e.
  it is the "value fits in 16 bits" mask, so **retail's compiler did not know the field's
  range**. The range comes from the *declared type* of `CPlayer::mMorphBallState`
  (`include/MetroidPrime/Player/CPlayer.hpp:548`, `EPlayerMorphBallState`, 4 enumerators), and
  MWCC propagates it through every cast. Measured this run, all 89.90%: `== kMS_Unmorphed`,
  `static_cast<int>(...) == 0`, `static_cast<unsigned>(...) == 0u`,
  `static_cast<int>(...) == static_cast<int>(kMS_Unmorphed)`, `!...`. Worse: 88.37% for
  `static_cast<ushort>(...) == 0` and for `static_cast<unsigned char>(...) == 0u`.
  The only lever left is declaring that member as a plain `int` in the shared `CPlayer.hpp`,
  which changes codegen for every other reader of the field in the whole tree. I did not do it:
  the blast radius is unbounded from this item, and the gate's "no function anywhere gets worse"
  is the wrong place to discover it.
- **Float literals: MWCC 2.7 folds them here and retail loads them from `.sdata2`.** This one
  root cause is all that is left in **three** functions. `.tmp/opencode/sbs2.py` (below) shows
  identical instruction counts with a single differing `lfs`/`lfd` each time:
  `GetNextBestPt` 280B, one `lfs f28,0(0)` vs `lfs f28,-24920(r2)` (0.0f, 97.14%);
  `UpdateAshGen` 752B, three of them - 0.0f, 0.3f and a **double** (99.97%);
  `UpdateOnFire` 1160B, two of them (95.05%). Our `.sdata2` already holds 0.0f, 0.3f and 1.0f at
  the right relative offsets (`data 100.00%` both before and after), as anonymous `@NNNN`
  compiler constants - so the pool slot exists and the relocation would resolve. MWCC just
  chooses the folded form at these sites. Tried this run for the 0.0f: `static const float` at
  file scope (97.14%, folded, and **nothing added to `.sdata2`**), and the same constant with
  external linkage (97.14%, also not emitted). Run 3 tried the ternary reordering and a hoisted
  `const float scale` on `UpdateAshGen` (99.97% each). The source construct that forces a pool
  load is most likely a **named** constant with external linkage referenced from a shared
  header, which I could not identify. Not worth more runs without a lead.
- **`__ct__CItem` (508B, 42.04%) is confirmed a dead end**, independently of run 3. Retail
  calls an out-of-line `fn_8014F9CC` (72B, 0x8014F9CC: `mtctr`+`bdnz` loop) to fill the 8
  `mOnFireGens` slots; MWCC **inlines and fully unrolls** ours into 8 copies. The count is the
  literal 8, so there is nothing opaque to stop the unroll, and forcing the helper out of line
  means changing `rstl::construct`/`uninitialized_fill_n` for the whole tree.
- **`UpdateFirePop`/`UpdateIcePop` (716B, 89.82%): run 2's wall stands, and this run pinned
  down exactly which slots differ** so nobody has to re-derive it. Both are **179 instructions
  against retail's 179**, same 128-byte frame, same total size. The only difference is where
  three 12-byte `CVector3f` temporaries and one 24-byte `CAABox` sret slot land: retail
  `sret@72, vec@24/36/48, args@60/48`; ours `sret@48, copy 48->72, args@36/24`. Retail copies
  each `GetCenterPoint()` result into a separate argument slot; we pass the sret slot straight
  through, and we copy the `CAABox` where retail does not. It is an allocation-*order* artifact
  of the same three locals, and run 2's nine spellings moved the total size every time. The
  `GetOtherBounds()` (not `GetRenderBoundsCached()`) requirement still stands.
- **`UpdateOnFire`'s `SetGlobalOrientAndTrans` is a different method, not a different
  spelling.** Retail builds a `CVector3f` on the stack from three floats at `actor+0x30`,
  `+0x40`, `+0x50` and calls a **virtual** entry (vtable+0x20) with `r4 = &temp`; ours calls the
  non-virtual `SetGlobalOrientAndTrans(const CTransform4f&)` with `actor+0x24`. That is also
  where retail's frame is 112 and ours 96, and where retail's extra `psq_st f31,104(r1)` comes
  from. Those three floats are 0x10 apart, so they are not one contiguous `CVector3f` member.
  Working out what they are needs a `CActor` layout pass I did not do; it is the whole
  remaining 4.95%.
- Rejected on score alone: an explicit redundant `sfx = static_cast<ushort>(sfx);` before the
  `AddEmitter` call reaches 95.26% instead of 95.05% (it recovers retail's second
  `clrlwi r4,r26,16`). Not kept: 0.21% is not worth a line that reads as a hack. Also measured
  and worse: moving the first ternary into the `else` arm as a three-arm if/else (90.02%), and
  an `int`-typed `sfx` with the narrowing at the call (94.48%).

## Tooling (this is the useful half of the run)

`tools/bytescmp.py` and `tools/probe_cc.sh` still do not work on this unit (runs 2 and 3: they
omit `-i extern/musyx/include`). But **you do not need to hand-compile at all**: ninja rebuilds
this unit in 0.8s, and `.tmp/opencode/probe.sh` (untracked, from run 3) does
`ninja ...CActorModelParticles.o` then regenerates the report and prints the unit's numbers plus
every sub-100% function. `.tmp/opencode/try.py` does the same for a named subset.

What I added, and what it is for - **`.tmp/opencode/sbs2.py`**:

```
python3 .tmp/opencode/sbs2.py .tmp/opencode/amp.o <symbol> <retail_vaddr> <size>
```

It disassembles our object symbol and the retail range out of `build/G2ME01/main.elf`, strips
relocated immediates and branch displacements, and prints a `difflib` alignment of the two
instruction streams, so the output is **localised** instead of desynchronised. This is the tool
that made every finding above cheap. It is untracked, so re-create it from this paragraph.
`tools/dis.sh <addr> <size>` is the companion for reading one retail range with symbol names.

A second trap: objdiff lists a unit's functions by symbol, and **`build/report.json` in the
tree is whatever the last build left there** - on arrival it showed 45/77 and run 3's numbers
for a tree that actually builds 44. Re-measure by rebuilding the unit before reading anything
into the report.

## Files changed

- `src/MetroidPrime/CActorModelParticles.cpp` - five functions, no header change, no new
  symbol, no new undefined reference, no layout change. Lines: `push_back_unsafe` at :39,
  `CSystem::Update` guard at :80, `UpdateOnFire` sfx at :406, `GetNextBestPt` at :619,
  `StartBurnDeath` at :735.
- `docs/research/port_link_gap_list.md`, `docs/research/port_link_gap.md` - the gate fix
  described at the top. Not part of the decompilation; disclosed because the diff carries them.

## Verification (all measured on this tree)

```
./tools/goal_check.sh build/goal/item.json   PASS (exit 0)
  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)   ok
  counts: matched 11959 -> 11961   linked 5728 -> 5728
  target rose: main/MetroidPrime/CActorModelParticles: 44 -> 46 / 77 functions
  All:  33.80% fuzzy, 27.00% matched, 12.64% linked (11961 / 28465 functions)
  main/MetroidPrime/CActorModelParticles: 65.51% fuzzy, 44.57% matched, 100.00% data
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (unchanged)
GATE PASS 38068b13+4 changed
```

`docs/HANDOFF.md` was rewritten by `goal_check.sh`'s gate run (`MP_GATE_DOCS_WRITE=1`) and
reverted with `git checkout`; the driver discards edits to it. No `tools/`, `build/goal/` or
`docs/research/port_link_baseline.txt` path touched, no `configure.py`/`config/`/`files.cmake`
change, no `.asm`, not committed. `.tmp/opencode/` helpers are untracked and will be cleaned.

## Still open, in the order I would take them next

1. `UpdateOnFire` 95.05% - identify the three floats at `CActor+0x30/0x40/0x50` and the virtual
   `vtable+0x20` entry they feed. That is the whole remaining 4.95%, and the same call shape
   almost certainly appears in the other `Update*` bodies.
2. `__ct__CItem` - needs `fn_8014F9CC` to exist as an out-of-line fill, i.e. a shared-header
   change. Not a `progress` item.
3. `UpdateImplosion` (1224B, 0.46%) and `GeneratePoints` (1676B, 0.24%) are the largest
   untouched functions in the unit and the only ones with no Prime 1 source at all.

WALL: StartBurnDeath 89.90% - one instruction: retail's `cntlzw`+`rlwinm r0,r0,27,31,31` mask needs MWCC to have an unknown range for CPlayer+0x38C, and the range comes from the declared 4-value enum in the shared CPlayer.hpp; eight source spellings tried, none changes it.
WALL: UpdateOnFire 95.05% - retail builds a CVector3f on the stack from CActor+0x30/0x40/0x50 and calls a virtual vtable+0x20 entry where ours calls non-virtual SetGlobalOrientAndTrans(const CTransform4f&); the three fields and the entry are unidentified.
WALL: GetNextBestPt 97.14% - one instruction: retail loads 0.0f from .sdata2, MWCC folds the literal; file-scope `static const float` and external-linkage `const float` both folded and were not even emitted.

---

# Run 5 (2026-10-01, lane 6)

## FIRST: re-measure. Run 4's change never landed.

The clean tree at `e2c47bce` (eighth upstream sync) was **44 / 77**, not the 46 that run 4's notes
report - `grep push_back_unsafe src/MetroidPrime/CActorModelParticles.cpp` finds nothing, and run 4's
diff is in no commit. Run 3's commit (`50954ed1`) landed only its `__dt__CItem` / `PointGenerator` /
`GetNextBestPt` part. **Every number in runs 3 and 4 was re-measured here before being used.** The
gap-list gate fix run 4 described *is* present on this tip (`port_link_gap.md` says 214), so no
gate repair was needed.

## Result

`main/MetroidPrime/CActorModelParticles` **44 -> 48 / 77** matched functions.
Unit fuzzy 63.20% -> **66.54%**, matched code 42.39% -> **52.33%**.
`All:` 12087 -> **12091** matched (28465 total). Linked 5795 -> 5795.
The unit stays `NonMatching`; no flip attempted.

`./tools/goal_check.sh build/goal/item.json` -> **PASS** (measured, exit 0).

| function | before | after | what did it |
|---|---|---|---|
| `__ct__Q220CActorModelParticles7CSystemFPCc` | 75.53% | **100%** | run 4's `push_back_unsafe`, re-measured |
| `Update__Q220CActorModelParticles7CSystemFv` | 96.11% | **100%** | run 3's single `\|\|`, re-measured |
| `UpdateFirePop__Q220CActorModelParticles5CItemFfPC6CActor` | 89.82% | **100%** | **NEW** - see "no named local" below |
| `UpdateIcePop__Q220CActorModelParticles5CItemFfPC6CActor` | 89.82% | **100%** | same |
| `StartBurnDeath__20CActorModelParticlesFR6CActorR13CStateManager` | 1.61% | **90.87%** | run 4's body + the `clrlwi` re-narrowing |
| `UpdateOnFire__Q220CActorModelParticles5CItemFfP6CActorR13CStateManager` | 86.13% | **98.62%** | run 4's sfx block + **NEW** `GetTransform().GetTranslation()` |
| `GetNextBestPt__FiRC13CSkinnedModel...` | 87.14% | **97.14%** | run 3's reference `delta` + `MagSquared` spelled out |

Four functions reached 100%; three more moved a long way. Nothing regressed. Everything is in
`src/MetroidPrime/CActorModelParticles.cpp` - no header change, no new symbol, no new undefined
reference, no layout change.

## THE FINDING: no named local for a by-value intermediate (breaks run 2's and run 4's wall)

Run 2 tried **nine** spellings of `UpdateFirePop` and wrote `WALL: UpdateFirePop 89.82%`; run 4 pinned
the stack slots down and re-confirmed the wall. Both were solving the wrong problem. Retail's
`0x8014ed0c..0x8014ed90`:

```
addi r3,r1,72 ; addi r5,actor+0x24 ; bl GetBounds        # sret -> 72  (no copy!)
addi r3,r1,36 ; addi r4,r1,72     ; bl GetCenterPoint   # sret -> 36
lfs f2,36(r1) ; lfs f1,40 ; lfs f0,44
addi r4,r1,60 ; stfs f2,60(r1) ; stfs f1,64 ; stfs f0,68
vtable+0x20                                                  # SetGlobalTranslation
```

Two slots per arm: the box, and a **second** `CVector3f` the return value is *copied* into. The fix is
to stop naming the box and name the vector instead:

```cpp
const CVector3f center = actor->GetModelData()->GetBounds(actor->GetTransform()).GetCenterPoint();
gen->SetGlobalTranslation(center);
```

179/179 instructions, 716/716 bytes, **100%** - for both `UpdateFirePop` and `UpdateIcePop`.

Measured, so nobody repeats it (ours is the left column, all four spellings apply to **both** functions):

| spelling | result |
|---|---|
| `const CAABox bounds = ...; SetGlobalTranslation(bounds.GetCenterPoint());` | 89.82% - 179 insns; the box costs a 12-insn copy |
| `CAABox bounds = ...` (non-const) + named `center` | 93.21% - 191 insns; still copies the box |
| `const CAABox bounds = ...` + named `const CVector3f center` | 93.21% - 191 insns |
| `CAABox bounds = ...` + named `CVector3f center` | 93.21% |
| **prvalue chain + named `const CVector3f center`** | **100.00%** |
| prvalue chain, **no** named local (`SetGlobalTranslation(...GetCenterPoint())`) | 90.55% - one slot is too few |

**Generalisable, and it is a rule not a spelling: in MWCC, naming a local initialised from a
by-value-returning call adds a copy of that return value.** `const` on the local changes nothing. So
the source that matches is the one with the **fewest** locals, and the prvalue chain must not be
shortened into the call argument. Note the two errors cancel: with the named box we had one slot too
many *and* one copy too many, which is why every run-2 spelling moved the total size.

`GetOtherBounds()` (not `GetRenderBoundsCached()`) is still required - run 2's note stands.

## `UpdateOnFire` 86.13% -> 98.62%: the three floats are the transform's m03/m13/m23

Run 4 left this open: "identify the three floats at `CActor+0x30/0x40/0x50` and the virtual
`vtable+0x20` entry they feed". They are not model scale and not `mPosition`:

```
8014dfd4: lfs  f2,80(r28)      # actor+0x50
8014dfdc: lfs  f1,64(r28)      # actor+0x40
8014dfe4: lfs  f0,48(r28)      # actor+0x30
8014dfe8: stfs f0,48(r1) ; stfs f1,52(r1) ; stfs f2,56(r1)    # a CVector3f temp at r1+48
8014dff8: lwz  r12,32(r12)                                      # vtable+0x20
8014e000: bctrl                                                # r4 = r1+48
```

`CActor::mTransform` is at 0x24 (the `AddEmitter` call's `actor+0x54` translation proves it, and
`include/MetroidPrime/CActor.hpp:279` says `// x24`). So 0x30/0x40/0x50 are `mTransform + 0x0C /
0x1C / 0x2C` = **`CTransform4f::m03 / m13 / m23`**, i.e. the source is

```cpp
gen->SetGlobalTranslation(actor->GetTransform().GetTranslation());
```

`CTransform4f::GetTranslation()` returns `CVector3f` **by value**, so MWCC materialises the three
loads into a stack temp and passes its address. `CActor::GetTranslation()` returns
`const CVector3f&` to `mPosition` (0x54) and emits no copy at all - which is what the old
non-virtual `SetGlobalOrientAndTrans(actor->GetTransform())` was doing, 11 instructions short.
**vtable+0x20 is `SetGlobalTranslation`**, confirmed because our `Update(dt)` already dispatches
through `vtable+0x0C` from the same header.

The sfx block (run 4's) then took the function the rest of the way; what is left is the redundant
`clrlwi r4,r26,16` before `AddEmitter`, recovered by an explicit `static_cast<ushort>(sfx)` at the
call (95.05 -> 98.62). Retail emits it because the value in `r26` came from three different arms;
declaring `sfx` as `short` instead gives `extsh` and is **worse** (97.84).

## What I measured and rejected, so the next run does not repeat it

- **`StartBurnDeath` is still one instruction from 100% and the mask is a real wall.** Retail
  `0x8014b74c`: `cntlzw r0,r0; rlwinm r0,r0,27,31,31; neg r3,r0; addi r0,r3,9602`; ours
  `cntlzw r0,r0; srwi r3,r0,5; addi r0,r3,9601`. Same value for every value the enum can hold.
  MWCC picks `srwi 5` when it has proved the range is 5 bits and `rlwinm 27,31,31` when it has not.
  Six more spellings tried here, **all measured**, none better than 90.87%: `static_cast<ushort>(x)
  == 0` 89.34; `static_cast<ushort>(x) != 0` (arms flipped) 87.50; `static_cast<uint>(x) == 0u`
  90.87; `static_cast<uint>(x) <= 0u` 90.87; `static_cast<unsigned>(x) >= kMS_Unmorphed` (arms
  flipped) 80.08; `static_cast<int>(x) < 1` 83.45. The range comes from the declared 4-value enum in
  the shared `CPlayer.hpp`, so this needs a shared-header change with unbounded blast radius.
- **`__ct__CItem` is confirmed a dead end, and now for a second, independent reason.** Run 3/4
  blamed the out-of-line `fn_8014F9CC` fill of `mOnFireGens`. That is real, but the rest of the
  166-vs-127-instruction gap is that retail calls **`__ct__13CUnitVector3fFRC9CVector3f` out of
  line** (`0x8014f934`, sret at `r1+16`) for `mImplosionClipPlane`, where our header inlines it and
  copies the three floats straight in. Two inline/out-of-line decisions in shared headers. Not a
  `progress` item. The constructor's *initial values* are right - retail stores 0/0/0/-1/0/99 at
  0xD8..0xEC, exactly as we do; the diff only showed a schedule difference.
- **The `.sdata2` float-literal pool slot is still unreachable** (runs 3 and 4 agree). It is all
  that is left in `UpdateOnFire` 98.62% (two `lfs f0,0(0)`), `UpdateAshGen` 99.97% (three),
  `GetNextBestPt` 97.14% (one). Retail loads them from r2-relative addresses in the small-data zero
  area; we fold the literal. Run 4 tried `static const float` at file scope and external-linkage
  `const float`; both folded and were not even emitted. I did not find a third spelling.
- **`AddEmitter`'s hidden sret pointer.** `AddEmitter__11CSfxManagerFUsRC9CVector3fibbs` reads
  `r3..r9`: `r3` is the `CSfxHandle` return slot, `r4` the id, `r5` the position, `r6` the area,
  `r7/r8` the bools, `r9` the priority. The mangled name lists one fewer parameter than the body
  uses, which cost me a while - do not read the register assignment straight off the name.
- Not attempted, and why: `UpdateImplosion` (1224 B, 0.46%), `GeneratePoints` (1676 B, 0.24%),
  `Render` (548 B, 0.73%) and `AddStragglersToRenderer` (420 B, 0.95%) are all still `TODO` stubs
  with no Prime 1 source; every one of them needs renderer/audio calls the port has no definition
  for, and a `progress` item is only *counted* at 100%, so a 90% transcription gains nothing and
  risks the link gate. The 19 `fn_*` gaps (1916 B total) are MWCC's out-of-line template COMDATs;
  they can only be matched under their `fn_` names, which C++ cannot produce.

## Files changed

`src/MetroidPrime/CActorModelParticles.cpp` only - seven functions. Lines: `push_back_unsafe` :39,
`CSystem::Update` guard :80, `UpdateFirePop` :285, `UpdateIcePop` :316, `UpdateOnFire` sfx :422 and
`SetGlobalTranslation` :445, `GetNextBestPt` :637, `StartBurnDeath` :752.

## Verification (all measured on this tree)

```
./tools/goal_check.sh build/goal/item.json   PASS (exit 0)
  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)   ok
  counts: matched 12087 -> 12091   linked 5795 -> 5795
  target rose: main/MetroidPrime/CActorModelParticles: 44 -> 48 / 77 functions
  All:  34.22% fuzzy, 27.32% matched, 12.75% linked (12091 / 28465 functions)
  main/MetroidPrime/CActorModelParticles: 66.54% fuzzy, 52.33% matched
./tools/probe_sources.sh   probe: 747 files, 0 failed, 0 errors; link: LINKED (291 undefined, 0 duplicates)
python3 tools/check_symbol_names.py   checked 525 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit MetroidPrime/CActorModelParticles   ok
```

`docs/HANDOFF.md` was rewritten by `goal_check.sh`'s gate run and reverted with `git checkout`; the
driver discards edits to it. No `tools/`, `build/goal/` or `docs/research/` path touched, no
`configure.py`/`config/`/`files.cmake` change, no `.asm`, not committed. `.tmp/opencode/` helpers
(`probe.sh`, `try.py`, `sbs2.py` - all from runs 3/4) are untracked and will be cleaned.

## Still open, in the order I would take them next

1. `UpdateImplosion` (1224 B) and `GeneratePoints` (1676 B) are the largest untouched functions in
   the unit and the only ones with no Prime 1 source. Each is a whole item.
2. The `.sdata2` float-literal pool slot: three functions, one unknown construct.
3. `StartBurnDeath`'s one-instruction mask, which needs `CPlayer::mMorphBallState` declared with an
   unknown range - a shared-header change that must be done as its own item with a whole-tree
   regression check.

WALL: StartBurnDeath 90.87% - one instruction: retail's `cntlzw`+`rlwinm r0,r0,27,31,31` mask needs MWCC to have an unknown range for CPlayer+0x38C, and the range comes from the declared 4-value enum in the shared CPlayer.hpp; six further source spellings measured this run, none changes it.

## The item's own table (it asked for before% / after% / whether Prime 1's source was enough)

| function the item listed | before | after | Prime 1's source |
|---|---|---|---|
| `Update__Q220CActorModelParticles7CSystemFv` | 96.11% | **100%** | body unchanged; needed the two guards merged into one `\|\|` |
| `__ct__Q220CActorModelParticles7CSystemFPCc` | 75.53% | **100%** | body unchanged; the *API call* was wrong - `push_back_unsafe`, not `push_back` |
| `__ct__Q220CActorModelParticles5CItemFRC7CEntityR20CActorModelParticles` | 42.04% | 42.04% | body and values are right; blocked by two inline/out-of-line decisions in shared headers |
| `UpdateFirePop__...FfPC6CActor` | 89.82% | **100%** | Prime 1's `AUTO(bounds,...)` helped; the winning shape is the *unnamed* box + a named `CVector3f` |
| `UpdateIcePop__...FfPC6CActor` | 89.82% | **100%** | identical body, identical spelling |
| `UpdateAshGen__...FfPC6CActorR13CStateManager` | 99.97% | 99.97% | Prime 1 plus Echoes' `mAshQueuedParticles` clamp; residual is the float pool slot |
| `UpdateOnFire__...FfP6CActorR13CStateManager` | 86.13% | **98.62%** | Echoes' player-count sfx selection + `GetTransform().GetTranslation()` |
| `AddStragglersToRenderer__20CActorModelParticlesCFRC13CStateManager` | 0.95% | 0.95% | no Prime 1 source; still a TODO stub |
| `Render__20CActorModelParticlesCFRC13CStateManagerRC6CActor` | 0.73% | 0.73% | no Prime 1 source; still a TODO stub |

**Superseded WALL lines in this file, for the next run:** `WALL: UpdateFirePop 89.82%` (run 2),
`WALL: UpdateIcePop 89.82%` (run 2) and `WALL: UpdateOnFire 95.05%` (run 4) are all **broken** - both
pop functions are now at 100% and `UpdateOnFire` at 98.62%. Do not stop on them.

---

# Run 6 (2026-10-01, lane 8)

## FIRST: re-measure. Run 5's numbers were stale again, and its two `TODO` stubs are the item.

The clean tree at `ef658434` measured **50 / 77**, not run 5's 48: `ecde8b30` and `d2ba0336` landed
run 5's `push_back_unsafe`, single-`||` `CSystem::Update`, `UpdateFirePop`/`UpdateIcePop`,
`StartBurnDeath`, `UpdateOnFire` sfx block, `GetNextBestPt` and the `d2ba0336` declaration-order
fixes. **Do not re-derive anything run 5 measured.** This run therefore went after the two functions
the item itself named and that every previous run left as `TODO` stubs: `AddStragglersToRenderer`
and `Render`.

**Run 5's claim that they have "no Prime 1 source" is wrong.** Prime 1 has both
(`prime-ref/src/MetroidPrime/CActorModelParticles.cpp:732` and `:778`); what Echoes dropped is only
Prime's thermal-visor branches, because Echoes has no visor.

## Result

`main/MetroidPrime/CActorModelParticles` **50 -> 52 / 77** matched functions.
Unit fuzzy 66.58% -> **71.78%**, matched code 57.92% -> **63.16%**.
`All:` 12149 -> **12151** matched (28465 total). Linked 5860 -> 5860. The unit stays `NonMatching`;
no flip attempted.

`./tools/goal_check.sh build/goal/item.json` -> **PASS** (measured, exit 0).

| function | before | after | notes |
|---|---|---|---|
| `AddStragglersToRenderer__20CActorModelParticlesCFRC13CStateManager` | 0.95% | **100%** | 420/420 bytes, **first try** |
| `Render__20CActorModelParticlesCFRC13CStateManagerRC6CActor` | 0.73% | **100%** | 548/548 bytes, 2 tries (see the `TUniqueId` note) |

Nothing regressed: `per-function diff  matched 12149 -> 12151  linked 5860 -> 5860  (+2 functions
at 100%, 0 units newly linked)`.

## The CItem layout, measured (both new functions need every member offset)

`tools/dis.sh` gives CItem-relative offsets because a `CItem*` is a `rstl::list` node plus 8.
Worked out from the disassembly and confirmed against `NESTED_CHECK_SIZEOF(CItem, 0x16c)`; the
sizes that matter are `TUniqueId` = **2** (not 4) and `CToken` = **8** (a pointer plus a `bool`),
which is what puts `mAshy` at 276 and `mIcePopGen.mItem` at 288 where retail reads them:

```
  0 mId(2)   4 mAreaId   8 mOnFireGens.mCount  12 mOnFireGens.mData[8x12]
108 mOnFireDelayTimer  112 mOnFire  116 mSfx
120/124 mAshGen   128 mAshPointIterator  132 mAshMaxParticles  136 mAshQueuedParticles  140 mAshSeed
144 mIceGens.mCount  148 mIceGens.mData[4x8]  180 mIcePointIterator  184 mIceSeed
188/192 mFirePopGen  196/200 mElectricGen  204 mElectricPointIterator  208 mElectricSeed  212 mElectricColor
216/220 mImplosionGen  224 mImplosionPointIterator  228  232  236 mImplosionSeed
240 mImplosionPoint(12)  252 mImplosionClipPlane(16)  268/272 mRainSplashGen  276 mAshy
280/284 mIcePopGen?? -> retail reads mIcePopGen.mItem at 288
292 mParticleOffsetScale  300 mIceXf  352 mParent  356 mRemTime  360 mLockDeps  = 0x16c
```

**Two things a next run should not have to re-derive.** `mAutoPtr.mItem` is the pointer, at
*member+4*, so `mAshGen`'s pointer is 124 and not 120 - every `lwz` in the two new functions is
`4(rX)`, `8(rX)` or `12(rX)` past the member, never the member itself. And the 8-element
`mOnFireGens` loop walks with **stride 12** (`addi r28,r28,12`), because
`pair<auto_ptr<CElementGen>, uint>` is 12 bytes (`auto_ptr` is `{bool; T*}` = 8) - the counted
`i < 8` loop and the pointer-increment loop over `mIceGens` (stride 8, bounded by `mCount << 3`)
are different shapes and retail keeps them different.

## The CParticleGen vtable offsets, measured (needed for the `Render` body)

`__vt__11CElementGen = .data:0x803BA3D8, size 0x8C`. The two leading words are zero, and
`__dt__11CElementGenFv` sits at `+0x08`, so **MWCC's vptr points at the start of the symbol** -
there is no Itanium `+8` address point on this ABI. Retail's `Render` loads
`lwz r12,16(r12)`, i.e. **slot 4 = `CParticleGen::Render()`**, which is where the header's
declaration order (`~dtor`, `Update`, `Render`, `SetOrientation`, `SetTranslation`) already puts
it. Cross-check: `SetGlobalTranslation` is at `+0x20`, which is what run 5 measured for the
`SetGlobalTranslation` call in `UpdateOnFire` - the two independent calibrations agree, so
`gen->Render()` needs no header change.

`AddStragglersToRenderer`'s `AddParticleGen` is `vtable+0x44` on `gpRender` (`CCubeRenderer*` ->
`IRenderer::AddParticleGen(const CParticleGen&)`, already used all over `src/`), and it is a
**virtual** call, so it adds no link name.

## Echoes' `Render` is Prime 1's with three differences, all readable from the disassembly

1. No `CGraphics` save/restore is *conditional*: the two early `return`s (not-found item, occluded
   area) skip `CGraphics::SetModelMatrix` in retail too - the single epilogue block at
   `0x8014BDC0` is shared, and the restore is the last thing before it.
2. The rain-splash generator draws **itself**, non-virtually:
   `bl Draw__20CRainSplashGeneratorCFRC12CTransform4f` with `r4 = &actor.mTransform`. Its guard is
   `actor.HasModelData()` - retail reads `CModelData+0x10` (`mAnimData.mItem`, `HasAnimation()`)
   then `CModelData+0x28` (`mNormalModel`'s validity byte, `HasNormalModel()`), which is exactly
   `include/MetroidPrime/CActor.hpp:161`.
3. `GetNextBestPt`-style loops are absent; only the generator list is walked.

The implosion generator is submitted and rendered too (`CItem+0xDC` = `mImplosionGen.mItem`), which
Prime 1 has no counterpart for. `mImplosionGen` in `Render`/`AddStragglersToRenderer` is a
**`CElementGen*`**, so `Render()` and `AddParticleGen()` apply unchanged.

## The one edit that took `Render` from 99.98% to 100%: name the `TUniqueId`

Retail emits **two** `sth r0,N(r1)` for the id and passes `r5 = r1+8`. Passing the prvalue
`FindSystem(actor.GetUniqueId())` gives the same two stores in the same slots *swapped*
(`r5 = r1+12`), which is 6 bytes of 548 and reads as 99.98%. Naming it -
`const TUniqueId uid = actor.GetUniqueId();` before the `FindSystem` line - reproduces retail's
order exactly. This is run 5's "no named local for a by-value intermediate" rule pointing the
*other* way: there, naming the local added a copy of a 24-byte `CAABox`; here, not naming a 2-byte
`TUniqueId` makes the compiler allocate the argument slot first. **The rule is about which object
MWCC materialises, not about naming being good or bad.**

## The link gap this opened, and why it is listed rather than defined

`CGraphics::GetModelMatrix()` is a header inline returning a reference to the static member, so
`Render` references `_ZN9CGraphics12mModelMatrixE` and the port has no definition for it -
`DolphinCGraphics.cpp:225` is the only one and `files.cmake` excludes that file.
`tools/gate.sh` failed with

```
port link gap               285  MISSING
  gap grew: _ZN9CGraphics12mModelMatrixE is not in port_link_gap_list.md
```

**It is listed, not defined, and that is the right call.** The port already models this exact guest
global as `extern "C" CTransform4f lbl_80416F74` (`.bss:0x80416F74` - retail's own name for it),
and `Carve802C24AC.cpp`'s `CGraphics::SetModelMatrix` writes *that* object. Defining
`CGraphics::mModelMatrix` as a second `CTransform4f` would satisfy the linker and hand the renderer
a model matrix nothing ever writes. That is a plausible-looking stand-in, which the brief forbids;
an open entry in the work list is the honest record. Two files, both outside the forbidden set:

- `docs/research/port_link_gap_list.md` - regenerated with `python3 tools/link_gap.py --write-list`.
  The diff is **exactly** `## static data members (4)` -> `(5)` plus the one new
  `- `_ZN9CGraphics12mModelMatrixE`` line, nothing else.
- `docs/research/port_link_gap.md` - the table's `| static data members | 4 |` -> `| 5 |`, plus a
  dated paragraph in the style of the 2026-09-30 `CTargetReticles` entry. The table's rows now sum
  to 285, which is what the list holds, and `check_docs_claims` passes.

**Generalisable, and it is a rule not a spelling: before defining a port symbol to close a link gap,
check whether the port already models that guest global under its retail `lbl_` name.** If it does,
defining the C++ name creates a second object that nothing writes, and listing the gap is correct.

## What I measured and rejected, so the next run does not repeat it

- **`GetModelData()` is not what the rain-splash guard calls.** `CActor+0x60` is
  `rstl::single_ptr<CModelData> mModelData` and retail reads it directly
  (`lwz r5,96(r30)`), so the guard is `actor.HasModelData()` (which reads the same pointer plus the
  two `CModelData` fields), not a hand-written null test - the byte count is identical either way,
  but `HasModelData()` is what the rest of the tree says the code means.
- **`CItem+0xE8` / `+0xEC` (`228` / `232`) are the wrong way round in the header's names.** Retail's
  `GeneratePoints` epilogue at `0x8014C9C4` decrements `CItem+232` by one per particle and stores
  `random.GetSeed()` to `CItem+236`; the header has `mImplosionMaxParticles = 228` and
  `mImplosionQueuedParticles = 232`. **The layout and every offset are right** (the two 100%
  functions and `StartImplosion`/`StopImplosion` all agree), so this is two member *names* to swap,
  not a layout change - but `UpdateImplosion` must be written with the decrement on `+0xE8`
  (`232`), not on `mImplosionMaxParticles` as named.
- **`GeneratePoints` (1676 B, 0.24%) is now mostly mapped, and is the next real item.** It is Prime
  1's function with `GetSkinnedPosition`/`GetSkinnedNormal` in place of the raw vertex arrays and
  **one extra Echoes block for the implosion generator**, which is the ash block with
  `mImplosionGen` (`CItem+220`) and a decrement of `1` instead of `min_val(16, n)`:
  `0x8014C9B4 ForceParticleCreation(1)`, `0x8014C9C4 lwz r3,232 / stw r3-1,232`,
  `0x8014C9DC stw seed,236`, `0x8014C9E0 stw previousIndex,224`. The rest is Prime 1's five blocks
  in order, and `CRainSplashGenerator::GeneratePoints(CSkinnedModel const&, SSkinningWorkspace
  const&)` already exists in the port (`src/MetroidPrime/CRainSplashGenerator.cpp`). **The risk is
  the electric block**: retail calls `fn_8014CC98` and `fn_8014CCE4` **out of line** (76 bytes each)
  for the two `SetOverride*Pos` calls, and those are two of the 19 unnameable `fn_` COMDATs in this
  unit. If MWCC makes the same inline decision for us the calls resolve; if not, the function cannot
  reach 100% no matter how good the source is. Check that first, with one build, before spending a
  run on the other 340 instructions.
- **`__ct__CItem` (508 B, 42.04%): runs 3, 4 and 5 all called it a dead end** (retail calls an
  out-of-line `fn_8014F9CC` to fill `mOnFireGens` and an out-of-line
  `__ct__13CUnitVector3fFRC9CVector3f` at `0x8014F934`; ours inlines and copies). Not re-measured
  this run; treat as still blocked.
- **`UpdateImplosion` (1224 B, 0.46%)** is Echoes-only and its `CItem` field offsets are now known
  (see the bullet above), which is new information for whoever takes it.
- Superseded for the record: run 5's "Not attempted, and why: `Render` (548 B, 0.73%) and
  `AddStragglersToRenderer` (420 B, 0.95%) are all still `TODO` stubs with no Prime 1 source ...
  needs renderer/audio calls the port has no definition for" is **wrong on both counts**. The
  renderer calls are virtual, and `CGraphics::SetModelMatrix`, `CTransform4f`'s copy constructor
  and `CRainSplashGenerator::Draw` are all already defined in port TUs. Do not skip these again.

## Tooling (re-created from runs 3/4/5's descriptions; all untracked, the driver cleans them)

- `.tmp/opencode/probe.sh` - `ninja` the one unit, regenerate `build/report.json`, print the unit's
  numbers and every sub-100% function. `probe.sh <substr> [<substr>]` filters the list.
- `.tmp/opencode/sbs2.py <obj.o> <symbol> <retail_addr> <size>` - the tool that made this run cheap.
  It disassembles our object symbol and the retail range, normalises `disp(rN)`/branch immediates on
  **both** sides, and aligns the two streams with `difflib`, so one extra instruction does not
  desynchronise the listing. Prints `RETAIL` / `ours` / `imm` lines. Two bugs I had to fix in the
  version reconstructed from run 4's paragraph: the text column must be whitespace-collapsed
  **without** splitting on `\t` (MWCC's object uses tabs in the mnemonic column, and splitting there
  turned every line into a false difference), and the `imm` comparison must be
  `on[j1+k] != rn[i1+k]` - comparing the normalised list against the *raw* retail list reports every
  line as different.
- `tools/bytescmp.py` works fine on this unit when pointed at
  `build/G2ME01/src/MetroidPrime/CActorModelParticles.o`; it is the right tool for "which exact
  bytes", `sbs2.py` for "which shape".
- Do **not** run `objdiff-cli report generate` before `ninja` has written the object: the report
  reads `build/G2ME01/src/.../CActorModelParticles.o`, and a hand compile to any other path leaves
  every percentage stale. `build/report.json` in the tree is whatever the last build left there.

## Files changed

- `src/MetroidPrime/CActorModelParticles.cpp` - the two functions plus four includes
  (`MetaRender/CCubeRenderer.hpp`, `MetroidPrime/CGameArea.hpp`, `MetroidPrime/CWorld.hpp`,
  `Kyoto/Graphics/CGraphics.hpp`). **No header change, no layout change, no new defined symbol.**
  The only new link reference is `CGraphics::mModelMatrix`, listed above.
- `docs/research/port_link_gap_list.md`, `docs/research/port_link_gap.md` - the link-gap entry
  described above. Not part of the decompilation; disclosed because the diff carries them.

## Verification (all measured on this tree)

```
./tools/goal_check.sh build/goal/item.json   PASS (exit 0)
  ok    no judge-owned path touched
  ok    gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12149 -> 12151   linked 5860 -> 5860
  ok    check_symbol_names.py
  ok    All:  34.32% fuzzy, 27.54% matched, 12.89% linked (12151 / 28465 functions)
  ok    target rose: main/MetroidPrime/CActorModelParticles: 50 -> 52 / 77 functions
  ok    no asm added
MP_GATE_DOCS_WRITE=0 ./tools/gate.sh build/goal/judge/report.base.json
  per-function diff  matched 12149 -> 12151  linked 5860 -> 5860  (+2 functions at 100%)
  port link gap ok    GATE PASS  ef658434+4 changed
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (unchanged)
python3 tools/check_decl_order.py --unit MetroidPrime/CActorModelParticles   ok
./tools/unit_fit.sh MetroidPrime/CActorModelParticles.cpp   40 extra functions, 4220 bytes
  (the known rstl COMDAT set, unchanged by this run - AddStragglersToRenderer and Render add none)
```

`docs/HANDOFF.md` was rewritten by `goal_check.sh`'s gate run (`MP_GATE_DOCS_WRITE=1`) and reverted
with `git checkout`; the driver discards edits to it. No `tools/`, no
`docs/research/port_link_baseline.txt`, no `build/goal/` path touched, no `configure.py` /
`config/` / `files.cmake` change, no `.asm`, not committed. `.tmp/opencode/` helpers are untracked
and will be cleaned.

## The item's own table (it asked for before% / after% / whether Prime 1's source was enough)

| function the item listed | before | after | Prime 1's source |
|---|---|---|---|
| `__ct__Q220CActorModelParticles5CItemFRC7CEntityR20CActorModelParticles` | 42.04% | 42.04% | not attempted; runs 3/4/5 measured two inline/out-of-line decisions in shared headers |
| `UpdateOnFire__...FfP6CActorR13CStateManager` | 98.62% | 98.62% | unchanged this run; the two `lfs` pool slots (see below) |
| `AddStragglersToRenderer__20CActorModelParticlesCFRC13CStateManager` | 0.95% | **100%** | Prime 1's body **minus the thermal-visor branches**, which Echoes dropped; matched first try |
| `Render__20CActorModelParticlesCFRC13CStateManagerRC6CActor` | 0.73% | **100%** | Prime 1's body plus the implosion generator, `CRainSplashGenerator::Draw`, and no thermal branches |

## Still open, in the order I would take them next

1. `GeneratePoints` (1676 B) - fully mapped above; check the two out-of-line `fn_` helpers first.
2. `UpdateImplosion` (1224 B) - Echoes-only, and its field offsets are now known.
3. `UpdateOnFire` 98.62% - the two `lfs` pool slots. Measured this run: both are `lfs f0,0(0)` with
   an `R_PPC_EMB_SDA21` relocation to an anonymous `.sdata2` constant, at the **right instruction
   and the right offset in the stream**; the two constants land at pool offsets 0 and 8 where retail
   has them 20 bytes apart (`lfs f0,-24920(r2)` / `-24900(r2)`), i.e. the *emission order into
   `.sdata2`* differs, not the code. This unit's whole `.sdata2` contribution is 0x1C bytes, so the
   order is a whole-TU property and reordering one function's literals will move other functions'
   loads. Runs 3, 4 and 5 each failed at it with source-level spellings; the lever is the order of
   *first use* across the file, not the spelling of any one literal.
4. `StartBurnDeath` 90.87% - run 5's wall stands unmeasured this run; the residual is one
   instruction and needs an unknown value range for `CPlayer+0x38C`.

## Addendum (same run): `GeneratePoints`' implosion block, decoded - it is the ash block

I mapped the rest of `GeneratePoints` after writing the section above, so the next run does not
have to. It is **Prime 1's function with `GetSkinnedPosition`/`GetSkinnedNormal` in place of the
raw vertex arrays, plus one extra Echoes block**, in this order:

1. `for (i = 0; i < 8; ++i)` over `mOnFireGens[i]`: `CRandom16(mOnFireGens[i].second)`,
   `random.Float() * (count-1)`, `gen->SetTranslation(ByElementMultiply(mParticleOffsetScale,
   model.GetSkinnedPosition(workspace, index)))` - `SetTranslation` is `vtable+0x10`.
2. ash block, Prime 1's, with `mAshGen` (`CItem+124`) and `mAshPointIterator` (`128`).
3. **implosion block** - the ash block's shape, not Prime 1's. Decoded from
   `0x8014C868..0x8014C9D0`:

```cpp
if (mImplosionMaxParticles > 0) {                 // CItem+232
  CRandom16 random(mImplosionSeed);               // CItem+236
  int previousIndex = mImplosionPointIterator;    // CItem+224
  while (mImplosionMaxParticles > 0) {
    const int index = GetNextBestPt(previousIndex, model, workspace, count, random);
    const CVector3f pos =
        CVector3f::ByElementMultiply(mParticleOffsetScale,
                                     model.GetSkinnedPosition(workspace, index));
    if (mImplosionClipPlane.GetHeight(pos) > 0.f) {          // CItem+252, the clip test
      mImplosionGen->SetGlobalTranslation(pos);               // vtable+0x18
    }
    CVector3f normal = model.GetSkinnedNormal(workspace, index);
    normal.SetZ(0.f);
    if (normal.CanBeNormalized()) {
      normal.Normalize();
      const CVector3f& right = CVector3f::Cross(normal, CVector3f::Up());
      mImplosionGen->SetOrientation(
          CTransform4f::FromColumns(right, normal, CVector3f::Up(), CVector3f::Zero()));
    }
    mImplosionGen->ForceParticleCreation(1);
    previousIndex = index;
    --mImplosionMaxParticles;                                // stw r3-1,232 at 0x8014C9D0
  }
  mImplosionSeed = random.GetSeed();
  mImplosionPointIterator = previousIndex;
}
```

   Three details that are not guessable: it is a **`while`**, not a counted `for` (the entry
   `b 0x8014C9C4` at `0x8014C874` jumps into the loop *foot*, which tests the old value and
   decrements unconditionally - a `do/while` rotation of `while (m > 0) { ...; --m; }`); the plane
   test is `GetHeight(pos) > 0.f` built by hand from `mImplosionClipPlane.mNormal.x/y/z` at
   `CItem+252/256/260` and `mConstant` at `264` (`fcmpo; cror eq,gt,eq; bne` leaves the loop when
   `dot <= constant`); and there is **no `SetTranslation`** in this block, only
   `SetGlobalTranslation` inside the plane test and `SetOrientation` after the normal test - the
   opposite order from the ash block.
4. ice block, Prime 1's: `MakeIceGen()` on `mParent` (`CItem+352`),
   `SetGlobalOrientAndTrans(mIceXf)` (`CItem+304`), `GetNextBestPt`, `SetTranslation`,
   `CUnitVector3f(normal)`, `CTransform4f::MakeRotationsBasedOnY`, `SetOrientation`,
   `mIceGens.push_back`, then `mIcePointIterator = (mIceGens.mCount == 4) ? -1 : index`
   (`CItem+180`, `mIceGens.mCount` at `144`).
5. electric block, Prime 1's, over `mElectricGen` (`CItem+200`): two
   `random.Range(0, count-1)` + `GetSkinnedPosition` + `SetOverrideIPos`/`SetOverrideFPos` +
   `ForceParticleCreation`, then `mElectricSeed` / `mElectricPointIterator`.
6. `mRainSplashGen->GeneratePoints(model, workspace)` - `CItem+272`, and
   `src/MetroidPrime/CRainSplashGenerator.cpp` already defines that overload, so no new link name.

**Why I stopped rather than writing it.** Two measured reasons, both mine, not inherited:

- **The `.sdata2` pool is a whole-TU property and `GeneratePoints` would add `0.f` and `> 0.f` to
  it.** `UpdateAshGen` (100%) and `GetNextBestPt` (100%) both reach retail only because their
  literal load sits at the right *pool slot* - that is what `d2ba0336` fixed by reordering two
  declarations. Adding two more float literals to this translation could move those loads and take
  two matched functions **down**, which is an outright item failure ("no function anywhere gets
  worse"). Writing `GeneratePoints` is therefore not a one-unit experiment; it is a
  whole-unit-pool experiment, and it has to be measured with the whole unit's function list in view,
  not with a single-function score.
- **The electric block's two `SetOverride*Pos` calls are out of line in retail**
  (`fn_8014CC98` / `fn_8014CCE4`, 76 bytes each, `0x8014CBE4` and `0x8014CC48`). Those are two of
  the 19 `fn_` COMDATs C++ cannot name, so they can never be matched themselves; the *call* only
  matches if MWCC makes the same inline decision for us. One build answers that.

Both are cheap to check first: compile the electric block alone and look at whether
`SetOverrideFPos` appears as a `bl` to a local COMDAT, and compare the unit's `.sdata2` size before
and after. If both are fine, the remaining ~340 instructions are a direct transcription of the
table above.

NEW: progress-prime1-cactormodelparticles-generatepoints | progress | MetroidPrime/CActorModelParticles | GeneratePoints (1676B, 0.24%) is fully mapped in the notes (Prime 1's five blocks plus a decoded implosion block); first check that the electric block's two out-of-line fn_8014CC98/fn_8014CCE4 calls are emitted out-of-line for us too, and that the two new 0.f literals do not move the .sdata2 pool slots UpdateAshGen and GetNextBestPt depend on.
