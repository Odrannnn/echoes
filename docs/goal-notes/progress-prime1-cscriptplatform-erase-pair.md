# progress-prime1-cscriptplatform-erase-pair

`MetroidPrime/ScriptObjects/CScriptPlatform` — `progress` item, unit stays `NonMatching`.

## Result

| | before | after |
|---|---|---|
| `fn_800A1050` (248 B) | 0.00% | **100.00%** |
| `fn_800A1004` (76 B) | 0.00% | **100.00%** |
| unit `matched_functions` | 26 / 60 | **28** / 60 |
| unit `fuzzy_match_percent` | 27.79% | 29.59% |
| tree `matched_functions` | 10041 / 28465 | **10043** / 28465 |

`tools/gate.sh build/goal/judge/report.base.json` prints
`per-function diff  matched 10041 -> 10043  linked 4917 -> 4917  (+2 functions at 100%, 0 units newly linked)`.
Independent full per-function diff over **every** unit in both reports:

```
better 2   worse 0   missing-now 0
  BETTER   0.00 -> 100.00 fn_800A1004
  BETTER   0.00 -> 100.00 fn_800A1050
```

## What I changed

One file, `src/MetroidPrime/ScriptObjects/CScriptPlatform.cpp`, +32/-1.

- **Added `fn_800A1050`** (`src/.../CScriptPlatform.cpp:315-336`) — `rstl::vector<SRiders>::erase(first, last)`,
  as a free function taking the vector by reference. The body is the template in
  `include/rstl/vector.hpp:251` with `this` spelled `riders`, so the signed-division-by-60 index
  (`first - riders.begin()`), the shift loop, `riders.mCount = newCount` and `return first` are the
  same statements the compiler already produced.
- **Added `fn_800A1004`** (`:338-341`) — `return fn_800A1050(riders, it, it + 1);`, the one-argument erase.
- **One call site changed** (`:355`): `mRiders.erase(it)` -> `fn_800A1004(mRiders, it)` in
  `RemoveRider`, the only `erase` call in the tree. Without it the templates would still be
  instantiated and the object would carry both spellings.

Both are `extern "C"`, which suppresses mangling, so they carry retail's `fn_XXXXXXXX` names and
objdiff pairs them with `config/G2ME01/symbols.txt`'s address-named entries. Declared **after**
`fn_800A1148` and **before** `RemoveRider` — descending by retail offset (0x1148, 0x1050, 0x1004,
0xd38) as the file already is.

## The first try was already the whole answer

`reason` says the pair "needs sr26 = (first - mItems)/60, a shift loop that assigns
mUid/optional timer/CTransform4f per element and ends with mCount = r26 and *out = first" — i.e. that
writing the bodies from the disassembly is the job. It is not: **the `rstl::vector` templates in
`include/rstl/vector.hpp` already emit these bytes.** The prior run's note said the pair was
"already byte-identical in our object", and the only thing missing was the name. Reusing the template
body verbatim as the `extern "C"` body, with `riders` for `this`, gave 100.00% on both functions on
the first build, with no iteration.

**The object did not grow.** `powerpc-eabi-size` reports `.text` **8615 B before and after**, and
`nm | grep -c erase__` is now **0** (was 2). The extern "C" definitions replaced the template
instantiations in place; nothing was added. That is the cheap check that the change is a rename and
not a second copy of the chain.

The calling convention came out right without any of it being spelled: `iterator` is a one-word class
with a non-trivial copy, so MWCC passes the two iterators as caller-built copies in `r5`/`r6` and
returns the sret'd iterator through `r3` — the same shape the template member function had, and the
same shape retail has.

## Notes for whoever takes `DecayRiders`

`DecayRiders`, `MoveRiders` and `DragSlaves` call this pair (relocations at 0x2418, 0x2fec, 0x3268,
0x3500 in the retail object), and now that the pair carries retail's names those `bl`s are named
correctly too — `DecayRiders` still reads 1.33%, `MoveRiders` 0.45%, `DragSlaves` 0.83%, because their
own bodies are stubs, not because of the erase. `fn_800A14DC` is already 100% in this tree (another
lane landed it), so `progress-prime1-cscriptplatform-slavevec`, which was the open one there, is
answered; `fn_800A14DC` is now called by name from `AddSlave`.

Walls, unchanged, not re-filed: `GetSortingBounds` 92.12%, `GetTouchBounds` 93.10%, `IsSlave` 95.29%,
`BuildNearListFromRiders` 98.01%, `__dt__` 77.78% / `__ct__` 42.52%
(`progress-prime1-cscriptplatform-dtor`).

`tools/unit_fit.sh MetroidPrime/ScriptObjects/CScriptPlatform.cpp` still lists 26 extra functions
(2724 B), all pre-existing template constructors/destructors and dtor instantiations; `fn_800A1004`
and `fn_800A1050` are not among them — they pair with retail. The unit cannot flip until the
constructors and `__ct__`/`__dt__` are done, which is the dtor item.

## Verified

```
sha1sum build/G2ME01/main.dol       -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/decomp_build.sh             -> All: 30.97% fuzzy, 23.24% matched, 11.78% linked (10043 / 28465 functions)
./tools/probe_sources.sh            -> probe: 749 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py -> checked 503 units; 0 declared names are missing from their object
python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptPlatform
                                    -> ok: 1 unit(s) checked, none emits its functions out of retail order
./tools/gate.sh build/goal/judge/report.base.json
```

`gate.sh` is ok on every step except `docs claims`, which reports only the two derived state-block
numbers this change moved (`matched 10043 / 28465`, `DOL units 8632 / 16726`). Per the brief I did not
hand-edit `docs/HANDOFF.md`: `goal_check.sh:105` runs the gate with `MP_GATE_DOCS_WRITE=1`, so the
judge rewrites them.

Diff is **one file**: no `.s`, no `asm`, no `tools/`, no `config/`, no `docs/`, no `build/goal/`.
Not committed. `objdiff.json` deliberately not touched.

## NEW:

(none — the item's stated work was real and is done; the remaining sub-100% functions in this unit
are walls already recorded above or belong to `progress-prime1-cscriptplatform-dtor`.)
