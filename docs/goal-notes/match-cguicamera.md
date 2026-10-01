# match-cguicamera - GuiSys/CGuiCamera is Matching, 9/9, flip PASS

`main/GuiSys/CGuiCamera` was `NonMatching` at 8/9 with one function left:
`Create__10CGuiCameraFP9CGuiFrameR12CInputStreamP11CSimplePoolUi` at 94.94% (452 bytes).
It is now **`Matching`, 100.00% fuzzy, 1300/1300 code and 128/128 data, 9/9 functions**, and
`tools/flip_test.sh GuiSys/CGuiCamera.cpp` prints `PASS -> kept as Matching` with
`build/G2ME01/main.dol` at `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

## The defect, measured

The body of `Create` was already instruction-for-instruction retail's. The whole 5.06% was the
`operator new` placement string. Retail's two `bl __nw__FUlPCcPCc` sites each compute their
`r4` argument in **two** instructions:

```
/* 80274AEC 3C 60 80 3B */ lis  r3, "@stringBase0"@ha
/* 80274AF4 38 83 E4 C0 */ addi r4, r3, "@stringBase0"@l
/* 80274AFC 38 60 00 D8 */ li   r3, 0xd8
/* 80274B00 38 84 00 27 */ addi r4, r4, 0x27
```

Ours emitted `addi r4, r3, 0` - one instruction, offset 0 - because `rs_new`'s pool holds a
single 7-byte literal, so the address is the pool base itself and the `+0x27` addi vanishes.
That also shifted every `bl` in the function by 4 bytes and made the three `fmr`/`fmr` pairs
differ, which is where the rest of the 5.06% came from.

`config/G2ME01/symbols.txt:17675` says what the offset is:
`@stringBase0 = .rodata:0x803AE4C0; // type:object size:0x2E scope:local data:string_table`,
and `build/G2ME01/asm/GuiSys/CGuiCamera.s` has the pool at `.rodata:0x0 | 0x803AE4C0 | size: 0x2E`:

```
.string "eCamTypePerspective"    // 0x00, 20 bytes with the NUL
.string "eCamTypeOrthogonal"     // 0x14, 19 bytes
.string "??(??)"                 // 0x27, 7 bytes  <- the placement string
```

So `0x803AE4C0 + 0x27` is the same `"??(??)"` every other `rs_new` in the game uses, and the
`+0x27` is not a fudge: it is the pool entry's own offset. **Neither leading name is referenced
anywhere in the binary.** I scanned the whole DOL's `.text`, `.rodata`, `.data`, `.sdata` and
`.sdata2` for `lis`/`addis 0x803B` paired with an `addi` of `0xE4C0` or `0xE4D4`: **2 hits, both
the `0xE4C0` + `0x27` pair inside `Create` itself.** They are dead pool entries in retail too.

## What I tried first, and why it does not work

The natural decompilation - let the two names be interned by declaring them - is a trap.
Measured, compiling single-TU probes with this unit's exact flags from `build.ninja`:

| source | pool interned | output |
| --- | --- | --- |
| `static const char* const k[] = {"eCamTypePerspective", "eCamTypeOrthogonal"}` | **yes**, both at 0x00 and 0x14 | **8 extra bytes of `.sdata2` ahead of the three float constants** |
| `if ("eCamTypePerspective" == "eCamTypeOrthogonal")` | yes, both | 4 comparison instructions of extra `.text` |
| `static const char* const k[]` + `sizeof(k)` | yes, both | 8 bytes of `.sdata2`, and no `??(??)` in the pool at all |
| unused `static inline` returning the names | **no** | - |
| `const char* n = 1 ? "a" : "b";` unused | only `"a"` | - |
| two `if (0) { const char* n = "..."; }` | **no** | - |
| an inline `dbg(const char*)` called with each name | **no** | - |
| `if ("a"[0] && 0)` | **no** | - |

The array costs 8 bytes of `.sdata2` because mwcceppc places small `const` data ahead of the
`@471`-`@473` float constants, and the unit claims only 16 bytes of `.sdata2` against our 12 -
the array overflows the claim and shifts the three float addresses, so `ConvertToScreenSpace`
stops matching. `.sdata2` is `lbl_8041E020`/`lbl_8041E024`/`lbl_8041E028` in the retail `.s` and
the offsets are load-bearing.

## The change

`src/GuiSys/CGuiCamera.cpp` spells the 46 pool bytes out as `lbl_803AE4C0` and points `rs_new`
at `+0x27` through the existing `CMEMORY_NEW_FILE` hook in `Kyoto/Alloc/CMemory.hpp`, the same
mechanism `MetroidPrime/Factories/CStateMachineFactory.cpp` uses. `rs_new` becomes
`new (lbl_803AE4C0 + 0x27, nullptr)`, which is the same pointer value retail passes - the
`"??(??)"` at 0x27 of the pool, byte-identical - so the placement string is unchanged in
meaning and only the two-instruction address form is restored. `#define CMEMORY_NEW_FILE` sits
before every include, as that header's comment requires.

Result on the object (`objdump -h`): `.rodata` **0x2E** (retail 0x2E), `.data` 0x3C, `.sdata2`
**0x0C** with the three floats at offsets 0/4/8 exactly as retail, `.text` 0x58C.

The 120 bytes of extra `.text` that `unit_fit.sh` reports are the 4 pre-existing COMDAT weak
copies (`__ct__CGuiWidgetParms`, `GetIsActive`, `GetIsVisible`, `Initialize`); they are
unchanged by this edit and `flip_test` discards them, which is why the flip passes.

## Gates

```
sha1sum build/G2ME01/main.dol   -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11401 -> 11402   linked 5507 -> 5516
  ok    check_symbol_names.py
  ok    All:  32.77% fuzzy, 25.51% matched, 11.96% linked (11402 / 28465 functions)
  ok    flip_test GuiSys/CGuiCamera.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS match-cguicamera
```

`build/report.json`: `main/GuiSys/CGuiCamera` `complete: true`, 9/9 functions,
`fuzzy_match_percent 100.0`, `matched_code 1300/1300`, `matched_data 128/128`.

## Files

- `src/GuiSys/CGuiCamera.cpp` - `lbl_803AE4C0` definition + `CMEMORY_NEW_FILE`, +20 lines
- `configure.py:410` - `NonMatching` -> `Matching` for `GuiSys/CGuiCamera.cpp`

`docs/HANDOFF.md`'s state block is the judge's own rewrite from `gate.sh`, not mine.

---

# Run 2 (lane 2, 2026-10-01) - re-applied after the rebase failure; judge PASS again

## Why this ran twice

Run 1's work was not wrong and was not rejected. `wt-mp2-goal/build/goal/run.log` shows the
sequence at the end of item 13:

```
[2026-10-01 01:19:31Z] judge PASS match-cguicamera - not reviewed (match items are outside
                       MP_GOAL_REVIEW_KINDS='port progress')
[2026-10-01 01:19:31Z] goal/decomp moved (6926dee -> 961068e) during match-cguicamera -
                       carrying the judged change onto it
Applied patch to 'configure.py' with conflicts.
Applied patch to 'docs/HANDOFF.md' with conflicts.
Falling back to direct application...
Applied patch to 'src/GuiSys/CGuiCamera.cpp' cleanly.
[2026-10-01 01:19:51Z] match-cguicamera does not apply on 961068e - releasing it for a fresh attempt
```

`goal/decomp` moved while the agent ran (another lane committed `progress-prime1-ccameramanager`),
so the carry-on conflicted on `configure.py` - the same hunk another lane had shifted - and the
driver released the item rather than resolve it. Run 2 re-derived nothing: the patch was still on
disk at `wt-mp2-goal-L2/build/goal/rebase.patch`, so this run re-applied that exact diff onto the
new base `961068ec` and re-measured everything. The judge passed again on the moved base.

**Lesson worth keeping (general, not GameCube-specific):** a judged `match` item can be released
without anyone being wrong. Read `build/goal/run.log`'s tail for your item id before assuming the
item is fresh, and look for `does not apply on <sha> - releasing it for a fresh attempt`; the
driver keeps your patch in `build/goal/rebase.patch`, so the second run can be minutes rather than
an hour. Do not re-solve a puzzle the driver already has your answer to.

## Re-measured on this tree (base 961068ec, not run 1's 6926dee8)

```
./tools/decomp_build.sh -r main/GuiSys/CGuiCamera
  87 files OK
  All:  32.79% fuzzy, 25.53% matched, 11.98% linked (11407 / 28465 functions)
  main/GuiSys/CGuiCamera: 100.00% fuzzy, 100.00% matched (9 / 9 functions)
sha1sum build/G2ME01/main.dol  -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/flip_test.sh GuiSys/CGuiCamera.cpp
  TEST GuiSys/CGuiCamera.cpp
    PASS  -> kept as Matching
  kept: 1 / 1   failed: 0   skipped: 0
./tools/probe_sources.sh
  probe: 751 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py   -> ok (inside goal_check)
./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11406 -> 11407   linked 5514 -> 5523
  ok    check_symbol_names.py
  ok    All:  32.79% fuzzy, 25.53% matched, 11.98% linked (11407 / 28465 functions)
  ok    flip_test GuiSys/CGuiCamera.cpp: PASS, Object(Matching) in configure.py
  goal_check: PASS match-cguicamera
```

`build/report.json`, `main/GuiSys/CGuiCamera`: `complete: true`, `fuzzy_match_percent 100.0`,
`matched_code 1300/1300`, `matched_data 128/128`, `matched_functions 9/9`, and all nine functions
- `Create`, both constructors, `Draw`, `ConvertToScreenSpace`, the three inherited accessors and
`~CGuiCamera` - individually at `100.0`.

The counts differ from run 1's (11406 -> 11407 here, 11401 -> 11402 there) only because other
lanes committed between the two runs; the delta this item contributes is the same +1 matched and
+9 linked. `main/GuiSys/CGuiCamera` measured 8/9 with `Create` at 94.938% on this tree before the
edit, exactly as `item.json`'s reason says - it was not stale.

`objdump -h` on `build/G2ME01/obj/GuiSys/CGuiCamera.o` after the edit: `.rodata` 0x30 (0x2E of
content + alignment to 8), `.data` 0x40, `.sdata2` 0x10, `.text` 0x514, which is retail's claim in
`config/G2ME01/splits.txt:1581-1585`.

## Files (this run)

- `src/GuiSys/CGuiCamera.cpp:1-14` - the pool comment + `extern "C" const char lbl_803AE4C0[];` +
  `#define CMEMORY_NEW_FILE (lbl_803AE4C0 + 0x27)`, before every include
- `src/GuiSys/CGuiCamera.cpp:26-31` - the 46 pool bytes as a char initialiser
- `configure.py:410` - `NonMatching` -> `Matching`

`python3 tools/check_decl_order.py --unit GuiSys/CGuiCamera.cpp` -> `0 unit(s) checked, none emits
its functions out of retail order` (it has no per-unit check for this path; `flip_test.sh` is what
actually proves the order, and it passes).

No `docs/HANDOFF.md` or `docs/RUNNING_THE_DECOMP.md` edit: the state block is the judge's rewrite
from `gate.sh`, and I did not edit it.
