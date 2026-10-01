# match-rstl-string-eq-out-of-line — DONE (judged PARTIAL, exit 3)

`rstl::operator==(const rstl::string&, const char*)` is now a real function in the DOL,
defined where retail puts it, and the local `rstl_string_eq_c` workaround is gone.

## What the item asked, and what it measured

Retail's free `rstl::operator==(const rstl::string&, const char*)` is
`__eq__4rstlFRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>PCc`
at **0x8008808C, 0x2C = 11 instructions** (`config/G2ME01/symbols.txt:2447`), inside
`CAutoMapper.cpp`'s claimed `.text` (0x80086D9C..0x80092310). Re-measured before acting:
`build/report.json` on the clean tree listed it for `main/MetroidPrime/CAutoMapper` with
**no `fuzzy_match_percent` at all** (None), i.e. objdiff had nothing to pair it with. The unit
was **75 / 100** matched functions, 92.03108% fuzzy.

There are exactly **10 `bl 8008808c` call sites in the DOL** (`objdump -d build/G2ME01/main.elf`),
in: `CAutoMapper::FindClosestVisibleWorld` (0x80087EF0), `CMorphBall::GetMorphBallModel`
(0x800C12D8), `CMapUniverse::Draw` (0x80154E6C), `CStateManager::CalculateScanCompletionRate`
(0x8018EDAC), `fn_8020F92C` (x2), `fn_80211128`, `fn_8021129C` (x3). Retail never inlines it.

## The change (3 files, no new function bodies beyond the one retail has)

1. **`include/rstl/string.hpp:412`** — dropped `inline` from the declaration. That alone was
   the blocker: declared `inline`, mwcceppc inlines the `compare` call at every site and the
   caller grows a `li r5,-1` and a `cmpwi r3,0` that retail does not have. Wrapped in
   `#ifdef TARGET_PC` — see "the port" below.
2. **`src/MetroidPrime/CAutoMapper.cpp:2242`** — the definition, in the position its retail
   offset requires. mwcceppc emits in reverse source order, and 0x8008808C sits **between**
   `FindClosestVisibleWorld` (0x80087E30) and `FindTeleportArea` (0x800880B8), so the
   definition goes between those two functions. `python3 tools/check_decl_order.py` → *ok: 981
   unit(s) checked, 30 permuted, all 30 accounted for* (unchanged from the baseline).
3. **`src/MetroidPrime/Player/CMorphBall.cpp`** — deleted the `extern "C" bool
   rstl_string_eq_c(...)` workaround (was lines 121-141) and changed the call site to
   `if (name == "")`. The call is now `R_PPC_REL24 __eq__4rstlF...`, i.e. **retail's own
   symbol**, not a renamed copy.

The body is the same `lhs.compare(rhs) == 0` the inline version made. The `bl` to
`compare__Q24rstl...CFPCci` at 0x800776C8 and the `cntlzw`/`srwi` pair are the compiler's own
code for it — nothing is hand-written.

## Measured result

```
All:  34.22% fuzzy, 27.29% matched, 12.75% linked (12088 / 28465 functions)
matched  12087 -> 12088   linked 5795 -> 5795   (+1 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/CAutoMapper :: __eq__4rstlFRC...PCc
  WORSE    main/MetroidPrime/CMapUniverse :: Draw__12CMapUniverseF... 84.02% -> 83.99%
no regression
```

Per unit, from `report.base.json` vs `build/report.json`:

- `main/MetroidPrime/CAutoMapper` **75 → 76** functions, fuzzy 92.03108 → 92.145096.
  - `__eq__4rstlF...`: None → **100.0**
  - `FindClosestVisibleWorld`: **98.50993 → 99.99338** (a second, unclaimed win: its
    `bl __eq__` at 0x80087EF0 is now real, so it pairs instead of inlining)
- `main/MetroidPrime/Player/CMorphBall` **116 → 116**, every function's percentage unchanged.
  `GetMorphBallModel` stays 100.0 — retiring the workaround cost nothing, confirming what the
  old comment at CMorphBall.cpp:131-134 had already worked out (objdiff normalises a
  `R_PPC_REL24` target, so the renamed symbol was never what the 0.0625% was).
- `main/MetroidPrime/CMapUniverse` **37 → 37**, `Draw` 84.02207 → **83.99117**.

**On that CMapUniverse `Draw` drop (-0.031%)**, since it is the one number that went down and it
should not be waved away: the call site is now *byte-identical to retail*. Retail
0x80154E5C..0x80154E74 is `lis r4,-32709 / mr r3,r28 / addi r4,r4,-27288 / addi r4,r4,7 /
bl 8008808c / clrlwi. r0,r3,24 / beq`; ours is `lis r4,0 (@stringBase0) / mr r3,r19 /
addi r4,r4,0 (@stringBase0) / addi r4,r4,7 / bl __eq__ / clrlwi. r0,r3,24 / beq` — the same
seven instructions, differing only in the register holding `this` and in the two sdata
relocations that objdiff normalises. Diffing instruction streams with `difflib`
(`objdump -d`, base object vs new object vs `main.elf`): base matched **138** of retail's 453
instructions, new matches **140**. The old body could not score higher *there*; the -0.031% is
the 8-byte register-allocation difference at `mr r3,r19` vs `mr r3,r28` propagating through a
function that is 84% matched for unrelated reasons (it disagrees with retail in `r24`/`r28`/
`r30` naming, `stwu r1,-608` vs `-624`, and the SDA base `lwz r3,0(0)` vs `lwz r3,-28248(r13)`
throughout). `CMapUniverse` is `NonMatching`, `report_diff.py` prints *no regression*, and
`goal_check.sh`'s gate is green. The drop is in a unit that was never near a flip.

## The port: a real failure the first run caught, and the fix

The first `goal_check.sh` run **failed**:

```
FAIL  gate.sh
  link_check: STRICT FAIL - regression gate: 292 undefined against a baseline of 291 (GREW)
GATE FAIL: probe link-gap
  gap grew: _ZN4rstleqERKNS_12basic_stringIcNS_11char_traitsIcEENS_17rmemory_allocatorEEEPKc
            is not in port_link_gap_list.md
```

Worth recording, because it is a trap this class of change keeps setting: **the port does not
compile `src/MetroidPrime/CAutoMapper.cpp`**. It is in `tools/check_files_cmake.py`'s
`EXCLUDED` list (`grep CAutoMapper tools/check_files_cmake.py` → listed as excluded), and
`build-port-link/build.ninja` contains **0** references to it. So the definition that is
correct for the DOL leaves the host with a dangling reference the moment any port-compiled TU
calls the operator — and `CMorphBall.cpp` does, at 0x800C12D8.

The fix is a `#ifdef TARGET_PC` branch in the same header, the mechanism the repo already uses
(`include/rstl/string.hpp:244` has one, and `src/Kyoto/Audio/CStreamAudioManager.cpp:335` is
the same pattern for `operator!=`). It keeps the port's behaviour unchanged from before this
change — the host previously inlined the operator through this very header — and gives the
DOL the out-of-line copy retail has. After it:

```
probe: 747 files, 0 failed, 0 errors; link: LINKED (291 undefined, 0 duplicates)
port link gap: 286 MISSING, all accounted for in port_link_gap_list.md
```

Back to the baseline 291, 0 duplicates. This is the second gate in this item's history that
only a real link could have caught; a green `decomp_build.sh` and a green `report_diff.py` both
passed on the broken version.

## Gates

All from `./tools/goal_check.sh build/goal/item.json`, which printed
`PARTIAL ... - flip_test MetroidPrime/CAutoMapper.cpp: FAIL, but the target rose`:

- `gate.sh` → **GATE PASS**, including the DOL sha1, all 86 REL hashes vs `config.yml`,
  `report_diff.py`, module wiring, docs claims, port probe and the port's real link.
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged).
- All 86 REL sha1s re-checked independently against `config/G2ME01/config.yml`: no mismatches.
- `check_symbol_names.py` → *checked 525 units; 0 declared names are missing*.
- `report_diff.py` → *no regression*.
- `check_decl_order.py` → *ok: 981 unit(s) checked, 30 permuted, all 30 accounted for*.
- `unit_fit.sh MetroidPrime/CAutoMapper.cpp` → 37 extra functions / 3316 bytes, **unchanged
  from the baseline** — the object emits nothing the retail object does not, so the new
  function did not make the unit unfit.
- No `asm`/`__asm` added (`goal_check.sh`'s own check: *ok no asm added*).

## What still stops the flip

`flip_test.sh MetroidPrime/CAutoMapper.cpp` fails, and will keep failing for a long time: the
unit is **76 / 100** functions, and 24 of the remaining ones are far from done —
`Update` 78.11% (10620 bytes), `Draw` 94.17% (4932), `ProcessControllerInput` 94.94%,
`__dt__ vector<auto_ptr<IWorld>>` 27.21%, `clear` 0.79%, `fn_8008BEB0` 0.00%,
`fn_8008F968` 0.00%. Two of those (`fn_8008BEB0`, `fn_8008F968`) are unnamed retail functions
whose semantics are not yet established. `unit_fit.sh` reports 37 extra functions / 3316 bytes,
which the tool itself notes is often harmless COMDAT weak copies — but only `flip_test` decides,
and it is the binding test.

**A function match is banked, not a unit.** Per the item's own rule this counts only when
`CAutoMapper` goes `Matching` in `configure.py` and the DOL still reproduces retail with our
object in the link. It is at 76/100, so the unit remains `NonMatching` and `docs/HANDOFF.md`'s
state block (machine-rewritten by the judge) is the record of the +1.

## Reusable lesson

**`#ifdef TARGET_PC` is not a formality in this repo — check the EXCLUDED list before moving a
definition between units.** A function that retail emits out of line inside unit X must be
defined in X for the DOL to hash, and the port frequently does not compile X. Every such move
needs the header branch, or the port's undefined count rises and `gate.sh` fails on `link-gap`
even though the decomp build, the report and the per-function diff are all green.

Second, for the next `rstl` free operator: `operator==(const char* lhs, const string& rhs)` and
`operator!=(const string& lhs, const char* rhs)` are already declared non-inline at
`include/rstl/string.hpp:422-423`, and `operator!=` is defined out of line in
`src/Kyoto/Audio/CStreamAudioManager.cpp:335` (a unit the port *does* compile, so it needs no
`TARGET_PC` branch — that asymmetry is why the branch above has to exist for `operator==` and
not for `operator!=`). `operator==(const char*, const string&)` has no definition anywhere in
the tree; nothing calls it, so it is declaration-only today.

No `NEW:` lines: nothing here is a blocker, and the two functions still open in this unit
(`fn_8008BEB0`, `fn_8008F968`) are unnamed retail functions whose behaviour is unknown, not a
spelling I failed to reach 100% on — no `WALL:` line either, per the rule that a wall is a set
of spellings tried in *this* run.
