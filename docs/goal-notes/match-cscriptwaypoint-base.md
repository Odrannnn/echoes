# match-cscriptwaypoint-base

**Not done - the flip is out of reach in one item, and (measured) this item cannot be judged
partial.** The tree is unchanged (`git status` clean); everything below is measured, and the
carve is written out as a recipe so the next run does not re-derive it.

## Why a `match` item on a brand-new unit is all-or-nothing (read this first)

`tools/goal_check.sh`'s `target_rose` does

```python
hits = [u for u in units if u["name"] == t or u["name"].endswith("/" + t)]
if len(hits) != 1:
    print(f"target {t} names {len(hits)} units in {path}, need exactly 1"); sys.exit(2)
```

on the **baseline** report. `MetroidPrime/ScriptObjects/CScriptWaypoint` is not a unit on the
branch head (`configure.py` has no entry, `config/G2ME01/splits.txt` has no claim), so
`target_rose` exits 2 and the PARTIAL branch at `goal_check.sh:307-317` can never be reached.
A carve therefore either flips or scores nothing. Anything less than a flip is worth exactly
zero here, which is why this item is a bad fit for a partial-credit lane.

## What the gap actually is (all measured)

`config/G2ME01/splits.txt` today:

```
MetroidPrime/ScriptObjects/CScriptTrigger.cpp:  .text 0x8007105C..0x80073474  .data 0x803B2018..0x803B20B0
MetroidPrime/Carve80073594.c:                  .text 0x80073594..0x8007359C
MetroidPrime/Enemies/CPatterned.cpp:           .text 0x80073938..0x8007B0DC  .data 0x803B2148..0x803B263C
```

so there are **two** unclaimed `.text` gaps (0x80073474..0x80073594 and 0x8007359C..0x80073938)
and one unclaimed `.data` gap (0x803B20B0..0x803B2148, held by `main/auto_07_803B20B0_data`,
0x98 bytes, no functions). `build/G2ME01/asm/auto_03_8007359C_text.s` is dtk's disassembly of
the second text gap: 0x8007359C..0x80073938, 0x39C = 924 bytes, 6 functions, **0 matched**.

### The .data gap is two vtables, and they give every name

Dumped straight from `orig/G2ME01/sys/main.dol` (.data base 0x803B0C00, file off 0x3ADC00):

| slot | `lbl_803B20B0` (0x84) | `__vt__6CActor` (0x803B1BC0, 0x80) | what it is |
| --- | --- | --- | --- |
| [0][1] | 0, 0 | 0, 0 | offset-to-top + typeinfo (no RTTI) |
| [2] | 0x800737E4 | `__dt__6CActorFv` | complete destructor |
| [3] | 0x8009CA8C | `TypesMatch__6CActorCFi` | **`TypesMatch`, not a deleting dtor** - MWCC has no separate deleting-dtor slot |
| [6] | 0x80073754 | `AcceptScriptMsg__6CActor...` | `CScriptWaypoint::AcceptScriptMsg` |
| [10] | 0x80073598 | `AddToRenderer__6CActor...` | `CScriptWaypoint::AddToRenderer` (in `Carve80073594.c`) |
| [11] | 0x80073594 | `Render__6CActor...` | `CScriptWaypoint::Render` (in `Carve80073594.c`) |
| [31] | 0x800735D8 | 0 (terminator) | **new virtual 1** - see below |
| [32] | 0x8007359C | - | **new virtual 2** = `FollowWaypoint` |
| [33] | (end, size 0x84) | 0 | |

`lbl_803B2134` (0x14) is `[0][1]=0`, `[2]=0x800736F4` (a destructor), `[3]=0x80073938`, `[4]=0` -
the vtable of a **local `CValidEntityPredicate` subclass** used by `fn_800735D8` (it stores
`__vt__21CValidEntityPredicate` then `lbl_803B2134`, the derived-ctor prologue, and destroys it
with `__dt__21CValidEntityPredicateFv`). Corroborated by `CScriptTrigger`'s 0x98-byte vtable at
0x803B2018, which adds exactly six virtuals at [31]..[36].

So the correct names, by slot, are:

| address | symbol it must be renamed to | in which unit |
| --- | --- | --- |
| 0x80073594 | `Render__15CScriptWaypointCFRC13CStateManager` | `MetroidPrime/Carve80073594.c` |
| 0x80073598 | `AddToRenderer__15CScriptWaypointCFRC13CStateManager` | `MetroidPrime/Carve80073594.c` |
| 0x8007359C | `FollowWaypoint__15CScriptWaypointCFRC13CStateManager` | the new unit |
| 0x800735D8 | see "the one unknown" | the new unit |
| 0x800736F4 | `__dt__<local predicate class>...` | the new unit |
| 0x80073754 | `AcceptScriptMsg__15CScriptWaypointFR13CStateManagerRC10CScriptMsg` | the new unit |
| 0x800737E4 | `__dt__15CScriptWaypointFv` | the new unit |
| 0x80073844 | `__ct__15CScriptWaypointF9TUniqueIdRCQ24rstl66basic_string<c,...>RC11CEntityInfoRC12CTransform4f` | the new unit |
| 0x803B20B0 | `__vt__15CScriptWaypoint` | the new unit |
| 0x803B2134 | `__vt__<local predicate class>` | the new unit |

The exact mangled spellings are **not** guessable from the map: take them from
`build/binutils/powerpc-eabi-nm -u build/G2ME01/src/MetroidPrime/ScriptObjects/CScriptCameraWaypoint.o`,
which already references five of them verbatim. **The previous item's table has the ctor and the
dtor swapped** (`docs/goal-notes/match-cscriptcamerawaypoint.md:72-73`): 0x800737E4 is the
*destructor* (it stores the vtable, calls `__dt__6CActorFv`, then `Free__7CMemoryFPCv` under
`extsh.`/`ble`) and 0x80073844 is the *constructor* (it calls
`__ct__6CActorF9TUniqueIdRC...` then `SetUseInSortedLists__6CActorFb` and `SetCallTouch__6CActorFb`).
`docs/research/missing_classes.md:268` agrees: `15CScriptWaypoint | fn_80073844 | 244 | 111` is
the constructor (object type id 111).

### `0x80073938` belongs to this TU, not to CPatterned

`fn_800735D8` and `fn_800736F4` call through `lbl_803B2134[3] = 0x80073938`, and
`build/G2ME01/obj/auto_07_803B20B0_data.o` has `U fn_80073938__10CPatternedCFR13CStateManager9TUniqueId`
next to its `U fn_80073594 ... U fn_800737E4` list. `0x80073938` is the first `.text` address of
`CPatterned.cpp`'s claim purely because dtk assigned the address to the range, not because
`CPatterned`'s source produced it. Its body (`./tools/dis.sh 0x80073938 0x40`) is

```
TCastToConstPtr<CScriptWaypoint>(mgr.GetObjectById(id)) != nullptr
```

i.e. the `IsValid` of the same local predicate. The same predicate with the same body is used a
second time in `build/G2ME01/asm/auto_03_801B73E4_text.s` (vtable `lbl_803B6900`, dtor
`fn_801B75FC`) - identical shapes, so these `IsValid`s are shared COMDATs and the class must be
declared with the **same name in both TUs** or the linker keeps two copies.

Consequences, all of which the carve has to pay for:

1. `include/MetroidPrime/Enemies/CPatterned.hpp:177` and `src/MetroidPrime/Enemies/CPatterned.cpp:593`
   declare/define `bool CPatterned::fn_80073938(CStateManager&, TUniqueId) const { return false; }`.
   That is a wrong guess: it is not a `CPatterned` member (nothing calls it, and the class it
   really belongs to is the local predicate). It scores **8.75%** today. It has to go when
   0x80073938 is claimed elsewhere, and the local class's `IsValid` has to be named in
   `symbols.txt` to replace it.
2. The carve's `.text` claim has to be `0x80073594..0x80073978` (7 functions) and
   `CPatterned.cpp`'s `.text` start has to move from `0x80073938` to `0x80073978`. If the carve
   stops at 0x80073938, our object emits its own `IsValid` COMDAT, `CPatterned`'s retail object
   still carries a second one, and every address after 0x80073938 shifts by 0x40 - the DOL sha1
   breaks on the *first* flip attempt.
3. `0x80073594`/`0x80073598` are already claimed by `MetroidPrime/Carve80073594.c`, a **Matching**
   unit. Renaming them to the C++ spellings forces that `.c` file to define functions literally
   named `Render__15CScriptWaypointCFRC13CStateManager` (a C++ definition would mangle to
   `_Z41...`). Absorbing them into the new unit instead is also fine for the judge: the two
   functions move from a `complete` unit to the new one, and `linked` is
   `base - 2 + 8`. Both are 4-file changes (`configure.py`, `splits.txt`, `files.cmake`, the source).

## What I did establish about the bodies (so nobody repeats it)

Compiled a scratch TU with the unit's exact cflags (from `build.ninja`, rule `mwcc_sjis`):

* **The return type of virtual [31] (`fn_800735D8`) is a class, not `TUniqueId`.** MWCC puts
  `this` in **r4** and `mgr` in **r5** there, which only happens behind a hidden return pointer;
  confirmed against the other caller of `FindConnectedObjects_if`
  (`auto_03_801B73E4_text.s:801B7530-801B754C`, which passes `r3 = sret, r4 = waypoint, r5 = mgr,
  r6 = state, r7 = message, r8 = predicate`). A 2-byte class with a `TUniqueId`-shaped first
  member and a user-declared ctor reproduces it exactly - `sth r0, 0x0(r3)` with `r3` the sret
  slot, no extra stack temporary, while returning plain `TUniqueId` gives `sth r0, 0x0(r3)` with
  `r3 = this`. So the real declaration is `virtual <small class> <name>(CStateManager&) const;`
  and the class name is unrecoverable (it is not part of the mangled name). **MWCC does not
  encode the return type in the symbol**, so the rename is `tools/apply_rename.py` on the address.
* **`FollowWaypoint` (0x8007359C) is `return CheckConnectedObject(mgr, kArrived, kFollow)`** -
  0x41525256 = `"ARRV"`, 0x464F4C57 = `"FOLW"`, return value untouched in r3.
* **`fn_800736F4` is reproducible today.** A local `class D : public CValidEntityPredicate` with
  an **out-of-line** destructor emits `__dt__<D>Fv` whose 25 instructions are identical to
  0x800736F4 (same `mr. r30,r3 / beq / lis / li r4,0 / stw r0,0(r30) / bl __dt__21CValidEntityPredicateFv /
  extsh. / ble / bl Free__7CMemoryFPCv` sequence, 0x60 bytes both).
* **The `IsValid` at 0x80073938 is reproducible today**: the scratch TU's
  `IsValid` body is instruction-for-instruction the retail 0x40 bytes (`mr r3,r4 / addi r4,r1,8 /
  lhz r0,0(r5) / sth r0,8(r1) / bl GetObjectById / bl TCastToPtr<...> / neg / or / srwi / blr`).
  Note it must be `TCastToConstPtr<>` - `GetObjectById` returns `const CEntity*` and
  `TCastToPtr` will not take it.
* **Constants** (read from the DOL, `.sdata2` base 0x8041A3C0 / file off 0x3C3B40):
  `lbl_8041AAA8` = float `0x3F7D70A4` = **0.99f**; `lbl_8041AAB0` = double
  `0x4330000080000000` = **2^52 + 2^31**. The index is
  `(uint)(0.99f * mgr.Random()->Float() * <int expr over the count>)`; `mgr` is in r5 and
  `mgr + 0x16E4` is the `CRandom16` (`CStateManager::Random()`), so the call is
  `mgr.Random()->Float()`. The float result is truncated in place
  (`fctiwz` / `stfd 0x38(r1)` / `lwz 0x3c(r1)`), not by a helper call - a scratch
  `static_cast<uint>(0.99f * rnd * count)` emits a `bl` to a libgcc-style helper instead, so the
  source expression is not a plain one.
* The order in the source file must be **descending by retail address** (`IsValid` 0x80073938
  first, `Render` 0x80073594 last) so that mwcceppc's reverse emission plus mwldeppc's verbatim
  `.text` reproduce ascending addresses; check with
  `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptWaypoint`.

## The one unknown: the index expression in `fn_800735D8` (0x800735D8, 284 bytes)

The tail is

```
lwz  r31, 0x20(r1)          ; the vector's count
cmpwi r31, 0 / bne          ; empty -> store kInvalidUniqueId
...
addi r3, r30, 0x16e4 / bl CRandom16::Float()      ; f1 = rnd
xoris r3, r31, 0x8000        ; <-- r31 ^ 0x8000, no ori, no clrlwi
lis  r0, 0x4330
stw  r3, 0x34(r1) / stw r0, 0x30(r1)
lfd  f2, 0x30(r1)            ; 2^52 + (r31 ^ 0x8000)
lfd  f3, lbl_8041AAB0        ; 2^52 + 2^31
lfs  f0, lbl_8041AAA8        ; 0.99f
fsubs f2, f2, f3            ; (r31 ^ 0x8000) - 2^31
fmuls f1, f1, f2 / fmuls f0, f0, f1
fctiwz f0, f0 / stfd f0, 0x38(r1) / lwz r0, 0x3c(r1)
slwi r0, r0, 1 / lhzx r0, r5, r0   ; r5 = vector data at 0x28(r1)
sth  r0, 0x0(r29)            ; r29 = the sret slot
```

`2^52 + 2^31` as the subtrahend is MWCC's **signed** int->double conversion, so the converted
value is *not* the count: for the index to land inside the vector the expression must evaluate to
about `count / 0.99`, and `(count ^ 0x8000) - 2^31` is not that. I could not find the spelling.
Spellings compiled and their `xoris`-sequence, for the next run to skip:

| source expression (all `static_cast<uint>(0.99f * mgr.Random()->Float() * E)`) | emitted |
| --- | --- |
| `E = count` | `stw r31, 0xc`, no `xoris` |
| `E = static_cast<ushort>(count)` | `clrlwi r3,r31,16`, no `xoris` |
| `E = static_cast<int>(static_cast<ushort>(count \| 0x8000))` | `ori / clrlwi / xoris` (3 insns) |
| `E = count + 0x8000` | `addis r3,r31,1 / addi r3,r3,-0x8000` |

Retail has a **bare** `xoris r3, r31, 0x8000`, i.e. MWCC believes r31 is already 16-bit clean, so
the count reaches it through a `ushort` (or a `short`) somewhere - most likely the *vector element
type* or a `TUniqueId`-sized quantity rather than `v.size()`. The `0x20(r1)` word is the value
compared against 0, so it is the thing that must also be the 16-bit quantity; if `rstl::vector`'s
size field is not at +4 the whole reading shifts and the expression changes with it. **Check the
`rstl::vector` layout in the repo before trying more spellings** - I ran out of budget here.

## Also worth knowing (cost me time, would cost the next run time)

* `config/G2ME01/symbols.txt` renames are safe for the gates here: `orig/G2ME01/sys/main.dol`
  contains **no** symbol names at all (`b'CScriptTrigger'`, `b'__ct__'`, `b'fn_8007359C'` all
  absent), so renaming cannot change the DOL bytes the sha1 gate checks, and it can only *add*
  names the RELs never imported.
* objdiff pairs functions by **name**, so no rename means no credit for the new unit at all - the
  six gap functions sit at 0% in `main/auto_03_8007359C_text` precisely because of their `fn_*`
  names. `tools/apply_rename.py` is the sanctioned mechanism.
* `tools/report_diff.py` pairs a vanished name with an added one of the **same size and no worse
  score** in the same module, so a carve that renames/moves functions does not fail the gate.
* `include/MetroidPrime/ScriptObjects/CScriptWaypoint.hpp:22-23` is **provably wrong** about
  `NextWaypoint`: `build/G2ME01/src/.../CScriptCameraWaypoint.o`'s `.data` relocations put
  `NextWaypoint__21CScriptCameraWaypointCFR13CStateManager` at offset 0x7c (= slot [31]), while
  retail's own vtable for that class (`0x803B3988`, [31] = 0x800735D8) points at the *base*
  function. So retail's `CScriptCameraWaypoint::NextWaypoint` is a hiding non-virtual, and slot
  [31] is a different virtual of `CScriptWaypoint`. Whatever the carve calls [31], it must not
  also be called `NextWaypoint`, or `CScriptCameraWaypoint.hpp:16`'s `override` stops compiling.
* Baseline this run, for the next run to diff against:
  `./tools/decomp_build.sh` -> `All: 30.96% fuzzy, 23.20% matched, 11.74% linked (10035 / 28465 functions)`;
  `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
  `main/MetroidPrime/Enemies/CPatterned` is 27/103 with `fn_80073938__10CPatterned...` at 8.75%.
  Nothing in the tree was modified.

## NEW:

NEW: progress-cpatterned-fn80073938 | progress | MetroidPrime/Enemies/CPatterned | `fn_80073938__10CPatternedCFR13CStateManager9TUniqueId` (0x80073938, 0x40 bytes) is a stub `return false;` scoring 8.75%; its real body is `return TCastToConstPtr<CScriptWaypoint>(mgr.GetObjectById(id)) != nullptr;` which a scratch TU compiled with this unit's exact cflags reproduces instruction-for-instruction, so replacing the stub is worth +1 matched function (verified in match-cscriptwaypoint-base).

---

# Run 2026-10-02 (lane 2, wt-mp2-goal-L2) - DONE, the unit is Matching

**Done.** The whole of the previous run's "the flip is out of reach in one item" reading is
superseded: the gap the item was written for really is gone, `LoadWaypoint` was the only thing
left, and it needed **one redundant cast**. `MetroidPrime/ScriptObjects/CScriptWaypoint.cpp` is
`Matching` and reproduces retail.

## The fix

`src/MetroidPrime/ScriptObjects/CScriptWaypoint.cpp:64` - `LoadWaypoint` now spells the property
loop out by hand instead of `#include ".../SLdrWaypoint.inc"`, and the count read is

```cpp
const u16 propertyCount = static_cast< u16 >(input.ReadUint16());
```

instead of the generated `const int propertyCount = input.ReadUint16();`. That single extra
`u16` conversion node moves four `mr`/`mr.` operands off **r29** onto **r28** and the function
goes from 99.72% to 100.00%. It is the same fix, already measured, as
`src/MetroidPrime/ScriptObjects/CUnknown90.cpp:74` and `CScriptRelay.cpp:21`; I only had to find
it - the two earlier notes on this item never mention it.

Why a `.cpp` edit rather than the `.inc`: `include/MetroidPrime/ScriptLoader/SLdrWaypoint.inc`
is generated by `scripts/generate_script_loaders.py`, and `CScriptRelay.cpp` / `CUnknown90.cpp`
already set the precedent of hand-writing the loop with a comment when the generated spelling
costs bytes. `CScriptWaypoint.cpp` is the only TU that includes `SLdrWaypoint.inc`.

## Spellings measured this run, all in a scratch TU with the unit's exact cflags

Baseline (`#include "SLdrWaypoint.inc"` as the tree had it) leaves **4 differing instructions**,
all of the same kind: `mr. r29,r3` / `mr r3,r29` where retail has r28. Nothing else in the
288-byte function differs - no branch, no stack slot, no call order.

| variant of the count read | differing instructions | pointer temp |
| --- | --- | --- |
| `const int propertyCount = input.ReadUint16();` (the `.inc`) | 4 | r29 |
| `const uint propertyCount = ...` | 5 | r29 |
| `const u16 propertyCount = ...` (cast-free!) | 4 | r29 |
| `int propertyCount = ...` (no `const`) | 4 | r29 |
| `const u16 propertyCount = static_cast< u16 >(input.ReadUint16());` | **0** | **r28** |
| `const int propertyCount = static_cast< u16 >(input.ReadUint16());` | **0** | **r28** |

The declared type alone does **not** do it; only the extra conversion node does. That is the one
trap here, and it is why this took a search rather than a look.

Other spellings tried in the same loop/tail and all rejected (each either changed the register
*and* the code, or changed neither): `for (uint i = ...)` (5), loop counter declared outside the
`for`, `i++` vs `++i` in the header, `const` dropped from `propertyId`/`propertySize`,
`propertySize` as `uint`, `case` without braces, `default:` written first (47), `if/else`
instead of `switch` (65), `continue`-instead-of-`break` (65), a dead `bool` from
`propertyId == 0x255a4580` (**moves the temp to r28 but costs 62 instructions**), the loop
hoisted into an `inline` free function or an `inline` member (71 each), and on the tail:
`CEntity* ret = rs_new ...; return ret;`, `return static_cast<CEntity*>(rs_new ...)`,
returning `CScriptWaypoint*` instead of `CEntity*`, and hoisting `LdrToTransform4f` /
`LdrToEntityInfo` / `mgr.AllocateUniqueId()` into named locals (34-66 instructions each; the
hoists also change evaluation order, which retail's code does not have).

## What is *not* blocking the flip, in case a later run reads `unit_fit.sh`

`tools/unit_fit.sh` still reports this unit as over: `.sdata` ours 44 vs claimed 8, an
unclaimed 1-byte `.sbss`, `.data` 148 vs 152, `.rodata` 7 vs 8, and 500 bytes of extra weak
COMDATs. I measured what that costs by flipping the unit while it was still at 99.72% and
diffing the linked DOL byte by byte:

```
total differing bytes: 6   (4 instructions, all the r29/r28 register operands)
```

Nothing else. So the dead constants in `.sdata` (`@303`-`@318`, `@631` in `.sbss`), the `.data`
/`.rodata` alignment padding, and the five weak COMDATs are all dropped by mwldeppc - and
`main/MetroidPrime/ScriptObjects/CScriptCameraWaypoint` is `Matching` while carrying the *same*
`@303`-`@318` block. objdiff keeps reporting `.data 98.96%` and `.sdata 16.67%` for this unit
even now that it is complete; those numbers are about the object, not the DOL.

One consequence worth knowing, because it is a live wrong-answer-that-looks-100: `NextWaypoint`
is byte-identical to retail and scores 100%, but the two `.sdata` constants it loads are **not
retail's**. Ours are `@700 = 0x13` and `@702 = {0,1}`; retail's are `lbl_8041AAA8 = 0.99f` and
`lbl_8041AAB0 = 2^52 + 2^31`. Both `lfs`/`lfd` encode the same SDA21 offset, so objdiff cannot
see it, and our object's own `.sdata2` even carries the *correct* 0.99f and double - unreferenced.
So `ids[int(mgr.Random()->Float() * ids.size() * 0.99f)]` compiles to something that multiplies
by two denormals and always indexes 0. The index expression is still unsolved (see the earlier
"the one unknown" section); the link does not care, the game would.

## Measured, this run

- `./tools/decomp_build.sh MetroidPrime/ScriptObjects/CScriptWaypoint` before:
  `All: 36.98% fuzzy, 30.42% matched, 13.41% linked (13045 / 28465 functions)`;
  unit 99.94% fuzzy, 9/10 functions, `LoadWaypoint...` 99.72%.
- after: unit **100.00% fuzzy, 10/10 functions**, `.text` 1284/1284;
  `All: 36.98% fuzzy, 30.42% matched, 13.43% linked (13046 / 28465 functions)`,
  linked 6148 -> 6158, DOL units 11444 -> 11445.
- `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptWaypoint` ->
  `ok: 1 unit(s) checked, none emits its functions out of retail order`.
- `tools/flip_test.sh MetroidPrime/ScriptObjects/CScriptWaypoint.cpp` -> `PASS -> kept as Matching`.
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `./tools/goal_check.sh build/goal/item.json` -> `PASS match-cscriptwaypoint-base`, every gate ok.
- Changed: `src/MetroidPrime/ScriptObjects/CScriptWaypoint.cpp` (the loop) and `configure.py:642`
  (`NonMatching` -> `Matching`). `docs/HANDOFF.md`'s state block was rewritten by `goal_check.sh`
  itself. No `asm`, no carve, no `config/` or `splits.txt` edit, nothing committed.
