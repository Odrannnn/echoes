# carve-801f8a54 — `MetroidPrime/Carve801F8A54.c`

`kind: match`, 1 function, **flipped**. `matched` 13607 -> 13608, `linked` 6655 -> 6656,
`total_functions` still `28465`.

## What I did

Carved `fn_801F8A54` (retail `.text` `0x801F8A54..0x801F8A60`, 12 bytes) out of dtk's unclaimed
`auto_03_801F7AD0_text` run into its own `Matching` unit as a plain-C definition. Four files, each
entry in address order:

| file | entry |
| --- | --- |
| `src/MetroidPrime/Carve801F8A54.c` | new, 51 lines: 50 of header comment and `void fn_801F8A54(int* vec) { vec[1] = 0; }` on line 51 |
| `config/G2ME01/splits.txt:1424-1425` | `.text start:0x801F8A54 end:0x801F8A60`, immediately after `Carve801F7AC8.c` (which ends at `0x801F7AD0`) and before `ScriptObjects/CUnknown90.cpp` (`0x801F9050`) |
| `configure.py:802-808` | six comment lines + `Object(Matching, "MetroidPrime/Carve801F8A54.c"),` **on one line** (line 808), between `Carve801F7AC8.c` (801) and `ScriptObjects/CUnknown90.cpp` (809) |
| `files.cmake:828-831` | three comment lines + `src/MetroidPrime/Carve801F8A54.c` (line 831), in the carve block next to `Carve801F7AC8.c` / `ScriptObjects/Carve801F97C8.c` |

The claim is `.text` only, 12 bytes, and it spans nothing: below, `0x801F7AD0..0x801F8A54` is
unclaimed, and above, `0x801F8A60..0x801F9050` (`fn_801F8A60`, 0xA4) is too — measured with a
scan of every `.text` range in `splits.txt`; the nearest claimed range below is
`MetroidPrime/Carve801F7AC8.c` and the nearest above is `MetroidPrime/ScriptObjects/CUnknown90.cpp`,
which is where the `MetroidPrime/` directory comes from.

No `PortLinkStubs.cpp` duplicate existed: `grep -n "fn_801F8A54\|801F8A" src/MetroidPrime/PortLinkStubs.cpp`
returned nothing before the change, so nothing had to be deleted there.

## What I measured

- **Retail's bytes** — `build/G2ME01/asm/auto_03_801F7AD0_text.s:1143-1148`: `li r0, 0x0` /
  `stw r0, 0x4(r3)` / `blr`, 3 instructions. `config/G2ME01/symbols.txt:8170` gives
  `fn_801F8A54 = .text:0x801F8A54; // type:function size:0xC`.
- **The unit's object** — `python3 tools/carve_diff.sh 0x801F8A54 0xC build/G2ME01/obj/MetroidPrime/Carve801F8A54.o`
  prints `retail: 3 instructions, 12 bytes` / `ours: 3 instructions, 12 bytes` /
  `differing instructions: 0` / **`BYTE-EXACT`**. (Unlike the loader-setter carves, this one has no
  `@sda21` operand, so `carve_diff.sh` is clean here rather than reporting the one expected
  relocation difference.)
- **`./tools/flip_test.sh MetroidPrime/Carve801F8A54.c`** — `PASS  -> kept as Matching`,
  `kept: 1 / 1   failed: 0   skipped: 0`.
- **`build/report.json`** — `main/MetroidPrime/Carve801F8A54`: `fuzzy_match_percent 100.0`,
  `total_functions 1`, `matched_functions 1`, `complete_units 1 / 1`, `complete: true`. The source
  unit it came out of, `main/auto_03_801F7AD0_text`, went 22 -> 21 functions and is still 0%.
- **`python3 tools/check_decl_order.py --unit MetroidPrime/Carve801F8A54.c`** — `ok: 0 unit(s)
  checked, none emits its functions out of retail order` (one function, so the descending-source-order
  rule is trivially satisfied).
- **`./tools/goal_check.sh build/goal/item.json`** — `goal_check: PASS carve-801f8a54`, every check
  `ok`: `gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)`,
  `counts: matched 13607 -> 13608   linked 6655 -> 6656`, `check_symbol_names.py`,
  `All:  37.69% fuzzy, 31.12% matched, 13.99% linked (13608 / 28465 functions)`,
  `flip_test MetroidPrime/Carve801F8A54.c: PASS, Object(Matching) in configure.py`.
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (retail).

## The twin, and what the function is for

`fn_801F8A54` is byte-for-byte the matched `fn_80004010` (`src/MetroidPrime/Carve80004010.c:98`,
`Matching`, 100.00%), so the spelling is the twin's own: `vec[1] = 0`. That twin's header records
what its copy is for — the element-count half of an `rstl::vector` clear, whose caller frees `+0xC`
once the cleared word tests zero — but **none of that is claimed here**, because this copy is not
that. It has exactly one caller, measured with `grep -rn 'bl fn_801F8A54' build/G2ME01/asm/`:
0x801F8A3C inside `fn_801F89E0` (0x801F89E0, 0x74), itself anonymous in the same unclaimed run.
That function passes `addi r3, r31, 0x14`, and `mr r31, r3` at 0x801F89FC makes r31 the object it
was handed, so what is cleared is the word at **+0x18** of that object — after it has built a
0x14-byte stack record (a `0.0f` pair from `lbl_8041D640@sda21` plus the packed byte at `0x18(r1)`)
and handed it to `fn_801F8A60`. A reset followed by the zeroing of a trailing flag.

There are **no callees and no data references** — the three instructions touch nothing but r3 — so
there was nothing to declare `extern` and no `.data`/`.rodata`/`.sbss` is claimed. The pointer is
spelled `int*` because the twin's is; MWCC does not encode a variable's type in its name, and the
12 bytes of `.text` are the only thing the spelling has to reproduce.

`.c` because `symbols.txt:8170` carries only the `fn_801F8A54` placeholder, so a `.cpp` definition
would mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.

## Notes for the next run

- **The twin family here is small, and that is worth recording.** Unlike the 64 module-loader
  setters (see `docs/goal-notes/carve-802399f4.md`), the `li r0,0 / stw r0,4(r3) / blr` shape has
  no further unsourced copies left in this tree's unclaimed `auto_*` runs as far as this run looked
  — not filed as `NEW:`, since it is a restatement of the carve vein already in
  `docs/RUNNING_THE_DECOMP.md`, and I did not walk the whole binary for it.
- **Lesson: `carve_diff.sh` can be clean.** The loader-setter carves report one differing
  instruction because an unlinked `@sda21` store reads `stw r3,0(0)`. A carve with **no** `sda21`
  operand and no `lis` reports `BYTE-EXACT` outright, which makes it a usable pre-check rather
  than only a count check.
- `docs/HANDOFF.md` shows as modified in this worktree: that is the judge's own
  `MP_GATE_DOCS_WRITE=1 ./tools/gate.sh` re-deriving the state block, not an edit of mine — I
  touched no `docs/` file.