# carve-80218df4 — `MetroidPrime/ScriptLoader/Carve80218DF4`, +1 function (13577 -> 13578)

## Result

One new `Matching` unit, one function, at **100.00%**, verified by `tools/flip_test.sh`:

    $ ./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve80218DF4.c
    TEST MetroidPrime/ScriptLoader/Carve80218DF4.c
      PASS  -> kept as Matching
    kept: 1 / 1   failed: 0   skipped: 0

`./tools/goal_check.sh build/goal/item.json` → **PASS**:

    goal_check: item carve-80218df4 (match) target=MetroidPrime/ScriptLoader/Carve80218DF4
      ok    no judge-owned path touched
      ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
      ok    counts: matched 13577 -> 13578   linked 6625 -> 6626
      ok    check_symbol_names.py
      ok    All:  37.69% fuzzy, 31.12% matched, 13.99% linked (13578 / 28465 functions)
      ok    flip_test MetroidPrime/ScriptLoader/Carve80218DF4.c: PASS, Object(Matching) in configure.py
    goal_check: PASS carve-80218df4

`gate.sh`'s per-function diff line, which is the judge recognising the carve rather than a loss:

    per-function diff   SPLIT   main/auto_03_80218DF4_text: 1 function(s) accounted for
                                across 1 new unit(s) in main (exact count match - a split, not a loss)

## Range claimed

Exactly the seeded range, `.text` only, no `.data`/`.sbss`:

    config/G2ME01/splits.txt
    MetroidPrime/ScriptLoader/Carve80218DF4.c:
    	.text       start:0x80218DF4 end:0x80218DFC

`total_functions` still **28465** after the `splits.txt` edit (measured on the rebuilt report).

## The four files

| file | what |
| --- | --- |
| `src/MetroidPrime/ScriptLoader/Carve80218DF4.c` | new, one function, plain C |
| `config/G2ME01/splits.txt` | 2 lines, `.text 0x80218DF4..0x80218DFC` |
| `configure.py:890-893` | one-line `Object(Matching, "MetroidPrime/ScriptLoader/Carve80218DF4.c")` with its comment |
| `files.cmake:918-921` | the source in `MP_GAME_SOURCES`, with its comment |

Plus a **comment-only** fix to the fifth file, `src/MetroidPrime/ScriptLoader/DarkTrooper.cpp:6-9`:
its header said "The 8-byte setter at 0x80218DF4 is deliberately NOT claimed ... must stay in dtk's
auto unit", which this carve makes false. Reworded to name the new unit, in `SandBoss.cpp:6-10`'s
wording (that file's own carve, `Carve802189D0.c`, had the same comment updated the same way).
No code in `DarkTrooper.cpp` changed. `docs/research/rel_loaders.md` needs no edit: its row 144 for
`DarkTrooper.cpp` describes the thunk at 0x80218DC8, not the setter, so nothing in it went stale.

## What the function is, and every number behind it

`fn_80218DF4` is **DarkTrooper's loader setter** — the twin of the matched `fn_80200E3C` in
`Carve80200E3C.c` (SpacePirate's), as the item said. Measured, not recalled:

* `config/G2ME01/symbols.txt:9508` — `fn_80218DF4 = .text:0x80218DF4; type:function size:0x8 align:4`.
* `build/G2ME01/asm/auto_03_80218DF4_text.s` — the range holds exactly
  `stw r3, gLoader_DarkTrooper@sda21(r0)` (`90 6D 96 E8`) and `blr` (`4E 80 00 20`).
  After the carve, `build/G2ME01/asm/MetroidPrime/ScriptLoader/Carve80218DF4.s` is **byte-identical
  to it, line for line**, at the same two addresses. (This is the better `carve_diff` substitute:
  `tools/carve_diff.sh 0x80218DF4 0x8 build/G2ME01/src/.../Carve80218DF4.o` reads the *unlinked*
  `.o`, where the `@sda21` displacement is still 0, so it reports `NOT byte-exact` on a carve that
  is exact. The linked per-unit `.s` above is the measurement that decides.)
* The `@sda21` displacement really is this slot: `tools/sda.py` gives G2ME01's `_SDA_BASE_` as
  0x8041FD80, and 0x8041FD80 − 0x96E8 = **0x80419468** = `gLoader_DarkTrooper`
  (`config/G2ME01/symbols.txt:20726`, `.sbss`, `size:0x8 data:4byte`). dtk renders the relocation
  as `(r0)` but the encoding is `stw r3, 0x96E8(r13)`; objdump calls it `stw r3,-26904(r13)`.
* Neighbours and their displacements, so the "byte-shape twin" claim is checkable:
  `gLoader_SpacePirate` 0x80419358 = `90 6D 95 D8`, `gLoader_SandBoss` 0x804193E0 = `90 6D 96 60`,
  `gLoader_MetroidAlpha` 0x80419418 = `90 6D 96 98`, `gLoader_DarkTrooper` 0x80419468 = `90 6D 96 E8`.
  All four differ in the 16-bit field only; the `4E 80 00 20` `blr` is common to all four.
* Argument type, read off module 12's own listing
  (`build/G2ME01/DarkTrooper/asm/auto_00_00000000_text.s`): `RELExit` at `.text:0xB8` (0x24 bytes)
  does `li r3,0x0` at 0xC0 then `bl fn_80218DF4` at 0xC8; `fn_12_FC` at `.text:0xFC` (0x30 bytes,
  reached by `bl fn_12_FC` at 0xE8 from `RELMain`) does `lis r4,fn_12_12C@ha` /
  `lis r3,lbl_12_bss_0@ha` / `addi r0,r4,fn_12_12C@l` / `stwu r0,lbl_12_bss_0@l(r3)` and then
  `bl fn_80218DF4` at 0x118 with r3 still pointing at `lbl_12_bss_0`. So the argument is that
  record's address, and the record is **4 bytes**: `config/G2ME01/rels/DarkTrooper/symbols.txt:288`
  gives `lbl_12_bss_0 = .bss:0x00000000; size:0x4 data:4byte`. Hence the bare
  `struct SDarkTrooperLoader { unsigned int loader; }` — the same shape `Carve802189D0.c` uses for
  module 55, and not the 0x1C-byte record of `Carve80200E3C.c`'s module 72, which holds two
  pointers-to-member-function as well.
* The name is the module's own import name: `fn_80218DF4` is in
  `build/G2ME01/DarkTrooper/DarkTrooper.preplf`'s import table, and
  `src/MetroidPrime/ScriptObjects/CDarkTrooperRel.cpp:109` already declares
  `void fn_80218DF4(FScriptLoader* loader);` inside its `extern "C"` block and calls it at both
  sites. MWCC encodes no parameter type in a function name, so the C++ declaration and this C
  definition agree. That file is not in `files.cmake` (module-entry reason, unchanged by this item).

## Other gates, measured

    $ sha1sum build/G2ME01/main.dol
    6ef9b491d0cc08bc81a124fdedb8bfaec34d0010     # the pinned DOL hash, unchanged
    $ ./tools/decomp_build.sh
    87 files OK
    All:  37.69% fuzzy, 31.12% matched, 13.99% linked (13578 / 28465 functions)
    $ python3 tools/check_symbol_names.py
    checked 609 units; 0 declared names are missing from their object
    $ python3 tools/check_decl_order.py --unit main/MetroidPrime/ScriptLoader/Carve80218DF4
    ok: 1 unit(s) checked, none emits its functions out of retail order
    $ python3 tools/check_decl_order.py
    ok: 1249 unit(s) checked, 37 permuted, all 37 accounted for in decl_order.md
    $ ./tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve80218DF4.c
       .text      claimed      8   ours      8   retail      8   fits
       no extra functions: our object defines only what the retail unit object does

`gate.sh` reported `ok` on every step (`ninja + build.sha1`, `hashes vs config.yml` = all 86 RELs,
`module wiring`, `docs claims`, `port probe`, `port link gap`, `reach stubs`) and the port link's
undefined count did not move (287, 0 duplicates) — this file defines `fn_80218DF4` and references
`gLoader_DarkTrooper`, which `DarkTrooper.cpp` already defines and which the port compiles.

## Notes for the next run

* **Nothing blocked this item.** One spelling, one attempt, 100.00% on the first build — it is the
  twin case `Carve802189D0.c` and `Carve80218B68.c` established, so there is no wall to record and
  no spelling table worth keeping.
* **No `PortLinkStubs.cpp` duplicate had to be deleted.** `grep -rn fn_80218DF4` over `src/`
  returns only `CDarkTrooperRel.cpp` (two calls, one declaration) and `CDarkCommandoRel.cpp` (a
  comment). Rule 3 of the carve vein did not apply here.
* The seed item's directory hint (`MetroidPrime/ScriptLoader`) is right, and taken from the nearest
  claimed range below: `DarkTrooper.cpp` claims 0x80218DC8..0x80218DF4, this run slots between it
  and `GlowBug.cpp` at 0x80218DFC with no unclaimed gap on either side. No link-order cycle: the
  carve is not adjacent to a unit boundary in a way `dtk dol split` objects to, and it built clean.
* **Stale sibling comment, deliberately not fixed (out of scope for this item, no `NEW:` filed
  because a documentation fix is not work that raises a count):**
  `src/MetroidPrime/ScriptLoader/MetroidAlpha.cpp:6-8` still reads "The 8-byte setter at 0x80218B68
  is deliberately NOT claimed: REL modules import it by its retail name, so it cannot be renamed
  and must stay in dtk's auto unit" — but `Carve80218B68.c` has been a `Matching` unit for some
  time, so that sentence is already false on `HEAD` and this item did not cause it.
  `GlowBug.cpp:7` (0x80218E28) and `SpankWeed.cpp:7` (0x80218DC0) are still true: both addresses
  are unclaimed and are the obvious next two carves in this family.