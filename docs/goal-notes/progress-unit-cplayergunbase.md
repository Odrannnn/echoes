# progress-unit-cplayergunbase — `MetroidPrime/Player/CPlayerGunBase.cpp`

Lane 4, worktree `../wt-mp2-goal-L4`, HEAD `feda13f1`. Unit stays `NonMatching`; no `flip_test` was
run to decide anything. `tools/goal_check.sh build/goal/item.json` with
`MP_GOAL_BASE=$PWD/build/report.base.json`: **PASS**, `target rose: 12 -> 17 / 21 functions`.

## Measured

Baseline `build/report.base.json` (recorded on this HEAD by `tools/gate.sh --baseline`, per
`build/goal/judge/record-gate.log`) vs `build/report.json` after:

| | base | now |
|---|---|---|
| unit `matched_functions` | 12 / 21 | **17 / 21** |
| unit `matched_code` | 1192 | 1912 |
| unit `fuzzy_match_percent` | 31.845785 | 46.168583 |
| DOL `matched_functions` | 12332 | 12337 |
| DOL `fuzzy_match_percent` | 34.80824 | 34.81739 |
| `All:` line | 34.81% / 28.40% / 12.90% | 34.82% / 28.41% / 12.90% |

`linked` 5863 -> 5863 (unchanged, correct for a unit that did not flip). No function anywhere got
worse (`gate.sh`'s `report_diff.py` step is green).

Per function, before -> after:

| function | bytes | before | after |
|---|---|---|---|
| `fn_801DDF18` | 12 | 0.00 | **100.0** |
| `fn_801DDF0C` | 12 | 0.00 | **100.0** |
| `HolsterGun__14CPlayerGunBaseFR13CStateManager` | 184 | 2.17 | **100.0** |
| `Update__14CPlayerGunBaseFfR13CStateManager` | 188 | 45.87 | **100.0** |
| `AcceptScriptMsg__14CPlayerGunBaseFR13CStateManagerRC10CScriptMsg` | 324 | 9.77 | **100.0** |

Left, all still at their floor of 4 matching bytes (an empty body): `ProcessInput` 404 B 0.99%,
`CreateGunLight` 276 B 1.45%, `UpdateGunHolster` 784 B 0.51%, `UpdateTransform` 800 B 0.50%.

Files touched: `src/MetroidPrime/Player/CPlayerGunBase.cpp` and
`include/MetroidPrime/CEntityInfo.hpp` (three `EScriptObjectMessage` values added — enum values are
compile-time constants, so codegen-neutral; the gate's per-function diff is green). Nothing in
`tools/`, `docs/` or `build/goal/` was edited. No asm.

## 1. `fn_801DDF18` / `fn_801DDF0C` — 0.00% -> 100.0% (12 B each)

`./tools/dis.sh 0x801DDF0C 0x18`:

```
fn_801DDF0C:  xor r0,r3,r4 ; and r3,r4,r0 ; blr      ->  b & ~a
fn_801DDF18:  xor r0,r3,r4 ; and r3,r3,r0 ; blr      ->  a & ~a
```

`ProcessInput` (retail 0x801DE29C) is their only caller, at 0x801DE40C and 0x801DE3FC, passing
`(mLastInputFlags, mInputFlags)`; the results go to `mPressedInputFlags` (916... measured: 920) and
`mReleasedInputFlags` (916). So they are the pressed/released edge masks.

Two things had to be right, and neither is guessable:

- **`a & ~b` does not produce retail's code.** MWCC 2.7 lowers `& ~` to a single `andc` — 8 bytes,
  measured: `andc r3,r3,r4 ; blr`. Retail's leaves are 12. Spelling the complement as
  `a & (a ^ b)` is what keeps the `xor`, and it is byte-identical for both helpers.
  12 signature/body spellings measured (`.tmp/opencode/battery.py`, `tools/try_batch.py`):
  `uint`/`int`/`long`/`ushort`/`unsigned long` all give `andc`; `(a^b)&a` and `a&(a^b)` give retail's
  three instructions; `a^(a&b)` gives `and`+`xor` (wrong order); `a|~(a|b)` gives `or`+`orc`.
- **`extern "C"` is required, not decoration.** dtk's target object
  `build/G2ME01/obj/MetroidPrime/Player/CPlayerGunBase.o` carries them as *global* symbols named
  exactly `fn_801DDF0C` / `fn_801DDF18` (`nm` shows `T`), because retail has no name for them and
  dtk synthesises one from the address. objdiff pairs a target function with the built function of
  the same symbol name, so a `static` C++ definition — mangled `fn_801DDF0C__FUiUi` — sits at 0%
  however exact its bytes. Measured: `static` + `& ~` -> 0.00%, `static` + `(a^b)&a` -> 0.00%,
  `extern "C"` + `& ~` (8-byte `andc`) -> 46.67%, `extern "C"` + `(a^b)&a` -> 100.0%. This is the
  arrangement `include/MetroidPrime/CCameraManager.hpp:111-128` already documents for
  `fn_801AB298`/`fn_801AAE20`.

Declarations sit between the two `GetWorldShadow` overloads and `Holster`, because mwcceppc emits
definitions in reverse source order. **They are still uncalled**: see blocker 1.

## 2. `HolsterGun` — 2.17% -> 100.0% (184 B)

Retail 0x801DDE14. Body:

```cpp
if (mGunHolsterState == kGHS_Holstered || mGunHolsterState == kGHS_Holstering) return;
CPlayer* player = GetPlayerFromAll(mgr);
float holsterTime = gpTweakPlayerGun->GetGunHolsterTime();
if (player->GetMorphballTransitionState() == CPlayer::kMS_Morphing) holsterTime = 0.1f;
if (mGunHolsterState == kGHS_Drawing)
  mGunHolsterRemTime = holsterTime * (1.f - mGunHolsterRemTime / 0.45f);
else
  mGunHolsterRemTime = holsterTime;
mGunHolsterState = kGHS_Holstering;
player->SetAimTarget(kInvalidUniqueId);
```

- **One `if` with `||`, not two `if`s and not a `switch`.** Retail is
  `lwz 936(r3) / cmpwi 0 / beq <end> / cmpwi 3 / bne <body> / b <end>` with the state word loaded
  once. `tools/try_batch.py`, differing instructions: `||`-if **0**, two `if`s 2, nested `!=` 2,
  `switch` with `default` body 6, `switch` on the two live cases 5.
- Constants read from `.sdata2` through `r2 - offset`, resolved against `SDA2 = 0x804223C0`
  (`tools/sda.py` only handles r13; the r2 pairs are 0x8041D350 = 0.1f, 0x8041D340 = 0.45f,
  0x8041D344 = 1.0f). `0.8041D340`..`0x8041D358` is unclaimed by `splits.txt`, i.e. this unit's own.
- `CPlayer + 0x38C` is `mMorphBallState`; the comparison is against 2, so `kMS_Morphing` under this
  header's enum order. **This is the first thing a next run should re-check**: the enum's internal
  order (Unmorphed/Morphed/Morphing/Unmorphing) is not independently pinned anywhere I could find —
  the bytes prove the value is 2, the *name* comes from the header's existing order.

## 3. `Update` — 45.87% -> 100.0% (188 B)

Retail 0x801DE1E0. Three findings:

- `mUnderwater` is set from the **first-person camera's fluid list**, not from anything the player
  owns: `bl GetPlayer` / `lwz r4,4888(r3)` = `CPlayer::mCameraManager` (0x1318, header-pinned) /
  `lwz r4,24(r4)` = `CCameraManager::mFpCamera` (0x18) / `lwz r5,272(r4)` = `CActor::mFluidIds`
  (0x110). The `neg`/`or` pair is the `!= 0` bool conversion, and `CCameraManager`'s 0x18/0x16
  offsets are pinned by `IsInCinematicCamera` (`lhz 22(r3)`) and `GetCurrentCameraId`
  (`lhz 20(r3)`).
  **0x110 is `mCount`, not the data pointer** — `rstl::reserved_vector` in this tree is
  `{int mCount; uchar mData[N];}`, measured with `tools/probe_offsets.cpp`'s method (see
  `.tmp/opencode/probe110.cpp`: `mFluidIds` 0x110, `mPreviousFluidIds` 0x11C, `sizeof(CActor)`
  0x158). So the test is `CActor::IsInFluid()`. Getting this wrong first cost one iteration:
  spelling it as `GetFluidList().data() != nullptr` emits an out-of-line `bl GetFluidList` plus an
  `addi r5,r3,4` and lands at **75.21%**.
- The bitfield store is `rlwimi r3,r4,8,24,24` = the *write* form with `mb = 24`, which the header's
  own table (`include/MetroidPrime/Player/CPlayerGunBase.hpp:54-69`) maps to `mUnderwater`, index 0.
- `rstl::single_ptr::operator->` in this tree does **not** null-check, so retail's
  `lwz r3,144(r30) / cmplwi r3,0 / beq` has to be written out: `if (mRainSplashGenerator.get() != nullptr)`.

`const_cast< CCameraManager* >` is needed because `CPlayer::GetCameraManager()` is a const accessor
returning `const CCameraManager*` and `FirstPersonCamera()` is not; the cast is invisible in the
object (same idiom as `src/MetroidPrime/Player/CPlayerGun.cpp`'s `TouchModel`).

## 4. `AcceptScriptMsg` — 9.77% -> 100.0% (324 B)

Retail 0x801DE09C. The eight message codes come straight out of the comparison tree's `lis`/`addi`
pairs and are, in ascending order, the ones now added to `EScriptObjectMessage` plus the three
already in the header:

| code | name | arm (retail address) |
|---|---|---|
| 0x58435254 | `kSM_XCRT` (existing) | 0x801DE160: `mSoundVolume = player->GetSoundPan(CPlayer::kMSP_3); CreateGunLight(mgr);` |
| 0x5844454C | `kSM_XDelete` (existing) | 0x801DE17C: `DeleteGunLight(mgr);` |
| 0x58454E46 | `kSM_XENF` (existing) | none — falls to `CEntity::AcceptScriptMsg` |
| 0x5845505A | **`kSM_XEPZ` (added)** | 0x801DE18C: `mInPhazonPool = true;` |
| 0x58455846 | `kSM_XEXF` (existing) | none |
| 0x58494E46 | `kSM_XINF` (existing) | none |
| 0x5849505A | **`kSM_XIPZ` (added)** | 0x801DE18C, **same arm as XEPZ** |
| 0x5858505A | **`kSM_XXPZ` (added)** | 0x801DE1A0: `mInPhazonPool = false;` |

Things that had to be measured:

- **The message is read before `GetPlayer`.** Retail does `lwz r28,8(r5)` then `bl GetPlayer`, with
  r28 spilled at 16(r1), so the source pulls `msg.GetMessage()` into a named value first. With
  `switch (msg.GetMessage())` after `CPlayer* player = GetPlayer(mgr);` the load happens after the
  call, into r5, and the function sits at **71.85%**.
- **Arms are emitted in source order.** Retail's arms are XCRT, XDelete, XEPZ, XXPZ in that order,
  so the source lists them in that order (mine first had XDelete first: 99.94% with all bytes
  identical except one `beq` displacement).
- **`kSM_XIPZ` shares the XEPZ arm.** That one `beq` was the last 0.06%: with XIPZ falling to
  `default`, objdiff reported **99.93827%** on a function whose instruction stream and five
  relocations were otherwise byte-identical to retail's (verified instruction-by-instruction, and by
  the `.rela.text` list). Retail's `beq` at +0x9c targets `+0xf0` (the XEPZ arm), not `+0x114`.
  Five spellings of the no-op group (grouped-before-`default`, `default` first, each with its own
  `break`, no `default`, interleaved order) all reported exactly 99.93827% — the number is real, not
  an artefact of a variant.
- The four no-op labels (`XENF`, `XEXF`, `XINF` and, before the last fix, `XIPZ`) fall through to
  the shared `CEntity::AcceptScriptMsg(mgr, msg)` tail, which the source writes after the switch.

## Blockers for the four functions still at the floor

**1. `ProcessInput` (404 B) needs three symbols that are unmangled free functions retail, so this
unit can only *call* them, and calling them needs definitions that do not exist in the tree.**
`config/G2ME01/symbols.txt`:

```
9857: fn_8022A5B4 = .text:0x8022A5B4; // type:function size:0x8C
9881: fn_8022b7f4__7CPlayerCFRC11CFinalInput = .text:0x8022B7F4; // type:function size:0x80
9884: fn_8022b974__7CPlayerCFRC11CFinalInput = .text:0x8022B974; // type:function size:0x4C
```

`fn_8022A5B4` has no class mangling and is called on `GetControlHintManager()`'s result
(`mr r3,r31 / bl GetControlHintManager / mr r5,mgr / li r4,1 / bl fn_8022A5B4`), so it is a free
function taking `CHintManager*`; the other two are called as `(player, input)`. Declaring them is not
enough: `src/MetroidPrime/Player/CPlayerGunBase.cpp` is in `files.cmake`, so `tools/probe_sources.sh`
compiles and **strictly links** it for the host, and a declared-but-undefined `extern "C"` would fail
that link and with it the gate's `port probe` step. Writing the three bodies (0x8C + 0x80 + 0x4C =
436 bytes, in units this item does not own) is a separate piece of work.
Everything *else* in `ProcessInput` is decoded and cheap, for whoever takes it on: the two helper
calls at the tail are `mReleasedInputFlags = fn_801DDF18(mLastInputFlags, mInputFlags)` and
`mPressedInputFlags = fn_801DDF0C(...)` then `mLastInputFlags = mInputFlags`; the gate is
`r28 = mInBigStrike && !(mMorphBallState == 1)` and `r27 = player->GetFrozenState() &&
!(mMorphBallState == 1)`, cleared (all four of 908/912/916/920) unless
`fn_8022A5B4(hintMgr, 1, mgr)` returns true; otherwise `mInputFlags` is built from
`FireBeamHeld` | 4 if `fn_8022B7F4` | 2 if `GetDigitalInput(player + 0x13D0, 18, input, 0)` |
8 if `fn_8022B974`.

**2. `CreateGunLight` (276 B) needs a `CEntityInfo` global and a string literal at retail addresses
this unit does not own.** `./tools/dis.sh 0x801DDF88 0x114`: it does
`mgr.AllocateUniqueId()`, stores the result to `mLightId` (0x37A), `new CGameLight` (440 bytes), and
passes `lis r3,-32709 / addi r4,r3,-21448` = **0x803BAC30** as the `const CEntityInfo*`, plus
`addi r4,r4,7` = 0x803BAC37 as an `rstl::string_l`. Both addresses are inside
`Kyoto/Particles/CVectorElement.cpp`'s claimed `.data` (`splits.txt`: `.data start:0x803babe8
end:0x803baeb8`), i.e. the `CGameLight`'s entity info belongs to another unit's data layout, and
this tree's `CGameLight(TUniqueId, TAreaId, bool, const rstl::string&, const CTransform4f&,
TUniqueId, const CLight&, uint, uint, float, const CEntityInfo* info = nullptr)` defaults `info` to
null while retail passes a real one. Making it match needs that global to exist at the right address,
which is a data-layout question about CVectorElement, not a body of this function.

**3. `UpdateGunHolster` (784 B) and `UpdateTransform` (800 B)** are not blocked, just large and
untouched. `UpdateTransform`'s first 12 relocations in retail's object (`GetFixedVerticalAim`,
`AxisAngle`, `CTransform4f::GetRotation`, `CQuaternion::BuildTransform4f`, `__ml__`,
`CTransform4f::operator=`) say it is a quaternion/assist-aim computation; `UpdateGunHolster` resolves
scan/cinematic/input transitions. No attempt was made on either.

No `NEW:` lines: the remaining work is in this same unit, so it would be a restatement of the item.

## Verification run by this lane

```
./tools/decomp_build.sh Player/CPlayerGunBase        # All: 34.82% fuzzy, 28.41% matched, 12337/28465
python3 tools/check_decl_order.py                    # ok: 981 unit(s) checked, 29 permuted, all accounted for
python3 tools/check_symbol_names.py                  # checked 525 units; 0 declared names missing
./tools/unit_fit.sh MetroidPrime/Player/CPlayerGunBase.cpp
MP_GOAL_BASE=$PWD/build/report.base.json ./tools/goal_check.sh build/goal/item.json
#   ok gate.sh / counts 12332 -> 12337, linked 5863 -> 5863 / target rose 12 -> 17 / no asm
#   goal_check: PASS progress-unit-cplayergunbase
```

`unit_fit.sh` reports six functions present in our object but not in retail's (568 bytes): six COMDAT
weak template/inline destructors out of `__ct__`/`__dt__`
(`__dt__rstl::vector<CRainSplashGenerator::SRainSplash>`, two `destroy*` template instantiations,
`__dt__rstl::single_ptr<CWorldShadow>`, `__dt__rstl::single_ptr<CRainSplashGenerator>`,
`__dt__CRainSplashGenerator`). They are pre-existing — nothing in this item emits them — and
`unit_fit.sh`'s own text says they are the harmless kind CAi carries and still flips.

Scratch scripts used and left in `.tmp/opencode/` (gitignored): `battery.py` (the 12 leaf-helper
spellings), `probe110.cpp` + `probe_cc2.sh` (the `mFluidIds` offset probe), `asm_variants.py` (the
five no-op-group spellings), `var_holster.py` / `var_asm.py` / `var_a.py` / `var_b.py` (try_batch
variant files). Note `tools/try_batch.py` matches built functions by *exact* symbol name, so it is
useless for the two `fn_801DDF*` helpers (their built names carry a `__FUiUi` suffix); compare those
two by disassembly instead.

---

# progress-unit-cplayergunbase — lane 8, second attempt

Worktree `../wt-mp2-goal-L8`, branch `goal/lane-8`, HEAD `db86766b`. Unit stays `NonMatching`; no
`flip_test.sh` was run. `MP_GOAL_BASE=$PWD/build/report.base.json ./tools/goal_check.sh
build/goal/item.json`: **PASS**, `target rose: 17 -> 19 / 21 functions`.

Lane 4's notes above are confirmed on this tree (17/21 at HEAD `db86766b`, not 12 as on their
base), so nothing below re-does their work. The two functions they listed as "large and untouched"
both went to 100% this run, and the one they called blocked was not — see (2).

## Measured

| | base (`build/report.base.json`) | now |
|---|---|---|
| unit `matched_functions` | 17 / 21 | **19 / 21** |
| unit `matched_code` | 1912 | 3496 |
| unit `fuzzy_match_percent` | 46.168583 | 83.90804 |
| DOL `matched_functions` | 12360 | 12362 |
| `All:` line | 34.92% / 28.54% / 12.90% | 34.94% / 28.56% / 12.90% |

`linked` 5863 -> 5863 (correct for a unit that did not flip). `gate.sh`'s `report_diff.py` step is
green, so no function anywhere got worse.

| function | bytes | before | after |
|---|---|---|---|
| `UpdateGunHolster__14CPlayerGunBaseFRC11CFinalInputR13CStateManager` | 784 | 0.51 | **100.0** |
| `UpdateTransform__14CPlayerGunBaseFR13CStateManagerRC9CVector3fRC12CTransform4fR12CTransform4f` | 800 | 0.50 | **100.0** |

Both were verified instruction-by-instruction against retail's object (`.tmp/opencode/sbs.py`:
`differing instrs: 0`, 196/196 and 200/200), not just on objdiff's percentage.

Files touched: `src/MetroidPrime/Player/CPlayerGunBase.cpp` (both bodies) and
`src/MetroidPrime/PortCTweakPlayerControls.cpp` (two new host-only readers). `docs/HANDOFF.md` is
in the diff because `goal_check.sh` runs `gate.sh` with `MP_GATE_DOCS_WRITE=1`, which rewrote the
derived state block; it was not hand-edited. Nothing in `tools/`, `docs/` or `build/goal/` was
edited by me. No asm.

`tools/unit_fit.sh` still reports the same six pre-existing COMDAT weak extras (568 bytes, nothing
in this item emits them). `.text` claimed 4176 / ours 4072 — SHORT by 104, consistent with the two
bodies still being empty: `powerpc-eabi-nm --print-size` on our object gives 4 bytes for
`ProcessInput...R13CStateManager` (retail 0x194) and 4 for `CreateGunLight...` (retail 0x114). The
104 does not equal 404 + 276 minus 8 because `ours` in that line is the claimed-range total, not
the sum of the named functions; I did not chase the difference and neither should a reader.

## 1. `fn_8021580C` / `fn_80215818` — the "blocker" lane 4 listed for `UpdateGunHolster` was not one

Lane 4 named three missing symbols for `ProcessInput` and none for `UpdateGunHolster`. The unit's
own target object lists six undefined `fn_*` that no source defines:

```
fn_800CEF2C  fn_8021580C  fn_80215818  fn_8022A5B4  fn_8022b7f4__...  fn_8022b974__...
```

`fn_8022b7f4`/`fn_8022b974` *are* defined — `src/MetroidPrime/Player/CPlayerVisor.cpp` spells them as
`CPlayer::fn_8022b7f4`, and Itanium mangling of `CPlayer::fn_8022b7f4(CFinalInput const&)` is
literally `fn_8022b7f4__7CPlayerCFRC11CFinalInput`. But **`fn_8021580C` and `fn_80215818` had no
definition anywhere**, and `UpdateGunHolster` calls each of them twice. That is all that stood
between an empty body and a matched function.

`./tools/dis.sh 0x8021580C 0x20` — they are two more three-instruction readers in the same run as
`fn_80215854`/`fn_80215860` (`lwz r3,0(r3)` / `lbz r3,<off>(r3)` / `blr`), at byte offsets **331**
(0x14B) and **330** (0x14A). They have no `configure.py` unit: 0x8021580C is in an unclaimed
`.text` gap (the neighbouring claim is `MetroidPrime/Tweaks/CTweakGuiColors.cpp`,
`splits.txt:1198`, starting at 0x80215878). So they belong in
`src/MetroidPrime/PortCTweakPlayerControls.cpp`, which is the repo's existing home for exactly
this (`fn_8021583C`, `fn_80215854`, `fn_80215860`, `fn_8021586C`), for the same reason: not a unit,
so it is `files.cmake`-only and the DOL still links retail's own bytes.

They are spelled at their measured offsets rather than through a field, because
`SLdrTweakPlayerControls::booleans` is at 0x130 and the generated header declares 21 members
(0x130..0x144), so 0x14A/0x14B are past its end — same as the existing `fn_8021583C`. Adding two
functions to that file cannot move any unit's `.text`: it is not compiled by the dtk build.

**Generalisable: `powerpc-eabi-nm build/G2ME01/obj/<unit>.o | grep ' U '` lists every symbol a unit
still has to borrow.** That is the complete blocker list for writing a body, and reading the
caller's object is cheaper than reading the previous run's notes.

## 2. `UpdateGunHolster` — 0.51% -> 100.0% (784 B)

Fully decoded from `./tools/dis.sh 0x801DDAAC 0x310`. The parts that were not obvious:

- **The two control ids are 18 and 28, and this header's names for them are already right — do not
  change the header.** They come out of the `li r4,18` / `li r4,28` the four `CControlMapper` calls
  pass. `CControlMapper::GetDescriptionForCommand` (0x80009E74) is a jump table at 0x803B1434
  indexed by the command (`cmplwi r3,75` / `slwi r0,r3,2` / `lwzx` / `mtctr` / `bctr`) whose targets
  are `lis r3,-32710; addi r3,r3,22656` = 0x803A5880 plus a per-entry offset, so the string blob
  in `.rodata` gives the exact name for every value: 18 = "Missile/PowerBomb", 28 = "Toggle
  Holster", i.e. `CControlMapper::kC_MissileOrPowerBomb` and `kC_ToggleHolster`. Reaching for
  `kC_ChargeBeam`/`kC_PlasmaBeam` by play instinct compiles and is wrong by 2 and 1. Any future
  enum-value argument in this tree can be settled the same way.
- **Arm order in the source is 2, 1, 0, 3.** mwcceppc emits the arms in source order *here*, and
  retail's `.text` runs `kGHS_Drawn` from +0x60, `kGHS_Drawing` +0x178, `kGHS_Holstered` +0x1AC,
  `kGHS_Holstering` +0x2A4. All 24 permutations measured (`tools/try_batch.py`): the spread is
  28..202 differing instructions and **2103 wins at 28**; the other 23 score 43..202.
- **`GetControlMapper()` is passed by address (`addi r3,r31,5072`)**, i.e. the member, not an
  out-of-line accessor call — `player->GetControlMapper().GetDigitalInput(...)` gives that.
- **Two spellings of the same comparison, both 2-instruction swaps.** The countdown's "has it run
  out" test is `cror eq,lt,eq` + `bne`, i.e. `if (mGunHolsterRemTime <= 0.f)` — *not* `>= 0.f`,
  which emits `cror eq,gt,eq`. And in `kGHS_Drawing` / `kGHS_Holstering` the same `<=` has to be
  written as the *negated* then-clause: `if (mGunHolsterRemTime > 0.f) { countdown } else { finish }`,
  because that is the only form MWCC lowers to retail's plain `fcmpo` + `ble`. Writing
  `if (<= 0) {finish} else {countdown}` gives `cror eq,lt,eq` + `bne` there and costs 6
  differing instructions.
- **`kGHS_Drawn`'s else-half is `else if (!(fire || missile)) { ... } else { ... }`**, not
  `else if (fire || missile) { ... } else if (f0C) { ... }`. Identical program, 5 differing
  instructions: only the first form puts the `GetGunNotFiringTime()` block where retail tail-merges
  it, *after* the countdown block, with both tests branching to it (`bne 0x504` twice).
- `kGHS_Holstered` calls `GetPlayer(mgr)` a second time for the morph test; retail does not CSE it,
  so the second call is written out rather than reusing the `player` local.
- The draw-suppression test reads `CPlayer+0x1314` = **`mPlayerState`** (`CPlayerState+0x30` and
  `+0x34` are `mCurrentVisor` / `mTransitioningVisor`, compared against 2 = `kPV_Scan`), and
  `player+0x5CC` is `mGrappleState`. Decoded by computing the offsets by hand after
  `lwz r3,4884(r31)`: 4884 is 0x1314, not 0x131C — `SFrozenResources` is not involved.
- The four flag words cleared on `kMS_Unmorphed` are stored **descending**
  (920, 916, 912, 908 = pressed, released, last, current), which is what writing them in that order
  produces.

## 3. `UpdateTransform` — 0.50% -> 100.0% (800 B)

`./tools/dis.sh 0x801DD78C 0x320`. Four arms, all the same quaternion -> transform chain.
Three things were not guessable:

- **`CMath::Limit(v, 1.f)` is retail's clamp, and the header already models it.**
  `fabs/frsp/fcmpo/ble` + `lfs -1.0` + `fsel f1,f31,f0,f1` + `fmuls f31,f0,f1` is exactly
  `include/Kyoto/Math/CMath.hpp:44-52`: `Limit(v,h) = AbsF(v) > h ? h * Sign(v) : v`, and
  `Sign(v) = FastFSel(v, 1.f, -1.f)` where `FastFSel` is the header's existing `fsel out, v, h, l`.
  With `h = 1.f` that is `fsel f1,f31,f0,f1` then `fmuls f31,f0,f1`, exactly. A hand-written
  clamp costs the whole 6-instruction block. **Look for the header helper before writing the
  arithmetic** — three of this repo's `CMath` helpers already lower to the exact instruction runs
  retail uses.
- **`CRelAngle`'s constructor is private** (`include/Kyoto/Math/CRelAngle.hpp:37`), so the angle
  must go through `CRelAngle::FromRadians(angle)`; `CRelAngle(angle)` does not compile. Retail
  materialises the 4-byte result and passes its address (`addi r5,r1,16`).
- **Arm order in the source is 1, 0, 2, 3** — the *opposite* of `UpdateGunHolster`, and unlike the
  reverse-source-order rule. Retail's `.text` has `kGHS_Drawing` at +0x7C, `kGHS_Holstered` at
  +0x148 and `kGHS_Holstering` at +0x1D8, with `kGHS_Drawn` falling straight to the tail. Do not
  generalise one function's arm order to its neighbour: measure, 24 permutations is cheap
  (`tools/try_batch.py`, one run, about four minutes for the whole set).

`rotation` is the 3rd argument and the axis is `rotation.GetRight()` = `CVector3f(m00, m10, m20)`,
read at +0/+16/+32 before the switch and handed to an out-of-line
`__ct__13CUnitVector3fFRC9CVector3f`. `result` is written by
`quat.BuildTransform4f() * rotation.GetRotation()` and then `SetTranslation(position)`; every state
including `default` ends at `mTransform = result`.

## Still open, and why (for whoever takes these)

**`ProcessInput` (404 B) — still blocked, now for exactly one symbol.** Lane 4's read of the
blocker list was short by two: with `fn_8021580C`/`fn_80215818` defined this run, the only missing
symbol left is **`fn_8022A5B4`** (0x8C = 140 bytes, retail 0x8022A5B4, also unclaimed by any
`splits.txt` `.text` claim, so `Port*.cpp` is the home). Everything else in the function is decoded
and cheap:

```
r26 = (player->mMorphBallState == 1)                       // subfic/cntlzw/srwi
r28 = mInBigStrike && !r26                                 // rlwinm. bit 4 of the byte at 942
r27 = player->GetFrozenState() && !r26
if (r28 || r27) -> clear 920/916/912/908 and return
if (!fn_8022A5B4(player->GetControlHintManager(), 1, mgr)) -> same clear and return
mInputFlags  = player->FireBeamHeld(input) ? 1 : 0         // clrlwi/neg/or/srwi
mInputFlags |= fn_8022b7f4(player, input)        ? 4 : 0
mInputFlags |= GetDigitalInput(kC_MissileOrPowerBomb, input) ? 2 : 0
mInputFlags |= fn_8022b974(player, input)        ? 8 : 0
mReleasedInputFlags = fn_801DDF18(mLastInputFlags, mInputFlags)
mPressedInputFlags  = fn_801DDF0C(mLastInputFlags, mInputFlags)
mLastInputFlags = mInputFlags
```

`fn_8022A5B4` itself is a loop over a `CHintManager` table — `lwz 24(r3)` (count) x `mulli 112` vs
`lwz 32(r3)` (data), per entry `lhz 4(entry)` -> a `TUniqueId` on the stack -> `GetObjectById` ->
`TCastToPtr<CUnknown46>` -> `lwz 424(obj)` (0x1A8) `and.` with the mask. **It cannot be written
against this tree's `CHintManager`:** `include/MetroidPrime/CHintManager.hpp` is a guessed layout
(`rstl::vector<SHint> mHints` at 0x10, `CHECK_SIZEOF 0x44`) and retail reads a count at **0x18** and
a data pointer at **0x20**, which that layout does not produce; the file says so itself. Landing
this needs `CHintManager`'s real offsets pinned first, and `CUnknown46` does not exist in `include/`
at all. That is a data-layout job on another unit's header, not a body.

**`CreateGunLight` (276 B) — unchanged, and the blocker is still a data address.** Lane 4's
finding stands: it needs the `CGameLight` entity-info global at 0x803BAC30 (and the
`rstl::string_l` at 0x803BAC37), both inside `Kyoto/Particles/CVectorElement.cpp`'s claimed `.data`
(`splits.txt`: `.data start:0x803babe8 end:0x803baeb8`). Re-measured this run and unchanged.

## Verification run by this lane

```
./tools/decomp_build.sh                    # All: 34.94% fuzzy, 28.56% matched, 12.90% linked (12362 / 28465)
python3 tools/check_decl_order.py          # ok: 981 unit(s) checked, 28 permuted, all 28 accounted for
python3 tools/check_symbol_names.py        # checked 525 units; 0 declared names are missing
./tools/unit_fit.sh MetroidPrime/Player/CPlayerGunBase.cpp
./tools/probe_sources.sh                   # 752 files, 0 failed; link: LINKED (289 undefined, 0 duplicates)
MP_GOAL_BASE=$PWD/build/report.base.json ./tools/goal_check.sh build/goal/item.json
#   ok gate.sh / counts 12360 -> 12362, linked 5863 -> 5863 / target rose 17 -> 19 / no asm
#   goal_check: PASS progress-unit-cplayergunbase
```

Instruction-level check used throughout (`.tmp/opencode/sbs.py`, gitignored): prints retail's and
our object's disassembly of one function side by side and counts differing instructions with branch
targets and relocation operands normalised. `tools/try_batch.py` reports the same number per
variant but only prints a unified diff; `sbs.py` prints both columns, which is what identifies
*which* arm is misplaced rather than just that something is.

No `NEW:` lines: the remaining work is in this same unit or behind another unit's header layout,
so filing either would be a restatement of this item.

---

# progress-unit-cplayergunbase — lane 8, third attempt

Worktree `../wt-mp2-goal-L8`, branch `goal/lane-8`, HEAD `6a2f742f`. Unit stays `NonMatching`; no
`flip_test.sh` was run. `MP_GOAL_BASE=$PWD/build/report.base.json ./tools/goal_check.sh
build/goal/item.json`: **PASS**, `target rose: 19 -> 20 / 21 functions`.

`item.json`'s `reason` is stale (it says 17/21 and names four functions left). Lanes 4 and 8 above
had already landed two of them, so this run started at 19/21 with `CreateGunLight` (276 B, 1.45%)
and `ProcessInput` (404 B, 0.99%) the only two left. **`CreateGunLight` is now 100%.**
`ProcessInput` was taken to **95.74% and then reverted** — see (3) for why, and for the body, which
is finished and is the whole remaining work on this unit.

## Measured

| | base (`build/report.base.json`) | now |
|---|---|---|
| unit `matched_functions` | 19 / 21 | **20 / 21** |
| unit `matched_code` | 3496 | 3772 (= +276, exactly `CreateGunLight`'s size: nothing else moved) |
| unit `fuzzy_match_percent` | 83.90804 | 90.421455 |
| DOL `matched_functions` | 12376 | 12377 |
| DOL `fuzzy_match_percent` | 34.959015 | 34.963177 |
| `All:` line | 34.96% / 28.63% / 12.90% | 34.96% / 28.63% / 12.90% |

`gate.sh`'s `report_diff.py` step: `+1 functions at 100%, 0 units newly linked`, `linked 5863 ->
5863` — so no function anywhere got worse. `matched_code` rising by exactly the one function's size
is the check that the `0.f` I added did not move the unit's `.sdata2` pool and shuffle another
function's `lfs` offset; see (1).

Files touched: `src/MetroidPrime/Player/CPlayerGunBase.cpp` (the `CreateGunLight` body, four
includes, the `lbl_803AAC38` declaration, and a comment on `ProcessInput`),
`src/MetroidPrime/PortPoolStandIns.cpp` (the host bytes of `lbl_803AAC38`), and
`docs/research/port_link_gap{,_list}.md` (the one new port gap, which `gate.sh` requires - see
(4)). `docs/HANDOFF.md` is in the diff because `goal_check.sh` runs `gate.sh` with
`MP_GATE_DOCS_WRITE=1`, which rewrote the derived state block; it was not hand-edited. Nothing in
`tools/`, `docs/HANDOFF.md`, `docs/RUNNING_THE_DECOMP.md` or `build/goal/` was edited by me. No asm.

## 0. Two things about this tree that cost me an hour, and will cost the next run the same

**(a) `build/G2ME01/obj/...` is the *retail target* and `build/G2ME01/src/...` is *our* object —
the names are the opposite of what they look like, and the two previous attempts' notes read them
the wrong way round.** `objdiff.json` says so: `"target_path": "build/G2ME01/obj/…"` (dtk's
retail object) and `"base_path": "build/G2ME01/src/…"` (built from our source by the `mwcc_sjis`
rule; `build.ninja:6179`). Lanes 4 and 8 disassembled `obj/…` and read the `blr`-only
`CreateGunLight`/`ProcessInput` as "dtk did not reconstruct them"; that was **our** stale object.
`build/G2ME01/obj/MetroidPrime/Player/CPlayerGunBase.o` has the full 276-byte and 404-byte
functions and 20 of the 21 symbols at 100%. The `nm -u` list quoted in lane 4's blocker 1
(`lbl_803AAC38`, `kInvalidAreaId`, `fn_8022A5B4`, `sForwardVector__9CVector3f`, …) is the
*retail* object's, and it is the right list to work from.

**(b) The worktree's object was stale and ninja believed it.** At the start,
`build/G2ME01/src/MetroidPrime/Player/CPlayerGunBase.o` was dated 2026-09-30 21:57 with *full*
`CreateGunLight`/`ProcessInput` bodies, while the checked-out source (06:51:36 the same morning)
has both empty — i.e. the `build/` directory was seeded from a tree whose source no longer matches
it, and because the `.o` ended up 1 s newer than the source, `ninja -d explain` said "no work to
do" and `decomp_build.sh` reported the stale object's numbers. `touch
src/MetroidPrime/Player/CPlayerGunBase.cpp` first, then build. The numbers happened to come out
the same (19/21 either way) so nothing was *decided* on the stale object, but `report.json` is
only as good as the object it was generated from. **Re-measure means: touch the source, build,
then read `build/report.json`.**

## 1. `CreateGunLight` — 1.45% -> 100.0% (276 B)

Retail 0x801DDF88, decoded from the target object (identical to `./tools/dis.sh 0x801DDF88 0x114`).
**Instruction-for-instruction identical: `.tmp/opencode/sbs.py` reports `differing instrs: 0
retail len 69 ours len 69`**, not just objdiff's 100%.

```cpp
if (mLightId != kInvalidUniqueId) return;
mLightId = mgr.AllocateUniqueId();
const uint lightSource = mLightId.Value();
mgr.AddObject(rs_new CGameLight(mLightId, kInvalidAreaId, false,
    rstl::string_l(lbl_803AAC38 + 7), mTransform, mPlayerUniqueId,
    CLight::BuildDirectional(CVector3f::Forward(), CColor::Black()),
    lightSource, 0, 0.f, nullptr));
```

Five things were not guessable, and each is worth more than the function:

- **`0x803BAC30` is not what the code points at, and the object at that address is not a
  `CEntityInfo`.** This corrects blocker 2 in both notes above. `lis r3,-32709` is
  `0x803B0000` and `addi r4,r3,-21448` is `+0xAC38`, so `r4 = 0x803BAC38`; the target object's
  relocation names it `lbl_803AAC38`, and `config/G2ME01/symbols.txt:17380` has
  `lbl_803AAC38 = .rodata:0x803AAC38; size:0x7 data:string` — a **7-byte rodata string**
  (`3f 3f 28 3f 3f 29 00`, read out of `build/G2ME01/obj/auto_06_803AAC38_rodata.o`), in an
  *unclaimed* rodata range, not in `CVectorElement`'s claimed `.data`. It is the same `"??"`
  placeholder `rs_new` passes as its `operator new` file argument
  (`include/Kyoto/Alloc/CMemory.hpp:59`, `new ("\?\?(\?\?)", nullptr)`), and `addi r4,r4,7` makes
  the light's **name** the second NUL — an empty string. So the two dead `lis`/`addi` pairs before
  and after the null check are one for `rs_new`'s file argument and one for the name; nothing is a
  `CEntityInfo`, and the constructor's eleventh argument really is `nullptr` (`stw r0,16(r1)` with
  a literal 0). **`rstl::string_l(<label> + N)` is already this tree's idiom** — see
  `CFirstPersonCamera.cpp:24` and `CMainShutdownSubsystems.cpp:121` — so the spelling was free.
- **objdiff does not compare the *name* of an `ADDR16_HA`/`LO` relocation.** This is what made
  the address question moot, and it is measurable from a function that already matched: our
  `__ct__` references `@stringBase0` where retail references `lbl_803AAC38` (`nm` shows the retail
  object carrying **both** — its own 7-byte `@stringBase0` *and* the external `lbl_803AAC38`),
  and `__ct__` is at 100%. So the source may use `rs_new` for the allocation and the external
  label for the name, and the two `lis`/`addi` pairs still compare equal. Declaring
  `extern "C" const char lbl_803AAC38[];` and pointing at it is still the honest spelling, and it
  needs a host definition (below) because `probe_sources.sh` strictly links.
- **`mLightId.Value()` has to be a named local.** `TUniqueId::Value()` is `value & 0x3FF`, which
  is exactly retail's `clrlwi r29,r0,22` for the eighth argument. Written inline as that argument,
  MWCC emits the `lhz` *before* the allocation and the `clrlwi` *inside* the null-check arm:
  **12 differing instructions** (`tools/try_batch.py`, variant `base`). Hoisted into
  `const uint lightSource` it is **6**, and the 6 are the `lis`/`addi` for the name landing one
  slot earlier than retail's. `const ushort` and `int` locals are also 6; hoisting
  `mPlayerUniqueId` the same way is **25**; `static_cast<uint>(mLightId.Value())` inline is 12.
- **`rs_new …` and `mgr.AddObject` must be one full expression.** Retail calls `AddObject`
  (0x801DE074) *before* the `extsb. r0,r27 / beq` pair that destroys the `rstl::string`
  temporary, so the temporary's scope has to reach past the call. With
  `CGameLight* light = rs_new …; mgr.AddObject(light);` the destructor runs first and the two
  calls swap: **91.30%**, and 6 differing instructions. As one expression: **0**. The
  `extsb.`/`beq` guard itself is MWCC's, and needs no spelling.
- **mwcceppc does not enforce `protected`; gcc does.** `CVector3f::sForwardVector` is a protected
  static, and the body compiles and links for the DOL with it — then `tools/probe_sources.sh`
  fails on `'CVector3f::sForwardVector' is protected within this context`. `CVector3f::Forward()`
  is the existing public accessor for the same object (`CUnitVector3f.hpp:43`), needs no cast, and
  emits the identical `lis`/`addi` against `sForwardVector__9CVector3f`: still 100%. **Any future
  body that a protected member reaches is a DOL-build pass and a port-probe failure.**

Two more that were free, and are worth not re-deriving: `TAreaId` is four bytes in this tree, so
the second argument's `lwz`/`stw` of `kInvalidAreaId` is the plain argument and not a truncation;
and **adding `0.f` did not move the unit's `.sdata2` pool** — 0.0f is already in it (the
constructor's `mCooldown(0.f)`), so the `lfs f1, -0x506C(r2)` offset matched and no other
function's `lfs` moved. That is what `matched_code` +276 exactly measures.

`tools/unit_fit.sh` now reports **7** functions present in our object but not in the retail unit
object (648 bytes): the six pre-existing COMDAT weak destructors plus one new,
`__dt__rstl::basic_string<…>` (80 bytes), which is the `rstl::string_l` temporary's destructor —
the same harmless kind the tool's own text describes. `.text` is over the claimed range by 640
bytes, which is those 648 less the 8 the two empty bodies used to take.

## 2. `fn_8022A5B4` — what lane 8 got right, and the one thing they did not

`./tools/dis.sh 0x8022A5B4 0x8C`, confirmed against
`build/G2ME01/obj/auto_03_8022A5AC_text.o` (which is what defines it in the DOL link, so the DOL
link is fine and only the host needs a body):

```
r0  = self[0x18]                      count
r30 = self[0x20]                      data
r31 = r30 + count * 112               end
loop: id = *(u16*)(r30 + 4)  ->  mgr.GetObjectById(id)  ->  TCastToPtr<CUnknown46>(that)
      if (r3 != 0 && (*(u32*)(r3 + 0x1A8) & mask) != 0) return true
      r30 += 112; if (r30 != r31) goto loop
return false
```

Lane 8's decode of the loop is right. What they missed is what the read at **+0x1A8** is:
`include/MetroidPrime/CGameHint.hpp:48` has `CHECK_SIZEOF(CGameHint, 0x1a8)` and
`CGameHint : public CActor`, so the flag word is the **first member of a class derived from
`CGameHint`**, not a field of some unrelated type. Retail's `CUnknown46` is that class; this tree
has no declaration for it, and `CGameHint` is already 100%-matched at its own size, so the class
is missing rather than mis-sized. That also means the "fix `CHintManager`'s layout" note below is
only two thirds of the work.

## 3. `ProcessInput` — 0.99% -> 95.74%, then reverted

**The body is finished and is 10 register-allocation instructions from 100%.** With
`fn_8022A5B4` declared, the gate clean of raw offsets, and the following source, objdiff reports
**95.74%** and `.tmp/opencode/sbs.py` reports 10 differing instructions, all of them the register
MWCC picks for one variable:

```cpp
CPlayer* player = GetPlayer(mgr);
const bool morphed = player->GetMorphballTransitionState() == CPlayer::kMS_Morphed;
const bool inBigStrike = mInBigStrike && !morphed;
const bool frozen = player->GetFrozenState() && !morphed;
if (inBigStrike || frozen || fn_8022A5B4(player->GetControlHintManager(), 1, mgr)) {
  mPressedInputFlags = 0; mReleasedInputFlags = 0; mLastInputFlags = 0; mInputFlags = 0;
  return;
}
mInputFlags = player->FireBeamHeld(input) ? 1 : 0;
uint mask = 0u;                                     // ONE variable, reused for all three terms
if (player->fn_8022b7f4(input)) { mask = 4u; }
mInputFlags |= mask;
if (player->GetControlMapper().GetDigitalInput(CControlMapper::kC_MissileOrPowerBomb, input)) {
  mask = 2u;
}
mInputFlags |= mask;
if (player->fn_8022b974(input)) { mask = 8u; }
mInputFlags |= mask;
mReleasedInputFlags = fn_801DDF18(mLastInputFlags, mInputFlags);
mPressedInputFlags = fn_801DDF0C(mLastInputFlags, mInputFlags);
mLastInputFlags = mInputFlags;
```

plus `extern "C" uint fn_801DDF18(uint, uint);` / `fn_801DDF0C` declared near the top (the
definitions stay where they are; mwcceppc emits in reverse source order) and
`#include "MetroidPrime/CHintManager.hpp"`.

The 10: retail puts the mask phi in a **volatile** register each time — `li r6,0 / beq / li r6,4`,
then `li r5,2`, then `li r3,8`, each `or r0,r0,rX`-ed into the common `lwz`/`or`/`stw`. This build
puts it in **r26**, a callee-saved register that `stmw r25,20(r1)` has already saved, for all three
terms. Nothing else differs. Every callee-saved register is spoken for in both (r25 `mgr`, r26
`morphed`, r27 `frozen`, r28 `inBigStrike`, r29 `this`, r30 `input`, r31 `player`), so r26 is the
only free one here and a volatile is the only free one there; it is a tie-break I could not move.

**The two semantic findings, both worth keeping whatever happens to the register:**

- **The hint test is not negated.** Retail's `clrlwi. r0,r3,24 / beq <body>` branches *over* the
  clearing block, so a **true** `fn_8022A5B4` clears the masks. The check asks "is a control hint
  active", and an active hint blocks weapon input exactly as a big strike or a freeze does. My
  first version wrote `!fn_8022A5B4(...)`, which compiles, inverts the program, and mirrors the
  branch — 76.39% with the right everything else, versus 95.74% with the `!` removed.
- **`mInputFlags` is a plain `= c ? 1 : 0` for the first term and a reused `mask` variable for the
  other three.** The first term's `neg r0,r5 / or r0,r0,r5 / srwi r0,r0,31` is MWCC if-converting
  the phi; the other three stay branches, because the value goes into a *variable* that is then
  OR-ed, so the `|=` is common to both arms instead of duplicated into the taken one.

**66 spellings measured** (`tools/try_batch.py`, `.tmp/opencode/pi_variants.py`, differing
instructions; 33 was the naive first attempt, **10 is the best found**):

| tail spelling for terms 2-4 | differing |
|---|---|
| `mInputFlags \|= c ? N : 0;` | 33 (with term 1 as `= FireBeamHeld(...)`), 21 (with `? 1 : 0`) |
| `if (c) { mInputFlags \|= N; }` | 20, then **15** (with term 1 as `? 1 : 0`) |
| `if (c) { mInputFlags \|= N; } else { mInputFlags \|= 0; }` | 15 — MWCC deletes the no-op arm first |
| `if (!(c)) {} else { mInputFlags \|= N; }`, `do {} while (0)`, `!= false`, `+= 0` first, `\| 0` | 15 each |
| `const uint m = c ? N : 0u; mInputFlags \|= m;` | 21 |
| `uint m; if (c) m = N; else m = 0; mInputFlags \|= m;` | 21 (if-converted) |
| `uint m = N; m = c ? m : 0; …` | 21 |
| `uint m = 0u; if (c) m = N; mInputFlags \|= m;` — **one variable per term** | 28 |
| `… m = N; if (!c) m = 0; …` | 30 |
| same, `mask = 0u;` re-executed before each `if` | 28 |
| all three variables declared at the top of the function | 69 / 53 |
| `switch (c ? 1 : 0)`, `static_cast<uint>(c) * N`, `c ? (1u << k) : 0` | 34 / 28 / 21 |
| **`uint mask = 0u;` declared after term 1, reused** | **10** |
| same with `int`, or with `mInputFlags = mInputFlags \| mask` | 10 each |
| term 1 as `mInputFlags \|= c ? 1 : 0` | 47 (two more than `=`) |

**Why it is reverted rather than left in.** The only missing symbol is `fn_8022A5B4`, and giving
the port a body for it needs three raw offsets (`self + 0x18`, `self + 0x20`, `entity + 0x1A8`).
`tools/check_raw_offsets.py` is a `gate.sh` step and it is right to be: `PortGlobals.cpp` has no
section in `docs/research/raw_offsets.md`, so `GATE FAIL: raw-offsets link-gap`. Two ways out, and
a next run should pick one deliberately:

1. **Model the layout (the right one).** `rstl::vector<T>` here is
   `{rmemory_allocator; int mCount; int mCapacity; T* mItems;}`, so retail's count at 0x18 and data
   at 0x20 mean **`rstl::vector<SHint> mHints` sits at 0x14**. That is consistent with everything
   else the header already claims: `fn_801B9480`'s comment puts the current hint id at **0x0C**,
   and `CHECK_SIZEOF(CHintManager, 0x44)` = 0x14 + three 16-byte vectors exactly. So the guessed
   layout is one member short before `mHints` and the fix is to put it at 0x14, give `SHint` its
   measured 0x70 stride with the `TUniqueId` at +4, and add the hintable class derived from
   `CGameHint` that owns the word at 0x1A8. `CHintManager` has no unit of its own, so no `.text`
   moves. Then `fn_8022A5B4` is written against members and there are no raw offsets at all.
2. **Add the `## src/MetroidPrime/PortGlobals.cpp (3 sites)` section** to
   `docs/research/raw_offsets.md`, which is what 71 other files do. It is one measured line, but
   it books the debt in a `Port*.cpp` rather than paying it, and `docs/research/` is close enough
   to the judge that I did not want to decide that inside a `progress` item.

Neither choice is made here, so the body is not in the tree. What is left in the source is a
comment pointing at this file, so nothing is lost.

## 4. The one port gap this item opened, and why it is booked rather than closed

Writing `CreateGunLight` made `CPlayerGunBase.o` reference
`CGameLight::CGameLight(TUniqueId, TAreaId, bool, rstl::string const&, CTransform4f const&,
TUniqueId, CLight const&, uint, uint, float, CEntityInfo const*)`. `gate.sh`'s `link-gap` step
ratchets against `docs/research/port_link_gap_list.md`, so a newly missing symbol fails until the
list is regenerated — and the tool says so: *"Run --write-list and describe what provides each new
symbol in port_link_gap.md."* That is what I did: `--write-list` is a 2-line diff (the
`other game methods` row 212 -> 213 and the one name), plus one dated entry in
`docs/research/port_link_gap.md` in the style of the six already there.

It is not closeable here. `src/MetroidPrime/CGameLight.cpp:4` has the body, and that unit is out
of `files.cmake` with a measured reason (`tools/check_files_cmake.py:381`: listing it opens five
names of its own — `CEntityInfo`'s copy constructor and destructor, `CActorParameters`'s
constructor and two more — and closes none). Defining it host-side instead runs into the same
five through `CActor(..., CModelData(), CMaterialList(kMT_NoStepLogic), CActorParameters(), ...)`,
because `CActorParameters.cpp` is not in the port build. It closes when `CActorParameters` and
`CEntityInfo` are. The DOL build is unaffected: the name is resolved by the linker out of
`build/G2ME01/obj/MetroidPrime/Player/CPlayerGunBase.o`, and the unit stays `NonMatching`.

`lbl_803AAC38` needed no such booking: the host bytes went into `src/MetroidPrime/PortPoolStandIns.cpp`
next to `lbl_803A56C0`, retail's other transcribed rodata pool, and
`build/probe-logs/link_check.log` reports `290 undefined against a baseline of 291 (no growth)`,
0 duplicates.

## Verification run by this lane

```
touch src/MetroidPrime/Player/CPlayerGunBase.cpp      # see 0(b): the seeded object is stale
./tools/decomp_build.sh
#   All:  34.96% fuzzy, 28.63% matched, 12.90% linked (12377 / 28465 functions)
#   main/MetroidPrime/Player/CPlayerGunBase: 90.42% fuzzy, 90.33% matched (20 / 21 functions)
python3 tools/check_symbol_names.py     # checked 525 units; 0 declared names are missing
python3 tools/check_decl_order.py       # ok: 981 unit(s) checked, 28 permuted, all 28 accounted for
python3 tools/check_raw_offsets.py      # ok: 167 raw-offset site(s) in 71 file(s), all documented
./tools/probe_sources.sh                # 752 files, 0 failed; link: LINKED (290 undefined, 0 dups)
./tools/unit_fit.sh MetroidPrime/Player/CPlayerGunBase.cpp   # 7 harmless COMDAT extras, 648 B
sha1sum build/G2ME01/main.dol          # 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/gate.sh                         # GATE PASS  6a2f742f+5 changed
MP_GOAL_BASE=$PWD/build/report.base.json ./tools/goal_check.sh build/goal/item.json
#   ok gate.sh / counts 12376 -> 12377, linked 5863 -> 5863 / target rose 19 -> 20 / no asm
#   goal_check: PASS progress-unit-cplayergunbase
```

Instruction-level check used throughout (`.tmp/opencode/sbs.py`, gitignored; rewritten this run to
take the function name and the unit as arguments): prints the target object's and our object's
disassembly of one function side by side and counts differing instructions with branch targets
normalised. `tools/try_batch.py` reports the same number per variant but only prints a unified
diff, and it matches built functions by *exact* symbol name, so it is no use on a name that
carries a `__FUiUi` suffix (the two `fn_801DDF*` leaves).

No `NEW:` lines: everything left is in this same unit, and filing it would restate the item.

## Lane 8: passed, then failed on the moved tip (2026-10-02 05:55:17Z)

The judged change failed goal_check.sh (exit 1) once rebased onto e761ac133c98; re-do it against the current tip.

---

# progress-unit-cplayergunbase — lane 8, fourth attempt

Worktree `../wt-mp2-goal-L8`, branch `goal/lane-8`, HEAD `e761ac13`. Unit stays `NonMatching` in
`configure.py`; the flip was attempted and reverted (see (5) for why it fails). `MP_GOAL_BASE=$PWD/
build/goal/judge/report.base.json ./tools/goal_check.sh build/goal/item.json`: **PASS**,
`target rose: 19 -> 21 / 21 functions`. **The unit is at 100% fuzzy and 100% matched.**

`item.json`'s `reason` is stale again (it says 17/21 and names four functions left). All four were
taken to 100% across the runs above and this one; `CreateGunLight` was re-done here because the
third attempt's judged change failed on the moved tip (see the last section of the notes above),
and `ProcessInput` — the one every earlier run called blocked — went this run.

## Measured

| | base (`build/goal/judge/report.base.json`) | now |
|---|---|---|
| unit `matched_functions` | 19 / 21 | **21 / 21** |
| unit `matched_code` | 3496 | **4176** (= the whole `.text`) |
| unit `fuzzy_match_percent` | 83.90804 | **100.0** |
| DOL `matched_functions` | 12390 | 12392 |
| DOL `fuzzy_match_percent` | 35.010788 | 35.02107 |
| `All:` line | 35.01% / 28.68% / 12.90% | 35.02% / 28.69% / 12.90% |

`gate.sh`'s `report_diff.py`: `+2 functions at 100%, 0 units newly linked`, `linked 5863 -> 5863`,
`no regression`. Port link `291 undefined, 0 duplicates` — **unchanged**, see (3).

| function | bytes | before | after |
|---|---|---|---|
| `CreateGunLight__14CPlayerGunBaseFR13CStateManager` | 276 | 1.45 | **100.0** |
| `ProcessInput__14CPlayerGunBaseFRC11CFinalInputR13CStateManager` | 404 | 0.99 | **100.0** |

Both verified instruction-for-instruction against the **target** object with `.tmp/opencode/sbs.py`
(`differing instrs: 0 retail len 69 ours len 69` and `differing instrs: 0 retail len 101 ours len 101`),
not just on objdiff's percentage.

Files touched: `src/MetroidPrime/Player/CPlayerGunBase.cpp` (both bodies, the four includes, the
`lbl_803AAC38` and `fn_*` declarations, and **three wrong-callee fixes** — (4)), `files.cmake`,
`include/MetroidPrime/CHintManager.hpp`, `include/MetroidPrime/CGameHint.hpp`,
`src/MetroidPrime/TypesMatch.cpp`, `src/MetroidPrime/PortGlobals.cpp`,
`src/MetroidPrime/PortPoolStandIns.cpp`, new `src/MetroidPrime/PortCHintManager.cpp`, and
`docs/research/port_link_gap{,_list}.md`. `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` are in
the diff because `goal_check.sh` runs `gate.sh` with `MP_GATE_DOCS_WRITE=1`, which rewrote the
derived state block and the probe file count; neither was hand-edited. Nothing in `tools/` or
`build/goal/` was edited by me. No asm.

## 0. Two corrections to the notes above, both measured this run

**(a) `item.json`'s `reason` and this file's earlier counts are both behind the tree.** Re-measured
properly — `touch src/MetroidPrime/Player/CPlayerGunBase.cpp` first, per the third attempt's own
warning that the seeded object is stale — the tree is at 19/21 with **two** functions left, not
four. A `report.json` read without rebuilding answers a question about the previous build.

**(b) The third attempt did not fail on its code.** `build/goal/run.log:3640-3655` records the real
reason: `GATE FAIL: docs`, `stale: the gap table says other game methods is 213, the generated list
has 214`. Its code passed everything else, including `target rose: 19 -> 20`. The lesson is the one
`tools/check_docs_claims.py` exists for and this file's own §4 above describes: **a generated table
row has to be regenerated, not recomputed by hand** — `python3 tools/link_gap.py --write-list`
rewrites `port_link_gap_list.md`, and the `port_link_gap.md` group table has to be read off *that*
file afterwards. Doing it in that order is the whole fix.

## 1. `CreateGunLight` — 1.45% -> 100.0% (276 B), re-done

The third attempt's body was correct and reproduced instruction-for-instruction; this run re-applied
it and **corrected one thing in it**. It defined

```cpp
extern "C" const char lbl_803AAC38[] = "??(?)\0";
```

in `PortPoolStandIns.cpp`. Retail's seven bytes are `3f 3f 28 3f 3f 29 00` (`powerpc-eabi-objdump -s
-j .rodata build/G2ME01/obj/auto_06_803AAC38_rodata.o` reads `3f3f283f 3f290000`) — that is
`"??(??)\0"`, **six characters plus a NUL**, not five. `CreateGunLight` asks for `lbl_803AAC38 + 7`,
so the definition has to be **eight** bytes with a trailing NUL, or the pointer names one past the
object. `"??(??)\0\0"` is what is there now. This is invisible to objdiff (it does not compare the
contents of a `.rodata` object another unit owns) and to the DOL build, and it is exactly the
`lbl_803A56C0` trap the neighbouring definition in the same file documents — the leading `\0\0`
there, the trailing one here.

Everything else in that body re-measured identically; the argument-by-argument derivation in the
notes above stands, including that `rs_new ...` and `mgr.AddObject` must be one full expression
(91.30% otherwise) and that `mLightId.Value()` has to be a hoisted local (12 differing instructions
inline, 6 hoisted).

## 2. `ProcessInput` — 0.99% -> 100.0% (404 B), and the notes' blocker was not one

The blocker every earlier run recorded was `fn_8022A5B4`, on the grounds that writing it needs three
**raw offsets** (`CHintManager`+0x18/+0x20 and entity+0x1A8) for a `CUnknown46` that `include/` does
not have, and that `tools/check_raw_offsets.py` is a gate. **Every one of those three claims is
false**, and re-measuring costs about ten minutes:

- **`CHintManager`'s existing layout already produces retail's offsets.** The third attempt's
  "one member short before `mHints`" is wrong. `tools/probe_cc.sh` with `tools/probe_offsets.cpp`'s
  method (`.tmp/opencode/probe_hint2.cpp`) reads back, under mwcceppc:

  | | measured | retail |
  |---|---|---|
  | `mHints` | **0x14** | — |
  | `rstl::vector::mCount` (at `mHints`+8) | **0x18** | `lwz 24(r3)` |
  | `rstl::vector::mItems` (at `mHints`+12) | **0x20** | `lwz 32(r3)` |
  | `sizeof(CHintManager)` | **0x44** | — |

  The header's `CHECK_SIZEOF(CHintManager, 0x44)` was already right and nothing had to move.
- **`CUnknown46` is not absent from `include/`** — it was declared in `src/MetroidPrime/TypesMatch.cpp`
  all along (`TYPES_MATCH_CLASS(CUnknown46, CGameHint)` at line 411), just as a throwaway with no
  members. It is retail's control-hint actor: `docs/research/missing_classes.md` already names the
  `LoadControlHint` / `CTLH` loader as `10CUnknown46`, `fn_8022CEFC`, 752 bytes. `CGameHint` is
  `CHECK_SIZEOF` **0x1A8** and that constructor writes the word at +0x1A8 as its second act
  (`stw r30,424(r29)` with `li r0,19` in r30), while `fn_8022A5B4` reads `lwz r0,424(r3)` — so the
  flag word is `CUnknown46`'s **first member**, not an unrelated field. The class moved to
  `include/MetroidPrime/CGameHint.hpp` with that one member modelled and **no `CHECK_SIZEOF`**, since
  the remaining 0x148 is unread and a guessed total would be a claim the tree cannot back.
- **`SHint`'s real layout was already in the header's comment and only needed writing down.** 0x70
  stride, `TUniqueId` at +4, pinned by three functions walking the table identically —
  `fn_801BA3EC` (0x801BA3EC), `fn_801BA428` (0x801BA428) and `fn_8022A5B4` all do
  `mulli rX,count,112` and `lhz rX,4(entry)`.

So `fn_8022A5B4` is written **against members**, in a new `src/MetroidPrime/PortCHintManager.cpp`
(the unclaimed-`.text`-gap arrangement `PortCTweakPlayerControls.cpp` already uses), and
`tools/check_raw_offsets.py` stays green at **167 sites in 71 files** — no new file, no new debt.
`TCastToPtr<CUnknown46>` went into `PortGlobals.cpp` beside the other `PORT_CAST_TO_PTR`s, because
`TypesMatch.cpp` holds the real one and is out of the port build. Moving `CUnknown46`'s *declaration*
into the header (so it has members) required deleting the `TYPES_MATCH_CLASS` line that made a second,
member-less `CUnknown46`; its `TypesMatch` and `TCastToPtr` bodies stay put and `TypesMatch` is
still **511 / 511 at 100%**, so nothing in that `Matching` unit moved.

**Generalisable: a "raw offsets needed" blocker is often a "the header does not model this yet"
blocker wearing a disguise.** Two of the three offsets were already reachable and one needed a class
that already existed in a source file. Check what the header *produces* (`tools/probe_cc.sh` +
`tools/probe_offsets.cpp`) before booking debt in `raw_offsets.md`.

## 3. The four spellings that took `ProcessInput` from 10 to 0 differing instructions

The earlier attempts measured 66 spellings and stopped at 10, all of them the same register
allocation. Measured with `tools/try_batch.py` this run, differing instructions:

| spelling | differing |
|---|---|
| one reused `uint mask = 0u;` (the earlier best) | 10 |
| one variable per term, each declared at the top | 28 |
| one variable per term, each in its own scope | 28 |
| `<< 2` / `<< 1` / `<< 3` instead of a mask | 28 |
| `uint m = c ? N : 0u;` per term | 21 |
| **each term's condition pulled into a named `const bool` first** | **7** |
| named condition on term 4 only, plus `== true` | 6 |
| **term 4's condition as `const uint` compared `!= 0u`** | **0** |

Two findings, both about *register lifetime*, and both worth more than this function:

- **A mask's zero-init is hoistable above the call that produces its condition.** Retail's three
  phi pairs are `li r6,0 / beq / li r6,4`, `li r5,0 / beq / li r5,2`, `li r3,0 / beq / li r3,8` —
  each in a **volatile** register, and each `li rX,0` landing *after* the `bl`. Written the obvious
  way, `mask = 0u; if (cond) { mask = Nu; } mInputFlags |= mask;` has a constant zero-init with
  nothing to order it against, so MWCC sinks it above the call, `mask` is live across the `bl`, and
  it must be callee-saved — **r26**, which is the entire 10-instruction difference. Introducing the
  condition as a named `const bool` *first* makes the zero-init depend on the call, and the mask
  becomes volatile-local. 10 -> 7. This is a general shape, not a spelling: **anything MWCC is free
  to hoist above a call is what forces a callee-saved register.**
- **The last 7 is if-conversion, and a boolean is what triggers it.** With `const bool`, MWCC
  lowers `mask = c8 ? 8u : 0u` to `clrlwi / cmplwi r0,1 / neg / or / srwi.` in **r4** rather than
  retail's `clrlwi. r0,r3,24 / li r3,0 / beq / li r3,8` — a boolean compare against a constant is
  exactly what its if-conversion pass is built for. Declaring the *same* condition `const uint` and
  testing `!= 0u` removes the boolean-ness and the phi survives: 7 -> **0**. The other two terms
  still need a `bool` — `const uint` on term 2 costs 10 — so **the three terms deliberately use
  different types.** Do not "tidy" that into three identical lines; it costs the match.

The first two findings in the notes above still stand and are load-bearing: **the hint test is not
negated** (a true `fn_8022A5B4` clears the masks; `beq` branches over the clearing block), and term
1 is `= c ? 1 : 0` while terms 2-4 are read-modify-written from the member.

## 4. Three functions that objdiff scored 100% called the **wrong function**

`tools/flip_test.sh` failed first on `undefined: 'CStateManager::GetObjectByIdFromListAll(TUniqueId)'`
— a symbol that appears nowhere in retail's object. Comparing the two objects' **relocation symbol
names** per function finds three genuine mismatches, all in code already at 100%:

| function | retail's `bl` target | what the source called |
|---|---|---|
| `GetPlayer` | `GetObjectById__13CStateManagerCF9TUniqueId` (const) | `ObjectById` (non-const) |
| `GetPlayerFromAll` | `ObjectById__13CStateManagerF9TUniqueId` | `GetObjectByIdFromListAll` |
| `ProcessInput` | `GetControlHintManager__7CPlayerCFv` (const) | `GetControlHintManager` (non-const) |

`GetPlayerFromAll` is the worst: **the name says "from all lists" and retail calls the plain
`ObjectById`.** All three are fixed and all 21 functions now agree on relocation names, except
`__dt__` which carries one extra COMDAT weak reference.

**objdiff does not compare the symbol name of a `R_PPC_REL24` relocation** — the instruction stream
is one `bl` either way, so a wrong callee scores 100% and hides until the unit is linked with our
object in it. `GetPlayerFromAll` had been sitting at 100% across at least three previous runs of
this item. The check is four lines of `powerpc-eabi-objdump -d -r` per object plus a dict compare
(`.tmp/opencode/`, throwaway); **it is worth running on any `progress` item whose unit is close to a
flip**, because it is the only thing standing between "100%" and "reproduces retail".

## 5. The flip, and exactly what stops it

`./tools/flip_test.sh MetroidPrime/Player/CPlayerGunBase.cpp` → **FAIL**, and the reason is measured,
not guessed:

```
WARNING: 87 computed checksum(s) did NOT match
sha1sum build/G2ME01/main.dol -> 3f7da7fcbb9250d31555c7a1d1867fd3ab229f8e   (expected 6ef9b491...)
```

Flipped by hand and both DOLs kept: the flipped one is **608 bytes larger** (3969632 vs 3969024) and
comparing it section by section against `tools/dol_read.py`'s table shows **every** section differing,
starting with `.text` at +0x2A. That is what a size change does to a fixed-address image — it is not
648 misplaced bytes, it is one oversized object shifting everything after it. The cause is
`tools/unit_fit.sh`:

```
.text  claimed 4176  ours 4824  retail 4176  over by 648
7 function(s) present in ours but not in the retail unit object, 648 bytes total
```

six pre-existing COMDAT weak template/inline destructors plus the `rstl::string` temporary
destructor that `CreateGunLight`'s `rstl::string_l(...)` argument introduced (80 bytes). The tool's
own text says CAi carries 224 bytes of these and still flips, so the count alone is not the verdict —
but here the DOL sha1 is, and it fails. `configure.py` was restored and the tree rebuilt
(`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`) before the judge ran.

**So the next thing this unit needs is not a body: it is for those seven COMDAT instantiations to
stop being emitted.** That is a template/inline question about `CRainSplashGenerator`'s members and
`rstl::single_ptr`'s destructor, in *other* units' headers, and it is the kind of change that moves
`.text` everywhere — a `match` item, not a `progress` one. Not attempted here.

## Verification run by this lane

```
touch src/MetroidPrime/Player/CPlayerGunBase.cpp   # see §0(a): the seeded object is stale
./tools/decomp_build.sh
#   All:  35.02% fuzzy, 28.69% matched, 12.90% linked (12392 / 28465 functions)
#   main/MetroidPrime/Player/CPlayerGunBase: 100.00% fuzzy, 100.00% matched (21 / 21 functions)
sha1sum build/G2ME01/main.dol                       # 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
86 RELs cmp orig/G2ME01/files/RelProd/*.rel         # 0 differ
python3 tools/check_symbol_names.py                 # checked 525 units; 0 declared names are missing
python3 tools/check_decl_order.py                   # ok: 981 unit(s) checked, 28 permuted, all 28 accounted for
python3 tools/check_raw_offsets.py                  # ok: 167 raw-offset site(s) in 71 file(s), all documented
python3 tools/check_files_cmake.py                  # every configured DOL object is in files.cmake or excluded
./tools/probe_sources.sh                            # 753 files, 0 failed; STRICT PASS 291 undefined, 0 dups
python3 tools/link_gap.py --rebuild --write-list   # 286 entries; fn_8022A5B4 removed, CGameLight ctor added
python3 tools/check_docs_claims.py                  # docs claims agree with the tree
./tools/unit_fit.sh MetroidPrime/Player/CPlayerGunBase.cpp   # 7 COMDAT extras, 648 B - see §5
./tools/flip_test.sh MetroidPrime/Player/CPlayerGunBase.cpp  # FAIL: DOL sha1 differs, see §5
MP_GOAL_BASE=$PWD/build/goal/judge/report.base.json ./tools/goal_check.sh build/goal/item.json
#   ok gate.sh / counts 12390 -> 12392, linked 5863 -> 5863 / target rose 19 -> 21 / no asm
#   goal_check: PASS progress-unit-cplayergunbase
```

Scratch scripts, all gitignored in `.tmp/opencode/`: `sbs.py` (side-by-side instruction diff of one
function against the **target** object), `probe_hint.cpp` / `probe_hint2.cpp` + `tools/probe_cc.sh`
(the `CHintManager` / `SHint` offset probe — `offsetof` under mwcceppc, not the host), and
`pi2.py`..`pi10.py` (the 40-odd `ProcessInput` spellings for `tools/try_batch.py`).

No `NEW:` lines: everything still open on this unit is in it (the COMDAT extras, §5), so filing one
would restate the item.
