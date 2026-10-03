# carve-8022ebc8 — `fn_8022EBC8`, the EmperorIngStage3 loader setter

**Item:** `carve-8022ebc8`, `kind: match`, target `MetroidPrime/ScriptLoader/Carve8022EBC8`.
**Lane 3, 2026-10-03. Verdict: `tools/goal_check.sh build/goal/item.json` → `goal_check: PASS`.**

## What I did

Carved `fn_8022EBC8` out of dtk's unclaimed `main/auto_03_8022EBC8_text` as its own one-function
`Matching` unit, in the four files a carve is:

| file | change |
| --- | --- |
| `src/MetroidPrime/ScriptLoader/Carve8022EBC8.c` | new, 1 function, the claim |
| `config/G2ME01/splits.txt` | `.text start:0x8022EBC8 end:0x8022EBD0`, in address order between `EmperorIngStage3.cpp` and `DestructableBarrier.cpp` |
| `configure.py` | `Object(Matching, "MetroidPrime/ScriptLoader/Carve8022EBC8.c"),` on one line, after `EmperorIngStage3.cpp` |
| `files.cmake` | the `.c` path, after `Carve8022EB54.c`, with the four-line comment the sibling carve entries carry |

Plus one comment-only fix to `src/MetroidPrime/ScriptLoader/EmperorIngStage3.cpp`, because the
carve made its header false. It said:

> This unit claims .text 0x8022EB9C..0x8022EBC8 and .sbss 0x804195F0..0x804195F8. The 8-byte
> setter at 0x8022EBC8 is deliberately NOT claimed: REL modules import it by its
> retail name, so it cannot be renamed and must stay in dtk's auto unit.

That is now the opposite of what the tree does, so it now says the setter is its own unit and that
this unit references the slot as `extern`. This is the same edit `carve-8021f9b0` made to
`DigitalGuardian.cpp` (`git show dda0295f`), and it moves no bytes — no `.s`, no code.

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` also show as modified in `git status`. **I did
not edit them** — the judge's own `gate.sh` rewrote the derived counts when it ran
(`matched 13591 -> 13592`, `linked 6639 -> 6640`, `DOL units 11653 -> 11654`, probe `992 -> 993`
files). Leaving them is correct; the driver discards edits to those files anyway.

## What the function is, measured

`.text 0x8022EBC8..0x8022EBD0`, 0x8 = 8 bytes, 1 function
(`config/G2ME01/symbols.txt:9937`):

```
fn_8022EBC8 = .text:0x8022EBC8; // type:function size:0x8 align:4
```

Disc bytes (`python3 tools/dol_read.py 0x8022EBC8 0x8 orig/G2ME01/sys/main.dol`):

```
hex : 90 6d 98 70 4e 80 00 20
u32  : 0x906d9870 0x4e800020
```

dtk's own listing of the unclaimed range (`build/G2ME01/asm/auto_03_8022EBC8_text.s:10-12`) is the
same two instructions, with the name resolved:

```
stw r3, gLoader_EmperorIngStage3@sda21(r0)
blr
```

`r13 - 0x6780 = 0x804195F0`, and `config/G2ME01/symbols.txt:20780` gives
`gLoader_EmperorIngStage3 = .sbss:0x804195F0; // type:object size:0x8 data:4byte`. So it is a
**loader setter**: store the handed-in record address in the module's small-data slot, return.

### Why the unit is `.c`, not `.cpp`

The EmperorIngStage3 module imports this exact name from the DOL:

```
$ strings build/G2ME01/EmperorIngStage3/EmperorIngStage3.plf | grep 8022EBC8
fn_8022EBC8
```

so it cannot be renamed, and a `.cpp` definition would mangle to `_Z<len>fn_8022EBC8v` and objdiff
would pair nothing. A `.c` unit is compiled `-lang=c`, so the definition *is* the symbol.

### What the callers say the argument is

Both callers are in module 18's own listing,
`build/G2ME01/EmperorIngStage3/asm/auto_00_000000F8_text.s`:

* `RELExit` at `.text:0xC30C` does `li r3, 0x0` at `0xC314` and then `bl fn_8022EBC8` at `0xC31C`
  (file line 13827) — a null loader, as in every other module's `RELExit` of this family.
* `fn_18_C350` at `.text:0xC350`, 0x30 bytes, is the module constructor `RELMain` calls at
  `.text:0xC338`. It does `lis r3, lbl_18_bss_30@ha` at `0xC35C`, stores `fn_18_C380` with
  `stwu r0, lbl_18_bss_30@l(r3)` at `0xC368`, and calls at `0xC36C` (file line 13855) with `r3`
  still that address.

So the argument is the **address of the module's loader record**. The record is **one word** here,
unlike the two-word record the DigitalGuardian carve had:
`build/G2ME01/EmperorIngStage3/asm/auto_05_00000000_bss.s:13-15` gives `lbl_18_bss_30 size:0x4`.

That one word is the loader the DOL thunk calls. `LoadEmperorIngStage3` (`.text:0x8022EB9C`,
`size:0x2C`, `symbols.txt:9936`) is `src/MetroidPrime/ScriptLoader/EmperorIngStage3.cpp:18-20`,
whose body is `(*gLoader_EmperorIngStage3.value)(mgr, input, info)` — it loads the record pointer
out of the slot and calls member 0. `docs/research/rel_loaders.md:169` records that thunk against
slot `0` of `0x804195F0`. So `fn_18_C380` is the `FScriptLoader` the DOL jumps to, and `0` after
`RELExit`, which is why the thunk is not reached on a tear-down.

### The slot is claimed elsewhere, so this unit is `.text` only

`gLoader_EmperorIngStage3` is `.sbss 0x804195F0..0x804195F8` (`symbols.txt:20780`), the `value`
pointer plus the `padding` word of the `SLoaderSlot` at `EmperorIngStage3.cpp:11-14`. It is claimed
**and defined** by `MetroidPrime/ScriptLoader/EmperorIngStage3.cpp`
(`config/G2ME01/splits.txt:1968-1970`), so `Carve8022EBC8.c` declares it `extern` and does not
re-claim `.sbss`. The store writes `+0` only, which is retail's single `stw`; `padding` is what
keeps `gLoader_DestructableBarrier` at 0x804195F8 eight-byte aligned. MWCC does not encode a
variable's type in its name, so the `extern` spelling resolves to the same symbol.

### Why its own unit

Both neighbours are claimed units and this 8-byte range is exactly the gap between them:
`Carve8022EB54.c` ends at 0x8022EB9C, `EmperorIngStage3.cpp` runs 0x8022EB9C..0x8022EBC8, and
`DestructableBarrier.cpp` starts at 0x8022EBD0 (`splits.txt:1965-1977`). No unclaimed gap is
spanned and no unit claims two discontiguous ranges in one section. **`dtk dol split` accepted it**,
so the "cyclic dependency / link order" failure the carve vein documents for a carve that starts
exactly where a neighbouring `Matching` unit ends did **not** happen here.

Definitions are **descending by address** (`python3 tools/check_decl_order.py` — one function, and
the gate's `decl order` step is `ok`); plain C, so `fn_8022EBC8` does not mangle; exactly the range
0x8022EBC8..0x8022EBD0 claimed and nothing else.

### No `PortLinkStubs.cpp` duplicate

`grep -rn "fn_8022EBC8" src/ include/` on the clean tree found nothing outside this new file, so
there was no duplicate to remove. The port build adds no undefined symbol either:
`EmperorIngStage3.cpp` is in `files.cmake` and defines the slot, and nothing here calls anything.

## How I verified it

```
$ ./tools/decomp_build.sh MetroidPrime/ScriptLoader/Carve8022EBC8.c
87 files OK
All:  37.69% fuzzy, 31.12% matched, 13.99% linked (13592 / 28465 functions)
$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
```

`build/report.json`, the unit is complete and byte-exact:

```
main/MetroidPrime/ScriptLoader/Carve8022EBC8
  complete: true          total_functions 1  matched_functions 1
  fuzzy 100.0%  matched_code 8/8  complete_code 8/8
  .text size "8" virtual_address 2149772232   (= 0x8022EBC8)
  fn_8022EBC8  size 8  fuzzy_match_percent 100.0
```

dtk's listing of **our** object is character-for-character the listing of the retail one:

```
$ cat build/G2ME01/asm/MetroidPrime/ScriptLoader/Carve8022EBC8.s
# 0x8022EBC8..0x8022EBD0 | size: 0x8
/* 8022EBC8 0022B9C8  90 6D 98 70 */  stw r3, gLoader_EmperorIngStage3@sda21(r0)
/* 8022EBCC 0022B9CC  4E 80 00 20 */  blr
```

```
$ ./tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve8022EBC8.c
.text  claimed 8  ours 8  retail 8  fits
no extra functions: our object defines only what the retail unit object does

$ ./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve8022EBC8.c
PASS  -> kept as Matching

$ python3 tools/check_files_cmake.py
every configured DOL object is either in files.cmake or excluded with a reason

$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13591 -> 13592   linked 6639 -> 6640
  ok    check_symbol_names.py
  ok    flip_test MetroidPrime/ScriptLoader/Carve8022EBC8.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-8022ebc8
```

`gate.sh`'s per-function diff says `SPLIT main/auto_03_8022EBC8_text: 1 function(s) accounted for
across 1 new unit(s) in main (exact count match - a split, not a loss)` — i.e. nothing was lost
out of the `auto_*` unit, which is the one way a carve can quietly cost a function.

`linked` rose by 1 (6639 → 6640), which is the count the one rule is about: the unit is `Matching`
in `configure.py` **and** the build still reproduces retail with our object in the link.

## Lesson worth keeping (not a `NEW:` item)

**`tools/carve_diff.sh` cannot verify a carve that stores to a small-data slot.** It disassembles
the *unlinked* `.o`, so the `R_PPC_EMB_SDA21` relocation is still 0 and it prints
`ours: 00000000 stw r3,0(0)` against `retail: 8022ebc8 stw r3,-26512(r13)` and ends
`NOT byte-exact`. It reports the same for the already-merged, flip-verified `fn_8021F9B0`:

```
$ ./tools/carve_diff.sh 0x8021F9B0 0x8 build/G2ME01/obj/MetroidPrime/ScriptLoader/Carve8021F9B0.o fn_8021F9B0
  +0   retail: 8021f9b0 stw r3,-26736(r13) ours: 00000000 stw r3,0(0)
NOT byte-exact
```

So for any `stw/lwz ...,sym@sda21(r0)` carve the verdict is **not** `carve_diff.sh`. Use
`build/binutils/powerpc-eabi-objdump -dr` (the relocation names the symbol, which proves the right
slot) plus `build/G2ME01/asm/<unit>.s` against the `auto_*` listing, and `flip_test.sh` decides.
This affects the whole loader-setter family in `MetroidPrime/ScriptLoader/`; it is a
documentation/tooling observation, not a unit that can be matched, so no `NEW:` line.