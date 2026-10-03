# carve-801d7e9c (match) - DONE

Carved `fn_801D7E9C` (0x801D7E9C..0x801D7EBC, 0x20 = 32 bytes) out of dtk's unclaimed
`auto_03_801D72D0_text` as its own `Matching` unit. **PASS**, twice, on the final tree.

## The change (four carve files + one stand-in)

| file | what |
|---|---|
| `src/MetroidPrime/Weapons/GunController/Carve801D7E9C.c` | new, 124 lines, one definition (`:124`) |
| `configure.py:625-630` | `Object(Matching, "MetroidPrime/Weapons/GunController/Carve801D7E9C.c")` on **line 630 alone**, with a 5-line comment above it, in address order between `CGunMotion.cpp` (0x801D6BDC..0x801D72D0) and `Carve801D8248.c` (0x801D8248) |
| `config/G2ME01/splits.txt:1303-1304` | `MetroidPrime/Weapons/GunController/Carve801D7E9C.c:` / `.text start:0x801D7E9C end:0x801D7EBC` |
| `files.cmake:2093-2098` | the `.c` source on line 2098 plus its comment, above the `Carve801D8248.c` line |
| `src/MetroidPrime/PortLinkStubs.cpp:1986-2015` | appended announced stand-in `stub_carve801d7e9c_0` (comment from 1986, `asm` label 2013, body 2015) |

The claim is **exactly** the 0x20 range and nothing else. Both neighbours stay unclaimed:
below it 0x801D72D0..0x801D7E9C (`fn_801D76BC`, 0x7E0, ends exactly at this claim), above it
0x801D7EBC..0x801D8248 (`fn_801D7EBC`, 0x108, immediately above). `total_functions` in
`build/report.json` is still **28465** after the `splits.txt` edit. One function, so declaration
order cannot be wrong (`check_decl_order.py --unit` reports "0 unit(s) checked").

## Measured

- `tools/decomp_build.sh` -> `All: 37.69% fuzzy, 31.13% matched, 14.00% linked (13621 / 28465 functions)`
- `build/report.json`, unit `main/MetroidPrime/Weapons/GunController/Carve801D7E9C`:
  `fuzzy_match_percent 100.0`, `total_functions 1`, `matched_functions 1`,
  `matched_functions_percent 100.0`, `metadata.complete true`
- judge counts: **matched 13620 -> 13621, linked 6668 -> 6669**
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged)
- `python3 tools/check_symbol_names.py` -> `checked 610 units; 0 declared names are missing from their object`
- `tools/unit_fit.sh MetroidPrime/Weapons/GunController/Carve801D7E9C.c` ->
  `.text claimed 32 ours 32 retail 32 fits` + `no extra functions`
- `tools/flip_test.sh MetroidPrime/Weapons/GunController/Carve801D7E9C.c` ->
  `PASS  -> kept as Matching`, `kept: 1 / 1   failed: 0   skipped: 0`
- port link (`build/gate-linkcheck.log`): `compile errors 0`, `unique undefined symbols 286`
  (the judge's baseline `build/goal/judge/undef.base.count` is **286**, so it did not move),
  `duplicate definitions 0`
- `./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS carve-801d7e9c`**, every
  check `ok`: no judge-owned path touched, gate.sh, counts, check_symbol_names, the `All:` line, and
  `flip_test ...: PASS, Object(Matching) in configure.py`.

`tools/carve_diff.sh 0x801D7E9C 0x20 build/G2ME01/src/.../Carve801D7E9C.o` reports 8 instructions /
32 bytes on **both** sides and exactly one differing instruction, the `bl`:

```
  +3   retail: 801d7ea8 bl 801db2c0 <Update__10CGunWeaponFfR13CStateManager>
       ours  : 0000000c bl c      <fn_801D7E9C+0xc>
```

That is the `R_PPC_REL24` relocation, unresolved in the `.o` by construction -
`objdump -r` on our object shows `0000000c R_PPC_REL24 Update__10CGunWeaponFfR13CStateManager` and
nothing else - and it resolves to the right address at link time, which is what objdiff's 100.00%
and `flip_test`'s PASS measure. **A carve whose only call is external therefore always prints
"NOT byte-exact" from `carve_diff.sh`; the acceptance test is `flip_test.sh`, not this script.**

## What the function is, and how each leg was measured

**`fn_801D7E9C` is retail's unnamed `CGunWeapon::Update(float, CStateManager&)`**, a frame and one
`bl` to `Update__10CGunWeaponFfR13CStateManager` at 0x801DB2C0. Four independent measurements:

1. **The callee is retail-named.** `config/G2ME01/symbols.txt:7661`
   `Update__10CGunWeaponFfR13CStateManager = .text:0x801DB2C0; // type:function size:0x26C`, and the
   `bl` displacement in the bytes (`48 00 34 19` at 0x801D7EA8, re-read with
   `python3 tools/dol_read.py 0x801D7E9C 0x20`) lands on 0x801DB2C0.
2. **The function is a vtable slot, and the slot is 0x2C.** 0x801D7E9C occurs **exactly once** in
   `main.dol` (searched as a big-endian word), at file 0x3B4344 = `.data` 0x803B7264 (with
   `.data` 0x803B0C00@0x3ADCE0). Reading the table at 0x803B7238 out of the disc gives 21 entries
   that are `include/MetroidPrime/Weapons/CGunWeapon.hpp:66-107`'s `virtual` list in declaration
   order, and **twelve slots in** - base + 0x2C - is 0x801D7E9C. The header's slot twelve
   (line 93) is `virtual void Update(float dt, CStateManager& mgr);`.
3. **A second table pins 0x2C without the header.** `CPowerBeam`'s table starts at 0x803B71D8 - its
   base slot is `__dt__10CPowerBeamFv` (0x801D5AD8, `symbols.txt:7573`) - and its base + 0x2C is
   `Update__10CPowerBeamFfR13CStateManager` (0x801D5528, `symbols.txt:7568`), a retail-named
   override of the same slot. Two tables, same offset, one of them named.
4. **Retail's own source says the base implementation is a forwarder.**
   `src/MetroidPrime/Weapons/CPowerBeam.cpp:129-131`, in a `Matching` unit, is
   `void CPowerBeam::Update(float dt, CStateManager& mgr) { CGunWeapon::Update(dt, mgr); ...` and
   the callee those eight instructions call is exactly that base method.

The signature is measured, not assumed: there is no register move anywhere in the eight
instructions, so `r3` (`this`), `f1` (the float) and `r4` (the `CStateManager&`) are already in the
registers the callee wants - a qualified base call passing all three through untouched. Hence
`extern void Update__10CGunWeaponFfR13CStateManager(void* self, float dt, void* mgr);` and a
one-line body. The first spelling compiled to the byte-exact object on the first attempt; no
alternative spelling was needed.

The shape twin named in the brief, `fn_80004438` (`src/MetroidPrime/Carve80004438.c`, `Matching`),
is these eight instructions with a different `bl` target, as are `fn_80004C4C`, `__sys_free`
(0x80008A28, `src/MetroidPrime/main.cpp`) and `fn_8016F69C`
(`src/MetroidPrime/Carve8016F69C.c`) - four copies now of "0x20-byte frame, save LR, one call,
restore, return". **That measures the frame, not the identity**: a `rstl::destroy` looks the same,
and it is legs 1-4 above, not the shape, that make this one an `Update`.

## Why the PortLinkStubs entry, and what it is

The callee is supplied in the DOL by `MetroidPrime/Weapons/CGunWeapon.cpp`'s own object
(`nm build/G2ME01/obj/MetroidPrime/Weapons/CGunWeapon.o` ->
`0000306c T Update__10CGunWeaponFfR13CStateManager`), so the carve adds **no** undefined symbol to
the DOL link. The **host** link needs one: `build-port-link/.../CGunWeapon.cpp.o` defines
`_ZN10CGunWeapon6UpdateEfR13CStateManager` (Itanium), while a plain C definition site emits the `bl`
against MWCC's spelling, which nothing on the host provides. Without the stand-in the port's gap
would grow by exactly that name and the gate's `port link gap` step would fail.
`stub_carve801d7e9c_0` is an **empty-body announced stand-in** - it does not reproduce those 620
bytes and does not claim to; the header says so in the file's own words. This file is not in
`configure.py`, so it cannot reach `main.dol`. Same trade `stub_80004438_0` makes for
`Carve80004438.c` and `stub_carve8016f69c_0` for `Carve8016F69C.c`. There was **no** duplicate to
remove: `grep -rn 801D7E9C src/ include/` found nothing before this change, and the gate's
`duplicate definitions 0` confirms none after.

Derived terms of `PortLinkStubs.cpp`, measured either side of the append rather than carried:
`grep -cE 'asm\("'` **213 -> 214**; `grep -cE 'asm\("(fn_|lbl_)'` unmoved at **49** (MWCC's
spelling is not an `fn_`/`lbl_` placeholder); `^extern "C" char stub_data_*` unmoved at **10**;
`^extern "C" void stub_[A-Za-z0-9_]*\(\) asm\(` unmoved at **201** (this one takes parameters).

## Nothing filed for the queue

No queue item is filed from this run. Nothing new is blocked here. The rest of the `auto_03_801D72D0_text` gap
(0x801D72D0..0x801D7E9C and 0x801D7EBC..0x801D8248) is **not** twin carves: those are 0x138-0x7E0
byte `CGunWeapon` methods, not frame-and-one-call forwarders, so a `goal_seed.py` twin pairing will
not find them, so a carve item for them would be a requeue with no expected yield. The
neighbour worth a future `match`/`progress` item is `fn_801D7E9C`'s own callee,
`Update__10CGunWeaponFfR13CStateManager` (0x801DB2C0, **620 bytes**), which is a real
`CGunWeapon::Update` body inside the already-claimed but `NonMatching`
`MetroidPrime/Weapons/CGunWeapon.cpp` - that is a `progress` item on a unit someone may be working,
so it is left to the driver to queue rather than filed from a carve lane.