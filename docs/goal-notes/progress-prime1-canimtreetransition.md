# progress-prime1-canimtreetransition

`kind: progress`, target `Kyoto/Animation/CAnimTreeTransition`. Stays `NonMatching`; no
`flip_test.sh` was run. Adapted Prime 1's `src/Kyoto/Animation/CAnimTreeTransition.cpp` to this
repo's own headers and member names, changing only what the measured diff showed.

## Result

`main/Kyoto/Animation/CAnimTreeTransition`: **5/18 -> 9/18** functions at 100%,
matched code 15.83% -> 46.56%, fuzzy 40.44% -> 88.34%.
`All:` 30.34% fuzzy / 22.19% matched / 11.74% linked (9834) -> 30.37% / 22.21% / 11.74% (9838).

`build/report.json`, per function, before -> after:

| function | before | after | Prime 1's source |
| --- | --- | --- | --- |
| `VAdvanceView` | 4.69% | **100%** | unchanged apart from `CAdvancementResults`->`SAdvancementResults` and `GetBlendingWeight`->`VGetBlendingWeight` naming |
| `VGetBestUnblendedChild` | 77.73% | **100%** | small edit - needs `if (!child) return right; return child;` |
| `VGetBlendingWeight` | 86.90% | **100%** | unchanged, one expression |
| `VGetTimeRemaining` | 75.15% | **100%** | small edit - needed a const-ref `max_val` (see below) |
| `VSimplified` | 1.80% | 94.33% | unchanged, body restored |
| `VReverseSimplified` | 5.78% | 92.80% | unchanged, body restored |
| `AdvanceViewForTransitionalPeriod` | 16.44% | 97.58% | unchanged, body restored |
| `VClone` | 86.86% | 94.13% | small edit - needed the out-of-line clone call (see below) |
| 7-arg `__ct__` | 75.57% | 88.41% | unchanged; `GetBoolPOIState("Loop")` -> `VGetBoolPOIState(GetLoopPOIHash())` |
| `fn_802AA708`, `fn_802AA224`, `fn_802AA020`, `fn_802A9FD4` | 0.00% | 0.00% | unnamed in retail; see "What is left" |

No function anywhere got worse (`tools/report_diff.py build/report.pre_cre.json build/report.json`
prints `no regression`). The diff adds no `asm`. Two files touched:
`src/Kyoto/Animation/CAnimTreeTransition.cpp`, `include/Kyoto/Animation/CAnimTreeTransition.hpp`.

The three bodies the unit was missing as TODO stubs - `VSimplified`, `VReverseSimplified`,
`AdvanceViewForTransitionalPeriod`, `VAdvanceView` - are Prime 1's, unchanged except for this
repo's names (`CAdvancementDeltas`->`SAdvancementDeltas`, `CAdvancementResults`->`SAdvancementResults`,
`Clone()`->`VClone()`, `Simplified()`->`VSimplified()`, `GetBlendingWeight()`->`VGetBlendingWeight()`,
`IncAdvancementDepth`/`DecAdvancementDepth`/`mCullSelector` already existed here). They call
`AdvanceViewBothChildren`, which is still a TODO stub in
`CAnimTreeDoubleChild.cpp`; that is why `AdvanceViewForTransitionalPeriod` stops at 97.58% and why
`VAdvanceView` can only match where the child's contribution is not observable.

## The two things that needed a decision, and why

### `rstl::max_val` takes and returns by value here, by reference in Prime 1

`VGetTimeRemaining` compares two `CCharAnimTime`s in place and copies out one of them:

```
addi r3,r1,0x10 ; bl __lt__13CCharAnimTimeCFRC13CCharAnimTime
addi r3,r1,0x10 ; beq ... ; addi r3,r1,0x8
lfs f0,0(r3) ; stfs f0,0(r30) ; lwz r0,4(r3) ; stw r0,4(r30)
```

`include/rstl/math.hpp` here has `inline T max_val(T a, T b)`, which makes each operand a
by-value parameter and materialises both into temporaries first. Prime 1's
`extern/rstl/include/rstl/math.hpp` has `inline const T& max_val(const T& a, const T& b)`, which
compares in place. Measured, one function at a time, on this unit:

- `rstl::max_val(mB->VGetTimeRemaining(), mTransDur - mTimeInTrans)`, repo header: **75.15%**
- two named locals + `bRem < transRem ? transRem : bRem`: **54.82%** and **74.32%**
- a file-local `inline const T&` template taking references: **100%**

Changing the shared header is not in scope for this item and breaks 15 other units
(`rstl::max_val< const CCharAnimTime& >(...)` in `VGetSteadyStateAnimInfo` and callers elsewhere stop
deducing; the full build fails on `CAnimTreeTransition.cpp:74` and then on 14 REL modules). So the
unit keeps a file-local `max_val_in_place` in the `.cpp` with a comment saying why. **A separate
item should fix `include/rstl/math.hpp` to the reference signature and fix up its callers** - that
is the real defect, and the local copy is a workaround for it, not the fix.

### Retail calls a weak out-of-line `IAnimReader::Clone`

`VClone` clones via `bl Clone__11IAnimReaderCFv` (a weak out-of-line function, `symbols.txt:11515`,
`.text:0x8028F244`) rather than inlining the vtable dispatch. Here `IAnimReader::Clone()` is an
inline one-liner in `IAnimReader.hpp`, so `mA->VClone()` inlines the dispatch and costs 52 differing
instructions. A file-local `clone_reader(const IAnimReader&)` wrapper reproduces the call
(**86.86% -> 94.13%**).

Making `IAnimReader::Clone`/`Simplified` out-of-line in `IAnimReader.cpp` also reaches 94.13% but
**fails to link**: the weak symbol has to come from a linked object, and `IAnimReader.cpp` is
`NonMatching` (`configure.py:898`), so its object is not in the link. Reverted; the wrapper keeps
the change inside this unit. Note the wrapper is a thin forward to `VClone`, not a stub - it does
the real virtual call.

## What is left, with the spellings already tried

- **`VSimplified` 94.33% / `VReverseSimplified` 92.80%**: the only difference left is the constant
  pool. Retail loads `lbl_8041E390@sda21` / `lbl_8041E394@sda21` (shared `.sdata2` floats at
  `0x8041E390`/`:394`, `symbols.txt:24981`/`:24982`); we load our own `@546@sda21` /
  `@585@sda21` from a unit-local `.rodata`. No `Matching` unit in this tree loads an `lbl_` float
  from `.sdata2` (checked all 368), so the pool is placed differently in this build and this is a
  link-layout property, not a source spelling. Two `lfs` + one branch each; the rest is byte-equal.
  `VSimplified` also has retail's `Simplified__11IAnimReaderFv` called out of line vs our inlined
  `mB->VSimplified()`; the `clone_reader` trick would apply, worth trying.
- **`AdvanceViewForTransitionalPeriod` 97.58%**: `AdvanceViewBothChildren` is still a stub in
  `CAnimTreeDoubleChild.cpp:86`, so retail's `bl fn_802AA224` (the weak copy-ctor for
  `CDoubleChildAdvancementResult`) has no counterpart in our object. Blocked on that unit.
- **7-arg `__ct__` 88.41%**: the constructor now calls an out-of-line `GetLoopPOIHash` like retail
  (which inlines the function-local static's guard at 75.57%). Retail's is the unnamed
  `fn_802AA708`; a *file-static* function is what reproduces that (an out-of-line static *member*
  is 88.41% too, but a member definition in the `.cpp` regressed `CreatePrimitiveName` from 100% to
  92.31% by shifting the string pool, so the member was removed from the header and the function
  made file-local). The residual is the two unnamed locals it references (`lbl_804198B8`,
  `lbl_804198BC`) plus one `.rodata` string.
- **`VClone` 94.13%**: after the wrapper, the only difference is the register order of the
  `GetBlendRoot()`/`mRunA`/`mLoopA`/`mInitialized` argument setup - retail loads `mInitialized`
  before `mRunA`, we load `mRunA` first. Prime 1's argument order is unchanged and matches. Likely
  register allocation, not a source difference.
- **`fn_802A9FD4` / `fn_802AA020` / `fn_802AA224` / `fn_802AA708` 0.00%**: unnamed in retail, so
  objdiff pairs them by our name and finds nothing. They are COMDAT weak copies
  (`__ct__rstl42pair<...>`, `__ct__CDoubleChildAdvancementResult`, ...) that mwldeppc discards;
  `tools/unit_fit.sh` lists 13 such extras, 1176 bytes. Not reachable by naming.

## Gates

All run at the end, on this tree:

- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (expected)
- `./tools/probe_sources.sh` -> `749 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)`
- `python3 tools/check_symbol_names.py` -> `checked 503 units; 0 declared names are missing from their object`
- `./tools/decomp_build.sh` -> `All: 30.37% fuzzy, 22.21% matched, 11.74% linked (9838 / 28465 functions)`, no FAILED
- all 86 RELs `cmp`-identical to `orig/G2ME01/files/RelProd/`, and all 86 sha1s in
  `config/G2ME01/config.yml` match the disc
- `python3 tools/report_diff.py build/report.pre_cre.json build/report.json` -> `no regression`
- `python3 tools/check_decl_order.py --unit Kyoto/Animation/CAnimTreeTransition.cpp` -> clean
- `config/G2ME01/splits.txt` untouched; `report.json` `total_functions` still **28465**

`tools/link_check.sh` prints `NOT LINKED` / `unchanged from baseline (250 undefined, 0 duplicates)` -
that is the pre-existing state of this worktree, not something this change caused.
`tools/check_docs_claims.py` flags only the two derived counts in the `docs/HANDOFF.md` state block
(`9834 -> 9838` matched, and the DOL sub-count), which the judge rewrites from the tree.

`tools/unit_fit.sh Kyoto/Animation/CAnimTreeTransition.cpp` reports 13 functions in ours but not
retail (1176 bytes) and `.sdata2` over by 16; that is the COMDAT set above and is not a flip
verdict. `flip_test.sh` was deliberately not run: this is a `progress` item and the unit stays
`NonMatching`.

## Pre-existing, not from this change

`tools/report_diff.py` also prints
`WORSE ForgottenObject/MetroidPrime/ScriptObjects/CScriptForgottenObject :: RenderInternal 95.18% -> 88.19%`.
Confirmed by `git stash` + rebuild: at HEAD, unmodified, that function already measures 88.19%, the
same as after this change. `build/report.pre_cre.json` (the recorded baseline) says 95.18%, so the
baseline report and the tree disagree independently of this item.

`NEW: rstl-math-const-ref | port | include/rstl/math.hpp | min_val/max_val take and return by value here but by const reference in Prime 1's rstl; the by-value form costs a copy and blocks VGetTimeRemaining in CAnimTreeTransition - fix the header and the callers that spell the template argument explicitly (VGetSteadyStateAnimInfo uses rstl::max_val< const CCharAnimTime& >, which stops deducing under a const& signature)`

`NEW: cdoublechild-advancebothchildren | match | Kyoto/Animation/CAnimTreeDoubleChild | AdvanceViewBothChildren is still a TODO stub returning a zero-time result; it is what stops AdvanceViewForTransitionalPeriod at 97.58% and CAnimTreeDoubleChild itself from flipping`
