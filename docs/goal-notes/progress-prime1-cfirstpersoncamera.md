# progress-prime1-cfirstpersoncamera

`kind: progress`, target `MetroidPrime/Cameras/CFirstPersonCamera`. The unit stays `NonMatching`;
`flip_test.sh` was not run to decide anything (it is a `progress` item). Not committed.

## Result

`build/report.json`, `main/MetroidPrime/Cameras/CFirstPersonCamera`:

| measure | before | after |
|---|---|---|
| `matched_functions` | **11 / 17** | **12 / 17** |
| `fuzzy_match_percent` | 11.245161 | 11.887345 |
| `matched_code` | 396 / 8060 (4.9131513 %) | 484 / 8060 (6.004963 %) |
| `matched_data` | 144 / 144 (100 %) | 144 / 144 (100 %) |
| `.text` fuzzy | 11.245161 % | 11.887345 % |

Whole-project `matched_functions` 10005 -> 10006 (`All: 30.83% fuzzy, 23.09% matched, 11.74% linked
(10006 / 28465 functions)`), `complete_units` 724 -> 724, `linked` 4896 -> 4896. No function
anywhere got worse (`report_diff.py` inside `gate.sh` is the check).

`tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS progress-prime1-cfirstpersoncamera`**,
every sub-check `ok` (gate.sh, counts, check_symbol_names.py, All:, target rose, no asm).

## The diff (one file, 2 hunks)

`src/MetroidPrime/Cameras/CFirstPersonCamera.cpp` only.

1. `rstl::string("First Person Camera")` -> `rstl::string_l("First Person Camera")` in the ctor
   initialiser list. **92.50 % -> 99.32 %** on `__ct__18CFirstPersonCamera...`.
2. `if (msg.GetMessage() == kSM_XALD) { ... }` -> a one-case `switch (msg.GetMessage())`.
   **75.91 % -> 100 %** on `AcceptScriptMsg__18CFirstPersonCamera...`; that is the +1 function.

## Per function, before % -> after %

| function | retail bytes | before | after | note |
|---|---|---|---|---|
| `__ct__...RC9TUniqueIdRC12CTransform4f9TUniqueIdfffffii` | 448 | 92.50 | **99.32** | 112 instructions now, instruction-for-instruction identical to retail; 4 bytes left (see wall) |
| `AcceptScriptMsg__...FR13CStateManagerRC10CScriptMsg` | 88 | 75.91 | **100.00** | **newly matched** |
| `UpdateElevation__...FR13CStateManager` | 188 | 6.28 | 6.28 | blocked, see below |
| `Think__...FfR13CStateManager` | 768 | 0.52 | 0.52 | not attempted, see below |
| `UpdateTransform__...FR13CStateManagerf` | 5132 | 0.08 | 0.08 | not attempted, see below |
| `UpdateFluidEffects__...FR13CStateManager` | 1040 | 0.90 | 0.90 | not attempted, see below |

**Prime 1's source was not ported for any of the three functions the item names.** I decoded
retail's own code instead, and for `UpdateElevation` retail's code is a different algorithm from
Prime 1's (see below), so Prime 1's `UpdateElevation` would not have compiled to these bytes.
Prime 1's `Think` is the same shape as Echoes' but Echoes adds the health gate, the fluid-effect
tick, the morphball-transition path with a `close_enough` factor test, a turret block and a
`mPitchTransitionTimer` countdown that Prime 1 does not have. `UpdateTransform` in Echoes is 5132
bytes against Prime 1's much shorter one and inlines whole quaternion/orbit paths.

## Rules that paid for themselves here

### `rstl::string_l`, not `rstl::string`, in a ctor initialiser list

`rstl::string("...")` resolves to the four-argument
`basic_string(const char*, int, const rmemory_allocator&)` and emits `li r5,-1` + `addi r6,r1,8`
plus a `mr r4,r0` to pass the literal, and shifts **every** stack temporary by 4 - so the ctor came
out 456 bytes against retail's 448 and the two `TUniqueId` by-value scratch words landed at 12/16
instead of 8/12. `rstl::string_l("...")` is the free function retail calls
(`string_l__4rstlFPCc`, returns by value) and the whole prologue then matches byte for byte.
**Worth grepping every `rstl::string(` that appears in a constructor initialiser list in this repo.**

### A one-case `switch` is not the same as an `if` for this compiler

`if (msg.GetMessage() == kSM_XALD)` compiles to the fused difference idiom
`lwz r3,8(r31); addis r0,r3,-22593; cmplwi r0,19524; bne` - 9 instructions. Retail emits
`lis r3,22593; lwz r4,8(r31); addi r0,r3,19524; cmpw r4,r0; beq; b` - the constant is *materialised*
into a register and the taken branch is `beq` with an explicit `b` to the join. A `switch` with one
`case` and a `default` produces exactly retail's form. This is why the repo's script objects, which
all use `switch (msg.GetMessage()) case kSM_XALD:`, match: **prefer the `switch` spelling.**

### Reading retail's constants in this repo

`tools/sda.py` already carries the bases (`_SDA_BASE_` 0x8041FD80, `_SDA2_BASE_` 0x804223C0). Note
that the `lfs fX,<neg>(r2)` float constants in *this* unit resolve against **SDA2**, not SDA -
`-22044(r2)` is `0x8041CDA4` = `5.0f`, `-22040(r2)` = `0x8041CDA8` = `0.0f`, `-21984(r2)` =
`0x8041CDE0` = `0.0174532924f` (pi/180). `report.json`'s `virtual_address` field is 0x170 below
`config/G2ME01/symbols.txt`; use `symbols.txt` for addresses, `report.json` for sizes and scores.

## Blocked, with the evidence

### `UpdateElevation` - its one remaining callee is in an unclaimed .text gap (hard wall)

Retail, decoded in full (`.text 0x801B0220`, 188 bytes, 47 instructions):

```
mPitch = 0.f;                                        // stfs f0,568(r3)      (0x0f8 = 0.0f)
if (CameraManager(mgr).IsInCinematicCamera()) return;
CPlayer* player = TCastToPtr<CPlayer>(mgr.GetObjectById(GetWatchedObject()));   // 0x158
if (!player) return;
if (mPitchId == kInvalidUniqueId) return;             // 0x23C
CUnknown42* vol = TCastToPtr<CUnknown42>(mgr.GetObjectById(mPitchId));
if (!vol) return;
mPitch = 0.0174532924f * fn_801FB6CC(player);         // r4 = player + 36 (0x24)
```

Every callee except the last is named and claimed. The last is not:

```
$ python3 - <<'PY'   # ranges from config/G2ME01/splits.txt
... if s <= 0x801FB6CC < e: print(cur, hex(s), hex(e))
PY
hit []                 # no unit's .text covers 0x801FB6CC
prev (2149552208, 2149552528, 'MetroidPrime/ScriptObjects/CUnknown90.cpp')   # 0x801F9190
next (2149576432, 2149576440, 'MetroidPrime/ScriptObjects/Carve801FEEF0.c')  # 0x801FEEF0
```

So `fn_801FB6CC` (0x801FB6CC, `size:0x358`) sits in the ~0x5D60-byte unclaimed gap
0x801F9190..0x801FEEF0. **Nothing in the link defines that address**, so no C++ can call it: the
function cannot be written at all until that range is carved, and carving 0x358 bytes of
un-understood code is a different item. The first fourteen instructions are the ceiling for this
function as things stand.

Two details a later attempt will need: retail stores the watched id **twice** before the first
`GetObjectById` (`sth r0,8(r1); sth r0,12(r1)` with `r4 = r1+12`) but only once before the second
(`sth r0,8(r1)` with `r4 = r1+8`); and the volume type is `TCastToPtr<10CUnknown42>`, not Prime 1's
`CScriptCameraPitchVolume`.

### `__ct__...` - the last 4 bytes are retail's merged string pool (hard wall)

Everything matches except one instruction:

```
retail: lis r9,hi(lbl_803AA5B0); addi r9,r9,lo(lbl_803AA5B0); ...; addi r4,r9,19; bl string_l
ours:   lis r9,hi(@stringBase0);  addi r0,r9,lo(@stringBase0);  ...; mr     r4,r0;    bl string_l
```

`19` is the length of `"First Person Camera"`. Retail's literal is at `lbl_803AA5B0 + 19` inside a
merged `.rodata` pool: `config/G2ME01/symbols.txt:17296` gives
`lbl_803AA5B0 = .rodata:0x803AA5B0; size:0x28`, and the bytes there are
`??(??)\0WaterSheets\0First Person Camera\0` - the literal is 19 bytes into a label owned by other
code. **That `.rodata` range is claimed by no unit** (checked against every section in
`splits.txt`), so nothing in the link defines the label and `extern const char lbl_803AA5B0[]`
would not resolve. A locally declared string cannot reproduce the +19: our pool puts the literal at
offset 0. The only way to get `addi r4,r9,19` is `string_l(label + 19)`, i.e. a pointer past the end
of a real string - wrong code, so I did not do it. 3 of the 4 remaining bytes objdiff counts.

### `Think`, `UpdateTransform`, `UpdateFluidEffects` - not attempted, and not walls

Read the disassembly; none of them is blocked the way `UpdateElevation` is, so I stopped rather than
guess. What a later attempt needs, from retail:

* `Think` (0x801AEA6C, 768 B): `CPlayer* p = TCastToPtr<CPlayer>(mgr.ObjectById(GetWatchedObject()))`,
  return if null; **return without calling `CActor::Think` if
  `p->GetHealthInfo()->GetHP() <= 0.f`** - the virtual at `CPlayer`'s vtable +0x3C is
  `CActor::GetHealthInfo() const` (0x8000B900) and retail reads the float at +4, i.e. `healthB`;
  then `if (mFluidEffectsPending) { UpdateFluidEffects(mgr); mFluidEffectsPending = false; }`;
  then `if (mDeferBallTransitionProcessing) mDeferBallTransitionProcessing = false;` **else** the
  morphball path. The two bools are bits 25 and 24 of the byte at 0x25C: **bit 24 =
  `mDeferBallTransitionProcessing`, bit 25 = `mFluidEffectsPending`** (from the ctor's two
  `rlwimi` at `+0x170`/`+0x174`). Morphball path: `lwz 908` is `mMorphBallState` (0x38C) compared
  with 1 (`kMS_Morphed`), 0 (`kMS_Unmorphed`) and 3 (`kMS_Unmorphing`); `lwz 904` is `mCameraState`
  (0x388) compared with **5**, but the repo's `EPlayerCameraState` stops at 4
  (`kCS_Spawned == 4`), so Echoes' enum has an un-named 6th value this header does not have - that
  needs resolving before the function can match. Then `mCloseInTimer -= dt` (0x240),
  `backupXf = GetTransform()`, `UpdateElevation(mgr)`, `UpdateTransform(mgr, dt)`,
  `SetTransform(ValidateCameraTransform(GetTransform(), backupXf))`,
  `mPitchTransitionTimer -= dt` (0x254), and a turret block on `lwz 4856` (`kTS_Entering` ->
  blend the pitch by `clamp(0, 4868 / -22032(r2), 1.f)`, `kTS_Exiting` -> plain
  `GetTurretTransform(mgr)`), then `CActor::Think(dt, mgr)`.
* `UpdateTransform` (0x801AEE14, 5132 B): 5132 bytes of inlined quaternion/orbit composition. All
  its callees are named, so it is reachable, but it is a full sitting item on its own.
* `UpdateFluidEffects` (0x801AE598, 1040 B): two near-identical halves (enter / leave), each
  building a water-sheet script object: `TCastToPtr<CScriptWater>`,
  `mgr.AllocateUniqueId()`, `rstl::string_l`, `CColor::White()`, then `fn_800EDFE4`, `fn_800EDFA0`,
  `fn_800EDCB4` (all three are *named* in `symbols.txt`, so declared already or declarable),
  `mgr.AddObject()`, then `GetSoundPan` + `CSfxManager::SfxStart` and
  `ApplySubmergedPitchBend`. All callees claimed.

## Gates

```
$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   build/G2ME01/main.dol      (expected value)

$ ./tools/probe_sources.sh
probe: 749 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)

$ python3 tools/check_symbol_names.py
checked 503 units; 0 declared names are missing from their object

$ python3 tools/check_decl_order.py --unit MetroidPrime/Cameras/CFirstPersonCamera
ok: 1 unit(s) checked, none emits its functions out of retail order

$ ./tools/goal_check.sh build/goal/item.json
goal_check: PASS progress-prime1-cfirstpersoncamera
  ok  no judge-owned path touched / gate.sh / counts: matched 10005 -> 10006  linked 4896 -> 4896
  ok  check_symbol_names.py / All: 30.83% fuzzy, 23.09% matched, 11.74% linked (10006 / 28465)
  ok  target rose: main/MetroidPrime/Cameras/CFirstPersonCamera: 11 -> 12 / 17 functions
  ok  no asm added
```

`gate.sh` rewrote the two derived numbers in `docs/HANDOFF.md` (10005->10006, DOL units
8594->8595); I reverted that file so the diff is the source change only - the judge rewrites those
from the tree anyway.

`git status` at the end: `M src/MetroidPrime/Cameras/CFirstPersonCamera.cpp` and nothing else.

## Notes / no NEW items

No `NEW:` line. The `UpdateElevation` blocker and the ctor's 4 bytes are measured walls, which
`docs/goal-unit-prompt.md` says to record here rather than queue; the pool-offset and
`switch`-vs-`if` findings are lessons, also not queueable. The unclaimed gap 0x801F9190..0x801FEEF0
is real work someone could do, but it is ~24 KB of unknown code and not one bounded unit, so
filing it would be a guess about scope.

WALL: __ct__18CFirstPersonCameraFRC9TUniqueIdRC12CTransform4f9TUniqueIdfffffii 99.32% - the last 4 bytes are retail's `addi r4,r9,19` into the merged .rodata string-pool label `lbl_803AA5B0`, which no unit claims, so our own pool puts the literal at offset 0 and the compiler emits `mr r4,r0`.
WALL: UpdateElevation__18CFirstPersonCameraFR13CStateManager 6.28% - its last callee `fn_801FB6CC` (0x801FB6CC, 0x358 bytes) is inside the unclaimed .text gap 0x801F9190..0x801FEEF0, so nothing in the link defines it and the call cannot be written.
