# carve-80218aa0 (match) — DONE

Goal item `carve-80218aa0`, target `MetroidPrime/ScriptLoader/Carve80218AA0`. Wrote `fn_80218AA0`
(0x80218AA0, 8 bytes) as a new `Matching` carve unit. **One function matched, one unit flipped.**
The judge passed:

```
goal_check: item carve-80218aa0 (match) target=MetroidPrime/ScriptLoader/Carve80218AA0
goal_check: PASS carve-80218aa0
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13566 -> 13567   linked 6614 -> 6615
  ok    check_symbol_names.py
  ok    All:  37.68% fuzzy, 31.11% matched, 13.98% linked (13567 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptLoader/Carve80218AA0.c: PASS, Object(Matching) in configure.py
```

## What the bytes are

`build/G2ME01/asm/auto_03_80218AA0_text.s` (dtk's own listing, while the range was unclaimed):

```
# 0x80218AA0..0x80218AA8 | size: 0x8
.fn fn_80218AA0, global
/* 80218AA0 002158A0  90 6D 96 80 */  stw r3, gLoader_MinorIng@sda21(r0)
/* 80218AA4 002158A4  4E 80 00 20 */  blr
```

It is the byte-shape twin the item named: `stw r3, <slot>@sda21(r0)` / `blr`, the same two
instructions as the already-`Matching` `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c`,
with one slot different. Its three nearest relatives in the DOL are the same shape with one slot
each: `fn_80218918` (`gLoader_Ings`, `Carve80218918.c`), `fn_802188E4` (`gLoader_DarkSamus`,
`Carve802188E4.c`) and `SetLoader_Sandworm__FP18SSandworm_FuncPtrs` (`gLoader_Sandworm`,
`Carve8021887C.c`).

The body written is the same one-word store and return:

```c
struct SMinorIngLoaderRecord;
extern struct SMinorIngLoaderRecord* gLoader_MinorIng;

void fn_80218AA0(struct SMinorIngLoaderRecord* loader) { gLoader_MinorIng = loader; }
```

## Measured, not recalled

- `config/G2ME01/symbols.txt:9478` — `fn_80218AA0 = .text:0x80218AA0; // type:function size:0x8 align:4`.
- `config/G2ME01/symbols.txt:20713` — `gLoader_MinorIng = .sbss:0x80419400; // type:object size:0x8 data:4byte`.
- The slot is **claimed and defined by `MinorIng.cpp`**, not by this unit: `config/G2ME01/splits.txt:1653-1655`
  gives it `.text 0x80218A74..0x80218AA0` and `.sbss 0x80419400..0x80419408`, and
  `src/MetroidPrime/ScriptLoader/MinorIng.cpp:16` defines `SLoaderSlot gLoader_MinorIng;`. So this
  carve claims **`.text` only** and takes the slot `extern` — the same shape as
  `Carve80218918.c`/`gLoader_Ings`.
- **The name is retail's and unmangled, so the unit is `.c`.** `strings build/G2ME01/MinorIng/MinorIng.plf | grep 80218AA0`
  returns the plain `fn_80218AA0`; the module imports that exact symbol, so it cannot be renamed and
  an alias would resolve to nothing. A `.cpp` would mangle it. This matches the plain-`fn_` import
  `configure.py:2855-2857` already records for the `MinorIng` module.
- Both module call sites, from `build/G2ME01/MinorIng/asm/auto_00_00000000_text.s`:
  `RELExit` at module `.text:0x9C` (line 99 `li r3, 0x0`, line 101 `bl fn_80218AA0`) and
  `fn_44_E0` at module `.text:0xE0` (line 128 `stwu r0, lbl_44_bss_84@l(r3)`, line 129 `bl fn_80218AA0`).
  `lbl_44_bss_84` is 4 bytes (`build/G2ME01/MinorIng/asm/auto_05_00000000_bss.s:55-57`, `.skip 0x4`).
- The one reader is `MinorIng.cpp:19`, `(*gLoader_MinorIng.value)(mgr, input, info)` — which is why
  the store hands over the **address** of a slot holding one loader pointer, not a loader.
- The claim is bounded on both sides by claimed units, so it spans no gap: `MinorIng.cpp` ends at
  0x80218AA0 below and `ElitePirate.cpp` starts at 0x80218AA8 above (`splits.txt:1661-1663`).

## Verification (all run in this lane)

- `./tools/decomp_build.sh MetroidPrime/ScriptLoader/Carve80218AA0.c` — `[10/13] CHECK config/G2ME01/build.sha1`
  printed `87 files OK`; `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`,
  the pinned retail hash. So all 86 RELs still match `orig/G2ME01/files/RelProd/`, including
  `MinorIng`, whose sha1 in `config/G2ME01/config.yml` the build re-checked.
- `build/report.json`: `total_functions` **28465, unchanged**; `matched_functions` 13566 -> **13567**;
  `complete_units` 992 -> **993**; the new unit `main/MetroidPrime/ScriptLoader/Carve80218AA0` is
  100.0 on `fuzzy_match_percent`, `matched_functions_percent` and `matched_code_percent`. No other
  unit moved: `MinorIng`, `ElitePirate`, `MinorIng/MetroidPrime/ScriptObjects/CMinorIngRel` all still 100.0.
- `./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve80218AA0.c` — `PASS -> kept as Matching`,
  `kept: 1 / 1   failed: 0   skipped: 0`.
- `python3 tools/check_symbol_names.py` — `checked 608 units; 0 declared names are missing from their object`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptLoader/Carve80218AA0` — `ok: 1 unit(s)
  checked, none emits its functions out of retail order`. (Pass `--unit` **without** the `.c`: the
  filter matches the `build/report.json` unit name `main/MetroidPrime/ScriptLoader/Carve80218AA0`,
  so `--unit ...Carve80218AA0.c` silently checks 0 units and still prints `ok`.)
- `./tools/goal_check.sh build/goal/item.json` — **PASS**, output quoted at the top.

## Byte-exactness, and a caveat about `carve_diff.sh`

`tools/carve_diff.sh 80218AA0 8 build/G2ME01/.../Carve80218AA0.o fn_80218AA0` prints **NOT byte-exact**
for this carve — and that is a limitation of the tool on this shape, not a defect in the unit. Both
`.o` trees a build leaves behind (`build/G2ME01/obj/...` and `build/G2ME01/src/...`) are pre-link
ELFs whose `stw` displacement is still a relocation, so objdump prints
`stw r3,0(0)` against retail's `stw r3,-27008(r13)` and every comparison differs at +0. The same
run against `build/G2ME01/main.elf` is meaningless for a different reason: that file starts its
instruction list at the section's own base, not at 0x80218AA0.

The measurements that *are* decisive here, both run here:

- the pinned DOL sha1 above — the whole `main.dol`, including these 8 bytes, is byte-identical to retail;
- `powerpc-eabi-objdump -d main.elf --start-address=0x80218AA0 --stop-address=0x80218AA8` gives
  `90 6d 96 80 / 4e 80 00 20`, and `nm -n main.elf` puts `80218aa0 T fn_80218AA0` — the same 8 bytes
  and the same address dtk's listing records, compared programmatically (not by eye).

So `carve_diff.sh` should be read as *uninformative* for any carve that stores through an extern
global; the DOL hash plus the flip is the acceptance test. **No `NEW:` filed for this** — it is a
tooling limitation, not a blocker, and the carve vein already says only `flip_test.sh` decides.

## Files

The four a carve requires, plus the two comments that said the setter was deliberately unclaimed:

- `config/G2ME01/splits.txt:1657-1658` — `MetroidPrime/ScriptLoader/Carve80218AA0.c` /
  `.text start:0x80218AA0 end:0x80218AA8`, inserted between the `MinorIng.cpp` block (`:1653-1655`)
  and `ElitePirate.cpp` (`:1661`), i.e. in address order. `total_functions` re-checked at 28465.
- `configure.py:860` — `Object(Matching, "MetroidPrime/ScriptLoader/Carve80218AA0.c"),` on one line,
  between `MinorIng.cpp` (`:859`) and `ElitePirate.cpp` (`:861`).
- `files.cmake:875-878` — the path plus a three-line comment, in address order between
  `Carve80218918.c` (0x80218918) and `Carve802201F8.c` (0x802201F8).
- `src/MetroidPrime/ScriptLoader/Carve80218AA0.c` — new, 89 lines: the header in the style of
  `src/Dolphin/Carve8038A7DC.c` and `Carve80218918.c`, then the two-line body.
- `src/MetroidPrime/ScriptLoader/MinorIng.cpp:6-9` — the stale "the 8-byte setter at 0x80218AA0 is
  deliberately NOT claimed ... must stay in dtk's auto unit" is now false; rewritten the way
  commit `21237571` did for `DarkSamus.cpp`.
- `src/MetroidPrime/ScriptObjects/CMinorIngRel.cpp:84-92` — one clause went stale with it ("the file
  that already records why this setter is deliberately not claimed in the DOL"); now names the carve
  unit and keeps every other measured claim.

No `PortLinkStubs.cpp` duplicate: `grep -n "80218AA0" src/MetroidPrime/PortLinkStubs.cpp` returns
nothing (exit 1), and `gate.sh`'s `port link dups` step passed inside `goal_check.sh`.

The port compile count moved 967 -> 968 files (`files.cmake` line), which `gate.sh` re-derived into
`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md`. **Those two edits are `gate.sh`'s own, not mine** —
the brief says the driver discards agent edits to them and re-derives those counts itself.

## For the next run

The four sibling setters in this run are **already queued**, so there is nothing to file:
`build/goal/queue.json` carries `carve-80218a04`, `carve-80218a38`, `carve-80218a6c`, `carve-80218ad4`,
`carve-80218b08`, `carve-80218b68`, `carve-80218bc8`, `carve-802188b0` and `carve-802187e4`, all
`kind: match` on `MetroidPrime/ScriptLoader/Carve<addr>`. Each is the identical two-instruction carve:
take the owner's `.sbss` slot `extern`, keep the `.c` extension so the retail import name stays
unmangled, claim `.text` only, and check that the address is bounded on both sides by claimed units —
0x80218A04/0x80218A38/0x80218A6C/0x80218AD4 are all of that kind here.