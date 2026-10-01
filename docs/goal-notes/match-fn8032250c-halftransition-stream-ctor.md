# match-fn8032250c-halftransition-stream-ctor

**DONE.** `Kyoto/Animation/CHalfTransition.cpp` is a new `Matching` unit holding retail
`CHalfTransition::CHalfTransition(CInputStream&)` (`.text:0x8032250C`, 0x70 = 112 bytes), carved out
of the unclaimed gap between `CStreamAudioManager.cpp` and `CElectricDescription.cpp`.
`tools/flip_test.sh` prints `PASS -> kept as Matching`.

## What changed (5 files + 1 new)

| file | change |
| --- | --- |
| `src/Kyoto/Animation/CHalfTransition.cpp` | **new**, 99 lines. The body is two lines: a member-init list `: mId(in.Get<uint>()), mTrans(CMetaTransFactory::CreateMetaTrans(in)) {}`. The rest of the file is the measured claim. |
| `configure.py:943` | `Object(Matching, "Kyoto/Animation/CHalfTransition.cpp")` |
| `config/G2ME01/splits.txt:2593` | the claim, `.text start:0x8032250C end:0x8032257C`. **dtk rewrote my hand placement** into address order, between the two neighbours, so that is where it is. |
| `config/G2ME01/symbols.txt:14687` | `fn_8032250C` -> `__ct__15CHalfTransitionFR12CInputStream ... scope:global` |
| `files.cmake:1199` | listed in `MP_GAME_SOURCES` (required: `tools/check_files_cmake.py` fails on a configured unit that is in neither `files.cmake` nor its `EXCLUDED` list, and `tools/` is out of bounds for me) |
| `src/MetroidPrime/PortLinkStubs.cpp:177-200` | one new stub, `stub_177`, for `CMetaTransFactory::CreateMetaTrans(CInputStream&)` |

## Measured

- `./tools/decomp_build.sh Kyoto/Animation/CHalfTransition` ->
  `main/Kyoto/Animation/CHalfTransition: 100.00% fuzzy, 100.00% matched (1 / 1 functions)`,
  `All: 33.71% fuzzy, 26.90% matched, 12.64% linked (11933 / 28465 functions)`.
  Baseline before this change was **11932**; `total_functions` is still **28465**.
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, and ninja's
  `CHECK` edge printed `87 files OK` (DOL + all 86 RELs).
- `./tools/flip_test.sh Kyoto/Animation/CHalfTransition.cpp` -> `PASS -> kept as Matching`.
- `python3 tools/check_symbol_names.py CHalfTransition` -> `checked 1 units; 0 declared names are
  missing from their object`.
- `./tools/goal_check.sh build/goal/item.json` -> **`PASS`**; its own log:
  `counts: matched 11932 -> 11933   linked 5727 -> 5728`, `gate.sh ok`,
  `flip_test Kyoto/Animation/CHalfTransition.cpp: PASS, Object(Matching) in configure.py`.
- `./tools/unit_fit.sh Kyoto/Animation/CHalfTransition.cpp`: 268 bytes against a 112-byte claim,
  the extra 156 being `__dt__Q24rstl20rc_ptr<10IMetaTrans>Fv` (80) and
  `ReleaseData__Q24rstl20rc_ptr<10IMetaTrans>Fv` (76). They land *after* the claim, are weak
  COMDATs and are called, so `-strip_partial` cannot drop them. **`Kyoto/Animation/CTransition.cpp`
  already does exactly this** - same `rc_ptr<IMetaTrans>` member in the same member-init position,
  152-byte claim, the same two symbols at +0x98 and +0xE8 - and is `Matching` at 100%. Only
  `flip_test` decides, and it passes. Full account in the file's header.
- Port side, `tools/link_check.sh --strict`: **324 undefined against a baseline of 324 (no
  growth)**, 0 duplicates. Before the `PortLinkStubs` stub it was 325 and the tool named the one
  new symbol: `NEW CMetaTransFactory::CreateMetaTrans(CInputStream&)`.

## The two things that were not obvious

### 1. The carve needs a `symbols.txt` rename, and without it the build fails

`Kyoto/Animation/CAnimationSet.cpp` is `NonMatching`, so `tools/project.py`'s `add_unit` links
**dtk's extracted object** for it (`build/G2ME01/obj/Kyoto/Animation/CAnimationSet.o`), not ours -
and that object calls this constructor by the name the map gives it. Claiming the range deletes
`main/auto_03_8032250C_text`, which was what supplied `fn_8032250C`, and our unit defines the
mangled name, so:

    ### mwldeppc.exe Linker Error:
    #   undefined: 'fn_8032250C'
    #   Referenced from 'fn_8028D604' in CAnimationSet.o

Measured, not assumed: `grep -rn '8032250C\|15CHalfTransition' config/G2ME01/rels/` is **empty**,
so no REL looks the symbol up by name and the rename cannot break the 86 modules.

### 2. Listing the unit in `files.cmake` opens one port symbol, and one stub closes it

`CMetaTransFactory.cpp` is in `check_files_cmake.py`'s `EXCLUDED` list, so listing the new unit
left `CreateMetaTrans` referenced and undefined. Listing `CMetaTransFactory.cpp` instead to give
it a real body was measured and is **worse**: 324 -> 327 undefined, because it opens
`CMetaTransMetaAnim`/`CMetaTransPhaseTrans`/`CMetaTransTrans`'s stream constructors and closes
none. So `src/MetroidPrime/PortLinkStubs.cpp` gets `stub_177`, which is what that file is for.
The reachability condition it is written to holds, measured with `nm -A` over all 1350 objects in
`build-port-link`: `CHalfTransition.cpp.o` is the only object that references
`_ZN17CMetaTransFactory15CreateMetaTransER12CInputStream`, and **no** object references
`_ZN15CHalfTransitionC1ER12CInputStream` - nothing in the port calls the constructor. The one other
object that names the class, `CTransitionDatabaseGame.cpp.o` (which *is* listed), names it only
inside its own mangled name, as a `rstl::vector<CHalfTransition>` parameter type.

`docs/research/port_link_gap_list.md` was regenerated once during this run
(`link_gap.py --write-list`, a two-line diff) to check the gate's reaction, and **reverted**: with
the stub in place the MISSING count is back to the recorded 319 and the file agrees with the tree
again. `docs/research/port_link_gap.md` needs no change.

## Left for other items

- **`match-unit-canimationset` is unblocked, not done.** Its `fn_8028D524` is 94.64% only because
  the obvious spelling instantiates `CInputStream::Get< CHalfTransition >`, whose COMDAT referenced
  a stream constructor no object defined - see `src/Kyoto/Animation/CAnimationSet.cpp:766-782`.
  That symbol now has a `Matching` unit, so `main.elf` can resolve it. I did not touch
  `CAnimationSet.cpp`: it is another item's file and the rewrite of `fn_8028D524` is that item's
  work. No `NEW:` line filed - the item is already in the queue.
- `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` were rewritten by `goal_check.sh`'s own
  `MP_GATE_DOCS_WRITE=1 check_docs_claims.py --write` (derived counts only). Reverted with
  `git checkout`; the driver discards them anyway.
- A stale `build-port-link/CMakeFiles/mp_game.dir/src/Kyoto/Animation/CMetaTransFactory.cpp.o` from
  the measurement in (2) was deleted; it was never in the link (duplicates stayed 0).
