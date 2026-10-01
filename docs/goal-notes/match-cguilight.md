# match-cguilight — `GuiSys/CGuiLight` is Matching

`./tools/flip_test.sh GuiSys/CGuiLight.cpp` → `PASS -> kept as Matching`.
`./tools/goal_check.sh build/goal/item.json` → `goal_check: PASS match-cguilight`
(`ok counts: matched 11400 -> 11401   linked 5507 -> 5514`).

## What the remaining 3.94% actually was

`Create__9CGuiLightFP9CGuiFrameR12CInputStreamP11CSimplePoolUi` was 628 bytes of retail and 616
of ours, and the *only* difference was one instruction repeated at each of the three allocation
sites (12 bytes, 96.06% → 100%):

```
80276F38  38 83 E5 08   addi  r4, r3, "@stringBase0"@l
80276F3C  38 60 00 E4   li    r3, 0xe4
80276F40  38 84 00 41   addi  r4, r4, 0x41      <-- missing from ours
80276F44  38 A0 00 00   li    r5, 0x0
```

Everything else in `Create` was already byte-exact (checked instruction by instruction against
`build/G2ME01/obj/GuiSys/CGuiLight.o`, branch targets compared by relocation symbol). The unit's
`.rodata` was at 17.72% for the same reason: retail's pool is 0x48 bytes holding four literals and
ours was 7 bytes holding one.

## Retail's string pool (`tools/dol_read.py 0x803AE4F0 0x80`)

```
0x803AE508  "kGUI_LightTypeSpot"        18 chars
0x803AE51B  "kGUI_LightTypePoint"       19 chars
0x803AE52F  "kGUI_LightTypeDirectional" 25 chars
0x803AE549  "??(??"                     5 chars   <- @stringBase0 + 0x41
```

65 bytes of names, then the `rs_new` placement string. Only the last literal is referenced:
`objdump -r` on the retail object shows exactly six `@stringBase0` relocations, the three
`HA`/`LO` pairs of the three `__nw__FUlPCcPCc` calls. **Retail emitted three string literals that
no code in the unit references.**

## The wall, and the way round it

mwcceppc GC/2.7 only pools literals that emitted code references. Four spellings tried, all
measured by rebuilding only `build/G2ME01/src/GuiSys/CGuiLight.o` and dumping `.rodata`:

| spelling | pool in the object | extra `.text` |
|---|---|---|
| `static const char* GetLightTypeName(ELightType)` with a `switch` returning the three names | names pooled, but `??(??)` lands at offset **0** | yes - `GetLightTypeName__F10ELightType`, 0x8c bytes, no retail symbol |
| `static inline` / plain `inline` unused function in the .cpp | nothing pooled | no |
| the same `inline` function in `include/GuiSys/CGuiLight.hpp` | nothing pooled | no |
| literals in a dead `if (0) { }` block inside `Create` | nothing pooled | no |

The pool order is also not source order: with the `static` table *above* `Create`, the `??(??)`
from `rs_new` in `Create` is still interned first (pool `??(??)`, Spot, Point, Directional), and
substituting a distinct `new ("ZZprobe", ...)` string keeps it first. `-str reuse` does not
tail-merge: keeping `rs_new` alongside an explicit 71-byte pool array gave a 77-byte pool
(71 + a second `??(??)`) and the original 616-byte `Create`.

What reproduces the bytes is spelling the pool out as one array and addressing the placement
string by offset:

```cpp
static const char kLightTypeNamePool[] =
    "kGUI_LightTypeSpot\0kGUI_LightTypePoint\0kGUI_LightTypeDirectional\0??(??)";
enum { kLightTypeNamePoolPlacement = 65 };
...
ret = new (kLightTypeNamePool + kLightTypeNamePoolPlacement, nullptr) CGuiLight(parms, lt);
```

`.rodata` becomes byte-identical to retail (72/72) and each site emits
`lis`/`addi`/`addi r4,r4,65`/`li r5,0` in retail's order, so `Create` is 157 instructions against
retail's 157. The array is `static`, so no other unit's pool moves.

## Measured after

* `build/report.json`, `main/GuiSys/CGuiLight`: 100.0% fuzzy, **7 / 7** matched functions,
  `complete: true`; `.text` 1464/1464, `.rodata` 72/72, `.data` 64/64, `.sdata2` 8/8 all 100.0%.
* Tree: `matched_functions` 11400 -> **11401**, `complete_units` 738 -> **739**,
  `All: 32.75% fuzzy, 25.51% matched`.
* `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; all 86 RELs
  `cmp`-equal (both are inside `goal_check`'s `gate.sh`, which passed).
* `configure.py:411` is now `Object(Matching, "GuiSys/CGuiLight.cpp")` (`flip_test.sh` made the
  edit and kept it). No `splits.txt` change, so `total_functions` is still 28465;
  `check_decl_order.py --unit GuiSys/CGuiLight.cpp` passes.
* `tools/unit_fit.sh` still reports 6 COMDAT weak copies ours defines and retail does not
  (`__as__6CLightFRC6CLight`, `__ct__Q210CGuiWidget15CGuiWidgetParmsF...`, four `CGuiWidget`
  virtuals, 292 bytes). They are unchanged by this diff and harmless: the flip holds.

## Lesson for other lanes

**A retail unit whose `.rodata` is a string pool holding literals nothing references cannot be
reproduced by writing those literals in the source - not as a `static` table, not as an unused
`inline` in the .cpp or a header, not in dead code.** mwcceppc GC/2.7 pools a literal only when
emitted code references it, so the only route is to spell the pool out and address the
referenced literal by its offset inside it. The symptom to recognise is a lone
`addi rX, rX, <small constant>` immediately after a string `@l` load, with the unit's `.rodata`
mostly unmatched; the tell is that the offset is exactly the byte length of the *unreferenced*
literals. `.rodata` and a `.text` percentage can be the same single issue - here the 17.72%
`.rodata` and the 3.94% of `Create` were one thing, not two.

Scratch scripts used for the instruction-by-instruction diff are in `.tmp/opencode/`
(`try.sh` rebuilds just the object; `textdiff.py` diffs it against the retail object, comparing
branches by relocation symbol). Gitignored, not in the diff.
