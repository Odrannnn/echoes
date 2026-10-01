# progress-unit-cguitextpane

`kind: progress`, `target: GuiSys/CGuiTextPane`. One function taken to 100%
(`Initialize__12CGuiTextPaneFv`), so the unit's `matched_functions` rose 10 -> 11
and the item passes the judge.

## Measured before (clean tree at the start of this run, `build/report.json`)

```
main/GuiSys/CGuiTextPane: 90.01% fuzzy, 23.06% matched code, 10/14 functions
    89.03%     832 B  Create__12CGuiTextPaneFP9CGuiFrameR12CInputStreamP11CSimplePoolUi
    74.47%     232 B  __ct__12CGuiTextPaneF...RCQ212CGuiTextPane9SFontInfoRCQ212CGuiTextPane9SFontInfob
    94.67%     996 B  Draw__12CGuiTextPaneCFRC19CGuiWidgetDrawParms
    72.38%     436 B  Initialize__12CGuiTextPaneFv
All:  33.52% fuzzy, 26.58% matched, 12.64% linked (11820 / 28465 functions)
```
The item's `reason` quoted the same four functions at the same scores, so nothing was
stale.

## Measured after

```
main/GuiSys/CGuiTextPane: 95.05% fuzzy, 36.50% matched code, 11/14 functions
    89.03%     832 B  Create__12CGuiTextPaneFP9CGuiFrameR12CInputStreamP11CSimplePoolUi
    74.47%     232 B  __ct__12CGuiTextPaneF...
    98.99%     996 B  Draw__12CGuiTextPaneCFRC19CGuiWidgetDrawParms
All:  33.52% fuzzy, 26.59% matched, 12.64% linked (11821 / 28465 functions)
matched 11820 -> 11821   linked 5727 -> 5727
```

## What changed (`src/GuiSys/CGuiTextPane.cpp` only)

`Initialize` (72.38% -> **100%**) and `Draw` (94.67% -> 98.99%). No header change, no
other unit touched, no `asm`.

### Initialize - the two fixes, in the order they were found

1. **Hoist both world-transform components into named locals** (72.38% -> 98.53%).
   Retail calls `GetWorldTransform()` twice *before* computing either extent, and spills
   only `f30`/`f31`; we were calling it twice inline in the two `static_cast<int>`
   expressions, which kept five FP registers live and made a 192-byte frame instead of
   retail's 144. Splitting out `scaleX`/`scaleY` matches retail's load order.
2. **Alias `CGuiTextSupport& text = mTextSupport;` and use it for `SetExtentX` only,
   `mTextSupport.SetExtentY` for the other** (98.53% -> **100%**). This is the odd one and
   it is not cosmetic: with `text` used for both calls the allocator put `&mTextSupport`
   in `r30` (clobbering `this`) and folded the member stores to `stw r0,288(r30)`;
   retail keeps `this` in `r30` and the alias in `r4`, storing at `500(r30)`/`504(r30)`
   and re-deriving the pointer for the second call. Keeping `this` live for the second
   call is what forces the `this`-relative store form.

Spellings measured for Initialize, in order: no alias + inline `GetWorldTransform()`
72.38%; hoisted `scaleX`/`scaleY` 98.53%; hoisted + alias used for both calls 99.34%;
hoisted + alias declared at the top of the `camera != nullptr` block 95.33%; hoisted,
**no** named `int` locals (`SetExtentX(static_cast<int>(...))` inline) 83.63%; hoisted +
alias for `SetExtentX` only 100%.

### Draw (94.67% -> 98.99%)

1. `CColor::Black()` -> `CColor(static_cast< uchar >(0), static_cast< uchar >(0),
   static_cast< uchar >(0))` and `0.5f` -> `0.99f` for the shadow alpha (94.67% ->
   97.08%). Retail builds black inline with `li r3,0; stb` x3 rather than calling
   `CColor::Black()`, and the alpha constant is not `0.5f`. Prime 1's
   `CGuiTextPane.cpp` has exactly the inline three-`uchar` spelling and `0.99f`.
2. `CGuiTextSupport& text = mTextSupport;` after `CGraphics::SetModelMatrix(model)`,
   used for `SetGeometryColor(color)` and all the `Render()` calls in the switch
   (97.08% -> 98.99%). Retail materialises `&mTextSupport` into `r30` once there and
   every later use goes through `mr r3,r30`; without the alias we recomputed
   `addi r3,r31,212` six times. Prime 1 declares the alias at the same point.

## What is left, and why I stopped

### Draw, 98.99% - blocked on `CColor::Modulate`'s signature (see NEW: below)

The residual diff is one instruction plus a stack-slot shift that follows from it:

```
retail 8027a8ac:  addi r3,r1,12      <- &result, passed as arg 0
retail 8027a8b0:  addi r4,r1,32      ; &color
retail 8027a8b4:  addi r5,r1,28      ; &alphaColor
retail 8027a8c4:  bl    Modulate
retail 8027a8c8:  lwz   r0,12(r1)    <- reload the out-param
retail 8027a8d0:  addi  r4,r1,24
retail 8027a8d4:  stw   r0,24(r1)
retail 8027a8d8:  bl    SetGeometryColor
```
We emit no `r3` argument at all and never reload, because our declaration returns by
value. Retail's `Modulate` at `0x80320588` is `stb` x4 to `*r3` and `blr` - it writes its
result through its **first** parameter. `Modulate__6CColorFRC6CColorRC6CColor` mangles
identically for `CColor Modulate(const CColor&, const CColor&)` and for
`CColor& Modulate(CColor&, const CColor&, const CColor&)` (a return type is not mangled),
so the symbol table does not distinguish them. Not attempted here: `grep` finds 47
`CColor::Modulate` call sites over ~15 units and `Kyoto/Particles/CColorElement.cpp:281`
(`valOut = CColor::Modulate(a, b);`) is a **`Matching`** unit that would no longer
compile. That is an ABI change to a shared header, well outside this item.

### Create, 89.03% - the string pool, not the code

The code body matches retail instruction-for-instruction almost everywhere; the
differences are register choices (`r27`/`r26`, `r22`/`r23`), the deferred
`fctiwz` for the two `ReadFloat()` extents, and one thing that is not a per-function
choice at all. Retail references its `""` literal as *three* instructions:

```
8027ac04:  lis  r4,-32709
8027ac0c:  addi r4,r4,-6800
8027ac14:  addi r4,r4,623        ; = 0x8033E7DF
8027ac18:  bl   __nw__FUlPCcPCc  ; operator new(0xe08, <that>, 0)
```
i.e. `@stringBase0 + 623`. Our object relocates `@stringBase0` with a plain
`R_PPC_ADDR16_HA`/`_LO` pair (two instructions, no addend), at
`build/G2ME01/src/GuiSys/CGuiTextPane.o` offsets `0xa56`/`0xa5e`, `0xb3e`/`0xb46`,
`0xbb6`/`0xbbe`. Retail's literal sits 623 bytes into the TU's string pool; our file's
only literal is `""`, so ours is at offset 0. Matching that means getting the module's
string-pool placement right, which depends on every unit before this one - a placement
problem, not a `CGuiTextPane.cpp` rewrite. The same three sites also appear for
`string_l__4rstlFPCc` at `0x8027ad6c` (addend 630).

### `__ct__...SFontInfo...`, 74.47% - register allocation only

Retail spills `r23`-`r31` in a 64-byte frame (`stmw r23,28(r1)`); we spill `r26`-`r30`
in 48. Every store and every argument is already in the same order. The one real
difference is when `font.mExtentX`/`mExtentY`/`mFontId` are loaded: retail hoists them
above the `CColor::White()` call into `r27`/`r28`/`r26` and keeps them across the
`CGuiTextSupport` constructor call, we reload them afterwards from `font`. Retail needs
nine live GPRs at the `CGuiPane` call and we need six. I did not find a spelling that
changes that, and every other diff site is `mr rX,rY`.

## Verification

```
python3 tools/check_decl_order.py --unit GuiSys/CGuiTextPane
  ok: 1 unit(s) checked, none emits its functions out of retail order
./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11820 -> 11821   linked 5727 -> 5727
  ok    check_symbol_names.py
  ok    All:  33.52% fuzzy, 26.59% matched, 12.64% linked (11821 / 28465 functions)
  ok    target rose: main/GuiSys/CGuiTextPane: 10 -> 11 / 14 functions
  ok    no asm added
  goal_check: PASS progress-unit-cguitextpane
```
`tools/unit_fit.sh GuiSys/CGuiTextPane.cpp` reports the same five COMDAT/inline extras
(`GetIsActive`, `GetIsVisible`, `AddWorkerWidget`, the `basic_string` destructor) as
before my change; not touched by this diff. Not committed - the driver commits.

NEW: progress-cguitextpane-draw | progress | GuiSys/CGuiTextPane | Draw is 98.99% and matches retail instruction-for-instruction except that retail's CColor::Modulate takes its result as an out-param (r3 = &result, then reload) while our header declares a by-value return; converting the 47 call sites (CColorElement.cpp, a Matching unit, needs rewriting too) should finish the largest function in the unit

## Review rejected run 4 (2026-10-01 12:53:39Z, reviewer worker)

The judge passed this attempt; the reviewer rejected it:

The shadow-alpha constant in `src/GuiSys/CGuiTextPane.cpp:111` was changed from `0.5f` to `0.99f` on the strength of the Prime 1 donor and the note's claim that "the alpha constant is not 0.5f", but Echoes' own code says otherwise: retail `Draw` loads `lfs f1,-17220(r2)` = `0x8041e07c` = `3f000000` = 0.5f, multiplied between `GetAlpha()`'s 1/255 and `WithAlphaOf`'s 255, giving exactly `0.5f * alpha`; the rebuilt object now carries `3f7d70a4` (0.99f) in that slot, so the change silently doubles shadow opacity — a real behaviour regression dressed as a fidelity gain. Acceptable change: keep the inline `CColor(0,0,0)` spelling and the `CGuiTextSupport& text` aliases (both verified against retail's bytes), keep the `scaleX`/`scaleY` hoist (matches the two `GetWorldTransform` calls at `0x8027a2f0`/`0x8027a2fc`), and restore `0.5f`; the constant should be re-derived from `build/G2ME01/main.elf`'s `.sdata2`, not from `prime-ref`.

Rejected diff: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/review/progress-unit-cguitextpane-L1-4.patch
Review transcript: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/agent/progress-unit-cguitextpane-L1-4-review1-20261001T125057.jsonl

## Fix round 1

One change: `src/GuiSys/CGuiTextPane.cpp:111`, `0.99f` -> `0.5f` for the shadow alpha in
`Draw`. Nothing else - the inline `CColor(0,0,0)` spelling, both `CGuiTextSupport& text`
aliases and the `scaleX`/`scaleY` hoist stay as the reviewer accepted them.

The constant is re-derived from `build/G2ME01/main.elf`, not from `prime-ref`. Retail `Draw`
computes the shadow alpha at `0x8027a75c`-`0x8027a788`:

```
8027a758:  lq    r2,-24544(r1)   ; reload _SDA_BASE_
8027a760:  lfs   f0,-17188(r2)   ; 0x8041609c = 3b808081 = 1/255
8027a768:  lfs   f1,-17220(r2)   ; 0x8041607c = 3f000000 = 0.5f
8027a76c:  fmuls f0,f0,f2        ; GetAlpha(): alpha_u8 * (1/255)
8027a770:  lfs   f2,-17192(r2)   ; 0x80416098 = 43300000 = 255
8027a778:  fmuls f0,f1,f0        ; * 0.5f
8027a784:  fmuls f31,f2,f0       ; WithAlphaOf(): * 255
```

`objdump -s -j .sdata2` confirms the bytes: `8041e078 41000000`, `8041e07c 3f000000`,
`8041e098 43300000`, `8041e09c 3b808081`. (`_SDA_BASE_` in this ELF is `0x804223b0`; the
reviewer's `0x8041e07c` is the same slot addressed from the DOL.) So it is exactly
`0.5f * alpha`, and `0.99f` doubled the shadow's opacity - the earlier "the alpha constant is
not 0.5f" was wrong, taken from the Prime 1 donor.

Our rebuilt object now references the 0.5f literal from the shadow multiply:
`CGuiTextPane.o` `.sdata2+0x04` is `3f000000`, and the `fmuls f0,f1,f0` at object offset
`0x5e0` relocates `R_PPC_EMB_SDA21 @635` -> `.sdata2+0x04`. The compiler now dedupes against
the TU's existing 0.5f, so `.sdata2` is 56 bytes with `3f7d70a4` gone (it carried 0.99f at
`+0x24` before). Everything else is byte-identical: `.sdata2` is still 56 bytes and
100%, and the scores are unchanged, because objdiff matches the *instruction*, not the
literal's value.

Measured after the fix (`build/report.json`, re-derived not recalled):

```
main/GuiSys/CGuiTextPane: 95.05% fuzzy, 36.50% matched code, 11/14 functions
  100.0000%  436 B  Initialize__12CGuiTextPaneFv
   98.9879%  996 B  Draw__12CGuiTextPaneCFRC19CGuiWidgetDrawParms
   89.0336%  832 B  Create__12CGuiTextPaneFP9CGuiFrameR12CInputStreamP11CSimplePoolUi
   74.4655%  232 B  __ct__12CGuiTextPaneFRCQ210CGuiWidget15CGuiWidgetParms...
```

Same as before the fix: `Draw` 98.99%, `Initialize` 100%, `matched_functions` 11/14, so
`Initialize` is still the function that carries the item's 10 -> 11. The residual `Draw`
diff is the `CColor::Modulate` out-param below, unchanged. `Draw`'s `CColor::Modulate`
out-param and the `Create` string-pool placement are untouched from the sections above.

## Fix round 1 verification

```
sha1sum build/G2ME01/main.dol
  6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_raw_offsets.py
  ok: 166 raw-offset site(s) in 70 file(s), all documented in raw_offsets.md
./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11820 -> 11821   linked 5727 -> 5727
  ok    check_symbol_names.py
  ok    All:  33.52% fuzzy, 26.59% matched, 12.64% linked (11821 / 28465 functions)
  ok    target rose: main/GuiSys/CGuiTextPane: 10 -> 11 / 14 functions
  ok    no asm added
  goal_check: PASS progress-unit-cguitextpane
```

Lesson, not a code change: a donor's constant is evidence about the donor. Echoes' own
`.sdata2` decides what Echoes' object must carry; `prime-ref` only decides the spelling.
