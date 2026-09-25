# What retail holds at the game globals and sentinel addresses

Measured 2026-09-25 from `build/G2ME01/main.elf` (a linked ELF), cross-checked against
`build/G2ME01/main.dol` at the matching file offset and against `config/G2ME01/symbols.txt`.
All nine are defined for the PC link in `src/MetroidPrime/PortGlobals.cpp`; this is the
evidence behind the values there and the reason that file is not a `configure.py` unit.

## How to read an address in the DOL

Two things are needed and neither is obvious from the map:

- The small-data bases are **`r13` = 0x8041FD80** (`_SDA_BASE_` in the ELF) and
  **`r2` = 0x804223C0** (`_SDA2_BASE_`). The compiler uses r13 for `.sbss` and r2 for
  `.sdata2`/`.rodata`, which is the reverse of what the labels suggest. Every address below
  was found by grepping the disassembly for the offset that lands on it, then confirmed by
  reading the store.
- Section file offsets are *not* the VMAs. `.rodata` is at VMA 0x803A56C0 / file offset
  0x3A27A0, `.sdata2` at 0x8041A3C0 / 0x3C3C20. `objdump -h` on `main.elf` prints both.

## The three sentinels: found, not guessed

`fn_800E9BF4` (`.text:0x800E9BF4`, 0x20 bytes) is the only writer of any of them anywhere
in the image - one `stw`/`sth` each, nowhere else. It is registered as an initialiser by
being the second word of `.ctors` entry 13 (`.ctors` is at `.data:0x803A54A0`, 8 bytes per
entry, so 0x803A5508 holds `fn_800E8B48` and then this):

```
800e9bf4:  3c 60 00 01  lis    r3,1             ; r3 = 0x00010000
800e9bf8:  38 80 ff ff  li     r4,-1            ; r4 = 0xFFFFFFFF
800e9bfc:  38 03 ff ff  addi   r0,r3,-1         ; r0 = 0x0000FFFF
800e9c00:  90 8d 93 a0  stw    r4,-27744(r13)   ; 0x80419120  kInvalidEditorId = -1
800e9c04:  b0 0d 93 a4  sth    r0,-27740(r13)   ; 0x80419124  kInvalidUniqueId  = 0xFFFF
800e9c08:  90 8d 93 a8  stw    r4,-27736(r13)   ; 0x80419128  kInvalidAreaId    = -1
800e9c0c:  90 8d 93 ac  stw    r4,-27732(r13)   ; 0x8041912C  (unnamed, also -1)
800e9c10:  4e 80 00 20  blr
```

The map's `size:0x2` for `kInvalidUniqueId` and `size:0x4` for the other two is what the
`sth` versus the two `stw` predict, so the widths are confirmed from two directions.
A fourth sentinel at 0x8041912C is set to -1 by the same function and is still unnamed; it
is the next thing to do in this area.

Nothing in the tree contradicts these values - and nothing needed correcting, because the
port routes every use through the named constants. The only other 0xFFFF in the tree,
`InvalidSfxId` in `TGameTypes.hpp:86` and `CSfxManager::kInternalInvalidSfxId`, are an
*audio* sfx id in the same numeric range, not the unique-id sentinel; leave them.

**A hazard, not a bug to fix:** `kInvalidUniqueId` has two declarations with the same link
name and different C++ types - `extern const TUniqueId` in `TGameTypes.hpp:16` and
`extern "C" const unsigned short` in `CScriptWallCrawler.cpp:5`, whose object really does
carry `U kInvalidUniqueId`. Both are 2 bytes at offset 0 so the value is the same, and the C
spelling is not gratuitous: `CScriptWallCrawler.cpp` is a **`Matching`** REL unit, so its
declaration cannot be changed without risking its bytes. The two spellings have to coexist
until someone reconciles them, and whoever adds that file to `files.cmake` is the first to
compile both in one program. Nothing in the port build compiles it today.

## gpRender: a second name for a singleton the port already owns

`.sbss:0x804192F8`, 4 bytes, **written exactly once in the whole DOL**, at 0x80008468 in
`CGameGlobalObjects::PostInitialize` (`.text:0x800083E0`):

```
80008434:  bl     AllocateRenderer__FR12IObjectStoreR10COsContextR10CMemorySysR8IFactory
80008460:  stw    r31,328(r29)        ; CGameGlobalObjects::x148
80008464:  lwz    r0,328(r29)
80008468:  stw    r0,-27272(r13)      ; gpRender
```

So `gpRender` is not an independent object: it is `CGameGlobalObjects`' own renderer member
under a second name. 449 `lwz` sites read it. The port's `CGameGlobalObjects::PostInitialize`
already performs exactly this store, so nothing had to be decided - the definition is
`nullptr` and the existing assignment fills it.

## The gpTweak* pointers: the Tweaks REL module owns them

No DOL code ever assigns any of them. The only DOL code that touches them is `fn_800324A4`
(0x800324A4..0x80032670, the first word of `.ctors` entry 4), which stores 0 into **fifteen**
of them and registers a destructor for each:

```
800324fc:  stw r0,-28240(r13)   ; 0x80418F30 gpTweakGame      + dtor 0x80032B40
8003256c:  stw r0,-28224(r13)   ; 0x80418F40 gpTweakPlayerB   + dtor 0x800329AC
80032588:  stw r0,-28220(r13)   ; 0x80418F44 gpTweakPlayerA   + dtor 0x800329AC
```

`__register_global_object` (0x80344E20) is a three-instruction push onto a global list of
12-byte `{next, dtor, slot}` records, so this is a plain at-exit destructor registration and
nothing more. `gpTweakPlayerA` and `gpTweakPlayerB` share one destructor: one tweak class,
instantiated twice.

The objects come from the **Tweaks REL module**, in `REL_CreateTweakGlobals`
(`Tweaks.rel .text:0x508`, 0x5AC = 1452 bytes) - fifteen
"operator new(size, file, 0) -> store into the global slot" sequences, with sizes 4 eleven
times, 52, 248 twice and 604. Fifteen allocations against fifteen registrations, which is
worth having: it is what makes "these are the tweak singletons and nothing else is" a
measurement rather than an assumption. To disassemble it, the module's text starts at **file
offset 0xA4** of `Tweaks.rel` (find it from the `RELExit` / `RELMain` / `TweaksInit`
prologues at 0xA4, 0xC8, 0xE8) and objdump needs `-EB`, or every word decodes misaligned.

`gpTweakContents` (`.sbss:0x804193B0`) has **no DOL reference at all** - only the Tweaks
module touches it, and the port's `Tweaks.cpp::REL_LoadTweaks` already does the
`new CTweakContents` that retail's loader does.

DOL load-site counts, for scale: gpTweakGame 9, gpTweakPlayerA 7, gpTweakPlayerB 2,
gpTweakManager 10.

`gpTweakPlayerGun`, `gpTweakPlayerGunMulti` and `gpTweakPlayerGunSingle` are **not in the
DOL's symbol table at all**: they are Tweaks-module BSS, so no retail address exists for
them. Only `gpTweakPlayerGun` has a user in the port (`CPlayerGun.cpp`).

## BuildTime: a `const char*` pointing at the build stamp

`.sdata2:0x8041D550`. The map's `size:0x8` is the distance to the next symbol, not the
object's width: the relocation against it is `R_PPC_ADDR32` (4 bytes), and the eight bytes at
the address are that pointer followed by a zero word. The proof of the value is a relocation,
not a disassembly: retail's `auto_11_8041D400_sdata2.o` covers `.sdata2`
0x8041D400..0x8041E250, defines `BuildTime` at section offset 0x150, and carries

```
00000150 R_PPC_ADDR32      BuildString
```

`BuildString` is `.rodata:0x803AC3C6`, in the **unclaimed** block `auto_06_803A9620_rodata.o`
at section offset 0x2DA6, and those bytes are
`"Build v1.028 10/18/2004 10:44:32\0AD\0\0\0"`, sitting immediately after `MetroidBuildInfo`
at 0x803AC3B0 (offset 0x2D90, `"!#$MetroidBuildInfo!#$\0"`). The whole stamp is one literal
and `BuildTime` points 22 bytes into it.

So `BUILD_TIME_DUMMY` in `RAssertDolphin.hpp` - the port's own transcription of that literal -
is the same 31 visible bytes, and `%s` prints exactly what retail prints. Worth being precise
about one thing here, because it is easy to assume the other way: **no `Matching` unit emits
this literal.** It lives in an unclaimed `.rodata` block, so `BUILD_INFO` is a transcription
rather than something the build already reproduces, and this definition is the one place it
becomes load-bearing. (Checked by dumping `.rodata` of both `src/…/RAssertDolphin.o` and
`obj/…/RAssertDolphin.o`: neither contains `!#$MetroidBuildInfo!#$`.)

Nothing in the DOL stores to it. The DOL reads it **twice** and writes it zero times, both
reads in `ErrorHandler` (0x8028C3F4, a `Matching` unit):
`lwz r4,-20080(r2)` at 0x8028C634 and `lwz r5,-20080(r2)` at 0x8028C778 -
0x804223C0 - 20080 = 0x8041D550. The port's `RAssertDolphin.cpp` is the same Matching unit
and already references `BuildTime`, so retail's reader and the port's reader are the same
code.

The earlier guess that this was dead data was wrong, and the way to be sure was to read the
DOL as well as the ELF: the DOL's `.sdata2` is at file offset 0x3C3B40, not the ELF's
0x3C3C20, and reading the ELF's mapping with a swapped byte order is what makes 0x803AC3C6
look like 0xC6C33A80.

## Why the definitions are in a file configure.py does not claim

Two distinct failure modes, both measured, and the second is the one that is easy to miss:

1. **A `Matching` unit collides at link time.** `RAssertDolphin.cpp` is the only user of
   `BuildTime` and it is `Matching`, so a definition there would be a second definition of a
   symbol the retail object also defines. This is a hard link error, not a percentage.
2. **A `NonMatching` unit does not collide but does get worse.** mwcceppc gives a new
   small-data symbol an SDA slot, which shifts the offsets of every other small-data access
   in the object. Adding *only* the one line `const char* BuildTime = BUILD_TIME_DUMMY;` to
   `MetroidPrime/main.cpp` moved `__ct__CGameArchitectureSupport` 84.51% -> 81.54% and
   `AddWorldPaks` 96.00% -> 95.97% (`StreamNewGameState` improved 19.29% -> 25.90%, and the
   unit average rose 27.24% -> 27.29%, so the average hides it). `tools/gate.sh` reports the
   per-function losses as `regression` and fails. The unit's `.text` also grows by 36 bytes
   and every function after the insertion point moves by 4.

A file `configure.py` never claims is invisible to all of it. That is the whole reason
`src/MetroidPrime/PortGlobals.cpp` exists, and the same reasoning applies to the 20
`lbl_804*` retail globals still outstanding: they want the same file, not a unit.
