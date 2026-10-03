# carve-80046c0c - `MetroidPrime/Carve80046C0C`, 2 functions, Matching

**Result: `flip_test.sh` PASS, kept as `Matching`. Judge: `goal_check: PASS carve-80046c0c`.**
`main.dol` sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (retail). `matched 13542 -> 13544`,
`linked 6590 -> 6592`, `total_functions` still **28465**.

## What the range is

`.text 0x80046C0C..0x80046D44`, 0x138 = 312 bytes, 2 functions, claimed whole:

| function | address | size | instructions |
| --- | --- | --- | --- |
| `fn_80046C0C` | 0x80046C0C | 0xC0 | 48 |
| `fn_80046CCC` | 0x80046CCC | 0x78 | 30 |

`config/G2ME01/symbols.txt:1319-1320` carries the two `fn_<addr>` placeholders, so the definitions
had to stay C (a C++ one mangles to `_Z<len>fn_<addr>...` and objdiff pairs nothing) - hence `.c`.

**They are a `rstl::vector<TCachedToken<CStringTable> >::reserve(int)` and the
`rstl::uninitialized_copy` it calls**, i.e. two instantiations of `include/rstl/vector.hpp:167-179`
and `include/rstl/construct.hpp:117-127`. That reading is not inferred: both bodies are
**instruction-for-instruction identical** to two already-matched instantiations of the *same two*
templates in `src/MetroidPrime/Player/CScanDisplay.cpp` (which claims 0x801123F8..0x80116C74),
differing only in their `bl` displacements:

* twin of `fn_80046C0C`: `reserve__Q24rstl65vector<28TCachedToken<12CStringTable>,
  Q24rstl17rmemory_allocator>Fi` at 0x80116A68, 0xC0, `symbols.txt:4825`
* twin of `fn_80046CCC`: `uninitialized_copy<rstl::pointer_iterator<TCachedToken<CStringTable>,...>,
  TCachedToken<CStringTable>*>__4rstlF...` at 0x80116B28, 0x78, `symbols.txt:4826`

Element is 12 bytes = `TCachedToken< CStringTable >` (`CToken` at +0: `mObjRef`, `mLockHeld`;
`T* mItem` at +8). Per-element work: `bl __ct__6CTokenFRC6CToken` (0x803015B4, `symbols.txt:13923`)
then `lwz/stw` of the `+8` word; teardown `bl __dt__6CTokenFv` (0x8030154C, `symbols.txt:13922`)
with `li r4,0`.

## The four files (plus one host alias)

`configure.py:728`, `config/G2ME01/splits.txt` (after `Carve80045CD4.c`, before `Carve80046DB8.c`),
`files.cmake` (same position), and `src/MetroidPrime/Carve80046C0C.c`. Each entry in address order.
`Object(Matching, "MetroidPrime/Carve80046C0C.c")` is on one line, as `flip_test.sh` requires.

**Fifth file: `src/Kyoto/Alloc/PortMwccNew.cpp` gains `__ct__6CTokenFRC6CToken`** - retail's own
mangled name for `CToken::CToken(const CToken&)`, following the file's existing pattern
(`__ct__11CHealthInfoFRC11CHealthInfo`, `__ct__20CDamageVulnerabilityFRC20CDamageVulnerability`).
It is **not** a stub: the body is `CToken`'s own copy constructor. It exists only for the host link -
retail defines the name at 0x803015B4 and `Kyoto/CToken.cpp` claims that range, so the DOL resolves
it. Without it the carve's `bl` was the single new MISSING symbol and `link_check.sh` reported
`undefined went 287 -> 288`; with it: `unchanged from baseline (287 undefined, 0 duplicates)`.
That file is not in `configure.py`, so it cannot reach `main.dol`.

No `PortLinkStubs.cpp` entry had to be deleted: neither `fn_80046C0C` nor `fn_80046CCC` was stubbed.

## Measurements

```
sha1sum build/G2ME01/main.dol          6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/flip_test.sh MetroidPrime/Carve80046C0C.c   PASS -> kept as Matching
python3 tools/check_decl_order.py --unit main/MetroidPrime/Carve80046C0C
                                      ok: 1 unit(s) checked, none emits its functions out of retail order
./tools/unit_fit.sh MetroidPrime/Carve80046C0C.c
   .text claimed 312 ours 312 retail 312 fits
   no extra functions: our object defines only what the retail unit object does
python3 tools/check_symbol_names.py    599 units; 0 declared names are missing
./tools/goal_check.sh build/goal/item.json
   ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
   ok  counts: matched 13542 -> 13544   linked 6590 -> 6592
   ok  flip_test MetroidPrime/Carve80046C0C.c: PASS, Object(Matching) in configure.py
   goal_check: PASS carve-80046c0c
```

`build/report.json`: `main/MetroidPrime/Carve80046C0C` is `complete: true`, 2/2 matched, 100.00%.

`unit_fit.sh` also prints `.sbss2 claimed - ours 8 <- NOT CLAIMED by splits.txt`; that is the
compiler's own 8-byte scratch, not a claim, and the same line appears for the neighbouring carves.
It does not affect the flip.

## Codegen rules found here (each measured by changing only that one thing)

1. **mwcceppc's C mode is C89, strictly.** A declaration after a statement in a block is rejected
   ("expression syntax error" on the *next* declaration), a `const` local cannot be assigned after
   its declaration ("illegal assignment to constant"), and it will not implicitly convert a struct
   to a struct pointer. Each cost one build. This is the same C dialect the other `.c` carves use.
2. **MWCC passes a by-value class argument as a pointer to a caller-side temporary**, so the
   `uninitialized_copy` parameters must be declared **by value**, not as pointers - retail's
   `lwz r31,0x0(r3)` reads `begin`'s home slot and `mr r29,r4` keeps the *address* of `end`'s, which
   is re-read (`lwz r0,0x0(r29)`) in every loop test. Declaring them as pointers does not compile.
3. **`uninitialized_copy` must be called with two by-value iterators built from compound literals**,
   not through named locals: that is what produces retail's four home-slot stores at `r1+0x08..0x14`
   and its argument registers `addi r3,r1,0x14` / `addi r4,r1,0x0C`. (`Carve800047E0.c`'s header
   records both spellings; the same shape reappeared here unprompted.)
4. **`last = cur + self->mCount`, not `self->mItems + self->mCount`.** Written the other way,
   retail's two instructions (`lwz r30,0xc(r27)` ; `add r31,r30,r0`) become three, with
   `mr r30,r3` between them, and the function is 4 bytes long.
5. **mwcceppc hands out `r29..r31` in local-declaration order here**, so declaring `last`, `cur`,
   `newData` (that order) puts `newData` in `r29`, `cur` in `r30`, `last` in `r31` as retail does.
   The shape `include/rstl/vector.hpp:172` has - `newData` first - swaps `newData` and `last`; the
   function is otherwise identical, so this is a register-permutation difference only.
   Correspondingly in `fn_80046CCC`, declaring `cur` **before** `tmp` is what puts `cur` in `r31`
   and `tmp` in `r30`.
6. **The teardown carries a doubled null test**: `if (cur) { if (cur) { ... } }` is retail's
   `cmplwi r30,0x0 / beq .L_80046C9C / beq .L_80046C9C` (0x80046C84-0x80046C8C). One test instead of
   two is 4 bytes short. It appears only in the `destroy` loop - the `construct` side
   (`fn_80046CCC`) has a single `beq`.

`tools/carve_diff.sh` reports "NOT byte-exact" for this unit **and that is expected**: it compares
retail's range against the whole object `.text`, so it walks into `fn_80046CCC` after
`fn_80046C0C`. Compared per symbol, the only differing fields are the five `bl` displacements, which
are relocations resolved at link time - which is why `main.dol` is byte-identical to retail.

## Nothing blocked

No `NEW:` and no `WALL:` line: the item is done, both functions at 100.00% and the unit flips.

Unrelated observations, not acted on (per the brief, these belong here rather than in the diff):
- `tools/carve_diff.sh` would be more useful with a `--symbol` filter than with a `SYM` argument it
  does not use (`ours = [i for i in ours if True]`); it silently measures the wrong range for any
  unit with more than one function.
- `docs/RUNNING_THE_DECOMP.md` "The carve vein" says "only `tools/flip_test.sh` catches" a
  permuted `.text`. That is still true, but `tools/carve_diff.sh` per symbol plus the `main.dol`
  sha1 catch it too, and they are much cheaper to re-run.
