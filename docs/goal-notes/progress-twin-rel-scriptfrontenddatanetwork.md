# progress-twin-rel-scriptfrontenddatanetwork

`twin` item on `module:ScriptFrontEndDataNetwork` (module 59). **PASS.** The module's summed
`matched_functions` went **17 -> 32 of 105**; the project's went **13199 -> 13214 of 28465**
(`linked` 6247 -> 6262). Module sha1 unchanged and equal to `config/G2ME01/config.yml`:
`583529f588a342f34238cea369553ab662762fac`; all **86 RELs OK**, `main.dol`
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; `./tools/goal_check.sh build/goal/item.json` ->
`PASS progress-twin-rel-scriptfrontenddatanetwork`.

## What landed (two claims, one new unit each side of the module's own code)

**1. `MetroidPrime/ScriptObjects/CFrontEndDataNetworkRel.cpp` (new), `.text 0x1B8..0x3F4`, ten
functions, 10/10 at 100.00%.** This is the item's named run, taken whole. Before the claim all ten
were 0% (they sat in `ScriptFrontEndDataNetwork/auto_00_00000000_text`, which reported 0 of 68
matched); after it the unit reports 100.00% fuzzy and every function is exact.

| addr | name | size | what it is | twin used | spelling |
| --- | --- | --- | --- | --- | --- |
| 0x1B8 | `fn_59_1B8` | 0x20 | `rstl::destroy<T>(T*)` | `fn_801FD638` (Carve801FD638.c) | unchanged |
| 0x1D8 | `fn_59_1D8` | 0x24 | `rstl::destroy_impl<T>(T*)` | `fn_801FD658` (same file) | unchanged |
| 0x1FC | `fn_59_1FC` | 0x58 | deleting dtor, member at +0xC | `__dt__11CMayaSplineFv` (Carve80032774.cpp) | same body, +0xC instead of +0x8 |
| 0x254 | `fn_59_254` | 0x58 | deleting dtor, member at +0x8 | `__dt__11CMayaSplineFv` | unchanged |
| 0x2AC | `fn_59_2AC` | 0x54 | `~vector`-shaped: `Free(*(void**)(self+0xC))` | `fn_80032854` (Carve80032774.cpp) | unchanged |
| 0x300 | `fn_59_300` | 0x54 | the same destructor, second instantiation | `fn_80032854` | unchanged |
| 0x354 | `fn_59_354` | 0x2C | vtable entry 0x3C -> slot 0x38 | `fn_2_9C` (CAtomicAlphaRel.cpp) | unchanged (13-virtual class) |
| 0x380 | **`RELExit`** | 0x24 | `fn_8021FA18(0)` | `RELExit` (CAtomicAlphaRel.cpp) | unchanged |
| 0x3A4 | **`RELMain`** | 0x20 | `fn_59_3C4()` | `RELMain` (CAtomicAlphaRel.cpp) | unchanged |
| 0x3C4 | `fn_59_3C4` | 0x30 | loader slot store + `fn_8021FA18(&slot)` | `fn_2_10C` (CAtomicAlphaRel.cpp) | unchanged |

**2. `REL/REL_Setup.cpp`, `.text 0x72C0..0x7464` + `.rodata 0xA8..0x12C`, 5 functions, 5/5 at
100.00%** - the free "claim the tail" win every module gets (`_unresolved` 0xC4, `_epilog` 0x24,
`_prolog` 0x24, `ModuleDestructors` 0x4C, `ModuleConstructors` 0x4C). Not named in the `Rel(...)`
block: `configure.py` already carries the shared "REL" lib, and a second entry is
`Duplicate object name`.

`files.cmake`: **not touched.** The new file defines `RELMain`/`RELExit`, which is
`check_files_cmake.py`'s counted-and-skipped case - it now prints 48 such units, was 47. Adding it
to `files.cmake` would make the host link ask for `fn_59_3F4`, which nothing implements.

## The one thing that had to be right, and it failed loudly first

The claim **and the `symbols.txt` renames are one change, not two.** With only the claim added, the
link dies:

```
Failed: While resolving relocations in 'build/G2ME01/ScriptFrontEndDataNetwork/ScriptFrontEndDataNetwork.plf'
Caused by:
    Failed to find symbol fn_59_380 in any module
```

because the still-retail `_epilog` in the unclaimed tail `bl`s the module's entry points. The
renames that fix it (each replacing its `fn_` line, never inserted beside it) are
`fn_59_380 -> RELExit`, `fn_59_3A4 -> RELMain`, `fn_59_73CC -> ModuleDestructors`,
`fn_59_7418 -> ModuleConstructors`, all `scope:global`. `RELExit`/`RELMain` are what `_epilog`/
`_prolog` resolve against; without `scope:global` the module grows 48 bytes of relocations.

## Measured, so the next run does not have to re-derive it

- **No `force_active` was needed.** `powerpc-eabi-objdump -r` over the module's `auto_*` objects
  before the claim: `fn_59_1B8` <- `fn_59_168` (0x18C) and `fn_59_6E28` (0x6E4C); `fn_59_254` <-
  `fn_59_0` four times (0x40/0x4C/0x58/0x64) and eight more above 0xAC4; `fn_59_1FC` <- `fn_59_1D8`
  plus 0x52E0 and 0x55D8; `fn_59_354` <- the `.data` vtable `lbl_59_data_8` at +0x3C;
  `RELExit`/`RELMain` <- `_epilog` 0x7394 / `_prolog` 0x73B8. `tools/audit_rel_claim.py
  ScriptFrontEndDataNetwork` agrees: 3 claims, 0 problems, `105 text symbols, plf 105, 0 dropped by
  -strip_partial`.
- **`mw_version` is the module default `GC/1.3.2` and no per-object override was needed** - all ten
  matched at 100% on the first build, including the four destructor-shaped bodies whose twins were
  compiled by the DOL's `GC/2.7`. The SandBoss `GC/2.7` warning did **not** apply here.
- `python3 tools/check_decl_order.py --unit CFrontEndDataNetworkRel` -> ok.
  `./tools/unit_fit.sh MetroidPrime/ScriptObjects/CFrontEndDataNetworkRel.cpp` -> `.text claimed
  572 ours 572 retail 572 fits`, `no extra functions`.
- `python3 tools/check_symbol_names.py` -> 585 units (was 584), 0 missing names.

## Left for the next run (measured, not attempted)

The item's other 20 twins are isolated, not runs, and each would need its own file. The cheapest
**run** left is `.text 0x5704..0x5744`, four functions, all readable off
`build/G2ME01/ScriptFrontEndDataNetwork/asm/auto_00_000003F4_text.s`:

- `fn_59_5704` (0xC): `lhz r0,0x15a(r4); sth r0,0(r3); blr` - halfword out-param at +0x15A of the
  source; its caller at 0x4A20 passes `r3 = r1+0x18`.
- `fn_59_5710` (0xC): `lhz r0,0(r4); sth r0,0x158(r3); blr`.
- `fn_59_571C` (0x8): `li r3,0; blr`; in the vtable `lbl_59_data_8` at +0x30.
- `fn_59_5724` (0x20): frame + `bl EnsureRendered__6CActorCFRC13StateManager`; vtable +0x28.

That unit would **not** define RELMain/RELExit, so it needs the `files.cmake` +
`#ifdef __MWERKS__`-with-empty-host-branch arrangement `CSandBossRelTail.cpp` uses, which is why it
was not taken here. Above it `fn_59_5744` (0x5FC) and below `fn_59_5578` (0x18C) are unclaimed.

Not attempted, and the reasons are structural rather than measured: `fn_59_3F4` (0x6D0, the
module's entity loader) needs the `CEntity`/`CPatterned` hierarchy; `fn_59_4E60`/`fn_59_5478`
(0x100 each) are `rstl::vector` copies; `fn_59_6438`/`fn_59_64E4` (0xAC/0x1D0) are `CMayaSpline`
copies; `fn_59_2DAC` (0x84) and `fn_59_6BC4` (0xA4) are `reserve`s.

No `WALL:` line: nothing was left at a sub-100% score. No `NEW:` filed - the remaining work is the
same target this item already names, and the driver requeues a `progress` item by itself.

Note for the driver: `docs/HANDOFF.md` shows as modified in `git status` - that is
`goal_check.sh`'s own `MP_GATE_DOCS_WRITE=1 ./tools/gate.sh` rewriting the derived counts (matched
13199 -> 13214, linked 6247 -> 6262, REL units 1664 -> 1679, our own units in modules 119 -> 120),
not an edit of mine.
