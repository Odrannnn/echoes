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

---

# Second run (lane 3, 2026-09-30) - 9/18 -> 12/18

Re-measured on a clean HEAD first: the unit was exactly where the first run left it, 9/18,
88.34% fuzzy, 46.56% matched code. So nothing was `STALE:`. Three functions reached 100%,
one of them by an idea the first run wrote down and never tried.

`main/Kyoto/Animation/CAnimTreeTransition`: **9/18 -> 12/18** functions at 100%, matched code
46.56% -> 66.60%, fuzzy 88.34% -> 91.30%.
`All:` 31.30% fuzzy / 23.67% -> 23.68% matched / 11.83% linked; matched functions
**10308 -> 10311**, linked 5048 -> 5048. One file touched:
`src/Kyoto/Animation/CAnimTreeTransition.cpp` (the previous run's header workaround
`max_val_in_place` is untouched and still needed).

| function | before | after | what changed |
| --- | --- | --- | --- |
| `VSimplified` | 94.33% | **100%** | `mB->VSimplified()` / `mB->VClone()` now go through file-local `simplified_reader` / `clone_reader` instead of an inlined vtable dispatch |
| `VReverseSimplified` | 92.80% | **100%** | `mA->VClone()` now goes through `clone_reader` |
| `fn_802AA708` | 0.00% | **100%** | the file-local `"Loop"` POI-hash helper renamed `GetLoopPOIHash` -> `extern "C" fn_802AA708` (see below) |
| 7-arg `__ct__` | 88.41% | 88.41% | unchanged - measured wall, see below |
| `AdvanceViewForTransitionalPeriod` | 97.58% | 97.58% | unchanged - the previous run's blocker (`AdvanceViewBothChildren` stub) is gone; the residue is elsewhere |
| `VClone` | 94.13% | 94.13% | unchanged - register allocation only |

## The three that moved

### Retail calls `IAnimReader::Simplified` / `Clone` out of line, so both wrappers are needed

The first run added `clone_reader` for `VClone` only and wrote "`VSimplified` also has retail's
`Simplified__11IAnimReaderFv` called out of line vs our inlined `mB->VSimplified()`; the
`clone_reader` trick would apply, worth trying." It is exactly that, plus the *second* wrapper:

- `VSimplified` emitted `lwz r12,0(r4); lwz r12,88(r12); mtctr; bctrl` where retail has
  `bl Simplified__11IAnimReaderFv`, and again `lwz r12,84(r12)` where retail has
  `bl Clone__11IAnimReaderCFv`. Two file-local wrappers (`simplified_reader(IAnimReader&)`,
  `clone_reader(const IAnimReader&)`) turn each four-instruction dispatch into the one
  instruction retail has: **133 -> 127 instructions, 94.33% -> 99.84%** (report: 100.0).
- `VReverseSimplified` had the same `mA->VClone()` inline dispatch: 53 -> 50 instructions,
  92.80% -> 99.70% (report: 100.0). It is 50/50 exactly and byte-equal after the wrapper.

Both wrappers are real forwarders to the virtuals, not stubs. Neither can be folded into
`IAnimReader` itself: `IAnimReader.cpp` is `NonMatching` (`configure.py:898`), so its object is
not in the link, which is what the first run measured. So they stay in this `.cpp`.

### objdiff pairs by **name**, so retail's unnamed functions can be matched - `extern "C"` is the lever

`fn_802AA708` is retail's name for the guarded-local-static POI-hash helper. It sat at 0.00%
purely because ours was called `GetLoopPOIHash`: identical 72 bytes, no pairing. Declaring it

```cpp
extern "C" uint fn_802AA708() { static uint hash = CPOINode::GetHashForString("Loop"); return hash; }
```

gives the symbol the unmangled name and objdiff pairs it: **0.00% -> 100%**, 12/18. `static` does
*not* work - a static function is still mangled (`fn_802AA708__Fv`) and pairs with nothing.
This is the tree's existing convention for retail functions with no recoverable C++ name; see
`src/MetroidPrime/CModelDataModelSlots.cpp:103,133` (`extern "C" fn_800E4E9C`, `fn_800E4E50`) and
`src/MetroidPrime/CStateManagerScriptMsgArrayCursor.cpp:46`. Nothing else in `src/` or `include/`
defines `fn_802AA708`, so the new global C symbol does not collide in the port link
(`tools/probe_sources.sh`: `LINKED (250 undefined, 0 duplicates)`, same 250 as before).

This does **not** move the 7-arg `__ct__` - see the wall below - because the remaining diff there
is not the call.

## What is left, measured this run

`objdiff-cli --no-color diff -p . -u main/Kyoto/Animation/CAnimTreeTransition --format
json-pretty -o out <symbol>` prints objdiff's per-instruction verdict (`DIFF_ARG_MISMATCH`,
`DIFF_INSERT`, `DIFF_DELETE`, `DIFF_REPLACE`). The unit name is the one in `objdiff.json`
(`main/...`), **not** the `src/...` path - with the path form it answers "Failed: Either target
and base or project and unit must be specified". Two things this shows that guessing from a
disassembly does not:

1. **`DIFF_ARG_MISMATCH` is charged for a relocation's *symbol name*.** `lfs f1, @659@sda21`
   against retail's `lfs f1, lbl_8041E394@sda21` is a mismatch even though both hold `0.0f`, and
   so is `bl clone_reader__FRC11IAnimReader` against `bl Clone__11IAnimReaderCFv`. But
   `report.json`'s `fuzzy_match_percent` charges much less than `diff`'s `match_percent`:
   `fn_802AA708` is 99.17 in the diff view and **100.0** in the report, and it still counts as a
   matched function. So `diff` is the tool for finding *what* differs and `report.json` is the
   score that counts; do not read `diff`'s percentage as the number in the notes.
2. **Our unit-local `.sdata2` float pool cannot be made to match.** Retail's constants are
   `lbl_8041E390` / `lbl_8041E394` / `lbl_8041E39C` in *another* unit's `.sdata2`; ours are
   `@560` / `@659` / `@743` in this unit's (`.sdata2` 24 bytes here vs 8 in the retail object).
   That is a link-layout property of the build, as the first run said, and it is the last thing
   standing between the 7-arg `__ct__` (after the wall below) and 100%.

### The 7-arg `__ct__`, 88.41%: one hoisted load, no honest spelling avoids it

Everything before `stb r31,60(r28)` is byte-identical. After it:

```
retail  stb r31,60(r28); bl fn_802AA708; mr r4,r3; lwz r3,0(r29); lwz r12,0(r3); lwz r12,56(r12)
ours    stb r31,60(r28); lwz r31,0(r29); bl fn_802AA708; lwz r12,0(r31); mr r4,r3; mr r3,r31
```

MW hoists the `*a` load into the callee-saved register r31 (freed by the `stb` just above)
*before* the hash call; retail loads `*a` straight into r3 *after* it, and only needs `r29`,
which already holds `a`. The rest of the function is identical. **45 instructions against
retail's 44.**

Spellings tried this run, each rebuilt and scored, **all 88.41%**:

| spelling | score |
| --- | --- |
| `a->VGetBoolPOIState(fn_802AA708())` (the current one, and the previous run's `GetLoopPOIHash()`) | 88.41% |
| `a.GetPtr()->VGetBoolPOIState(...)` | 88.41% |
| `(*a).VGetBoolPOIState(...)` | 88.41% |
| `((CAnimTreeNode*)a.GetPtr())->VGetBoolPOIState(...)` | 88.41% |
| `((const IAnimReader&)*a).VGetBoolPOIState(...)` | 88.41% |
| `mLoopA` moved to the ctor body (`mRunA` stays `const`, so the store order becomes 60,62,61) | 81.82% |
| `mLoopA` **and** `mRunA` moved to the body in retail's order 60,61,62, with `mRunA` de-`const`ed in the header | 80.80% |

Forcing the hash call first with a comma expression that passes a **wrong** hash -
`mLoopA((fn_802AA708(), a->VGetBoolPOIState(0)))` - reaches **95.45%** (42/44), and objdiff then
reports exactly one remaining diff, the `lfs f1` constant-pool name. That is the proof that the
hoist is the whole gap. **Do not use the comma form**: it passes `0` instead of the hash, which is
exactly the "plausible-looking stand-in" the brief forbids. It was a measurement, not a candidate,
and it is not in the tree.

### `AdvanceViewForTransitionalPeriod`, 97.58%: the first run's blocker is gone, the rest is not

`CAnimTreeDoubleChild` is now 18/18 at 100% and `AdvanceViewBothChildren` is real, so
`NEW: cdoublechild-advancebothchildren` is closed. AVFTP did not move, because nothing about
`AdvanceViewBothChildren` was ever the remaining diff. objdiff lists:

- `bl __ct__Q220CAnimTreeDoubleChild29CDoubleChildAdvancementResultFRC...` vs retail `bl fn_802AA224`
- `bl __ct__Q24rstl42pair<...>` (x2) vs retail `bl fn_802A9FD4`
- `bl Interpolate__18SAdvancementDeltas...` vs retail `bl fn_802A04F4` (a different unit; Echoes
  does not name it, Prime 1 does)
- `lwz/stw sAdvancementDepth__18CAnimTreeTweenBase@sda21` (x4) vs retail's unnamed `lbl_804198*`
- `lfs f0, @126@sda21`, `lwz r0, @125@sda21`, `lfs f0, @704@sda21`
- and one real instruction: `addi r4,r1,0x88` where retail has `mr r4,r30`

The first four are all "retail's symbol is unnamed here". `fn_802AA224` and `fn_802A9FD4` are
*this* unit's functions and the `extern "C"` trick above would work on them - but they are
compiler-generated COMDAT copy constructors (`rstl::pair`, `CDoubleChildAdvancementResult`) with
no source name to give, so no spelling reaches them. Same for `fn_802A9FD4`'s role at
`AdvanceViewForTransitionalPeriod`'s two `rstl::pair` returns. **97.58% is this function's
ceiling from source.** The `addi r4,r1,0x88` is register allocation: both objects set
`r30 = r1+136` at the same place and retail reuses it, ours rematerialises the address twice.

### `VClone`, 94.13%: callee-saved register assignment

Retail allocates `this`->r31, `that`->r25, new->r27, flags r30/r29/r28/r26. Ours allocates
`this`->r30, `that`->r31, new->r26, flags r29/r28/r27/r25. 100 instructions each; the only other
difference is where the `lbz` of offsets 0x3C/0x3D/0x3E land. Every instruction is the same
operation with the same offset, so this is MW's allocator, not the source. Nothing was tried
beyond the existing wrapper.

## Gates, all run at the end on this tree

- `./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS progress-prime1-canimtreetransition`**,
  exit 0: gate.sh ok, counts `matched 10308 -> 10311, linked 5048 -> 5048`, symbol names ok,
  `All: 31.30% fuzzy, 23.68% matched, 11.83% linked (10311 / 28465 functions)`,
  `target rose: main/Kyoto/Animation/CAnimTreeTransition: 9 -> 12 / 18 functions`, no asm added.
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (expected)
- `./tools/probe_sources.sh` -> `752 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)`
- `python3 tools/check_symbol_names.py` -> `checked 505 units; 0 declared names are missing from their object`
- `python3 tools/report_diff.py <baseline> build/report.json` -> `no regression`, and the only
  three lines are the three `+100%` functions above
- all 86 RELs `cmp`-identical to `orig/G2ME01/files/RelProd/` and all 86 `config.yml` sha1s
  match (that is inside `gate.sh`'s "hashes vs config.yml ok")
- `python3 tools/check_decl_order.py --unit Kyoto/Animation/CAnimTreeTransition.cpp` -> ok
- `tools/unit_fit.sh Kyoto/Animation/CAnimTreeTransition.cpp` -> 13 functions in ours but not
  retail, 1160 bytes (was 1176), `.sdata2` over by 16, `.sbss` short by 3. Not a flip verdict.
- `config/G2ME01/splits.txt` untouched; `report.json` `total_functions` still **28465**
- `flip_test.sh` deliberately not run: `kind: progress`, the unit stays `NonMatching`.

`tools/gate.sh` run by hand (without the judge's `MP_GATE_DOCS_WRITE=1`) reports
`GATE FAIL: docs`, naming only the two derived counts in the `docs/HANDOFF.md` state block
(`10311 / 28465` and `DOL units 8763 / 16726`). The judge rewrites those from the tree; that edit
was reverted so this run's diff is `src/Kyoto/Animation/CAnimTreeTransition.cpp` alone.

Also checked, and *not* stale: the first run recorded that
`CScriptForgottenObject :: RenderInternal` measures 88.19% at HEAD while
`build/report.pre_cre.json` says 95.18%. This run's `report_diff.py` against the lane's own
baseline reports no regression anywhere, so that disagreement is not reproducing here.

WALL: __ct__19CAnimTreeTransitionFbRCQ24rstl25ncrc_ptr<13CAnimTreeNode>RCQ24rstl25ncrc_ptr<13CAnimTreeNode>RC13CCharAnimTimebiRCQ24rstl66basic_string 88.41% - MW hoists the `*a` load above the `bl fn_802AA708`; nine spellings of the mLoopA initialiser and the two ctor-body forms all score 88.41% or worse, and only a comma expression that passes the wrong hash moves it, so what is left is the register allocator

---

# Third run (lane 9, 2026-10-02) - 12/18 -> 13/18

Re-measured on clean HEAD b127c5ae: 12/18, 7-arg `__ct__` 88.41%, AVFTP 97.58%, VClone 94.13%.
Not stale.

**The 7-arg `__ct__` is now 100%** (supersedes the previous run's `WALL:` - that wall was wrong
about the spellings; it is not register allocation). The hoist of `*a` above `bl fn_802AA708` goes
away when the call is an argument to a small inline helper taking the node pointer and the hash:

```cpp
static inline bool loop_state(const rstl::ncrc_ptr< CAnimTreeNode >& a, uint hash) {
  return a->VGetBoolPOIState(hash);
}
... , mLoopA(loop_state(a, fn_802AA708()))
```

The hash call is evaluated first and the `*a` load happens inside the inlined body, as in retail.
Only `src/Kyoto/Animation/CAnimTreeTransition.cpp` touched.

Tried and rejected this run: in `VClone`, copying `mInitialized` to a local before the `rs_new`
(94.13% -> 86.75%, reverted). AVFTP and VClone unchanged.

`./tools/goal_check.sh build/goal/item.json` -> `PASS`, `target rose: 12 -> 13 / 18`,
matched 12412 -> 12413, no asm added.
