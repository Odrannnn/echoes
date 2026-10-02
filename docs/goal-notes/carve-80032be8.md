# carve-80032be8 - `MetroidPrime/Factories/Carve80032BE8` is `Matching` and flipped

**Result: PASS.** `tools/goal_check.sh build/goal/item.json` exits 0 in
`wt-mp2-goal-L3`, last line `goal_check: PASS carve-80032be8`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12582 -> 12583   linked 5944 -> 5945
  ok    check_symbol_names.py
  ok    All:  35.44% fuzzy, 29.26% matched, 12.97% linked (12583 / 28465 functions)
  ok    flip_test MetroidPrime/Factories/Carve80032BE8.c: PASS, Object(Matching) in configure.py
```

`total_functions` is still **28465** after the `splits.txt` edit, and `build/report.json` now
has the unit at `fuzzy_match_percent 100.0`, `matched_functions 1 / total_functions 1`,
`matched_code 84 / 84`, `complete_units 1`.

## What the carve is

`.text 0x80032BE8..0x80032C3C`, `0x54` = 84 bytes, **1 function**, the whole of dtk's
`auto_03_80032BE8_text` (`build/G2ME01/asm/auto_03_80032BE8_text.s:4`, `symbols.txt:973`):

```
fn_80032BE8  0x80032BE8  0x54  21 instructions
```

The item's reason was correct and was re-measured before acting, not taken on trust.
`fn_80032BE8` is byte-identical to the four matched by `carve-80032a98` (`fn_80032A98`,
`fn_80032AEC`, `fn_80032B40`, `fn_80032B94`) apart from the two `bl` displacements, which are
address-relative. Measured by disassembling both ranges of `build/G2ME01/main.elf` and
comparing encodings: 21 instructions each, **2 differing**, and both are the `bl` to
`Free__7CMemoryFPCv` (0x802CE388) - `48 29 b8 cd` / `48 29 b8 bd` at 0x80032A98 versus
`48 29 b7 7d` / `48 29 b7 6d` at 0x80032BE8. Nothing else differs, so the body was **read off
the twin, not guessed**:

```c
void* fn_80032BE8(void* self, short flag) {
  if (self) {
    Free__7CMemoryFPCv(*(const void**)self);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
```

`lwz r3,0(r30)` + `bl Free__7CMemoryFPCv` is the member teardown with no null test on the
loaded pointer, which is `delete mPtr` for `rstl::single_ptr<T>` whose only member is `T* mPtr`
at +0 (`include/rstl/single_ptr.hpp:17`); `mr. r30,r3 / beq` guards the receiver and
`extsh. r0,r31 / ble` re-tests the second argument - MWCC's deleting-destructor convention.

**Which type it is, is measured too.** `fn_800324A4` (the `.ctors` entry at 0x803A54C0, still in
the unclaimed unit `auto_fn_800324A4_text`) loads `fn_80032BE8@ha`/`@l` into r4 and
`gpTweakAutoMapper@sda21` into r3 before `bl __register_global_object` at
`build/G2ME01/asm/auto_fn_800324A4_text.s:14-19`, so r4 is the destructor and r3 the object. That
global is a `rstl::single_ptr<CTweakAutoMapper>`
(`include/MetroidPrime/Tweaks/CTweakAutoMapper.hpp:96`). The item's reason said lines 12-22; the
`lis`/`addi` pair that carries the address is at 14-18 with the call at 19 - same fact, cited
more tightly here.

## Files (the carve is four)

| file | change |
| --- | --- |
| `src/MetroidPrime/Factories/Carve80032BE8.c` | new; definitions **descending by address**, plain C so the `fn_` name does not mangle |
| `configure.py:661` | `Object(Matching, "MetroidPrime/Factories/Carve80032BE8.c"),` one line, in address order between `Carve80032A98.c` and `Carve80045CD4.c` |
| `config/G2ME01/splits.txt:118-119` | `.text start:0x80032BE8 end:0x80032C3C`, in address order between `Carve80032A98.c` and `CGameProjectile.cpp` |
| `files.cmake:416` | `src/MetroidPrime/Factories/Carve80032BE8.c`, in address order between `Carve80032A98.c` and `Carve800358E0.c` |

The claim **spans no unclaimed gap** at either end, which was checked rather than assumed:
`Carve80032A98.c` ends at 0x80032BE8 and `MetroidPrime/Weapons/CGameProjectile.cpp` starts at
0x80032C3C, and 0x80032BE8 + 0x54 = 0x80032C3C exactly.

No `PortLinkStubs.cpp` duplicate had to be deleted: `grep -rn "fn_80032BE8" src/ include/` hits
only the comment in `Carve80032A98.c:57` that pointed at this carve, so nothing defined the name
anywhere else. The carve declares no retail data, only the one function it calls
(`Free__7CMemoryFPCv`, already defined for the host link by `src/Kyoto/Alloc/PortMwccNew.cpp:34`),
and the port link is unchanged.

The file's comment says `Free__7CMemoryFPCv` is **claimed** by `Kyoto/Alloc/CMemory.cpp`
(`.text` 0x802CE224..0x802CE72C, `splits.txt:2271`, `MatchingFor("G2ME01")` at `configure.py:910`),
so **our own** object supplies those bytes - which is the correction `carve-80032a98` recorded
about `Carve800E1548.c:73-77`. The stale sentence there is another lane's file and was left alone
(a doc fix is not a `NEW:` candidate); this file does not repeat it.

## Verification (every number measured in this run)

```
tools/carve_diff.sh 80032BE8 54 build/G2ME01/src/MetroidPrime/Factories/Carve80032BE8.o
    retail: 21 instructions, 84 bytes / ours: 21 instructions, 84 bytes
    differing instructions: 2   <- the two `bl`s, unlinked
    (+9 retail bl 802ce388 <Free__7CMemoryFPCv>  ours bl 24 <fn_80032BE8+0x24>)
    (+13 retail bl 802ce388 <Free__7CMemoryFPCv> ours bl 34 <fn_80032BE8+0x34>)
tools/unit_fit.sh MetroidPrime/Factories/Carve80032BE8.c
    .text claimed 84 ours 84 retail 84 fits; no extra functions
python3 tools/check_decl_order.py --unit main/MetroidPrime/Factories/Carve80032BE8
    ok: 1 unit(s) checked, none emits its functions out of retail order
./tools/decomp_build.sh
    All: 35.44% fuzzy, 29.26% matched, 12.97% linked (12583 / 28465 functions)
sha1sum build/G2ME01/main.dol
    6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py
    checked 528 units; 0 declared names are missing from their object
./tools/flip_test.sh MetroidPrime/Factories/Carve80032BE8.c
    PASS  -> kept as Matching      kept: 1 / 1   failed: 0   skipped: 0
```

`auto_03_80032BE8_text` is gone from the report after the split, and its neighbour
`auto_03_80032674_text` is **unchanged at 10 functions / 1060 bytes** - the claim did not shift
an unrelated unit. `main/MetroidPrime/Factories/Carve80032A98` still reads 100.0% with
4 / 4 matched.

`unit_fit` and `check_decl_order` were run *before* the build, per the prompt; with one function
in the unit the declaration-order hazard cannot bite, and `check_decl_order` says so explicitly.

## Caveats

- `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in `git status`: that is
  **`goal_check.sh`/`gate.sh` rewriting their own derived counts** (matched 12582 -> 12583,
  linked 5944 -> 5945, DOL units 11003 -> 11004), not a lane edit - `git diff docs/` is only
  those lines. Nothing in `docs/` was written by this run and no `tools/` file was touched.
- This lane's own mistake, recorded so it is not repeated: several `goal_check.sh` runs were
  started before an earlier one had finished, and the driver briefly moved the session's working
  directory into another lane's worktree (`wt-mp2-goal-L13`), so one judge invocation graded
  **L13's** item `carve-801fa780` instead of this one. The `PASS` quoted at the top is from a
  run whose own log shows the L3 item id, the L3 baseline path
  (`.../wt-mp2-goal-L3/build/goal/judge/report.base.json`) and this unit in the flip line. The
  stray L13 process was killed and L3 was rebuilt from clean before that final run; nothing in
  L13's tree was modified deliberately. **One judge run at a time, and check `pwd` first.**
- No `WALL:` line: nothing here sat below 100%.
- No `NEW:` filed. The next candidate in this neighbourhood is `fn_800324A4` (0x800324A4,
  0x1D0 = 464 bytes, 1 function, still in the unclaimed `auto_fn_800324A4_text`): it is the
  tweak-globals `.ctors` initialiser and would give **+1** for writing the ten
  `__register_global_object` calls with their `gpTweak*` arguments - but each of those calls
  needs the named global *and* a per-global name string, so it is a real item rather than a
  twin, and it is left for a lane to measure rather than filed on this one's authority.