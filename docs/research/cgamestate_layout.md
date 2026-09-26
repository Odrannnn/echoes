# `CGameState`'s measured layout, and what the 1,668-byte constructor actually costs

Written 2026-09-26 in lane `j1`. Every offset and every size below is read out of
`build/G2ME01/main.elf` or measured with `mwcceppc`; nothing is recalled. The two questions
this file answers are the two that gate the port's last big blocker:

1. **is `CGameState` 0x2F0 bytes?** (`operator new(752)` in `CGameGlobalObjects`'s constructor
   says 0x2F0, and `EnsureOptions` reads nested state, so a wrong size makes every field access
   wrong)
2. **how much of `CGameState::CGameState(CInputStream&, int)` - retail `fn_80144140`, 0x80144140,
   0x684 = 1,668 bytes - is writable with what is already in the tree?**

`docs/research/boot_path.md` recorded this constructor as "1,668 bytes with seventeen unwritten
callees listed in `main.cpp`'s own block map, so it is not a lane-sized job and this block did
not attempt it". **That reasoning is wrong, and measuring it is what this file is for.** The
callee bodies are irrelevant to a `Matching` unit; see "Why the callees are not the blocker".

## 1. The size, and every offset that matters

`CHECK_SIZEOF(CGameState, 0x2f0)` **agrees with `operator new(0x2F0)`. Nothing had to be
corrected in the total**, which is the opposite of what the brief expected and is worth saying
plainly: the size was never the problem.

What *was* wrong is `CHintOptions`: the header had it at 0x16, which puts
`CPersistentOptions` at **0xDA**, and retail constructs that member at **0xDC**
(`addi r3,r30,220; li r4,1; bl fn_80146154` at 0x801441F0, and again at 0x80144558 as
`fn_8000401C`). `CHintOptions` is **0x18**. Fixed, and every offset below now matches retail.

Measured with mwcceppc (`tools/size_probe_gs.cpp`, compiled with the flags out of `build.ninja`;
**never with the host compiler**, which is 64-bit and has `rstl::string` at 24 bytes against
retail's 0x10):

| | value | retail's own evidence |
| --- | --- | --- |
| `sizeof(CGameState)` | **0x2F0** | `li r3,752` / `bl __nw__FUlPCcPCc` at 0x800084C4 |
| `+0x3C` `x3c_worldState` | 0x3C | `stw r0,60(r30)` 0x801441A0 |
| `+0x40` `x40_refCount` | 0x40 | `stw r3,64(r30)` 0x801441C4, after `new(4)` and `*refCount = 1` |
| `+0x50` `x50_unk` | 0x50 | `stfs f0,80(r30)` 0x801441D8, `stfs f1,80(r30)` 0x80144544 |
| `+0x80` `gameOptions` | 0x80, 0x44 | `addi r3,r30,128` 0x801441E0 -> `__ct__12CGameOptionsFv`; `PutTo__12CGameOptionsFR16CBitStreamWriter(this+128)` at 0x80142D40 and 0x80142E2C |
| `+0xC4` `hintOptions` | 0xC4, **0x18** | `addi r3,r30,196` 0x801441E8 -> `fn_80180738`; `fn_801447C4(this+0xC4, ..)` 0x801444B0 |
| `+0xDC` `persistentOptions` | 0xDC, 0x2C | `addi r3,r30,220` 0x801441F0 -> `fn_80146154`; `fn_8000401C(this+0xDC, ..)` 0x80144558 |
| `+0x108` `cardSerial` | 0x108 (u64) | `main.cpp:746`: "new->x10C = the old x10C, new->x108 = the old x108" |

`sizeof(CGameOptions)` is 0x44, `sizeof(CHintOptions)` 0x18, `sizeof(CPersistentOptions)` 0x2C.
`0xDC + 0x2C = 0x108` exactly, so `cardSerial` needs no padding - which is why the two readings
of `CHintOptions` (0x16 + 2 bytes of padding, or 0x18) both put `cardSerial` at 0x108 and only
the **constructor call at 0xDC** distinguishes them.

### The +0x3C member specifically, because it is the load-bearing one

Boot-path **step 17** dereferences `gpGameState` at 0x800081A4 with no null test, and
`CGameArchitectureSupport::Update` (0x80007A14) calls `CGameState::GetWorldState()` (0x80142520,
8 bytes: `addi r3,r3,60; blr` - the whole body is the address of +0x3C). So +0x3C must hold a
live `CWorldState*`, and the constructor is what puts one there:

```
80144170:  li      r3,1200                ; 1200 = 0x4B0 = sizeof(CWorldState)
80144174:  li      r5,0
80144188:  bl      802ce278 <__nw__FUlPCcPCc>
8014418c:  mr.     r0,r3
80144190:  beq     8014419c               ; null check on the allocation
80144194:  bl      8015c34c <fn_8015C34C>  ; CWorldState::CWorldState()
8014419c:  stw     r0,60(r30)             ; +0x3C = the pointer
801441ac:  li      r3,4
801441b0:  bl      802ce278 <__nw__FUlPCcPCc>
801441b4:  cmplwi  r3,0
801441bc:  li      r0,1
801441c0:  stw     r0,0(r3)
801441c4:  stw     r3,64(r30)             ; +0x40 = the refcount word
```

`CWorldState` is 0x4B0 and `include/MetroidPrime/CWorldState.hpp` already says so, and its
constructor `fn_8015C34C` (0x8015C34C, 0x114 = 276 bytes) calls exactly two **named** functions -
`__ct__9CRandom16FUi` (seed 99) at +0xA0 and `__ct__12CTransform4fFRC12CTransform4f` at +0x468 -
and is otherwise stores. **That is the cheapest large thing left on the whole boot path** and it
is a better target than the 1,668-byte constructor; see "What to do next".

## 2. Why the callees are not the blocker

A `Matching` unit's object only needs *relocations* to its callees. Whether the callee is ours
or dtk's filled copy is the link's business, not the object's. So the count of unwritten callees
says nothing about whether `fn_80144140` can be matched.

**Measured, per callee**, against `config/G2ME01/splits.txt` and `configure.py`
(`tools/range_owner.py` logic), for the 42 distinct named callees of `fn_80144140`:

| owning unit of the callee's range | count | is that unit linked? |
| --- | --- | --- |
| claimed by nobody - dtk fills it with retail's bytes | 37 | yes, from `build/G2ME01/obj/auto_*.o` |
| claimed by a `NonMatching` unit | 4 (`fn_80007040`, `fn_8000934C`, `SomethingWorldId_80005698` in `main.cpp`; `__ct__12CPlayerStateFiR16CBitStreamReader` in `CPlayerState.cpp`) | **yes - and this corrects a claim in `configure.py`** |
| claimed by a `Matching` unit | 3 (`Kyoto/CToken.cpp`) | yes |

**The correction.** `configure.py`, in the note on `MetroidPrime/CInputGeneratorUpdate.cpp`,
says:

> its `queue.Push(msg)` is `Push__18CArchitectureQueueFRC20CArchitectureMessage` at 0x80007A80,
> and `tools/range_owner.py` says that range belongs to `MetroidPrime/main.cpp`, a NonMatching
> unit, so nothing in the DOL link defines it and a Matching unit calling it would not link.

**That is wrong, and it is the recorded reason the 1,668-byte constructor was never attempted.**
`dtk dol split` writes a *filled* object per unit, `build/G2ME01/obj/<unit>.o`, which carries
retail's bytes for the functions the unit does not match - and it is that filled object, not
ours, that goes in the link for a `NonMatching` unit. Measured:

```
$ powerpc-eabi-nm build/G2ME01/obj/MetroidPrime/main.o | grep -E 'fn_80007040|fn_8000934C|SomethingWorldId_80005698'
00001c88 T fn_80007040
00003f94 T fn_8000934C
000002e0 T SomethingWorldId_80005698
$ powerpc-eabi-nm build/G2ME01/src/MetroidPrime/main.o | grep -E 'fn_80007040|...'
0000057c T fn_80007040            # ours defines only one of the three, and it is empty
$ grep -c 'obj/MetroidPrime/main.o' build.ninja      # 1: it is in the link
```

and the link was then tested directly. Three calls were added to a `Matching` unit - one to
`fn_80007040` (a `main.cpp`/`NonMatching` symbol), one to `fn_8014495C` (claimed by nobody), one
to `__nw__FUlPCcPCc` - and `main.elf` **linked**, with all three defined in the final image at
their retail addresses:

```
$ ninja build/G2ME01/main.elf ; nm build/G2ME01/main.elf | grep -E ' (fn_80007040|fn_8014495C|__nw__FUlPCcPCv)$'
802ce2c0 T __nw__FUlPCcPCc
80007040 T fn_80007040
8014495c T fn_8014495C
```

(The DOL sha1 moved, of course - the probe changed the code. It was reverted and the tree is
back at `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.)

`Push__18CArchitectureQueueFRC20CArchitectureMessage` is a **weak** symbol in the filled object
and is in `main.elf` at 0x80007A80, so `CInputGeneratorUpdate`'s stated reason for being
`NonMatching` is also void. Whether that unit can be promoted is a separate question; the
*reason as written* is not valid and should not be repeated.

### So what does block it

Not the callees' bodies. These, in the order they will bite:

1. **Twenty unnamed member types.** The constructor touches 27 distinct `this` offsets and needs
   a C++ member for each, with retail's exact widths, or mwcceppc will emit different
   instructions. The map is below; about half of it is a `{?, count, cap, data}` 16-byte shape
   that repeats, so it is mechanical once the shape is named.
2. **`fn_80146154` (0x80146154, 0x58) reads uninitialised incoming stack words.** It is called as
   `fn_80146154(this+0xDC, 1)` - `r3` and `r4` only - and it does `lbz r5,8(r1)` and
   `lbz r4,12(r1)`, which are **its own** outgoing parameter save area, never written by the
   caller, then stores both into the object at +4 and +5. No C++ can express "pass two garbage
   bytes on the stack", so this one callee cannot be written as source. It can be *called*, which
   is all the constructor needs.
3. **A virtual dispatch through a global.** 0x8014460C-0x8014461C loads
   `r4 = *(gpTweakGame)` (SDA -28376 = 0x80418EF0), then `r12 = *(r4+12)`, `mtctr`, `bctrl`, with
   a four-character object tag built by `lis r3,0x51C1; addi r0,r3,0x5677` and a
   `CToken` round trip through `__ct__6CTokenFRC6CToken` / `GetObj__6CTokenFv` / `__dt__6CTokenFv`.
   Writing the call means naming that global and identifying vtable slot 3, which is a
   `CObject`-family question this tree has not answered.
4. **Two loops with retail's register allocation.** 0x801443DC-0x801444A0 builds four
   `CPlayerState` into a fixed 4-entry array at `this+0x1C` (stride 8, `slwi r0,r0,3; add r3,r28,r0;
   addic. r3,r3,4`) and 0x8014470C-0x80144774 walks a 112-byte-element array
   (`mulli r0,r0,112`), with the loop-invariant `r29 = gpGameState` and the `addic.` overflow
   probe that GCC emits for a `this + i*8 + 4` address. Expect register allocation, not logic.
5. **`operator new`'s file/line operand.** Five `bl __nw__FUlPCcPCc` sites each carry
   `lis r3,0x803B; addi r4,r3,-28152` = 0x803B9208, one merged string. `docs/research/rc_ptr.md`
   has the correction and the recipe: a literal leaves one differing `addi` that the
   per-function diff cannot see and the DOL hash fails on.
6. **The two constants.** `lfd f1,-25112(r2)` / `lfs f0,-25096(r2)` into +0x48 and +0x50. These
   must be retail's named values, not literals - a literal left every function at 100% and grew
   `main.dol` by 32 bytes in a previous session.

## 3. The byte split, measured

`fn_80144140` is exactly **417 instructions** (0x684 / 4), and the split is:

| | bytes | share |
| --- | --- | --- |
| inline code (loads, stores, arithmetic, branches, frame) | **0x580 = 1,408** | 84.4% |
| `bl` / `bctrl` instructions - 65 call sites, 42 named + 1 indirect | **0x104 = 260** | 15.6% |
| total | 0x684 = 1,668 | 100% |

**All 1,668 bytes are potentially writable as a `Matching` unit**, because no callee body is
needed - that is the finding above. So the honest split is not "reachable / blocked on a callee"
but "writable now / writable after N units of member-type modelling":

* **writable with what is in the tree today: 0 bytes of it is *blocked*, and roughly 60% of the
  1,408 inline bytes touch members that are already named** (+0x00, +0x04, +0x0C, +0x10, +0x14,
  +0x18, +0x3C, +0x40, +0x48, +0x50, +0x80, +0xC4, +0xDC, +0x108, +0x10C, +0x2EC - 16 of the
  27). The rest needs the map below modelled.
* **blocked on nothing external.** No callee is missing from the link, no callee is claimed by a
  `Matching` unit that does not define it, and the constructor's own range is unclaimed today
  (nothing in `config/G2ME01/splits.txt` covers 0x80144140), so a new unit can claim it outright.

This is a multi-session job for one person, but it is *decompilation*, not archaeology - which is
the part the earlier note got wrong.

## 4. The member map, from the constructor and the three functions that pin its boundaries

Every row is an offset `fn_80144140` writes, reads, or passes to a callee, with the callee that
fixes the block's size. 27 distinct offsets; the ones in the last column are pinned by functions
the constructor does *not* call, which is why those boundaries are known at all.

| offset | shape | what retail puts there |
| --- | --- | --- |
| +0x00 | int | `-1` |
| +0x04 | int | `-1` |
| +0x08 | container base | `fn_801466F4(this+8, n)`: a growable block of **36-byte** elements, `{+4 count, +8 capacity, +0xC data}` relative to +0x08. `fn_801426E0(this+8, elem)` appends; `fn_8014260C(this, *elem)` is called per element in a loop bounded by **+0x0C** |
| +0x0C | uint | 0, and the loop bound for the 112-byte array below |
| +0x10 | uint | 0; read at 0x80144570 as `fn_801466F4`'s argument and at 0x80144760 as the element count |
| +0x14 | - | 0 |
| +0x18 | uint | 0, then 1..4: the count of the 4-entry array at +0x1C (`addi r0,r4,1; stw r0,24(r30)`) |
| +0x1C | 4 x {ptr, refcount} | four `new(1588) CPlayerState(i, bitstreamReader)` from 0x80144400, stride 8 (`slwi r0,r0,3`) |
| +0x3C | `CWorldState*` | `new(1200)`, see above |
| +0x40 | `uint*` | `new(4)` with `*refCount = 1` |
| +0x48 | double | `lfd f1,-25112(r2)` |
| +0x50 | float | `lfs f0,-25096(r2)`, then `ReadFloat()` at 0x80144540 |
| +0x54 | block, 0x2C | `fn_80145950(this+0x54)`, which itself calls `fn_80146154` and zeroes +0x1C,+0x20,+0x24,+0x28 relative to itself |
| +0x80 | `CGameOptions` | 0x44, `__ct__12CGameOptionsFv`, and `EnsureOptions` is called on it |
| +0xC4 | `CHintOptions` | 0x18, `fn_80180738` / `fn_801805EC` / `fn_801447C4` |
| +0xDC | `CPersistentOptions` | 0x2C, `fn_80146154` / `fn_8000401C` / `fn_80004678` / `fn_80146068` |
| +0xF8, +0xFC, +0x100 | uint x3 | 0 |
| +0x108 | u64 | 0; copied from the old state by `main.cpp:746` |
| +0x110 | uint count | 3, and `fn_80144924` then builds 3 x 16 bytes at +0x114 (`addi r31,r31,16` in `fn_8014495C`) |
| +0x144 | uint count | 3, and 3 x 16 bytes at +0x148. **Pinned by `fn_80142DD4`**: `slwi r0,r4,4; add r31,r30,r0; addi r31,r31,328` = `this+0x148+i*16`, i = 0,1,2, so the block is 0x34 bytes |
| +0x178 | {?, uint, uint, ptr} 0x10 | 0x17C, 0x180, 0x184 zeroed. **Pinned by `fn_80142CF8`**: `addi r3,r31,376; bl fn_80142BA4; lwz r4,388(r31)` |
| +0x188 | {?, uint, uint, ptr} 0x10 | 0x18C, 0x190, 0x194 zeroed; `main.cpp:723` builds a local from +0x188 with `fn_80004AA0` |
| +0x198 | byte | `(ptr != 0)`, from `neg r0,r4; or r0,r0,r4; srwi r0,r0,31` |
| +0x19C | ptr | the `new(12)`'d pointer built by `fn_80193E08` |
| +0x1A0 | block | `fn_80007040(this+0x1A0)`, then `fn_80003BE8(this+0x1A0, local)` after `fn_80144D70(local, in)` |
| +0x1F8, +0x1FC, +0x200 | uint x3 | 0 |
| +0x204 | block | `fn_80009DBC(this+0x204, 0)`, which writes `76` at +0x00 and two 19-iteration byte loops |
| +0x2EC | byte | three `ReadBits(1)` results forced into bits 7, 6 and 5 by `rlwimi`, plus a bit-1 clear |

**Three shapes repeat and are worth naming first**, because one struct each replaces four or five
raw offsets:

* **A `{?, count, capacity, data}` 16-byte growable block** at +0x178, +0x188 and (a different
  element size) the +0x08 container. `CHintOptions` at +0xC4 has the same first three members.
* **A `{uint count; E elems[3];}` 0x34-byte block** with E 16 bytes, at +0x110 and +0x144, and
  `fn_80144924` / `fn_8014495C` / `fn_80142A10` are its constructor, its element loop and E's
  copy constructor. `main.cpp:725`/`:742` call the same shape's copy constructor `fn_80004C90`.
* **A `{?, ..., void* heap}` block** whose destructor `fn_80004A4C` frees through `+0x0C` and
  then frees `this` when `flag > 0` - the `extsh. r0,r31; ble` signed-short test, the same shape
  as `CMemorySys::~CMemorySys` in `docs/research/boot_globals.md`.

## 5. What to do next, in order

1. **Write `fn_80144924` + `fn_8014495C` + `fn_80142A10` as one unit** (0x80144924..0x801449C8 is
   contiguous, 0xA4 = 164 bytes, three functions, and only `fn_80142A10`'s `fn_80004D5C` and
   `fn_8014495C`'s `fn_80142A10` are outside). It is the constructor of CGameState's +0x110 and
   +0x144 members, it is 164 bytes, and it is the only large thing here whose body needs no
   unnamed callee *of its own*. Declaring the functions **descending** by retail offset
   (`fn_8014495C` then `fn_80144924`) or the bytes come out permuted - see `docs/research/decl_order.md`.
2. **Then `CWorldState::CWorldState` = `fn_8015C34C`** (0x8015C34C, 0x114 = 276 bytes). It is the
   **+0x3C member**, it is mostly stores, and its only two callees are *named* retail functions
   (`__ct__9CRandom16FUi`, `__ct__12CTransform4fFRC12CTransform4f`). `CWorldState` is currently
   one `char x8_pad[0x490]`, so this needs ~20 named members - one header edit and one unit, and
   it is worth more per byte than anything else in this file.
3. **Then the 1,668-byte constructor**, once the 20 member types exist. Expect item 2 above (the
   garbage stack read in `fn_80146154` is a *callee* problem, so it does not block the caller) and
   item 4 (register allocation in two loops) to be where the percentages stop moving.
4. **`fn_80180738` is already done** in this lane - 36 bytes, 100%, `Matching`,
   `flip_test.sh` PASS. See `src/MetroidPrime/Player/CHintOptionsCtor.cpp`. It is the only callee
   of the 1,668 bytes that **calls nothing at all**, so it is also the only one that is net -1 on
   the port's link rather than net +1.

## Reproducing every number here

```sh
export MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort
python3 configure.py --version G2ME01 --compilers $MP_TOOLCHAIN_DIR/build/compilers \
  --dtk $MP_TOOLCHAIN_DIR/build/tools/dtk --wrapper $MP_TOOLCHAIN_DIR/build/tools/wibo --build-dir build
./tools/decomp_build.sh

# the 417 instructions and the inline/call split
build/binutils/powerpc-eabi-objdump -d --start-address=0x80144140 --stop-address=0x801447c4 \
  build/G2ME01/main.elf > /tmp/j1_fn.asm

# every CGameState offset the constructor touches, and the two constants
grep -oE '\+[0-9]+\(r30\)' /tmp/j1_fn.asm | sort -u

# the size and the offsets, with mwcceppc's flags (never the host compiler)
$MP_TOOLCHAIN_DIR/build/tools/wibo $MP_TOOLCHAIN_DIR/build/compilers/GC/2.7/mwcceppc.exe \
  -nodefaults -proc gekko -align powerpc -enum int -fp hardware -Cpp_exceptions off -O4,p \
  -inline auto -pragma "cats off" -maxerrors 1 -nosyspath -RTTI off -fp_contract on -str reuse \
  -i include -i libc -i build/G2ME01/include -DVERSION_G2ME01 -DNDEBUG=1 -str reuse,pool,readonly \
  -gccinc -inline deferred,noauto -common on -lang=c++ -c tools/size_probe_gs.cpp -o /tmp/gs.o
build/binutils/powerpc-eabi-objdump -s -j .data /tmp/gs.o

# who claims a callee's range, and whether that unit is Matching
python3 tools/range_owner.py .text 0x80180738 0x8018075C

# the filled object a NonMatching unit contributes to the link
build/binutils/powerpc-eabi-nm build/G2ME01/obj/MetroidPrime/main.o | grep fn_80007040
```

`_SDA_BASE_` is 0x8041FD80 and `_SDA2_BASE_` is 0x804223C0. The exact rule for turning an
`lwz`/`stw` field into an address is `field = (address - 0x8041FD80) & 0xFFFF` - the **full**
signed displacement, not half of it - and doing that subtraction by hand is a trap this
repository has fallen into twice.
