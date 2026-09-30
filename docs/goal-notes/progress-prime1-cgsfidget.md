# progress-prime1-cgsfidget — `MetroidPrime/Weapons/GunController/CGSFidget`

`kind: progress`. The unit stays `NonMatching`; I did not run `flip_test.sh`. Baseline re-measured
on the clean tree before I touched anything: **4 / 6 functions matched**, unit fuzzy 70.82%,
`All: 31.54% fuzzy, 24.03% matched, 11.83% linked (10381 / 28465)`.

**Result: 6 / 6 matched, unit 100.00% fuzzy and 100.00% matched** (`All:` 31.54% / 24.04% /
10383). Both functions the item named are now 100%, and Prime 1's source matched with one small
edit. `tools/goal_check.sh build/goal/item.json` → **PASS**, whole gate including `main.dol` sha1,
86 RELs, per-function report diff, `check_docs_claims.py`, module wiring and the port probe.

## Per function, before → after

| function | before | after | what it took |
|---|---|---|---|
| `SetAnim` | 95.80% | **100.00%** | Prime 1's named `CPASAnimParm` local (one line) |
| `LoadAnimAsync` | 0.00% (declared, no body) | **100.00%** | Prime 1 verbatim, 3 lines + the `fn_` declaration |

`__ct__`, `Update`, `UnLoadAnim` and `IsAnimLoaded` were already 100% and stayed there. The whole
unit is now 100% fuzzy, which is a signal, not a result — it is `NonMatching` and this item did not
flip it.

## What each change was, and the evidence for it

### `SetAnim` — 95.80% → 100%: the elided `CPASAnimParm` copy

The item seeded the unit as "2 unmatched functions", but `SetAnim` was one four-instruction insert
away. The diff (`objdiff-cli diff`, target = ours) was four `DIFF_INSERT`s at retail 0x244-0x254
plus a run of shifted stack offsets:

```
  580  INSERT  lwz r4, 0x10(r1)
  584          addi r3, r1, 0x60
  588  INSERT  lwz r0, 0x14(r1)
  592  INSERT  stw r4, 0x60(r1)
  596  INSERT  stw r0, 0x64(r1)
  600          bl GetBoolValue__12CPASAnimParmCFv
```

Retail loads the two words `CPASAnimState::GetAnimParmData` sret into a **named local** at
`0x60(r1)` and calls `GetBoolValue` on that local. We chained the call onto the sret temp, so
`GetBoolValue`'s `this` was the temp and the copy never happened; that also left the frame 0xF0
instead of retail's 0x100 and pushed the `CPASAnimParmData` temporary 0x24 bytes up. Prime 1
already has the named local:

```cpp
CPASAnimParm parmData(pas.GetAnimState(pas::kAS_Getup)->GetAnimParmData(anim.second, 3));
bool loop = parmData.GetBoolValue();
```

Taking it over unchanged, with `const` added on both (this tree's style, and the neighbour
`SetAnim` already used it) took it straight to 100% — no further tuning. So this one needed a small
edit, not a codegen lesson.

### `LoadAnimAsync` — 0.00% → 100%: the symbol was declared and had no body

`CGunController.cpp:68` has called it since the previous item, and `CGSFidget.hpp:16` declares it,
but nothing defined it, so our object carried 5 of retail's 6 functions. Prime 1's body is
`FindBestAnimation` on the same parm data plus one call, and retail's disassembly at 0x801DD05C is
that and nothing else — 0xF0 bytes, same frame shape, same eight `CPASAnimParm` temporaries in the
same slots. It went in with Prime 1's spelling, changing only `data.GetCharacterInfo()
.GetPASDatabase()` to this tree's `data.GetPASDatabase()` (already used by `SetAnim`, and
`CAnimData::GetPASDatabase` is exactly `mCharInfo.GetPASDatabase()` with `mCharInfo` at +0x3C,
which is the `addi r4, r27, 0x3c` retail uses as `FindBestAnimation`'s `this`).

**One thing Prime 1's source could not supply, and it is the reason this item is not simply
"copy Prime 1":** Prime 1's last call is `NWeaponTypes::get_token_vector(data, anim.second,
mAnims, true)`, and Echoes renamed it. Retail's call at 0x801DD084 is `bl fn_8018A8D0` with
`r3 = data`, `r4 = anim.second` (still live from the `lwz` at 0x801DD11C), `r5 = this` and
`r6 = 1` — the `bl fn_8018A8D0` is at 0x801DD134. Disassembling `fn_8018A8D0` confirms that shape
and adds what it is: it builds a
`CAnimPlaybackParms` from `animId` (`stw r4, 0x24(r1)`), resolves it through
`CAnimData::GetAnimationPrimitives`, and hands it to `fn_8018A5C4` with `preLock` — the
single-id form, distinct from the `rstl::vector<int>` list form already declared as
`fn_8018A7E8`. So I declared it in `include/MetroidPrime/Weapons/WeaponCommon.hpp` next to its two
siblings, in the same `extern "C"`-inside-`NWeaponTypes` form, and left the body alone: the unit is
`NonMatching`, so the DOL link never asks for the symbol, and writing a stand-in for it is exactly
the thing this repo does not do.

**Declaration order.** First attempt put `LoadAnimAsync` immediately before `SetAnim` in the
source, which `tools/check_decl_order.py --unit ...` caught:

```
  * SetAnim__9CGSFidgetFR9CAnimDataiiiR13CStateManager LoadAnimAsync__9CGSFidgetFR9CAnimDataiiiR13CStateManager
  * LoadAnimAsync__9CGSFidgetFR9CAnimDataiiiR13CStateManager SetAnim__9CGSFidgetFR9CAnimDataiiiR13CStateManager
  -> would break on a flip
```

objdiff and `unit_fit.sh` were both perfectly happy with that permutation, which is the case the
"declare in reverse" rule exists for. It belongs **after** `SetAnim` (retail 0x6C < 0x15C, source
descending). Reordered, the same command says `ok: 1 unit(s) checked, none emits its functions out
of retail order`, and every one of the six is now retail's exact size:

```
ours                                        retail
00000000 00000020 T IsAnimLoaded             00000000 00000020 T IsAnimLoaded
00000020 0000004c T UnLoadAnim               00000020 0000004c T UnLoadAnim
000001c0 000000f0 T LoadAnimAsync            0000006c 000000f0 T LoadAnimAsync
000002ec 00000190 T SetAnim                  0000015c 00000190 T SetAnim
0000047c 00000060 T Update                   000002ec 00000060 T Update
0000052c 00000024 T __ct__                   0000034c 00000024 T __ct__
```

## Two documentation changes the gate forced, and one it did not

`gate.sh`'s `link-gap` step and then its `docs` step both failed on the first run of the judge,
and both are the point of the gate rather than an obstacle to it:

1. `python3 tools/link_gap.py --rebuild --write-list` rewrote
   `docs/research/port_link_gap_list.md` — `_ZN9CGSFidget13LoadAnimAsync...` out,
   `fn_8018A8D0` in, **246 → 246**. Its predecessor paragraph in `docs/research/port_link_gap.md`
   (dated 2026-09-30, the `CGunController` item) had concluded that this callee "is not closable
   from what is in this tree" because "retail's body ends in `get_token_vector(CAnimData&, int,
   rstl::vector<CToken>&, bool)`" and no such function was declared. That is **superseded**, and I
   said so in place: the obstacle was never the token machinery, only that nobody had named the
   single-id form, and the gap list only carries names.
2. The per-group table in the same file said 167 / 63; the generated list now has 166 / 64. Fixed.

## Measured, for whoever takes this unit next

- `tools/unit_fit.sh MetroidPrime/Weapons/GunController/CGSFidget.cpp`: `.text` claimed 880, ours
  1688, "over by 808". Baseline on the clean tree was ours 1432, "over by 552" — the growth is
  `LoadAnimAsync`'s 0xF0 plus 16 bytes of inter-COMDAT padding. `.rodata` is **SHORT by 5** — the
  11-byte `"Whole Body"` against a claimed 16 — and that was already true at baseline; it is
  retail's 5-byte inter-unit gap, and the unit's `.rodata` scores 100% in the report.
- All 6 functions our object emits that the retail unit object does not are **`W` (weak) COMDATs**,
  and there were already all 6 at baseline — my change added none:
  `reserve__Q24rstl42vector<6CToken...>Fi` 204, `__as__...` 180, `__dt__...` 160,
  `clear__...` 124, `~basic_string` 80, `__dt__16CPASAnimParmDataFv` 60 = 808 bytes.
  Both the retail linker and mwldeppc discard duplicate COMDATs, and CAi carries 224 bytes of the
  same kind and still flips. So the unit looks like a **plausible `match` candidate now** — it is
  100% across the board and in the right order — but I did not test that, because this item says
  the unit stays `NonMatching`. `tools/flip_test.sh` decides it, not the percentages.
- `fn_8018A8D0`'s own body (0x8018A8D0..0x8018A984) is in the **unclaimed gap** between
  `MetroidPrime/CDamageInfo.cpp` (ends 0x8018A188) and `MetroidPrime/Player/CMorphBallShadow.cpp`
  (starts 0x8018A9CC), together with `fn_8018A7E8` and `fn_8018A5C4`. Nothing in `splits.txt`
  claims any of them, so the port link has never been asked for those symbols and cannot be until a
  unit claims that gap.

## Nothing filed as `NEW:`

No new blocker: both named functions reached 100%, and the one wall I hit (declaration order) was a
known rule with a known tool to catch it, not a measured wall.
