# progress-cgamestate-dtor-80143b94

`kind: progress` - `MetroidPrime/Player/CGameState`, stays `NonMatching`. Lane 5, `wt-mp2-goal-L5`,
branch `goal/lane-5`. No carve, no `configure.py` or `config/` change, no `asm`, nothing committed.

## Result: PASS. The target rose 102 -> 103, and the destructor is byte-exact

```
./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10065 -> 10067   linked 4918 -> 4918
  ok    check_symbol_names.py
  ok    All:  31.01% fuzzy, 23.30% matched, 11.78% linked (10067 / 28465 functions)
  ok    target rose: main/MetroidPrime/Player/CGameState: 102 -> 103 / 116 functions
  ok    no asm added
  goal_check: PASS progress-cgamestate-dtor-80143b94

sha1sum build/G2ME01/main.dol            6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh                 750 files, 0 failed, 0 errors; LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py      504 units, 0 declared names are missing from their object
python3 tools/check_decl_order.py --unit MetroidPrime/Player/CGameState   ok
git diff --check                         clean
```

Per-function diff of `build/report.json` against the snapshot taken before either edit (not the
judge's baseline file, so this is my own arithmetic and not the judge's):

* `__dt__11CGMFrontEndFv` **83.00% -> 100.00%**, size 180 -> 180
* `__dt__14CGMMultiplayerFv` (CGMDeathMatch) **87.33% -> 100.00%**, size 120 -> 120
* **regressions: 0.** Global `matched_functions` 10065 -> 10067; `linked` 4918 -> 4918 unchanged.

The judge's `gate.sh` rewrites the derived counts in `docs/HANDOFF.md`; per the item prompt I
restored that file each time, so the diff is only the two source files.

## What changed, and the one non-obvious part

**1. `src/MetroidPrime/Player/CGameState.cpp` - `CGMFrontEnd::~CGMFrontEnd() {}`,** declared in
CGMFrontEnd.hpp and defined in this unit, placed between `CGMFrontEnd(const CGMFrontEnd&)`
(0x80143C40) and `StartGameFromFrontEnd` (0x80143D28) so the unit's definitions stay descending by
retail offset; `check_decl_order.py` confirms it.

The body is empty and that is the whole point. Retail's 180 bytes are the member teardown and
nothing else, and **the object already contained the loop**: the standalone weak
`__dt__Q24rstl49reserved_vector<Q211CGMFrontEnd13SPlayerConfig,4>Fv` at object offset 0x29b8 is
instruction-for-instruction the block retail inlines at 0x80143BC0-0x80143C08 (verified by
disassembly, both sides). Adding the definition took it from `U __dt__11CGMFrontEndFv` (undefined,
0.00%) to **83.00% on the first try**.

**2. `include/MetroidPrime/Player/CGameMode.hpp:13` - `virtual ~CGameMode();` ->
`virtual ~CGameMode() {}`.** This is the part that is not obvious, and without it the function sits
at 83.00% and cannot be finished. `~CGameMode` was declared and defined nowhere in the port, so it
was `U` in all four objects that need it; mwcceppc emitted the base teardown as an out-of-line
`mr r3,r30 / li r4,0 / bl __dt__9CGameModeFv` and had to park the D0 flag in r31 (`mr r31,r4` in
the prologue, `extsh. r0,r31` at the tail) and `this` in r30, which cost three extra
prologue/epilogue instructions and swapped every register against retail. Retail's own
`__dt__9CGameModeFv` (0x80004798, 0x48 bytes, read out of the DOL) is the null guard, the
`__vt__7CGameMode` store and the D0 test - and `__dt__11CGMFrontEndFv` has that **inlined**, which
is only possible if the base destructor is visible to the translation unit. Declared `{}` (in-class,
therefore inline), it is, and the result is byte-identical to retail apart from the two vtable
relocations objdiff ignores.

This follows the repo's existing convention rather than inventing one: `CGameState.cpp`'s own note
on `LoadCompressedGameOptions` records that `CMemoryInStream` is "`a virtual class whose destructor
is declared `{}`" and that its teardown "is the compiler's own base-class teardown".

## The 83.00% -> 100.00% difference, exactly

Two edits, one measurement each:

| | object bytes | score |
| --- | --- | --- |
| no definition at all | `U __dt__11CGMFrontEndFv` | 0.00% |
| `~CGMFrontEnd() {}`, `~CGameMode` declared only | 0xB4 = 180, **45/46 instructions are retail's, in retail's order** | 83.00% |
| `~CGMFrontEnd() {}`, `~CGameMode() {}` | 0xB4 = 180, 46/46 | **100.00%** |

The 8 differing instructions at 83% were all consequences of the one `bl`: `mr r31,r4`,
`stw r30,8(r1)`, `mr. r30,r3` in the prologue (retail: `mr. r31,r3`), the five-instruction base
teardown becoming three, `extsh. r0,r31` becoming `extsh. r0,r4`, and `lwz r30,8(r1)` in the
epilogue. There was no register-allocation puzzle here at all once the base destructor was visible;
the earlier note's "this is a shared-header question" was right, and the answer turned out to be
one line rather than a 20-user audit.

## Cost, stated plainly

Inlining `~CGameMode` makes mwcceppc emit a **weak COMDAT copy of `__dt__9CGameModeFv` (0x48) and
`__vt__9CGameMode` (0x68) into every object whose class derives from `CGameMode` and needs a
destructor** - the four here (`CGameState.o`, `CGMDeathMatch.o`, `CGMMultiplayer.o`, `CGMCoin.o`).
`CGameState.o`'s `.data` goes 12 -> 220 bytes and `unit_fit.sh`'s "present in ours but not in the
retail unit object" count goes 96 -> 97 (10792 -> 10864 bytes), which is the
`CAi carries 224 bytes of these and still flips` case that tool's own output describes.

The one measurable cost: **`main/MetroidPrime/Player/CGMCoin` `matched_data` 144 -> None.**
`CGMCoin.o`'s `.data` grows 0x8C -> 0xF4 because the weak `__vt__9CGameMode` lands in it, and
objdiff stops counting CGMCoin's `.data` as matched. **No function regressed anywhere** (verified
per-function over the whole report, 0 regressions) and CGMCoin's own fuzzy and matched-function
counts are unchanged at 77.03% and 8/16. It is a data-section accounting effect on a unit that is
`NonMatching` and 8/16 anyway. I could not avoid it: any spelling that inlines the base teardown
also emits the base vtable, and `CGMCoin.o` needs the teardown because it has `~CGMCoin`.

## Also established, so nobody re-derives it

- **`build/G2ME01/main.elf` cannot answer a question about a claimed unit.** It is *our* build;
  `tools/dis.sh` is retail for code only in **unclaimed** `.text`. Every byte of `__dt__11CGMFrontEndFv`
  had to come out of the DOL. `.tmp/opencode/raw.py` wraps a `dol_read` byte range with
  `powerpc-eabi-objcopy -I binary -O elf32-powerpc -B powerpc:common --rename-section
  .data=.text,code,alloc,load,readonly,data,contents` and re-anchors the addresses; the trailing
  `,data,contents` is what makes the section disassemble. **The system's `objcopy` does not know
  `powerpc:common`** - use `build/binutils/powerpc-eabi-objcopy` or the command fails.
- **The whole `reserved_vector` element loop is already right.** The header was not the problem and
  the earlier `NEW: progress-cgamestate-reserved-vector-default-ctor` is unaffected by this item.
  `__ct__11CGMFrontEndFRC11CGMFrontEnd` is still 88.86% and still needs the `mCount(0)` change that
  costs `SPreviousGameResults` and `__ct__10CGameStateFv`; that audit is still outstanding.
- **22 bodiless `__dt__*Fv` remain in port-owned units** (measured, from `build/report.json`), led by
  `__dt__Q29CGameArea16CPostConstructedFv` (776 B, `MetroidPrime/CGameArea`, unit 252/305) and
  `__dt__18CAnimTreeTimeScaleFv` (160 B, `Kyoto/Animation/CAnimTreeBlend`, unit 5/10). **34
  partially-matching ones** exist too, four of them in one unit at exactly 83.3871% -
  `__dt__18CBSFlyerLocomotionFv`, `__dt__20CBSBlendedLocomotionFv`, `__dt__23CBSWallWalkerLocomotionFv`,
  `__dt__25CBSAiMovedFlyerLocomotionFv` in `MetroidPrime/BodyState/CBSLocomotion`. Four identical
  scores is a base-class teardown, not four coincidences - see the `NEW:` line.

## Files touched

- `src/MetroidPrime/Player/CGameState.cpp` +15/-0 (the definition and its comment)
- `include/MetroidPrime/Player/CGameMode.hpp` +8/-1 (the inline destructor and its comment)

Nothing else. `docs/HANDOFF.md` was rewritten by the judge's gate each run and restored each time.
The scratch harness is `.tmp/opencode/raw.py` (gitignored); `.tmp/opencode/base_report.json` is the
before-snapshot every number in this file is diffed against.

## New queue items

NEW: progress-cbslocomotion-dtors | progress | MetroidPrime/BodyState/CBSLocomotion | four deleting destructors - `__dt__18CBSFlyerLocomotionFv`, `__dt__20CBSBlendedLocomotionFv`, `__dt__23CBSWallWalkerLocomotionFv`, `__dt__25CBSAiMovedFlyerLocomotionFv` - all sit at *exactly* 83.3871% of 124 bytes, which is the signature of one shared base-class teardown being emitted out of line instead of inlined: `include/MetroidPrime/BodyState/CBodyState.hpp:12` declares `virtual ~CBodyState() = 0;` with no definition anywhere, so mwcceppc emits `bl __dt__13CBodyStateFv` and has to keep the D0 flag and `this` in callee-saved registers around it; giving the base destructor an inline `{}` body is what took `__dt__11CGMFrontEndFv` from 83.00% to byte-exact on this date (see `docs/goal-notes/progress-cgamestate-dtor-80143b94.md`) - four functions in one unit at one score, so measure the base first