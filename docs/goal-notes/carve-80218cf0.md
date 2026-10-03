# carve-80218cf0 — `MetroidPrime/ScriptLoader/Carve80218CF0.c`, 1 function, Matching

**Result: PASS.** `./tools/goal_check.sh build/goal/item.json` printed `goal_check: PASS
carve-80218cf0`, with `flip_test MetroidPrime/ScriptLoader/Carve80218CF0.c: PASS, Object(Matching)
in configure.py`. `matched 13573 -> 13574`, `linked 6621 -> 6622`, `sha1sum
build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (retail).

## What I did

Carved `.text 0x80218CF0..0x80218CF8` (0x8 = 8 bytes, 1 function) out of dtk's unclaimed
`main/auto_03_80218CF0_text` as a new `Matching` unit, in all four places the carve rule
requires.

- `src/MetroidPrime/ScriptLoader/Carve80218CF0.c` — **new**, the one body.
- `config/G2ME01/splits.txt:1707` — `MetroidPrime/ScriptLoader/Carve80218CF0.c: .text
  start:0x80218CF0 end:0x80218CF8`, between `SplitterMainChassis.cpp` (ends 0x80218CF0) and
  `ChozoGhost.cpp` (starts 0x80218CF8), i.e. address order, with no unclaimed gap on either
  side.
- `configure.py:875` — `Object(Matching, "MetroidPrime/ScriptLoader/Carve80218CF0.c"),` on
  **one line**, between the `SplitterMainChassis.cpp` and `ChozoGhost.cpp` entries.
- `files.cmake:905` — `    src/MetroidPrime/ScriptLoader/Carve80218CF0.c`, after
  `src/MetroidPrime/ScriptLoader/Carve80218A04.c` and before
  `src/MetroidPrime/ScriptLoader/Carve802201F8.cpp`, with a three-line comment giving the
  reason it is a `.c`.

`total_functions` is still **28465** after the `splits.txt` edit (measured from
`build/report.json`). No `PortLinkStubs.cpp` duplicate had to go: `grep -rn 'fn_80218CF0' src
include tools` finds only `src/MetroidPrime/ScriptObjects/CSplitterRelMain.cpp` (which declares
and calls it, it does not define it) plus the new file.

## The body, and why the twin is the right model

`build/G2ME01/asm/auto_03_80218CF0_text.s` is two instructions:

```
# .text:0x0 | 0x80218CF0 | size: 0x8
.fn fn_80218CF0, global
/* 80218CF0 00215AF0  90 6D 96 C0 */  stw r3, gLoader_SplitterMainChassis@sda21(r0)
/* 80218CF4 00215AF4  4E 80 00 20 */  blr
.endfn fn_80218CF0
```

The seeded twin `fn_80200E3C` is the same two instructions with a different `@sda21`
displacement, so the C is one store and a return:

```c
void fn_80218CF0(struct SSplitterFuncPtrs* record) { gLoader_SplitterMainChassis = record; }
```

`gLoader_SplitterMainChassis` is `.sbss 0x80419440`, `size:0x8 data:4byte`
(`config/G2ME01/symbols.txt:20721`), already claimed and already defined by the `Matching`
`src/MetroidPrime/ScriptLoader/SplitterMainChassis.cpp`, so this unit claims `.text` **only**
and takes the pointer as `extern`. That is the arrangement `Carve80200E3C.c` (SpacePirate) and
`Carve802189D0.c` (SandBoss) use, and it keeps the unmangled `fn_80218CF0` in the DOL link,
which is what module 75's two `bl fn_80218CF0` resolve against.

**The callers, read off module 75's own listing** rather than assumed
(`build/G2ME01/Splitter/asm/MetroidPrime/ScriptObjects/CSplitterRelMain.s`, the same listing
`src/MetroidPrime/ScriptObjects/CSplitterRelMain.cpp` already writes from):

- `RELExit` (0x8210, 0x24) does `li r3, 0x0` / `bl fn_80218CF0` — null on the way out.
- `fn_75_8254` (0x8254, 0x5C) does `lis r3, lbl_75_bss_20@ha` / `stwu r8, lbl_75_bss_20@l(r3)`
  then `bl fn_80218CF0` with `r3` still pointing at `lbl_75_bss_20`.

So the argument is the **address of a record**, which is why the store hands it
`&lbl_75_bss_20`. That record is 5 words written into a 0x18-byte `.bss` object
(`config/G2ME01/rels/Splitter/symbols.txt:669` gives `lbl_75_bss_20 = .bss:0x00000020;
size:0x18 data:4byte`; the 0x18 is `.bss`'s `align:8` rounding the 0x14 the body writes):
`fn_75_82B0` at +0, `fn_75_FC` at +4, and three words copied out of `.data`
(`lbl_75_data_CAC`) at +8/+0xc/+0x10, which is one 12-byte CodeWarrior
pointer-to-member-function. That is the same `SSplitter_FuncPtrs` `CSplitterRelMain.cpp`
already declares, so the local struct here is spelled as its two `FScriptLoader`s plus the
member pointer as three words — it is a `.c` unit and plain C has no member pointers.

**Plain C, and therefore a `.c`, is correct here rather than a compromise.** The body has no
`__ptmf_scall`, so nothing needs r12 (contrast `docs/goal-notes/carve-8024492c.md`, where a
twin that dereferences a member pointer forced a `.cpp`). The only requirement is that the
symbol stay unmangled: `symbols.txt:9498` carries the `fn_<addr>` placeholder
(`fn_80218CF0 = .text:0x80218CF0; // type:function size:0x8 align:4`), REL modules import that
exact name, and a C++ definition would mangle to `_Z<len>fn_80218CF0v` and objdiff would pair
nothing.

## Verification

```
$ ./tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve80218CF0.c
== MetroidPrime/ScriptLoader/Carve80218CF0.c  (config/G2ME01/splits.txt)
   .text      claimed      8   ours      8   retail      8   fits
   no extra functions: our object defines only what the retail unit object does

$ ./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve80218CF0.c
  PASS  -> kept as Matching
kept: 1 / 1   failed: 0   skipped: 0

$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13573 -> 13574   linked 6621 -> 6622
  ok    check_symbol_names.py
  ok    All:  37.69% fuzzy, 31.12% matched, 13.99% linked (13574 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptLoader/Carve80218CF0.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-80218cf0
```

From `build/report.json` after the build: `main/MetroidPrime/ScriptLoader/Carve80218CF0` is
`matched_code_percent 100.0`, `matched_functions 1 / 1`, `complete_units 1`. The gate's
per-function diff reports `SPLIT main/auto_03_80218CF0_text: 1 function(s) accounted for across
1 new unit(s) in main (exact count match - a split, not a loss)`.

`tools/check_decl_order.py --unit` and `tools/check_files_cmake.py` both pass (the latter:
`968 sources; 821 DOL objects; 282 documented exclusions`, `0 ... dead`).
`tools/check_symbol_names.py`: `checked 609 units; 0 declared names are missing`.
`powerpc-eabi-nm -n` on the built object gives `fn_80218CF0` at `+0`, retail order (one
function, so order cannot be wrong).

`tools/link_check.sh`: `unique undefined symbols 286`, matching the judge's recorded baseline
`build/goal/judge/undef.base.count` = `286`, and `duplicate definitions 0`. The file adds no
undefined symbol: nothing in the host link calls `fn_80218CF0`, and its only reference,
`gLoader_SplitterMainChassis`, is already defined by `SplitterMainChassis.cpp`, which the port
compiles.

## Notes for the next run

- **`tools/carve_diff.sh` cannot be used on a carve whose only symbol is an `@sda21` store,
  even a one-function one.** Run on our built object it prints `NOT byte-exact` with
  `+0 retail: stw r3,-26944(r13) ours: stw r3,0(0)` — the relocation is still
  `R_PPC_EMB_SDA21 gLoader_SplitterMainChassis` in the `.o`, and the displacement is only filled
  by mwldeppc. **The already-`Matching` twin `Carve802189D0.o` gives the same `NOT byte-exact`
  on the same tool** (`+0 retail: stw r3,-27040(r13) ours: stw r3,0(0)`), so the verdict is a
  property of the tool, not of this body. `flip_test.sh` and the DOL sha1 are the checks that
  decide it, and both are green. Worth knowing before reading a `NOT byte-exact` here as a
  spelling to iterate on.
- **`SplitterMainChassis.cpp`'s header comment is now stale.** It says "The 8-byte setter at
  0x80218CF0 is deliberately NOT claimed: REL modules import it by its retail name, so it
  cannot be renamed and must stay in dtk's auto unit." That is this file. The same sentence
  appears in `Carve802189D0.c`'s header as history ("used to reserve these eight bytes"), which
  is the wording to copy. Not edited here: this item's rule is to keep the diff to the carve,
  and the file belongs to a `Matching` unit this item does not claim. Recorded as
  `NEW:`-worthy documentation only — it moves no count, so it is not filed.
- **`CSplitterRelMain.cpp` was already waiting for this.** Its header claimed the setter "is
  **the plain DOL symbol**", with `fn_80218CF0` declared `extern "C"` and called under that
  name; MWCC does not encode a parameter type in a function name, so its
  `void fn_80218CF0(SSplitter_FuncPtrs* record)` and this file's
  `void fn_80218CF0(struct SSplitterFuncPtrs* record)` agree at link time. Both spellings are
  now true at once, which is the intended outcome.

No `NEW:` line: the item did not stall. The two notes above are a tooling defect and a stale
comment, neither of which is work whose success this item's success depends on.