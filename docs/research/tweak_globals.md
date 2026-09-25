# `REL_CreateTweakGlobals`, store by store

Written 2026-09-25 in lane `d5` from the retail module's own disassembly,
`build/G2ME01/Tweaks/asm/MetroidPrime/Tweaks/Tweaks.s`, lines 387-777, cross-checked
against `build/G2ME01/main.elf` with `build/binutils/powerpc-eabi-objdump`. Every address
below is a **module-relative** offset in `Tweaks.rel`'s `.text` unless it is written
`0x8000…`/`0x8041…`, which are DOL addresses. Nothing here is recalled.

**Purpose.** `docs/research/boot_path.md` measures that the wall on the frame loop is
`CGameArchitectureSupport`'s constructor dereferencing `gpTweakPlayerA`
(`lwz r29,-28220(r13)` at 0x80007F38) with no null test. This file answers the
question that follows: *what does retail put there, and is it usable?*

The short answer is in the next section. The short version: `gpTweakPlayerA` does end up
non-null, but it points at a **4-byte heap object holding a pointer**, not at anything with
`CTweakPlayer` members, and the two methods the constructor calls on it
(`GetLeftAnalogMax`, `GetRightAnalogMax`) **have no definition anywhere in this tree**.
Writing this function does not unblock the frame loop, and cannot.

## The verdict, first

| question | answer | evidence |
| --- | --- | --- |
| does `gpTweakPlayerA` end up non-null? | **yes** | store at Tweaks .text **0x78C** |
| what does it point at? | a 4-byte object from `operator new[]`, whose only word is `&gpTweakContents->TweakPlayer` (retail offset **+0x10E8**) | init at 0x7A4-0x7B4 |
| is that a `CTweakPlayer`? | **no** — the header `include/MetroidPrime/Tweaks/CTweakPlayer.hpp` has no data members at all, and its five accessors are undefined in the tree | all five on `docs/research/port_link_gap_list.md:114-118` |
| can the constructor then read a float? | **not from this tree** | `src/MetroidPrime/main.cpp:225-226` calls both accessors; neither exists |
| is `gpTweakContents` sized right for these offsets? | **no** — `sizeof(CTweakContents)` is **0x37D0** here against retail's **0x31F4**, 1,500 bytes too big, and every member from `TweakBall` on is at the wrong offset | see "The header layout disagrees" below |
| is this function even called on the host? | **no** | `REL_CreateTweakGlobals` is reachable only through `STweaks_FuncPtrs::CreateGlobals`; nothing in `src/`, `platform/` or `include/` calls that pointer |
| does it touch `gpGameState` (the *other* null deref, 0x800081A4)? | **no** | the function has no reference to `gpGameState`; `nm` on its object confirms it |

## Shape of the function

`.text:0x00000508`, **size 0x5AC = 1,452 bytes**, 363 instructions, one `r31` callee-saved
register and a 0x10-byte frame. Measured call census:

| callee | count | what it is |
| --- | --- | --- |
| `__nw__FUlPCcPCc` (`operator new[](unsigned long, char const*, char const*)`) | **15** | every allocation |
| `Free__7CMemoryFPCv` (0x802CE388) | 13 | 11 plain `Free` of a slot's old value, 2 at the tail of an inlined 8-byte-element container teardown |
| `fn_802150EC` (0x802150EC, 0x20C) | 2 | constructor of the 248-byte object |
| `fn_82_C4C` (module .text 0xC4C, 0x38) | 1 | constructor of the 52-byte object |
| `fn_82_AB4` (module .text 0xAB4, 0x94) | 1 | constructor of the 604-byte object |
| `fn_82_480` (module .text 0x480, 0x88) | 1 | "destroy the old value of `lbl_80418F3C`, then store 0 into it" |
| `fn_82_304` (module .text 0x304, 0x48) | 1 | the same for `lbl_80418F58`, via a 5-`SLdrSpline` teardown |
| `fn_80216D84` (0x80216D84, 0x74) | 1 | copies 15 floats out of its argument into `.sdata2` |
| `fn_800BC024` (0x800BC024, 0xDC) | 1 | reads `*lbl_80418F28` and calls three float accessors on it |
| `lbl_82_section4_3F0` (via `lis`/`addi`) | 30 | the "file name" argument of every `new` |

### The `new` argument, which is not what it looks like

Every one of the 15 allocations is

```
li   r3, <size>
addi r4, r4, lbl_82_section4_3F0@l
li   r5, 0x0
addi r4, r4, 0xe            ; the pointer is lbl_82_section4_3F0 + 14
bl   __nw__FUlPCcPCc
```

`lbl_82_section4_3F0` is `.rodata` 0x3F0, declared `size:0x18`. Its bytes, from
`objdump -s` of `build/G2ME01/Tweaks/obj/auto_03_00000000_rodata.o`:

```
03f0  5374616e 64617264 2e4e5457 4b003f3f   Standard.NTWK.??
0400  283f3f29 00000000                     (??)....
```

So the object is the 13-character string **`"Standard.NTWK"`** (the tweaks file this module
reads — `REL_LoadTweaks` checks the little-endian tag `0x4e54574b` = `"NTWK"` at 0xD04) with
four bytes of unowned padding after the NUL. `+0xE` points **into that padding, past the
terminator**, at a two-character run dtk prints as `"??"`. The third argument is a literal
`0`. This is MWCC's `__FILE__`/`__LINE__` debug pair: the value is dead, and no source
expression has to reproduce it — only the emitted bytes do.

### The schedule is a statement lag, and it is not reproducible by construction

The emitted order per slot is always

```
Free(slot)              ; destroy whatever the slot held
slot = r31              ; store the object built by the PREVIOUS block
r31 = new ...           ; build the object for THIS slot's member
if (r31) r31->x = ...
```

so the store to slot *n* is emitted **before** the allocation for slot *n*. A source that
reads as "`slot = new T(&member)`" cannot produce that, and neither can any reordering of
one. A `Matching` body is therefore out of reach for this function whatever the modelling
is right; the numbers below are what the shape is worth, not a target.

## `CTweakContents`, as retail lays it out

Measured two ways that agree: `__ct__14CTweakContentsFv` (.text **0x1290**, 0xA8) and
`__dt__14CTweakContentsFv` (.text **0x1184**, 0x10C) call the 16 member constructors at
these offsets, ascending in the ctor and descending in the dtor. Total size comes from
`REL_LoadTweaks` (.text 0xCD4), which does `li r3, 0x31F4` before
`__nw__FUlPCcPCc` and then `__ct__14CTweakContentsFv` on the result.

| member | retail offset | retail size | retail dtor |
| --- | --- | --- | --- |
| `TweakAutoMapper` | 0x0000 | 0x174 | `__dt__19SLdrTweakAutoMapperFv` |
| `TweakBall` | 0x0174 | 0x27C | `__dt__13SLdrTweakBallFv` |
| `TweakCameraBob` | 0x03F0 | 0x048 | `__dt__18SLdrTweakCameraBobFv` |
| `TweakGame` | 0x0438 | 0x0FC | `__dt__13SLdrTweakGameFv` |
| `TweakGui` | 0x0534 | 0x728 | `__dt__12SLdrTweakGuiFv` |
| `TweakGuiColors` | 0x0C5C | 0x44C | `__dt__18SLdrTweakGuiColorsFv` |
| `TweakParticle` | 0x10A8 | 0x040 | `__dt__17SLdrTweakParticleFv` |
| `TweakPlayer` | 0x10E8 | 0x37C | `__dt__15SLdrTweakPlayerFv` |
| `TweakPlayer2` | 0x1464 | 0x37C | `__dt__15SLdrTweakPlayerFv` |
| `TweakPlayerControls` | 0x17E0 | 0x154 | `__dt__23SLdrTweakPlayerControlsFv` |
| `TweakPlayerControls2` | 0x1934 | 0x154 | `__dt__23SLdrTweakPlayerControlsFv` |
| `TweakPlayerGun` | 0x1A88 | 0x798 | `__dt__18SLdrTweakPlayerGunFv` |
| `TweakPlayerGunMuli` | 0x2220 | 0x798 | `__dt__18SLdrTweakPlayerGunFv` |
| `TweakPlayerRes` | 0x29B8 | 0x4F8 | `__dt__18SLdrTweakPlayerResFv` |
| `TweakSlideShow` | 0x2EB0 | 0x078 | `__dt__18SLdrTweakSlideShowFv` |
| `TweakTargeting` | 0x2F28 | 0x2CC | `__dt__18SLdrTweakTargetingFv` |
| **`sizeof(CTweakContents)`** | **0x31F4 (12,788)** | | |

**The declaration order in `include/MetroidPrime/Tweaks/CTweakContents.hpp` is correct** —
all sixteen members, in this order, and the file's names match. Only the sizes are wrong.

## The header layout disagrees, and it is wrong in the direction that matters

Measured by compiling a throwaway probe against the port's own include set
(`g++ -std=gnu++20 -DAURORA -DTARGET_PC -include platform/compat.h -Iplatform/include
-Iextern/aurora/include -Iextern/musyx/include -Iinclude -Iinclude/LZO`) that prints
`offsetof`/`sizeof` per member. Those offsets are what **mwcceppc** would compile
`&gpTweakContents->TweakPlayer` to.

| member | retail offset | tree offset | drift | retail size | tree size | drift |
| --- | --- | --- | --- | --- | --- | --- |
| `TweakAutoMapper` | 0x0000 | 0x0000 | 0 | 0x174 | 0x180 | +0x00C |
| `TweakBall` | 0x0174 | 0x0180 | +0x00C | 0x27C | 0x288 | +0x00C |
| `TweakCameraBob` | 0x03F0 | 0x0408 | +0x018 | 0x048 | 0x050 | +0x008 |
| `TweakGame` | 0x0438 | 0x0458 | +0x020 | 0x0FC | 0x120 | +0x024 |
| `TweakGui` | 0x0534 | 0x0578 | +0x044 | 0x728 | 0x7F0 | +0x0C8 |
| `TweakGuiColors` | 0x0C5C | 0x0D68 | +0x10C | 0x44C | 0x458 | +0x00C |
| `TweakParticle` | 0x10A8 | 0x11C0 | +0x118 | 0x040 | 0x060 | +0x020 |
| **`TweakPlayer`** | **0x10E8** | **0x1220** | **+0x138** | 0x37C | 0x388 | +0x00C |
| `TweakPlayer2` | 0x1464 | 0x15A8 | +0x144 | 0x37C | 0x388 | +0x00C |
| `TweakPlayerControls` | 0x17E0 | 0x1930 | +0x150 | 0x154 | 0x160 | +0x00C |
| `TweakPlayerControls2` | 0x1934 | 0x1A90 | +0x15C | 0x154 | 0x160 | +0x00C |
| `TweakPlayerGun` | 0x1A88 | 0x1BF0 | +0x168 | 0x798 | 0x878 | +0x0E0 |
| `TweakPlayerGunMuli` | 0x2220 | 0x2468 | +0x248 | 0x798 | 0x878 | +0x0E0 |
| `TweakPlayerRes` | 0x29B8 | 0x2CE0 | +0x328 | 0x4F8 | 0x780 | +0x288 |
| `TweakSlideShow` | 0x2EB0 | 0x3460 | +0x5B0 | 0x078 | 0x098 | +0x020 |
| `TweakTargeting` | 0x2F28 | 0x34F8 | +0x5D0 | 0x2CC | 0x2D8 | +0x00C |
| **`sizeof`** | **0x31F4** | **0x37D0** | **+0x5DC** | | | |

Two consequences, both load-bearing:

1. **`gpTweakPlayerA` would point 0x138 bytes past retail's `TweakPlayer`** if anything
   wrote it with the tree's headers. `REL_CreateTweakGlobals` hard-codes retail's `+0x10E8`.
2. **`REL_LoadTweaks`'s `new CTweakContents()` allocates 0x37D0 here, against retail's
   0x31F4**, so every offset-based read of a tweak is wrong in the port, not only the
   pointer.

This is a finding, not something to reconcile by editing the headers: the sizes come from
`SLdrTweak*` member lists in `include/MetroidPrime/ScriptLoader/`, which are **generated**
(`scripts/generate_script_loaders.py`) from field-name hashes, and a lane fixing them
needs the retail `LoadTypedefSLdrTweak*` bodies, not this file.

## The fifteen slots, one row per store

`gpTweakContents` is 0x804193B0. "member" is retail's `CTweakContents` member for the
offset. "registered dtor" is the destructor the DOL's `.ctors` entry 4
(`fn_800324A4`, 0x800324A4..0x80032670) registers for that slot via
`__register_global_object` (0x80344E20).

| # | DOL address | symbol | store at | `new` size | what the object is | member offset | member | registered dtor |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 0x80418F28 | `lbl_80418F28` | 0x568 | 4 | 4-byte holder, word 0 = pointer | 0x0000 | `TweakAutoMapper` | 0x80032BE8 |
| 2 | 0x80418F2C | `lbl_80418F2C` | 0x5B8 | 4 | 4-byte holder | 0x0174 | `TweakBall` | 0x80032B94 |
| 3 | 0x80418F30 | **`gpTweakGame`** | 0x608 | 4 | 4-byte holder | 0x0438 | **`TweakGame`** | 0x80032B40 |
| 4 | 0x80418F34 | `lbl_80418F34` | 0x658 | 4 | 4-byte holder | 0x0534 | `TweakGui` | 0x80032AEC |
| 5 | 0x80418F38 | `lbl_80418F38` | 0x6A8 | 4 | 4-byte holder | 0x0C5C | `TweakGuiColors` | 0x80032A98 |
| 6 | 0x80418F3C | `lbl_80418F3C` | **none** | 52 | `{void* tweak; rstl::string s[3];}` | 0x10A8 | `TweakParticle` | 0x80032A00 |
| 7 | 0x80418F40 | **`gpTweakPlayerB`** | 0x73C | 4 | 4-byte holder | 0x1464 | `TweakPlayer2` | 0x800329AC |
| 8 | 0x80418F44 | **`gpTweakPlayerA`** | 0x78C | 4 | 4-byte holder | 0x10E8 | **`TweakPlayer`** | 0x800329AC |
| 9 | 0x80418F48 | `lbl_80418F48` | 0x7DC | 4 | 4-byte holder | 0x1934 | `TweakPlayerControls2` | 0x80032958 |
| 10 | 0x80418F4C | `lbl_80418F4C` | 0x82C | 4 | 4-byte holder | 0x17E0 | `TweakPlayerControls` | 0x80032958 |
| 11 | 0x80418F50 | `lbl_80418F50` | 0x8E4 | 248 | `{void* tweak; int count; …}` | 0x2220 | `TweakPlayerGunMuli` | 0x800328A8 |
| 12 | 0x80418F54 | `lbl_80418F54` | 0x99C | 248 | same class as #11 | 0x1A88 | `TweakPlayerGun` | 0x800328A8 |
| 13 | 0x80418F58 | `lbl_80418F58` | **none** | 604 | 5 `SLdrSpline` + tweak at +0x258 | 0x29B8 | `TweakPlayerRes` | 0x8003271C |
| 14 | 0x80418F5C | `lbl_80418F5C` | 0xA30 | 4 | 4-byte holder | 0x2EB0 | `TweakSlideShow` | 0x800326C8 |
| 15 | 0x80418F60 | `lbl_80418F60` | 0xA8C | 4 | 4-byte holder | 0x2F28 | `TweakTargeting` | 0x80032674 |
| — | 0x80418F64 | `lbl_80418F64` | 0xA94 | — | **not allocated**; receives the *old* value of `lbl_80418F54`, read at 0xA80 | — | — | none |

Fifteen allocations, fifteen slots, fifteen dtor registrations. `lbl_80418F64` is a
sixteenth `.sbss` word with no registration and no allocation of its own.

### Per-row proof, in address order

| range | what it does |
| --- | --- |
| 0x508-0x540 | prolog; `r31 = new[4]`; `r31[0] = gpTweakContents` (offset 0, a plain `lwz`, not an `addi`) |
| 0x544-0x590 | `Free(lbl_80418F28)`; `lbl_80418F28 = r31`; `r31 = new[4]`; `r31[0] = +0x174` |
| 0x594-0x5E0 | same for `lbl_80418F2C`, offset `0x438` |
| 0x5E4-0x630 | same for **`gpTweakGame`**, offset `0x534` |
| 0x634-0x680 | same for `lbl_80418F34`, offset `0xC5C` |
| 0x684-0x6D4 | `Free(lbl_80418F38)`; `lbl_80418F38 = r31`; `r3 = new[52]`; if non-null `fn_82_C4C(r3, +0x10A8)` |
| 0x6D8-0x714 | **`fn_82_480(&lbl_80418F3C, 0)`** — destroys the old value and stores 0; no store of the new object; then `r31 = new[4]`, `r31[0] = +0x1464` |
| 0x718-0x764 | `Free(gpTweakPlayerB)`; **`gpTweakPlayerB = r31`**; `r31 = new[4]`, `r31[0] = +0x10E8` |
| 0x768-0x7B4 | `Free(gpTweakPlayerA)`; **`gpTweakPlayerA = r31`**; `r31 = new[4]`, `r31[0] = +0x1934` |
| 0x7B8-0x804 | `Free(lbl_80418F48)`; `lbl_80418F48 = r31`; `r31 = new[4]`, `r31[0] = +0x17E0` |
| 0x808-0x860 | `Free(lbl_80418F4C)`; `lbl_80418F4C = r31`; `r3 = new[248]`; if non-null `{r3[0] = +0x2220; r3[1] = 0; fn_802150EC(r3, +0x2220);}` |
| 0x864-0x91C | teardown of `lbl_80418F50`'s old value (inlined, 0x870-0x8CC, reads the count at `obj[1]`, walks it in 8-byte strides); `lbl_80418F50 = r31`; `r3 = new[248]`; if non-null `{r3[0] = +0x1A88; r3[1] = 0; fn_802150EC(r3, +0x1A88);}` |
| 0x91C-0x9CC | teardown of `lbl_80418F54`'s old value (inlined, identical shape); `lbl_80418F54 = r31`; `r3 = new[604]`; if non-null `fn_82_AB4(r3, +0x29B8)` |
| 0x9CC-0xA0C | **`fn_82_304(&lbl_80418F58, 0)`** — destroys the old value (5 `SLdrSpline`) and stores 0; no store of the new object; then `r31 = new[4]`, `r31[0] = +0x2EB0` |
| 0xA0C-0xA5C | `Free(lbl_80418F5C)`; `lbl_80418F5C = r31`; `r31 = new[4]`, `r31[0] = +0x2F28` |
| 0xA5C-0xAB0 | `lbl_80418F60 = r31`; `lbl_80418F64 = <old lbl_80418F54>`; `fn_80216D84(gpTweakContents + 0x3F0)`; `fn_800BC024()`; epilog |

## The three object classes, identified from their own code

### The 4-byte holder — rows 1-5, 7-10, 14, 15

`new[4]` and one `stw` at offset 0. Its *consumer* is the proof of what word 0 means:
`fn_800BC024` (0x800BC024) starts

```
800bc040:  lwz   r3,-28248(r13)   ; 0x80418F28, row 1's slot
800bc044:  bl    8021775c         ; -> f1
800bc04c:  lwz   r3,-28248(r13)
800bc050:  bl    80217768         ; -> f1
800bc058:  lwz   r3,-28248(r13)
800bc05c:  bl    80217774         ; -> f1
```

so word 0 is passed straight on as `this`. It is a **pointer to a tweak struct, used as
`this`**, not a pointer to free.

That leaves a real oddity, and it is worth stating rather than smoothing over: the
registered destructor for row 1 (0x80032BE8) is

```
80032c08:  lwz   r3,0(r30)        ; obj[0] == &gpTweakContents->TweakAutoMapper
80032c0c:  bl    802ce388         ; CMemory::Free
80032c18:  mr    r3,r30
80032c1c:  bl    802ce388         ; CMemory::Free(obj)
```

`CMemory::Free` (0x802CE388) is `OSDisableInterrupts; p->vtable[6](p); OSRestoreInterrupts` —
a straight `free`, with no interior-pointer check. For row 1 the value happens to be the
base of the `CTweakContents` block, so it is a legitimate free; for rows 2, 3, 4, 5, 7, 8, 9,
10, 14, 15 it is `base + 0x174 … base + 0x2F28`, i.e. **an interior pointer**, and
`REL_FreeTweaks` frees the same thirteen slots the same way. Measured, and not explained
away: retail frees interior pointers here. It only runs from `.dtors` and from
`REL_FreeTweaks`, so it is a teardown-time hazard, not a boot-time one.

### The 52-byte class — row 6, `fn_82_C4C` (.text 0xC4C, 0x38)

```
fn_82_C4C(this, tweak):
  this[0x00] = tweak
  this[0x04] = mNull<rstl::basic_string<char>...>;  this[0x08] = 0; this[0x0C] = 0
  this[0x14] = mNull<...>;                          this[0x18] = 0; this[0x1C] = 0
  this[0x24] = mNull<...>;                          this[0x28] = 0; this[0x2C] = 0
```

4 + 3 × 16 = **0x34**, and `0x34` is exactly the `new` size at 0x6A0. So the class is
`{ void* tweak; rstl::string a, b, c; }`, each string initialised to the shared null
sentinel. Its matching teardown is `fn_82_480` (.text 0x480, 0x88), which calls
`internal_dereference__Q24rstl66basic_string<...>` on `obj+0x24`, `obj+0x14`, `obj+0x04`
in that order and then `CMemory::Free(obj)`; the DOL's `fn_80032A00` has the same shape.

### The 248-byte class — rows 11, 12, `fn_802150EC` (0x802150EC, 0x20C)

`obj[0] = tweak; obj[1] = 0; fn_802150EC(obj, tweak)`. `fn_802150EC` reads `obj[1]` as a
count, zeroes it, then walks `obj[0]` at `+0xD0`, `+0xBC` and `+0xB8`:

```
80215104:  lwz   r5,4(r31)          ; the count
80215154:  stw   r0,4(r31)          ; count = 0
80215164:  lwz   r4,0(r31)          ; the tweak
8021516c:  addi  r4,r4,208          ; tweak + 0xD0
80215170:  bl    80218698
80215184:  addi  r4,r4,188          ; tweak + 0xBC
802151a4:  lfs   f1,184(r6)         ; tweak + 0xB8
802151a8:  bl    80215000
802151b4:  bl    802152f8           ; writes into obj+4
```

and its teardown is the inlined count-and-walk at 0x870-0x8CC (identical to the DOL's
`fn_800328A8`). 248 bytes holding a pointer, a count and a built table.

### The 604-byte class — row 13, `fn_82_AB4` (.text 0xAB4, 0x94)

```
fn_82_AB4(this, tweak):
  zero this[0x24], [0x4C], [0x74], [0x80], [0x8C], [0x98], [0xA4], [0xB0], [0xBC]   (9 words)
  __ct__10SLdrSplineFv(this+0x104);  … +0x148; … +0x18C; … +0x1D0; … +0x214        (5 splines)
  this[0x258] = tweak
  fn_80214424(this)
```

`0x258` + 4 = **0x25C**, the `new` size at 0x994. Its teardown is `fn_82_34C`
(.text 0x34C, 0x88) — the same five `__dt__10SLdrSplineFv` offsets in reverse — reached
through `fn_82_304`, and the DOL's registered dtor 0x8003271C calls the equivalent
`fn_80032774`.

## Two allocations with no destination

Rows 6 and 13 build their objects (52 and 604 bytes) and then run the *destroy-old* helper
for the slot — `fn_82_480(&lbl_80418F3C, 0)` and `fn_82_304(&lbl_80418F58, 0)` — whose last
act is `stw r4, 0(r3)`, i.e. **store 0 into the slot**. No `stw` to either slot appears
anywhere else in the function (grep: the only `lbl_80418F6*`/`gpTweak*` stores in the whole
1,452 bytes are at 0x608, 0x73C, 0x78C, 0xA8C and 0xA94).

So on return from `REL_CreateTweakGlobals`, `lbl_80418F3C` and `lbl_80418F58` are **null**,
and their 52- and 604-byte objects are unreachable. Both slots *do* have registered
destructors in the DOL, which will therefore run `fn_80032A00`/`fn_8003271C` against null —
harmless, since both null-check first (`cmplwi r3,0` / `mr. r31,r3; mr. r30,r3; mr. r3,0(r29)`).

Given the statement-lag described above, the likelier reading is that the compiler sank a
store I cannot place, not that retail leaks 656 bytes. Either way, **`lbl_80418F3C` and
`lbl_80418F58` do not end up pointing at anything**, and no slot that matters for the boot
path is affected.

## The last two calls

- **`fn_80216D84(gpTweakContents + 0x3F0)`** — 15 × `lfs`/`stfs` from `arg+0x10 … arg+0x44`
  to `.sdata2` 0x8041A6B0 … 0x8041A730 (`_SDA2_BASE_` 0x804223C0 minus 32016 … 31948). The
  argument is `TweakCameraBob` + 0, so this copies 15 camera-bob constants into a DOL
  global table. **`TweakCameraBob` is the one member that gets no slot of its own.**
- **`fn_800BC024()`** — no argument; reads `*lbl_80418F28` and calls 0x8021775C / 0x80217768
  / 0x80217774 on it, then writes derived floats to `0x80419F40`-ish via `stfsu -11000(r3)`.
  So the last thing the function does is run the auto-mapper over `TweakAutoMapper`.

## What this means for the boot path

`docs/research/boot_path.md` step 17 and "The wall, in one paragraph" should now read:

1. `gpTweakPlayerA` is **not** filled by DOL code. It is filled only here, at Tweaks .text
   0x78C, with a 4-byte object whose word 0 is `&gpTweakContents->TweakPlayer` (retail
   +0x10E8). The finding is that this does **not** make the constructor at 0x80007EC4 able
   to run:
2. `gpTweakContents` is allocated only by `REL_LoadTweaks` (.text 0xCD4, **0x218 = 536
   bytes**), and `REL_CreateTweakGlobals` dereferences it with no null test — so
   `CreateGlobals` must run strictly after `Loader`. `REL_FreeTweaks` (.text 0x8C,
   **0x278 = 632 bytes**) is its counterpart and already has a body in the tree.
3. Nothing in the port calls either. `STweaks_FuncPtrs::CreateGlobals` and `::Loader`
   (`include/MetroidPrime/ScriptLoaderRel.hpp:72`) are assigned in `TweaksInit` and
   invoked by nothing; `mp_relmain_tweaks` only calls `TweaksInit`
   (`platform/compiled_modules.cpp:49`).
4. Even with both called, `CTweakPlayer::GetLeftAnalogMax` and `::GetRightAnalogMax`, which
   `src/MetroidPrime/main.cpp:225-226` calls, are **undefined in the tree** — all five
   `CTweakPlayer` accessors are on the port link-gap ratchet
   (`docs/research/port_link_gap_list.md:114-118`). And `gpTweakPlayerA` would point at a
   4-byte object, not at a `CTweakPlayer`, so those accessors would need the wrapper shape
   the disassembly shows.
5. The **second** null dereference, `gpGameState` at 0x800081A4, is untouched by this
   function. `nm` on `build/G2ME01/Tweaks/obj/MetroidPrime/Tweaks/Tweaks.o` shows no
   reference to `gpGameState`. That one needs `CMain::StreamNewGameState`
   (`src/MetroidPrime/main.cpp:493`) and therefore the paks of step 13.

So the order of work on step 17 is: **model `CTweakPlayer` as the 4-byte wrapper with real
accessor bodies, fix the `SLdrTweak*` sizes, and give the Tweaks module a caller** — not
"write `REL_CreateTweakGlobals`". This function is the last of those four, not the first.

## What was written, and what it scores

`src/MetroidPrime/Tweaks/Tweaks.cpp` now carries the body, under
`#else` of `#ifdef TARGET_PC` (see the host-link note below). Measured:

| | |
| --- | --- |
| objdiff, `REL_CreateTweakGlobals__Fv` | **68.29%**, 1,452 bytes |
| our object's `.text` for the function | **0x4A0 = 1,184 bytes** against retail's 0x5AC |
| range claimed | **none.** The `Tweaks` unit stays `NonMatching` and keeps its whole `0x00000000..0x00001338` claim; `splits.txt` and `configure.py` are unchanged |
| `Tweaks.rel` sha1 | `1c38866b845b17b6c6084d687f5e0d17a28ea72d`, equal to `config.yml`, before and after — **which proves nothing about this code**, because a `NonMatching` unit links `obj/`, not `src/` |
| unit effect on the gate | `matched 3043 -> 3043, linked 1653 -> 1653, no regression`. The three functions already in the unit keep their scores: `__ct__14CTweakContentsFv` 99.95%, `__dt__14CTweakContentsFv` 99.97%, `DecodeAnyTweak` 99.99% |
| unit effect on the port | `tools/link_gap.py --rebuild` **721 before, 721 after**, over 126 objects |

The 268 missing bytes are compiler scheduling, and they are accounted for rather than
guessed. A plain slot is **20 instructions / 80 bytes** in retail, and counting the emission
accounts for the whole difference:

- **The "file name" argument: 2 instructions per site, 15 sites = 30.** Retail emits
  `lis r4, lbl_82_section4_3F0@ha; addi r4, r4, lbl@l; addi r4, r4, 0xe` at every one of the
  15 allocation sites - 30 references to the symbol in the function. mwcceppc
  common-subexpression-eliminates it into `r31` and emits `addi r31,r4,14; mr r4,r31` once,
  so each site pays one instruction instead of three. No source expression undoes this:
  `lbl_82_section4_3F0` is the module's own `.rodata` and that pool is **unsplittable** - it
  lives entirely in the FORCEACTIVE `auto_03_00000000_rodata.o` base object - so a
  `Matching` unit here must not contribute `.rodata`, and the only way to get a fresh literal
  back is the `NEW` macro, which introduces one. A hard cap on the percentage, not a
  modelling gap.
- **`gpTweakContents`: 1 instruction per site, 15 sites = 15.** Retail reloads it with
  `lis r3, gpTweakContents@ha; addi r3, r3, gpTweakContents@l; lwz r3, 0(r3)`; ours drops the
  `addi` because both relocations land in the same word. Hoisting the pointer into a local
  was tried and made things *worse* (1,044 bytes) because retail reloads it, so the source
  must not - the instruction is lost to the compiler either way.
- **One extra callee-saved register: about 22 instructions.** Retail keeps the allocation
  result in `r31` across the intervening `CMemory::Free` and needs nothing else. This source
  needs `r30` as well - the newly built object is live across the `Free` - which adds the
  `stw r30`/`lwz r30` pair plus a register shuffle per slot.

**`__nw__` vs `__nwa__` is a one-line finding worth keeping.** The 15 allocations in retail
call `__nw__FUlPCcPCc`, i.e. `operator new(unsigned long, char const*, char const*)` — the
**scalar** new. Writing `new (file, 0) T[1]` makes mwcceppc emit
`__nwa__FUlPCcPCc` (`operator new[]`) and objdiff pairs nothing, because the symbol name is
part of the call target. `new (file, 0) T` — no array subscript — is what reproduces it, and
it also removes the array-construction loop, which is why it is 140 bytes closer than the
`[1]` form.

## The host cannot compile the body, and must not be asked to

The body uses mwcceppc's three-argument `new T(file, line)` and names thirteen DOL `.sbss`
slots (`lbl_80418F28` .. `lbl_80418F64`) that exist only in `main.dol`. Left unguarded, the
**port build fails to compile** — measured, not predicted:

```
/usr/include/c++/15/new:206:7: error: operator new(size_t, void*)
/usr/include/c++/15/new:206:7: note: operator new expects 2 arguments, 3 provided
ninja: build stopped: subcommand failed.
error: could not build mp_game, mp_platform, mp_port_entry in /tmp/opencode/d5/build-port
```

and the same declarations would add thirteen undefined symbols to the port link. So the
file carries an empty body under `#ifdef TARGET_PC` and the real one under `#else`, which
is the arrangement `src/MetroidPrime/main.cpp` already uses for `CMain::RsMain`. Because
nothing on the host calls this function, the empty body is not a behaviour change: it is
reachable only through `STweaks_FuncPtrs::CreateGlobals`, which `TweaksInit` assigns and
nothing invokes.

## The neighbours, for whoever takes them next

Both are in `config/G2ME01/rels/Tweaks/symbols.txt`:

| function | module `.text` | size | state in this tree |
| --- | --- | --- | --- |
| `REL_FreeTweaks__Fv` | 0x0000008C | **0x278 = 632 bytes** | has a body (`delete gpTweakGame; gpTweakGame = nullptr;`), scores **8.80%** |
| `REL_LoadTweaks__FR12CInputStream` | 0x00000CD4 | **0x218 = 536 bytes** | has a body, scores **62.49%** |

`REL_FreeTweaks` is the more tractable of the two and is largely mapped by the table above.
It is `__dt__14CTweakContentsFv(gpTweakContents, 1)` and `gpTweakContents = 0`, then fifteen
tear-downs in slot order with the same statement lag `REL_CreateTweakGlobals` has — it frees
slot *n* and zeroes slot *n-1*:

| order | slot | how it is torn down |
| --- | --- | --- |
| 1 | `lbl_80418F28` … `lbl_80418F38` | `CMemory::Free`, five in a row (0xC4-0x134) |
| 2 | `lbl_80418F3C` | `fn_82_480(&lbl_80418F3C, 0)` at 0x150 — three `rstl::string`s |
| 3 | `gpTweakPlayerB`, `gpTweakPlayerA`, `lbl_80418F48`, `lbl_80418F4C` | `CMemory::Free`, four in a row (0x160-0x1B4) |
| 4 | `lbl_80418F50` | inlined count-and-walk teardown, then `Free` (0x1D4-0x228) |
| 5 | `lbl_80418F54` | the same inlined teardown, then `Free` (0x244-0x298) |
| 6 | `lbl_80418F58` | `fn_82_304(&lbl_80418F58, 0)` at 0x2B8 — five `SLdrSpline`s |
| 7 | `lbl_80418F5C`, `lbl_80418F60` | `CMemory::Free`, two in a row (0x2C8-0x2E4) |

632 bytes of `lis`/`addi`/`lwz`/`Free` and no logic at all, and **`lbl_80418F64` is never
touched** — which is consistent with it holding an already-freed pointer. The one thing to get
right is the lag: "free the next slot, zero the previous one" is what retail emits, so the
source has to keep a local across each `Free`, exactly as `REL_CreateTweakGlobals` does.

`REL_LoadTweaks` is the harder one and the `Tweaks` `LoadTypedef*` switches are already on the
known-hard list in `docs/RUNNING_THE_DECOMP.md`.

## Reproducing the measurements

```sh
export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
python3 configure.py --version G2ME01 --compilers $MP_TOOLCHAIN_DIR/build/compilers \
  --dtk $MP_TOOLCHAIN_DIR/build/tools/dtk --wrapper $MP_TOOLCHAIN_DIR/build/tools/wibo --build-dir build
./tools/decomp_build.sh
# the function itself
awk '/^\.fn REL_CreateTweakGlobals__Fv/,/endfn REL_CreateTweakGlobals__Fv/' \
  build/G2ME01/Tweaks/asm/MetroidPrime/Tweaks/Tweaks.s
# the rodata string
build/binutils/powerpc-eabi-objdump -s -j .rodata build/G2ME01/Tweaks/obj/auto_03_00000000_rodata.o
# the 15 slots and their registered destructors
build/binutils/powerpc-eabi-objdump -d --start-address=0x800324A4 --stop-address=0x80032680 build/G2ME01/main.elf
# the header layout (probe source inlined; it need not live in the tree)
cat > /tmp/ctc_probe.cpp <<'EOF'
#include "MetroidPrime/Tweaks/CTweakContents.hpp"
#include <cstdio>
#include <cstddef>
#define P(T) printf("%-24s off=0x%04zx size=0x%03zx\n", #T, \
                   offsetof(CTweakContents, T), sizeof(((CTweakContents*)0)->T))
int main() {
  printf("sizeof(CTweakContents) = 0x%zx\n", sizeof(CTweakContents));
  P(TweakAutoMapper);   P(TweakBall);          P(TweakCameraBob);
  P(TweakGame);         P(TweakGui);           P(TweakGuiColors);
  P(TweakParticle);     P(TweakPlayer);        P(TweakPlayer2);
  P(TweakPlayerControls);  P(TweakPlayerControls2);
  P(TweakPlayerGun);    P(TweakPlayerGunMuli); P(TweakPlayerRes);
  P(TweakSlideShow);    P(TweakTargeting);
  return 0;
}
EOF
g++ -O0 -std=gnu++20 -DAURORA -DTARGET_PC -include platform/compat.h \
  -Iplatform/include -Iextern/aurora/include -Iextern/musyx/include -Iinclude -Iinclude/LZO \
  /tmp/ctc_probe.cpp -o /tmp/ctc_probe && /tmp/ctc_probe
```

The probe is a throwaway: it is not in `files.cmake` and `configure.py` never sees it, so
it costs the port build and the matching build nothing.
