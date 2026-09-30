# progress-rel-head-tweaks

`kind: progress`, `target: module:Tweaks`. Judge: `./tools/goal_check.sh build/goal/item.json` →
**PASS**, exit 0. One file changed: `src/MetroidPrime/Tweaks/Tweaks.cpp` (+15 lines, one of them
code).

## Measured before and after

`build/report.json`, per-unit `matched_functions` summed over `Tweaks/` (the judge's definition
for a `module:` target):

| unit | before | after |
| --- | --- | --- |
| `Tweaks/MetroidPrime/Tweaks/Tweaks` | 18 / 20 | **20 / 20** |
| `Tweaks/MetroidPrime/ScriptLoader/Tweaks` | 242 / 245 | 242 / 245 |
| `Tweaks/REL/REL_Setup` | 5 / 5 | 5 / 5 |
| `Tweaks/auto_03_000003F0_rodata` | 0 / 0 | 0 / 0 |
| **module Tweaks** | **265 / 270** | **267 / 270** |

Project: `matched_functions` **10236 → 10238**, `linked` 5018 → 5018 (unchanged, both units stay
`NonMatching`). `All: 31.22% fuzzy, 23.54% matched, 11.82% linked (10238 / 28465 functions)`.

Gates, all re-measured here, not recalled: `sha1sum build/G2ME01/main.dol` =
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; all 86 REL `.rel` files hash-match
`config/G2ME01/config.yml` (0 differ); `tools/gate.sh` (which also runs `report_diff.py`,
`check_module_wiring.py`, `check_docs_claims.py` and the port probe) clean; `check_symbol_names.py`
clean; `probe_sources.sh` clean.

## The item's reason was stale - re-measured first

The reason said "REL module with no own-code unit yet: decompile its head (accessors/RELMain/
RELExit)". That was wrong on this tree: the module was already at 265/270, the head is claimed by
`MetroidPrime/Tweaks/Tweaks.cpp` (`.text 0x0..0x1338`, `.bss 0x0..0xC` in
`config/G2ME01/rels/Tweaks/splits.txt`) and 18 of its 20 functions already matched. The real
remaining work was the 5 functions at 265, so that is what this item did.

## What the two gains were: the module's `operator new` placement string

`./tools/decomp_build.sh Tweaks` on the clean tree listed exactly five unmatched functions, and
`objdump -d -r` on both objects showed that **two** of them differed from retail by one repeated
4-byte idiom and nothing else:

```
$ build/binutils/powerpc-eabi-objdump -d -r --start-address=0x508 --stop-address=0xab4 \
      build/G2ME01/Tweaks/obj/MetroidPrime/Tweaks/Tweaks.o  > r.txt   # retail
$ ... same range on build/G2ME01/src/MetroidPrime/Tweaks/Tweaks.o    > o.txt   # ours
$ diff -u r.txt o.txt | grep -E '^[+-]' | grep -v '^[+-][+-][+-]' | sort | uniq -c | sort -rn
     15 -R_PPC_ADDR16_LO	lbl_82_section4_3F0
     15 -R_PPC_ADDR16_HA	lbl_82_section4_3F0
     15 -38 84 00 0e 	addi    r4,r4,14
     15 +R_PPC_ADDR16_LO	@stringBase0
     15 +R_PPC_ADDR16_HA	@stringBase0
     12 -93 e5 00 00 	stw     r31,0(r5)
     12 +93 e5 00 00 	stw     r31,0(r5)
```

That is the whole of `REL_CreateTweakGlobals` (86.50%, 1452 bytes) and the whole of `REL_LoadTweaks`
(99.25%, 536 bytes): every one of the module's 16 `__nw__FUlPCcPCc` call sites passes retail's
`operator new` a pointer built as

```
lis  r4, lbl_82_section4_3F0
addi r4, r4, 0
addi r4, r4, 14
```

while ours passes `&"??(??"` as mwcceppc's per-TU `@stringBase0` literal, dropping the third
instruction. 15 x 4 = 60 bytes, which is exactly how much shorter our function was (retail
0x508..0xAB4 = 1452, ours 0x508..0xA78 = 1392).

### Why retail has an `addi r4,r4,14`: a string pool, and how to get one

The `+14` is **not** a reloc addend and not a runtime pointer add in retail's source. It is how
MWCC addresses a string literal that is not at offset 0 of the translation unit's pool:

```
$ cat pool2.cpp
extern "C" void sink(const char*);
void f(void) { sink("AAAAAAAAAAAAAAAAAAAA"); sink("BB"); sink("CCCCCCCCCCCCCCCC"); }
$ build/binutils/powerpc-eabi-objdump -d -r pool2.o
   8:  lis  r3,0     R_PPC_ADDR16_HA  @stringBase0
  10:  addi r3,r3,0   R_PPC_ADDR16_LO  @stringBase0
  ...
  1c:  lis  r3,0     R_PPC_ADDR16_HA  @stringBase0      <- second literal
  20:  addi r3,r3,0   R_PPC_ADDR16_LO  @stringBase0
  24:  addi r3,r3,21                                    <- pool offset
$ objdump -s -j .rodata pool2.o        # one 0x25-byte pool object, three literals
```

All *used* literals of a TU share one pool object; only offset 0 gets the two-instruction form
with no third add. This module's pool is `"Standard.NTWK\0"` (14 bytes) then the `"?\?(\?\?)"` tag at
0x0E - `build/G2ME01/Tweaks/asm/auto_03_000003F0_rodata.s`, 0x3F0..0x405. Our TU had exactly one
literal, so it sat at offset 0 and every site was 4 bytes short.

The fix is to put retail's 14-byte leading literal back in the pool. A literal only enters the pool
if something in the TU uses it, and a *string* initialiser does not (it becomes its own object) -
only a **`static const char*` initialiser** does. Measured on this unit (`.rodata` / `.data` sizes
of our own object, and the count of `addi r4,r4,14` sites):

| seed spelling | `.rodata` | `.data` | `addi r4,r4,14` |
| --- | --- | --- | --- |
| none (baseline) | 0x07 | 0x08 | 0 |
| `static const char k[] = "Standard.NTWK";` (array) | 0x17 | 0x08 | 0 |
| `static const char* const k = "Standard.NTWK";` | 0x19 | 0x08 | 16 |
| **`static const char* k = "Standard.NTWK";`** | **0x15** | 0x0C | **16** |
| unused local `const char* p = "..."` in a function | 0x07 | 0x08 | 0 |
| `static inline const char* f() { return "..."; }` | 0x07 | 0x08 | 0 |

The non-`const` pointer is the one that lands the pool byte-identical to retail's: our object's
`.rodata` is now 0x15 bytes reading `Standard.NTWK\0??(??)\0`, which is retail's
`Tweaks/obj/MetroidPrime/Tweaks/Tweaks.o` `.rodata` exactly. `src/MetroidPrime/Tweaks/Tweaks.cpp`
carries the comment explaining all of this.

Also measured, so nobody repeats them: **`CMEMORY_NEW_FILE` is the wrong tool here.** Declaring
`extern "C" const char lbl_82_section4_3F0[];` + `#define CMEMORY_NEW_FILE (lbl_82_section4_3F0 + 14)`
(the `CStateMachineFactory.cpp` mechanism) links to retail's symbol but MWCC 2.7 CSEs the value into
`r31` once and reuses it - `addi r31,r4,14; mr r4,r31` at one site and `mr r4,r31` at 15 - which is
further from retail than the baseline, not closer. `&lbl[14]`, `(char*)lbl + 14`,
`(char*)((unsigned)lbl + 14)`, `lbl + 14u`, a `static const char[0x18]` + 14, and a struct member at
offset 14 all hoist the same way, and GC 1.3.2 / 2.5 / 2.6 / 2.7 all hoist identically. **MWCC only
emits the `lis/addi 0/addi off` three-instruction form for a pooled string literal, never for
`symbol + constant`.** That is the general rule this item turned up.

## What is still unmatched, and the wall

`Tweaks/MetroidPrime/ScriptLoader/Tweaks` is unchanged at 242/245. The three that remain are
exactly the three **float-valued** five-property typedef loaders, all at **98.22% / 304 bytes**:

- `LoadTypedefSLdrTweakPlayer_Collision__FR25SLdrTweakPlayer_CollisionR12CInputStream`
- `LoadTypedefSLdrTweakGame_TimeLimitChoices__FR30SLdrTweakGame_TimeLimitChoicesR12CInputStream`
- `LoadTypedefSLdrTweakPlayerGun_Beam_Combo__FR29SLdrTweakPlayerGun_Beam_ComboR12CInputStream`

They are the *only* three 5-float loaders in the module, and the int-valued siblings
(`LoadTypedefSLdrTweakGame_FragLimitChoices`, `..._CoinLimitChoices`) are at 100%. Every
instruction is in the same order with the same immediates; **only the callee-saved register
numbers differ** (32 of 304 bytes; first difference at function offset 17, the first `mr`):

| role | retail | ours |
| --- | --- | --- |
| `input` (param r4) | r30 | r29 |
| `sldrThis` (param r3) | r29 | r28 |
| first switch compare constant | r28 | r27 |
| loop counter `i` | r31 | r30 |
| `propertyCount` | r27 | r31 |

Ours is retail's allocation shifted down one, with `propertyCount` pushed to r31. MWCC 2.7's
allocator tie-break, not a source-level difference. Spelled 17 ways in this run, each compiled with
the unit's exact flags and compared byte-for-byte against retail (`objdump -d` of the one function
from both objects), best result 29 differing bytes - no spelling reached 100%:

- `const float v = input.ReadFloat(); sldrThis.x = v;` per case - 32 (folded away, no change)
- drop `const` from `propertyId` / `propertySize` / `propertyCount` - 32, 32, 32
- `const u16 propertyCount` - 32; `for (u16 i...)` - 55; `for (uint i...)` - 33
- `while` loop with `++i` after the body - 32; `const int n = propertyCount` - 32;
  `for (int i = 0, n = propertyCount; ...)` - **29** (closest, still not 0)
- `for (volatile int i...)` - 188; `for (int i = propertyCount; i != 0; --i)` - 228
- `input.Get<u16>()` for the size - 41; `const int propertyId = input.Get<int>()` - 32;
  `const uint propertySize` - 32; a separate `u16 propertySize;` declaration - 32;
  a local `SLdrTweakPlayer_Collision& s = sldrThis;` alias - 32; a pre-loop `const int i = 0` - 32

WALL: LoadTypedefSLdrTweakPlayer_Collision__FR25SLdrTweakPlayer_CollisionR12CInputStream 98.22% - callee-saved register numbers only, 17 spellings tried, best 29/304 bytes
WALL: LoadTypedefSLdrTweakGame_TimeLimitChoices__FR30SLdrTweakGame_TimeLimitChoicesR12CInputStream 98.22% - same shape and same wall as SLdrTweakPlayer_Collision
WALL: LoadTypedefSLdrTweakPlayerGun_Beam_Combo__FR29SLdrTweakPlayerGun_Beam_ComboR12CInputStream 98.22% - same shape and same wall as SLdrTweakPlayer_Collision

A next run should not re-try these spellings. What has not been tried is a **cflag** rather than a
source spelling - `-pragma inline_max_size` on this `Rel(...)` is the only allocator lever left,
and it would have to be tried without losing any of the 242 matching functions in the sibling unit.

## Why `MetroidPrime/Tweaks/Tweaks.cpp` is not `Matching`, and the next item

`MetroidPrime/Tweaks/Tweaks.cpp` is **20/20 at 100.00%** now, and the one rule says a unit is done
only when `flip_test.sh` passes, so it stays `NonMatching` and I did not run a flip that would have
changed `configure.py`. `tools/unit_fit.sh MetroidPrime/Tweaks/Tweaks.cpp` says why it cannot flip
today:

```
   .text      claimed   4920   ours   4920   retail   4920   fits
   .bss       claimed     12   ours      0   retail     12   SHORT by 12
   .rodata    claimed      -   ours     21   <- NOT CLAIMED BY splits.txt
   .data      claimed      -   ours     12   <- NOT CLAIMED BY splits.txt
   no extra functions: our object defines only what the retail unit object does
```

Three separate section-placement problems, all pre-existing (the 8 bytes of `.data` are on the
branch head; this change takes them to 12):

1. `REL_loader_Tweaks` is `C` (COMMON) in our object and `B` in retail's, so the claimed `.bss`
   0x0..0xC is empty. This is the known REL trap `unit_fit.sh`'s REL column cannot see
   (`RUNNING_THE_DECOMP.md`, "Four structural facts ... 4").
2. The 0x15-byte string pool belongs to `Tweaks/auto_03_000003F0_rodata` (`.rodata 0x3F0..0x408`),
   an `auto_generated` unit, not to this one.
3. Our object has 12 bytes of `.data` retail does not have (8 bytes of MWCC per-TU pool bookkeeping
   that were already there, plus the 4-byte pool seed).

Fixing 2 and 3 means the carve this repo's REL recipe calls for: move `.rodata 0x3F0..0x408` from
the `auto_03` unit onto `MetroidPrime/Tweaks/Tweaks.cpp` in `config/G2ME01/rels/Tweaks/splits.txt`
(all four files in one change, and `total_functions` must stay 28465), and get the seed pointer out
of `.data` - which no source spelling achieves, see the table above. Fixing 1 needs the
`-common`-related section choice for `REL_loader_Tweaks` to come out as `.bss`.

NEW: match-tweaks-module-head | match | MetroidPrime/Tweaks/Tweaks.cpp | unit is 20/20 at 100.00%; the flip needs .rodata 0x3F0..0x408 moved off the auto_03 unit onto it, REL_loader_Tweaks out of COMMON into .bss, and 4 bytes of .data removed

## For the next lane on this module

- `Tweaks/auto_03_000003F0_rodata` is a 0-functions auto unit holding 24 bytes of `.rodata`. It is
  now provably the Tweaks head unit's own string pool, so it is the right thing to carve.
- The `-pool off` in the `Rel("Tweaks", ...)` block was not what kept the literal at offset 0 -
  removing it changes nothing (measured). It is the *absence of a second literal* that did.
- `src/MetroidPrime/ScriptLoader/Tweaks.cpp` (245 functions, one TU) is where the other three live;
  `tools/probe_cc.sh` compiles it in 1.6 s, so a spelling sweep there is cheap - use the unit's
  exact cflags from `build.ninja`, not `probe_cc.sh`'s DOL defaults, and compare the single
  function's bytes from both objects rather than trusting the unit percentage.

## Judge output, verbatim

```
goal_check: item progress-rel-head-tweaks (progress) target=module:Tweaks
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10236 -> 10238   linked 5018 -> 5018
  ok    check_symbol_names.py
  ok    All:  31.22% fuzzy, 23.54% matched, 11.82% linked (10238 / 28465 functions)
  ok    target rose: module:Tweaks: 265 -> 267 / 270 functions
  ok    no asm added
goal_check: PASS progress-rel-head-tweaks
```

Not committed, as the brief requires.
