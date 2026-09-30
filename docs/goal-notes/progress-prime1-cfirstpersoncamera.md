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

---

# Run 2 (lane 6, 2026-09-30)

## Result

`build/report.json`, `main/MetroidPrime/Cameras/CFirstPersonCamera`:

| measure | before | after |
|---|---|---|
| `matched_functions` | **12 / 17** | **13 / 17** |
| `fuzzy_match_percent` | 11.887345 | 23.337209 |
| `matched_code` | 484 / 8060 (6.004963 %) | 672 / 8060 (8.337469 %) |

Whole-project `matched_functions` 10361 -> 10362, `linked` 5048 -> 5048. No function
anywhere got worse.

`tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS
progress-prime1-cfirstpersoncamera`**, every sub-check `ok`, `target rose: 12 -> 13 / 17`.

## The diff (two files)

`src/MetroidPrime/Cameras/CFirstPersonCamera.cpp` and `include/MetroidPrime/Player/CPlayer.hpp`.

1. **`UpdateElevation` written: 6.28 % -> 100 %**. This is the +1 function. The previous run
   recorded it as a hard wall; it is not one, and the notes' own reason was wrong (below).
2. **`Think` written: 0.52 % -> 97.79 %.** Two instructions of register allocation and one
   dead branch short of 100 %; it does not count as matched and is not claimed.
3. Four inline accessors and one enumerator added to `CPlayer.hpp` (see "Header changes").
   No layout change: `CHECK_SIZEOF(CPlayer, ...)` still passes and no member moved.

## The previous run's `UpdateElevation` wall was wrong

It claimed `fn_801FB6CC` "sits in an unclaimed gap, nothing in the link defines that address,
so no C++ can call it". The gap claim is **re-measured and still true** - the function is at
0x801FB6CC, `splits.txt` has no unit covering it (prev 0x801F9050-0x801F9190
`CUnknown90.cpp`, next 0x801FEEF0 `Carve801FEEF0.c`, `hit []`). But the conclusion does not
follow, for a reason the previous run did not test:

**An unlinked object may reference an undefined symbol.** `ninja` only links
`build/G2ME01/src/*.o` for units marked `Matching` in `configure.py`; everything else stays
retail. This unit is `NonMatching`, so `CFirstPersonCamera.o` is never in the link, so
`fn_801FB6CC` never has to resolve. Measured: the full build links and
`build/G2ME01/main.dol` is still `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. **A callee in an
unclaimed gap blocks a `Matching` flip, not a `progress` item.** Worth re-checking before
declaring any other gap-callee a wall.

## What retail's `UpdateElevation` actually does

Decoded in full (`.text 0x801B0220`, 188 bytes, 47 instructions, reproduced exactly):

```
mPitch = 0.f;
if (CameraManager(mgr).IsInCinematicCamera()) return;
const CPlayer* player = TCastToConstPtr<CPlayer>(mgr.GetObjectById(GetWatchedObject()));
if (!player) return;
if (mPitchId == kInvalidUniqueId) return;
const CUnknown42* vol = TCastToConstPtr<CUnknown42>(mgr.GetObjectById(mPitchId));
if (!vol) return;
mPitch = 0.0174532924f * fn_801FB6CC(vol, player->GetTransform());
```

Four details the previous run got wrong or missed, each worth one or more instructions:

- **The callee takes two arguments.** Retail `addi r4,r31,36; bl fn_801FB6CC` with the volume
  pointer still live in `r3` from the cast. Declaring it one-argument puts the transform in
  `r3` and emits `addi r3,r31,36` - wrong register, 97.55 %. Signature
  `float fn_801FB6CC(const CUnknown42*, const CTransform4f&)`. **`r31+36` is
  `CActor::mTransform` (0x24), so the second argument is a `CTransform4f&`, not a void\*.**
- **The volume cast is not optional.** My first version null-checked `mPitchId` and then called
  straight through; it compiled and linked, and was *wrong* - it dropped the second
  `GetObjectById` + `TCastToPtr<CUnknown42>` + null test (14 instructions) and only read
  78.47 %. The compiler was happy; only the disassembly caught it. **A green build and a
  plausible body are not the same as retail's body** - the clearest instance of
  `docs/PROCESS_LESSONS.md` in this item.
- **`GetObjectById` is the const overload** (`CF9TUniqueId`), not `ObjectById`; the manager
  argument is non-const so the const method needs a `const CEntity*`, and
  `TCastToConstPtr` (not `TCastToPtr`) is the only overload that takes one.
- **The pitch-id test is a separate `if`, not `||`.** `if (a == null || b == kInvalid)`
  compiles to `bne`+`b`; retail emits a single `beq`, which is what nested `if`s give
  (97.55 % -> 100 % on this one line).

`CUnknown42` is forward-declared in the .cpp (`class CUnknown42;`). It is already
castable - `src/MetroidPrime/TypesMatch.cpp:345` has `TYPES_MATCH_CLASS(CUnknown42, CActor)`
and `:790` `CAST_TO_IMPL(CUnknown42, 42)` - so no header is needed and none was added.

## `Think`: 0.52 % -> 97.79 %, and why it stops

Written in full from retail (`0x801AEA6C`, 768 B). Prime 1's `Think` is the right shape but
Echoes adds the health gate, the fluid tick, the morphball path, the pitch/close-in timer
countdowns and the turret block, so it could not be adapted - it was decoded instead. The
previous run's reading of the two bit flags (bit 24 = `mDeferBallTransitionProcessing`,
bit 25 = `mFluidEffectsPending`) is **confirmed** here.

Spellings that did **not** reach 100 %, all measured this run, so a later attempt skips them:

| spelling | % |
|---|---|
| `if (player != nullptr && hp > 0.f)` | 95.44 |
| same, `!(hp <= 0.f)` | 97.11 |
| `if (player) { if (hp > 0.f) { ... } }` nested | 97.11 |
| `hp` hoisted to a named float | 96.04 |
| early-return guard `if (player == null \|\| hp <= 0.f) return;` | 96.28 |
| morph factor as `float morph = 0.f; if (dur != 0.f) morph = t/dur;` then Clamp | 97.63 |
| morph factor inline in `close_enough(CMath::Clamp(0.f, kZero == dur ? kZero : t/dur, 1.f), 1.f)` | 97.63 |
| turret blend with a named `turretPos` / `delta` local | 97.63 |
| **`if (player && !(hp <= 0.f))` + turret blend as one expression on `xf`** | **97.79** |

The 2.21 % that is left is exactly three things, verified instruction-by-instruction:

1. **`Think` needs retail's redundant `beq` at 0x801AEABC** (the second of a
   `mr. r31,r3; beq END; beq BODY` pair). 28 sites in `.text` have this shape, all in
   destructors and `&&` chains, so it is a codegen pattern for a short-circuit whose
   "then" block needs no fallthrough - I could not find a spelling that produces it here.
   Same shape, no effect on the score either way.
2. **The morph factor's divide-by-zero guard wants `lfs f2` + `fcmpu cr0,f2,f1`**, ours emits
   `lfs f0` + `fcmpu cr0,f0,f1` and reloads `0.f` into `f2` afterwards. Pure register choice
   for the identical expression; nine spellings tried (table above plus `dur` hoisted,
   `!=`/`==` on both sides, literal vs named `0.f`, single `Clamp` vs split assignment).
3. **One `fmr f1,f31` before the `ValidateCameraTransform` call** (retail 0x801AEC6C) that our
   build does not emit - `dt` is already in `f1` there, so it is a scheduling artefact.

None of the three is a logic difference, but **97.79 % is not 100 % and does not count as a
matched function.** I am not claiming it.

## Header changes (`CPlayer.hpp`)

- `kCS_Cinematic` added to `EPlayerCameraState` as value **5**. Measured, not guessed:
  `SetCameraState__7CPlayer` (0x80016428) has a comparison tree ending
  `cmpwi r4,5; beq 0x80016578`, and that case calls `GetCurrentCamera(mgr, true)` +
  `TCastToPtr<CCinematicCamera>` - so 5 is the cinematic case. Every other call site in
  `.text` passes 0, 2 or 4 (`li r4,0/2/4` before `bl 80016428`), so **4 is still
  `kCS_Spawned`** and the previous run's "the enum has an un-named 6th value" was right about
  the value and wrong to leave it unresolved. `mCameraState` is at 0x388 (`lwz 904(r31)`).
- `GetCameraState()`, `GetMorphTime()`, `GetMorphDuration()` (0x1138/0x113C - the pair
  `UpdateMorphBallState` divides, and `SkipMorphTransition` sets the numerator to 1.0f),
  `GetTurretTimer()` (0x1304). All inline, all named from the offsets the disassembly shows.

## Gates

```
$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   build/G2ME01/main.dol   (expected value)

$ ./tools/probe_sources.sh
probe: 752 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)

$ python3 tools/check_symbol_names.py
checked 505 units; 0 declared names are missing from their object

$ python3 tools/check_decl_order.py --unit MetroidPrime/Cameras/CFirstPersonCamera
ok: 1 unit(s) checked, none emits its functions out of retail order

$ ./tools/goal_check.sh build/goal/item.json
goal_check: PASS progress-prime1-cfirstpersoncamera
  ok  no judge-owned path touched / gate.sh / counts: matched 10361 -> 10362  linked 5048 -> 5048
  ok  check_symbol_names.py / All: 31.50% fuzzy, 23.93% matched, 11.83% linked (10362 / 28465)
  ok  target rose: main/MetroidPrime/Cameras/CFirstPersonCamera: 12 -> 13 / 17 functions
  ok  no asm added
```

`gate.sh` rewrote two derived numbers in `docs/HANDOFF.md` (10361->10362, DOL units
8813->8814); I reverted that file so the diff is the source change only - the judge rewrites
those from the tree anyway.

`git status` at the end: `M include/MetroidPrime/Player/CPlayer.hpp` and
`M src/MetroidPrime/Cameras/CFirstPersonCamera.cpp`, nothing else.

## Still open, for a later item

- `Think` 97.79 % -> the three items above. 2.21 %, all register/scheduling.
- `__ct__` 99.32 % - the previous run's 4-byte `.rodata` string-pool wall
  (`addi r4,r9,19` into `lbl_803AA5B0`, claimed by no unit). **Not re-measured this run**;
  the reasoning about unclaimed labels is now suspect for the same reason `fn_801FB6CC`
  was - a label in an unclaimed `.rodata` range may still be *referenced* by an unlinked
  object. Worth one experiment.
- `UpdateTransform` 0.08 % (5132 B) and `UpdateFluidEffects` 0.90 % (1040 B) not attempted;
  both are large. `UpdateFluidEffects`'s three helpers (`fn_800EDFE4`, `fn_800EDFA0`,
  `fn_800EDCB4`) are all named and all claimed, so it is reachable - the previous run's
  assessment stands.

## Notes / no NEW items

No `NEW:` line. The `fn_801FB6CC` finding is a *correction* to a wall already recorded here,
not new work, and the enum resolution is now in the header. Filing either would be a
restatement of this item.

WALL: Think__18CFirstPersonCameraFfR13CStateManager 97.79% - the last 2.21% is a redundant
`beq` short-circuit branch at 0x801AEABC, `f2` vs `f0` register choice for the morph factor's
divide-by-zero guard, and one `fmr f1,f31` before the `ValidateCameraTransform` call; all
register/scheduling, and nine source spellings of the two code regions are tabulated above.

---

# Run 3 (lane 3, 2026-10-01)

## Result

`build/report.json`, `main/MetroidPrime/Cameras/CFirstPersonCamera`:

Before = `build/goal/judge/report.base.json`, after = `build/report.json`, both read after the
change (the judge's own baseline is the authoritative "before"):

| measure | before | after |
|---|---|---|
| `matched_functions` | **13 / 17** (76.47059 %) | **14 / 17** (82.35294 %) |
| `fuzzy_match_percent` | 23.341438 | 23.490818 |
| `matched_code` | 672 / 8060 (8.337469 %) | **1120 / 8060 (13.8957815 %)** |
| `matched_data` | 144 / 144 (100 %) | 144 / 144 (100 %) |

`matched_code` rises by exactly **448** = the ctor's whole size: at 99.32 % it contributed
nothing, at 100 % it contributes all of it. `Think` at 98.96 % still contributes nothing, so
its improvement shows up only in `fuzzy_match_percent`.

**Re-measure first, as the brief requires: on this tree the unit was at 13 / 17, not the 14
run 2 recorded.** `__ct__` was 99.32 % here, so run 2's work was present but its ctor fix was
not. Treat the "before" column in run 2's table as run 2's own baseline, not this tree's.

Whole-project `matched_functions` 11343 -> 11344, `linked` 5507 -> 5507, `complete_units` and
the DOL/REL figures unchanged. No function anywhere got worse.

`./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS
progress-prime1-cfirstpersoncamera`**, every sub-check `ok`, `target rose: 13 -> 14 / 17`.

## The diff (one file, two hunks)

`src/MetroidPrime/Cameras/CFirstPersonCamera.cpp` only.

1. **The ctor's `.rodata` pool label: 99.32 % -> 100 %.** This is the +1 function and it
   retires run 1's `WALL:` on the ctor, which was wrong for the same reason run 2's
   `fn_801FB6CC` wall was wrong (an unlinked object may reference an undefined symbol).
2. **`Think`: 97.79 % -> 98.96 %.** Two independent codegen fixes, below. Not 100 %, not
   claimed as matched; kept because it is more matched code with no regression.

## The ctor: `rstl::string_l(lbl_803AA5B0 + 19)`, not `rstl::string_l("First Person Camera")`

Run 1 called the last 4 bytes a hard wall on the grounds that `lbl_803AA5B0` is a `.rodata`
range no unit claims, so `extern const char lbl_803AA5B0[]` would not resolve. **It resolves
fine, because this unit is `NonMatching`, so `CFirstPersonCamera.o` is never in the link and
the symbol never has to be defined.** That is run 2's `fn_801FB6CC` finding, applied here.
The build links and `main.dol` is still `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

The bytes, re-measured this run:

```
$ build/binutils/powerpc-eabi-objdump -s --start-address=0x803AA5B0 ... main.elf
 803aa5b0 3f3f283f 3f290057 61746572 53686565  ??(??).WaterShee
 803aa5c0 74730046 69727374 20506572 736f6e20  ts.First Person
 803aa5d0 43616d65 72610000                    Camera..
```

`symbols.txt` gives `lbl_803AA5B0 = .rodata:0x803AA5B0; size:0x28`, so `+ 19` is exactly the
`F` of the literal and the `0x13` is its length. Retail materialises the base and adds the
offset (`lis r9,hi(lbl); addi r4,r9,19`); a local literal makes our own pool start at offset 0
and MWCC emits `mr r4,r0`. `lbl_803AA5B0 + 19` is **the same 19 bytes**, so this is not a
pointer past a real string - it is the address of the string inside retail's merged pool.
This is the repo's existing convention: `CConsoleOutputWindowCtor.cpp` does
`rstl::string_l(lbl_803A89E8)` and `CMainShutdownSubsystems.cpp` does
`rstl::string_l(lbl_803A56C0 + 0xCB)` for the same reason. `check_symbol_names.py` only looks
at names *inside a unit's declared `.text` ranges*, so a new `extern "C"` data reference does
not trip it (measured: 514 units, 0 missing).

Side effect worth knowing: **our object no longer emits a `.rodata` section at all**
(`unit_fit.sh` listed `.rodata 20 bytes NOT CLAIMED` on the clean tree, nothing after). We now
reference retail's literal instead of duplicating it.

**Grep every `rstl::string("literal")` in a ctor initialiser list, and every
`rstl::string_l("literal")` whose literal is not at offset 0 of its own pool, against
`symbols.txt`'s `.rodata` string labels.** Where a pool label exists, the reference form
reproduces retail; the literal form cannot.

## `Think` 97.79 % -> 98.96 %: two fixes, both measured, both in the disassembly

### Fix A - the HP test is an early-return **guard**, not a wrapped body (+0.57 %)

Run 2 recorded the shape as `if (player && !(hp <= 0.f)) { ...body... }` and tabulated nine
spellings around it, none reaching 100 %. The clue is that retail 0x801AEAAC..0x801AEAE4 is
**four** branches, not two:

```
801aeab4  mr.  r31,r3
801aeab8  beq  END          ; player == 0 -> epilogue
801aeabc  beq  BODY         ; DEAD: same condition, to the body
801aeac0  lwz  r12,0(r3)    ; GetHealthInfo()
   ...
801aeadc  cror eq,lt,eq
801aeae0  bne  BODY         ; hp test, true -> body
801aeae4  b    END          ; false -> epilogue
```

`if (player && !(hp <= 0.f)) { ... }` compiles that pair to a single `beq END` and the `bne`
to `beq END` - 190 instructions against retail's 192. Writing the early return as a **guard
clause** produces retail's four branches:

```cpp
if (player == nullptr || player->GetHealthInfo()->GetHP() <= 0.f) {
  return;
}
```

**97.79 % -> 98.36 %.** The lesson generalises: when retail emits both a `b<cond> then` *and*
a `b<else>` for one test, the then-block is the fall-through and the source is a guard, not a
wrapper. A wrapper lets MWCC invert the branch; a guard does not.

Spellings measured this run, all with the clamp fix below already in place:

| gate spelling | `Think` |
|---|---|
| `if (player == nullptr \|\| hp <= 0.f) return;` **(kept)** | **98.96** |
| same, `!(hp > 0.f)` | 98.41 |
| same, `!(hp <= 0.f)` | 98.33 |
| same, `0.f >= hp` | 97.71 |
| same, `!player \|\| hp <= 0.f` | 98.96 |
| same, `!(player != nullptr) \|\| hp <= 0.f` | 98.96 |
| same + trailing `return;` / blank line / `do{}while(false)` | 98.96 |
| `if (player) { if (!(hp <= 0.f)) { body } }` | 98.39 |
| the same, both arms with explicit `return;` | 98.39 |
| `if (player) { if (hp <= 0.f) return; body }` | 98.39 |
| `if (player) { if (hp > 0.f) { body } }` / `>= 0.f` | 97.86 / 98.39 |
| `if (player != nullptr) { if (!(hp <= 0.f)) { body } }` | 98.39 |
| guard + a second `if (player) { body }` | 97.92 |
| `hp` hoisted to a named float | 98.39 |
| `const CPlayer* player` + `TCastToConstPtr` | build failed (non-const methods below) |

`hp > 0.f` is *worse* than `!(hp <= 0.f)` everywhere, on both gate shapes (97.27 measured
separately on run 2's base). `hp <= 0.f` is the spelling retail used.

### Fix B - the clamp belongs **inside** the non-zero arm (+0.60 %)

Retail 0x801AEB78..0x801AEBB0 holds `0.f` in **f2** across the divide-by-zero guard *and* uses
f2 as the clamp's lower bound, and the `duration == 0` arm branches straight past the clamp
(`b 0x801AEBB4`) because it is already the minimum:

```
801aeb78  lfs    f2,0.0f        ; the guard's zero
801aeb7c  lfs    f1,4412(r31)   ; mMorphDuration
801aeb80  fcmpu  cr0,f2,f1
801aeb84  bne    0x801aeb8c
801aeb88  b      0x801aebb4      ; == 0 -> past the clamp, f2 already holds 0.f
801aeb8c  lfs    f0,4408(r31)   ; mMorphTime
801aeb90  fdivs  f0,f0,f1
801aeb94  fcmpo  cr0,f2,f0      ; clamp's min vs value, same f2
```

`Clamp(kZero, kZero == dur ? kZero : t/dur, 1.f)` - the clamp *around* the ternary - reloads
`0.f` into f2 (`lfs f2,@925` that retail has no instruction for) and puts the guard's zero in
f0. Moving the clamp inside the non-zero arm lets MWCC keep one register for the value:

```cpp
const float morph =
    kZero == player->GetMorphDuration()
        ? kZero
        : CMath::Clamp(kZero, player->GetMorphTime() / player->GetMorphDuration(), 1.f);
```

The morph region then matches retail **instruction for instruction**, f2 included.

| clamp spelling (on the fix-A guard) | `Think` |
|---|---|
| clamp in the non-zero arm, `kZero` named **(kept)** | **98.96** |
| clamp in the non-zero arm, literals | 98.96 |
| clamp around the ternary, `kZero` named | 98.36 |
| around the ternary, literals / named `kZero`+`kOne` | 98.36 |
| around the ternary, `!=` with the arms swapped | 97.81 |
| around the ternary, statement form `morph = 0.f; if (dur != 0.f) ...` then clamp | 97.81 |
| around the ternary, `0.f != dur` | 97.81 |
| statement form, clamp only inside the `if` | 97.71 |
| hand-written two-sided clamp, no `CMath::Clamp` | 96.43 / 96.64 |

**Lesson: `CMath::Clamp` around a conditional expression reloads its bound; the same clamp
inside the arm that can produce a non-bound value keeps it in one register.**

### What is left: two single dead instructions

Net -2 instructions against retail's 192; the score is 98.96 % (760 of 768 bytes' worth).
Both remaining are *dead* in retail:

1. **`beq BODY` at 0x801AEABC** - unreachable, same condition as the `beq END` before it,
   which is why it survives MWCC's peephole. 107 sites of the same adjacent-`beq`-pair shape
   exist in `.text` (measured: 11 in `CParticleDatabase`, 8 in `CCameraFilter`, 7 in
   `CMemoryCard`, 6 each in `CStateManager` and `CAutoMapper`, ...). The only one in a
   `Matching` unit is `CIOWinManager.cpp` `RemoveIOWin` (0x80049B0C, 0x80049B98), whose
   source is `if (prevNode == nullptr) mPumpRoot = node->GetNext(); else
   prevNode->SetNext(node->GetNext()); delete node;` - an **if/else whose then-arm jumps
   away**, which is what produces `beq <then>; beq <join>; beq <join>; <else body>`. So the
   idiom needs an if/**else** with a jumping then-arm, and every gate spelling I tried
   normalises to a two-arm structure the optimiser flattens. Fifteen gate spellings in the
   table above did not produce it.
2. **`fmr f1,f31` at 0x801AEC20**, between `bl UpdateTransform` and
   `bl ValidateCameraTransform`. Nothing between those two calls reads f1 - the next float
   user is `fsubs f0,f1,f31` for `mCloseInTimer`, which uses f31 directly. It is the reload
   of `dt` for `CActor::Think(dt, mgr)` placed immediately after the clobbering call instead
   of immediately before the use; ours places it at the use (0x801AED38's counterpart).
   Scheduling, not logic.

Both are 4 bytes of register/branch placement. I stopped here rather than keep guessing.

## Gates

```
$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  build/G2ME01/main.dol   (expected value)

$ ./tools/decomp_build.sh main/MetroidPrime/Cameras/CFirstPersonCamera
All:  32.62% fuzzy, 25.35% matched, 11.94% linked (11344 / 28465 functions)
main/MetroidPrime/Cameras/CFirstPersonCamera: 23.49% fuzzy, 13.90% matched (14 / 17 functions)
   UpdateTransform__18CFirstPersonCameraFR13CStateManagerf   0.08%  5132 bytes
   Think__18CFirstPersonCameraFfR13CStateManager         98.96%  768 bytes
   UpdateFluidEffects__18CFirstPersonCameraFR13CStateManager   0.90%  1040 bytes

$ ./tools/probe_sources.sh
probe: 751 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)

$ python3 tools/check_symbol_names.py
checked 514 units; 0 declared names are missing from their object

$ python3 tools/check_decl_order.py --unit MetroidPrime/Cameras/CFirstPersonCamera
ok: 1 unit(s) checked, none emits its functions out of retail order

$ ./tools/unit_fit.sh MetroidPrime/Cameras/CFirstPersonCamera.cpp
   .text      claimed   8060   ours   2020   retail   8060   SHORT by 6040
   .data      claimed    144   ours    140   retail    144   SHORT by 4
   .sdata     claimed      -   ours     40   <- NOT CLAIMED BY splits.txt
   .sdata2    claimed      -   ours     24   <- NOT CLAIMED BY splits.txt
   extra:    +   80  __dt__Q24rstl66basic_string<c>...Fv
   extra:    +   44  GetHealthInfo__6CActorCFv
```

The `.data` 4-byte shortfall and the two extra weak/COMDAT symbols are **pre-existing** -
measured on the clean tree before this run's edit, identical. The only `unit_fit` line that
changed is `.rodata`, which disappeared (see the ctor section). The unit stays `NonMatching`,
so none of this gates the item.

```
$ ./tools/goal_check.sh build/goal/item.json
goal_check: item progress-prime1-cfirstpersoncamera (progress) target=MetroidPrime/Cameras/CFirstPersonCamera
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11343 -> 11344   linked 5507 -> 5507
  ok    check_symbol_names.py
  ok    All:  32.62% fuzzy, 25.35% matched, 11.94% linked (11344 / 28465 functions)
  ok    target rose: main/MetroidPrime/Cameras/CFirstPersonCamera: 13 -> 14 / 17 functions
  ok    no asm added
goal_check: PASS progress-prime1-cfirstpersoncamera
```

`gate.sh` rewrote the derived numbers in `docs/HANDOFF.md`; I reverted that file so the diff
is the source change only - the judge rewrites those from the tree anyway. `git status` at
the end: `M src/MetroidPrime/Cameras/CFirstPersonCamera.cpp` and nothing else.

## Still open, for a later item

- **`UpdateFluidEffects` 0.90 %, 1040 B, not attempted - this is the next real target.** I
  decoded it in full this run (0x801AE598) but did not write it; it is a full sitting job.
  Every callee is named and claimed, so it is reachable. Structure, for whoever takes it:
  two near-identical halves, **enter** (morph state 3 `kMS_Unmorphing` **or** 1
  `kMS_Morphed`, i.e. `beq` into the same block from both tests) and **leave** (state 0
  `kMS_Unmorphed`), so it is one shared body entered from three tests, not two copies. Each
  half: `ObjectById(mPendingFluidId)` (`lhz 580(r3)` = 0x244) + `TCastToPtr<CScriptWater>`;
  bail if the cast is null **or** if `water->mX(0x274) == 0`; then
  `operator new(0x180, 0)`, three inlined `CToken` locals at `r1+100/112/124`
  (enter) and `r1+60/72/84` (leave) built by `__ct__6CTokenFRC6CToken` from
  `r29+0x27C` / `r29+0x268` - the *same* offsets the two halves use are `0x27C` and `0x268`,
  i.e. enter and leave differ in the offsets they read (`0x29E` vs `0x28C` for the sound id,
  `0x274` vs `0x26C` for the string), then `AllocateUniqueId`, `string_l(lbl_803AA5B0 + 7)`
  (= `"WaterSheets"`), `CColor::White()`, `fn_800EDFE4(mgr)` (returns a `CVector3f` from a
  lazily-initialised global at 0x803F6AC0), `fn_800EDFA0(mgr, this->field_0x1DC)`, then
  `fn_800EDCB4(newObj, &token, &token2, &uid, true, &name, vec, field_0x1DC)` - 8 args,
  `mgr.AddObject(r28)`, then the three `__dt__6CToken` calls each guarded by an
  `extsb. r<flag>` "was it constructed" bool (`r25/r26/r27`, all `li 1` on the success path,
  `li 0` on the bail path), then `lhz 0x28E/0x29C(r29)` for the sound id,
  `Player(mgr)`, `GetSoundPan(kMSP_4)` (`li r4,4`), and
  `CSfxManager::SfxStart(id, 127, pan, <const 0x8041E2F0>, false, false, <const 0x8041E2EC>)`
  followed by `GetPlayer(mgr)->ApplySubmergedPitchBend(handle)`. Finally
  `mPendingFluidId = kInvalidUniqueId` (`lhz -27740(r13)`).
  **The two `SfxStart` constant args are always loaded `lwz -16600(r2)` / `lha -16604(r2)` in
  all 155 call sites in the DOL** - they are 0x8041E2F0 and 0x8041E2EC, and this repo's
  `CSfxManager::kAllAreas = -1` / `kMedPriority = 127` are defined out of line in
  `CSfxManager.cpp`, so they compile to a symbol load, not an SDA2 small-data load. Matching
  that argument idiom needs a decision about which constants the header should expose; no
  `SfxStart` caller in the DOL is in a `Matching` unit, so there is no source to copy the
  spelling from. That is the one part of this function I would expect to need a header
  decision rather than a source spelling.
- **`UpdateTransform` 0.08 %, 5132 B**, not attempted; 5132 bytes of inlined quaternion and
  orbit composition, all callees named. A full sitting item.
- **`Think` 98.96 % -> 100 %** - the two dead instructions above.

## Notes / no NEW items

No `NEW:` line. The ctor fix is a correction to a wall already recorded in this file, not new
work. `UpdateFluidEffects` is the same unit this item already names, so queueing it would be
a restatement of this item. The `SfxStart` constant-argument idiom is a lesson (recorded
above), and the `RemoveIOWin` `beq`-pair source is a technique, not a unit.

WALL: Think__18CFirstPersonCameraFfR13CStateManager 98.96% - the last two instructions are
both dead in retail: the `beq 0x801AEABC` to the body under the same condition as the `beq`
before it, which needs an if/else with a jumping then-arm (the one known source is
`CIOWinManager.cpp`'s `RemoveIOWin`) and which none of the 15 gate spellings in the table
above produces; and the `fmr f1,f31` at 0x801AEC20, which re-materialises `dt` right after
`bl UpdateTransform` rather than at `CActor::Think` where ours puts it. Both are branch
placement and register scheduling, not logic.
