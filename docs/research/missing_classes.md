# The 75 classes the 86 real REL loaders construct - and the finding that the class was not the blocker

Measured 2026-09-26 by lane `g2` at commit `3d2ce92`, from `build/G2ME01/main.elf`,
`config/G2ME01/symbols.txt`, `config/G2ME01/splits.txt` and lane e5's 86-row table in
`docs/research/real_loaders.md`. Reproduce the table with

```sh
python3 tools/missing_classes.py
```

which is the tool this file adds: it reads e5's per-loader `new` size, constructor and
vtable back out of that table and adds the three things that decide what to do next - the
class name from vtable slot +0x0c, the constructor's size, and how many of the vtable's
function slots point into a `.text` range no unit in this tree claims.

## The number that sizes the rest of the project

| | count |
| --- | --- |
| real entity loaders | **86** (77,500 bytes) |
| distinct vtables behind them | **76** |
| distinct classes behind those vtables | **75** |
| classes retail's own symbol table names (`TypesMatch__<mangled>CFi`) | **57 of the 76 vtables** |
| classes that now have a header in `include/MetroidPrime/ScriptObjects/` | **2 of 75** - `CScriptRelay`, `CUnknown90` |
| classes that now have a ctor unit | **0 of 75** - deliberately; see below |
| loaders written | **2** - `LoadRelay` (**`Matching`, 100%**, 340 bytes) and `LoadTimeKeyframe` (`NonMatching`, 99.75%, 320 bytes) |

That is the same shape e5 measured, and the second half of this file is the correction:
**the missing class is not what stops a loader from being a `Matching` unit.** One loader
is now written with **no ctor unit and no vtable claim at all** - see "The loader does not
need the ctor" below. What a loader actually needs is three things, and only the third is
decompilation:

1. **the loader's own name in `symbols.txt`.** `dtk` propagates the name in that file into
   the relocation inside `__sinit_ScriptLoader_cpp`, so the name has to be the one retail's
   own linker produced or the DOL link fails with `undefined:`. It is the MWCC spelling -
   `LoadTimeKeyframe__FR13CStateManagerR12CInputStreamRC11CEntityInfo` - **not** the
   `_Z16LoadTimeKeyframeR13CStateManagerR12CInputStreamRK11CEntityInfo` that
   `docs/research/port_link_gap_list.md` uses, which is the *port's* GCC mangling of the
   same function. Getting this backwards costs one link.
2. **the entity constructor's name in `symbols.txt`**, same mechanism, and with the same
   spelling. `fn_801F9474` became
   `__ct__10CUnknown90F9TUniqueIdRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>RC11CEntityInfof`.
3. **the loader's body.** That is the decompilation, and it is 288..3,640 bytes of property
   parsing per loader.

The vtable and the constructor's *body* are not on that list, because a loader reaches its
entity class only through the constructor, and the constructor's bytes come from dtk's
object whether or not this tree has written them. `docs/research/real_loaders.md` says the
constructor's address must lie in a range a unit with source claims; that is the claim that
has to be corrected - it is the constructor's **name** that has to exist, not its range.

## The loader does not need the ctor - worked end to end, twice

`LoadRelay` (`SRLY`, 0x800B8EFC, 340 bytes) is **`Matching` at 100%**, with its class
`CScriptRelay` in `include/MetroidPrime/ScriptObjects/CScriptRelay.hpp` and the loader in
`src/MetroidPrime/ScriptObjects/CScriptRelay.cpp`.
`LoadTimeKeyframe` (`TKEY`, 0x801F9050, 320 bytes) is the same recipe and lands at 99.75%.
**Neither claims a vtable or a constructor range.** What each took:

- **a header with a `CHECK_SIZEOF` that matches retail's `operator new` argument** -
  `CScriptRelay : CEntity` plus a `short` at +0x24 and a byte at +0x26,
  `CHECK_SIZEOF(CScriptRelay, 0x28)`, and `operator new` is called with 0x28. `CEntity` is
  `CHECK_SIZEOF(..., 0x24)`, and the constructor at 0x800B919C stores exactly two bytes past
  it. **That is the whole layout job for a class this small, and it is 20 minutes, not a
  session.** `CUnknown90` is the same shape with one `float` at +0x24 and
  `CHECK_SIZEOF(CUnknown90, 0x28)`. The layout comes from the constructor's `this`-relative
  stores, which is the only place the object is written.
- **a constructor declared, not defined.** mwcceppc emits no vtable for the class - the key
  function is `CEntity`'s - so nothing references `__vt__12CScriptRelay`, and the retail
  vtable at `lbl_803B35B8` stays dtk's. The declared constructor is an external call that
  the renamed dtk object satisfies. The port build gets a body under `#ifndef __MWERKS__`.
- **the loader body**: the `SLdr*` aggregate, a property loop, `operator new`, one
  constructor call, the `SLdrEditorProperties` destructor.
- **two renames in `config/G2ME01/symbols.txt`**, and both have to be the **MWCC** spelling:

  | | name in `symbols.txt` |
  | --- | --- |
  | `LoadRelay` | `LoadRelay__FR13CStateManagerR12CInputStreamRC11CEntityInfo` |
  | its ctor | `__ct__12CScriptRelayF9TUniqueIdRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>RC11CEntityInfob` |
  | `LoadTimeKeyframe` | `LoadTimeKeyframe__FR13CStateManagerR12CInputStreamRC11CEntityInfo` |
  | its ctor | `__ct__10CUnknown90F9TUniqueIdRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>RC11CEntityInfof` |

  **not** the `_Z16LoadRelayR13CStateManagerR12CInputStreamRK11CEntityInfo` that
  `docs/research/port_link_gap_list.md` uses, which is the *port's* GCC mangling of the
  same function. Getting this backwards costs one link, with
  `undefined: _Z16...` and `Referenced from '__sinit_ScriptLoader_cpp' in ScriptLoader.o`.
  Compile the unit standalone first and read the name out of `nm -u`; do not guess.

Three things had to be right that are not obvious, and all three are general:

**The `SLdr*` aggregate must have no user-declared constructor or destructor.** All 201
generated `include/MetroidPrime/ScriptLoader/SLdr*.hpp` headers declare a pair, and
`SLdrAreaAttributes.hpp` is the one that had it removed - with the reason written in the
file. Retail's loader calls `__ct__20SLdrEditorPropertiesFv` on the `editorProperties`
member in place and `__dt__20SLdrEditorPropertiesFv` on the way out, never a
`__ct__`/`__dt__` for the aggregate. **This is true of all 201 and is the first thing to
remove when writing any of the other 85 loaders** - `scripts/generate_script_loaders.py`
puts them back, so the fix has to go there too.

**The parsed values must live in the `SLdr*` struct, not in bare locals.** A bare `float`
local is register-cached by mwcceppc - it kept ours in `f31`, saved it with
`stfd`/`psq_st`, and grew the frame from 112 to 128 bytes. `sldrThis.time` at r1+76 is what
retail has, and it is `SLdrTimeKeyframe` at r1+16 plus `SLdrEditorProperties` at 0x3c.

## Four traps, all of them measured here, and two more that cost a build each

1. **A default float must be retail's symbol, not a literal.** `lbl_8041D648` is an
   **eight**-byte `.sdata2` symbol (`lbl_8041D648 = .sdata2:0x8041D648; //
   type:object size:0x8`) whose second word, `0.0f`, **no instruction in the whole DOL
   reaches** - `grep -E '\-19828\(r2\)' ` over the disassembly is empty. A `1.0f` literal
   makes the object carry 4 bytes of `.sdata2` where the claim has to be 8, and everything
   after it in the section moves. `extern "C" const float lbl_8041D648;` emits the
   identical `lfs f0,-19832(r2)` and no `.sdata2` at all.
2. **`.rodata` can be claimed, but only at 4-byte granularity.** `operator new`'s class-name
   argument is the literal `??(??)` (6 bytes + NUL, padded to 7 by mwcceppc, 8 on output),
   and the pool slot is `lbl_803AC568` = `.rodata:0x803AC568; // type:object size:0x7`.
   Claiming 0x803AC568..0x803AC56F makes `dtk dol split` fail with *"Invalid alignment for
   split: auto_06_803AC56F_rodata"*; claiming 0x803AC568..0x803AC570 is accepted, because
   the linker pads our 7-byte `.rodata` to its own 8-byte alignment and the dtk object for
   the unit is 8 bytes. So: **claim 8, emit 7.**
3. **A weak symbol the unit does not reference costs nothing at link time.** mwcceppc emits
   `__dt__16SLdrTimeKeyframeFv` (84 bytes) as a weak definition, and `CScriptAreaProperties.o`
   emits `__dt__18SLdrAreaAttributesFv` the same way. `mwldeppc` **drops the section**: with
   the unit `Matching`, `fn_801F9190` still links at 0x801F9190 and the weak destructor is
   absent from `main.elf` entirely. **This corrects `LANE.md`**, which says a unit that emits
   functions the retail object does not define "can never be `Matching`" - true for strong
   ones, not for weak ones. It is why the loader's `.text` is 404 bytes in the object and
   320 in the link.
4. **`r2` is `0x804223C0` in this DOL.** It is set once, at 0x80003464
   (`lis r2,-32702 ; ori r2,r2,9152`), and every `lfs fX,K(r2)` / `lwz rX,K(r13)` in the
   game is `r2 + K`. With that one number every small-data constant in a loader is readable
   straight out of the ELF, which is how `sldrThis.time`'s default was found to be `1.0f`
   and how `CScriptSpecialFunction`'s members were mapped.
5. **`__nw__`'s address argument is `0x803B0000 + sign_extend(K)`, not `0x803B0000 + K`.**
   `lis r4,-32709 ; addi r4,r4,-15000` is **0x803AC568**, the `??(??)` string - not
   0x803BC568, which is a 256-byte identity table and reads convincingly like a bug.
6. **`dtk dol split` needs a claim to cover whole symbols.** A four-byte `.sdata2` claim
   inside an eight-byte symbol is refused by name, with the symbol and both addresses in
   the message. That error is the useful one; it names the symbol to claim.

## The eight-loader cluster (`lbl_803B4A88`) is still the wrong target, and now for a measured reason

`LoadDamageActor`, `LoadFogVolume`, `LoadEnvFxDensityController`, `LoadSilhouette`,
`LoadRumbleEffect`, `LoadRadialDamage`, `LoadSpecialFunction` and `LoadSpinner` all
construct **one** class, `CScriptSpecialFunction` (`TypesMatch__22CScriptSpecialFunctionCFi`
at vtable slot +0x0c of `lbl_803B4A88`), and all eight call the **same** constructor
`fn_80109A20` with `operator new`(0x240). The FourCC selects the behaviour through the
constructor's arguments, not through the class. 5,640 bytes of loaders for one class.

f3 declined it because eight unrelated behaviours sit behind one vtable and no `SLdr*` type
supplies its members. **The first half is now measured and it is not the obstacle; the
second half is.** From the constructor at 0x801F09A20 (856 bytes), which is the only place
the object is written:

- `CScriptSpecialFunction : CActor`, and `CActor` is `CHECK_SIZEOF(..., 0x158)`. The first
  member store is `stw r20,344(r31)`, so the class's own data is 0x158..0x240 = **0xE8 =
  232 bytes**, and the constructor's last store is `stb r4,564(r31)` with the object
  0x240 = 576 bytes. Constructor largest store 564 vs `new` 576: **12 bytes of tail
  padding**, the same 4-16 byte agreement f3 measured on the other 54.
- The vtable is **29 function slots, the same 29 as `__vt__6CActor`, in the same order** -
  `CScriptSpecialFunction` adds no virtual slot at all. It overrides 8 of them (slots 0, 1,
  3, 4, 6, 7, 8, 11) and inherits 21. So claiming the vtable needs **8** renames, not 29.
- The layout is fully mappable from the ctor's stores, and the two interesting shapes are
  `0x1FC` (a `{CVector3f* , int count, int capacity, CVector3f*}` container, filled by
  `fn_80109D78`) and `0x21C` (a 24-byte `CAABox` copied field-wise, guarded by the `bool`
  at `0x234`).
- **The open question, and it is a real one:** the two flag bytes at `+0x238` and `+0x239`
  are written by `lbz` / `rlwimi`(MB=ME 24..29) / `stb` chains - six one-bit fields then
  two. `rlwimi` with MB=ME 24..31 puts the field in bits 24..31 of the register while
  `stb` writes bits 0..7, so **on this compiler those initialisations are dead writes**, and
  `LoadAIKeyframe`'s "set the AI-keyframe flag" (`li r4,1 ; rlwimi r0,r4,6,25,25`) is a
  *clear*. Retail's bytes are what they are and a header must reproduce them, but whether
  the flags are meant to live in a byte-sized unit or a word-sized one is not decidable
  from the DOL. `CActor.hpp` uses MB/ME 0..27 with `lwz`/`stw`, so a word-sized unit is the
  shape the rest of the tree uses.

So the cluster is 8 loaders for (a) one 232-byte member map, (b) a 29-slot vtable with 8
renames, (c) an 856-byte constructor. That is the largest single block in the group's
remaining 76,840 bytes and it is worth doing **later**, not first: the same 8 loaders cost
8 headers of 20 minutes each and reach 100% one at a time, and the first of them
(`LoadRelay`) took less than `LoadTimeKeyframe` did. **What the cluster needs that the cheap
path does not is a `Matching` ctor unit and a vtable claim, and that is a different, larger
piece of work - the 8 renames in (b) and 856 bytes in (c) are the whole difference.**

## The one that is not 100%, and why that is not the recipe's fault

`LoadRelay` reached 100% first time with the recipe above. `LoadTimeKeyframe` is at
**99.75%**: 4 differing bytes in 320, and all four are the same thing - the register that
receives the `operator new` result. Retail uses **r28**, the register that held the property
count; ours uses **r29**, the register that held the `0x44335aff` FourCC. Both are dead by
then, the prologue is byte-identical, and the loop is byte-identical:

```
retail   7c 7b 1b 79  mr.  r28,r3      ours   7c 7c 1b 79  mr.  r29,r3
         7c 78 1b 78  mr   r3,r28              7c 79 1b 78  mr   r3,r29
         7c 7b 1b 79  mr   r28,r3               7c 7c 1b 79  mr   r29,r3
         7c 78 1b 78  mr   r3,r28               7c 79 1b 78  mr   r3,r29
```

`LoadRelay` picks the FourCC register (r26) exactly as retail does, so **this is a property
of that one function, not of the recipe** - which is the useful part of the measurement.
**Twenty source spellings and nine flag sets all produce the same 4 bytes.** The spellings:
`ReadInt32()` vs `Get<uint>()` for the id (**this one matters** - `ReadInt32` gives
`lwz r6,0(r3)` where retail has `lwz r4,0(r4)`, i.e. 9 extra differing bytes, and
`Get< uint >()` is what fixes it, so use the `CScriptStreamedAudio` spelling, not the
`CScriptAreaProperties` one), `const` vs non-`const` locals, `u16`/`int`/`u32` loop counters,
`for` vs `while`, the FourCC as a local `const` vs an immediate, the two `case` labels in
either order, `if`/`else if` instead of `switch`, the object in a named local typed
`CUnknown90*` or `CEntity*` or declared before the loop, and each of
`mgr.AllocateUniqueId()` / `LdrToEntityInfo(...)` / `sldrThis.time` given a named local (all
three of those make it **worse**: 37, 51 and 79 differing bytes, because they add stack
slots). The flag sets: `-O4,p`/`-O4,s`, `-inline deferred,noauto` vs `-inline auto`,
`-fp_contract` on/off, `-pragma "cats off"`, `-common on`, and
`-pragma "inline_max_size(0)"` (70 bytes - much worse).

This is the same wall `docs/research/real_loaders.md` records for `LoadAreaAttributes`
("register allocation, not logic"), now on a second loader and with the search space written
down. **The other 84 will not all hit it, so the honest expectation from the recipe is one
loader per class, most of them at 100%, and a minority stuck on this.**

## The 76 vtables

`new` is the size of the entity object, from `r3` at the `operator new` call. **The class
column is vtable slot +0x0c**, which is `TypesMatch`; where it reads `CEntity`, `CActor` or
`CPhysicsActor` the class **inherits** `TypesMatch` and the name is the base's, not the
class's - those 16 are unnamed, and f3's trap applies (`LoadSteam`'s slot holds
`TypesMatch__14CScriptTriggerCFi` while its ctor calls `CScriptTrigger`'s).
**ctor bytes** is what a `Matching` ctor unit would have to reproduce; it is blank for
`CUnknown90` because this lane renamed that symbol. **slots** is the vtable's function-slot
count and **unclaimed** how many of those point into a `.text` range no unit with source
claims - that is the cost of claiming a vtable.

| loader | FourCC | `sizeof` | bytes | class (vtable slot +0x0c) | ctor | ctor bytes | slots | unclaimed |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `LoadWaypoint` | `WAYP` | 344 | 288 | `15CScriptWaypoint` | `fn_80073844` | 244 | 111 | 82 |
| `LoadCameraWaypoint` | `CAMW` | 344 | 288 | `10CUnknown43` | `fn_800A56B8` | 72 | 37 | 33 |
| `LoadSpiderBallAttractionSurface` | `BALS` | 384 | 292 | `10CUnknown81` | `fn_800FEFDC` | 392 | 31 | 29 |
| `LoadTargetingPoint` | `TGPT` | 352 | 296 | `21CScriptTargetingPoint` | `fn_8012CC14` | 252 | 233 | 215 |
| `LoadTimeKeyframe` | `TKEY` | 40 | 320 | `10CUnknown90` | `fn_801F9474` | None | 11 | 7 |
| `LoadRipple` | `RIPL` | 52 | 324 | `7CEntity` | `fn_80117580` | 148 | 23 | 19 |
| `LoadRelay` | `SRLY` | 40 | 340 | `12CScriptRelay` | `fn_800B919C` | 120 | 141 | 117 |
| `LoadSpiderBallWaypoint` | `BALW` | 392 | 344 | `10CUnknown82` | `fn_800E9B58` | 132 | 33 | 31 |
| `LoadGrapplePoint` | `GRAP` | 408 | 368 | `19CScriptGrapplePoint` | `fn_800ED668` | 268 | 31 | 29 |
| `LoadCameraShaker` | `CAMS` | 296 | 392 | `19CScriptCameraShaker` | `fn_800D5578` | 132 | 19 | 13 |
| `LoadAIJumpPoint` | `AJMP` | 392 | 392 | `18CScriptAiJumpPoint` | `fn_80150148` | 408 | 31 | 29 |
| `LoadScriptLayerController` | `SLCT` | 48 | 396 | `22CScriptLayerController` | `fn_8022FEEC` | 172 | 47 | 43 |
| `LoadSwitch` | `SWTC` | 40 | 404 | `13CScriptSwitch` | `fn_8014A564` | 120 | 23 | 19 |
| `LoadPathMeshCtrl` | `PMCT` | 352 | 412 | `10CUnknown10` | `fn_801EBA30` | 276 | 31 | 29 |
| `LoadMemoryRelay` | `MRLY` | 40 | 420 | `7CEntity` | `fn_80176734` | 136 | 175 | 165 |
| `LoadAreaDamage` | `ADMG` | 124 | 444 | `7CEntity` | `fn_801E374C` | 280 | 15 | 9 |
| `LoadWorldLightFader` | `WLIT` | 76 | 452 | `10CUnknown54` | `fn_800FFAF0` | 316 | 23 | 19 |
| `LoadControllerAction` | `CNTA` | 48 | 476 | `7CEntity` | `fn_8014A1C0` | 152 | 31 | 25 |
| `LoadMidi` | `MIDI` | 60 | 476 | `7CEntity` | `fn_8015CC78` | 212 | 91 | 81 |
| `LoadPointOfInterest` | `POIN` | 352 | 484 | `10CUnknown71` | `fn_8010EF4C` | 312 | 31 | 29 |
| `LoadTimer` | `TIMR` | 72 | 500 | `10CUnknown91` | `fn_80086CB8` | 228 | 57 | 48 |
| `LoadDamageActor` | `DMGA` | 576 | 504 | `22CScriptSpecialFunction` **(8 loaders)** | `fn_80109A20` | 856 | 31 | 29 |
| `LoadCounter` | `CNTR` | 52 | 512 | `14CScriptCounter` | `fn_8009288C` | 140 | 15 | 10 |
| `LoadRandomRelay` | `RRLY` | 48 | 512 | `7CEntity` | `fn_800BAA5C` | 164 | 125 | 105 |
| `LoadCameraBlurKeyframe` | `BLUR` | 56 | 516 | `7CEntity` | `fn_800BD9E4` | 168 | 91 | 77 |
| `LoadAIHint` | `AIHT` | 376 | 524 | `13CScriptAIHint` | `fn_8022A408` | 316 | 31 | 29 |
| `LoadPlayerHint` | `HINT` | 440 | 528 | `17CScriptPlayerHint` | `fn_8010C418` | 376 | 31 | 29 |
| `LoadPickupGenerator` | `PKGN` | 660 | 532 | `10CUnknown67` | `fn_8010D78C` | 204 | 43 | 37 |
| `LoadFogVolume` | `FOGV` | 576 | 540 | `22CScriptSpecialFunction` **(8 loaders)** | `fn_80109A20` | 856 | 31 | 29 |
| `LoadActorRotate` | `AROT` | 524 | 540 | `10CUnknown36` | `fn_8010B054` | 460 | 39 | 35 |
| `LoadAIWaypoint` | `AIWP` | 368 | 544 | `17CScriptAIWaypoint` | `fn_801E87D8` | 156 | 33 | 31 |
| `LoadRepulsor` | `REPL` | 360 | 544 | `15CScriptRepulsor` | `fn_802279B0` | 284 | 31 | 29 |
| `LoadActorKeyframe` | `ACKF` | 56 | 548 | `20CScriptActorKeyframe` | `fn_800D60C8` | 220 | 11 | 7 |
| `LoadCoverPoint` | `COVR` | 400 | 552 | `17CScriptCoverPoint` | `fn_800EC790` | 488 | 62 | 58 |
| `LoadEnvFxDensityController` | `FXDC` | 576 | 552 | `22CScriptSpecialFunction` **(8 loaders)** | `fn_80109A20` | 856 | 31 | 29 |
| `LoadPlayerStateChange` | `PSCH` | 56 | 560 | `7CEntity` | `fn_8014B558` | 128 | 23 | 19 |
| `LoadCameraPitch` | `CAMP` | 552 | 576 | `10CUnknown42` | `fn_801FBAAC` | 280 | 31 | 29 |
| `LoadSilhouette` | `SILH` | 576 | 580 | `22CScriptSpecialFunction` **(8 loaders)** | `fn_80109A20` | 856 | 31 | 29 |
| `LoadDock` | `DOCK` | 744 | 636 | `11CScriptDock` | `fn_800B76E4` | 580 | 213 | 183 |
| `LoadRumbleEffect` | `RUMB` | 576 | 636 | `22CScriptSpecialFunction` **(8 loaders)** | `fn_80109A20` | 856 | 31 | 29 |
| `LoadTriggerOrientated` | `TRGO` | 552 | 636 | `24CScriptTriggerOrientated` | `fn_801B83DC` | 312 | 37 | 35 |
| `LoadBallTrigger` | `BALT` | 584 | 648 | `24CScriptTriggerOrientated` | `fn_801193A8` | 332 | 64 | 61 |
| `LoadHUDHint` | `HHNT` | 392 | 656 | `10CUnknown63` | `fn_802331D8` | 440 | 186 | 174 |
| `LoadTrigger` | `TRGR` | 456 | 680 | `14CScriptTrigger` | `fn_8007311C` | 560 | 37 | 35 |
| `LoadEMPulse` | `EMPU` | 392 | 688 | `6CActor` | `fn_8012E6AC` | 396 | 31 | 29 |
| `LoadShadowProjector` | `SHDW` | 392 | 696 | `6CActor` | `fn_80192FA0` | 396 | 31 | 29 |
| `LoadDistanceFog` | `DFOG` | 76 | 704 | `10CUnknown54` | `fn_800FFAF0` | 316 | 23 | 19 |
| `LoadTriggerEllipsoid` | `TRGE` | 512 | 712 | `23CScriptTriggerEllipsoid` | `fn_801E2CB4` | 380 | 37 | 35 |
| `LoadGenerator` | `GENR` | 64 | 720 | `7CEntity` | `fn_800A5398` | 220 | 45 | 39 |
| `LoadAreaAttributes` | `REAA` | 92 | 744 | `7CEntity` | `__ct__21CScriptAreaPrope` | None | 11 | 7 |
| `LoadDamageableTrigger` | `DTRG` | 456 | 744 | `24CScriptDamageableTrigger` | `fn_800D0DE4` | 428 | 93 | 87 |
| `LoadSoundModifier` | `SNDM` | 336 | 760 | `10CUnknown78` | `fn_8022F674` | 244 | 55 | 49 |
| `LoadSteam` | `STEM` | 488 | 764 | `14CScriptTrigger` | `fn_8011723C` | 360 | 37 | 35 |
| `LoadSubtitle` | `SUBT` | 3728 | 772 | `6CActor` | `fn_8020E2F0` | 600 | 31 | 29 |
| `LoadRadialDamage` | `RADD` | 576 | 784 | `22CScriptSpecialFunction` **(8 loaders)** | `fn_80109A20` | 856 | 31 | 29 |
| `LoadTeamAI` | `TMAI` | 160 | 812 | `16CScriptTeamAiMgr` | `fn_801756EC` | 316 | 187 | 173 |
| `LoadCameraFilterKeyframe` | `FILT` | 68 | 828 | `7CEntity` | `fn_800BD574` | 200 | 99 | 83 |
| `LoadPortalTransition` | `PRTT` | 84 | 868 | `23CScriptPortalTransition` | `fn_80232234` | 212 | 194 | 180 |
| `LoadSpecialFunction` | `SPFN` | 576 | 948 | `22CScriptSpecialFunction` **(8 loaders)** | `fn_80109A20` | 856 | 31 | 29 |
| `LoadVisorGoo` | `VGOO` | 400 | 956 | `6CActor` | `fn_8014983C` | 628 | 37 | 31 |
| `LoadCamera` | `CAMR` | 872 | 956 | `13CScriptCamera` | `fn_801DF138` | 360 | 31 | 29 |
| `LoadTextPane` | `TXPN` | 3768 | 1072 | `15CScriptTextPane` | `fn_80200914` | 1176 | 31 | 29 |
| `LoadSpinner` | `SPIN` | 576 | 1096 | `22CScriptSpecialFunction` **(8 loaders)** | `fn_80109A20` | 856 | 31 | 29 |
| `LoadConditionalRelay` | `CRLY` | 128 | 1128 | `7CEntity` | `fn_801981EC` | 144 | 83 | 59 |
| `LoadDamageableTriggerOriented` | `DTRO` | 552 | 1164 | `10CUnknown50` | `fn_802273A8` | 288 | 124 | 116 |
| `LoadColorModulate` | `CLRM` | 140 | 1208 | `20CScriptColorModulate` | `fn_8015444C` | 420 | 11 | 7 |
| `LoadAdvancedCounter` | `ACNT` | None | 1208 | `7CEntity` | `fn_8022E8DC` | 276 | 63 | 55 |
| `LoadDebris` | `DBR1` | 944 | 1248 | `10CUnknown52` | `fn_800D48D8` | 1516 | 57 | 49 |
| `LoadDynamicLight` | `DLHT` | 1096 | 1256 | `19CScriptDynamicLight` | `fn_802216C8` | 428 | 95 | 86 |
| `LoadAmbientAI` | `AMIA` | 864 | 1308 | `13CPhysicsActor` | `fn_80179E1C` | 360 | 167 | 159 |
| `LoadControlHint` | `CTLH` | 512 | 1324 | `10CUnknown46` | `fn_8022CEFC` | 752 | 31 | 29 |
| `LoadPathCamera` | `PCAM` | 776 | 1416 | `10CUnknown65` | `fn_801E09D8` | 384 | 11 | 7 |
| `LoadSound` | `SOND` | 432 | 1528 | `12CScriptSound` | `fn_8009F240` | 676 | 31 | 29 |
| `LoadCameraHint` | `CAMH` | 576 | 1564 | `10CUnknown40` | `fn_800B8524` | 700 | 175 | 147 |
| `LoadEffect` | `EFCT` | 720 | 1772 | `13CScriptEffect` | `__ct__13CScriptEffectF9T` | None | 35 | 31 |
| `LoadSpindleCamera` | `SPND` | 1760 | 1832 | `10CUnknown83` | `fn_801DFE28` | 476 | 31 | 29 |
| `LoadWorldTeleporter` | `TEL1` | 164 | 1884 | `22CScriptWorldTeleporter` | `fn_801489E4` | 496 | 45 | 37 |
| `LoadVisorFlare` | `FLAR` | 616 | 2036 | `10CUnknown96` | `fn_801477D8` | 344 | 31 | 29 |
| `LoadPlatform` | `PLAT` | 1168 | 2072 | `15CScriptPlatform` | `fn_800A40E8` | 1388 | 43 | 39 |
| `LoadActor` | `ACTR` | 920 | 2284 | `12CScriptActor` | `fn_80070744` | 916 | 75 | 71 |
| `LoadDoor` | `DOOR` | 1200 | 2376 | `11CScriptDoor` | `fn_8007DAFC` | 1944 | 47 | 44 |
| `LoadSurfaceCamera` | `SURC` | 648 | 2468 | `10CUnknown85` | `fn_801EA83C` | 368 | 31 | 29 |
| `LoadDebrisExtended` | `DBR2` | 944 | 2948 | `10CUnknown52` | `fn_800D3F20` | 2300 | 57 | 49 |
| `LoadRoomAcoustics` | `RMAC` | 236 | 2992 | `10CUnknown76` | `fn_8013559C` | 792 | 202 | 186 |
| `LoadWater` | `WATR` | 816 | 3640 | `12CScriptWater` | `fn_800DA20C` | 2704 | 37 | 35 |

Three vtables are shared: `lbl_803B4A88` by 8 loaders (`CScriptSpecialFunction`),
`lbl_803B4648` by 2 (`LoadWorldLightFader`, `LoadDistanceFog` - `CUnknown54`), and
`lbl_803B3708` by 2 (`LoadDebris`, `LoadDebrisExtended` - `CUnknown52`). `LoadAIKeyframe`
(56 bytes, the smallest of the 86) has no `new` at all: it calls `LoadActorKeyframe` and
touches one byte of the result.

**The cheapest next three, by this lane's own measurement, are the three smallest
`sizeof` values with a class retail names:** `LoadMemoryRelay` (`MRLY`, 420 bytes, 40,
`TypesMatch` inherited from `CEntity` so the class is *unnamed* until its own
`TypesMatch__` is found - it is not in `symbols.txt`, which is itself worth a look),
`LoadSwitch` (`SWTC`, 404 bytes, 40, `CScriptSwitch`) and `LoadControllerAction` (`CNTA`,
476 bytes, 48, `TypesMatch` inherited). `LoadRandomRelay` (512 bytes, 48) and
`LoadScriptLayerController` (396 bytes, 48, `CScriptLayerController`) follow. All six are
`CEntity`-derived, so their layouts are a handful of members past 0x24 and their vtables are
11-31 slots that nothing has to claim.

## The recipe, for the next lane

1. Pick a class off the table. **Prefer a small `sizeof` and few vtable slots** - the
   `CScript`-prefixed ones with ~31 slots derive from `CActor` (0x158), so a 300-byte
   `sizeof` means ~50 bytes of own members; the `CEntity`-derived ones with 11-23 slots are
   the cheapest and there are nine of them.
2. `grep "TypesMatch__<mangled>CFi" config/G2ME01/symbols.txt` to confirm the name is the
   class's and not a base's.
3. Read the constructor's `this`-relative stores to get the layout, and `operator new`'s
   `r3` for the size. `CHECK_SIZEOF` it, with the mwcceppc toolchain
   (`tools/size_probe.cpp`), not the host's.
4. Write the header with the constructor **declared only**, and the accessors you can
   justify. No raw offsets: `tools/check_raw_offsets.py` is in `gate.sh` and a class written
   as `x1a4_` offsets is worse than no header (`docs/research/raw_offsets.md`, kind C).
5. Rename the constructor in `symbols.txt` to the name mwcceppc mangles your declaration to.
   Compile the unit standalone first (`tools/g2try.sh <unit>`) and read the undefined symbol
   out of `nm -u`; do not guess the mangling.
6. Remove the `SLdr*` aggregate's constructor and destructor declarations, and put the parsed
   values in the aggregate rather than in bare locals. `scripts/generate_script_loaders.py`
   puts the pair back, so the fix has to go there too.
7. Reference retail's float constants by their `lbl_` names; claim `.rodata` in 8-byte
   slots. The `??(??)` slot is `lbl_<addr of the lis/addi pair>`, and reading the pair needs
   the sign extension in trap 5.
8. Write the loader with `input.Get< uint >()` for the property id, then
   `tools/g2try.sh <unit>` until the diff is empty. `tools/flip_test.sh <unit>.cpp` is the
   acceptance test; expect it to pass on the first or second attempt, and if it does not,
   the residue will be a handful of bytes in one register - see the section above.
