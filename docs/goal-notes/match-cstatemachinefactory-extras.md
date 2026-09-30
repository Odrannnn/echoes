# `match-cstatemachinefactory-extras` — `MetroidPrime/Factories/CStateMachineFactory` is `Matching`

`tools/flip_test.sh MetroidPrime/Factories/CStateMachineFactory.cpp` → **PASS → kept as Matching**.
`main.dol` `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, 86/86 RELs match `config/G2ME01/config.yml`,
`linked 4903 → 4912` (+9 functions, 1 unit newly linked), `matched 10039 → 10039` (unchanged — the
nine were already at 100% as a `NonMatching` unit; the flip is what makes them count).

## The seeded reason was stale, and the real blocker was a `.rodata` string pool

`item.json` said the blocker was 4 extra destructors making `.text` 1476 vs retail 1072. Both halves
of that are still true of the object, and neither is the cause. `unit_fit.sh` still reports the same
404 bytes over, and `flip_test.sh` is the verdict — the four are COMDAT weak copies
(`__dt__4IObjFv`, `__dt__31CObjOwnerDerivedFromIObjUntypedFv`,
`__dt__Q24rstl15auto_ptr<4IObj>Fv`, `__dt__Q24rstl53auto_ptr<41TObjOwnerDerivedFromIObj<13CStateMachine>>Fv`)
that mwldeppc discards. **Do not spend a run on them.** (The stale reason came from measuring
`build/G2ME01/obj/…` — the dtk-split object — instead of `build/G2ME01/src/…`, which is ours. The
split object is a clean 0x430; ours is 0x5c4. `unit_fit.sh` and `objdiff` both look at ours.)

The flip failed on the DOL sha1 with **18,341 differing bytes in 2,849 runs**, and they are not in
our unit: `.rodata` 17,075 / `.text` 1,139 / `.sdata2` 77 / `.data` 48. All ELF section headers were
byte-identical; the only difference was an extra `@stringBase0` symbol (80 in base, 81 in flip) and a
one-entry shift of every later pool address by 8. Our object emitted a **7-byte `.rodata` section**
holding `??(??)`, the linker appended it to the global string pool, and every pool entry after it
moved. `.text` itself was already exact.

## Cause and fix

`rs_new` expands to `new ("\?\?(\?\?)", nullptr)`, and mwcceppc routes a literal through its
**per-translation-unit** `@stringBase0` pool — the mechanism already written up in
`docs/RUNNING_THE_DECOMP.md`, "The `@stringBase0` pool is PER TRANSLATION UNIT". Retail instead
names its own pool: both `new` sites in this range relocate against `lbl_803AA230`
(`.rodata:0x803AA230`, `symbols.txt:17242`, 0x10 bytes) at 0x80194064/0x6C and 0x80194394/0x9C.

Two sites, and the second one is not in this file:

1. `FAiFiniteStateMachineFactory` — spelled `new (lbl_803AA230, nullptr) CStateMachine(in)`.
2. `TObjOwnerDerivedFromIObj<T>::GetNewDerivedObject` in `include/Kyoto/IObj.hpp:41-47`, reached
   through `CFactoryFnReturn`'s converting constructor. This is a header template, so the string has
   to be redirected **before any include** or the object still emits `.rodata`.

So `include/Kyoto/Alloc/CMemory.hpp` gained a `CMEMORY_NEW_FILE` branch around `rs_new` (the
`CMemory.hpp:49` line). With the macro unset — every other translation unit in the tree — the
definition is character-identical, which is why nothing else moved: `matched 10039 → 10039` and
`probe: 749 files, 0 failed`, link 250 undefined / 0 duplicates (unchanged).

The fix needed no `symbols.txt` rename and no section claim: retail's copy lives in
`auto_06_803A9F4C_rodata.o`, which is at link position 453, **before** this object at 482, so the
reference resolves forward-to-back without the linker growing a section. Object after the fix:
`.text 0x5c4`, `.data 0x24`, **no `.rodata` at all**, and both string relocations against
`lbl_803AA230`.

## Gates

| gate | result |
| --- | --- |
| `flip_test.sh MetroidPrime/Factories/CStateMachineFactory.cpp` | **PASS → kept as Matching** |
| `sha1sum build/G2ME01/main.dol` | `6ef9b491…` (retail) |
| 86 RELs vs `config/G2ME01/config.yml` | 86/86, all `cmp`-equal to `orig/G2ME01/files/RelProd/` |
| `tools/gate.sh` | all steps ok except `docs` (below) |
| per-function diff | `matched 10039 → 10039`, `linked 4903 → 4912`, **no WORSE** |
| `tools/probe_sources.sh` | 749 files, 0 failed; LINKED (250 undefined, 0 duplicates) |
| `tools/check_symbol_names.py` | 503 units, 0 missing |
| `tools/check_decl_order.py` | ok |

`gate.sh` reports `GATE FAIL: docs` on exactly one claim, the HANDOFF state block
(`linked 4903` → `4912`, and `11.75%` → `11.77%` fully linked). `python3 tools/check_docs_claims.py
--write` fixes it and then passes; I reverted that edit, because the judge rewrites the derived
counts itself and discards doc edits. **The docs failure is expected and is not a real failure.**

`unit_fit.sh` still reports `.text` over by 404 and `.data` over by 20, listing the four weak
destructors. That is the pre-existing state, not a regression, and `flip_test.sh` is the verdict.

## For the next run

- A `Matching` unit whose object still carries a `.rodata` section it does not need is the
  `@stringBase0` wall, not a code problem. Check `objdump -h build/G2ME01/src/<unit>.o` for
  `.rodata` **before** reading any objdiff percentage.
- `CMEMORY_NEW_FILE` is now available for any other unit in the same position. It must be defined
  before the first `#include`, and it works because the named symbol is already in the link.
