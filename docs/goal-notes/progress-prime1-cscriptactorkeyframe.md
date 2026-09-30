# progress-prime1-cscriptactorkeyframe

`kind: progress`, target `MetroidPrime/ScriptObjects/CScriptActorKeyframe`
(`src/MetroidPrime/ScriptObjects/CScriptActorKeyframe.cpp`). The unit stays `NonMatching`;
`flip_test.sh` was not run to decide anything.

## Result

`matched_functions` **3 -> 5 of 8** for the unit. `./tools/goal_check.sh build/goal/item.json`
exits 0 with every check green and prints `target rose: main/MetroidPrime/ScriptObjects/
CScriptActorKeyframe: 3 -> 5 / 8 functions`. Whole-DOL `matched_functions` 10367 -> 10369,
`linked` unchanged at 5048, `All:` 31.49% fuzzy / 23.97% matched. The DOL sha1, all 86 RELs, the
port probe, `check_symbol_names.py` and the docs claims all pass, and no `asm` was added.

## Per function (objdiff `fuzzy_match_percent`, `build/report.json`)

| function | before | after | outcome |
| --- | --- | --- | --- |
| `AcceptScriptMsg__20CScriptActorKeyframeFR13CStateManagerRC10CScriptMsg` | 89.34% | **100.00%** | **matched** |
| `Think__20CScriptActorKeyframeFfR13CStateManager` | 85.26% | **100.00%** | **matched** |
| `UpdateEntity__20CScriptActorKeyframeF9TUniqueIdR13CStateManager` | 63.43% | 63.43% | **unmoved** - see the wall below |

The two ScriptLoader functions in the unit (`LoadActorKeyframe`, `LoadAIKeyframe`) have no retail
object here and were never reachable from this unit; the ctor and `~CScriptActorKeyframe` were
already at 100% before this run.

**Prime 1's source matched unchanged nowhere.** Echoes's `CScriptActorKeyframe` is a fork with a
different message set (`kSM_XALD` vs Prime 1's `kSM_InitializedInArea`, a `mUseOriginator` flag
Prime 1 has no equivalent of, a different `Think` loop that tests the current body state before
exiting it). Everything below came from reading retail's disassembly.

## What was measured, and what fixed it

### `AcceptScriptMsg` 89.34% -> 100.00%

The whole diff was **case order in the `switch`**. Retail's dispatch tests `kSM_XALD` first
(`lis r5,22593` / `addi r0,r5,19524` = 0x58414C44, `beq` to **+0x18c**, i.e. *past* the end of the
`kSM_Action` body) and `kSM_Action` second (`beq` to +0x48, the fall-through). Our source declared
`case kSM_XALD:` first, so our `beq` jumped forward to +0x48 and the `kSM_Action` body sat at
+0x60 - every instruction after that was shifted by 8 bytes and the byte comparison fell apart.

mwcceppc emits switch cases in **declaration order**: the first `case` after the dispatch chain
gets the fall-through address, later ones are reached by branches, and a case whose body is
followed by more code lands past the rest. Putting `case kSM_Action:` first and `case kSM_XALD:`
second reproduces retail's addresses exactly. **89 of 113 instructions were wrong before; after the
reorder the only 8 remaining differences are relocations** (`bl` targets, and the two
`lhz r0,-27740(r13)` SDA reads of `kInvalidUniqueId`), which objdiff ignores.

No other change to this function was needed: the `mUseOriginator`/`mPassive` bitfield tests, the
`GetConnectionList()` loop, the `GetIdListForScript` result-pair materialisation and the
`SendScriptMsgs` argument setup were already right.

### `Think` 85.26% -> 100.00%

Three fixes, each measured:

1. **Frame size, 64 -> 80 bytes, and a real `CBodyStateCmd` temporary.** Retail builds the
   command on the stack and passes it by reference: `lis r3,-32709` / `addi r5,r3,9812`
   (0x803B2654 = `__vt__13CBodyStateCmd`), `li r0,10` (`kBSC_ExitState`), `stw r0,28(r1)`,
   `stw r5,24(r1)`, `addi r3,r6,4` (the `CommandMgr`), `addi r4,r1,24`, then `bl DeliverCmd`, then
   a second `lis`/`addi` pair writing 0x803B2654 back to 24(r1) - the temporary's destructor.
   Our source wrote `DeliverCmd(kBSC_ExitState)`, which picks the `EBodyStateCmd` overload
   (`addi r3,r4,4` / `li r4,10` / `bl`), needs no 8-byte stack temporary and needs no destructor.
   Changing it to `DeliverCmd(CBodyStateCmd(kBSC_ExitState))` took the frame from 64 to 80 bytes and
   produced the whole sequence. This is also why the unit's `.text` was 752 bytes short: the extra
   bytes live in this temporary.

2. **Two cached `CAnimData*` locals removed.** Retail re-evaluates the accessor at each use:
   `lwz r3,96(r28)` / `lwz r3,16(r3)` again before `DelAdditiveAnimation`, again before
   `GetCurrentAnimation`, again before `DelAdditiveAnimation` in the `CPatterned` branch. Our
   source cached `CAnimData* animation = ...` once per branch and reused it. Declaring no local and
   spelling `actor->AnimationData()->...` / `ai->AnimationData()->...` at each call site reproduces
   retail's four extra `lwz` pairs and its register choices (retail keeps the result in `r3`/`r4`
   across the compare, ours spilled to `r27`).

3. **`mgr.GetIdForScript(...)` given a named `uid` local.** Retail materialises the result twice:
   `addi r3,r1,12` / `addi r5,r1,20` / `stw r0,20(r1)` for `GetIdForScript`, then
   `lhz r0,12(r1)` / `addi r4,r1,8` / **`sth r0,16(r1)`** / `sth r0,8(r1)` for `ObjectById` - the
   `16(r1)` store is the local, the `8(r1)` store the by-value argument. Writing
   `TUniqueId uid = mgr.GetIdForScript(it->objId); CEntity* entity = mgr.ObjectById(uid);`
   produces both stores. Inlined as a nested call, only one is emitted.

**128 of 129 instructions were wrong before; after these three changes 125 of 141 match and the
16 remaining differences are all relocations** (`bl` targets and the `lfs f1,-28264(r2)` SDA read
of `0.f`).

### `UpdateEntity` - unmoved, and why

`63.43%`, unchanged. 114 of 153 instructions differ, 612 bytes ours against retail's 784. Retail's
`+0x114..+0x1a4` block, which our source replaced with a `TODO` comment, is:

```
800d5cf8  bl  BuildTransitionTree        # parms built at 84(r1): mAnimId, -1, 1.f, then
800d5d00                                 #   xc_=0, four null pointers, mUseLocator=0, mAnimating=1
800d5cfc  lwz r0,32(r1)                 # the returned rc_ptr: two words copied to 40(r1)/44(r1),
800d5d08  stw r0,40(r1)                 #   refcount bumped at 0(r5), then ReleaseData on the
800d5d10  lwz r4,0(r5)                  #   temporary at 28(r1) - i.e. `auto tree = ...` in a
800d5d1c  bl  ReleaseData               #   named local, destructed at the end of the block
800d5d20  lwz r3,40(r1)
800d5d28  cmplwi r3,0
800d5d30  lwz r12,0(r3)                 # virtual call through vtable slot 4
800d5d34  lwz r12,16(r12)
800d5d3c  bctrl
800d5d40  cmpwi r3,3                    # == 3
800d5d48  li   r29,1                    # -> noTrans
800d5d50  addi r4,r1,84
800d5d54  clrlwi r5,r29,24
800d5d5c  bl  SetAnimation              # SetAnimation(parms, noTrans)
```

So `noTrans` is **not** a constant `false`; it is
`tree && tree->GetType() == kMTT_Snap` (`EMetaTransType` has `kMTT_MetaAnim=0, kMTT_Trans=1,
kMTT_PhaseTrans=2, kMTT_Snap=3`, so the compare against 3 is `kMTT_Snap`). Retail then also clears
one `CAnimData` bit after `SetAnimation` (`lbz r0,684(r4)` / `rlwimi r0,r5,1,30,30` /
`stb r0,684(r4)` - offset 684 = 0x2AC, bit 30 = the `x2ac_` byte's bit 6), and finally releases
the tree.

**The blocker is a header one, not a spelling one.** `CAnimData::BuildTransitionTree` is declared
**private** in `include/MetroidPrime/CAnimData.hpp:194` and this function is not a member of
`CAnimData`, so no spelling in this `.cpp` can call it. Making it reachable needs either a
`friend`/access change in that shared header or a public wrapper - a change that touches a header
many other units include, which is outside what a `progress` item on this unit should do. The
`CAnimData` unit is `NonMatching` at 73/216, so header changes there are not free either.

The `CPatterned` branch is separately blocked: retail materialises the `CBCScriptedCmd` with its
own vtable (`0x803B37E0` = `__vt__14CBCScriptedCmd`, then `0x803B2654` = `__vt__13CBodyStateCmd` at
64(r1) with `kBSC_Scripted=22` at 68(r1)), calls `DeliverCmd(EBodyStateCmd)` with `22`, and then
copies the four fields straight into the manager at `700(r30)`/`704`/`708`/`712` - i.e. it assigns
`mScripted` directly, not through the `DeliverCmd(const CBCScriptedCmd&)` path our source takes.
`CBodyStateCmdMgr::DeliverCmd(const CBodyStateCmd&)` currently just forwards
`cmd.GetCommandId()` (`src/MetroidPrime/BodyState/CBodyStateCmdMgr.cpp:97`), so this needs that
overload's semantics too.

WALL: UpdateEntity 63.43% - the retail `noTrans` path needs `CAnimData::BuildTransitionTree`, which is private in the shared header include/MetroidPrime/CAnimData.hpp, plus a `CBodyStateCmdMgr::DeliverCmd` overload change; not reachable from this unit's .cpp.

## Method notes

* Rebuild + per-function percentages: `.tmp/opencode/m.sh` (gitignored) - `ninja` on the unit's
  object, then `objdiff-cli report generate`.
* Instruction-level diff: `python3 tools/bytescmp.py <obj.o> <symbol> <retail_addr> <size>`. It
  counts relocations as differences, so a list that is *all* `bl` and SDA reads means the function
  matches. That was the check that told me both functions were done, before objdiff confirmed.
* `tools/probe_cc.sh` does **not** work for this tree (its include set predates the SDK shim
  queue, so `musyx/musyx.h` is missing). The flags that do work are the ones `build.ninja` uses;
  copy them from `ninja -t commands build/G2ME01/src/<unit>.o`. Worth fixing in the tool.