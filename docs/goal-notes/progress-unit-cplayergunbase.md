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
