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
