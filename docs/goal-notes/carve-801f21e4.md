# carve-801f21e4 (match) - target `MetroidPrime/Carve801F21E4`

**`goal_check.sh build/goal/item.json`: PASS (exit 0)**, run on the final tree:

```
goal_check: item carve-801f21e4 (match) target=MetroidPrime/Carve801F21E4
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13624 -> 13625   linked 6672 -> 6673
  ok    check_symbol_names.py
  ok    All:  37.70% fuzzy, 31.13% matched, 14.00% linked (13625 / 28465 functions)
  ok    flip_test MetroidPrime/Carve801F21E4.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801f21e4
```

Not `STALE`: on the clean tree `build/report.json` had **no** `MetroidPrime/Carve801F21E4` unit at
all, and `main/auto_03_801F0D24_text` carried 16 functions / 8488 B with **no** `matched_functions`
key (i.e. 0).

## What I claimed

`.text 0x801F21E4..0x801F2204`, **0x20 = 32 bytes, 1 function** - exactly the run the item asked for:

| fn | addr | size | instructions | what it is |
| --- | --- | --- | --- | --- |
| `fn_801F21E4` | 0x801F21E4 | 0x20 | 8 | `CEnergyProjectile::Touch`, a pure forwarder to `CGameProjectile::Touch` |

Retail's bytes re-read from the disc, `tools/dol_read.py 0x801F21E4 0x20`:

```
94 21 ff f0  stwu r1,-0x10(r1)      80 01 00 14  lwz r0,0x14(r1)
7c 08 02 a6  mflr r0                7c 08 03 a6  mtlr r0
90 01 00 14  stw r0,0x14(r1)        38 21 00 10  addi r1,r1,0x10
4b e4 38 e9  bl 80035ad8 <Touch__15CGameProjectileFR6CActorR13CStateManager>
4e 80 00 20  blr
```

`build/report.json`: `main/MetroidPrime/Carve801F21E4` **1 / 1 functions, 100.00% fuzzy,
total_code 32, complete_units 1, `metadata.complete = true`**. `total_functions` still **28465**;
`total_units` 2428 -> 2430.

The claim splits dtk's unclaimed run, and the cut lands on function boundaries:

| unit | before | after |
| --- | --- | --- |
| `main/auto_03_801F0D24_text` | 16 fn / 8488 B | **15 fn / 5312 B** |
| `main/MetroidPrime/Carve801F21E4` | - | **1 fn / 32 B** |
| `main/auto_03_801F2204_text` | - | **15 fn / 3144 B** |

`fn_801F2168` (0x801F2168, 0x7C) ends on the `blr` at 0x801F21E0 and `fn_801F2204` (0x801F2204,
0x9C) starts at 0x801F2204, so neither neighbour run is cut mid-function. gate.sh's report diff
calls it `SPLIT main/auto_03_801F0D24_text: 16 function(s) moved into
main/MetroidPrime/Carve801F21E4, main/auto_03_801F2204_text (exact count match - a split, not a
loss)`.

## The carve: four files, address order, plus one port-link stand-in

| file | entry |
| --- | --- |
| `config/G2ME01/splits.txt:1422-1423` | `MetroidPrime/Carve801F21E4.c: .text start:0x801F21E4 end:0x801F2204` (after `CRELFileManager.cpp` 0x801F0D24, before `Carve801F3690.c` 0x801F3690) |
| `configure.py:818` | `Object(Matching, "MetroidPrime/Carve801F21E4.c"),` **on one line**, between `Carve801EF84C.cpp` and `Carve801F3690.c` |
| `files.cmake:836` | `src/MetroidPrime/Carve801F21E4.c` (after `src/MetroidPrime/Carve801EF84C.cpp`, before `src/MetroidPrime/Carve801F3690.c`) |
| `src/MetroidPrime/Carve801F21E4.c` | the source, one definition |
| `src/MetroidPrime/PortLinkStubs.cpp:1987-2010` | `stub_carve801f21e4_0` - the host-link binding, see the port section (comment 1987-2006, `asm` label 2008-2009, body 2010) |

No `PortLinkStubs.cpp` duplicate existed: `grep -rn 'fn_801F21E4' src/ include/` returned only this
carve, and the symbol is in neither `PortLinkStubs.cpp` nor `PortMwccNew.cpp`.

## How the function was identified (measured, not recalled)

- **It is only ever reached through a vtable.** `grep -rn 801F21E4 build/G2ME01/asm/` returns
  exactly two `.fn`/`.endfn` pairs - the same range seen from dtk's two stale `auto_*` dumps
  (`auto_03_801F0A74_text.s:1684`, `auto_03_801F0D24_text.s:1488`) - and the only reference is
  `.4byte fn_801F21E4` at `auto_07_803B78F0_data.s:29`. **There is no `bl` to it anywhere.**
- **+0x4C in that table is the `Touch` slot, and it is read off the tables, not off a header.**
  `lbl_803B78F0` (`.data:0x803B78F0`) holds `__dt__`, `TypesMatch`, `PreThink`, `Think`,
  `AcceptScriptMsg`, `SetActive`, `ClearFluidList`, `PreRender`, `AddToRenderer`, `Render`,
  `CanRenderUnsorted`, `PreRenderAllViewports`, `HealthInfo`, `GetHealthInfo`,
  `GetDamageVulnerability` (no args), `GetDamageVulnerability` (3 args), `GetTouchBounds` (+0x48,
  `fn_801F2204`), **`fn_801F21E4` (+0x4C)**, `GetOrbitPosition` (+0x50), ... **52** tables in `.data`
  hold `Touch__6CActorFR6CActorR13CStateManager` (0x8004C564) in that position, and in every one the
  word after it is a `GetOrbitPosition` entry (48 the base, four a derived override) while the word
  before it is a `GetTouchBounds` in 45 - `CActor`'s own table, identified by
  `TypesMatch__6CActorCFi` at 0x803B8820, has `GetTouchBounds` at 0x803B885C, `Touch` at 0x803B8860 and
  `GetOrbitPosition` at 0x803B8864. 51 independently identified tables in the same relative position
  is what fixes the slot, and it is the slot `include/MetroidPrime/CActor.hpp:93` declares as
  `virtual void Touch(CActor&, CStateManager&)`.
- **The class is `CEnergyProjectile`, from the same table.** `+0x24` is
  `PreRender__17CEnergyProjectileFR13CStateManager`, `+0x28`
  `AddToRenderer__17CEnergyProjectileCFRC13CStateManager`, `+0x2C` `Render__17CEnergyProjectile...`,
  `+0x34` `PreRenderAllViewports__17CEnergyProjectile...`, `+0x6C` `GetSortingBounds__17CEnergyProjectile...`
  and `+0x7C` `StopProjectile__17CEnergyProjectileFR13CStateManager` - all named in
  `include/MetroidPrime/Weapons/CEnergyProjectile.hpp:30,33` - while `+0x64` and `+0x80` are the two
  `CGameProjectile` entries this class does **not** override
  (`FluidFXThink__15CGameProjectileF...`, `RayCollisionCheckWithWorld__15CGameProjectileF...`).
- **The callee settles the rest.** `powerpc-eabi-objdump -d build/G2ME01/main.elf` at 0x801F21F0
  prints `bl 80035ad8 <Touch__15CGameProjectileFR6CActorR13CStateManager>`; `symbols.txt:1011` gives
  0x80035AD8 `size:0x60`. Those 96 bytes are retail's `CGameProjectile::Touch` work - a frame,
  `bl Touch__6CActorFR6CActorR13CStateManager`, `mr r3,r31` + `bl TCastToPtr<11CScriptDock>`, the
  two-id compare and `sth r0,1024(r30)` - which is
  `src/MetroidPrime/Weapons/CGameProjectile.cpp:87-94` (`CActor::Touch`, the dock cast, `mTouchedDock`
  at +0x400). No register move precedes the `bl` in this carve, which is what a qualified base call
  produces. The bytes are inside `MetroidPrime/Weapons/CGameProjectile.cpp`'s existing claim
  (0x80032C3C..0x80036200, `NonMatching`), so dtk's object supplies them in the DOL link and the
  carve **declares** the name and claims nothing.
- **The twins.** The brief's twin `fn_80004438` (0x80004438, 0x20,
  `src/MetroidPrime/Carve80004438.c`, `Matching`) is these eight instructions with a different `bl`
  target, and so is `__sys_free` (0x80008A28, `src/MetroidPrime/main.cpp`). The closer match is
  `fn_8016F69C` (0x8016F69C, 0x20, `src/MetroidPrime/Carve8016F69C.c`, `Matching`): the same eight
  instructions, a `Touch` vtable override forwarding to the class one step up, there
  `CActor::Touch`, here `CGameProjectile::Touch`.

## Verified (all measured this run, in this tree)

```
sha1sum build/G2ME01/main.dol            6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (our object is in the link)
./tools/decomp_build.sh MetroidPrime/Carve801F21E4.c   All: 37.70% fuzzy, 31.13% matched (13625 / 28465)
./tools/flip_test.sh MetroidPrime/Carve801F21E4.c      PASS -> kept as Matching   kept: 1/1 failed: 0 skipped: 0
./tools/unit_fit.sh MetroidPrime/Carve801F21E4.c       claimed 32, ours 32, retail 32, fits; no extra functions
python3 tools/check_decl_order.py --unit Carve801F21E4  1 unit(s) checked, none emits its functions out of retail order
python3 tools/check_symbol_names.py                    checked 610 units; 0 declared names are missing
python3 tools/check_files_cmake.py                     every configured DOL object is in files.cmake or excluded with a reason
./tools/carve_diff.sh 0x801F21E4 0x20 build/G2ME01/obj/MetroidPrime/Carve801F21E4.o fn_801F21E4
      8 vs 8 instructions; 1 differing (the bl word)  |  objdump -r: R_PPC_REL24 Touch__15CGameProjectileFR6CActorR13CStateManager at 0x0c
```

`carve_diff.sh`'s single differing instruction is the `bl` word, which in an **unlinked** object
still holds `R_PPC_REL24` and therefore reads as 0. The matching `main.dol` sha1 is what proves the
branch resolves to 0x80035AD8. Byte-exactness otherwise. Source order is descending by address and
was checked, not assumed; with one function the file cannot get it wrong.

## The port link: one announced stand-in, measured both ways

The carve's `bl` opens exactly one symbol nothing on a host link defines, and
`src/MetroidPrime/PortLinkStubs.cpp` - next to the `Touch__6CActorFR6CActorR13CStateManager` stand-in
`fn_8016F69C` asks for - gained `stub_carve801f21e4_0` under an `asm(...)` name label, keyed to the
asking unit as that file's convention requires.

**This one is an empty body and it does lose work; the reason is that the port has no such class.**
Measured, not assumed: `grep -n 'Weapons/CGameProjectile' files.cmake` is **empty** - the port does
not compile `CGameProjectile.cpp` at all, which is also why `CGameProjectile::GetBeamAttribType` sits
in the link as an undefined symbol - and `nm` over all **1663** objects of `build-port-link` names
`CGameProjectile::Touch` in **none** of them. So there is no host implementation to forward to, and
the alternative carve-801708c4 used for `AddToRenderer__6CActorCFRC13CStateManager` (a real
forwarder into the host's own `CActor::AddToRenderer`) does not exist here: a wrapper calling the
host `CGameProjectile::Touch` would only move the undefined reference to a mangled name. The
stand-in says so in its own comment, as the rule requires.

- `tools/link_check.sh --strict` **with** the stand-in: `286 undefined, 0 duplicates, 0 compile
  errors` - the judge's `build/goal/judge/undef.base.count` reads **286**, so nothing grew.
- **Counterfactual, measured the other way round**: the same tree with the carve in place and the
  stand-in **not yet added** reported `unique undefined symbols 287`, with the log naming
  ``undefined reference to `Touch__15CGameProjectileFR6CActorR13CStateManager'``. The stand-in is
  worth exactly the one symbol the carve opens.

## Note for the next run

`docs/research/port_link_baseline.txt` still says `undefined 287` while the branch head actually
links at **286** (`build/goal/judge/undef.base.count`, and my tree measures 286 with the stand-in) -
the same stale-in-the-safe-direction note `carve-801708c4` recorded. Both files are on the
do-not-edit list for this item, so it is measured here rather than fixed. A lane told to hold the
count at 287 and reading 286 has not broken anything.

## Not done, and why

- No `WALL:` - the one function reached 100.00% / `flip_test` PASS on the first correct build, so
  there is no sub-100% score to report.
- No `NEW:` - nothing measured this run says another function of this run reaches 100%. The rest of
  the run is `fn_801F2168` (0x801F2168, 0x7C bytes, ends on a `blr` at 0x801F21E0) and
  `fn_801F2204` (0x801F2204, 0x9C bytes, which the vtable identifies as `CGameProjectile::GetTouchBounds`
  and whose body is float work over `self`+0x54/+0x568 with an `rstl::optional_object< CAABox >`
  return) - both over the seeder's 64-byte limit, neither with a twin I measured, and both inside
  `auto_03_801F2204_text`, which also holds 14 more unsourced functions.
- No doc was edited by me. `goal_check.sh`'s own gate step rewrote `docs/HANDOFF.md`'s state block
  (3 lines); I reverted that rewrite with `git checkout -- docs/HANDOFF.md`, so the diff is exactly
  the five files listed above.
- Not committed.
