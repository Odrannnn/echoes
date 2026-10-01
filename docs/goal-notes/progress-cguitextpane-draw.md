# progress-cguitextpane-draw

`kind: progress`, `target: GuiSys/CGuiTextPane`. `Draw__12CGuiTextPaneCFRC19CGuiWidgetDrawParms`
98.99% -> **100%**, so the unit's `matched_functions` rose 11 -> 12 and the judge passes.

## Measured before (`build/report.base.json`, clean tree)

```
main/GuiSys/CGuiTextPane: 95.05% fuzzy, 36.50% matched code, 11/14 functions
    98.98795%   996 B  Draw__12CGuiTextPaneCFRC19CGuiWidgetDrawParms
    89.03365%   832 B  Create__12CGuiTextPaneFP9CGuiFrameR12CInputStreamP11CSimplePoolUi
    74.46552%   232 B  __ct__12CGuiTextPaneFRCQ210CGuiWidget15CGuiWidgetParms...
All: 33.70518% fuzzy, 26.88081% matched, 12.64216% linked (11931 / 28465)
```

## Measured after

```
main/GuiSys/CGuiTextPane: 95.36% fuzzy, 67.20% matched code, 12/14 functions
    100.0000%   996 B  Draw__12CGuiTextPaneCFRC19CGuiWidgetDrawParms
    89.0336%    832 B  Create__12CGuiTextPaneFP9CGuiFrameR12CInputStreamP11CSimplePoolUi
    74.4655%    232 B  __ct__12CGuiTextPaneFRCQ210CGuiWidget15CGuiWidgetParms...
All: 33.71% fuzzy, 26.90% matched, 12.64% linked (11932 / 28465)
```

Whole-report function-by-function diff against `report.base.json`: **1 better, 0 worse, 0 new,
0 gone**. The only section that moved is `main/GuiSys/CGuiTextPane .text` 3244 B / 95.05055% ->
3244 B / 95.36128%; `.data`, `.sbss`, `.sdata2` unchanged.

## What changed - `src/GuiSys/CGuiTextPane.cpp:139-140` only

```diff
     uchar alpha = color.GetAlphau8();
     CColor alphaColor(alpha, alpha, alpha, static_cast< uchar >(255));
-    text.SetGeometryColor(CColor::Modulate(color, alphaColor));
+    CColor modulatedColor = CColor::Modulate(color, alphaColor);
+    text.SetGeometryColor(modulatedColor);
```

Two lines, one file. No header change, no `CColor::Modulate` signature change, no other unit
touched, no `asm`, no config. Semantics are identical - the temporary was already being
materialised, it is just now named, so MWCC copies it instead of aliasing it.

## The item's `reason` was wrong; this is the real mechanism

The reason said "retail's `CColor::Modulate` takes its result as an out-param (r3 = &result,
then reload) while our header declares a by-value return; converting the 47 call sites
(including the `Matching` unit `Kyoto/Particles/CColorElement.cpp`) should finish it". That
conclusion is refuted by three measurements:

1. **Retail's own body is what our by-value declaration already emits.** `tools/dis.sh
   0x80320588 0x9C` (`Modulate__6CColorFRC6CColorRC6CColor`) is four `stb` to `*r3` and
   `blr`, leaving r3 untouched. Our `build/G2ME01/src/Kyoto/Graphics/DolphinCColor.o`
   `Modulate__6CColorFRC6CColorRC6CColor` (offset `0x1ac`) is **byte-identical**, all 156
   bytes, with the *current* `static CColor Modulate(const CColor&, const CColor&)`
   declaration. MWCC already lowers a 4-byte POD return through a hidden pointer in r3, so
   there is no ABI change to make: `main/Kyoto/Graphics/DolphinCColor` is 12/12 today.
2. **The symbol would have changed.** `Modulate__6CColorFRC6CColorRC6CColor` is exactly what
   MWCC emits for the two-`const CColor&` static signature today; an explicit `CColor*` first
   parameter mangles differently, objdiff would lose the function, and the 12/12 unit would
   drop. The change the reason proposed would have made things worse.
3. **The diff was not the out-param at all.** Retail:
   `addi r3,r1,12 / addi r4,r1,32 / addi r5,r1,28 / bl / lwz r0,12(r1) / mr r3,r30 /
   addi r4,r1,24 / stw r0,24(r1) / bl SetGeometryColor` - four `CColor` stack objects. We
   emitted three, because the `const CColor&` parameter bound directly to `Modulate`'s
   return temporary: `addi r3,r1,12 ... bl / mr r3,r30 / addi r4,r1,12 / bl
   SetGeometryColor`. Retail's `lwz`+`stw` is the inline copy of the return value into a
   **separate named local**, and that extra object is what pushes `alphaColor` from 24(r1) to
   28(r1) and `color` from 28(r1) to 32(r1). Naming the local is the whole fix.

Lesson for the queue: "retail passes the result in r3 and reloads it" is not evidence of an
out-parameter in the declaration. Diff the *callee's* body against our object first - here it
already matched byte for byte - and then count the caller's stack objects, because a hidden
pointer return looks exactly like an out-param.

## Verification

```
python3 tools/check_decl_order.py --unit GuiSys/CGuiTextPane
  ok: 1 unit(s) checked, none emits its functions out of retail order
python3 tools/bytescmp.py .tmp/opencode/ctp.o Draw__12CGuiTextPane 0x8027A540 0x3E4
  43 differing instructions of 249 (996 bytes ours vs 996 retail)
  -> all 43 are relocations (35 `bl` targets, 8 `r2`-relative `lfs`/`lfd`, 1 `r13`-relative
     `lbz`); no non-relocated byte differs, and the emitted size is 0x3E4 = retail's.
python3 tools/check_symbol_names.py
  checked 515 units; 0 declared names are missing from their object
sha1sum build/G2ME01/main.dol
  6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11931 -> 11932   linked 5727 -> 5727
  ok    check_symbol_names.py
  ok    All:  33.71% fuzzy, 26.90% matched, 12.64% linked (11932 / 28465 functions)
  ok    target rose: main/GuiSys/CGuiTextPane: 11 -> 12 / 14 functions
  ok    no asm added
  goal_check: PASS progress-cguitextpane-draw
```

`tools/unit_fit.sh GuiSys/CGuiTextPane.cpp` reports the same five pre-existing extras as
before this change (`__ct__CGuiWidgetParms` copy ctor, the `rstl::basic_string` destructor,
`GetIsActive`, `GetIsVisible`, `AddWorkerWidget`, 204 bytes) and the same `.text` over-by-176.
Not introduced here, and not touched by this diff.

## Still open in this unit (unchanged, re-measured)

- `Create` 89.03% - the body matches instruction-for-instruction except for register choice,
  the deferred `fctiwz` for the two `ReadFloat()` extents, and `""` being referenced as
  `@stringBase0 + 623` (three instructions) where ours relocates `@stringBase0` with a plain
  HA/LO pair. Retail's literal placement in the TU's string pool depends on every unit before
  this one; that is a placement problem, not a `CGuiTextPane.cpp` rewrite.
- `__ct__...SFontInfo...` 74.47% - register allocation only. Retail spills `r23`-`r31` in a
  64-byte frame, we spill `r26`-`r30` in 48, and retail hoists `font.mExtentX/mExtentY/mFontId`
  above the `CColor::White()` call where we reload them afterwards. Nine live GPRs at the
  `CGuiPane` call against our six; every diff site is `mr rX,rY`.

Not attempted this run, no `NEW:` filed: both were already characterised in
`docs/goal-notes/progress-unit-cguitextpane.md` and neither is a function-level spelling, so
they are recorded here rather than queued. The unit needs a third item only after those.

Not committed - the driver commits.