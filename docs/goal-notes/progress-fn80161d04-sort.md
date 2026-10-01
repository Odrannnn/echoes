# progress-fn80161d04-sort

`kind: progress`, target `main/auto_03_80161D04_text`. **PASS** (`tools/goal_check.sh` exits 0).
The unit went **0 -> 3 of 3** matched, 0.00% -> **100.00%** fuzzy, and `metadata.complete` is now
`true` — it is also **`Matching`**, promoted by `tools/flip_test.sh`, not asserted.

Diff, four files (a carve is four files or it is not a carve):

| file | change |
|---|---|
| `src/auto_03_80161D04_text.cpp` | **new**, the three function bodies |
| `config/G2ME01/splits.txt` | claim `.text 0x80161D04..0x80161FBC` |
| `configure.py` | `Object(Matching, "auto_03_80161D04_text.cpp")` |
| `files.cmake` | `src/auto_03_80161D04_text.cpp` |

## What landed

The item's reason was right about the body and wrong about the shape of the work. The three
functions are `rstl::sort` / `__insertion_sort` / `__sort3` for
`rstl::vector< rstl::pair< uint, uint > >`, and the bodies are the `include/rstl/algorithm.hpp`
templates written out with the template arguments concrete. **All three reached 100.00% on the
first build**, with no spelling iterations at all — the templates in this repo are already the
retail source, so the only real problem was naming, not codegen.

```
main/auto_03_80161D04_text  100.00% fuzzy, 100.00% matched code, 3 / 3 functions
   fn_80161F40 100.0  124    __insertion_sort
   fn_80161EC8 100.0  120    __sort3
   fn_80161D04 100.0  452    sort
All: 32.86% fuzzy, 25.70% matched, 12.18% linked (11456 / 28465 functions)
```

## The thing that decides whether a carve of an `auto_*` range links: the name

**The definitions must reproduce the `fn_` placeholder names verbatim, and they must be
`extern "C"`.** Measured, not inferred:

```
build/binutils/powerpc-eabi-nm build/G2ME01/obj/MetroidPrime/Player/CGameOptions.o | grep fn_80161D04
build/binutils/powerpc-eabi-nm build/G2ME01/obj/MetroidPrime/CMemoryCard.o            | grep fn_80161D04
```

Both print `U fn_80161D04`. `CGameOptions.cpp` (retail 0x80160E60, inside `ResetControllerAssets`)
and `CMemoryCard.cpp` (`InitializePump`, 0x80177204) both call the range's **first** function, and
`symbols.txt` calls it `fn_80161D04` — a placeholder, not a mangled name. Claiming the range
removes dtk's `auto_03_80161D04_text` object, which is what was satisfying those two references
today, so the carve has to take the name over:

* defining `rstl::sort<...>` mangles to `sort<...>` and leaves `fn_80161D04` **undefined** — the
  link fails (`rstl/rstl_string_l.cpp` documents the same trap for `fn_802FF3AC`);
* the definitions stay C++ because the parameters are class template instantiations, so a `.c`
  file was not an option here the way it was for `Carve800239F4.c`.

That makes the file **100% a rename problem**: `extern "C"` plus the concrete typedefs
(`TPair`, `TPairVector`, `TIt`, `TCmp`, spelled out from `rstl::vector<TPair>::iterator`) is the
whole change. `tools/check_symbol_names.py` -> `515 units; 0 declared names are missing` — it
skips `fn_`/`lbl_` placeholders, so this costs nothing at the symbol-name gate and needs **no
`symbols.txt` rename**.

**The file is named `auto_03_80161D04_text.cpp`, after the dtk auto unit it replaces.** That is
deliberate and is the one naming choice a reviewer should check: `build/report.json` names a DOL
unit after its object path, so keeping the name keeps `main/auto_03_80161D04_text` resolvable,
which is what this item's target is and what `goal_check.sh`'s `target_rose` looks up. A
differently-named file would have scored the same 3 functions but left the target unresolvable.

## What the range is, and the four things that are not guessable from the types

0x2B8 = 696 bytes, three functions, both ends retail function boundaries, no other unit's:
`fn_80161D04` 0x1C4 sort / `fn_80161EC8` 0x78 `__sort3` / `fn_80161F40` 0x7C `__insertion_sort`.
Neighbours: `MetroidPrime/Player/CGameOptions.cpp` ends at 0x80161D04 and
`MetroidPrime/CEnvFxManager.cpp` starts at 0x80161FBC, so this is a whole-unit carve of the gap,
not a cut out of a named unit. `tools/range_owner.py .text 0x80161D04 0x80161FBC` said
`UNCLAIMED` before the change and `total_functions` is still **28465** after it.

* **The key is word 0 and `less<uint>` is empty, so the whole comparator is one byte.** Every
  compare in all three functions is a single `cmplw` on `first`. That is why
  `pair_sorter_finder::operator()` inlines to nothing and why `__sort3` never reloads its
  fourth parameter.
* **`count` is elements, and `subf / srawi 3 / addze` is MWCC's `T* - T*`** (byte difference,
  divide by `sizeof(T)`, round towards zero) — not a source-level `/ 8`. `cmpwi r4,0x14` is 20
  elements; `srwi 31 / add / srawi 1 / slwi 3` is `count / 2 * sizeof(T)`, i.e. `first + count / 2`.
* **The partition reads the pivot out of the array once** (`lwz r7,0(r28)` = `*mid`, `r7` live
  across both scans) — it is not a pivot copied into a separate variable, and there is no
  `__sort3` result kept. The two scans and `iter_swap` are exactly the header's.
* **`__insertion_sort`'s `for (++next; ...)` is the counted form**: the trip count is
  `((last + 7) - (first + 8)) >> 3` = `n - 1`, computed once before the loop, and the loop is
  `mtctr`/`bdnz`. `rstl/algorithm.hpp`'s `for (++next; next < last; ++next)` already produces
  that, so nothing had to be tuned.

## Reverse declaration order was checked before the build, not after

`python3 tools/check_decl_order.py --unit auto_03_80161D04_text` -> `ok: 1 unit(s) checked, none
emits its functions out of retail order`. The source declares `fn_80161F40`, then `fn_80161EC8`,
then `fn_80161D04` — descending by retail address, because mwcceppc emits in reverse source order
and mwldeppc keeps `.text` verbatim. `tools/unit_fit.sh auto_03_80161D04_text.cpp` ->
`.text claimed 696 ours 696 retail 696 fits`, **no extra functions** — unlike
`MetroidPrime/Player/CGameOptions.cpp`, whose object still carries the 8 COMDAT-weak copies of
these same three instantiations (that unit's caveat from the previous run, unchanged here).

## Verification

```
sha1sum build/G2ME01/main.dol                    6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/decomp_build.sh                          All: 32.86% fuzzy, 25.70% matched, 12.18% linked (11456 / 28465)
./tools/flip_test.sh auto_03_80161D04_text.cpp   PASS  -> kept as Matching
tools/unit_fit.sh auto_03_80161D04_text.cpp      .text claimed 696 ours 696 retail 696 fits; no extra functions
python3 tools/check_decl_order.py --unit auto_03_80161D04_text   ok, no permutation
python3 tools/check_symbol_names.py              515 units; 0 declared names are missing
./tools/probe_sources.sh                         753 files, 0 failed, 0 errors; LINKED (250 undefined, 0 duplicates)
python3 tools/check_files_cmake.py               every configured DOL object is in files.cmake or excluded
python3 tools/check_raw_offsets.py               162 raw-offset site(s) in 69 file(s), all documented
./tools/link_check.sh                            250 undefined, 0 duplicates, unchanged from baseline
tools/goal_check.sh build/goal/item.json
  ok  no judge-owned path touched
  ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)   -> GATE PASS a05e1bcf+6 changed
  ok  counts: matched 11453 -> 11456   linked 5587 -> 5590
  ok  check_symbol_names.py
  ok  All:  32.86% fuzzy, 25.70% matched, 12.18% linked (11456 / 28465 functions)
  ok  target rose: main/auto_03_80161D04_text: 0 -> 3 / 3 functions
  ok  no asm added
  PASS progress-fn80161d04-sort
```

`gate.sh` ran with `MP_GATE_DOCS_WRITE=1` inside `goal_check.sh` and rewrote the derived counts in
`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md`; both were reverted, the driver regenerates
them. Nothing else is edited, no `asm` was added, nothing under `tools/`, `build/goal/` or
`docs/research/port_link_baseline.txt` was touched, and nothing is committed.

## Follow-up for whoever takes the `match` item on `MetroidPrime/Player/CGameOptions.cpp`

The caveat the previous run left is now easier to state. That unit's object carries **8
COMDAT-weak functions the retail object does not define, 1288 bytes**, and `unit_fit.sh` still
says so:

```
+452  sort<pointer_iterator<pair<Ui,Ui>,vector<pair<Ui,Ui>,...>,...>,pair_sorter_finder<...>>
+184  __as__ ... vector<pair<Ui,Ui>,...>          +172  reserve ...
+140  __dt__ reserved_vector<pair<b,b>,4>          +124  __insertion_sort ...
+120  __sort3<pair<Ui,Ui>,pair_sorter_finder<...>>  +84  __dt__ ... vector<pair<Ui,Ui>,...>
+12   clear ... vector<pair<Ui,Ui>,...>
```

Three of those eight (`sort`, `__insertion_sort`, `__sort3`, 696 bytes between them) are
**byte-identical to what this carve now defines as `fn_80161D04`/`fn_80161F40`/`fn_80161EC8`** —
measured, by comparing the COMDAT bytes in `CGameOptions.o` against the retail object. So the
remaining obstacle is the other five (`__as__`, `__dt__`, `reserve`, `clear` and the
`reserved_vector` destructor, 592 bytes) plus the fact that a `Matching` unit still cannot emit
a symbol under two names. This item did **not** touch that unit's file; nothing about its
`NonMatching` state changed.

## NEW

(none — the item's target rose 0 -> 3 and the unit also flipped, so this is a complete result and
there is nothing new to queue. The `CGameOptions` flip caveat above is characterised, not
re-queued: it needs a different technique, not another spelling of the same body.)