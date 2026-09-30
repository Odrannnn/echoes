# progress-rel-head-minoring

Goal item `progress-rel-head-minoring` (`kind: progress`, `target: module:MinorIng`).
Worktree `../wt-mp2-goal-L5`, branch `goal/lane-5`. Not committed - the driver commits.

## What landed

`MinorIng` (module 44) had no own-code unit: only `REL/global_destructor_chain.c` (2 functions)
and `REL/REL_Setup.cpp` (5). This adds the module's head, `.text 0x00000000..0x00000110`, as one
`Matching` unit, `src/MetroidPrime/ScriptObjects/CMinorIngRel.cpp`, and claims it in
`config/G2ME01/rels/MinorIng/splits.txt` plus a `Rel("MinorIng", ...)` in `configure.py`.

15 functions, all exact: `fn_44_0`, `fn_44_8`, `fn_44_10`, `fn_44_18`, `fn_44_28`, `fn_44_30`,
`fn_44_38`, `fn_44_48`, `fn_44_54`, `fn_44_60`, `fn_44_68`, `fn_44_70` (the twelve generated
accessors), `RELExit` (0x9C), `RELMain` (0xC0) and the loader registration `fn_44_E0` (0xE0).

## Measured

| what | before | after |
| --- | --- | --- |
| `module:MinorIng` matched functions (judge) | 7 / 217 | **22 / 217** |
| project matched | 10150 / 28465 | **10165 / 28465** (+15) |
| project linked | 4939 / 28465 | **4954 / 28465** (+15) |
| `MinorIng/MetroidPrime/ScriptObjects/CMinorIngRel` | - | 15/15, 272/272 bytes, 100.00% |
| REL sha1s matching `config.yml` | 86/86 | **86/86** |
| DOL sha1 | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` | unchanged |

```
$ python3 tools/audit_rel_claim.py MinorIng
ok   MetroidPrime/ScriptObjects/CMinorIngRel.cpp   0x00000000..0x00000110  15/15 functions
ok   REL/global_destructor_chain.c                 0x0000B484..0x0000B4F8  2/2 functions
ok   REL/REL_Setup.cpp                             0x0000B4F8..0x0000B69C  5/5 functions
0 claim(s) with a problem
MinorIng: preplf 217 text symbols, plf 217, 0 dropped by -strip_partial
```

```
$ ./tools/unit_fit.sh MetroidPrime/ScriptObjects/CMinorIngRel.cpp
   .text      claimed    272   ours    272   retail    272   fits
   no extra functions: our object defines only what the retail unit object does
$ python3 tools/check_decl_order.py --unit MinorIng/MetroidPrime/ScriptObjects/CMinorIngRel
ok: 1 unit(s) checked, none emits its functions out of retail order
$ ./tools/goal_check.sh build/goal/item.json
goal_check: PASS progress-rel-head-minoring
```

`powerpc-eabi-nm` on the object lists all 15 in ascending retail order (`fn_44_0` .. `fn_44_E0`),
so the reverse-declaration rule holds.

## What the arrangement is, and the three things the family did not predict

1. **The claim starts at 0x0.** Unlike `CChozoGhostRel.cpp`, which starts at 0x350 because
   destructor code sits below its accessors, MinorIng's `.text` opens on the accessor block, so
   dtk's `auto_00_00000000_text` splits once: ours at `0x0..0x110`, retail's at `0x110..0xB484`.
2. **The block is `CAtomicAlphaRel.cpp`'s re-ordered**, measured by diffing
   `build/G2ME01/MinorIng/asm/auto_00_00000000_text.s` over `0x0..0x9C` against that file's, not by
   reading the `fn_<id>_<off>` names (which say nothing about which function is which). The
   leading accessors are `+0x960` and `+0xA4C` (AtomicAlpha: `+0x8C8`, `+0x7D8`); the third
   function is `li r3,1` where AtomicAlpha's third is the `lbl_8041AAB8` store; and there is one
   `li r3,0` predicate after the byte read where AtomicAlpha has two. It also has **no**
   three-float copy and **no** `optional_object<CAABox>` wrapper, so the head is 15 functions and
   stops at 0x110 rather than AtomicAlpha's 0x13C. Copying the sibling without diffing gets the
   order wrong.
3. **The loader slot is `lbl_44_bss_84`, at `.bss:0x84`.** This module's `.bss` holds nine objects
   and `.bss:0x0` is a 0x18-byte one that `fn_44_110`'s code far above the head reads, so the
   name is `lbl_44_bss_84` - which is also the module's only `data:4byte` object, the thing that
   marks it as the slot `fn_44_E0` stores through. The setter import is the **plain DOL symbol
   `fn_80218AA0`** (`stw r3, gLoader_MinorIng@sda21(r0); blr`, immediately after `LoadMinorIng` at
   0x80218A74), so no `symbols.txt` rename and no DOL change - `extern "C"`, not the mangled
   `SetLoader_...` form AtomicAlpha has to spell out.

**No dead-strip hazard and no `force_active:` entry needed**: `build/G2ME01/MinorIng/ldscript.lcf`
already lists all twelve of `fn_44_0`..`fn_44_70` in FORCEACTIVE, and `lbl_44_data_620` (this
module's own 0x148-byte CPatterned vtable at `.data:0x620`) stores all twelve as entries - which is
also what identifies the block. `fn_44_70` is vtable entry 0x3C and calls slot 0x38, which
`.data:0x620` names `HealthInfo__3CAiFv`.

`fn_44_110` (0x110, 0x844) is the module's entity loader and everything from there up stays
retail, so dtk fills it and the module's sha1 holds. 217 text symbols: 15 ours, 5 `REL_Setup`,
2 `global_destructor_chain`, 195 unclaimed.

## Notes for the next run

- **`flip_test.sh` cannot test a REL unit and says so.** Run on this unit it prints
  `no source file (extern/musyx/src/MetroidPrime/ScriptObjects/CMinorIngRel.cpp)` and FAILs, then
  reverts. I confirmed it prints the identical thing for the already-landed, already-reviewed
  `CChozoGhostRel.cpp`, so it is the tool resolving REL source paths under `extern/musyx/src/`, not
  a defect in the change. For a REL module the acceptance test is the sha1 against
  `config/G2ME01/config.yml` (86/86 here) plus `unit_fit.sh` + `check_decl_order.py`, exactly as
  the module recipe in `docs/RUNNING_THE_DECOMP.md` says.
- **`tools/check_raw_offsets.py` is a gate step, so a new head with raw offsets needs a section in
  `docs/research/raw_offsets.md` in the same change** or `goal_check.sh` fails on
  `GATE FAIL: raw-offsets` even though everything else is green. It measured **1** site in the new
  file (`+0x44F`, `fn_44_28`) and the section says the real count is **six** members - the four the
  checker cannot see are listed in the section with the reason for each. While there I corrected the
  file's summary total, which the checker does not enforce: it read `150 in 59` while the tool
  measured `152 in 61`; it now reads the measured `153 in 62`.
- The file is deliberately **not** in `files.cmake`, as with every other head in this family:
  listing it would make the port link `fn_44_110` and `fn_80218AA0`, which it cannot.
- `fn_44_48`'s `extrwi r3, r0, 1, 28` is dtk's rendering of `rlwinm r3, r0, 29, 31, 31` - bit 3 of
  the byte at +0x34c, spelled `& 8`. Same word as AtomicAlpha's `fn_2_48`; not a real bit 28.
- `docs/HANDOFF.md` shows in `git status` as modified. I did not edit it - `goal_check.sh` rewrote
  the derived state block (89 units of our own code in 69 modules, `MinorIng` added to the list).
  Per the item prompt the driver discards edits to that file.

## Files touched

- `src/MetroidPrime/ScriptObjects/CMinorIngRel.cpp` (new, 15 functions)
- `config/G2ME01/rels/MinorIng/splits.txt` (+3: the `.text 0x0..0x110` claim)
- `configure.py` (+26: the `Rel("MinorIng", ...)` block and its comment, before the `Metroid` entry)
- `docs/research/raw_offsets.md` (+28/-4: the `CMinorIngRel.cpp` section, and the corrected
  summary total)
- `docs/HANDOFF.md` (rewritten by `goal_check.sh`, not by me)
