# progress-unit-cscriptwater-inhabitants

`kind: progress`, target `MetroidPrime/ScriptObjects/CScriptWater`. The unit stays `NonMatching`;
`flip_test.sh` was not run. **Measured: 15/36 -> 17/36 functions matched** in
`build/report.json`; the whole tree `12181 -> 12183` matched, `linked` unchanged at 5860.
`./tools/goal_check.sh build/goal/item.json` prints `PASS`; its own console lines were

    ok    no judge-owned path touched
    ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
    ok    counts: matched 12181 -> 12183   linked 5860 -> 5860
    ok    check_symbol_names.py
    ok    All:  34.39% fuzzy, 27.66% matched, 12.89% linked (12183 / 28465 functions)
    ok    target rose: main/MetroidPrime/ScriptObjects/CScriptWater: 15 -> 17 / 36 functions
    ok    no asm added
    goal_check: PASS progress-unit-cscriptwater-inhabitants

and it left `build/goal/check-counts.log` (`matched 12181 -> 12183   linked 5860 -> 5860`),
`build/goal/check-progress.log` (`...CScriptWater: 15 -> 17 / 36 functions`),
`build/goal/check-names.log` (`checked 525 units; 0 declared names are missing`) and
`build/goal/check-gate.log` (`GATE PASS  e0c96c4a+7 changed`).

Run 1 of `progress-unit-cscriptwater` (the parent item) left this pair as its only measured
blocker and named it exactly: *"the two 100%-matched hooks' real signatures (`UnkVtable84`/`88`
currently take nothing / a `TUniqueId`, retail passes two args to each)"*. That is what this
run resolved, and both functions are now 100%.

| function | before | after | the change that did it |
|---|---|---|---|
| `InhabitantExited__12CScriptWaterFR6CActorR13CStateManager` | 1.61% | **100%** | base call, `actor.SetInFluid(mgr, false, GetUniqueId())`, `GetFluidCount() == 0 && ShouldSendScriptMsgs`, `SendScriptMsg(&actor, GetUniqueId(), kSM_XEXF, kInvalidUniqueId)`, then `TCastToPtr<CGameCamera>` + the vtable-`0x88` hook with `(GetUniqueId(), mgr)` |
| `InhabitantAdded__12CScriptWaterFR6CActorR13CStateManager` | 1.49% | **100%** | same with `true`, `kSM_XENF` and the vtable-`0x84` hook; the fluid count is read into a bool *before* `SetInFluid` and tested after |

## Diff (6 files; `docs/HANDOFF.md` is the judge's own rewrite, not mine)

- `src/MetroidPrime/ScriptObjects/CScriptWater.cpp` - the two function bodies, plus
  `#include "MetroidPrime/Cameras/CGameCamera.hpp"`.
- `include/MetroidPrime/Cameras/CGameCamera.hpp`, `src/MetroidPrime/Cameras/CGameCamera.cpp`,
  `include/MetroidPrime/Cameras/CFirstPersonCamera.hpp`,
  `src/MetroidPrime/Cameras/CFirstPersonCamera.cpp` - `UnkVtable84`/`UnkVtable88` gain the
  `CStateManager&` parameter retail passes them. **This is required, not tidying**: with the
  old one-argument/zero-argument declarations both functions stop one instruction short (see
  finding 1). The *bodies* are untouched, so all four functions keep their bytes and stay at
  100%: `CGameCamera` 25/35 and `CFirstPersonCamera` 14/17, identical before and after.
- `config/G2ME01/symbols.txt` - four lines renamed, the only `config/` change:

      UnkVtable84__11CGameCameraFv                                -> UnkVtable84__11CGameCameraF9TUniqueIdR13CStateManager
      UnkVtable88__11CGameCameraF9TUniqueId                       -> UnkVtable88__11CGameCameraF9TUniqueIdR13CStateManager
      UnkVtable84__18CFirstPersonCameraFv                        -> UnkVtable84__18CFirstPersonCameraF9TUniqueIdR13CStateManager
      UnkVtable88__18CFirstPersonCameraF9TUniqueId               -> UnkVtable88__18CFirstPersonCameraF9TUniqueIdR13CStateManager

  Addresses and sizes are unchanged, and the new names are copied from what mwcceppc actually
  emitted (`powerpc-eabi-nm build/G2ME01/src/MetroidPrime/Cameras/CGameCamera.o`), not
  hand-mangled. This rename is safe for the 86 RELs, and both facts were measured rather than
  assumed: `grep -ral UnkVtable8 orig/G2ME01/files/RelProd/` finds **no** REL that names either
  hook, and the linked DOL itself carries none of these mangled names
  (`grep -ac UnkVtable8 build/G2ME01/main.dol` -> 0, and the same for `SendScriptMsg__`, so the
  names live only in our config and only matter inside the DOL link, where the definition and
  the vtable reference are renamed together). `check_symbol_names.py`: *"checked 525 units; 0
  declared names are missing from their object"*.

## The findings worth keeping

**1. Both camera hooks take `(TUniqueId, CStateManager&)`, and the one missing instruction is
the proof.** Retail's two call sites end:

    800D7B04  lhz r0, 0x8(r29)      ; GetUniqueId()
    800D7B08  mr  r5, r31           ; mgr                 <- the second argument
    800D7B0C  addi r4, r1, 0xc
    800D7B10  sth  r0, 0xc(r1)      ; the id, by address
    800D7B14  lwz  r12, 0x0(r3)
    800D7B18  sth  r0, 0x8(r1)      ; GetUniqueId()'s own return temp
    800D7B1C  lwz  r12, 0x88(r12)

Measured, with the declarations as they were before this change: **98.39%**, 61 instructions
against retail's 62, and the one missing instruction is exactly that `mr r5,r31`. Writing
`camera->UnkVtable88(GetUniqueId())` compiles to 61 instructions; adding `, mgr` and the
parameter makes it 62 and the function 100%. `InhabitantAdded` is the same story through slot
`0x84`, whose declaration took *no* parameters at all. The two `sth`s are not one store: the
first is the argument temp, the second is the temp mwcceppc builds for `GetUniqueId()`'s
by-value return - retail makes three separate `GetUniqueId()` reads here and does not share
them, and neither may we.

**2. `mwcceppc` 2.7 passes a by-value `TUniqueId` *by address*.** That is why
`actor.SetInFluid(mgr, false, GetUniqueId())` is already right with the by-value declaration in
`include/MetroidPrime/CActor.hpp:151` - the `bl` gets `r6 = &temp`, and retail's callee reads
`lhz r0, 0x0(r6)`. Do not "fix" that parameter to a reference; the declaration is not what
determines the call shape here. It also means a virtual that takes a `TUniqueId` can only ever
be *called* with a two-register setup if it has a second parameter - there is no spelling that
sets `r5` for a one-parameter call.

**3. `InhabitantAdded` reads the fluid count before `SetInFluid`; `InhabitantExited` reads it
after.** Not a stylistic difference - the load is on opposite sides of the call in retail's two
functions, so the two bodies cannot share a shape:

    InhabitantAdded   lwz r7,0x110(actor) / neg+or / srwi r31,r5,31 / ... / cmplwi r31,0
    InhabitantExited  bl SetInFluid        / lwz r0,0x110(actor) / cmpwi r0,0

`InhabitantAdded`'s saved bool is spelled `!!actor.GetFluidCount()` - the same idiom, and the
same spelling, that reaches 100% in `Touch`; `InhabitantExited`'s is a plain
`actor.GetFluidCount() == 0` after the call. The `0x8c` virtual in the middle is
`CScriptTrigger::ShouldSendScriptMsgs` (on `this`, `(CActor&, CStateManager&)`, bool-tested
with `clrlwi. r0,r3,24`), which run 1 of the parent item had already identified from the
object.

**4. Retail's message ids differ per function:** `kSM_XENF` (0x58454e46) on entry,
`kSM_XEXF` (0x58455846) on exit, `kSM_XINF` (0x58494e46) from the already-matched
`InhabitantIdle`. All three are in `include/MetroidPrime/CEntityInfo.hpp:215-217`.

## What is left in the unit (all measured, none filed as `NEW:`)

- `CalculateRenderBounds` 90.05% and `GetSplashSound` 85.71% are the parent's measured walls;
  25 and 12 spellings respectively are listed in `docs/goal-notes/progress-unit-cscriptwater.md`
  and nothing new was learned about them here.
- `UpdateSplashInhabitants` 97.43% - the parent already filed
  `NEW: progress-unit-cscriptwater-inhabitants` for it, so it is queued; not re-filed.
- The remaining sub-1% functions (`AcceptScriptMsg`, `Think`, `PreRender`, `Render`,
  `PreRenderAllViewports`, `SetupGrid`, `SetupGridClipping`) and the seven unnamed retail
  functions (`fn_800D9888` ... `fn_800DAE20`, 40-240 bytes each) were not looked at in this
  run, so I have no measurement that any of them can reach 100% and no `NEW:` is filed for
  them. The two hooks that blocked run 1 were the only blocker I measured, and it is gone.

## Gates, all re-run in this worktree

    sha1sum build/G2ME01/main.dol        6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
    ./tools/probe_sources.sh             probe: 749 files, 0 failed, 0 errors; link: LINKED
                                         (289 undefined, 0 duplicates)  <- same 289 as
                                         build/goal/judge/undef.base.count
    python3 tools/check_symbol_names.py  checked 525 units; 0 declared names are missing
    python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptWater
                                         ok: 1 unit(s) checked, none emits its functions out
                                         of retail order
    ./tools/goal_check.sh build/goal/item.json   PASS (above)

`build/goal/check-gate.log` also shows `per-function diff  matched 12181 -> 12183  linked
5860 -> 5860  (+6 functions at 100%, 0 units newly linked)` - the +6 are these two plus the
four camera hooks, which reached 100% under their new names without changing a byte.

## Method

`.tmp/opencode/w_score.sh` rebuilds just this unit and prints the per-function scores;
`.tmp/opencode/w_bytes.py <fn>` diffs retail's (`build/G2ME01/obj/...`, dtk's split of the
DOL) against our (`build/G2ME01/src/...`) instruction *bytes*, flagging `bl`/sda21 lines as
reloc-only. Note the two directories are not interchangeable: `obj/` is retail's object and
`src/` is ours, so disassembling the wrong one makes every function look identical. All scratch,
outside `tools/`, not part of the diff.
