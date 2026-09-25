# `CTweakPlayer`: the 4-byte cell, its five accessors, and the size-drift table that was a 64-bit artifact

Written 2026-09-26 in lane `e4` from `build/G2ME01/main.elf`, `config/G2ME01/symbols.txt` and
two purpose-built probes. Every address, size and offset below is measured; nothing is recalled.

**What this is for.** `docs/research/boot_path.md` records the wall on the frame loop:
`CGameArchitectureSupport`'s constructor loads `gpTweakPlayerA` at 0x80007F38 and calls two
methods on it at 0x80007F40 and 0x80007F4C with no null test, and
`docs/research/tweak_globals.md` records that the thing retail puts there is a **4-byte heap cell
holding a pointer**, not a `CTweakPlayer` with members. This file is the rest of that answer: the
cell's model, retail's own five accessors, what each of them reads, and **a correction to
`tweak_globals.md`'s size-drift table that changes what the next lane has to do.**

## The verdict, first

| question | answer |
| --- | --- |
| What is `CTweakPlayer` in retail? | **one pointer.** 4 bytes, `new[4]` in `REL_CreateTweakGlobals`, freed pointer-then-cell by the slot's registered destructor |
| How many accessors does retail define for it? | **five, all named in `config/G2ME01/symbols.txt`, 12 bytes each** - the premise that they are "not retail-named functions" is wrong. Six more are called but unnamed |
| Do the five reproduce retail's bytes from this tree? | **yes, all five, 12/12 bytes, verified by compiling them with mwcceppc and comparing against the DOL** |
| Is this tree's `SLdrTweakPlayer` laid out like retail's? | **yes, byte for byte** - two retail `lbz` thunks land on two named `bool` members, `sizeof` is 0x37C, and all fifteen members are at retail's offsets |
| Then is the "`CTweakContents` is 1,500 bytes too large" finding real? | **no. It is an artifact of measuring struct layout with a 64-bit host compiler.** The 32-bit figure is 0x3244 against retail's 0x31F4: **+0x50, not +0x5DC** |
| What *is* wrong with the generated headers? | **one member.** `SLdrTweakPlayerRes` is 0x548 against retail's 0x4F8. That single +0x50 is the whole `CTweakContents` drift, and it starts at `TweakSlideShow` |
| What still faults on the boot path? | both null dereferences. `gpTweakPlayerA` is still `nullptr` (nothing calls `REL_CreateTweakGlobals`), and `gpGameState` needs the paks |

## The cell

`REL_CreateTweakGlobals` (Tweaks.rel `.text:0x508`) allocates 15 objects, 11 of them 4 bytes, and
stores a pointer to a `CTweakContents` member in word 0 of each. For `gpTweakPlayerA` the store is
at Tweaks `.text:0x78C` and the pointer is `&gpTweakContents->TweakPlayer`; `gpTweakPlayerB` is
`TweakPlayer2` at 0x73C; `gpTweakGame` is `TweakGame` at 0x608. Retail offsets **+0x10E8**,
**+0x1464** and **+0x438** - all three confirmed against this tree's 32-bit layout below.

The registered destructor for the two player slots is one address, 0x800329AC, for two
instantiations, and every destructor in this table is the same free-a-member-then-free-self pair
(`lwz r3,0(r30); bl CMemory::Free; mr r3,r30; bl CMemory::Free`). That is what makes word 0 a
pointer to free rather than something with a destructor of its own - and, as
`tweak_globals.md` measured, it means retail frees interior pointers for thirteen of the fifteen
slots. That is teardown behaviour and not this file's subject.

So the model is one pointer, and the class is:

```cpp
class CTweakPlayer {
public:
  SLdrTweakPlayer* mTweak;
  float GetLeftAnalogMax();
  float GetRightAnalogMax();
  float GetVariaSuitDamageReduction();
  float GetDarkSuitDamageReduction();
  float GetLightSuitDamageReduction();
};
```

`include/MetroidPrime/Tweaks/CTweakPlayer.hpp` now says this, with the evidence in the comment.
`src/MetroidPrime/PortGlobals.cpp` carries the five bodies - that file is deliberately not a unit
in `configure.py`, so nothing in the matching build moves (measured: `matched 3115 -> 3115,
linked 1725 -> 1725`).

## Retail's five accessors

All five are in `config/G2ME01/symbols.txt` under the DOL's own naming convention
(`Name__12CTweakPlayerFv`, not a mangled `CTweakPlayer::` name - which is why a grep for
`CTweakPlayer::` finds nothing and they look unnamed). All five are `size:0xC` and live in the
`main/auto_03_80213CB8_text` unit.

| symbol | retail | bytes | reads | member | what calls it |
| --- | --- | --- | --- | --- | --- |
| `GetLightSuitDamageReduction` | 0x80217D30 | 12 | **+0x378** | `suitDamageReduction.light` | - |
| `GetDarkSuitDamageReduction` | 0x80217D3C | 12 | **+0x374** | `suitDamageReduction.dark` | - |
| `GetVariaSuitDamageReduction` | 0x80217D48 | 12 | **+0x370** | `suitDamageReduction.varia` | - |
| `GetRightAnalogMax` | 0x802184CC | 12 | **+0x1AC** | `misc.rightAnalogMax` | `__ct__CGameArchitectureSupport` 0x80007F40 |
| `GetLeftAnalogMax` | 0x802184D8 | 12 | **+0x1A8** | `misc.leftAnalogMax` | `__ct__CGameArchitectureSupport` 0x80007F4C |

Every one of them is the same three instructions, and the first is the proof that word 0 of the
cell is used as `this`:

```
802184cc:  lwz   r3,0(r3)
802184d0:  lfs   f1,428(r3)        ; 428 = 0x1AC
802184d4:  blr
```

`lwz r3,0(r3)` is not a vtable load and not an SDA load: it is `this->mTweak`, followed by a
load at a fixed offset in the tweak. Nothing else in the DOL is consistent with a different shape.

### The accessor family is bigger than the five, and it is the layout's best evidence

`gpTweakPlayerA` has seven load sites in the DOL and only two of them call one of the five. The
other four call **unnamed** thunks of the same kind, and `gpTweakPlayerB` is called through a fifth:

| caller site | thunk | shape | reads | member, per the 32-bit layout |
| --- | --- | --- | --- | --- |
| 0x800614F8 | `fn_80218530` | `lwz; lfs f1,-19140(r2); lfs f0,392(r3); fmuls; blr` | **+0x188** x a `.sdata2` constant | - |
| 0x8011956C | `fn_80217DE4` | `lwz; lfs; blr` | **+0x320** | - |
| 0x8013895C | `fn_8021856C` | `lwz; lfs; blr` | **+0x174** | - |
| 0x80138980 | `fn_80217E08` | `lwz; lfs; blr` | **+0x314** | - |
| 0x80040F90 | `fn_80217DFC` | `lwz; lfs; blr` | **+0x318** | - |

So retail's `CTweakPlayer` has at least **eleven** float accessors and this header declares five.
The other six are `fn_*` in `symbols.txt`, nothing in the port calls them, and declaring them would
be invention.

`CPlayer::GetTweakPlayer() const` is the *named* consumer, and it is what the three suit accessors
go through in this tree: `src/MetroidPrime/CStateManager.cpp:359-368` calls
`player->GetTweakPlayer()->GetVariaSuitDamageReduction()` and its two siblings. Retail defines
`GetTweakPlayer__7CPlayerCFv` at **0x8000BF94, 24 bytes**, and its whole body is

```
8000bf94:  lwz   r0,4896(r3)          ; 4896 = 0x1320, CPlayer's "player two" byte
8000bf98:  lwz   r3,-28220(r13)       ; gpTweakPlayerA
8000bf9c:  cmpwi r0,1
8000bfa0:  bnelr
8000bfa4:  lwz   r3,-28224(r13)       ; gpTweakPlayerB
8000bfa8:  blr
```

so it is `this->x1320 == 1 ? gpTweakPlayerB : gpTweakPlayerA`. It is still undefined here and still
on the ratchet (`_ZNK7CPlayer14GetTweakPlayerEv`), and it is the last undefined symbol in the chain
these five accessors sit in. It is `CPlayer`'s, not this lane's.

The whole region 0x80216C00-0x80218800 is a **thunk farm**, and it is not all `CTweakPlayer`: **283**
three-instruction float accessors live there, plus `lbz` and `lwz` ones, and the same shape serves
every one of the 4-byte-holder slots, so the offsets repeat across classes (0x80217104 and
0x802184E4 both read +0x1A4, 0x802170F8 and 0x802184D8 both read +0x1A8). **A
`lwz r3,0(r3); lfs f1,N(r3); blr` in that range is not evidence about any particular class** - only a
call site naming the global settles it. That is why this file reads offsets off the call sites and
not off the farm. The family is not only floats either: `lbz` and `lwz` thunks are in it, and
`fn_80218530` is five instructions with an `.sdata2` multiply.

**Two byte accessors settle the layout question, and they are the reason to believe the 32-bit
table below.** `fn_80218578` does `lwz r3,0(r3); lbz r3,368(r3); blr` — **+0x170** — and
`fn_802184F0` does `lwz r3,0(r3); lbz r3,416(r3); blr` — **+0x1A0**. Those are `bool` members, and
mwcceppc puts `SLdrTweakPlayer_Motion::gravityBoostMultipleAllowed` at **0x170** and
`SLdrTweakPlayer_Misc::nullAnalogScales` at **0x1A0**. Two independent retail reads land on two
named members that a *generated* header invented the names for, one of them through a load whose
width (`lbz`, not `lwz`) says the member is a bool and not a float. `fn_802184E4` then reads
**+0x1A4**, which is `misc.unknown_0xfb909bc3`, the word between the bool and `leftAnalogMax`:

| retail accessor | reads | tree member, mwcceppc 32-bit |
| --- | --- | --- |
| `fn_80218578` | +0x170, `lbz` | `motion.gravityBoostMultipleAllowed` |
| `fn_802184F0` | +0x1A0, `lbz` | `misc.nullAnalogScales` |
| `fn_802184E4` | +0x1A4 | `misc.unknown_0xfb909bc3` |
| `GetLeftAnalogMax` | +0x1A8 | `misc.leftAnalogMax` |
| `GetRightAnalogMax` | +0x1AC | `misc.rightAnalogMax` |
| `fn_80217D54` | +0x36C | - |
| `GetVariaSuitDamageReduction` | +0x370 | `suitDamageReduction.varia` |
| `GetDarkSuitDamageReduction` | +0x374 | `suitDamageReduction.dark` |
| `GetLightSuitDamageReduction` | +0x378 | `suitDamageReduction.light` |

The whole region 0x80216C00-0x80218800 is a **thunk farm of 283 such accessors**, and it is not all
`CTweakPlayer`: the same shape serves every one of the 4-byte-holder slots, and the offsets repeat
across classes (0x80217104 and 0x802184E4 both read +0x1A4, 0x802170F8 and 0x802184D8 both read
+0x1A8). **A `lwz r3,0(r3); lfs f1,N(r3); blr` in that range is not evidence about any particular
class** - only a call site naming the global settles it. That is why this file reads offsets off
the call sites and not off the farm. The family is not only floats either: `lbz` and `lwz` thunks
are in it, and `fn_80218530` is five instructions with an `.sdata2` multiply.

## The size-drift table in `tweak_globals.md` is superseded

`tweak_globals.md` measured the header layout with

```sh
g++ -O0 -std=gnu++20 -DAURORA -DTARGET_PC -include platform/compat.h -Iplatform/include ... /tmp/ctc_probe.cpp
```

and reported `sizeof(CTweakContents)` **0x37D0** against retail's 0x31F4, with the drift already
**+0x138** at `TweakPlayer`, and concluded that "the generated `SLdr*` headers make it 0x37D0, and
every member from `TweakBall` on is at the wrong offset". **That measurement is a 64-bit-host
artifact and the conclusion is wrong.** The port build is `ELF 64-bit` (measured on
`build-port/CMakeFiles/mp_port_entry.dir/platform/main.cpp.o`), and on x86-64 every
`rstl::string` is `{const char*, control*, uint, rmemory_allocator}` = 8+8+4+1 = **24 bytes**,
not retail's **16**: `sizeof(rstl::string)` is 0x18 under that `g++` and 0x10 under mwcceppc.
Fourteen strings is 0x50 per struct, and the generated structs are full of them.

Measured both ways, same tree, same headers:

| | 64-bit host `g++` | **32-bit mwcceppc** | retail |
| --- | --- | --- | --- |
| `sizeof(CTweakContents)` | 0x37D0 | **0x3244** | 0x31F4 |
| `offsetof(CTweakContents, TweakPlayer)` | 0x1220 | **0x10E8** | 0x10E8 |
| `sizeof(SLdrTweakPlayer)` | 0x388 | **0x37C** | 0x37C |
| `offsetof(SLdrTweakPlayer, misc)` | 0x17C | **0x174** | 0x174 |
| `offsetof(SLdrTweakPlayer, suitDamageReduction)` | 0x378 | **0x370** | 0x370 |
| `offsetof(SLdrTweakPlayer, misc)+offsetof(Misc,leftAnalogMax)` | 0x1B0 | **0x1A8** | 0x1A8 (the thunk) |
| `sizeof(SLdrTweakPlayerRes)` | 0x780 | **0x548** | 0x4F8 |
| `sizeof(rstl::string)` | 0x18 | **0x10** | 0x10 |

The 32-bit column is confirmed independently by our own retail-matching code: the
mwcceppc-compiled `__ct__14CTweakContentsFv` in `build/G2ME01/src/MetroidPrime/Tweaks/Tweaks.o`
emits `addi r3,r31,0x10e8` for `TweakPlayer`, `0x1464` for `TweakPlayer2` and `0x29b8` for
`TweakPlayerRes` - retail's offsets, from the same headers d5 measured.

**All sixteen `CTweakContents` members, 32-bit, against retail:**

| member | retail off | 32-bit off | drift | retail size | 32-bit size | drift |
| --- | --- | --- | --- | --- | --- | --- |
| `TweakAutoMapper` | 0x0000 | 0x0000 | 0 | 0x174 | 0x174 | 0 |
| `TweakBall` | 0x0174 | 0x0174 | 0 | 0x27C | 0x27C | 0 |
| `TweakCameraBob` | 0x03F0 | 0x03F0 | 0 | 0x048 | 0x048 | 0 |
| `TweakGame` | 0x0438 | 0x0438 | 0 | 0x0FC | 0x0FC | 0 |
| `TweakGui` | 0x0534 | 0x0534 | 0 | 0x728 | 0x728 | 0 |
| `TweakGuiColors` | 0x0C5C | 0x0C5C | 0 | 0x44C | 0x44C | 0 |
| `TweakParticle` | 0x10A8 | 0x10A8 | 0 | 0x040 | 0x040 | 0 |
| **`TweakPlayer`** | **0x10E8** | **0x10E8** | **0** | 0x37C | 0x37C | 0 |
| `TweakPlayer2` | 0x1464 | 0x1464 | 0 | 0x37C | 0x37C | 0 |
| `TweakPlayerControls` | 0x17E0 | 0x17E0 | 0 | 0x154 | 0x154 | 0 |
| `TweakPlayerControls2` | 0x1934 | 0x1934 | 0 | 0x154 | 0x154 | 0 |
| `TweakPlayerGun` | 0x1A88 | 0x1A88 | 0 | 0x798 | 0x798 | 0 |
| `TweakPlayerGunMuli` | 0x2220 | 0x2220 | 0 | 0x798 | 0x798 | 0 |
| `TweakPlayerRes` | 0x29B8 | 0x29B8 | 0 | 0x4F8 | **0x548** | **+0x50** |
| `TweakSlideShow` | 0x2EB0 | **0x2F00** | **+0x50** | 0x078 | 0x078 | 0 |
| `TweakTargeting` | 0x2F28 | **0x2F78** | **+0x50** | 0x2CC | 0x2CC | 0 |
| **`sizeof`** | **0x31F4** | **0x3244** | **+0x50** | | | |

`tweak_globals.md` also wrote that `REL_LoadTweaks`'s `new CTweakContents()` "allocates 0x37D0
here, against retail's 0x31F4, so every offset-based read of a tweak is wrong in the port". Under
mwcceppc it allocates 0x3244 and **fourteen of the sixteen members are at retail's offsets**; the
remaining error is one member's size, and it is e1's to fix from the `LoadTypedefSLdrTweakPlayerRes`
body. The general warning - a host-compiled `offsetof` is not what the compiler sees - stands, and
is the reusable part of this correction.

### The dependency on lane e1, restated

**For these five accessors: none.** They are byte-exact today (below), and they do not move when
`SLdrTweakPlayerRes` is fixed, because `TweakPlayer` ends at 0x1464 and `TweakPlayerRes` starts
at 0x29B8.

**For the Tweaks singleton bring-up as a whole: one member.** `SLdrTweakPlayerRes` is 0x548
against retail's 0x4F8, i.e. 20 words too many, and every offset from `TweakSlideShow` on is
therefore +0x50. `SLdrTweakPlayerRes` is two sub-structs of `rstl::string` members
(`_AutoMapperIcons` has 14, `_MapScreenIcons` has 32) plus `_GunResources` and
`_BallTransitionResources`, so 0x50 is five strings' worth - either five members the generator
invented, or one sub-struct carrying five words of filler. The retail `LoadTypedefSLdrTweakPlayerRes`
body settles it, and it is not needed for `gpTweakPlayerA`.

## The bodies, and the proof they are retail's bytes

`src/MetroidPrime/PortGlobals.cpp`, written against **named members** - not one raw offset, so
`tools/check_raw_offsets.py` is untouched and there is no kind-B debt to retire later:

```cpp
float CTweakPlayer::GetLeftAnalogMax() { return mTweak->misc.leftAnalogMax; }
float CTweakPlayer::GetRightAnalogMax() { return mTweak->misc.rightAnalogMax; }
float CTweakPlayer::GetVariaSuitDamageReduction() { return mTweak->suitDamageReduction.varia; }
float CTweakPlayer::GetDarkSuitDamageReduction() { return mTweak->suitDamageReduction.dark; }
float CTweakPlayer::GetLightSuitDamageReduction() { return mTweak->suitDamageReduction.light; }
```

Compiled with the `Tweaks` unit's own flags (mwcceppc GC/1.3.2, 32-bit), they emit retail's
**symbol names** - mwcceppc derives `GetLightSuitDamageReduction__12CTweakPlayerFv` from this
signature, which is why `config/G2ME01/symbols.txt` has those names - and each body is 12 bytes:

```
00000000 <GetLightSuitDamageReduction__12CTweakPlayerFv>:   80630000 c0230378 4e800020
0000000c <GetDarkSuitDamageReduction__12CTweakPlayerFv>:    80630000 c0230374 4e800020
00000018 <GetVariaSuitDamageReduction__12CTweakPlayerFv>:   80630000 c0230370 4e800020
00000024 <GetRightAnalogMax__12CTweakPlayerFv>:             80630000 c02301ac 4e800020
00000030 <GetLeftAnalogMax__12CTweakPlayerFv>:              80630000 c02301a8 4e800020
```

compared instruction-by-instruction and byte-for-byte against `build/G2ME01/main.elf`:
**all five identical, 12/12 bytes each.** (Order is reverse source order, as `LANE.md` says mwcceppc
emits; retail has the same order inside each of its two runs.)

### The `Matching` unit this could become, as an intended `configure.py` change

Not applied here - this lane does not edit `configure.py` - but measured, because it is five or six
functions that can only be had this way and the recipe is short:

1. Two new sources, e.g. `src/MetroidPrime/Tweaks/CTweakPlayerAnalog.cpp` and
   `.../CTweakPlayerSuit.cpp`, each holding the definitions **in descending retail-offset order**
   within its run.
2. Two `Object(Matching, "MetroidPrime/Tweaks/CTweakPlayerAnalog.cpp")` and
   `Object(Matching, "MetroidPrime/Tweaks/CTweakPlayerSuit.cpp")` lines in the `Tweaks` lib's
   object list (configure.py:819-821).
3. The claimed ranges are 0x802184CC..0x802184E4 (24 bytes, two functions) and
   0x80217D30..0x80217D54 (36 bytes, three functions). dtk re-splits the rest of
   `auto_03_80213CB8_text` around them.

Both ranges are contiguous runs of named 12-byte functions, so nothing unnamed has to be written.
The acceptance test is `tools/flip_test.sh` plus the DOL sha1 and the 86 REL hashes; the risk to
watch is `unit_fit.sh` reporting an extra emitted function, which cannot happen here because the
object defines nothing but the five.

## What still faults

1. **`gpTweakPlayerA` is `nullptr`.** `src/MetroidPrime/PortGlobals.cpp:158` still defines it as
   `nullptr`, and the only writer is `REL_CreateTweakGlobals`, reachable only through
   `STweaks_FuncPtrs::CreateGlobals`, which `TweaksInit` assigns and nothing invokes. Five
   undefined symbols are gone; the null dereference at 0x80007F38 is not. This is boot-path step
   17's item (c), "give the Tweaks module a caller", and it is a different lane.
2. **`gpGameState` is `nullptr`** at 0x800081A4. Needs `CMain::StreamNewGameState` and therefore
   the paks of step 13. Not reachable from the Tweaks module at all - `nm` on the Tweaks object
   shows no reference to it.
3. **The 64-bit host has a layout of its own.** The port build is 64-bit, so *on PC* every
   `SLdr*`/`CTweakContents` offset computed by the host compiler is wrong even where the mwcceppc
   layout is right - `rstl::string` is 24 bytes there. That is a property of running 32-bit game
   structs on a 64-bit host, it is the same class of problem as `PORT_NOTES.md`'s first finding
   about `OSModuleHeader`, and it is not something a lane can fix inside a tweak header. Until
   something addresses it, `GetLeftAnalogMax` returns a wrong number on the host even though its
   bytes are right on the console. **`CTweakPlayer` is not special here; it is the first place it
   became observable.**

## Reproducing the measurements

```sh
export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
./tools/gate.sh                     # matched 3115 -> 3115, linked 1725 -> 1725

# the two thunks the boot path calls, and their bytes
build/binutils/powerpc-eabi-objdump -d --start-address=0x80007f38 --stop-address=0x80007f60 \
  build/G2ME01/main.elf
build/binutils/powerpc-eabi-objdump -d --start-address=0x802184cc --stop-address=0x802184f0 \
  build/G2ME01/main.elf
build/binutils/powerpc-eabi-objdump -d --start-address=0x80217d30 --stop-address=0x80217d60 \
  build/G2ME01/main.elf

# the five names, in config/G2ME01/symbols.txt
grep -n 'CTweakPlayer' config/G2ME01/symbols.txt

# THE 32-BIT LAYOUT PROBE.  This is the reusable part of this file: a struct layout
# measured with a host compiler is not what mwcceppc compiles.  Emit offsetof/sizeof
# as .data and read them out of the object, because the target cannot be run.
mkdir -p /tmp/probe && cat > /tmp/probe/ctc.cpp <<'EOF'
#include "MetroidPrime/Tweaks/CTweakContents.hpp"
#include <stddef.h>
#define SZ(T) ((unsigned int)sizeof(((CTweakContents*)0)->T))
#define OF(T) ((unsigned int)offsetof(CTweakContents, T))
extern "C" unsigned int g_probe[] = {
  (unsigned int)sizeof(CTweakContents), OF(TweakPlayer), SZ(TweakPlayer),
  OF(TweakPlayer2), SZ(TweakPlayer2), OF(TweakPlayerRes), SZ(TweakPlayerRes),
  OF(TweakSlideShow), SZ(TweakSlideShow), OF(TweakTargeting), SZ(TweakTargeting),
  (unsigned int)sizeof(rstl::string),
};
EOF
$MP_TOOLCHAIN_DIR/build/tools/wibo build/tools/sjiswrap.exe \
  $MP_TOOLCHAIN_DIR/build/compilers/GC/1.3.2/mwcceppc.exe \
  -nodefaults -proc gekko -align powerpc -enum int -fp hardware -Cpp_exceptions off -O4,p \
  -inline auto -pragma "cats off" -pragma "warn_notinlined off" -maxerrors 1 -nosyspath \
  -RTTI off -fp_contract on -str reuse -i include -i libc -i build/G2ME01/include \
  -DBUILD_VERSION=0 -DVERSION_G2ME01 -multibyte -DNDEBUG=1 -use_lmw_stmw on \
  -str reuse,pool,readonly -gccinc -inline deferred,noauto -common on -sdata 0 -sdata2 0 \
  -lang=c++ -c /tmp/probe/ctc.cpp -o /tmp/probe/          # the cflags are Tweaks.o's, from build.ninja
build/binutils/powerpc-eabi-nm -S /tmp/probe/ctc.o | grep g_probe   # find the array's offset
build/binutils/powerpc-eabi-objdump -s -j .data /tmp/probe/ctc.o
```

The same probe with host `g++` reproduces `tweak_globals.md`'s numbers exactly, which is how the
artifact was identified rather than argued about.
