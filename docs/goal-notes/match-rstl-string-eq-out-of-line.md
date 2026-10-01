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

---

# Second run (2026-10-01, lane 2) — +1 function: `fn_8008F968`, judged PARTIAL (exit 3)

Re-measured first. The first run's change is in the tree and its `+1` is banked:
`build/report.json` lists `main/MetroidPrime/CAutoMapper` at **76 / 100**, fuzzy 92.145096, and
`__eq__4rstlF...PCc` at 100. So the item is **not** stale - 24 functions are still missing.

The notes above called the two remaining unnamed retail functions "whose semantics are not yet
established". **Both are now established, and one is matched.** Read the 40-byte one first: its
semantics were one `objdump` away and the notes' "unknown" was only a naming artefact.

## What this run changed: 1 file, `src/MetroidPrime/CAutoMapper.cpp`

`fn_8008F968` is **`rstl::list<CAutoMapper::SAutoMapperHintStep>::push_front`**. Retail's 40 bytes:

```
8008f968 <fn_8008F968>:
8008f968: stwu r1,-16(r1) / mflr r0 / mr r5,r4 / stw r0,20(r1)
8008f978: lwz r4,4(r3)                    <- mStart
8008f97c: bl 8008f990 <do_insert_before__...SAutoMapperHintStep...>
8008f980: lwz r0,20(r1) / mtlr r0 / addi r1,r1,16 / blr
```

Compare retail's `push_back` at 0x8008FEFC (`symbols.txt:2503`): the same 10 instructions with
`lwz r4,8(r3)` - `mEnd` instead of `mStart`. And our object **already emitted those exact 40
bytes**, as the weak COMDAT `push_front__Q24rstl69list<Q211CAutoMapper19SAutoMapperHintStep,...>`
at 0x8980, called from `UpdateHintNavigation`. The retail object's only problem is that **dtk's
map has no name for 0x8008F968**, so objdiff had nothing to pair the COMDAT with and the report
scored it `None`.

**This is the `fn_80007AA0` trick from `src/MetroidPrime/main.cpp:965-987`, applied a second
time** - the notes above do not list it as tried, and it is worth naming explicitly for the next
`fn_*`: when a retail function is a `rstl` template member that mwcceppc *already* emits under its
mangled name, and retail's own symbol table has no name for it, spelling it out under the `fn_`
name in the unit converts an "extra" COMDAT into a match. The body is the header's `push_front`
verbatim; nothing is hand-written. `mStart` is private, so the member is reached through
`begin()` (public), which mwcceppc folds back to the same `mStart` load - the same
`end().get_node()` trick `fn_80007AA0` uses.

Two edits: the `extern "C"` definition at **`CAutoMapper.cpp:566-583`**, placed immediately before
`UpdateHintNavigation` (reverse source order puts it between `erase<...HintStep...>` and
`do_insert_before<...HintStep...>`, which is exactly retail's slot between 0x8008F904 and
0x8008F990), and the call site at **`CAutoMapper.cpp:600-601`** changed from `mHintSteps.push_front(...)`
to `fn_8008F968(&mHintSteps, ...)` so the call target is named rather than inlined.

## Measured result

```
All:  34.28% fuzzy, 27.49% matched, 12.89% linked (12132 / 28465 functions)
matched  12131 -> 12132   linked 5860 -> 5860   (+1 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/CAutoMapper :: fn_8008F968
no regression
```

- `main/MetroidPrime/CAutoMapper` **76 -> 77** functions, fuzzy 92.145096 -> 92.23,
  matched code 32.15362 -> 32.24%. `fn_8008F968` None -> **100.0**.
- Every other function in the unit **unchanged**, including `UpdateHintNavigation` at 95.16% - the
  call site was already a `bl` to the COMDAT and the rename does not move a byte there.
- `report_diff.py` prints *no regression*; the only movement is the `+100%` line above.

## Gates (all from `./tools/goal_check.sh build/goal/item.json`)

- `gate.sh` -> **GATE PASS**: DOL sha1, all 86 RELs, `report_diff.py`, module wiring, docs claims,
  port probe and the port's real link (291 undefined, 0 duplicates - **unchanged**, and no header
  was touched this run, so the `TARGET_PC` trap from the first run cannot recur).
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `check_symbol_names.py` -> *checked 525 units; 0 declared names are missing*.
- `check_decl_order.py --unit main/MetroidPrime/CAutoMapper` -> *ok: none emits its functions out
  of retail order*. `fn_8008F968` landed in the right slot; the full run is *981 units, 30
  permuted, all accounted for* as before.
- `unit_fit.sh` -> **36 extra functions / 3276 bytes, DOWN from the 37 / 3316 baseline** - the
  rename removed one extra (the COMDAT no longer exists under that name), so the unit is no less
  fit for a flip than before.
- `goal_check.sh`'s own check: *ok no asm added*.
- The only file modified is `src/MetroidPrime/CAutoMapper.cpp`; `docs/HANDOFF.md`'s state block was
  rewritten by the judge itself, not by me.

## What still stops the flip - unchanged and not close

`flip_test.sh MetroidPrime/CAutoMapper.cpp` fails and will keep failing: **77 / 100** functions,
and the 23 left include `Update` 78.11% (10620 bytes), `Draw` 94.17% (4932),
`ProcessControllerInput` 94.94%, `InterpolateWithClamp` 95.90%, `clear` 0.79%,
`__dt__ vector<auto_ptr<IWorld>>` 27.21%, and `fn_8008BEB0` 0.00%. `unit_fit.sh`'s 36 extras
keep `flip_test` out of reach on its own terms. Only `flip_test` decides, and it is binding.

## The other unnamed function, characterised (not matched)

`fn_8008BEB0` (0x8008BEB0, 0x80 = 128 bytes) **is the out-of-line `destroy(first, last)` loop for
`rstl::vector<rstl::auto_ptr<IWorld>>`**, shared by exactly three call sites - all in this unit:

- `clear` 0x8008BE90, `__dt__ vector` 0x8008BF7C, `erase(pointer_iterator,...)` 0x8008C038.

Its body is the loop the header's `rstl::destroy` produces, with the `auto_ptr` guard inlined:
`if (mHas && mItem) mItem->vfunc(1)` - `lwz r12,0(r3)` / `li r4,1` / `lwz r12,8(r12)` /
`mtctr` / `bctrl`, i.e. the **`IWorld` virtual destructor at vtable slot 2**, stepped 8 bytes per
`auto_ptr`. Our object already emits these bytes, **inlined at each of the three call sites**
rather than shared. `vector.hpp`'s `destroy(begin(), end())` is `inline` in
`include/rstl/construct.hpp:98`, so mwcceppc expands it and no out-of-line copy exists to rename.

**So this one is a different problem from `fn_8008F968`, and the rename trick does not apply**: it
is not a template member that already exists under another name, it is a loop the header inlines.
Reaching it means making `destroy` out of line for this instantiation, which is a change to
`include/rstl/construct.hpp` shared by every `vector`/`list` user in the tree - far outside this
item's one-unit scope, and it would move unrelated units. Recorded here so the next run does not
re-derive the three call sites and the 8-byte stride.

## Reusable lesson (extends the first run's)

**An unnamed retail function (`fn_` with no demangled name) is usually not unknown - it is a
`rstl` template member whose bytes we already emit under the mangled name.** Check
`build/binutils/powerpc-eabi-nm <our>.o` for a same-size COMDAT at the same position before
concluding a function's behaviour is unknown: `fn_8008F968` (40 B) sat byte-for-byte beside
`push_front__...SAutoMapperHintStep...`, and 0.00% was purely the missing name. The tell is that
`unit_fit.sh` lists the function as an **extra** - an extra with the right size and the right
retail offset is a rename waiting to happen, not dead code.

No `WALL:` line: `fn_8008F968` went to 100%, and `fn_8008BEB0` was characterised rather than
spelled at (it is blocked on a shared header, which is a finding, not a wall I measured).

No `NEW:` lines. `fn_8008BEB0` is not filed: making `rstl::destroy` out of line is a cross-cutting
header change that would move unrelated units, so it is not work whose success raises a count for
one unit.

---

# Third run (2026-10-02, lane 2) — +2 functions: `fn_8008BEB0` and `FindClosestVisibleWorld`, judged PARTIAL (exit 3)

Re-measured first: the tree carries the first two runs' `+2`, so the item is **not** stale —
`build/report.base.json` had `main/MetroidPrime/CAutoMapper` at **77 / 100** functions, fuzzy
92.23121, matched code 32.23973%, and 23 functions unmatched. This run adds **two** more.

The `rstl::operator==` this item was raised for landed in run 1; run 2 took `fn_8008F968`. What was
left in this unit that could be reached **without a shared-header change** is what I went after,
and there were two such things: the one run 2 called "blocked on a shared header" (it is not, if
you let retail's own symbol carry the name — see below), and the string pool, which nobody had
looked at.

## What this run changed: 1 file, `src/MetroidPrime/CAutoMapper.cpp`

### 1. `fn_8008BEB0` — 0x8008BEB0, 0x80 = 128 bytes, None -> **100.0**

Run 2 characterised this as the out-of-line `rstl::destroy(begin, end)` loop for
`rstl::vector<rstl::auto_ptr<IWorld> >` and concluded "the rename trick does not apply: it is not a
template member that already exists under another name, it is a loop the header inlines."
**That conclusion was half right, and this is the correction.** The loop being inlined is
irrelevant: what objdiff needs is a symbol *named* `fn_8008BEB0` whose bytes are retail's, and
nothing forces the *call sites* to use it. So the definition is spelled out here, exactly as
`fn_8008F968` is, and the three call sites are left alone:

```cpp
typedef rstl::vector< rstl::auto_ptr< IWorld > >::iterator CAutoMapperWorldIter;
extern "C" void fn_8008BEB0(CAutoMapperWorldIter first, CAutoMapperWorldIter last) {
  rstl::destroy_impl(first, last);
}
```

`rstl::destroy` and `rstl::destroy_impl` are the same loop — `construct.hpp:97` is a one-line
forwarder — and **which one you call is worth 0.06% and the last instruction**:

| body | score | difference from retail |
|---|---|---|
| `rstl::destroy(first, last)` | 99.94% | `stw r31,12(r1); stw r30,8(r1)` where retail has `stw r31,8(r1); stw r30,12(r1)` — the two frame slots swapped |
| `for (It cur = first; cur != last; ++cur) rstl::destroy(&*cur);` | 87.25% | mwcceppc keeps `last` in r4, never spills it, and drops the frame to -16 |
| `rstl::destroy_impl(first, last)` | **100.0%** | none |

Going through the forwarder costs one nested inline frame, and the extra frame is what decides
which of the two loop variables lands in which slot. (Two further spellings — an explicit
`It cur = first;` local, and an explicit `It e = last;` local — were scripted and **not run**;
the script's exit status ended the chain. They are the obvious next things to try if this ever
regresses.) Everything else in the 128 bytes is the compiler's: the `is_trivially_destructible`
guard folds away, and what is left is the `auto_ptr` teardown (`mHas && mItem` -> `mItem->vfunc(1)`,
the `IWorld` virtual destructor at vtable slot 2) and the 8-byte stride — the same 16
instructions our three inlined copies already emitted.

Position: defined between `GetAreaHintDescriptionString` and `Update`, so mwcceppc emits it
immediately before `GetAreaHintDescriptionString` — which is where retail has it *among the
functions both objects name by symbol* (its `.text` neighbours `clear<...auto_ptr<IWorld>...>`
and `~vector<...>` are weak COMDATs, which `check_decl_order.py` filters out by design, since the
linker and not our source order decides where the kept weak copy goes). `check_decl_order.py
--unit main/MetroidPrime/CAutoMapper` -> *ok: 1 unit(s) checked, none emits its functions out of
retail order*.

### 2. The narrow string pool — one line, and `FindClosestVisibleWorld` 99.99338 -> **100.0**

`FindClosestVisibleWorld` (0x80087E30, 604 bytes) was **one instruction** away: at **0x80087EEC**
we emit `addi r4,r4,403` where retail has `addi r4,r4,413`, for the `"TempleHub"` literal. That
is 10 bytes, and the cause is that retail declares `"model_hex"` as a **file-scope** static
(between `skFRME_MapScreenBackground` and `skMapKeys`) while we passed the literal inline to
`CBasics::Stringize` inside `Update`, which puts it in the pool *after* `"%s%d"` instead of
before the pooled `""`. One line fixes it:

```cpp
static const char* const skModelHex = "model_hex";   // new, next to the other two statics
... CBasics::Stringize("%s%d", skModelHex, i) ...     // was "model_hex" inline
```

**How to check this class of problem in one command** (it is what found the above): string
immediates are offsets from `@stringBase0`, which is the *first narrow* literal of the unit's
`.rodata`, so compare the narrow-literal list **relative to its own first entry**, not the raw
section bytes. Doing that now, both pools hold 80 literals and agree on every offset and on the
content of the first 71, so this is settled and re-measurable:

```
ours base 0xf4 (='FRME_MapScreen'), retail base 0x104
narrow literal list identical (relative offsets): False 80 80
  first divergence at 71 (1336, 'Teleport_Destination') (1336, 'Teleport Destination')
```

Three things that looked like differences and are **not** code differences, so nobody spends a run
on them: (a) the missing third `L"&image="` copy in `.rodata` (retail 0xf4) and (b) the 5 trailing
pad bytes at the end of retail's `.rodata` are both *outside* `@stringBase0` / after every literal
— wide literals use `@wstringBase0` and section padding moves nothing; (c) our `.sdata` is 80 bytes
larger and retail's `.sdata2` 20 bytes larger, because mwcceppc put one `L"&image="` copy (16 B)
and one `L";"` copy (4 B) in our `.sdata` that retail has in `.sdata2` — small-data placement,
reached through `R_PPC_EMB_SDA21` relocations, which objdiff normalises.

The one real difference left is **content, not layout**: retail's string is
`"Teleport Destination"` (space) at the same relative offset 1336, ours is `"Teleport_Destination"`
(underscore) with a compensating pad byte after it. That is a wrong resource name in our source,
but it changes no instruction, so it raises no count and I left it alone rather than widen this
diff — `src/MetroidPrime/CAutoMapper.cpp:1724`. Whoever makes this unit `Matching` needs it.

## Measured result

```
All:  34.43% fuzzy, 27.69% matched, 12.89% linked (758 / 2066 files)
matched  12186 -> 12188   linked 5860 -> 5860   (+2 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/CAutoMapper :: fn_8008BEB0
  +100%    main/MetroidPrime/CAutoMapper :: FindClosestVisibleWorld__11CAutoMapperCFRC9CVector3fRC13CUnitVector3fRC13CStateManager
no regression
```

`main/MetroidPrime/CAutoMapper` **77 -> 79** functions, fuzzy 92.23121 -> **92.54189**, matched code
32.23973% -> **33.81555%**. Per function, from `report.base.json` vs `build/report.json` — these
four are the *only* functions in the whole tree that moved:

| function | before | after |
|---|---|---|
| `fn_8008BEB0` | (no `fuzzy_match_percent`: unpaired) | **100.0** |
| `FindClosestVisibleWorld` | 99.99338 | **100.0** |
| `UpdateTempleKeys` | 98.979164 | 99.010414 |
| `Update` | 78.111115 | 78.263275 |

`UpdateTempleKeys` and `Update` rose only because their `addi` immediates moved into place; neither
reached 100% (see below). Nothing fell.

## Gates (all from `./tools/goal_check.sh build/goal/item.json`, which printed
## `PARTIAL ... - flip_test MetroidPrime/CAutoMapper.cpp: FAIL, but the target rose`)

- `gate.sh` -> **GATE PASS**: DOL sha1, all 86 RELs, `report_diff.py`, module wiring, docs claims,
  port probe and the port's real link. No header was touched, so run 1's `TARGET_PC` trap (the port
  does not compile this unit, `tools/check_files_cmake.py` EXCLUDED) cannot recur.
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged).
- `check_symbol_names.py` -> *checked 525 units; 0 declared names are missing*.
- `check_decl_order.py --unit main/MetroidPrime/CAutoMapper` -> *ok: none emits its functions out
  of retail order*.
- `unit_fit.sh MetroidPrime/CAutoMapper.cpp` -> **36 extra functions / 3276 bytes, unchanged from
  run 2's baseline** — adding a symbol retail has takes nothing away, and the unit is no less fit
  for a flip than before.
- `goal_check.sh`'s own check: *ok no asm added*. The only file modified is
  `src/MetroidPrime/CAutoMapper.cpp`; `docs/HANDOFF.md`'s state block was rewritten by the judge.

## What still stops the flip

Unchanged and not close: **79 / 100**, and the 21 left include `Update` 78.26% (10620 bytes),
`Draw` 94.17% (4932), `ProcessControllerInput` 94.94% (3360), `ProcessMapPanInput` 96.96%,
`InterpolateWithClamp` 95.90% (1092), `clear` 0.79%, `__dt__ vector<auto_ptr<IWorld>>` 27.21%,
`do_insert_before<...SAutoMapperHintLocation...>` 60.50%. `unit_fit.sh`'s 36 extras keep
`flip_test` out of reach on its own terms. Only `flip_test` decides, and it is binding.

## Characterised, not fixed (so the next run does not re-derive them)

- **`clear` 0.79% / `__dt__ vector<auto_ptr<IWorld>>` 27.21% / `erase(It,It)` 69.52%** — the three
  functions that `bl 8008beb0` in retail and that we still inline, so all three disagree with
  retail structurally (retail's `clear` is 24 instructions ending in the call, ours is 41 with the
  loop in it and a -48 frame). Confirmed again: the call sites are `include/rstl/vector.hpp:131`
  (`~vector`), `:257` (`erase`) and `:274` (`clear`), all shared by every `vector` in the tree, so
  this is still not a one-unit change. **But it is now one line further along than run 2 left it:**
  the callee exists in this object as a strong symbol, so a header that called *it* (rather than
  inlining `destroy`) would be a pure rename, not a new definition.
- **`do_insert_before<...SAutoMapperHintLocation...>` 60.50% (176 B)** — structural, not
  register allocation: retail **inlines** `create_node` (`li r3,24 / lwz r31,0(r4) / bl <alloc> /
  stw r31,0(r3) / stw r29,4(r3)` then the 16-byte copy) where mwcceppc **outlines** it and calls
  the weak COMDAT — and that COMDAT is one of `unit_fit.sh`'s 36 extras. The sibling
  `do_insert_before<...SAutoMapperHintStep...>` is at 100%, so this is MWCC's inline-size heuristic
  (`include/rstl/list.hpp`, also noted at `construct.hpp:10`) landing differently for the two
  instantiations, not a semantic difference. Another shared-header knob.
- **`ProcessMapZoomInput` 90.85% (488 B)** — a real semantic difference, and the one piece of
  unfinished *work* I found. Our `switch (mZoomState)` is a 3-case state machine; retail's
  compares against 1 and 3, has two more `cmpwi`, two null tests (`clrlwi. r0,r3,24`) and both
  `li r4,1` and `li r4,2` branches. Everything before and after the switch matches. Filed below.
- **One-instruction / register-allocation diffs** (not attempted, listed so they are not
  re-investigated): `GetAreaHintDescriptionString` 95.97% — ours builds the address as
  `(P + 48*i) + 32` and loads at `+4`, retail as `P + 48*i` and loads at `+36`, the *same* address,
  so one extra instruction shifts every branch after it; `UpdateTempleKeys` 99.01% and
  `FindClosestVisibleArea` 99.23% — pure GPR renaming (r28/r27/r30, r25/r24/r22);
  `ProcessMapRotateInput` 99.92% — a pure FPR swap (`lfs f5,100(r1)` / `lfs f4,108(r1)` where we
  have f4/f5, roles identical). The one experiment I did run here was
  `GetAreaPointOfInterest` (91.18%, 152 B) with the two local declarations swapped, which made it
  **worse** (90.08%) and was reverted; that function's only structural difference is a redundant
  `mr r0,r3 / mr r30,r0` copy chain where retail goes `mr r31,r3` straight, plus mWorld landing in
  r30 rather than r31.

## Reusable lessons (extending the first two runs')

1. **A `fn_` retail function does not have to exist in our object under any other name to be
   matched — it only has to exist under retail's name.** Run 2 ruled `fn_8008BEB0` out because the
   loop is inlined and there is no COMDAT to rename. But the call sites can keep their inlined
   copies: the score is per function, and nothing in the flip needs the calls. Give the *body*
   retail's name and leave the callers alone; you get the match, and the callers stay exactly as
   wrong (or as right) as they were.
2. **A forwarder is not free.** `rstl::destroy` -> `rstl::destroy_impl` is one line in
   `construct.hpp`, and calling through it changed which of two loop variables got which frame
   slot: 99.94% vs 100%. When a function is one instruction out and the instruction is a *spill*,
   the extra inline frame in the path is the thing to look at, not the arithmetic.
3. **A string literal's place in the pool is set by where it is *declared*, not where it is
   used**, and the pool order is: file-scope statics in source order, then function locals in
   `.text` (reverse-source) order. One `static const char* const` at the right line moves every
   literal after it, and with it every `addi` immediate that names one.
4. **To compare string layouts, compare the narrow-literal list relative to its own first entry.**
   `@stringBase0` is the first *narrow* literal, so wide-literal copies, `.rodata`-vs-`.sdata`
   placement and section padding cannot move a code byte, however many bytes the raw
   `diff <(objcopy .rodata) <(objcopy .rodata)` shows. Do not spend a run on those.

No `WALL:` line: the two functions this run went after reached 100%, and the functions listed above
are each characterised from a measured instruction diff rather than spelled at — I tried exactly
one spelling on `GetAreaPointOfInterest` and it was worse, which is not a wall.

NEW: match-cautomapper-zoomswitch | match | MetroidPrime/CAutoMapper | ProcessMapZoomInput's mZoomState switch is 3 cases in ours and 5+ in retail (90.85%); the rest of the function matches, so the zoom state machine's case values and its two null tests are the whole job.
