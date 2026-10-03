# carve-8023e5a8

**Kind:** `match` **Target:** `MetroidPrime/ScriptLoader/Carve8023E5A8` **Result: PASS.**

`tools/goal_check.sh build/goal/item.json` printed, verbatim:

```
goal_check: item carve-8023e5a8 (match) target=MetroidPrime/ScriptLoader/Carve8023E5A8
goal_check: baseline /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal-L7/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13468 -> 13470   linked 6516 -> 6518
  ok    check_symbol_names.py
  ok    All:  37.54% fuzzy, 30.98% matched, 13.84% linked (13470 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptLoader/Carve8023E5A8.c: PASS, Object(Matching) in configure.py
goal_check: PASS carve-8023e5a8
```

## What I did

The carve shape, four files, each entry in address order:

- **`src/MetroidPrime/ScriptLoader/Carve8023E5A8.c`** (new, plain C, 2 functions,
  `.text 0x8023E5A8..0x8023E5F4`, 0x4C = 76 bytes).
- **`configure.py:906`** - `Object(Matching, "MetroidPrime/ScriptLoader/Carve8023E5A8.c"),`,
  between `Carve8023C950.c` (905) and `MetroidPrime/Carve80277090.c` (now 907). One line.
- **`config/G2ME01/splits.txt:1904-1905`** -
  `MetroidPrime/ScriptLoader/Carve8023E5A8.c:` / `.text start:0x8023E5A8 end:0x8023E5F4`,
  between the `Carve8023C950.c` entry (`.text` 0x8023C950..0x8023C998, 1901-1902) and the
  `ScriptLoader.cpp` entry (`.text` starts 0x80242894, 1907). Nothing else claims those 76
  bytes and no claim spans a gap.
- **`files.cmake:847`** - added after `Carve8023C950.c` (846), before `Carve802476D8.c` (848).

`build/report.json` after: `total_functions` **28465** (unchanged), `matched_functions`
13468 -> **13470**, `total_units` 2312 -> **2314**, `complete_units` 940 -> **941**. The unit
`main/MetroidPrime/ScriptLoader/Carve8023E5A8` is `metadata.complete: true`, 2/2 functions at
100.00%, `.text` 76/76 bytes.

## The two functions

Both are unclaimed dtk code in `auto_03_8023C998_text`
(`build/G2ME01/asm/auto_03_8023C998_text.s:2066-2092`), and both twins were exact.

**`fn_8023E5A8`, 0x8023E5A8, 0x3C = 60 bytes** - `__dt__5CMainFv` (0x800087DC, `src/MetroidPrime/
mainTail.cpp:135` `CMain::~CMain() {}`) word for word, `bl Free__7CMemoryFPCv` included:
receiver tested (`mr. r31,r3` / `beq`), flag re-tested as a **short** (`extsh. r0,r4` / `ble`),
`if (flag > 0)` **inside** `if (self)` so the receiver's `beq` lands on the epilogue, and the
epilogue returns the receiver in r3 (`mr r3,r31`) - which is what makes the return `void*` and
not `void` (as `void` mwcceppc drops the move, 14 instructions / 56 bytes). This is the
**third** copy of that exact body in the tree; `Carve8023C950.c` (`fn_8023C950`) and
`Carve8023C860.c` (`fn_8023C860`) are the first two and their comments are the prior
identification. Body copied from `Carve8023C950.c:99-106`.

Call sites, both in the unclaimed `auto_03_8009D644_text`, so nothing of ours needs it:
0x8009DABC and 0x8009DC14, `mr r3,r29 ; li r4,-1 ; bl fn_8023E5A8` either side of
`bl __dt__20SLdrEditorPropertiesFv`, in `fn_8009D644` (`symbols.txt:3106`, 0x5F8).

**`fn_8023E5E4`, 0x8023E5E4, 0x10 = 16 bytes** - `__ct__15CControllerAxisFv` (0x8030BF8C, the
inline `CControllerAxis() : mRelative(0.f), mAbsolute(0.f) {}` at
`include/Kyoto/Input/CControllerAxis.hpp:6`) exactly: `lfs f0, <zero> ; stfs f0, 0x0(r3) ;
stfs f0, 0x4(r3) ; blr`, one load and two stores, no frame. **The only difference from the
twin is where the zero comes from**: the twin reads its own translation unit's literal pool
(`"@244"@sda21(r0)`, `build/G2ME01/asm/Kyoto/Input/CDolphinController.s:798`); this copy reads
a named `.sdata2` object, `lbl_8041DCB0` (`symbols.txt:24608`, `.sdata2:0x8041DCB0`,
`data:float`), which dtk recorded as `.float 0`
(`build/G2ME01/asm/auto_11_8041DAC8_sdata2.s:567-570`). Same value, same four instructions,
both in .sdata2 so the `sda21` shape is identical. Checked arithmetically:
`_SDA2_BASE_` = 0x804223C0 (`tools/sda.py`) - 0x8041DCB0 = 0x4710 = -18192, which is the
`b8f0` half of retail's `c0 02 b8 f0`.

Single call site 0x8009DC60, `addi r3,r31,0x58 ; bl fn_8023E5E4`, in `fn_8009DC3C`
(`symbols.txt:3107`, 0xAC), immediately after `bl __ct__20SLdrEditorPropertiesFv` on the same
object and after `stw r0,0x3c(r31)` with `li r0,-1`. So it is a constructor's own zeroing of a
two-float member at +0x58, not a load from data.

## The one thing that did not work first time, and what fixed it

**`lbl_8041DCB0` needs a host block; declaring it `extern` and stopping there does not.**
`Carve80003858.c` gets away with `extern const float lbl_8041A3C0;` alone because
`docs/research/port_link_gap_list.md:279` already lists that symbol. Mine was new, so the
first `goal_check.sh` run failed its only check:

```
goal_check: FAIL carve-8023e5a8 - 1 failing check(s): gate.sh
        GATE FAIL: link-gap
```

and `build/gate-link.log` named it exactly:

```
link gap not accounted for:
  gap grew: lbl_8041DCB0 is not in port_link_gap_list.md
```

Fixed the `Carve80229EAC.c` way - a `#ifndef __MWERKS__` block after the externs,
`const float lbl_8041DCB0 = 0.f;`, carrying **retail's own value** as dtk recorded it rather
than a placeholder. Measured after: `probe: 916 files, 0 failed, 0 errors; link: LINKED (286
undefined, 0 duplicates)`, which is the gate's baseline (`build/goal/judge/undef.base.count`
= 286). The guard is `__MWERKS__` and not `TARGET_PC` because the thing that must not happen
is a **second** definition in the matching build, where dtk's `auto_11_8041DAC8_sdata2.o`
owns the pool entry.

**Generalised: a carve that reads a retail `.sdata2`/`.sbss` object dtk does not emit for us
needs its own host definition, or `gate.sh`'s `port link gap` step fails.** Two ways out, and
the cheap one is the guard: documenting it in `port_link_gap_list.md` costs a docs edit and a
new baseline claim, the `#ifndef __MWERKS__` definition costs five lines. Check
`grep -rn <symbol> docs/research/port_link_gap_list.md` before copying a `Carve*.c` data
extern verbatim.

## Verification

- `./tools/flip_test.sh MetroidPrime/ScriptLoader/Carve8023E5A8.c` -> `PASS -> kept as Matching`,
  twice (before and after the comment edits).
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (the pinned hash)
  after every build.
- `./tools/unit_fit.sh MetroidPrime/ScriptLoader/Carve8023E5A8.c`:
  `.text claimed 76 ours 76 retail 76 fits`, `no extra functions`.
- `./tools/carve_diff.sh 8023E5A8 0x4C build/G2ME01/obj/MetroidPrime/ScriptLoader/Carve8023E5A8.o`
  -> `retail: 19 instructions, 76 bytes` / `ours: 19 instructions, 76 bytes`, 2 "differing"
  instructions which are the two **unresolved relocations** in the `.o` -
  `bl 20 <fn_8023E5A8+0x20>` for `Free__7CMemoryFPCv` and `lfs f0, 0(0)` for
  `lbl_8041DCB0` - which is what an object with `U Free__7CMemoryFPCv` / `U lbl_8041DCB0`
  must look like. The linked DOL hash is what settles them.
- Descending source order confirmed in the object:
  `powerpc-eabi-nm` shows `T fn_8023E5A8` at `00000000` and `T fn_8023E5E4` at `0000003c`,
  i.e. the higher address (`fn_8023E5E4`) is written second in the file and emitted first.
- `tools/check_decl_order.py --unit MetroidPrime/ScriptLoader/Carve8023E5A8.c` prints
  `ok: 0 unit(s) checked` - it does **not** pick up a `.c` unit, so its "ok" is vacuous here;
  the `nm` offsets above are the real check.
- `./tools/probe_sources.sh` -> `916 files, 0 failed, 0 errors; link: LINKED (286 undefined,
  0 duplicates)`.

## Caveats

- `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in `git status`. Those
  are **tool-written** derived counts (`matched 13470`, `linked 6518`, `probe 916`), produced
  by `tools/probe_sources.sh` / the gate, not by me. The driver discards them before judging.
- No `PortLinkStubs.cpp` entry and no duplicate: `grep -rn 'fn_8023E5A8\|fn_8023E5E4' src/
  include/` returns only the new file.
- I did not add a row to `RUNNING_THE_DECOMP.md`'s module table - this is a carve of a DOL
  function, not a REL module attempt, and that file is one I am told not to edit.
- No `NEW:` lines: nothing was blocked, and nothing found here is new work.