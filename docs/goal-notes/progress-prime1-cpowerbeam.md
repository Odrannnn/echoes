# progress-prime1-cpowerbeam — `MetroidPrime/Weapons/CPowerBeam`

`kind: progress`, target `MetroidPrime/Weapons/CPowerBeam`. **PASS** (`tools/goal_check.sh`, this
worktree). Unit stays `NonMatching`; `linked` is unchanged at 5048 by design.

## Measured result

| | before | after |
| --- | --- | --- |
| unit `matched_functions` | **10 / 14** | **13 / 14** |
| unit `fuzzy_match_percent` | 93.22469 | 96.54 |
| `All: matched` (report.json) | **10321 / 28465** | **10324 / 28465** |

`python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`:

```
matched  10321 -> 10324   linked 5048 -> 5048   (+3 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/Weapons/CPowerBeam :: EnableSecondaryFx__10CPowerBeamFQ210CGunWeapon16ESecondaryFxType
  +100%    main/MetroidPrime/Weapons/CPowerBeam :: InitializeResources__10CPowerBeamFR13CStateManager
  +100%    main/MetroidPrime/Weapons/CPowerBeam :: Update__10CPowerBeamFfR13CStateManager
no regression
```

Per function, as the item asks (Prime 1's source at
`/run/media/odran/Leo/projects/Restored-projects/Chatgpt/prime-ref/src/MetroidPrime/Weapons/CPowerBeam.cpp`):

| function | before | after | Prime 1's source |
| --- | --- | --- | --- |
| `EnableSecondaryFx` | 97.871796% | **100%** | **matched unchanged**, needed no edit - it was only the string-pool shape (see change 1) |
| `Update` | 98.55652% | **100%** | **matched unchanged**, needed no edit - same cause, a free side effect of change 1 |
| `InitializeResources` | 87.98889% | **100%** | **Echoes-only**, absent from Prime 1; written from retail's instructions |
| `Fire` | 33.19672% | 54.10% | **not usable** - different signature and different body (below) |
| ctor / dtor / `ReInitVariables` / `PreRenderGunFx` / `PostRenderGunFx` / `UpdateGunFx` / `Load` / `Unload` / `ReleaseResources` / `IsLoaded` | 100% | 100% | unchanged |

`Fire` is the only function left in the unit. `WALL: CPowerBeam::Fire 54.10% - only the sentinel load
and its `cmplw` are scheduled two slots later than retail's; seven spellings tried, none reach 100%`
(the diff is quoted at the bottom, with every spelling and its score).

## The three changes, and why each is the one the bytes ask for

One file changed: `src/MetroidPrime/Weapons/CPowerBeam.cpp` (+31/-5). Nothing in `configure.py`,
`splits.txt`, `files.cmake` or a header.

### 1. The two pool-object names are `.sdata2` globals, not literals

Retail's `InitializeResources` loads them through small data:

```
lwz r4, -28376(r13)      ; gpSimplePool
addi r3, r1, 16
lwz r5, -20524(r2)       ; SDA21 -> 0x8041D394
```

and `0x8041D394` / `0x8041D398` hold `0x803AADF2` / `0x803AADFC`, which are the `.rodata` strings
`"ShotSmoke"` and `"Power2nd_1"` (read out of `build/G2ME01/main.elf` with `tools/sda.py`'s SDA2 base
`0x804223C0`). Declaring them the way `lbl_8041E2E6` is already declared above gives `lwz r5,<sym>(r2)`
and nothing else. **The values are already defined in `src/MetroidPrime/mainHead.cpp`** (port-only,
inside its `extern "C"` block), so this file only declares them - which is also why
`mainHead.cpp`'s note says defining them here instead would perturb an unrelated unit.

The second half of the win is a consequence, not a choice: with the two literals gone, this unit's
`@stringBase0` holds only `rs_new`'s own `"?\?(?\?)"`, so it sits at offset 0 and retail's
`EnableSecondaryFx` shape (`lis r3,ha; addi r4,r3,lo`, no extra `addi`) appears. Before the change
our object had a 28-byte `.rodata` - `"ShotSmoke\0Power2nd_1\0??(??)\0"` - and emitted
`addi r4,r4,21` after the `lis`/`addi` pair in two places. That single shift was the whole of
`EnableSecondaryFx`'s 2.13% and `Update`'s 1.44%.

**Generalised: retail's `.sdata2` holds a table of `const char* const` pointers to weapon/particle
names**, contiguous at `0x8041D360..0x8041D3C0` - `SamusArmFSM`, `SamusGunFSM`, `BombSet`,
`BombExplo`, `PowerBombExplo`, `GunMotion`, `holoTransition`, the seven `grapple*`, then `ShotSmoke`,
`Power2nd_1`, `IceSmoke`, `Ice2nd_1`, `Ice2nd_2`, `Wave2nd`, `Plasma2nd_1`, `MissileAuxMuzzle`,
`Missile2nd`, `Common_DGRP`, `Power_Anim_DGRP`, `VariaArm`, `BeamThirdPersonFx_DGRP`. Any unit that
calls `gpSimplePool->GetObj("...")` in this tree and is short of 100% for that reason has a pointer
global waiting for it. I did **not** find a second function that is blocked *only* by this, so I
filed no `NEW:` for it - it is a lead, not a measured target. Retail's
`CGunWeapon::InitializeResources` (0x801D8448) indexes that table with
`lis r3,-32709 / addi r3,r3,-21236 / slwi r0,r0,2 / lwzx r3,r3,r0`, so there is a real array behind
it rather than unrelated globals.

### 2. `InitializeResources`' guard is the "resources already allocated" flag, not `mSubtypeBasePose`

Retail:

```
lbz     r0,624(r3)
rlwinm. r0,r0,31,31,31     ; keeps bit 31 of rotl(r0,31) == bit 0 of the byte
bne     <epilogue>
```

`mSubtypeBasePose` is bit 3 and gave `rlwinm. r0,r0,28,31,31`. I confirmed the compiler's `bool : 1`
allocation with a standalone probe compiled by the project's own `mwcceppc` (`wibo ... mwcceppc.exe`,
`-str reuse,pool,readonly -gccinc -common on`, same as `cflags_retro`): for
`struct { uchar pad[4]; bool a:1; ... bool k:1; }`, `if (!s->a)` emits `rlwinm. r0,r0,25,31,31`
(bit 6), `!b` bit 5, `!c` bit 4, `!d` bit 3 ... `!g` bit 0, and the writes come out as
`rlwimi rA,rS,N,31-N,31-N` with N = the bit index. So **the first declared flag is bit 6 and the run
fills downwards; the seventh declared flag is bit 0.** In `CGunWeapon.hpp` that is
`mResourcesAllocated` (after `x270_24`, `mEnableCharge`, `mLoaded`, `mSubtypeBasePose`,
`mSuitArmLocked`, `mDrawHologram`), which is also the only reading that makes sense: retail's
`CGunWeapon::InitializeResources` opens with the same bit-0 test and closes with
`rlwimi r0,r3,1,30,30`, i.e. it sets bit 0 on the way out. `mResourcesAllocated` is otherwise only
initialised to `false` in `CGunWeapon`'s constructor and read nowhere, so the guard is new to it.

That reading is independent of the field naming: retail's `CGunWeapon::Load` writes its
`subtypeBasePose` parameter with `rlwimi r3,r29,4,27,27`, and our `Load` emits those exact four
bytes for `mSubtypeBasePose = subtypeBasePose;`, so bit 3 is the pose flag and bit 0 is something
else. **The `rlwimi` shift is the bit index and `rlwinm`'s is complemented: `rlwimi ...N...` writes
bit N, `rlwinm. r0,r0,S,31,31` tests bit `31-S`.** Worth having written down - it is why the
"obvious" rename was wrong and `x270_24` (which measured as bit 6, not bit 0) was a dead end.

### 3. `Fire` was calling `fn_80036F10()` and throwing the answer away

Prime 1's `Fire` is not adaptable: the Echoes override takes `projectile`, `projectileAttributes`,
`soundId`, `projectileId` and `soundHandle`, and Prime 1 calls `NWeaponTypes::play_sfx` *after* the
base call where Echoes resolves an id and passes it *into* the base call. What retail does, per
`src/MetroidPrime/mainHead.cpp`'s note on `.sdata2 0x8041D248`, is the sentinel plus a table lookup:

```
cmplw   r11,r0              ; soundId == 0xFFFF (lbl_8041E2E6)
bne     <keep the caller's id>
mr      r3,r27 / bl fn_80036F10
clrlwi  r5,r3,24 ; slwi r0,r25,1 ; neg r4,r5 ; or r4,r4,r5 ; rlwinm r4,r4,3,29,29 ; add r0,r4,r0
lhzx    r5,r3,r0            ; lbl_8041D248[row][chargeState]
```

so it is `lbl_8041D248[mgr.fn_80036F10() ? 1 : 0][chargeStage]` - the `rlwinm 3,29,29` is the row
select turned into a 0-or-8 byte offset, which is why the table has to stay two-dimensional
(a flat `(mult ? 2 : 0) + charge` index would need a different scaling and does not match).
`mainHead.cpp` already defines `extern const ushort lbl_8041D248[2][2]`, so only the declaration
was added here.

This took `Fire` from 33.20% to 54.10%. **The residual is two instructions in the wrong slot and
nothing else**:

```
 retail                                ours
 lhz r11,106(r1)                       lhz r11,106(r1)
 stfd f31,88(r1) / fmr f31,f3          stfd f31,88(r1) / fmr f31,f3
 lhz r0,0(0) R_PPC_EMB_SDA21 ...  <--  stfd f30,80(r1) / fmr f30,f2
 stfd f30,80(r1) / fmr f30,f2          stfd f29,72(r1) / fmr f29,f1
 cmplw r11,r0                    <--  stmw r22,32(r1)
 stfd f29,72(r1) / fmr f29,f1          mr r22,r3 ... mr r29,r10
 stmw r22,32(r1)                       lhz r0,0(0) R_PPC_EMB_SDA21 ...  <--
 ... mr r29,r10                        cmplw r11,r0                    <--
 bne <target>                          bne <target>
```

Same 61 instructions, same frame (`stwu r1,-96`, `stmw r22,32`), same register shuffle, same
`lhzx`/`stw` pair; only MPC's slot for the sentinel load and its compare.

Spellings tried for `Fire`, all measured this run, best first:

| spelling | score |
| --- | --- |
| uninitialised `ushort sound;` + `if (soundId == sentinel) sound = table; else sound = soundId;` | **54.10%** |
| the same with `lbl_8041E2E6 == soundId` (operands swapped) | 53.93% |
| assign back to the parameter, no local | 50.57% |
| `ushort sound = soundId;` then conditional overwrite | 49.10% |
| `if (soundId != sentinel) sound = soundId; else sound = table;` | 47.05% |
| `const ushort sound = cond ? table : soundId;` (adds a `clrlwi r0,r0,16`) | 44.18% |
| `lbl_8041D248[mgr.fn_80036F10()]` (no `? 1 : 0`) | 43.85% |
| the tree's original body | 33.20% |

## Notes for the next attempt

- The two things worth trying on `Fire` are both about *where* MPC puts the `.sdata2` load, not
  what the code does: whether declaring `lbl_8041E2E6` as `const ushort` instead of `ushort` (it is
  `0xFFFF`, so `const` is also more honest) changes the load's rank, and whether reading the
  sentinel into a `const` local before the `if` does. Neither was tried - both change the emitted
  bytes in a way that would need measuring, not guessing.
- The unit's `.data` is 92 bytes against a claimed 96, and its `.rodata` has no claimed range at all
  (`0x803AAB80`, retail's own `"?\?(?\?)"`, is in an unclaimed `.rodata` gap). Neither blocks a
  `progress` result, but both would block the flip, so the unit is not one edit from `Matching`.

## What I verified

- `./tools/goal_check.sh build/goal/item.json` -> `PASS progress-prime1-cpowerbeam`
  (`no judge-owned path touched`, `gate.sh` including DOL sha1 / 86 RELs / report diff / docs claims,
  `matched 10321 -> 10324`, `target rose: 10 -> 13 / 14`, `no asm added`).
- `python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json` -> `no regression`.
- `git status --porcelain --untracked-files=all` -> only `src/MetroidPrime/Weapons/CPowerBeam.cpp`.
- The standalone bitfield probe was written to `.tmp/opencode/` and deleted; it is not in the diff.