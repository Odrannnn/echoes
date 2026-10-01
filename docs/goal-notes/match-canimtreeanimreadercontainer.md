# match-canimtreeanimreadercontainer - DONE (full flip)

`Kyoto/Animation/CAnimTreeAnimReaderContainer.cpp` is `Matching` and
`tools/flip_test.sh Kyoto/Animation/CAnimTreeAnimReaderContainer.cpp` passes.
`tools/goal_check.sh build/goal/item.json` prints `goal_check: PASS`.

## What was measured first

`build/report.json` on the clean tree:

```
main/Kyoto/Animation/CAnimTreeAnimReaderContainer
  fuzzy 90.019%, matched 88.12%, matched_functions 26 / 28
  fn_802A5F74                        0.00%   (address 0x802A5F74, size 0xA0)
  VGetWeightedReaders...Cf           79.80%
```

`total_functions` is 28465 and did not change.

## Three separate things were wrong; each had to be found before the flip could pass

### 1. `VGetWeightedReaders` 79.80% -> 100% (a real codegen fix)

`out.push_back(pair)` went through `rstl::construct` -> `construct_impl` ->
`new (dest) T(src)`, because `pair<float, IAnimReader*>` is not one of the types
`include/rstl/construct.hpp` declares trivially constructible. mwcceppc emits placement
new's null guard for that, and the guard is two instructions retail does not have:

```
retail .text+0x9c                        ours before
  lwz  r0,0(r4)                            lwz  r0,0(r4)
  lwz  r5,20(r3)                           lwz  r5,20(r3)
  slwi r0,r0,3                             slwi r0,r0,3
  add  r3,r4,r0                            add  r3,r4,r0
  stfs f1,4(r3)                            addic. r3,r3,4     <- ours
  stw  r5,8(r3)                            beq  bc            <- ours
  lwz  r3,0(r4)                            stfs f1,0(r3)
  addi r0,r3,1                             stw  r5,4(r3)
  stw  r0,0(r4)                            lwz  r3,0(r4)
  blr                                     addi r0,r3,1
                                          stw  r0,0(r4)
                                          blr
```

Spelled as the two statements `push_back` is - `out.data()[out.size()] = pair(...)`
then `++out.mCount` (`mCount` is public for the reason the header gives) - the object is
byte-identical to retail's 40 bytes. Verified with a per-function byte compare of
`build/G2ME01/obj/Kyoto/Animation/CAnimTreeAnimReaderContainer.o` (retail) against
`build/G2ME01/src/Kyoto/Animation/CAnimTreeAnimReaderContainer.o` (ours): **27/28 retail
functions byte-identical** before the rename in (2), 28/28 after.

### 2. `fn_802A5F74` 0.00% -> 100% (a name, not code)

The bytes were already identical - it is the weak COMDAT copy mwcceppc emits for
`CAnimTreeEffectiveContribution`'s five-argument constructor (160 bytes, `symbols.txt`
carries no mangled name for that address, only dtk's `fn_802A5F74` placeholder, and
objdiff pairs functions **by name**, so it scored against nothing).

Identification, not a guess: it takes `(this, float, const rstl::string&, const
CSteadyStateAnimInfo&, const CCharAnimTime&, u32)` with the hidden return pointer in r3,
stores `mContributionWeight` at +0, copy-constructs `mName` at +4, then copies
`mSsInfo` (24 bytes), `mRemTime` (8) and `mDbIdx` (4) in member order - exactly the
struct's five members - and its only caller is `VGetContributionOfHighestInfluence`,
which passes `(1.f, mName, mReader->GetSteadyStateAnimInfo(), mReader->GetTimeRemaining(),
mAnimDbIdx)`, the constructor's own parameters.

Fixed with the repo's own tool, `tools/apply_rename.py` (one line of
`config/G2ME01/symbols.txt` replaced in place, not a rename beside it):

```
fn_802A5F74 = __ct__30CAnimTreeEffectiveContributionFfRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>RC20CSteadyStateAnimInfoRC13CCharAnimTimeUl
```

`symbols.txt` is not a ninja input, so `build/G2ME01/config.json` had to be removed to
make `dtk dol split` re-run; after that the whole build is incremental (~12 s).

### 3. The flip still failed - and **objdiff could not see why**

With all 28 functions byte-identical and objdiff reporting `100.00% fuzzy, 100.00%
matched`, `flip_test.sh` still FAILED: `main.dol` and 7 RELs differed, and the DOL was
**32 bytes longer** than retail's. Comparing the linked ELFs of the two builds
section by section showed only two differences, both invisible in the report:

| section | retail | ours | cause |
|---|---|---|---|
| `.sdata2` | 0x54c0 | 0x54c8 | our object emitted a local `1.0f` |
| `.rodata`  | same size | 1-byte shift from 0x803AEDF0 | our object emitted its own `"?(??"` |

* **`1.f` -> `lbl_8041E368`.** `VGetContributionOfHighestInfluence`'s `lfs f1,-16472(r2)`
  is `R_PPC_EMB_SDA21 lbl_8041E368` in the retail object (`.sdata2:0x8041E368`, `3f800000`,
  read back out of the linked ELF). Spelled as a literal, mwcceppc emits a 4-byte `@NNN`
  word in this object's own `.sdata2`, **no unit claims that range**, so the linked
  `.sdata2` grew by 8 bytes, `.sbss2`'s address moved with it, and every `bl`
  displacement after 0x802A5E00 in the DOL changed (4626 one-byte ranges in the diff).
  Declared and used by name, following `src/MetroidPrime/CConsoleOutputWindowCtor.cpp`.
* **`rs_new` -> `#define CMEMORY_NEW_FILE lbl_803AEDD8`.** `VClone`'s
  `R_PPC_ADDR16_HA`/`R_PPC_ADDR16_LO` pair resolves to `0x803AEDD8` (seven zero bytes).
  Unset, `rs_new` expands to `new ("\?\?(\?\?)", nullptr)`, mwcceppc puts that literal in
  this object's `@stringBase0`, the object grows a 7-byte `.rodata`, and mwldeppc appends
  it to the global string pool - landing our copy at 0x803AEDF0 instead of 0x803AEDD8.
  `include/Kyoto/Alloc/CMemory.hpp` documents this exact failure and names
  `src/MetroidPrime/Factories/CStateMachineFactory.cpp` as the precedent.

Both are **references**, declared and never defined; the bytes live in dtk's
`build/G2ME01/obj/auto_*` objects.

`lbl_8041E368` is guarded by `#if defined(__MWERKS__)`: `TARGET_PC` has no such symbol,
and naming it added one undefined symbol to the port's link (250 -> 251), which
`tools/link_check.sh --strict` reads as growth and `gate.sh`'s port-probe step fails on.
Measured: with the guard the port is back at **250 undefined, unchanged from baseline**.

### The lesson (worth more than the unit)

`push_back` aside, **both remaining failures were invisible in objdiff**: every retail
function's bytes were identical and the unit reported `100.00% fuzzy, 100.00% matched`,
and only `flip_test.sh` - plus a section-by-section ELF comparison - showed the object
contributing a `.rodata` and a `.sdata2` that retail's object does not. objdiff compares
*relocated bytes* and pairs *by name*; it does not check **which symbol a relocation
points at**, and it cannot see a section the target object does not have. Before
believing a 100%, check that the object's section list matches the retail object's:

```
build/binutils/powerpc-eabi-objdump -h build/G2ME01/src/<unit>.o
```

`tools/unit_fit.sh` already prints the giveaway - its last four lines listed
`.rodata 7`, `.sdata 36`, `.sbss 1`, `.sdata2 4` as "NOT CLAIMED BY splits.txt" and the
message was easy to skim past. A Matching unit that emits `.sdata`/`.sdata2`/`.rodata`
retail's object does not has to *claim* the range retail has it in, the way
`MetroidPrime/Player/CFidget.cpp` claims `.sdata2 0x8041D0C8..0x8041D0F0`; ours needed no
claim because retail's constant was already an auto range.

Our object still emits `.sdata 0x24` and `.sbss 1`, which retail's does not. They are
unreferenced locals and the flip passes with them, so they are recorded here rather than
chased.

## Verification

```
sha1sum build/G2ME01/main.dol      -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
all 86 RELs cmp -s vs orig/G2ME01/files/RelProd/   -> no difference
python3 tools/check_symbol_names.py -> checked 514 units; 0 declared names are missing
python3 tools/check_decl_order.py   -> 977 units checked, 31 permuted (unchanged)
./tools/link_check.sh --rebuild     -> 250 undefined, unchanged from baseline, 0 dups
./tools/flip_test.sh Kyoto/Animation/CAnimTreeAnimReaderContainer.cpp
                                    -> PASS  -> kept as Matching
./tools/goal_check.sh build/goal/item.json
  ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok  counts: matched 11424 -> 11426   linked 5537 -> 5565
  ok  check_symbol_names.py
  ok  All:  32.81% fuzzy, 25.61% matched, 12.06% linked (11426 / 28465 functions)
  ok  flip_test: PASS, Object(Matching) in configure.py
  goal_check: PASS match-canimtreeanimreadercontainer
```

`build/report.json`, unit now `complete: true`, 28/28 functions, 100% fuzzy and matched.
`All:` linked went 12.03% -> 12.06%; `complete_units` 741 -> 742.

## Files

- `src/Kyoto/Animation/CAnimTreeAnimReaderContainer.cpp` - the two named references,
  `CMEMORY_NEW_FILE`, the `push_back` spelling, and the comment on each.
- `config/G2ME01/symbols.txt:12048` - `fn_802A5F74` renamed in place.
- `configure.py:894` - `NonMatching` -> `Matching` (written by `flip_test.sh`).

No `splits.txt` change, no carve, no new source file, no asm. No `NEW:` item: the two
obstacles were both fixed here and neither leaves a unit behind that a lane could finish.