# progress-unit-csavegamescreen

**Result: PASS.** `MetroidPrime/CSaveGameScreen` went **20/24 -> 24/24 matched functions**,
fuzzy and matched code both **91.67% -> 100.00%**, and the unit's `.text` is now byte-identical
to retail over its whole claimed range. `build/report.json` is the source of every number here.

    goal_check: PASS progress-unit-csavegamescreen
      ok    no judge-owned path touched
      ok    gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
      ok    counts: matched 11814 -> 11818   linked 5727 -> 5727
      ok    check_symbol_names.py
      ok    All:  33.52% fuzzy, 26.58% matched, 12.64% linked (11818 / 28465 functions)
      ok    target rose: main/MetroidPrime/CSaveGameScreen: 20 -> 24 / 24 functions
      ok    no asm added

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged).
`splits.txt` untouched, so `total_functions` is still 28465.

## The item's `reason` was wrong about what was blocking this unit

The queue said the four remaining functions were at 0.0% and named the sizes
`fn_8017D188` (84 B), `fn_8017D124` (100 B), `fn_8017D378` (168 B), `fn_8017DEE8` (212 B).
Re-measuring showed **the code was already written and already byte-correct**. All four were
emitted by the existing `src/MetroidPrime/CSaveGameScreen.cpp` at exactly retail's sizes and
offsets; they scored 0% purely because `config/G2ME01/symbols.txt` still gave them dtk
placeholder names `fn_*`, and objdiff pairs functions **by name**. The source did not need to
change for them to become matched functions at all.

Measured, per function, by extracting both `.text` sections with
`powerpc-eabi-objcopy -O binary --only-section=.text` and comparing the byte ranges
(retail base 0x8017C548):

| function | bytes | our offset | retail offset | before | after | verdict |
|---|---|---|---|---|---|---|
| `Function__57TNonStaticCallback1<15CSaveGameScreen,CP14CGuiTableGroup>FPCvPCvP14CGuiTableGroup` | 84 | 0xC40 | 0xC40 | 0.0% | 100% | byte-identical before the rename |
| `Function__60TNonStaticCallback2<15CSaveGameScreen,CP14CGuiTableGroup,Ci>FPCvPCvP14CGuiTableGroupi` | 100 | 0xBDC | 0xBDC | 0.0% | 100% | byte-identical before the rename |
| `__dt__Q24rstl65vector<28TToken<18CWorldSaveGameInfo>,Q24rstl17rmemory_allocator>Fv` | 168 | 0xE30 | 0xE30 | 0.0% | 100% | byte-identical before the rename |
| `reserve__Q24rstl65vector<28TToken<18CWorldSaveGameInfo>,Q24rstl17rmemory_allocator>Fi` | 212 | 0x1D20 | 0x19A0 | 0.0% | 100% | byte-identical before the rename |

Note the last row: our `reserve` sits at .text offset 0x1D20 while retail has it at 0x19A0,
because our object also carries 12 COMDAT weak template destructors retail's does not (see
below). The **bytes** match; only the offset within our section differs. objdiff matches on
name, so this was never the problem.

## What I changed

**1. `config/G2ME01/symbols.txt` — four renames (the whole fix).**

Done with the repo's own tool, which pairs our object against retail's by byte content and
only renames functions that are already identical:

    python3 tools/autorename.py MetroidPrime/CSaveGameScreen
    renamed 4/4

That tool is `tools/autorename.py` -> `tools/fnmap.py` (pairing) -> `tools/apply_rename.py`
(writing). I did not hand-write the names and did not copy them from anywhere: each one is the
mangled name our own compile already emits for a symbol our own object already defines. Before
renaming I confirmed **no REL references any of the four** (`grep` over
`config/G2ME01/rels/*/symbols.txt` returns nothing for all four), so the 86 REL links cannot
break - and `check_symbol_names.py` passed, which is the check that catches exactly this hazard.

**2. `include/Kyoto/TFunctor.hpp` — a stale claim, corrected.**

This is the `include/` change the judge requires, and it is genuinely part of the item rather
than a gesture: this header **defines** `TNonStaticCallback1` and `TNonStaticCallback2`, the
two classes whose mangled names are now in `symbols.txt`.

- Its header comment claimed *"nothing in this tree includes this header yet"*. That was false
  before I started: `include/GuiSys/CGuiTableGroup.hpp:6` includes it. Corrected to name the
  real includer and the real reason only two forms exist (CSaveGameScreen is the only TU that
  *instantiates* them).
- Added a paragraph recording the measurement the rename rests on: both `::Function` bodies are
  out of line in retail, at 0x8017D188 (0x54) and 0x8017D124 (0x64), each a 12-byte `memcpy` of
  the method pointer into a stack slot followed by `__ptmf_scall` - which is why both take the
  method pointer as `const void*` and copy it rather than dereferencing in place. Read off
  `tools/dis.sh 0x8017D188 0x54` and `tools/dis.sh 0x8017D124 0x64`.

No behaviour changed; the header is comment-only. `src/` is untouched.

## Left undone, and why

**The unit stays `NonMatching`.** This is a `progress` item and the brief says not to run
`flip_test` to decide it. But the question the driver will ask next is whether it can now flip,
so I measured `tools/unit_fit.sh MetroidPrime/CSaveGameScreen.cpp` rather than guessing:

    .text  claimed 6772   ours 7712   over by 940
    12 function(s) present in ours but not in the retail unit object, 940 bytes total

All 12 are COMDAT weak template destructors and inline helpers -
`__dt__TCachedToken<CTexture/CGuiFrame/CStringTable>`, `__dt__TToken<...>`,
`__dt__single_ptr<CMemoryCardDriver>`, `__dt__basic_string<w,...>` and
`__pl__4rstlF...` - which both the retail linker and mwldeppc discard. `unit_fit.sh`'s own note
says this exact shape is harmless and that **only `flip_test.sh` decides**. I did not run it,
because the brief for a `progress` item is explicit that the flip is not this item's verdict.

A follow-up `match` item should just run `./tools/flip_test.sh MetroidPrime/CSaveGameScreen.cpp`.
The four functions that used to sit between `PumpLoad` and `ResetCardDriver` in the unit's
`.text` are the `TFunctor` thunks, and they are **COMDAT weak** - the same situation
`Kyoto/Bases/CAssertDolphin.cpp` and CAi already carry and flip through.

NEW: match-unit-csavegamescreen | match | MetroidPrime/CSaveGameScreen | unit is 24/24 at 100% fuzzy with .text byte-identical; only 12 discarded COMDAT weak template destructors remain, so `flip_test.sh` decides it and this should be a cheap flip.

## The lesson, for whoever seeded the next item

`reason` for this item asserted four functions sat at 0.0% and framed them as unwritten work.
They were not. **objdiff pairs functions by name, so an already-byte-identical function that
`symbols.txt` still calls `fn_` scores 0.0% and looks like unwritten code.** Before treating a
0.0% function as "needs writing", check whether the object already emits it:

    python3 tools/autorename.py <unit>

It renames only functions it has paired as byte-identical, so it cannot make anything worse.
Nine functions of `Kyoto/CPakFile` went 0% -> 100% in one call when it was written, and these
four were the same case. This is a **seed** problem, not a per-item one: an item queue built by
reading `fuzzy_match_percent` out of `report.json` will keep seeding units as "decompile the 0.0%
functions" when a one-command rename would have done it. The check that distinguishes the two is
`nm` on the unit object for a symbol of the same size.