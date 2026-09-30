# progress-prime1-cscripteffect

Target: `main/MetroidPrime/ScriptObjects/CScriptEffect` (35 functions).

Measured on the queued baseline after `./tools/gate.sh`: 8/35 matched; global matched 9939, linked 4896. Re-measured each listed function against `build/report.json`:

| Function | Before | After | Prime 1 adaptation / result |
| --- | ---: | ---: | --- |
| `SetActive__13CScriptEffectFb` | 93.333336% | 100.000000% | Same body; adding Prime 1's top-level `const` to the by-value parameter matches. |
| `Think__13CScriptEffectFfR13CStateManager` | 88.806170% | 97.519820% | Needed Echoes-specific particle-system handling; Prime 1-shaped nested timer checks, separate timeout/deletable branches and explicit white-color branch improved it, but did not reach 100%. |
| `Render__13CScriptEffectCFRC13CStateManager` | 89.833336% | 89.833336% | Tried Prime 1's `.get()` pointer-check idiom against Echoes' single polymorphic particle system; no measured change. Prime 1's separate electric-system body does not transfer unchanged. |
| `GetSortingBounds__13CScriptEffectCFRC13CStateManager` | 92.121216% | 92.121216% | Prime 1's direct `GetRenderBoundsCached()` fallback was tried and scored 58.52%; reverted. Kept Echoes' `CActor::GetSortingBounds(mgr)` fallback. |

The target rose 8 -> 9 matched functions; global matched rose 9939 -> 9940, linked stayed 4896. `./tools/goal_check.sh build/goal/item.json` passed: full gate (DOL and all 86 REL hashes, report diff, docs/wiring/probes), symbol check, no asm, and strict target increase. No flip was run; this is a `progress` item.

---

## Run 2 (lane 6, 2026-09-30)

Re-measured on this tree. Baseline: unit 9/35 matched, global 10354, linked 4896. All numbers
below are from `build/report.json` after `./tools/decomp_build.sh`; spellings were ranked with
`tools/try_batch.py` (differing-instruction count) and confirmed with `tools/bytescmp.py`.

The three functions the item named were not the productive targets: the item's `reason` lists
`Think`, `Render` and `GetSortingBounds` because they share a name with Prime 1, but a `progress`
item is judged on the unit's whole matched count, and the two biggest wins were a *bitfield
signedness* fix and a *return-by-value helper* that Prime 1's source implies and this port's
source had inlined away.

| Function | Before | After | What produced it |
| --- | ---: | ---: | --- |
| `AddToRenderer__13CScriptEffectCFRC13CStateManager` | 85.000% | **100.000%** | `mRenderOrder : 2` was an **`ERenderOrder` (signed enum) bitfield**. Retail reads it with `rlwinm. r0,r0,27,30,31` — a plain unsigned 2-bit field test. Declaring it `uint mRenderOrder : 2;` drops MWCC's sign-extend (`srawi`+`extsb`, 3 instructions) and the function goes 64 -> 56 bytes, exactly retail's size. |
| `Think__13CScriptEffectFfR13CStateManager` | 97.520% | **100.000%** | Two independent fixes. (a) Retail's `if (GetTransformDirtySpare())` body has **two** 48-byte `CTransform4f` stack temporaries and one extra `__ct__12CTransform4fFRC12CTransform4f` call; the two-step form in this port produced one. Prime 1's `static inline CTransform4f ClearTrans(const CTransform4f&)` returns by value, which is exactly what forces the second temporary — re-adding it verbatim (same body, same name) matched. (b) The final `GetModelFlags().GetTrans() != 0` test needs `static_cast< char >(...)`; without it MWCC emits `cmpwi r0,0`, retail emits `extsb. r0,r0`. This is the same idiom `CModelFlags::operator==` already carries (see its `// TODO: cast to char for extsb` comment at `include/Kyoto/Graphics/CModelFlags.hpp:102`). |
| `GetGlobalScale__13CScriptEffectCFv` | 64.759% | 93.103% | Prime 1 has no counterpart, but `CVector3f::One()` is a `static const CVector3f&`, and the ternary returning it made MWCC materialise a **pointer** (`lis`+`addi` to `lbl_8041AB7C`) and load through it. Retail loads one `lfs f0,-30788(r2)` and stores it to all three floats — a *value*. Writing `if (get()) return ...; return CVector3f(1.f,1.f,1.f);` gets the value shape. Remaining 2 instrs are pure scheduling. |
| `Render__13CScriptEffectCFRC13CStateManager` | 89.833% | 96.071% | Retail keeps the particle-count in **r3** across the branch merge; a `const int count = <ternary>` made MWCC copy it to **r4** in both arms. Splitting the ternary into `int count; if/else` (89.8 -> 3 differing instrs) plus hoisting `CParticleGen* ps = mParticleSystem.get();` inside the `if (count > 0)` body closes the rest of the register-allocation gap. Remaining 3 instrs are scheduling. |

Also tried and measured, **no** improvement (do not repeat): `Render` with the count as `const`/
`s32`/`long`/`uint`/`= 0`-initialised, `0 < count`, `mNumParticlesDrawing = mNumParticlesDrawing + count`,
`+= static_cast<uint>(count)`, accumulating *after* `Render()` (16), a `CParticleGen*` local hoisted
*outside* the `if` (19) or replaced by `ps->` throughout, `CParticleGen&` reference (6). The two
arms of the `'PART'` test swapped (13). For `GetGlobalScale`: a named `one` local in either order
(2, no better), the `!= nullptr` spelling (2), `if (!get()) return One; return ...;` (13), and
assigning to a pre-declared result (18). For `IsSystemDeletable` and `GetSortingBounds` every
alternative shape measured **worse or equal** — see the walls below.

The unit rose 9 -> 11 matched functions; global matched 10354 -> 10356, linked 4896 -> 4896
(unchanged, which is what a code-only change must do).
`./tools/goal_check.sh build/goal/item.json` **PASS**: full gate (DOL sha1, all 86 REL sha1s,
report diff, module wiring, docs claims, port probe), `check_symbol_names.py` 0 missing, no asm,
strict target increase, no judge-owned path touched.
`tools/probe_sources.sh`: 752 files, 0 failures, link LINKED (250 undefined, 0 duplicates).
`tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptEffect`: ok.
`tools/unit_fit.sh`: the 10 "extra" functions are unchanged from the clean tree (COMDAT inline /
template destructors) — this change adds none.

WALL: IsSystemDeletable__13CScriptEffectCFv 77.810% - after 6 spellings (ternary, if/else both
orders, `!get() ||`, named bool, `= true` pre-init) the residual is 4 instructions and all four
are the *same* two loads (`lwz r3,348(r3)` / `cmplwi r3,0`) hoisted above the `stw r0,20(r1)` /
`stw r31,12(r1)` prologue instead of after it. No source spelling moved the scheduler.

WALL: GetSortingBounds__13CScriptEffectCFRC13CStateManager 92.121% - residual is 2 instructions,
`lhz r4,400(r4)` emitted before the `stw r29`/`mr r29` prologue pair instead of after. Tried
reversed comparison, a `nullptr` local, splitting the cast out, and an early-return shape; all
equal or worse (up to 11).

NEW: match | MetroidPrime/ScriptObjects/CScriptEffect | the other 24 functions in the unit are at
0-92% and are not a single-item target: `AcceptScriptMsg` 46.7%, `CreateSystem` 58.0%,
`PreRender` 79.5%, `PreRenderAllViewports` 84.8%, `UpdateGeneratorRate` 85.6%, the constructor
88.2%, `UpdateSpline` 93.0% and `__ct__15CGameSplineDesc` 92.3% are all reachable-looking and each
would be its own `progress` item; split them out rather than re-queuing the whole unit.

NEW: match | MetroidPrime/ScriptObjects/CScriptEffect | `PreRenderAllViewports` 84.8% and
`UpdateSpline` 93.0% both differ from retail mainly in the **stack frame size** (-128 vs -144 and
-224 vs -240), i.e. an extra local rather than a logic difference; `UpdateSpline` also calls
`mSpline.GetDuration()` again in retail where this port caches it in a local. That is a
struct-layout/frame-shape investigation, not a spelling one.
