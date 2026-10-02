# carve-801fd52c

> **Second run (lane 10, 2026-10-02).**  The report below was written by a run whose change never
> reached a commit: this run started from a clean `goal/lane-10` (`c7f9598a`) with no
> `Carve801FD52C.cpp`, no `configure.py` entry and no splits claim.  Everything in it was re-derived
> and re-measured here, with three corrections, and the landing report is appended at the end.

**Kind:** `match` **Target:** `MetroidPrime/ScriptObjects/Carve801FD52C`

New `Matching` unit `src/MetroidPrime/ScriptObjects/Carve801FD52C.cpp`, claiming
`.text 0x801FD52C..0x801FD5E8` (0xBC = 188 bytes, 2 functions, both 100.0% / matched):
`fn_801FD52C` 0x84 = 132 bytes and `fn_801FD5B0` 0x38 = 56 bytes. Matched 13075 -> 13077, linked
6185 -> 6187, DOL `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

`./tools/goal_check.sh build/goal/item.json` exits **0** on this tree: every check green, including
`gate.sh` (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe) and
`flip_test MetroidPrime/ScriptObjects/Carve801FD52C.cpp: PASS`.

## What the two functions are

`fn_801FD52C` is `rstl::vector<T, rmemory_allocator>::~vector()` for a **36-byte** `T`;
`fn_801FD5B0` is the `rstl::destroy(pointer_iterator, pointer_iterator)` it calls. Retail names
neither - `config/G2ME01/symbols.txt:8268-8269` carries the `fn_<addr>` placeholder for both - so
the shapes are read off the call edges and fixed by named twins elsewhere in the same DOL:

- searching `orig/G2ME01/sys/main.dol`'s `.text` for
  `7c a5 02 14 90 a1 00 0c 80 1e 00 0c 90 a1 00 08 90 01 00 10 90 01 00 14 48 00` finds **62**
  occurrences of this exact four-store body, with the `mulli` immediate taking the values 0x0C,
  0x14, 0x18, 0x1C, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x44, 0x48, 0x4C, 0x68, 0x70 and others.  The
  named ones include `__dt__Q24rstl138vector<Q24rstl94pair<...>>Fv` (0x80005768),
  `__dt__Q24rstl48vector<11CTweakValue,Q24rstl17rmemory_allocator>Fv` (0x80006724) and
  `__dt__Q24rstl47vector<10CPrimitive,Q24rstl17rmemory_allocator>Fv` (0x80030990).
- `fn_801FD5B0` is the twin of the already-`Matching` `fn_801FDBE0`
  (`src/MetroidPrime/ScriptObjects/Carve801FDB5C.c`), same 0x38 bytes, same body:
  dereference both arguments, then call `destroy_impl` with the addresses of its own copies.
- `fn_801FD5E8` (0x801FD5E8, 0x50) is `destroy_impl` and is **claimed and Matching**
  (`Carve801FD5E8.c`); its callee `fn_801FD638` (0x20) is claimed by `Carve801FD638.c`.  So the
  call chain this unit reaches is entirely ours - **no port-link stub is needed for it.**

## The finding that made both functions match

`fn_801FD5B0` is byte-exact with the same spelling as `fn_801FDBE0`: two **by-value one-pointer
struct** parameters, forwarded to `destroy_impl`.

`fn_801FD52C` needs one more thing, and it is the whole difficulty of this item.  Its four stores
at `r1+0x08/+0x0C/+0x10/+0x14` are two by-value `pointer_iterator`s' **home slots**: each pointer
is stored twice, and `addi r3,r1,0x14` / `addi r4,r1,0xc` pass the *second* copy of each.
`src/MetroidPrime/Carve800045A0.c:26-31` documents the same four stores in the 12-byte-element
twin of this exact function and says removing them is 16 bytes short.

**The spelling that reaches the bytes, and the two that do not** (all measured this run, 47
instructions each unless noted, compared word for word against `orig/G2ME01/sys/main.dol` with the
`R_PPC_REL24` words resolved):

| spelling | instructions | object symbols | verdict |
| --- | --- | --- | --- |
| `(SIt){mItems}`, `(SIt){mItems + mCount * 0x24}` in a **`.c`** | 47, 0 differing | + **`.sbss2` of 8 bytes** (`b @7`, `b @9`) | byte-exact but **unlinkable** |
| named `SIt begin, end;` locals in `.c` | 32 for `fn_801FD52C` | clean | pairs land interleaved (`end` at +0x08/+0x10, `begin` at +0x0C/+0x14), not adjacent |
| two `static inline SIt Make*(v)` factories in `.c` | 47 | clean | right count, wrong slots again |
| **`SIt(mItems)`, `SIt(...)` with an in-class constructor, `.cpp`** | **47, 0 differing** | **only `fn_801FD52C` and `fn_801FD5B0`** | **matches and links** |

MWCC's C mode gives a compound literal **static** storage duration, so the first spelling emits an
8-byte `.sbss2` that retail's object does not have; `unit_fit.sh` then answers `NOT CLAIMED BY
splits.txt` and the DOL stops reproducing retail.  That is the same wall
`docs/goal-notes/carve-801fdb5c.md` recorded for the stride-0x30 twin, and the lesson generalises:
**byte-exact in isolation is not linkable** - judge with `unit_fit.sh` + `flip_test.sh`, never with
an instruction diff.

The C++ temporary an in-class constructor builds has automatic storage duration, so nothing extra
reaches the object and the two slot pairs land adjacent, which is what the bytes show.  The
constructor is declared and defined in class, so it inlines away:
`nm --defined-only -n` on `build/G2ME01/src/MetroidPrime/ScriptObjects/Carve801FD52C.o` reads
exactly

```
00000000 T fn_801FD52C
00000084 T fn_801FD5B0
```

That is also why the unit is a `.cpp` and not a `.c`.

## Decl order bit here, worth restating

Definitions must be **descending by retail address**, and the direction is easy to get backwards
because the *object* order is the reverse of it.  `fn_801FD5B0` (0x801FD5B0) is declared **first**
in the source and `fn_801FD52C` (0x801FD52C) second; mwcceppc emits in reverse source order, so
the object's `.text` reads `fn_801FD52C` @ 0x0, `fn_801FD5B0` @ 0x84, which is retail's order.
The first version of this file declared them the other way round and objdiff still paired both
functions at 100% - only the `nm` order and the split would have been wrong.
`python3 tools/check_decl_order.py --unit main/MetroidPrime/ScriptObjects/Carve801FD52C` prints
`ok: 1 unit(s) checked, none emits its functions out of retail order`.

## The port-link stub had to be retired

`Carve801FD4B0.cpp`'s carve left `stub_231` = `asm("fn_801FD52C")` in
`src/MetroidPrime/PortLinkStubs.cpp` for the very symbol this unit now defines.  Both definitions
at once is a link error, and `gate.sh` caught it:

```
link_check: duplicate definitions   1
link_check: NOT LINKED
link_check: DUP fn_801FD52C
GATE FAIL: probe link-dups
```

Retired `stub_231`, kept `stub_228` (`fn_801FDB5C`), `stub_229` (`fn_801FD998`) and `stub_230`
(`fn_801FD7D4`), which are still unclaimed.  The file's header counts moved with the change:
functions 191 -> 190, data unmoved at 9, the total 200 -> 199, and the three prose references to
"the four stand-ins" now say three.  After it: `probe_sources.sh` prints
`829 files, 0 failed, 0 errors; link: LINKED (285 undefined, 0 duplicates)` and `link_check`
prints `STRICT PASS - 285 undefined against a baseline of 287 (no growth), 0 duplicate(s)`.

**A carve that claims a symbol the port had a stub for must retire that stub in the same change**,
and the undefined count does *not* move when it does: the symbol was resolved before and is
resolved after, just by the real body instead of the empty one.

## Files

- `src/MetroidPrime/ScriptObjects/Carve801FD52C.cpp` (new, 216 lines, descending decl order)
- `configure.py:774` - `Object(Matching, ...)`, one line, after `Carve801FD4B0.cpp`
- `config/G2ME01/splits.txt:1374-1375` - `.text start:0x801FD52C end:0x801FD5E8`
- `files.cmake:581` - the source
- `src/MetroidPrime/PortLinkStubs.cpp` - `stub_231` retired, header counts and the block's prose

`total_functions` still 28465.  `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show in
`git status` because `goal_check.sh` rewrote their derived counts itself (13075 -> 13077 and the
linked figures); no edit of mine to either, and the judge is green with them as it left them.

NEW: match | MetroidPrime/ScriptObjects/Carve801FD998 | `fn_801FD998` (0x801FD998, 0x84, stride 0x2C) is this unit's twin with one `mulli` immediate changed - use the C++ `SIt(void*)` in-class-constructor spelling from `Carve801FD52C.cpp`, and retire `stub_229` with it

(`fn_801FD7D4`, the stride-0x24 twin, is already queued as `carve-801fd7d4`, and its port stub is
`stub_230` - noted here rather than filed, so the same two facts reach whoever runs it.)

---

# Landing report - second run (lane 10, 2026-10-02)

**Done, and this time in the tree.**  `MetroidPrime/ScriptObjects/Carve801FD52C` is `Matching`,
claiming `.text 0x801FD52C..0x801FD5E8` (0xBC = 188 bytes, 2 functions).  `goal_check.sh` PASSes on
this tree; the change is the four carve files plus the `PortLinkStubs.cpp` stub retirement, nothing
else of mine.

## Re-measured: the spelling

Retail's bytes re-read with `python3 tools/dol_read.py 0x801FD52C 0xBC` and decoded: 33 instructions
for `fn_801FD52C`, 14 for `fn_801FD5B0`, identical to the report above.  The report's winning
spelling is confirmed: a `.cpp` whose iterator has a one-argument constructor and whose two
iterators are materialised **as unnamed arguments at the call**
(`fn_801FD5B0(SIt(mItems), SIt(mItems + mCount))`), with `fn_801FD5B0` declared first (descending
addresses).  Compiled with the unit's exact flags (`-O4,p -inline deferred,noauto`,
`-pragma inline_max_size(125)`, `-lang=c++`):

| spelling measured this run | `fn_801FD52C` | object content | verdict |
| --- | --- | --- | --- |
| `.cpp`, ctor, **temporaries at the call** (this unit) | 33 of 33; only the 3 `bl` words relocate | `nm`: exactly `fn_801FD52C`, `fn_801FD5B0` | **Matching** |
| `.cpp`, ctor, **named** locals | 32 (0x80 vs 0x84), 22 words differ, tail shifted | clean | one instruction short |
| `.c`, `(struct SIt){...}` compound literals | 33, byte-exact | **`.sbss2` 8 bytes, symbols `@14`, `@16`** | byte-exact but unlinkable |

So the constructor alone is not what earns the 33rd instruction: **the temporaries must be unnamed
at the call**.  Named locals of the same constructed type let MWCC reuse one slot per argument (the
report above said this only for `.c` named locals; measured here for `.cpp`, both with and without
the constructor).  The `.c` compound-literal row is the `.sbss2` wall the report above describes,
reproduced this run - MWCC's C mode gives a compound literal static storage duration.

**Correction to the report above, from the 0x2C twin that landed while this item was queued:** the
`.c` path is not closed.  `src/MetroidPrime/ScriptObjects/Carve801FD998.c` (Matching, landed as
`062db81a`) reproduces the *same* 33-instruction body in a `.c` - with `unsigned char* volatile
firstCopy`/`lastCopy` locals, `end = data + count * 44`, and `fn_801FDA1C(&first, &last)` taking the
addresses of the locals through `const void*` parameters.  Two spellings from two files, both
verified; the queued `carve-801fd7d4` (0x84, `mulli 36`, walk `fn_801FD858`) is this exact body with
one `bl` changed and can use either.

## In the tree (all measured this run)

- `./tools/fast_try.sh MetroidPrime/ScriptObjects/Carve801FD52C` -> `100.00% fuzzy, 100.00% matched
  code, 2/2 functions`
- `./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801FD52C.cpp` -> `.text claimed 188 ours 188
  retail 188 fits` / `no extra functions: our object defines only what the retail unit object does`
- `python3 tools/check_decl_order.py --unit main/MetroidPrime/ScriptObjects/Carve801FD52C` -> `ok: 1
  unit(s) checked, none emits its functions out of retail order`
- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FD52C.cpp` -> `PASS -> kept as Matching`
- `./tools/goal_check.sh build/goal/item.json` -> `PASS`, `counts: matched 13098 -> 13100   linked
  6196 -> 6198`, `All: 37.08% fuzzy, 30.52% matched, 13.47% linked (13100 / 28465 functions)`;
  `build/report.json` `measures.total_functions` = 28465 (splits.txt edit did not move it) and the
  unit's entry is `fuzzy 100.0, matched_code 188/188, total_functions 2, matched_functions 2`
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`
- `./tools/probe_sources.sh` -> `probe: 834 files, 0 failed, 0 errors; link: LINKED (286 undefined, 0
  duplicates)`; `build/probe-logs/link_check.log` -> `STRICT PASS ... 286 undefined against a
  baseline of 287 (no growth), 0 duplicate(s), 0 compile error(s), linker_ran=1`
- `build/gate-link.log` -> `279 MISSING ... all accounted for in port_link_gap_list.md`

## Files touched

- `src/MetroidPrime/ScriptObjects/Carve801FD52C.cpp` (new, 214 lines, descending decl order)
- `configure.py:775` - `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FD52C.cpp"),`
- `config/G2ME01/splits.txt:1377-1378` - `.text start:0x801FD52C end:0x801FD5E8`
- `files.cmake:582` - the path
- `src/MetroidPrime/PortLinkStubs.cpp` - `stub_231` retired, its block's prose updated, header
  counts re-derived this run: the file held **191 function stubs and 10 data stubs** before the
  retirement and 190 / 10 after (`grep -cE 'asm\("'` 201 -> 200; `^extern "C" void stub_*() asm(`
  191 -> 190; `^extern "C" char stub_data_*` unmoved at 10).  The header line's previous `190 / 9`
  was one function high and one data object low against those terms - corrected in place.  Keeping
  `stub_231` with the unit's definition would be the duplicate the `link-dups` gate catches.

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show in `git status` because `goal_check.sh`
rewrote their derived counts itself (`MP_GATE_DOCS_WRITE=1`); no edit of mine to either.

No `NEW:` line: the one twin worth filing (`fn_801FD7D4`) is already queued as `carve-801fd7d4`, and
the `fn_801FD998` item the report above filed has landed (`062db81a`).  No `WALL:` - the item passed.

