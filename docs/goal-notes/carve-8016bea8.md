# carve-8016bea8 — `MetroidPrime/Carve8016BEA8` (match)

**Result: `goal_check: PASS`.** The unit is `Matching`, `complete_units: 1`, **2/2 functions at
100.00%**, `main/MetroidPrime/Carve8016BEA8`, `.text 0x8016BEA8..0x8016BF4C` = 0xA4 = 164 bytes of
164, `matched_functions` 2. Measured totals: **matched 13499 → 13501, linked 6547 → 6549**,
`total_functions` **28465** unchanged, DOL sha1 **`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`**,
`87 files OK`, `probe_sources.sh` **930 files, 0 failures**, port link **287 undefined / 0
duplicates** (unchanged — the stand-in below is what keeps it there), `check_symbol_names.py`
588 units / 0 missing.

```
ok  gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok  counts: matched 13499 -> 13501   linked 6547 -> 6549
ok  check_symbol_names.py
ok  flip_test MetroidPrime/Carve8016BEA8.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS carve-8016bea8
```

`tools/flip_test.sh MetroidPrime/Carve8016BEA8.cpp` → `PASS -> kept as Matching (kept: 1 / 1)`;
`tools/carve_diff.sh 0x8016BEA8 0xA4 …` → `retail: 41 instructions, 164 bytes / ours: 41
instructions, 164 bytes`, and the only two differing instructions are the two `bl`s, whose
displacements are relocations the link fills (`87 files OK` and the sha1 above are the proof the
filled bytes are right).

## What the two functions are

`include/rstl/string.hpp:397-408` already names them: `istring` is
`basic_string<char, case_insensitive_char_traits<char>>`, "retail emits both out of line for this
instantiation — `fn_8016BEA8` is `operator==` and it calls `fn_8016BED0`, which is `compare`.
Neither is claimed by any unit yet; they live in an unclaimed `.text` gap
(`0x8016BDEC..0x8016C230`) … so they need a unit of their own." This is that unit.

| | address | size | retail symbol | what it is |
|---|---|---|---|---|
| `fn_8016BED0` | 0x8016BED0 | 0x7C = 124 B, 31 insn | `fn_8016BED0` | `istring::compare`, the body of `string.hpp:379-382` |
| `fn_8016BEA8` | 0x8016BEA8 | 0x28 = 40 B, 10 insn | `fn_8016BEA8` | `istring::operator==`, the body of `string.hpp:384-387` |

Both **exact twins** were verified instruction by instruction (not by percentage) against units
already at 100% in this tree:

- `fn_8016BEA8` ≡ `__eq__Q24rstl66basic_string<c,…>CFRCQ24rstl66basic_string<c,…>`
  (`symbols.txt:1393`, 0x8004985C, 0x28) in `MetroidPrime/CIOWinManager.cpp` (`Matching`, 19/19) —
  the same ten instructions word for word, `bl compare__Q24rstl…` where this `bl fn_8016BED0` is.
- `fn_8016BED0` ≡ `compare__Q24rstl66basic_string<c,…>CFRCQ24rstl66basic_string<c,…>`
  (`symbols.txt:596`, 0x80021074, 0x7C) in `MetroidPrime/CCredits.cpp` (`MatchingFor("G2ME01")`,
  49/49) — the same 31 instructions word for word: same operand schedule, same `li r7,0`, same
  three `addi` into r5/r6/r4/r3, and **all sixteen `stw` displacements**, with
  `bl internal_compare<rstl::const_linear_iterator<…>>` where this `bl fn_8016BF4C` is.

`fn_8016BF4C` (0x8016BF4C, 0x228, `symbols.txt:6007`) is named by that measurement, not assumed:
the twin's callee is `internal_compare<…>` at 0x800210F0 and is 0x114 = 276 bytes, while
`fn_8016BF4C` is 552 and folds both sides of every comparison (`extsb` ×8, `cmpwi` ×12, `subi` ×6
over `['a','z']` / `[0xE0,0xFE]` / `[0x30A0,0x30FF]`) where the twin's body has **two** `extsb and
no range test at all. That is `case_insensitive_char_traits<char>::compare` =
`lower(lhs) - lower(rhs)`, `string.hpp:86-96` — the same `internal_compare` template
(`string.hpp:359-377`) instantiated with the other traits argument, emitted out of line a second
time. It is 4 bytes above this claim, so it stays unclaimed; it is **declared, never defined**
here, and `src/MetroidPrime/PortLinkStubs.cpp` gained the announced empty-body stand-in for the
host link (following `stub_carve801eb30c_0` from `carve-801eb30c`).

## The one deviation from the item's wording, and why

The item says `src/MetroidPrime/Carve8016BEA8.c` and "plain C so the fn_ names do not mangle".
**It is a `.cpp` with `extern "C"`, because plain C cannot produce `fn_8016BED0`'s frame.**
MWCC passes a *class-type* by-value argument as a pointer to a caller-side temporary, so
`internal_compare(begin(), end(), other.begin(), other.end())` needs a temporary per
`begin()`/`end()` result **plus** one per parameter — the eight words at +0x08..+0x47, 16 bytes
apart, with the four argument registers the second of each pair (`addi r3,r1,0x40`,
`r4,+0x30`, `r5,+0x20`, `r6,+0x10`). `-lang=c` gives POD structs a different frame. Measured with
`mwcceppc.exe` and the DOL's own flags (`tools/probe_cc.sh`'s command line, `-lang=c`), scoring
against `dol_read.py 0x8016BED0 124`:

| C spelling | result |
|---|---|
| 4 named locals of an 8-byte POD struct, by-value params | 86/124 bytes differ (param copies land at +0x08..+0x27) |
| `static inline` factory returning the struct, by-value params | 16/124 differ (all 8 slots, wrong pairing) |
| 16-byte struct (8 bytes of filler) by value | frame 0xA0, 82/124 differ |
| 12-byte struct + inline factory | frame 0x70, 53/124 differ |
| compound-literal arguments `(It){self,0}` | static objects in `.bss`, 4 extra symbols, 82/124 |
| array of 8, passed by value | frame 0x70, array at +0x28, 72/124 |
| array of 8, passed by pointer | dead stores eliminated, frame 0x30 |
| **C++ class with a constructor, by value, called as the call** | **byte-exact** (only the two `bl` relocations) |

The same measurement is recorded, for the same reason, in
`src/MetroidPrime/Carve8000432C.cpp:52-61`, and `extern "C"` is the trade that file and
`Carve8000447C.cpp` already make for unmangled `fn_` names. The unit *name* is the item's
(`MetroidPrime/Carve8016BEA8`) and both functions are in, so nothing was given up. Worth knowing
for the next lane: `fn_8016BEA8` alone (0x8016BEA8..0x8016BED0, 40 bytes) **does** match in plain
C — the `-lang=c` spelling of `operator==` is byte-exact on its own — so if a `.c` spelling were
required for some reason, that half is available and `fn_8016BED0` is what would have to go.

Also measured: naming the four iterators in locals instead of calling directly gives the right
length but the wrong frame, exactly as `Carve8000432C.cpp` records. So the call is written the way
`string.hpp:379-382` writes it, as the call itself.

## Files

- `src/MetroidPrime/Carve8016BEA8.cpp` — new, 147 lines: header comment, a local
  `rstl::const_linear_iterator` template (the same local-template device `Carve8000432C.cpp` uses
  for `pointer_iterator`, so the object carries two functions and one undefined symbol and nothing
  else), `struct S8016BEA8` with only `mPtr` at +0x00 / `mSize` at +0x08 read, the `fn_8016BF4C`
  declaration, and the two definitions **descending by address** (`check_decl_order.py --unit
  main/MetroidPrime/Carve8016BEA8` → ok).
- `configure.py:743` — `Object(Matching, "MetroidPrime/Carve8016BEA8.cpp")`, one line, in address
  order between `CInGameTweakManagerReadFromMemoryCard.cpp` and `CInGameTweakManagerCtor.cpp`.
- `config/G2ME01/splits.txt:1011-1012` — `.text start:0x8016BEA8 end:0x8016BF4C`, the only
  section, in address order; `total_functions` still 28465 after the edit.
- `files.cmake:722` — the source, in address order.
- `src/MetroidPrime/PortLinkStubs.cpp` — 23 lines: the announced stand-in for `fn_8016BF4C`,
  keyed `stub_carve8016bea8_0` after `stub_carve801eb30c_0`, no parameters (the host caller passes
  four iterators by value and the body ignores them). No duplicate: `fn_8016BF4C` was defined
  nowhere before this.
- `docs/HANDOFF.md`, `docs/RUNNING_THE_DECOMP.md` — **not edited by hand**; the judge's own
  `sync_state_block` rewrote the derived counts during `goal_check.sh` (matched 13501, linked 6549,
  probe 930).

No link-order cycle: the claim starts 0xBC bytes *into* dtk's `auto_03_8016BDEC_text`, so neither
neighbouring `Matching` unit's `.text` ends where this one starts. Also measured for the follow-on
claim below: adding `MetroidPrime/Carve8016BF4C.cpp: .text 0x8016BF4C..0x8016C174` to
`splits.txt` **does** split cleanly (`Splitting completed in 1.883s (wrote 2339 objects)`, no
cyclic-dependency error), so a carve that starts exactly where this unit's `.text` ends is
possible here — that is the case `RUNNING_THE_DECOMP.md` records as failing for
`CFrustumPlanes.cpp`. Reverted after measuring.

NEW: carve-8016bf4c | match | MetroidPrime/Carve8016BF4C | fn_8016BF4C 0x8016BF4C..0x8016C174 (138 instructions) is istring's internal_compare - source is include/rstl/string.hpp:359-377 with case_insensitive_char_traits<char>::compare at :86-96, and its char_traits twin internal_compare is matched at 0x800210F0 in MetroidPrime/CCredits.cpp; a splits.txt claim at 0x8016BF4C was measured to split with no link-order cycle.

Still unclaimed in the same gap, **not** filed as `NEW:` because each is a bigger job than an hour
of carving and both are *named* retail functions, so they need a `.cpp` that mangles correctly
rather than an `extern "C"` placeholder: `GetTweakValue__19CInGameTweakManagerCFRCQ24rstl66basic_string<…>`
(0x8016BDEC, 0xBC) and `HasTweakValue__19CInGameTweakManagerCFRCQ24rstl66basic_string<…>`
(0x8016C174, 0xBC). Both call `fn_8016BEA8`, i.e. this unit's `fn_8016BED0`, so a claim above
0x8016C174 is the natural next one; `MetroidPrime/CInGameTweakManagerReadFromMemoryCard.cpp` and
`MetroidPrime/CInGameTweakManagerCtor.cpp` already bracket the gap.