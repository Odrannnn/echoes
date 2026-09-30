# progress-prime1-canimsourcereader

Unit: `Kyoto/Animation/CAnimSourceReader.cpp` (DOL unit `main/Kyoto/Animation/CAnimSourceReader`),
stays `NonMatching`. Source: `src/Kyoto/Animation/CAnimSourceReader.cpp`, one file changed, +119/-12,
no other file touched, no `asm`.

## Result

All four seeded functions are now exact matches. The unit goes 23/31 -> 27/31 matched functions.

| function | before | after | Prime 1's source |
|---|---|---|---|
| `VAdvanceView__17CAnimSourceReaderFRC13CCharAnimTime` | 3.52% | **100.00%** | ported, needed the Echoes names below |
| `VReverseView__17CAnimSourceReaderFRC13CCharAnimTime` | 3.52% | **100.00%** | ported, needed the Echoes names below |
| `VGetAdvancementResults__17CAnimSourceReaderCFRC13CCharAnimTimeRC13CCharAnimTime` | 3.51% | **100.00%** | ported, needed the Echoes names below |
| `VClone__17CAnimSourceReaderCFv` | 99.58% | **100.00%** | one extra explicit wrapper (below) |

Per-function diff against `build/goal/judge/report.base.json`: **0 functions worse, 4 better**.
Global `matched_functions` **9986 -> 9990**; `matched_code_percent` 22.983389 -> 23.031982;
`fuzzy_match_percent` 30.751669 -> 30.796028; `total_functions` still 28465; `complete_code_percent`
unchanged (the unit is still `NonMatching`, as the item requires).

## What Echoes' fork changed relative to Prime 1

Prime 1's file is structurally the same function; only the types and two idioms differ. No Prime 1
header was copied and no class layout was changed - every member offset used here is this repo's.

- `CAdvancementResults` -> `SAdvancementResults`, `CAdvancementDeltas` -> `SAdvancementDeltas`
  (`include/Kyoto/Animation/IAnimReader.hpp`).
- Prime 1's `CAdvancementDeltas()` default-constructs its members. Echoes' `SAdvancementDeltas` has
  an empty default ctor, so the zero cases are written explicitly as
  `SAdvancementDeltas(CVector3f::Zero(), CQuaternion::NoRotation())`. Verified against the retail
  disassembly: the wrap branches load `sZeroVector__9CVector3f` / `sNoRotation__11CQuaternion` and
  then call `__ct__19SAdvancementResultsFRC13CCharAnimTimeRC18SAdvancementDeltas` (0x12e8, 0x1364,
  0x328, 0x3c4 in the retail object).
- Prime 1 guards the POI update with `if (mSource->HasPOIData())`. Echoes' `CAnimSource` has no
  `HasPOIData`; retail calls `UpdatePOIStates__21CAnimSourceReaderBaseFv` unconditionally
  (0x13d4, once, in `VAdvanceView`). So the guard is dropped.
- `CSegId::Root()` does not exist here; retail emits `li r0,0; stb r0,8(r1)` (0x13e0), i.e. the root
  segment is `CSegId(0)`.
- `VReverseView` in Prime 1 tests `mCurTime < CCharAnimTime()`; retail calls
  `__lt__13CCharAnimTimeCFRC13CCharAnimTime` (0x8e0) with a `CCharAnimTime` built by
  `__ct__13CCharAnimTimeFf` at 0x8d4, so the Prime 1 spelling is already right.
- `VClone`: the file returned `rs_new CAnimSourceReader(...)` relying on an implicit conversion.
  That gave 99.58%, one instruction short - retail stores the 12 constructor arguments in a
  different order, which the explicit
  `return rstl::ownership_transfer< IAnimReader >(rs_new CAnimSourceReader(...));` matches.
  (This is also the spelling `CFBStreamedAnimReader::VClone` in this repo already uses.)

`CSubAnimTypeToken<CAnimSource>` is 0x10 bytes here, so `mSource` is at 0x58 and `mSteadyStateInfo`
at 0x68; retail's `VClone` passes `r31+88` and `r31+104` (0xb98, 0xba8), which confirms the layout
was already right and no header change was needed.

## Verification

```
sha1sum build/G2ME01/main.dol   -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh        -> probe: 749 files, 0 failed, 0 errors
                                   link: LINKED (250 undefined, 0 duplicates)
build/probe-logs/link_check.log -> STRICT PASS, 250 against a baseline of 250
python3 tools/check_symbol_names.py -> checked 503 units; 0 declared names are missing
./tools/decomp_build.sh         -> All: 30.80% fuzzy, 23.03% matched (9990 / 28465)
all 86 RELs                     -> cmp-equal to orig/G2ME01/files/RelProd/ (86 ok, 0 diff)
python3 tools/check_decl_order.py --unit Kyoto/Animation/CAnimSourceReader
                                 -> ok, none emits its functions out of retail order
```

`python3 tools/check_docs_claims.py` reports the two derived counts in the `HANDOFF.md` state block as
stale (it wants `9990 / 28465` and `DOL 8579 / 16726`). Those are the judge's to rewrite from the
tree, and the brief forbids editing `docs/HANDOFF.md`, so the numbers are recorded here instead.

## Left in this unit (not in the item's scope)

Four functions in the retail object are still 0%, and none of them is in the seeded list:

- `fn_802A2E14` (84 bytes) - a destructor: frees `*(this+0xc)` via `Free__7CMemoryFPCv`, then, if
  the `int` argument is positive, `Free(this)`. A deleting destructor for a class whose vtable is at
  0 with a sub-object at 0xc.
- `fn_802A2E68` (132 bytes) - the same shape over a `rstl::vector` of 8-byte elements at 0xc.
- `fn_802A3AD0` (176 bytes) and `fn_802A3B80` (328 bytes) - copy constructors: allocate
  `count << 3` bytes with `allocate__Q24rstl17rmemory_allocatorFi` and copy 8-byte elements.
  `fn_802A3B80` copies `pair<uint, bool>`-shaped elements (`lwz` + `lbz` per 8 bytes),
  `fn_802A3AD0` copies two-word elements.

They are the implicit copy-ctor / destructor bodies of vector members of classes this TU instantiates
but does not name, and this repo's build emits its equivalents as weak COMDAT copies
(`tools/unit_fit.sh` lists 12 such extras, 1492 bytes, including three
`rstl::vector<rstl::pair<Ui,?>>` copy-ctors at 176 bytes each and three `__dt__` at 132 bytes each -
the same sizes as `fn_802A3AD0` and `fn_802A2E68`). They are not reachable from the source text
without naming a class the headers do not expose, so they are recorded rather than guessed at.
`tools/unit_fit.sh` also reports `.data` 152 bytes against a claimed 256, so this unit is not a flip
candidate regardless; the item did not ask for one and `flip_test.sh` was not run.

No `NEW:` line: nothing here is blocked in a way that a different target would unblock, and the unit
itself cannot flip until those unnamed retail functions are accounted for.
