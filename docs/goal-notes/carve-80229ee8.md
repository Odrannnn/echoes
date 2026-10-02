# carve-80229ee8 - `MetroidPrime/ScriptLoader/Carve80229EE8` (match) - PASS

**Result: `tools/goal_check.sh build/goal/item.json` -> `PASS carve-80229ee8`.** The unit is in the
report at 100.00% matched (1/1), `flip_test.sh` kept it, and the DOL is retail's again.

## What the item was

Carve `.text 0x80229EE8..0x80229F90` (0xA8 = 168 bytes, 42 instructions, **one function**,
`fn_80229EE8`, `symbols.txt:9830`) out of dtk's `auto_03_80229EE8_text` and match it.  This is the
remainder the earlier carve `Carve80229EE0.c` (8 bytes) left; both neighbours were already claimed
(`Carve80229EE0.c` ends exactly at 0x80229EE8, `FlyerSwarm.cpp` starts exactly at 0x80229F90), so
the auto unit disappears rather than being shortened.

## What the function is (this is the part the item's `reason` got partly wrong)

It is **not** a `CIngPuddle` initialiser.  It is the four-argument initialiser of the swarm data
class that `include/MetroidPrime/Enemies/CSwarmBasics.hpp:15` forward-declares as
`CBasicSwarmData`:

    +0x00  CDamageInfo, 0x1C      copied member-wise, inlined by MWCC (no fitting copy ctor)
    +0x1C  CHealthInfo, 0x20      bl __ct__11CHealthInfoFRC11CHealthInfo        (0x80070D60)
    +0x3C  CDamageVulnerability   bl __ct__20CDamageVulnerabilityFRC20CDamageVulnerability
                                  (0x8001C634, symbols.txt:510)
    +0xA8  one 4-byte field       stw r31, 0xa8(r29) - the fifth argument
                                  and `mr r3, r29`: the function returns the object.

Evidence, all measured:

* Caller `LdrToBasicSwarmData__FRC24SLdrBasicSwarmProperties` (0x80239C68, `symbols.txt:10094`,
  dtk unit `auto_03_802399F4_text`) builds a `CDamageInfo` (`bl LdrToDamageInfo` 0x8023B2F8),
  a `CHealthInfo` (`bl LdrToHealthInfo` 0x8023B2D0) and a `CDamageVulnerability`
  (`bl LdrToDamageVulnerability`) in locals, reads `lwz r7, 0x1bc(r31)`, and calls this function
  with exactly those (`auto_03_802399F4_text.s:232-244`).  It then fills +0x6C..+0xDC itself and
  copy-constructs its return value with the same class's copy ctor `fn_80239E94` (0x80239E94,
  0x190, line 324), whose head is byte-for-byte this function's pattern and whose last copied byte
  is +0xDC - i.e. the same object.
* Module loaders pass that return value as the eighth argument of `CSwarmBasics`'s constructor
  (`const CBasicSwarmData& data`, `CSwarmBasics.hpp:78-80`): `FlyerSwarm`'s loader does
  (`build/G2ME01/FlyerSwarm/asm/auto_00_000000D8_text.s:208,241`), and so do `MetareeSwarm`,
  `PlantScarabSwarm`, `IngBlobSwarm`, `BacteriaSwarm`, `EmperorIngStage3`.

## The two spellings that mattered

1. **The return.**  Retail's last instruction before the epilogue is `mr r3, r29` (0x80229F70).
   A `void` body reaches byte-exactness on the other 41 instructions and omits exactly that `mr`
   (measured: this was the only diff in the first build).  The definition returns
   `CBasicSwarmData&` and ends `return *self;`.  Rebuilt diff: `.text` of
   `build/G2ME01/src/MetroidPrime/ScriptLoader/Carve80229EE8.o` against dtk's pre-carve
   `build/G2ME01/obj/auto_03_80229EE8_text.o` is identical, same size (0xA8) and same two
   `R_PPC_REL24` relocs at +0x74/+0x80.
2. **A host definition the item's reason said already existed.**  The reason said "both callees are
   already declared and host-defined in `src/`".  `__ct__11CHealthInfoFRC11CHealthInfo` is
   (`PortMwccNew.cpp:99`), but **`__ct__20CDamageVulnerabilityFRC20CDamageVulnerability` was not
   defined anywhere in `src/` or `include/`** (`grep -rn` over both: no definition).  Without one,
   this carve's `bl` is a new undefined symbol on the host link.  Added next to the sibling in
   `src/Kyoto/Alloc/PortMwccNew.cpp` (a port-only file, not in `configure.py`, so it cannot affect
   `main.dol`): `new (self) CDamageVulnerability(*static_cast<const CDamageVulnerability*>(src));`.
   Measured after: port link `unique undefined symbols 285` against `undef.base.count` 285 (the
   gate's own line: "285 undefined against a baseline of 287 (no growth)").

## Files changed

* `src/MetroidPrime/ScriptLoader/Carve80229EE8.cpp` (new) - the carve's own claim; `extern "C"` so
  the symbol stays `fn_80229EE8` unmangled; one function, so declaration order cannot be wrong.
* `config/G2ME01/splits.txt:1734-1735` (`.text start:0x80229EE8 end:0x80229F90`),
  `configure.py` (`Object(Matching, "MetroidPrime/ScriptLoader/Carve80229EE8.cpp")` after
  `Carve80229EE0.c`), `files.cmake` - the other three parts of the carve.
* `src/Kyoto/Alloc/PortMwccNew.cpp:103-117` - the host copy-ctor definition above.

`total_functions` is still **28465** after the `splits.txt` edit (build's `All:` line).

## Verification (all measured in this tree, 2026-10-02)

    ./tools/decomp_build.sh            sha1sum build/G2ME01/main.dol
                                       6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  # retail
                                       "87 files OK"
                                       main/MetroidPrime/ScriptLoader/Carve80229EE8:
                                       100.00% fuzzy, 100.00% matched (1 / 1 functions)
                                       All: 13056 / 28465 functions
    ./tools/flip_test.sh ...Carve80229EE8.cpp      PASS -> kept as Matching
    ./tools/unit_fit.sh  ...Carve80229EE8.cpp      fits (168/168/168), no extra functions
    python3 tools/check_decl_order.py --unit ...   ok
    python3 tools/check_symbol_names.py            0 missing names
    python3 tools/check_raw_offsets.py             no new raw-offset site (this file has none)
    tools/link_check.sh --strict                   285 undefined, 0 duplicates, no growth
    all 86 RELs cmp-equal to orig/G2ME01/files/RelProd/
    ./tools/goal_check.sh build/goal/item.json     PASS
        matched 13055 -> 13056   linked 6165 -> 6166
        gate: "per-function diff SPLIT main/auto_03_80229EE8_text: 1 function(s) accounted for
        across 1 new unit(s) in main (exact count match - a split, not a loss)"

No `WALL:`, no `STALE:` (the item was not already done), no `NEW:` - nothing new to queue.

## Notes for the next reader (not filed as items)

* **`Carve80229EE0.c`'s comment is now stale by one sentence.**  It says the remainder "comes back
  under a new name, `auto_03_80229EE8_text` covering 0x80229EE8..0x80229F90 ... and it needs no
  `configure.py` entry".  That was true when it was written; this carve now claims that range.  I
  left it alone because the change would be an out-of-scope hunk in a neighbouring unit, but the
  next person touching that file should correct it in place.
* `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified because `goal_check` runs
  `gate.sh` with `MP_GATE_DOCS_WRITE=1`; that is the judge rewriting the derived state block, not
  an agent edit.  No agent edit was made to them.
