# progress-prime1-cexplosion - `MetroidPrime/CExplosion` 9 -> 14 / 16 functions

**Result: PASS.** `tools/goal_check.sh build/goal/item.json` exits 0. `matched_functions`
13099 -> 13104, `linked` 6197 unchanged, DOL sha1 unchanged at
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, all 86 RELs still byte-identical, no `asm` added,
and **no unit other than the target changed** in `build/report.json`.

| measure | before | after |
| --- | --- | --- |
| `main/MetroidPrime/CExplosion` matched_functions | 9 / 16 | **14 / 16** |
| `main/MetroidPrime/CExplosion` fuzzy | 96.42112% | 99.8994% |
| `main/MetroidPrime/CExplosion` matched_code | 2340 / 3220 | 2452 / 3220 |

## Prime 1's source did not help - retail's own object said where the code went

`prime-ref/src/MetroidPrime/CExplosion.cpp` and this repo's differ in every line (Prime 1's
`CExplosion` has `mRenderThermalHot`, thermal/xray flags, `CalculateRenderBounds`, no
`playerIndex`), and nothing in either file calls any of the five functions the item names. So
Prime 1 was read and used only as the negative result it is: **the two functions the item names
were not decompilation work in `CExplosion.cpp` at all**, they are `CParticleGen` methods that
retail happens to define in this object.

The measurement that settled it was `powerpc-eabi-nm` on the extracted retail objects:

```
$ nm -g build/G2ME01/obj/MetroidPrime/CExplosion.o | grep -E 'DrawFlags|GeneratorRate|ShouldDraw|vt__12CParticleGen'
00000000 D __vt__10CExplosion
0000007c D __vt__12CParticleGen          <- the vtable is *defined* here
00000bb0 T SetGeneratorRate__12CParticleGenFf
00000bb4 T SetDrawFlags__12CParticleGenFUi
00000bbc T GetGeneratorRate__12CParticleGenCFv
00000bc4 T GetDrawFlags__12CParticleGenCFv
00000bcc T ShouldDraw__12CParticleGenCFv

$ nm -g build/G2ME01/obj/Kyoto/Particles/{CElementGen,CParticleElectric,CParticleSwoosh,CParticleGen}.o \
      | grep -E 'DrawFlags|GeneratorRate|ShouldDraw'
all five are `U` in the first three and absent from CParticleGen.o
```

and `build/G2ME01/asm/MetroidPrime/CExplosion.s:837-888` gives all five bodies, which the header's
existing inline bodies already matched **exactly**. So this was a *placement* problem, not a
decompilation problem: `include/Kyoto/Particles/CParticleGen.hpp` defined all five inline, and
mwcceppc emitted a weak (`W`) copy into every including object while `CExplosion.o` emitted
none. The item's five unmatched functions were the five that header claim kept out of this unit.

## The fix: declare them, define them here (2 files)

| file | change |
| --- | --- |
| `include/Kyoto/Particles/CParticleGen.hpp:31-54` | the five virtuals become declarations; `static uint sDrawFlags/sDrawMask` removed (see below) |
| `src/MetroidPrime/CExplosion.cpp:13-51` | the five definitions, in descending retail `.text` order, plus two `extern "C"` decls |

**Why moving them also moves the vtable, which is what proves the placement.** mwccceppc puts a
class vtable in the TU holding its **key function** - the first non-inline, non-pure virtual in
declaration order. With the bodies inline, that was `AddModifier` (index 31), so
`__vt__12CParticleGen` was emitted by `src/Kyoto/Particles/CParticleGen.cpp` and every other
object referenced it `U`; ours `CExplosion.o` had `.data 0x7c`. With `SetGeneratorRate` (index 11)
defined in `CExplosion.cpp` it becomes the key function, and the object's `.data` became **0x108
= 264 bytes, retail's exact size** - retail's `__vt__10CExplosion` (0x7c) plus
`__vt__12CParticleGen` (0x8c = 35 words, the same table
`CIngSnatchingSwarmGenAccessors.cpp:97-103` counts in the IngSnatchingSwarm REL). That is the
strongest evidence the placement is right: the class layout did not have to be guessed, it
followed.

**The statics.** `ShouldDraw` reads `lbl_80419AA0` (ANDed with) and `lbl_80419A9C` (compared
against) at `.sbss:0x80419A98` (`auto_10_80419A98_sbss.s`), both already defined as globals in
`src/MetroidPrime/mainHead.cpp:128-129` and written by `CStateManager::fn_80036650`
(`src/MetroidPrime/CStateManager.cpp:1419-1440`). Naming them as `static uint sDrawFlags;
static uint sDrawMask;` would have needed *new* definitions, and any new `.sbss` object moves the
DOL. The two `extern "C"` declarations name retail's own words instead.

**`GetGeneratorRate` keeps `return 1.f;` and does not name `lbl_8041A920`.** `docs/goal-notes/
carve-800534bc.md` found that symbol at `.sdata2:0x8041A920` = `3f800000` and wrote the carve to
reference it - but that symbol is defined only on the PC side (`src/MetroidPrime/PortGlobals.cpp:996`,
deliberately not a `configure.py` unit), so naming it in a DOL unit is an undefined symbol in the
DOL link. The literal emits into this object's own `.sdata2` (`.float 1` at its tail, byte-for-byte
what retail's object carries) and objdiff scores the function **100%** even though the relocation
reads `@1094` rather than `lbl_8041A920`. Do not "fix" that reloc.

## Per function: before %, after %, what it took

All five are measured in `build/report.json` at `main/MetroidPrime/CExplosion`.

| function | retail `.text` | before | after | Prime 1's source | edits needed |
| --- | --- | --- | --- | --- | --- |
| `SetGeneratorRate__12CParticleGenFf` | 0x800534B0, 4 B | unmatched | **100.0%** | same body (`{}`), but inline in Prime 1 too | none to the body; it had to be moved out of line |
| `SetDrawFlags__12CParticleGenFUi` | 0x800534B4, 8 B | unmatched | **100.0%** | absent (Echoes-only, no draw flags in Prime 1) | header body already right |
| `GetGeneratorRate__12CParticleGenCFv` | 0x800534BC, 8 B | unmatched | **100.0%** | same body (`return 1.f;`) | none |
| `GetDrawFlags__12CParticleGenCFv` | 0x800534C4, 8 B | unmatched | **100.0%** | absent | header body already right |
| `ShouldDraw__12CParticleGenCFv` | 0x800534CC, 84 B | unmatched | **100.0%** | absent | one: read both flags into locals first (below) |

`SetGeneratorRate`, `GetGeneratorRate`, `SetDrawFlags` and `GetDrawFlags` compiled
**byte-identical to retail on the first attempt** once they were non-inline; the only difference
objdump shows is the local address prefix. `ShouldDraw` needed one edit, and it is the one lesson
worth keeping:

**To make mwcc hoist a global load above a virtual call, read the global into a local first.**
`(GetDrawFlags() & lbl_80419AA0) == lbl_80419A9C` compiles to 20 bytes - the flags are loaded
*after* `bctrl` into volatile r4/r0, no `r31`/`r30` spill. Retail loads them *before* the call into
r31/r30 and spills them, 84 bytes. Binding them first reproduces retail exactly:

```cpp
bool CParticleGen::ShouldDraw() const {
  const uint mask = lbl_80419AA0;
  const uint flags = lbl_80419A9C;
  const uint drawFlags = GetDrawFlags();
  return (drawFlags & mask) == flags;
}
```

Spellings measured this run, all 20- or 24-instruction, none matching retail's 24-instruction
`and r0,r3,r31` / `subf r0,r30,r0` tail until the three-local form above:
`flags == (call & mask)`; `(mask & call) == flags`; `const drawFlags = call; (drawFlags & mask) == flags`;
`const mask, flags; (call & mask) == flags` (hoists, but emits `and r0,r31,r3` - operands reversed);
`const CAABox& box = *bounds`-style reference in `PreRenderAllViewports` (no effect).

## Left on the table, measured

`PreRender` 99.333336% (360 B) and `PreRenderAllViewports` 99.79412% (408 B) are the only two
still unmatched. Both were diffed instruction by instruction (`objdump -dr`, addresses stripped)
and both are **not** decompilation errors:

- `PreRender` differs in exactly two `lfs` relocations - `@962/@963/@964` against retail versus
  `@977/@978/@979`. `tools/unit_fit.sh` names the cause in one line: our `.sdata2` is **36 bytes,
  retail's is 40**. The words are identical (`0, 0, double 1/60, 15.0f, 0.2f, 4.0f, 0.25f, 1.0f`)
  and retail has four bytes of tail padding that we do not; the instruction *bytes* are identical
  and only the small-data pseudo-symbol number moves.
- `PreRenderAllViewports` is instruction-for-instruction identical apart from **which stack slot**
  each temporary gets. Retail puts the else-branch `CAABox` at `r1+0x14` and the
  `optional_object<CAABox>` at `r1+0x2C`; ours does the reverse (`r1+0x14` / `r1+0x30`). Same
  `optional_object` layout (payload at +0, flag at +24), same `pos` at `r1+0x8`, same frame size,
  same control flow. Frame-slot **allocation order** is the only difference, and it runs backwards
  from the order the statements appear - retail allocated the else-branch temporary *before* the
  first statement's temporary.

Five spellings of `PreRenderAllViewports` were built and measured, differing line counts against
retail after normalising addresses (56 is the unmodified source; lower is better):

| spelling | differing lines |
| --- | --- |
| unmodified | 56 |
| `const CAABox& box = *bounds;` in the then-branch | 56 |
| `mHasRenderBounds` moved to the end of each branch | 74 |
| `if (!bounds) { ... } else { ... }` | 74 |
| `const CVector3f pos(GetTranslation());` | 74 |
| `const CAABox pointBounds = CAABox(pos, pos);` | 147 |

No `NEW:` item is filed for either: the missing count is one instruction pair on one function and
one stack-slot permutation on another, both inside a unit that **cannot be flipped** anyway - see
below. Filing an item for that would cost a lane about an hour and buy nothing.

## The unit cannot be flipped from here, and that is a separate, larger job

`tools/unit_fit.sh MetroidPrime/CExplosion.cpp`, measured after this change:

```
   .text      claimed   3220   ours   3780   retail   3220   over by 560
   .rodata    claimed      8   ours      8   retail      8   fits
   .data      claimed    264   ours    264   retail    264   fits     <- was 0x7c before
   .sdata2    claimed     40   ours     36   retail     40   SHORT by 4
   .sdata     claimed     -   ours     40   <- NOT CLAIMED BY splits.txt
   extra: +108 __dt__Q24rstl26single_ptr<12CParticleGen>Fv      extra: +100 __dt__12CParticleGenFv
   extra: + 84 __dt__25TToken<15CGenDescription>Fv             extra: + 84 __dt__30TToken<20CElectricDescription>Fv
   extra: + 80 __dt__Q24rstl66basic_string<c,...>Fv            extra: + 60 __dt__Q24rstl24optional_object<6CAABox>Fv
   extra: + 44 GetHealthInfo__6CActorCFv
   7 function(s) present in ours but not in the retail unit object, 560 bytes total
```

Those seven are weak/COMDAT destructors and inline helpers retail defines elsewhere; 3780 - 560 =
3220 = retail's `.text` exactly, so **every byte of retail's object is now accounted for**. Each
has to be moved out of line into its own translation unit for retail's copy to land in the right
object, and each is a shared-header edit. That is a `progress` item's worth of work, not a `match`
one.

## Measurements

```
./tools/unit_fit.sh MetroidPrime/CExplosion.cpp
  .data 264 = retail 264 (was 124);  .sdata2 ours 36 vs retail 40;  .text over by 560, all of it
  the seven extra weak functions

./tools/decomp_build.sh
  All:  37.08% fuzzy, 30.51% matched, 13.46% linked (13104 / 28465 functions)
  main/MetroidPrime/CExplosion: 99.90% fuzzy, 76.15% matched (14 / 16 functions)

python3 tools/check_decl_order.py --unit MetroidPrime/CExplosion
  ok: 1 unit(s) checked, none emits its functions out of retail order

python3 tools/check_symbol_names.py
  checked 584 units; 0 declared names are missing from their object

sha1sum build/G2ME01/main.dol -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010

./tools/goal_check.sh build/goal/item.json
  ok  no judge-owned path touched
  ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok  counts: matched 13099 -> 13104   linked 6197 -> 6197
  ok  check_symbol_names.py
  ok  All:  37.08% fuzzy, 30.51% matched, 13.46% linked (13104 / 28465 functions)
  ok  target rose: main/MetroidPrime/CExplosion: 9 -> 14 / 16 functions
  ok  no asm added
  goal_check: PASS progress-prime1-cexplosion
```

`build/report.json` was diffed unit by unit against a copy of the pre-change report: the only
unit whose `matched_functions` or `fuzzy_match_percent` moved is `main/MetroidPrime/CExplosion`
(9 -> 14, 96.42112% -> 99.8994%). `Kyoto/Particles/CElementGen` (89/104), `CParticleElectric`
(67/73), `CParticleSwoosh` (55/56) and `CParticleGen` (3/3) all emit the five as `U` now, exactly
as retail does, and **none of them lost a single matched function** - the calls that were inline
were already `bl` in retail too.

## For the next one

- **An unmatched function inside a `Matching`-looking unit is often a placement bug, not a
  decompilation bug.** Check `nm` on the retail object first: `T` vs `W`/`U` across objects says
  whether retail defined it inline in the header or out of line, and `D __vt__` says which object
  holds the vtable and therefore which object holds the class's key function.
- **The vtable is the cheapest evidence available.** It lives in the key function's TU, and its
  size (`.data`) is fully determined by the class declaration. Ours went 0x7c -> 0x108 to match
  retail without touching a single member offset.
- **`.sdata2` tail padding is a real objdiff difference.** Two objects can hold identical words and
  still score <100% on every function that loads a constant from them.