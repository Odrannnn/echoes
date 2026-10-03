# carve-803371EC — `fn_803371EC`, the second vtable slot returning `.sdata2` word `lbl_8041ED38` — Matching

`kind: match`, target `Kyoto/Math/Carve803371EC`. Claimed exactly `.text 0x803371EC..0x803371F4`
(0x8 bytes, 1 function) out of dtk's `main/auto_03_803371A8_text`, whose header is
`# 0x803371A8..0x803371F4 | size: 0x4C`. The claim is the **last run** of that auto range, so
the rest (0x803371A8..0x803371EC, six `li r0,0` / `stb` thunks) stays unclaimed and the unit
claims nothing else. `Kyoto/Math/Carve803371F4.c` claims the 4 bytes above, so this range is a
gap between a claimed unit and a claimed unit and cannot be folded into either.

## What it is

`build/G2ME01/asm/auto_03_803371A8_text.s`:

```
# .text:0x44 | 0x803371EC | size: 0x8
.fn fn_803371EC, global
/* 803371EC 00333FEC  C0 22 C9 78 */  lfs f1, lbl_8041ED38@sda21(r0)
/* 803371F0 00333FF0  4E 80 00 20 */  blr
```

and, independently, `build/binutils/powerpc-eabi-objdump -s -j .text
--start-address=0x803371EC --stop-address=0x803371F4 build/G2ME01/main.elf` reads
`c022c978 4e800020` — the same two instructions and nothing else.

The item's reason is right and re-measured: this is byte-for-byte the same encoding as
`fn_8033719C` (0x8033719C), 0x50 bytes away, and it loads the same retail word. Both are
byte-shape twins of the matched `CParticleGen::GetGeneratorRate` at 0x800534BC
(`src/MetroidPrime/CExplosion.cpp:44`, `lfs f1,lbl_8041A920@sda21(r0)` / `blr`). Retail returns
the float in `f1`, so the C is `float f(void) { return lbl_8041ED38; }`. It cannot be a folded
`li f1,0`: a float zero has to come from memory, which is why retail loads a pool word at all.

Displacement: `_SDA_BASE_ = 0x8041FD80`, so `0x8041ED38 - 0x8041FD80 = -0x1048 = 0xEFB8` →
`C0 22 C9 78`. ✓ The emitted object carries exactly that — `build/G2ME01/asm/Kyoto/Math/Carve803371EC.s`
is byte-identical to retail's two instructions, and `powerpc-eabi-nm build/G2ME01/obj/Kyoto/Math/Carve803371EC.o`
gives `T fn_803371EC` (unmangled, so objdiff pairs it) and `U lbl_8041ED38`.

## The pool word is `0.0f`, declared and not claimed

`config/G2ME01/symbols.txt:25436`:
`lbl_8041ED38 = .sdata2:0x8041ED38; // type:object size:0x4 align:4 data:float`.
`powerpc-eabi-objdump -s -j .sdata2 --start-address=0x8041ED30 build/G2ME01/main.elf` reads
`3a83126f 3f4ccccd 00000000 00000000`, so the word is `0x00000000` = `0.0f` (the second zero
is the pool's alignment pad for the `double` at 0x8041ED40). `grep -n '8041ED38'
config/G2ME01/splits.txt` matches **nothing**: no unit of ours claims it.

This carve declares the word and does not claim it, which is the same conclusion
`src/Kyoto/Math/Carve8033719C.c` reached by building the alternative and measuring it. The
short version, because it cost that run a full session: claiming `.sdata2 0x8041ED38..0x8041ED40`
and writing `return 0.f;` gives the literal a *local* pool name, the retail name leaves the DOL
link, and nine other auto functions that read the same word then fail to link
(`undefined: 'lbl_8041ED38' ... 9 more ... # Link failed.`). Naming the constant does put a
global definition at the right address but mwcceppc still constant-folds the read, which costs
the DOL 4 bytes of displacement (`C0 22 C9 7C` vs retail's `C0 22 C9 78`). So: `extern const
float lbl_8041ED38;`, and the pool word stays with dtk's `auto_11_8041EC28_sdata2.o`.

**The host-only definition stays in the twin and only there.** `src/Kyoto/Math/Carve8033719C.c:109`
is the single `const float lbl_8041ED38 = 0.f;` under `#ifndef __MWERKS__`; a second one here
would be a duplicate symbol in the port's flat link. This file declares the extern and defines
nothing host-only — `grep -rn 'const float lbl_8041ED38 =' src/` returns exactly one definition,
plus this file's comment. The value the host sees is retail's own `0.f`, measured above, not a
placeholder.

## The four files

* `configure.py` — `Object(Matching, "Kyoto/Math/Carve803371EC.c")`, one `Object(` per line,
  placed after `Carve803371A4.c` and before `Carve803371F4.c` (address order), with a comment on
  why the pool word is declared rather than claimed.
* `config/G2ME01/splits.txt` — `Kyoto/Math/Carve803371EC.c:` / `.text start:0x803371EC
  end:0x803371F4`, same neighbours, same order.
* `files.cmake` — `src/Kyoto/Math/Carve803371EC.c`, same position, with the matching comment.
* `src/Kyoto/Math/Carve803371EC.c` — new, header in the style of `src/Kyoto/Math/Carve8033719C.c`.

Nothing was copied from another tree or another commit. `git diff configure.py` is the one hunk
above and nothing else.

Directory is retail-own, from the nearest claimed range: 0x803371EC is 0xF5DC bytes into
`Kyoto/Math/CMayaSpline.cpp` (`.text start:0x80327C10`), matching the 0xF5E4 its neighbour
`Carve803371F4.c` records.

## Checks

```
$ ./tools/decomp_build.sh Kyoto/Math/Carve803371EC.c
All:  37.69% fuzzy, 31.13% matched, 14.00% linked (13622 / 28465 functions)
      Code: 2034444 / 6535816 bytes (13622 / 28465 functions)

$ ./tools/flip_test.sh Kyoto/Math/Carve803371EC.c
  PASS  -> kept as Matching
kept: 1 / 1   failed: 0   skipped: 0

$ ./tools/unit_fit.sh Kyoto/Math/Carve803371EC.c
   .text      claimed      8   ours      8   retail      8   fits
   no extra functions: our object defines only what the retail unit object does

$ python3 tools/check_decl_order.py --unit Kyoto/Math/Carve803371EC.c
ok: 0 unit(s) checked, none emits its functions out of retail order

$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  build/G2ME01/main.dol

$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13621 -> 13622   linked 6669 -> 6670
  ok    check_symbol_names.py
  ok    All:  37.69% fuzzy, 31.13% matched, 14.00% linked (13622 / 28465 functions)
  ok    flip_test Kyoto/Math/Carve803371EC.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-803371EC
```

`total_functions` is still 28465 after the `splits.txt` edit. gate.sh's per-function diff reports
the one expected line — `SPLIT main/auto_03_803371A8_text: 1 function(s) moved into
main/Kyoto/Math/Carve803371EC (exact count match - a split, not a loss)` — which is the count
going up by exactly the one function this unit claims. `check_symbol_names.py`: `checked 610
units; 0 declared names are missing from their object`. Decl order cannot go wrong in a
one-function file, but the tool is run anyway.

`tools/carve_diff.sh 803371EC 8 build/G2ME01/obj/Kyoto/Math/Carve803371EC.o` prints
`NOT byte-exact` and that is **wrong**, not a finding: it disassembles the *unlinked*
relocatable object, where the `@sda21` displacement is still an unresolved relocation
(`lfs f1,0(0)`), against the linked `main.elf` (`lfs f1,-13960(r2)`). It cannot pass for any
unit that loads a pool word. The acceptance test is `flip_test.sh`, which passed, and the linked
DOL byte at 0x803371EC is `c0 22 c9 78`.

## Nothing blocked it

No assembly, no stub, no claim of a range the object does not define, no change to a shared
unit's `.text` (`Kyoto/Math/Carve803371F4.c`, `Carve803371A4.c` and `CMayaSpline.cpp` all keep
their own bytes — gate.sh's report diff is clean apart from the one split line above), and no
documentation claim that a check did not measure. Not committed; the notes file is the only
thing outside the worktree.