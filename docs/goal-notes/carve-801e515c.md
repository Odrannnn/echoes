# carve-801e515c

**Kind:** `match` **Target:** `MetroidPrime/ScriptObjects/Carve801E515C` — **PASS**

New `Matching` unit `src/MetroidPrime/ScriptObjects/Carve801E515C.c`, claiming
`.text 0x801E515C..0x801E51A4` (0x48 = 72 bytes, 2 functions): `fn_801E515C`
(`config/G2ME01/symbols.txt:7824`, 0x24, 9 instructions) and `fn_801E5180` (`symbols.txt:7825`,
0x24, 9 instructions). Definitions are **descending by address**. `tools/flip_test.sh` PASS,
`./tools/goal_check.sh build/goal/item.json` exit 0, `GATE PASS`.

## The two functions, and what fixes their shape

Both are one-call `this + 4` forwarders, and the item's twin is exact. The twin named by the seeder
is `GetResourceIdByName__11CResFactoryCFPCc` (0x80006B80, 0x24, the already-`Matching`
`main/MetroidPrime/main` unit, 100.00%, source `include/Kyoto/CResFactory.hpp:64-65`):

    const SObjectTag* CResFactory::GetResourceIdByName(const char* name) const {
      return mResLoader.GetResourceIdByName(name);
    }

Measured word for word against the disc (`orig/G2ME01/sys/main.dol`, text section 1: file offset
0x640 at address 0x80003840, size **0x3A23C0**, read from the DOL header this run), each of the two
byte-shapes is **8 of 9 words identical** to that twin; the one differing word is the `bl` at
offset 0x10 — twin `48 2f 60 b5` → 0x802FCC44 (`GetResourceIdByName__10CResLoaderCFPCc`),
`fn_801E515C` `48 00 00 39` → 0x801E51A4, `fn_801E5180` `48 00 00 a1` → 0x801E5230 (targets decoded
from the encoded LI). So the shape is the twin's and only the two callees are this copy's own.

Callers, read out of the disc, fix the argument list at one pointer plus the pass-through r4:

- `fn_801E4F0C` (0x801E4F0C, 0x90, unclaimed) calls `fn_801E515C` at 0x801E4F50 with
  `r3 = lwz 0x4028(r27) + r31` (r31 stepping `0x10` per element) and `r4 = r28`; and calls the
  callee **directly** at 0x801E4F2C with `addi r3, r27, 0x402c` — 0x402c is 0x4028 + 4, the same
  adjustment this file makes.
- `fn_801E4F9C` (0x801E4F9C, 0xBC, unclaimed) calls `fn_801E5180` at 0x801E5004 with
  `r3 = 0x4028(r28) + (index << 4)`, `index` a signed halfword (`lha`), `r4 = r29`; and calls
  `fn_801E5230` directly at 0x801E5028 with `addi r3, r28, 0x402c`.
- `fn_801E4F0C` at 0x801E4F54 consumes `fn_801E515C`'s returned r3 as a truth value (`or r0,r30,r3`
  then `clrlwi`), which is `fn_801E51A4`'s `li r3,0` / `li r3,1` passed through.

Bodies written, plain C so the `fn_` names do not mangle (a C++ one would be `_Z<len>fn_<addr>v`
and objdiff would pair nothing):

    void* fn_801E515C(void* self, const void* key) { return fn_801E51A4((char*)self + 4, key); }
    void* fn_801E5180(void* self, const void* key) { return fn_801E5230((char*)self + 4, key); }

Nothing is asserted about the receiver's class: it never appears in either body, so the pointer is
`void*` and the `+4` is `(char*)self + 4`.

## Byte checks, and which input is which

- Our object `build/G2ME01/src/MetroidPrime/ScriptObjects/Carve801E515C.o`: `.text` is 72 bytes,
  and **64 of 72 bytes are identical to the disc** — the two differing words are at 0x10 and 0x34
  and both read `48 00 00 01` because their relocations are still unresolved in the object.
- The **built** `build/G2ME01/main.dol` vs the disc at 0x801E515C..0x801E51A4: **72 of 72 bytes
  identical** (whole text section 1 identical), sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `tools/carve_diff.sh` reads `main.elf` for "retail", which is *us* after the flip, so its verdict
  is an artefact of its inputs; the disc comparison above is the measurement.

## The two PortLinkStubs entries (no duplicate to retire)

`fn_801E515C` / `fn_801E5180` had **no** stub in `PortLinkStubs.cpp`, so nothing was retired. The
carve's two callees are both just past the claim and unclaimed, so the port's flat link would grow:
`python3 tools/link_gap.py --rebuild` without the new blocks reads **287 MISSING** with
`gap grew: fn_801E51A4 ... / fn_801E5230 ...`; with `stub_199` = `fn_801E51A4` and `stub_200` =
`fn_801E5230` (empty stand-ins, one block at `PortLinkStubs.cpp:1121-1142`) the same command reads
**285 MISSING, all accounted for** — the judge's baseline. Header clauses updated from the file's
own measurements: `grep -cE 'asm\("'` **169 → 171**, `grep -cE 'asm\("(fn_|lbl_)'` **33 → 35**, so
the breakdown reads 167 functions / 4 data objects and the residual game-method term stays 45.

## Verification (measured on this tree)

- `./tools/goal_check.sh build/goal/item.json` → exit 0, `PASS carve-801e515c`
- counts: matched **12687 → 12689**, linked **6046 → 6048** (baseline read from this tree's
  `build/report.json` before the change); `All: 35.54% fuzzy, 29.39% matched, 13.06% linked
  (12689 / 28465 functions)` — `total_functions` still **28465** after the `splits.txt` edit
- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801E515C.c` → `PASS -> kept as Matching`;
  gate `hashes vs config.yml ok`, 86 RELs cmp-equal, DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`
- gate log `GATE PASS 5dd4446f+7 changed`; `per-function diff SPLIT ... 16 function(s) moved into
  main/MetroidPrime/ScriptObjects/Carve801E515C, main/auto_03_801E51A4_text (exact count match - a
  split, not a loss)`
- `./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801E515C.c` → `claimed 72, ours 72,
  retail 72, fits`, `no extra functions`
- `python3 tools/check_symbol_names.py` → clean (goal_check step `ok`)
- probe: **811 files, 0 failed, 0 errors**; `link: LINKED (291 undefined, 0 duplicates)` — the
  port's undefined count did not move and `port link dups` is 0
- layout after the claim, from `build/report.json`: `main/auto_03_801E3E38_text` is now **4900 B /
  14 functions**, ending exactly at the claim (last function `__ct__11CPortalAreaFRC31TLockedToken<15CPortalAreaData>`
  at 0x801E5058 + 0x104); the new `main/MetroidPrime/ScriptObjects/Carve801E515C` is 72 B /
  2 functions, both 100.00% and complete; `main/auto_03_801E51A4_text` starts at 0x801E51A4
  (7828 B / 14 functions)
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801E515C.c` →
  `ok: 0 unit(s) checked` — carve units are not covered by that tool, so it proves nothing here.
  Proved it directly instead: `powerpc-eabi-nm --defined-only -n` reads `fn_801E515C @ 0x0`,
  `fn_801E5180 @ 0x24`, i.e. descending source order came out as retail's ascending `.text`.

## Files

- `src/MetroidPrime/ScriptObjects/Carve801E515C.c` (new, 97 lines, definitions descending)
- `configure.py:749` — one-line `Object(Matching, ...)`, in address order between
  `Carve801E3E34.c` and `Cameras/Carve801E7C14.c`
- `config/G2ME01/splits.txt:1163-1164` — `.text start:0x801E515C end:0x801E51A4`
- `files.cmake:573` — the source, same position
- `src/MetroidPrime/PortLinkStubs.cpp:12` and `:39-40` (header counts), `:1121-1142`
  (`stub_199` / `stub_200` with their rationale)

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified: that is `goal_check.sh`'s own
docs step (`MP_GATE_DOCS_WRITE`) rewriting the derived counts, which the driver discards. No hand
edit to them by this run.

## Left alone, and for the next run

- `fn_801E51A4` (0x801E51A4, 0x8C) and `fn_801E5230` (0x801E5230, 0x4C) stay retail's; they are now
  the head of `auto_03_801E51A4_text`. Both are real bodies (a halfword-keyed list search with a
  `b` to its loop test, and an allocate-through-`fn_801E528C`-and-link), not forwarders, which is why
  this claim stops at 0x801E51A4 and why they are stubbed in the port. They are the natural next
  carve here, but a body that needs the container's layout rather than a twin is the seeder's call,
  so no `NEW:` is filed on an unmeasured guess.
- In front of the claim, `fn_801E4F0C` (0x801E4F0C, 0x90) and `fn_801E4F9C` (0x801E4F9C, 0xBC) are
  the two callers and are unclaimed; the constructor between them and the claim is real-named.

No `WALL:`, no `NEW:` lines.

## Lane 12: passed, then failed on the moved tip (2026-10-02 14:03:15Z)

The judged change failed goal_check.sh (exit 1) once rebased onto 0291b134387b; re-do it against the current tip.

## Lane 12 redo on tip 0291b134 (2026-10-02) — PASS

Re-done in `../wt-mp2-goal-L12` against the current tip `0291b134`. Same carve, **new stub
numbers**; everything below was measured in this worktree this run, not recalled.

**Why the previous run failed on the moved tip (read out of the judge's own log,
`build/goal/check-gate.log`, not inferred):** the rebased change added `stub_199`/`stub_200` for
`fn_801E51A4`/`fn_801E5230`, but the tip's `0291b134 match: carve-801fdaa4` had taken `stub_199`
for `fn_801FDAE8`. Result:
`port probe ... link_check: compile errors 1 / src/MetroidPrime/PortLinkStubs.cpp:1154:17: error:
redefinition of 'void stub_199()' / the LINKER NEVER RAN ... STRICT FAIL ... GATE FAIL: probe
link-gap link-dups`. The carve itself was never wrong - a name collision, four lines up the file.

**The change this run** (7 files; identical to the previous attempt except the stub block):

- `src/MetroidPrime/ScriptObjects/Carve801E515C.c` (new, 97 lines, definitions descending)
- `configure.py:749` — one-line `Object(Matching, ...)`, between `Carve801E3E34.c` and
  `Cameras/Carve801E7C14.c`
- `config/G2ME01/splits.txt:1166-1167` — `.text start:0x801E515C end:0x801E51A4`
- `files.cmake:573` — the source, same position
- `src/MetroidPrime/PortLinkStubs.cpp` — `stub_200` = `fn_801E51A4`, `stub_201` = `fn_801E5230`
  (one block, appended after the tip's `stub_199`), plus the header counts

**Bytes, re-measured from the disc this run** (`orig/G2ME01/sys/main.dol`, text section 1: file
offset 0x640 at address 0x80003840):

    801E515C  94 21 FF F0 7C 08 02 A6 38 63 00 04 90 01 00 14 | 48 00 00 39 | 80 01 00 14 7C 08 03 A6 38 21 00 10 4E 80 00 20
    801E5180  ...                                            | 48 00 00 a1 | ...
    80006B80  twin GetResourceIdByName__11CResFactoryCFPCc   | 48 2F 60 B5 | ...

8 of 9 words identical to the twin, only the `bl` differs; LI decoded: 0x801E516C+0x38 = 0x801E51A4
and 0x801E5190+0xA0 = 0x801E5230. Both are `stwu r1,-0x10 / mflr r0 / addi r3,r3,4 / stw r0,... /
bl / lwz / mtlr / addi / blr`, i.e. the twin's shape with only the call target changed. The callers
(`fn_801E4F0C` 0x801E4F0C, `fn_801E4F9C` 0x801E4F9C, both unclaimed) were re-read out of dtk's asm
(`build/G2ME01/asm/auto_03_801E3E38_text.s:1198-1290`) and confirm r3 = member at +0x4028 (+4 =
0x402c when called directly) and r4 = the caller's own third argument.

**Verification (this tree, this run):**

- `./tools/decomp_build.sh` → `sha1sum build/G2ME01/main.dol` =
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; `total_functions` **28465**;
  `matched_functions` **12693 → 12695** (baseline `build/goal/judge/report.base.json` at 0291b134
  reads 12693), `complete_code` 853652 → **853724** (+72, exactly the claim);
  report unit `main/MetroidPrime/ScriptObjects/Carve801E515C` = 72 B, 2 functions, both 100.00%;
  the split leaves `main/auto_03_801E51A4_text` = 0x801E51A4..0x801E7038 (7828 B, 14 functions)
- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801E515C.c` → `PASS -> kept as Matching`
- `./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801E515C.c` → `claimed 72, ours 72, retail 72,
  fits`, `no extra functions`
- `powerpc-eabi-nm --defined-only -n` on our object → `fn_801E515C @ 0x0`, `fn_801E5180 @ 0x24`
  (descending source order = retail's ascending `.text`; `check_decl_order.py` does not cover carve
  units)
- `python3 tools/link_gap.py --rebuild`, **without** the two blocks: `287  MISSING`,
  `gap grew: fn_801E51A4`, `gap grew: fn_801E5230`; **with** them: `285  MISSING`,
  `ok: 285 MISSING symbol(s), all accounted for in port_link_gap_list.md`
- header counts, derived from the file itself: `grep -cE 'asm\("'` **169 → 171**,
  `grep -cE 'asm\("(fn_|lbl_)'` **33 → 35**; the prose now reads 167 functions / 4 data objects and
  the breakdown 86 REL loader, 45 game method, 35 unmangled, 1 allocator, 4 vtable/typeinfo
- `./tools/goal_check.sh build/goal/item.json` → **PASS carve-801e515c**, `ok no judge-owned path
  touched`, `ok gate.sh`, `ok counts: matched 12693 -> 12695   linked 6052 -> 6054`,
  `ok check_symbol_names.py`, `ok flip_test ... PASS, Object(Matching) in configure.py`;
  `build/goal/check-gate.log` = `GATE PASS  0291b134+7 changed`, all steps ok, per-function diff
  `SPLIT main/auto_03_801E3E38_text: 16 function(s) moved into ... Carve801E515C,
  main/auto_03_801E51A4_text (exact count match - a split, not a loss)`

**Trap worth recording for the next lane here:** this worktree's `build/report.json` on arrival was
a stale artefact of the *previous* attempt (it listed `Carve801E515C` while no such source or
`configure.py` entry existed), and `build/G2ME01/asm/MetroidPrime/ScriptObjects/Carve801E515C.s`
was likewise left over. `build/` is gitignored, so `git reset --hard` does not clear it. The
autoritative baseline is `build/goal/judge/report.base.json` (clean at 0291b134); re-run
`./tools/decomp_build.sh` before trusting any count.

**Left alone, unchanged from the previous run's call:** `fn_801E51A4` (0x801E51A4, 0x8C) is now the
head of `main/auto_03_801E51A4_text` and stays retail's; it and `fn_801E5230` (0x801E5230, 0x4C) are
real bodies (list search / allocate-and-link through `fn_801E528C`) that need the container's
layout, so they are not a twin carve and no `NEW:` is filed on that guess.

No `WALL:` (nothing blocked), no `NEW:` lines.

## Lane 12, third attempt, on tip 80db31e1 (2026-10-02) — PASS

Redone from scratch in `../wt-mp2-goal-L12` at **80db31e1** (`match: carve-801e8028`). The previous
run's PASS on 0291b134 is **not** in this tree: `git log --all | grep 515c` is empty and neither
`configure.py` nor `splits.txt` carried a trace. The driver's own log says why — `build/goal/run.log`
14:19:40-14:20:07: the judged change did apply to 80db31e1, but `rebase.patch` came out **1.3 GB**
(`docs/HANDOFF.md` alone is a 500 MB blob) and `git apply` died with `error: patch too large`, so
the item was released for a fresh attempt. Nothing was wrong with the carve itself. This run's
artifacts were recovered from that patch (`build/goal/rebase.patch:8814253-8814432`) and then
re-measured here, not trusted: every address, byte and call site below was read again on this tree.

**What differed from the last attempt and had to be re-derived:** the stub numbers. At 0291b134 the
block was `stub_200`/`stub_201`; at 80db31e1 `stub_200` is `fn_801FEE88` (carve-801fee40, e2acc22d),
so this run's are **`stub_201` = `fn_801E51A4`, `stub_202` = `fn_801E5230`**
(`src/MetroidPrime/PortLinkStubs.cpp:1171-1191`), and the header clauses moved with them. This is
the second time in a row a moving tip broke this item on stub numbering alone; always read the
current last `stub_N` out of the file, never reuse the number a note quotes.

**The change (7 paths, 4 of them the carve):**

- `src/MetroidPrime/ScriptObjects/Carve801E515C.c` (new, 97 lines, definitions **descending**:
  `fn_801E5180` then `fn_801E515C`)
- `configure.py:750` — one-line `Object(Matching, "MetroidPrime/ScriptObjects/Carve801E515C.c")`,
  between `Carve801E3E34.c` and `Cameras/Carve801E7C14.c`
- `config/G2ME01/splits.txt:1166-1167` — `.text start:0x801E515C end:0x801E51A4`
- `files.cmake:574` — the source, same position
- `src/MetroidPrime/PortLinkStubs.cpp` — the `stub_201`/`stub_202` block and the two header clauses
- (the driver's `goal_check.sh` rewrites `docs/HANDOFF.md` / `docs/RUNNING_THE_DECOMP.md` itself)

Bodies (plain C, so the `fn_` names do not mangle; the casts are for `probe_sources.sh`, which
syntax-checks every `files.cmake` source as C++ and leaves the object byte-identical):

    void* fn_801E5180(void* self, const void* key) { return fn_801E5230((char*)self + 4, key); }
    void* fn_801E515C(void* self, const void* key) { return fn_801E51A4((char*)self + 4, key); }

**Re-measured from the disc this run** (`orig/G2ME01/sys/main.dol`, text section 1 at file offset
0x640 / address 0x80003840, size 0x3A23C0, read out of the DOL header):

    801E515C  94 21 FF F0 7C 08 02 A6 38 63 00 04 90 01 00 14 | 48 00 00 39 | 80 01 00 14 7C 08 03 A6 38 21 00 10 4E 80 00 20
    801E5180  (same 8 words)                                    | 48 00 00 a1 |
    80006B80  twin GetResourceIdByName__11CResFactoryCFPCc     | 48 2F 60 B5 |

8 of 9 words identical to the twin, only the `bl` differs. LI decoded: 0x801E516C+0x38 = 0x801E51A4
and 0x801E5190+0xA0 = 0x801E5230. Callers re-read out of `build/G2ME01/asm/auto_03_801E3E38_text.s`:
`fn_801E4F0C` calls `fn_801E51A4` directly at 0x801E4F2C with `addi r3, r27, 0x402c` / `mr r4, r28`
and `fn_801E515C` at 0x801E4F50; `fn_801E4F9C` calls `fn_801E5180` at 0x801E5004 with
`r3 = 0x4028(r28) + (lha index << 4)`, `r4 = r29`, and `fn_801E5230` at 0x801E5028 with the same
`+0x402c`.

**Bytes:** our object `build/G2ME01/src/MetroidPrime/ScriptObjects/Carve801E515C.o` `.text` is
72 bytes and 64 of 72 are identical to the disc; the two differing words are at 0x10 and 0x34 and
both read `48 00 00 01` (relocations unresolved in the object). The **built** `build/G2ME01/main.dol`
vs the disc at 0x801E515C..0x801E51A4 is **72/72 identical**; DOL sha1
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. `tools/carve_diff.sh` still reads `main.elf` for
"retail" (ours after the flip), so the disc comparison is the measurement, not its verdict.

**Verification, all this tree / this run:**

- `./tools/decomp_build.sh` → DOL sha1 above, `total_functions` **28465**, `matched_functions`
  12697 (judge `build/goal/judge/report.base.json` at 80db31e1) → **12699**,
  `complete_code` 853796 → **853868** (+72, exactly the claim), `linked` 6056 → 6058
- report units: `main/MetroidPrime/ScriptObjects/Carve801E515C` = 72 B / 2 functions, both 100.00%;
  `main/auto_03_801E3E38_text` now 0x801E3E38..0x801E515C (4900 B, 14 functions, last is
  `__ct__11CPortalAreaFRC31TLockedToken<15CPortalAreaData>` at 0x801E5058 + 0x104);
  `main/auto_03_801E51A4_text` = 0x801E51A4..0x801E7038 (7828 B, 14 functions)
- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801E515C.c` → `PASS -> kept as Matching`
- `./tools/unit_fit.sh` → `claimed 72, ours 72, retail 72, fits`, `no extra functions`
- `build/binutils/powerpc-eabi-nm --defined-only -n` on our object → `fn_801E515C @ 0x0`,
  `fn_801E5180 @ 0x24`: descending source order came out as retail's ascending `.text`
  (`check_decl_order.py --unit` reads `ok: 0 unit(s) checked` — it does not cover carves)
- `python3 tools/check_symbol_names.py` → 532 units checked, 0 missing
- `python3 tools/link_gap.py --rebuild` **without** the two blocks: `287 MISSING`,
  `gap grew: fn_801E51A4`, `gap grew: fn_801E5230`; **with** them: `285 MISSING`,
  `ok: 285 MISSING symbol(s), all accounted for in port_link_gap_list.md` (the 285 baseline)
- header counts derived from the file itself: `grep -cE 'asm\("'` **169 → 171**,
  `grep -cE 'asm\("(fn_|lbl_)'` **33 → 35**, `asm("...") = {}` **4**, so the prose reads
  167 functions / 4 data objects and 86 REL loader / 45 game method / 35 unmangled / 1 allocator /
  4 vtable/typeinfo
- `./tools/goal_check.sh build/goal/item.json` → **PASS carve-801e515c**, `ok no judge-owned path
  touched`, `ok gate.sh`, `ok counts: matched 12697 -> 12699  linked 6056 -> 6058`,
  `ok check_symbol_names.py`, `ok flip_test`; `build/goal/check-gate.log` = `GATE PASS
  80db31e1+7 changed`, every step ok, per-function diff `SPLIT main/auto_03_801E3E38_text:
  16 function(s) moved into main/MetroidPrime/ScriptObjects/Carve801E515C,
  main/auto_03_801E51A4_text (exact count match - a split, not a loss)`
- probe (`build/gate-probe.log`): **817 files, 0 failed, 0 errors**; `link: LINKED (291 undefined,
  0 duplicates)` — the port's undefined count did not move and there are no duplicate definitions

**Stale-build-artifact trap, again (unchanged advice):** on arrival this worktree's `build/` still
held the *previous* attempt's `build/G2ME01/asm/MetroidPrime/ScriptObjects/Carve801E515C.s` and its
`.o` although no such source or `configure.py` entry existed; `build/` is gitignored so
`git reset --hard` does not clear it. The authoritative baseline is
`build/goal/judge/report.base.json` (recorded at 80db31e1, 12697 matched, 291 port undefined), and
the build must be re-run before any count from `build/report.json` is trusted.

**Left alone, same call as before:** `fn_801E51A4` (0x801E51A4, 0x8C) and `fn_801E5230`
(0x801E5230, 0x4C) are real bodies (a halfword-keyed list search with a `b` to its loop test, and an
allocate-through-`fn_801E528C`-and-link), not forwarders, so they neither belong in this claim nor
are they a twin carve — no `NEW:` is filed on that guess. In front of the claim `fn_801E4F0C`
(0x801E4F0C, 0x90) and `fn_801E4F9C` (0x801E4F9C, 0xBC) are the two callers and are unclaimed.

**Process note for the driver, not a `NEW:` item:** a `match` item whose diff must be carried across
a moving tip can be lost to `rebase.patch` size alone (this repo's `docs/HANDOFF.md` is a huge blob),
which wastes a whole lane and leaves no trace in the new tree. Splitting the docs out of the carried
patch, or carrying only `src/`, `configure.py`, `config/`, `files.cmake`, would have kept this one.

No `WALL:` (nothing blocked), no `NEW:` lines.

## Lane 12, fourth attempt, on tip 7196a62c (2026-10-02) — PASS

Re-done from scratch in `../wt-mp2-goal-L12` at **7196a62c** (`match: carve-80004438`, parent
80db31e1).  Nothing of the three earlier passes was in this tree: `grep Carve801E515C` found no hit
in `configure.py`, `files.cmake` or `splits.txt`, and no source file.  `build/` *did* still hold the
previous attempt's `build/G2ME01/src/MetroidPrime/ScriptObjects/Carve801E515C.o`,
`build/G2ME01/asm/MetroidPrime/ScriptObjects/Carve801E515C.s` and a stale
`build/G2ME01/asm/auto_03_801E51A4_text.s` (mtime 16:23, i.e. from a run whose splits edit is not in
this tree); all three were deleted before the first build, and `build/report.json` on arrival matched
`build/goal/judge/report.base.json` exactly (12699 matched), so the counts here are this tip's.

**This is the fourth consecutive run at this item and the first whose stub numbering did not move.**
At 80db31e1 the last block was `stub_200` (`fn_801FEE88`) and it still is at 7196a62c, so this run's
blocks are **`stub_201` = `fn_801E51A4`**, **`stub_202` = `fn_801E5230`**
(`src/MetroidPrime/PortLinkStubs.cpp:1189-1193`).  Read the current last `stub_N` out of the file
every time; do not reuse the number a note quotes.

**The change (5 paths, 4 of them the carve):**

- `src/MetroidPrime/ScriptObjects/Carve801E515C.c` (new, 57 lines, definitions **descending**:
  `fn_801E5180` then `fn_801E515C`, with the callees declared at the top)
- `configure.py:751` — one-line `Object(Matching, "MetroidPrime/ScriptObjects/Carve801E515C.c")`,
  between `Carve801E3E34.c` and `Cameras/Carve801E7C14.c`
- `config/G2ME01/splits.txt:1169-1170` — `.text start:0x801E515C end:0x801E51A4`
- `files.cmake:575` — the source, same position
- `src/MetroidPrime/PortLinkStubs.cpp` — the `stub_201`/`stub_202` block (`:1167-1193`) and the
  header clauses: `supplies 169` -> **171**, `165 functions` -> **167**, `33 unmangled` -> **35**

Bodies (plain C, so the `fn_` names do not mangle; the casts are for `probe_sources.sh`, which
compiles every `files.cmake` source as **C++** - `g++ ... -fsyntax-only`, so an implicit
`void*`->`char*` or a const-discarding argument is a hard error there and leaves the object
byte-identical):

    extern void* fn_801E5230(void* self, const void* key);
    extern void* fn_801E51A4(void* self, const void* key);

    void* fn_801E5180(void* self, const void* key) { return fn_801E5230((char*)self + 4, key); }
    void* fn_801E515C(void* self, const void* key) { return fn_801E51A4((char*)self + 4, key); }

**Re-measured from the disc this run** (`orig/G2ME01/sys/main.dol`, DOL header parsed, not recalled:
text section 1 at file offset **0x640** / address **0x80003840** / size **0x3a1c60**):

    801E515C  9421FFF0 7C0802A6 38630004 90010014 | 48000039 | 80010014 7C0803A6 38210010 4E800020
    801E5180  (same 8 words)                      | 480000A1 |
    80006B80  twin GetResourceIdByName__11CResFactoryCFPCc | 482F60B5 |

8 of 9 words identical to the twin; LI decoded: 0x801E516C+0x38 = 0x801E51A4, 0x801E5190+0xA0 =
0x801E5230.  Our object's `.text` is 72 bytes with **70 of 72** bytes identical to the disc - the two
differing words are 0x10 and 0x34 and both read `48 00 00 01` (relocations unresolved in the object).
The **built** `build/G2ME01/main.dol` vs the disc at 0x801E515C..0x801E51A4 is **72/72 identical**
(compared with both DOL headers parsed: section 1 at 0x640/0x80003840 in both).

**Verification, all measured on this tree this run:**

- `./tools/decomp_build.sh` -> DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`,
  `total_functions` **28465**, `matched_functions` 12699 -> **12701** (baseline
  `build/goal/judge/report.base.json` = 12699), `complete_code` 853864 -> **853936** (+72, exactly
  the claim), `matched_code` 1921056 -> 1921128; `linked` 6058 -> 6060
- report unit `main/MetroidPrime/ScriptObjects/Carve801E515C` = 72 B, 2/2 functions, **100.00% fuzzy,
  100.00% matched, complete**; the split leaves `main/auto_03_801E3E38_text` at
  0x801E3E38..0x801E515C (4900 B, 14 functions, last
  `__ct__11CPortalAreaFRC31TLockedToken<15CPortalAreaData>` at 0x801E5058+0x104) and
  `main/auto_03_801E51A4_text` at 0x801E51A4.. (14 functions).  Per-function diff over all 27911
  functions vs the baseline: **0 worse, 0 gone, 0 new names**
- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801E515C.c` -> `PASS -> kept as Matching`
- `./tools/unit_fit.sh` -> `claimed 72, ours 72, retail 72, fits`, `no extra functions`
- `build/binutils/powerpc-eabi-nm --defined-only -n` on our object -> `fn_801E515C @ 0x0`,
  `fn_801E5180 @ 0x24`: descending source order came out as retail's ascending `.text`
  (`check_decl_order.py --unit` reads `ok: 0 unit(s) checked` - it does not cover carves)
- `python3 tools/link_gap.py --rebuild` **without** the two blocks: `287 MISSING`, `gap grew:
  fn_801E51A4`, `gap grew: fn_801E5230`; **with** them: `285 MISSING`,
  `ok: 285 MISSING symbol(s), all accounted for in port_link_gap_list.md` (the list file itself is
  unchanged - the stubs are new *definitions*, so nothing was added to the gap)
- header counts derived from the file itself: `grep -cE 'asm\("'` **169 -> 171**,
  `grep -cE 'asm\("(fn_|lbl_)'` **33 -> 35**, `asm("_ZT` **4**, so the prose reads 167 functions /
  4 data objects and 86 REL loader + 45 game method + 35 unmangled + 1 allocator + 4 vtable = 171
- `./tools/goal_check.sh build/goal/item.json` -> **PASS carve-801e515c**, exit 0; every step `ok`;
  `build/goal/check-gate.log` = `GATE PASS  7196a62c+7 changed`, per-function diff
  `SPLIT main/auto_03_801E3E38_text: 16 function(s) moved into
  main/MetroidPrime/ScriptObjects/Carve801E515C, main/auto_03_801E51A4_text (exact count match -
  a split, not a loss)`, `port probe ok`, `port link gap ok`

## What this run tried that the notes did not: the two callees, measured

Both earlier runs left `fn_801E51A4` (0x801E51A4, 0x8C) and `fn_801E5230` (0x801E5230, 0x4C) alone,
calling them a 0x8C list search and an allocate-and-link "that need the container's layout", and
filed no `NEW:` because it was unmeasured.  This run measured them **without touching the tree**: a
scratch `.c` compiled with this unit's exact flags (`build.ninja:7038-7054`, mwcc GC/2.7
`-O4,p -inline deferred,noauto ... -lang=c`), `objcopy -O binary --only-section=.text` and a byte
compare against the disc - the same verdict as objdiff, with no claim involved.

- **`fn_801E5230` matched on the first spelling, byte-exact.**  76/76 bytes; the only differing word
  is the `bl` at offset 0x20, whose retail LI decodes to 0x801E5250+0x3c = **0x801E528C**:
  `char* node = fn_801E528C(*(void**)self); node[0] = key; node[1] = self[1]; self[1] = node;` -
  a push-front onto the chain at +4, the node being `{key at +0, next at +4}`.
- **`fn_801E51A4` is 140/140 bytes with an identical instruction sequence and a register-only diff.**
  Ten spellings, all 140 bytes (33 instructions) and the same 35 words in the same order; the best
  (v2: `unsigned short k = *key; char* head = self[1]; char* prev = 0; char* node = head;` with the
  comparison written `k == *(unsigned short*)(*(char**)node + 8)` and the `free`+`return 1`
  **duplicated into both branches**) leaves 13 differing words, all of them the same three values:
  `node` in r8 (retail **r4**), `k` in r5 (retail **r0**), and the first two loads in the other order
  (retail hoists `lhz r0,0x0(r4)` above `lwz r6,0x4(r3)`).  The `b`-to-test loop, the duplicated
  head/mid unlink tails, `li r7,0`, `cmplw`/`cmplwi` shapes, `li r3,1`/`li r3,0` and the frame are
  all already retail's.  Spellings that are *worse* and should be skipped: no `k` local (128 bytes,
  key reloaded inside the loop), `head` before `prev` with the comparison unreversed (14 words),
  `node` declared before `head` (16), `prev` first (16).
  So it is not the "container layout" - it is register allocation of one variable, which is the
  same family as this file's other walls, and it is **not** worth another lane's hour on its own;
  the carve below is worth it because its other function is already proven.

NEW: carve-801e5230 | match | MetroidPrime/ScriptObjects/Carve801E5230 | the contiguous run just past this item's claim: fn_801E5230 (0x801E5230, 0x4C) is measured byte-exact already (body in the notes above, one spelling), and fn_801E51A4 (0x801E51A4, 0x8C) is 140/140 bytes with retail's exact instruction sequence and a register-only diff - carry both as `Object(Matching, ...)` on 0x801E5230..0x801E527C at minimum, and note that claiming them retires `stub_201`/`stub_202` and needs new stubs for `fn_801E527C` (0x801E527C, 0x10) and `fn_801E528C` (0x801E528C, 0x44).

**Trap for whoever takes that item:** the four notes above this one disagree on the stub numbers for
exactly one reason - a carve's `PortLinkStubs.cpp` block is a *claim on a number*, and every tip that
landed another carve moved it.  Also: `build/` is gitignored, so `git reset --hard` does not clear a
failed attempt's `.o`/`.s`; a stale `Carve801E515C.o` with no source was sitting in this tree on
arrival.  And `tools/carve_diff.sh` still reads `main.elf` for "retail", which is *us* after the flip,
so its verdict is an artefact - the disc comparison above is the measurement.

No `WALL:` (nothing blocked), no `STALE:`.

## Lane 12, fifth attempt, on tip 7347718e (2026-10-02) — PASS, plus a measured spelling table for the neighbour

Re-done from scratch in `../wt-mp2-goal-L12` at **7347718e** (`docs: record the docs-bloat repair and
the history rewrite`; parent is the ninth-sync merge). Nothing of the four earlier passes was in
this tree: `grep -rn Carve801E515C configure.py files.cmake config/G2ME01/splits.txt src/` → no hit.

**Stale build artifacts again, and this is now the fourth run in a row that had to delete them:**
`build/G2ME01/src/MetroidPrime/ScriptObjects/Carve801E515C.o` and
`build/G2ME01/asm/MetroidPrime/ScriptObjects/Carve801E515C.s` were both present (mtime 16:58) with no
source, no `configure.py` entry and no `splits.txt` range. `build/` is gitignored, so `git reset
--hard` never clears them; both were deleted **before** the first build, and `build/report.json` on
arrival matched `build/goal/judge/report.base.json` exactly (matched **13020**, total 28465,
`complete_code` 875064), so the counts below are this tip's.

**What was new about this tip:** `PortLinkStubs.cpp` is now a *generated* file again
(`tools/gen_link_stubs.py` from `docs/research/boot_path_stubbable.tsv`) and its last block is
**`stub_224`**, not the `stub_199`/`stub_201` the older notes quote. `python3 tools/gen_link_stubs.py`
currently **refuses to run** on this tree (`refusing to shrink PortLinkStubs.cpp from 193 to 31
definitions` - the tsv is a stale snapshot), so the committed file with its hand-added blocks is the
durable record and a carve's blocks go in by hand, exactly as `carve-801fee40` did. The header's
numeric prose was **also stale after the ninth sync**, and one of its own greps is load-bearing:
`grep -cE 'asm\("'` is the file's own total, so any prose added to that header must write the
literal as `asm\("` (backslash) or it inflates the count it documents - I hit that and fixed it.

**The change (4 carve files + the stubs; 7 paths total):**

- `src/MetroidPrime/ScriptObjects/Carve801E515C.c` (new, 53 lines, definitions **descending**:
  `fn_801E5180` then `fn_801E515C`, callees declared extern at the top)
- `configure.py:758` — one-line `Object(Matching, "MetroidPrime/ScriptObjects/Carve801E515C.c")`,
  between `Carve801E3E34.c` and `Cameras/Carve801E7C14.c`
- `config/G2ME01/splits.txt:1297-1298` — `.text start:0x801E515C end:0x801E51A4`
- `files.cmake:567` — the source, same position
- `src/MetroidPrime/PortLinkStubs.cpp` — **`stub_225` = `fn_801E51A4` (`:1287-1288`)**, **`stub_226` =
  `fn_801E5230` (`:1299-1300`)**, plus the header numerals (`:8` 193 -> **195**, `:12` 187 -> **189**
  functions / 6 data objects, `:42` breakdown re-derived below)

**Stub numbering, read out of the file this run, not recalled:** last block before the change was
`stub_224` (`_Z19LoadWorldTeleporter...`). This is the third tip in a row that moved it (0291b134 ->
80db31e1 -> 7196a62c -> 7347718e all differ). **Read the current last `stub_N` every time.**

**Header numerals, derived from the file itself after the change:** `grep -cE 'asm\("'` **193 -> 195**;
`grep -cE 'asm\("(fn_|lbl_)'` **33 -> 35**; `grep -cE 'stub_data_[0-9]+\['` **6**; function stubs **189**.
Breakdown as re-derived there: 195 = 94 REL loader + 59 game method + 35 unmangled + 1
`rstl::rmemory_allocator::allocate` + 6 vtable/typeinfo (the pre-existing numbers 86/45/4 were stale
after the ninth sync; the REL-loader term alone now reads 94).

**Re-measured from the disc this run** (`orig/G2ME01/sys/main.dol`, DOL header parsed, text section
**1** = offset 0x640 / address 0x80003840 / size 0x3a1c60 - note `text0` at 0x100/0x80003100/0x540 is
a decoy if you take section 0 by mistake):

    801E515C  9421FFF0 7C0802A6 38630004 90010014 | 48000039 | 80010014 7C0803A6 38210010 4E800020
    801E5180  (same 8 words)                      | 480000A1 |
    80006B80  twin GetResourceIdByName__11CResFactoryCFPCc | 482F60B5 |

8 of 9 words identical to the twin, only the `bl` differs; LI decoded 0x801E516C+0x38 = 0x801E51A4 and
0x801E5190+0xA0 = 0x801E5230. The built `build/G2ME01/main.dol` is **72/72 bytes identical** to the
disc at 0x801E515C..0x801E51A4 and the **whole** text section 1 is 3808352/3808352 identical; our
object's `.text` is 70/72, the two differing words being 0x10 and 0x34, both `48 00 00 01`
(relocations unresolved in the object). `tools/carve_diff.sh` still reads `main.elf` for "retail" -
which is *us* after the flip - so the disc comparison, not its verdict, is the measurement.

**Verification, all measured on this tree this run:**

- `./tools/decomp_build.sh` -> DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
  `total_functions` **28465**; `matched_functions` 13020 -> **13022**; `complete_code` 875064 ->
  **875136** (+72, exactly the claim); `matched_code` 1986540 -> 1986612; report unit
  `main/MetroidPrime/ScriptObjects/Carve801E515C` = 72 B, 2/2 functions, 100.00% fuzzy, **complete**;
  the split leaves `main/auto_03_801E3E38_text` 4900 B / 14 functions and
  `main/auto_03_801E51A4_text` 7828 B / 14 functions
- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801E515C.c` -> `PASS -> kept as Matching`
- `./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801E515C.c` -> `claimed 72, ours 72, retail 72,
  fits`, `no extra functions`
- `build/binutils/powerpc-eabi-nm --defined-only -n` on our object -> `fn_801E515C @ 0x0`,
  `fn_801E5180 @ 0x24` (descending source order came out as retail's ascending `.text`;
  `check_decl_order.py --unit` still reports `ok: 0 unit(s) checked` - it does not cover carves)
- `python3 tools/link_gap.py --rebuild` **with** the two blocks: `281 MISSING`,
  `ok: 281 MISSING symbol(s), all accounted for in port_link_gap_list.md`; **without** them: `283
  MISSING`, `gap grew: fn_801E51A4`, `gap grew: fn_801E5230` (measured by deleting the two blocks,
  re-running, then restoring and `cmp`-ing the file back - the gap list itself is unchanged, the
  stubs are new *definitions*)
- `python3 tools/check_symbol_names.py` -> clean
- probe (`build/gate-probe.log`): **808 files, 0 failed, 0 errors**; `link: LINKED (287 undefined,
  0 duplicates)` - the port's undefined count did not move
- `./tools/goal_check.sh build/goal/item.json` -> **PASS carve-801e515c**, exit 0, every step `ok`,
  `ok counts: matched 13020 -> 13022   linked 6124 -> 6126`,
  `All: 36.96% fuzzy, 30.40% matched, 13.39% linked (13022 / 28465 functions)`;
  `build/goal/check-gate.log` = `GATE PASS  7347718e+7 changed`, per-function diff
  `SPLIT main/auto_03_801E3E38_text: 16 function(s) moved into
  main/MetroidPrime/ScriptObjects/Carve801E515C, main/auto_03_801E51A4_text (exact count match - a
  split, not a loss)`

**Bodies, unchanged from the earlier passes** (plain C so the `fn_` names do not mangle; the casts
are also what `probe_sources.sh` needs, since it syntax-checks every `files.cmake` source as C++):

    extern void* fn_801E5230(void* self, const void* key);
    extern void* fn_801E51A4(void* self, const void* key);

    void* fn_801E5180(void* self, const void* key) { return fn_801E5230((char*)self + 4, key); }
    void* fn_801E515C(void* self, const void* key) { return fn_801E51A4((char*)self + 4, key); }

## The neighbour, measured this run: `fn_801E51A4` (0x801E51A4, 0x8C) — 24 spellings, best 12 differing words

The previous run's NEW item (carve-801e5230) rests on two claims about the functions just past this
claim. Both were re-measured on **this** tree with a scratch `.c` compiled with this unit's exact
flags (`build.ninja:7259-7276`: mwcc `GC/2.7`, `-O4,p -inline deferred,noauto -lang=c`), `objcopy -O
binary --only-section=.text` and a byte compare against the disc — no claim, no flip involved.

- **`fn_801E5230` (0x801E5230, 0x4C) is byte-exact, confirmed.** 76/76 bytes; the only differing word
  is the `bl` at 0x20 (unresolved relocation), whose retail LI decodes to 0x801E5250+0x3C =
  **0x801E528C**. Body, first spelling, no variants needed:

      void fn_801E5230(void* self, void* key) {
          char* node = fn_801E528C(*(void**)self);
          ((void**)node)[0] = key;
          ((void**)node)[1] = ((void**)self)[1];
          ((void**)self)[1] = node;
      }

- **`fn_801E51A4` (0x801E51A4, 0x8C) is 140/140 bytes in every spelling tried and is a register
  allocation, not a body, problem — but the wall is narrower than the old note says.** 24 spellings
  this run (23 of them 140 B = 35 instructions, retail's exact sequence and branch targets), by bytes
  identical to the disc: `c1` **127/140 (12 differing words)**, `d4` 126/140 (12), `b2`/`c4` 125/140
  (14), `d1`/`d3`/`d5` 125/140 (13), `a2`/`b7`/`b8`/`c3`/`c5` 124/140 (13), the rest 122/140 (15)
  except `a7` (`unsigned long k`) 116/140 (15) and the tail-merged first attempt (128 B, 27 words). In
  the best ones the remaining 12 words are a
  **pure swap of two registers**: `node` sits in **r5** where retail has **r4**, and the two-load
  chain `lwz rX,0(node) ; lhz rX,8(rX)` takes **r4** where retail takes **r5** — the scratch is
  always the lowest free register, so the real question is only why retail's `node` got r4.
  **Correction to the earlier note: `k` is in r0 in every spelling I tried, which is retail's
  register** — it is not part of the diff. What that means in practice: the allocator gives the
  *locals* r5, r6, r7 in declaration order (r4 is left for the body's temporaries), so retail's
  `node`-in-r4 is the one thing to force, and declaration order alone has not done it. The two best
  orders were `k, node(decl), head=self[1], prev=0, node=head` (c1) and
  `k, head(decl), node=self[1], prev=0, head=node` (d4); `node` declared before `head` with the value
  taken straight from `self[1]` was *worse* (13), as was `struct Node{void* key; void* next;}` (15),
  an `unsigned long` key (15), a `for`-loop with `prev = node, node = node->next` in the increment
  (15), `prev`-declared-first locals (15), and the tail-merged form (27 words, 128 B) that the earlier
  notes already recorded.
- The other structural fact worth carrying: with the tails merged by the compiler the function is
  128 B, so **the `free`+`return 1` really must be written twice** for 140 B (both earlier notes say
  this; my `a1` re-measured it).

**Consequence for the queued item:** `carve-801e5230`'s premise holds (fn_801E5230 byte-exact). If a
future lane lands the 12-word spelling for `fn_801E51A4` too, the claim can extend down to 0x801E51A4
and retire `stub_225` as well; until then the 0x801E5230.. unit needs stubs for `fn_801E527C`
(0x801E527C, 0x10) and `fn_801E528C` (0x801E528C, 0x44), and this item's `stub_225` stays.

No `WALL:` line: the item's own two functions match (72/72 bytes, flip PASS, judge PASS) and the
`fn_801E51A4` work above is a neighbour, not this item's target - a WALL line here would park a
passing item for review. No `NEW:` line either: the only candidate is the carve-801e5230 item the
previous run already filed, and this run's measurements sharpen it rather than replace it.

## Lane 12, sixth attempt, on tip edb784ec (2026-10-02) — PASS, and the two carry failures diagnosed and removed

Re-done from scratch in `../wt-mp2-goal-L12` at **edb784ec** (`match: carve-8022eb54`).  Nothing of
the five earlier passes was in the tree: `grep -rn Carve801E515C configure.py files.cmake
config/G2ME01/splits.txt src/` found no hit.  `build/report.json` on arrival matched
`build/goal/judge/report.base.json` exactly (matched **13026**, total 28465, `complete_code`
875280), so the counts below are this tip's.  The **stale build artifacts were there again**
(`build/G2ME01/src/.../Carve801E515C.o` 760 B and `build/G2ME01/asm/.../Carve801E515C.s` 1178 B,
both mtime 17:11, with no source and no `configure.py` entry); this run left them in place and the
build rewrote both, and nothing about the result suggests they mattered — but deleting them first
is still the cheap habit.

### Why this item kept passing and never landing — measured, not inferred

The driver's own log (`build/goal/run.log`) shows the fifth attempt at 7347718e: judge PASS, then
`goal/decomp moved (7347718 -> edb784e) during carve-801e515c - carrying the judged change onto it`,
then `U docs/HANDOFF.md`, `U docs/RUNNING_THE_DECOMP.md`, **`U src/MetroidPrime/PortLinkStubs.cpp`**,
then `does not apply on edb784e - releasing it for a fresh attempt`.  `tools/run_goal.sh`'s
`rebase_onto_tip` does `git apply --index --3way --binary` and only union-merges conflicts that are
all under `docs/*.md`, so a third conflicted file is fatal.

`git apply --check --verbose build/goal/rebase.patch` (that lost patch, still in
`build/goal/rebase.patch`) on this tip names the failing hunks: `docs/HANDOFF.md:7`,
`docs/RUNNING_THE_DECOMP.md:121`, and `src/MetroidPrime/PortLinkStubs.cpp` **at line 5 - the header
paragraph only** (`file supplies 193 of them` / `187 functions, 6 data objects ...`).  Measured
directly: that file's section of the patch has **3 hunks** (`@@ -5,11`, `@@ -36,8`, `@@ -1258,6`),
all three in the header paragraph's numerals; `git apply --check --cached` on the whole section
fails (`patch failed: src/MetroidPrime/PortLinkStubs.cpp:5`), and on the same section with only the
tail hunk kept it returns **0**, plain and with `--3way`.  The two stub blocks were never the
problem: **every carve that lands while another item is being carried rewrites that same header
paragraph**, so a change that touches it cannot be carried.

**This run's two changes to the recipe fix exactly that, and only that:**

1. **No edit to the header paragraph at all.**  The diff of `PortLinkStubs.cpp` is
   **one hunk, a pure insertion** (`git diff -U0 ... | grep '^@@'` → `@@ -1289,0 +1290,47 @@`), at
   the tail after `stub_224`, where the fifth attempt also put its blocks.  Nothing is deleted or
   rewritten, so a concurrent commit elsewhere in the file cannot make it conflict.
2. **The stub names are keyed to the unit, not to the counter**: `stub_801e515c_0` /
   `stub_801e515c_1`.  This item has already lost one carry to a *number*: at 0291b134 the tip had
   taken `stub_199` for `fn_801FDAE8` (re-verified from git this run: `git show 0291b134 --
   src/MetroidPrime/PortLinkStubs.cpp` adds `stub_199() asm("fn_801FDAE8")`, in the commit
   `match: carve-801fdaa4`) and the re-judge died on
   `error: redefinition of 'void stub_199()'`.  A lane's rule is "read the last `stub_N` and add
   one", which is exactly what collides; a name no lane's rule generates cannot.  Nothing parses
   `stub_N` in this file (`grep -rn stub_ tools/` hits only `restub_reach.py`, which is
   `PortReachStubs.cpp`/`reachstub_\d+`), and the block comment says all of this where the next
   reader of the file will see it.

**The cost, stated plainly: the header paragraph's numerals are now 2 low.**  Measured from the
file after the change: `grep -cE 'asm\("'` **193 → 195**, `grep -cE 'asm\("(fn_|lbl_)'` **33 → 35**,
function stubs **189**, `stub_data_*` **6** (so 195 = 189 functions + 6 data objects), while the
paragraph still reads 193 / 187 / 33.  No tool validates them: `grep -rn PortLinkStubs
tools/*.py tools/*.sh` returns only prose in `check_files_cmake.py` and the generator itself, and
`tools/check_docs_claims.py` never mentions the file; `tools/gen_link_stubs.py` still refuses to
run on this tree ("refusing to shrink ... from 193 to 31 definitions"), so the committed file with
its hand-added blocks is the durable record, as it was for `carve-801fee40`.  The block comment
records the drift and the fix so the next lane that can safely edit that paragraph (necessarily on
another item's tree) corrects it.

### The change (5 paths + the new source, 4 of them the carve)

- `src/MetroidPrime/ScriptObjects/Carve801E515C.c` (new, 94 lines, definitions **descending**:
  `fn_801E5180` then `fn_801E515C`, callees declared extern above them)
- `configure.py:758` — one-line `Object(Matching, "MetroidPrime/ScriptObjects/Carve801E515C.c")`,
  between `Carve801E3E34.c` and `Cameras/Carve801E7C14.c`
- `config/G2ME01/splits.txt:1297-1298` — `.text start:0x801E515C end:0x801E51A4`
- `files.cmake:567` — the source, same position
- `src/MetroidPrime/PortLinkStubs.cpp:1290-1335` — the block comment and
  `stub_801e515c_0` = `fn_801E51A4`, `stub_801e515c_1` = `fn_801E5230`
- (`docs/HANDOFF.md` / `docs/RUNNING_THE_DECOMP.md` show as modified: that is `goal_check.sh`'s own
  `MP_GATE_DOCS_WRITE` step rewriting the derived counts, which the driver discards.  No hand edit.)

Bodies (plain C so the `fn_` names do not mangle; the casts are what `probe_sources.sh` needs, since
it syntax-checks every `files.cmake` source as C++):

    extern void* fn_801E51A4(void* self, const void* key);
    extern void* fn_801E5230(void* self, const void* key);

    void* fn_801E5180(void* self, const void* key) { return fn_801E5230((char*)self + 4, key); }
    void* fn_801E515C(void* self, const void* key) { return fn_801E51A4((char*)self + 4, key); }

**Re-measured from the disc this run** (`orig/G2ME01/sys/main.dol`, DOL header parsed: text section
1 = file offset **0x640** / address **0x80003840** / size **0x3a1c60**; section 0 at
0x100/0x80003100/0x540 is the decoy):

    801E515C  9421FFF0 7C0802A6 38630004 90010014 | 48000039 | 80010014 7C0803A6 38210010 4E800020
    801E5180  (same 8 words)                      | 480000A1 |
    80006B80  twin GetResourceIdByName__11CResFactoryCFPCc | 482F60B5 |

LI decoded: 0x801E516C+0x38 = 0x801E51A4, 0x801E5190+0xA0 = 0x801E5230.  A scratch copy of the new
source compiled with this unit's exact flags (mwcc `GC/2.7`, `-O4,p -inline deferred,noauto
-lang=c`) is **70 of 72 bytes** identical to the disc, the two differing words being 0x10 and 0x34,
both `48000001` (relocations unresolved in the object); the **built** `build/G2ME01/main.dol` at
0x801E515C..0x801E51A4 is **72/72 identical** and the whole text section 1 is identical.

The twin is 100.00% as a function inside `MetroidPrime/main.cpp` — note the *unit* is
`NonMatching` (98/99 functions, 96.68% fuzzy), so "the already-`Matching` main.cpp" in the older
notes was wrong about the unit and right about the function.

### The two callees, read out of `build/G2ME01/asm/auto_03_801E51A4_text.s` this run (one correction)

- `fn_801E51A4` (0x8C): `k = *key` (`lhz r0,0(r4)`), `head = self[1]`, walk with
  `cmplw *(u16*)(*(char**)node + 8), k`, unlink head-and-mid through **`fn_801E527C` (0x801E527C,
  0x10)** and `li r3,1`, else `li r3,0`.  The previous notes called that callee "`free`" and one of
  them called it 0x801E5250; the asm says `bl fn_801E527C` at 0x801E51E8 and 0x801E5200 (the
  0x801E5250 is its own `bl` inside `fn_801E5230`).
- `fn_801E5230` (0x4C): `lwz r3,0(self)` then `bl fn_801E528C` (0x801E528C, 0x44), node[0] = key,
  node[1] = self[1], self[1] = node, and **r3 (the node) is returned untouched**.

That keeps the queued `carve-801e5230` premise intact and adds its minimum: the unit at
0x801E5230..0x801E527C would need stubs for `fn_801E527C` (0x10) and `fn_801E528C` (0x44).

### Verification, all measured on this tree this run

- `./tools/decomp_build.sh` → DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
  `total_functions` **28465**; `matched_functions` 13026 → **13028**; `complete_code` 875280 →
  **875352** (+72, exactly the claim); `matched_code` 1986756 → 1986828; `complete_units`
  809 → **810**; report unit `main/MetroidPrime/ScriptObjects/Carve801E515C` = 72 B, 2/2 functions,
  **100.00% fuzzy, complete**; the split leaves `main/auto_03_801E3E38_text` at
  0x801E3E38..0x801E515C (4900 B, 14 functions) and `main/auto_03_801E51A4_text` at
  0x801E51A4..0x801E7038 (7828 B, 14 functions)
- `./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801E515C.c` → `PASS -> kept as Matching`
- `./tools/unit_fit.sh MetroidPrime/ScriptObjects/Carve801E515C.c` → `claimed 72, ours 72, retail 72,
  fits`, `no extra functions`
- `build/binutils/powerpc-eabi-nm --defined-only -n` on our object → `fn_801E515C @ 0x0`,
  `fn_801E5180 @ 0x24` (descending source order came out as retail's ascending `.text`;
  `check_decl_order.py --unit` still reads `ok: 0 unit(s) checked` - it does not cover carves)
- `python3 tools/link_gap.py --rebuild` **without** the two blocks: exits 1, **283 MISSING**,
  `gap grew: fn_801E51A4 ...` and `gap grew: fn_801E5230 ...`; **with** them: **281 MISSING**,
  `ok: 281 MISSING symbol(s), all accounted for in port_link_gap_list.md`
- `python3 tools/check_symbol_names.py` → `checked 575 units; 0 declared names are missing`
- probe (`build/gate-probe.log`): **811 files, 0 failed, 0 errors**;
  `link: LINKED (287 undefined, 0 duplicates)` — the port's undefined count did not move
- `./tools/goal_check.sh build/goal/item.json` → **PASS carve-801e515c**, exit 0, every step `ok`,
  `ok counts: matched 13026 -> 13028   linked 6130 -> 6132`,
  `All: 36.96% fuzzy, 30.40% matched, 13.39% linked (13028 / 28465 functions)`;
  `build/goal/check-gate.log` = `GATE PASS  edb784ec+7 changed`, per-function diff
  `SPLIT main/auto_03_801E3E38_text: 16 function(s) moved into
  main/MetroidPrime/ScriptObjects/Carve801E515C, main/auto_03_801E51A4_text (exact count match - a
  split, not a loss)` and `0` lines matching `WORSE|GONE|UNLINKED|FELL` in `build/gate-diff.log`

### For the next lane

- The last `stub_N` in `PortLinkStubs.cpp` is still **225** (`fn_801FEAE0`, carve-801fea98), because
  this run's two blocks are not numbered; a conventional block goes in at 226.
- If this item has to be redone again, the five older sections above are all correct about the carve
  itself; the only things that changed between them were the stub numbers and the splits.txt /
  configure.py line numbers.  Re-measure those, keep the block comment, and keep the two
  carry-hygiene choices above — they are the only reasons this sixth attempt is not the sixth loss.

No `WALL:` (nothing blocked), no `NEW:` line (the only candidate, `carve-801e5230`, is already
queued), no `STALE:`.
