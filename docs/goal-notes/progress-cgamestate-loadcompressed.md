# progress-cgamestate-loadcompressed

`kind: progress` - `MetroidPrime/Player/CGameState`, stays `NonMatching`. No carve, no
`configure.py` change, no `config/` change, no `asm`, nothing committed.

## Result

`LoadCompressedMultiplayerOptions__10CGameStateFv` (retail `0x80142A30`, 140 B) and
`LoadCompressedGameOptions__10CGameStateFi` (retail `0x80142ABC`, 148 B) are both **100.00%**
and now count as exact matches in `build/report.json`.

| | before | after |
| --- | --- | --- |
| unit `matched_functions` | 92 / 116 | **94 / 116** |
| unit `matched_code` | 9772 / 18236 | **10060 / 18236** |
| unit `fuzzy_match_percent` | 81.61834 | **83.19763** |
| global `All:` matched | 9908 / 28465 | **9910 / 28465** |

`matched_code` rose by exactly **288 = 140 + 148**, and `total_code` stayed at 18236 with every
section size unchanged (`.ctors` 4, `.rodata` 456, `.sdata` 12, `.sdata2` 32, `.text` 18236).
That is the evidence that no other function in the unit moved: the delta is the two new
functions and nothing else.

## The diff

One file, additive only: `src/MetroidPrime/Player/CGameState.cpp` (+1 include, +2 definitions,
+1 POD mirror struct, +3 `extern "C"` declarations, +comment).

Retail's two functions are one body differing by **two instructions** in how the block address
is formed. The multiplayer block at `+0x178` is read straight off `this`
(`lwz r4,388(r31)` / `lwz r5,384(r31)`, i.e. `+0x0C` and `+0x08`); `mCompressedGameOptions` is a
three-element array of 16-byte elements at `+0x148`, so `LoadCompressedGameOptions` scales the
slot first (`slwi r0,r4,4` / `add r5,r31,r0`, then `+0x0C` / `+0x08` off that). 148 - 140 = 8 =
those two instructions. Everything after is identical.

## The one non-obvious spelling: the length is `capacity()`, not `size()`

Both functions pass the block's `+0x08` word as `CMemoryInStream`'s length. In
`include/rstl/vector.hpp` the members are `mAllocator / mCount / mCapacity / mItems`, so `+0x04`
is the **count** and `+0x08` the **capacity** - the same two words `SGameStateBlock` names
`x04_count` and `x08_cap` (`include/MetroidPrime/Player/CGameStateBlocks.hpp:45-50`). So the
second argument is `capacity()`. `CMemoryInStream`'s parameter is `unsigned long`
(`CMemoryInStream.hpp:13`), and `size()` returns an `int` off `+0x04`, so `size()` reads a
different offset than retail's and will not match. The existing already-100% siblings cannot
distinguish the two (`CopyCompressedMultiplayerOptions` and
`RecordCompressedMultiplayerOptions` both pass the literal `0x20`), which is why this is worth
writing down.

Corroboration from outside the unit: `src/MetroidPrime/CMemoryCardDriver.cpp:375,390` already
spells this idiom as `CMemoryInStream r(mSystemData.data(), mSystemData.capacity())`.

## The two blockers the earlier notes predicted, and how each was cleared

`progress-cgamestate-bodiless-runs` filed this item saying it needs "extern \"C\" declarations
AND a hand-destroyed stack temporary". Both are real, and both are now solved:

1. **`fn_80003D00` (copy assignment) and `fn_80004D84` (destructor) are unnamed and unclaimed.**
   Confirmed: they live in `main/auto_03_80003BE8_text` and `main/auto_03_80004D84_text`, i.e.
   dtk fills them from retail and no source file defines them. Both are nonetheless `T` globals
   in `build/G2ME01/main.elf` (`80003d00`, `80004d84`), so an `extern "C"` declaration links.
   This is the same arrangement `src/MetroidPrime/CMainResetGameState.cpp:281,293` already uses.

2. **`CGameOptions` has a *declared* destructor**, so a local of that type makes mwcceppc run
   `~CGameOptions()` at scope exit through the C++ name `__dt__12CGameOptionsFv` - a name
   `config/G2ME01/symbols.txt` does not carry, and which nothing in the matching build defines
   (`powerpc-eabi-nm` on `CGameOptions.o` returns nothing for it). The temporary is therefore a
   POD mirror, `SGameOptionsLoad { u8 x00[sizeof(CGameOptions)]; }` with `CHECK_SIZEOF(...,0x44)`,
   destroyed by hand. This is the `SGameOptionsCopy` trick from `CMainResetGameState.cpp:300-306`,
   repeated here because both units need it.

   **The constructor does not need the same treatment**, and that is the asymmetry that makes
   this writable at all: `__ct__12CGameOptionsFR16CBitStreamReader` (`0x80161828`, `0x320`) *is*
   named in the symbol table and is already 100% matched in `src/MetroidPrime/Player/CGameOptions.cpp`.
   It is declared `extern "C"` under its own MWCC name and called on the mirror - the arrangement
   `src/MetroidPrime/Player/CGameStateCtor.cpp:98` already uses for `__ct__12CGameOptionsFv`.
   So of the three retail callees, one is a named C++ symbol reachable directly and two are
   unnamed `fn_` symbols reached by hand.

`CMemoryInStream` and `CBitStreamReader` are ordinary locals of their own types. The six
instructions at `0x80142A90-0x80142AA4` (`lis`/`addi` of the `CInputStream` vtable, `addi` of the
object, `li r4,0`, `stw` of the vtable at the object, `bl __dt__12CInputStreamFv`) are the
compiler's own base-class teardown, emitted for free because `CMemoryInStream`'s destructor is
declared `virtual ... override {}` (`CMemoryInStream.hpp:15`). They are not written by hand.

## Declaration order

`python3 tools/check_decl_order.py --unit MetroidPrime/Player/CGameState` -> `ok: 4 unit(s)
checked, none emits its functions out of retail order`. The two definitions sit between
`CopyCompressedMultiplayerOptions` (`0x80142B50`) and `fn_80142A10` (`0x80142A10`), which is
descending by retail offset, as mwcceppc requires.

## Verification (all re-run after the final edit)

```
sha1sum build/G2ME01/main.dol                  6ef9b491d0cc08bc81a124fdedb8bfaec34d0010  (expected)
./tools/probe_sources.sh                       749 files, 0 failed, 0 errors;
                                               link: LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py            503 units, 0 declared names missing
./tools/decomp_build.sh                        All: 30.50% fuzzy, 22.57% matched,
                                               11.74% linked (9910 / 28465 functions)
86 x orig/G2ME01/files/RelProd/*.rel           cmp-equal, 0 differ
python3 tools/check_decl_order.py --unit ...   ok
```

`tools/unit_fit.sh MetroidPrime/Player/CGameState.cpp` reports 98 functions present in ours but
not in the retail unit object (10936 bytes). These are pre-existing `rstl` template and COMDAT
instantiations (`reserve__Q24rstl37vector<Uc,...>Fi`, `__ct__Q24rstl...`, the red-black-tree
`internal_compare`, ...) and are **not** from this change: the unit's `.text` is still exactly
retail's 18236 bytes and `matched_code` grew by exactly the two new functions' 288 bytes, so
nothing was appended. This unit is not being flipped, so `unit_fit` is not a gate here; it is
recorded because the next lane that tries to flip this unit will hit it.

## Two things the next lane should know

**`build.ninja` in this worktree was stale and inconsistent, and I had to repair it.** The
`main.elf` link statement pulls **1727** objects from `build/G2ME01/obj/` (dtk's retail split,
which is what `flip_test.sh`'s header describes: "ninja links the DOL from
build/G2ME01/obj/*.o, the objects dtk split out of retail") and only **724** from
`build/G2ME01/src/`. The compile rules, however, emit **only** `src/` paths, so 1330 of the
linked `obj/` paths have no build rule at all - ninja tolerates them purely because the files
happen to exist from an earlier run. `build/G2ME01/obj/MetroidPrime/Player/CGameState.o` is
objdiff's `target_path` for this unit, i.e. **retail's own object**, and because the unit is
`NonMatching` that is the object actually linked, which is why the DOL sha1 is unaffected by any
edit to this source. I deleted that file while chasing a stale artifact and the build stopped
dead; it is regenerated with

```sh
build/tools/dtk dol split config/G2ME01/config.yml build/G2ME01
```

(`build.ninja:27156-27162`, the `split` rule that produces `build/G2ME01/config.json`). Worth
knowing before anyone concludes that a green DOL sha1 says anything about this unit: **it does
not**, until the unit is flipped. That is the `PROCESS_LESSONS.md` failure in its plainest form
and it is the whole reason this item is a `progress` item.

**A stale `build/G2ME01/obj/.../CGameState.o` reads exactly like a finished answer and is not
one.** I disassembled it early on, found `LoadCompressed*Options` fully present at their retail
offsets, and briefly took it for a previous lane's merged solution. It is dtk's retail object.
`objdiff.json` makes the distinction explicit and is the file to check first next time:
`base_path` is what we compile, `target_path` is retail.

## Not done / not filed

- The unit is `NonMatching` and stays that way; 22 functions remain below 100%, untouched.
- `python3 tools/check_docs_claims.py` now reports the `docs/HANDOFF.md` state block stale
  (`9910 / 28465` and the DOL-unit figure). Left alone deliberately - the brief says the driver
  rewrites the derived counts and discards edits to those files.
- No `NEW:` lines. The remaining bodiless functions in this unit are already enumerated in
  `docs/goal-notes/progress-cgamestate-memcpy.md:138-139` and in
  `build/goal/notes/triaged-2026-09-29.md`; re-filing them would duplicate queue entries rather
  than raise a count.
