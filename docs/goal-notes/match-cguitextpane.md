# match-cguitextpane

`kind: match`, `target: GuiSys/CGuiTextPane`. The unit went **12 -> 13 / 14** functions and the
item passes the judge as **PARTIAL** (the flip is unreachable while `Create` is at 96.82%).

## Measured before (clean tree, `build/report.json` at the start of this run)

```
main/GuiSys/CGuiTextPane: 95.36% fuzzy, 67.20% matched code, 12/14 functions
    96.82 <- was 89.03%   Create__12CGuiTextPaneFP9CGuiFrameR12CInputStreamP11CSimplePoolUi  832 B
    74.47                __ct__12CGuiTextPaneF...RCQ212CGuiTextPane9SFontInfo...             232 B
All:  33.75% fuzzy, 26.92% matched, 12.64% linked (11943 / 28465 functions)
```

The item's `reason` quoted 74.47% for the ctor, so nothing was stale.

## Measured after

```
main/GuiSys/CGuiTextPane: 99.18% fuzzy, 74.35% matched code, 13/14 functions
    100.00  232 B  __ct__12CGuiTextPaneF...RCQ212CGuiTextPane9SFontInfoRCQ212CGuiTextPane9SFontInfob
    100.00  996 B  Draw__12CGuiTextPaneCFRC19CGuiWidgetDrawParms
    100.00  436 B  Initialize__12CGuiTextPaneFv
    100.00  136 B  __ct__Q212CGuiTextPane9SFontInfoFR12CInputStream
    100.00  112 B  __dt__12CGuiTextPaneFv
    100.00   84 B  SetDimensions__12CGuiTextPaneFRC9CVector2fb
    100.00   64 B  GetFontAssets__12CGuiTextPaneCFv
    100.00   68 B  Update__12CGuiTextPaneFf
    100.00   32 B  __ct__Q212CGuiTextPane9SFontInfoFiiRC6CColorRC6CColorUi
    100.00   28 B  __ct__Q24rstl47vector<10SObjectTag,...>FiRC10SObjectTagRCQ24rstl47rmemory_allocator
    100.00   12 B  GetWidgetTypeID__12CGuiTextPaneCFv
    100.00    8 B  GetWidgetUsageFlags__12CGuiTextPaneCFv
    100.00    4 B  ScaleDimensions__12CGuiTextPaneFRC9CVector3f
    96.82   832 B  Create__12CGuiTextPaneFP9CGuiFrameR12CInputStreamP11CSimplePoolUi
All:  33.75% fuzzy, 26.93% matched, 12.64% linked (11944 / 28465 functions)
matched 11943 -> 11944   linked 5728 -> 5728
```

`Draw` also reached 100% from 98.99% as a side effect of the `Create` change (the `CColor::Modulate`
out-param diff recorded in `docs/goal-notes/progress-unit-cguitextpane.md` is gone from the
*measured* set - see "Draw" below for the exact spelling and what it did and did not fix).

## What changed (two files, `src/` and `include/`, no `asm`)

### 1. The constructor, 74.47% -> **100%** - three trivial accessors on `SFontInfo`

This is the whole item. `include/GuiSys/CGuiTextPane.hpp` gains three inline accessors and
`src/GuiSys/CGuiTextPane.cpp` reads the three scalar fields through them:

```cpp
int GetExtentX() const { return mExtentX; }
int GetExtentY() const { return mExtentY; }
CAssetId GetFontId() const { return mFontId; }
...
, mTextSupport(font.GetFontId(), font.GetExtentX(), font.GetExtentY(), properties,
               font.mFontColor, font.mOutlineColor, CColor::White(), pool)
```

**Why it works, measured.** The previous run recorded this function as "register allocation only"
and could not move it with 16 spellings of the constructor body. It is not the body: it is that
`font.mExtentX` and friends are **direct member loads**, so mwcceppc has no argument value to hoist
and keeps the loads in the argument-setup block, after the `CColor::White()` call. Written as
accessor calls, the three loads become *hoisted inline-helper arguments* - the mechanism
`docs/RUNNING_THE_DECOMP.md` documents under "mwcceppc hoists inline-helper arguments in reverse call
order" - and they then land **above** the `White()` call in reverse argument order, in
`r27`/`r28`/`r26`, exactly where retail has them. That single change moved the whole allocation:
retail's 64-byte frame with `stmw r23,28(r1)` (we had 48 with `stmw r26,24(r1)`), `this` in `r29`,
`pool` in `r23`, and the `mFontInfo` copy reusing `r27`/`r28` instead of reloading. Every store and
every argument was already in the same order before, so once the three loads were hoisted the
function came out byte-identical.

This is **the same lever as the `CVectorElement`/`CRealElement` fix** and the same class of
knowledge: when retail loads three values out of one object and keeps them across a call, and ours
loads them after the call, add accessors before touching anything else. I found it with a
standalone `mwcceppc` probe (`.tmp/opencode/probe1.cpp`, the unit's own flags) that reproduces the
constructor's shape in isolation - the probe went from our 48-byte shape to retail's 64-byte shape
on the accessor spelling alone, which is a far faster loop than rebuilding the unit. **That probe is
the recipe: for a function that differs from retail only in where loads sit around a call, reduce
it to a ~50-line standalone TU and try spellings there.**

### 2. `Create`, 89.03% -> **96.82%** - two spellings, each measured

```
spelling                                              Create
no change (the tree as handed to me)                   89.03%
in.ReadBool() kept as-is                               89.03%
float extentXf/extentYf locals + static_cast<int>      94.63%   <- the float locals
  + wordWrap as `in.ReadBool() ? true : false`         96.82%   <- the ternary
```

**The float locals (89.03% -> 94.63%).** Retail keeps the first extent in `f31` across the *second*
`ReadFloat()` call and converts both after it returns (`fmr f31,f1; bl ReadFloat; fctiwz f2,f31;
fctiwz f0,f1`), i.e. the conversion is sunk to the call site. Written
`int extentX = static_cast<int>(in.ReadFloat());` mwcceppc converts immediately, which kills the
`f31` live range. Read the two floats into named `float` locals first and convert afterwards; the
values then stay in FP registers exactly as retail has them. The same reading explains why writing
the casts inline at the `SFontInfo` construction is much worse (79.83%): it sinks the reads past the
three `ReadInt32()`s and reorders the stream.

**The ternary (94.63% -> 96.82%).** Retail materialises `wordWrap` to 0/1 with `neg`/`or`/`srwi`
*at the read site*, and keeps it in `r26`. `in.ReadBool()` returns `bool`, and mwcceppc keeps a
`bool` as the raw byte plus a deferred `clrlwi`/`neg`/`or`/`srwi` triple at its use, which pushed the
conversion six instructions later and cost a register. `in.ReadBool() ? true : false` makes the value
an `int` expression, and the materialisation lands where retail has it. Equivalents that did **not**
work, all measured: `const bool` (94.63%), `in.Get<bool>()` (94.63%),
`static_cast<bool>(in.ReadUint8())` (94.63%), `!!in.ReadBool()` (94.63%), `== true` (94.63%),
`static_cast<int>(...) ? 1 : 0` into an `int` local (95.52%), `in.ReadBool() ? 1 : 0` into a `bool`
(96.82%, same as the ternary), `const uchar` byte then ternary (96.82%, same). `const` on the
whole declaration changes nothing.

### 3. `Draw`, 98.99% -> **100%** - a side effect, and why

The previous notes blamed `Draw`'s last 1% on `CColor::Modulate` taking an out-param in retail and
returning by value here, which would need an ABI change to a shared header across 47 call sites.
**That is not what is holding `Draw` back.** With this item's two changes in the tree, `Draw` measures
**100.00%** on its own, with `CColor::Modulate` still declared `static CColor Modulate(const CColor&,
const CColor&)` in `include/Kyoto/Graphics/CColor.hpp` and all 47 call sites untouched. The
out-param theory was wrong - it was made from the disassembly alone rather than from a measurement
of the function with the rest of the TU in its final state, so it described a diff that the register
allocation in `Create`/`__ct__` was in fact masking. No `CColor.hpp` change is needed and the
`progress-cguitextpane-draw` item filed for it is void; I have not filed a correction because the
driver only takes `NEW:` lines.

## What is left: `Create` at 96.82%, and why it is not reachable from this unit

Two things, both measured, neither source-expressible in this file:

1. **A one-slot register shift in the whole function (the bulk of the residual 3.18%).** Retail
   assigns `frame`->`r28`, `in`->`r29`, `pool`->`r30`, `version`->`r31`, `fontId`->`r27`,
   `wordWrap`->`r26`; we assign the same values to `r27`/`r28`/`r29`/`r30` and `fontId`->`r31`.
   The instruction sequence is otherwise identical - same `ReadFloat`/`stfd f2,208(r1)` pairs, same
   `fctiwz` pair, same two `SFontInfo` calls, same `CGuiTextProperties` argument block, same
   `ParseBaseInfo`/`SetText` tail. Only the register numbers and the one-slot shift differ. Ruled
   out by measurement: `const` on `parms`/`width`/`height`/`version`/`extentX`/`extentY`/
   `alternateId`/`alternateX`/`alternateY`, a `const uint v = version` before the branch, moving the
   two extent conversions after the three `ReadInt32()`s, moving `wordWrap` after the reads (89.70%,
   worse - it reorders the stream), `const` on the `wordWrap` ternary, a static zero
  `CVector3f` seed for `scaleCenter` (85.33%, worse).
2. **The module string-pool addend, inherited from the previous run and re-confirmed.** Retail
   materialises its `rs_new` `__FILE__` argument and its `""` as three instructions -
   `lis r4,lbl_803AE570@ha; addi r4,r4,lbl_803AE570@l; addi r4,r4,623` - and `+630` for
   `string_l("")`. We emit a two-instruction `R_PPC_ADDR16_HA`/`_LO` pair against `@stringBase0`
   with addend 0 and 7. Retail's pool base is `lbl_803AE570` (0x803AE570, defined by
   `build/G2ME01/obj/auto_06_803AE560_rodata.o`), so retail's `""` is at 0x803AE7E6 and its
   `??(??)` at 0x803AE7DF; ours is in this unit's own 8-byte `.rodata`, which `splits.txt` does not
   claim. The bytes are identical - both pools hold `??(??)\0` - only the base differs, so this is a
   placement question that depends on every unit linked before this one, not a `CGuiTextPane.cpp`
   rewrite. **Confirmed from the linked ELF:** `nm build/G2ME01/main.elf` shows
   `803ae570 R lbl_803AE570` and `objdump -s` at 0x803AE7D0 shows `ion_EvenBottom\0??(??)\0\0\0&font=...`
   at exactly the two offsets retail adds. This is the same wall as `Tweaks`'
   "MWCC common-subexpression-eliminates the `__FILE__` argument of `new`, retail does not",
   with the pool-base half added.

## Verification

```
python3 tools/check_decl_order.py --unit GuiSys/CGuiTextPane
  ok: 1 unit(s) checked, none emits its functions out of retail order
./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11943 -> 11944   linked 5728 -> 5728
  ok    check_symbol_names.py
  ok    All:  33.75% fuzzy, 26.93% matched, 12.64% linked (11944 / 28465 functions)
  flip  flip_test GuiSys/CGuiTextPane.cpp: FAIL - judged below as partial progress
  ok    target rose: main/GuiSys/CGuiTextPane: 12 -> 13 / 14 functions
  ok    no asm added
  goal_check: PARTIAL match-cguitextpane - flip_test FAIL, but the target rose; commit it and keep the item
```

`tools/unit_fit.sh GuiSys/CGuiTextPane.cpp` reports the same five COMDAT/inline extras as before my
change (`CGuiWidgetParms` copy ctor, the `basic_string` destructor, `GetIsActive`, `GetIsVisible`,
`AddWorkerWidget` - 204 bytes) and the same unclaimed 8-byte `.rodata`; my diff adds none of them and
removes none. Not committed - the driver commits.

## For the next run on this item

`Create` is 96.82% and its two remaining walls are each worth one lane, and neither is a
`CGuiTextPane.cpp` rewrite:

- **The register shift** is a fresh measurement from this run. It is a whole-function one-slot
  shift, so it is worth the standalone-probe loop above with the full function (not just the
  constructor) reduced into `.tmp/opencode/`: if retail's `frame`/`in`/`pool`/`version` in
  `r28`-`r31` and ours in `r27`-`r30` is an allocation-order artefact, a probe can find the
  spelling in minutes where a unit rebuild cannot.
- **The string pool** is the one I would not queue as a unit item at all: `lbl_803AE570` is defined
  by an `auto_*` rodata object, so the fix is a `splits.txt`/pool-base question for the whole module
  and it is shared with every unit that calls `rs_new` or `string_l("")`. If it is ever fixed it
  will fix several units at once, and this one is the test case.

NEW: progress-cguitextpane-create | progress | GuiSys/CGuiTextPane | Create is 96.82% and its body is instruction-for-instruction retail except a one-slot register shift (frame/in/pool/version in r27-r30 vs retail's r28-r31) and the module string-pool addend; reduce it into a standalone mwcceppc probe and try spellings there, which is how the constructor's accessor fix was found