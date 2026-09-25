# The first real link attempt

The port's link gap was, until now, a number derived from `nm` set arithmetic
(`tools/link_gap.py`). This records an actual `ld.bfd` run over the real
executable target, which is better evidence and found things the arithmetic
could not see.

Reproduce it with the two commands in `PORT_NOTES.md` under "Two builds exist".

## What it establishes

| | |
|---|---|
| Aurora configures from this tree | yes, ~20 s, it fetches its own SDL3 and Dawn |
| Game units that compile | **118 of 118**, zero compile errors |
| Unique undefined symbols at link | **562** (was 732 at the first attempt) |
| Duplicate definitions at link | **0** — was 4, resolved; see the section below |
| Binary produced | no — the link fails, so the port does not boot yet |

So the port is not "blocked on an unimplemented build system". The build system
works, every game source compiles, and the whole remaining problem is 562
symbols. The structural issue is gone.

## Cross-checking `link_gap.py` against the linker

`link_gap.py` reported **724** at the time. The linker reported **727**. The deltas are not
noise and are worth recording, because the arithmetic tool is now validated
against ground truth and its blind spot is known:

**Ten symbols the linker wants that `link_gap.py` did not list:**

| symbol | why `nm` missed it |
|---|---|
| `_ctors`, `_dtors` | linker-synthesised section symbols; never in any object's symbol table |
| `AIInitDMA`, `AIStartDMA`, `AIGetDMAStartAddr`, `AIRegisterDMACallback` | no definition existed anywhere, in the port or in Aurora |
| `vtable for CPlayer`, `vtable for CCollidableAABox`, `vtable for CSimplePool`, `typeinfo for CGunWeapon` | a vtable is only *emitted* by the TU that defines the class's key function. While that key function is unwritten, no object contains the vtable at all, so `nm` has nothing to count. |

**One entry `link_gap.py` listed that the linker did not want:**
`CPlayer* TCastToPtr<CPlayer>(CEntity*)` — a weak template instantiation the
linker resolved. Stale ratchet entry.

The four vtable/typeinfo entries are the important ones: **a class whose key
function is not yet written is invisible to the gap list**, and closing it needs
the key function, not a vtable definition. Do not "fix" these by emitting a vtable
by hand.

## Two port bugs the gap arithmetic could never have found

**`_ctors`/`_dtors` in `src/REL/REL_Setup.cpp`.** The unit walked the GameCube's
linker-generated `_ctors`/`_dtors` tables. Those do not exist in an ELF link, so
the host build referenced two symbols that cannot resolve. Fixed with a
`#ifdef`-guarded host branch walking `__init_array_start`/`__init_array_end`; the
`__MWERKS__` branch is untouched, so the unit still matches retail byte for byte.
This is a **port** bug, not a decompilation gap — the retail code is correct and
was always going to be.

**`platform/ai_dma.cpp` was never compiled.** It existed in the tree, fully
written, and nothing built it, so the port was missing all five AI DMA entry
points. It is now its own target, `mp_port_audio`, because it is the one port
source that is not engine-independent: it includes SDL3, which only exists after
Aurora is added. Aurora's `dolphin/ai.h` *declares* `AIInitDMA`, `AIStartDMA`,
`AIGetDMAStartAddr` and `AIRegisterDMACallback` and implements **none** of them —
the port owns all five. `AIStartDMA` was the one the file did not have; it is
now there, delegating to the same play-state path the rest of the file uses.

## Negative result: `src/Dolphin/*.c` cannot be built for the host

`configure.py` configures all four (`PPCArch.c`, `ai.c`, `db.c`, `dtk.c`) but
`files.cmake` names none of them, and that turns out to be correct rather than an
oversight. They are GameCube register shims written as assembly-in-C: `u32`
typedefs, inline PPC `asm`, MMIO pokes. Adding all four to the port build gives
**15 compile errors** and no objects. On the host, `platform/ai_dma.cpp` and
`platform/shims.cpp` replace them. Do not retry this.

## The remaining structural blocker: `RELMain`/`RELExit` — RESOLVED

The first link reported 4 duplicate definitions, all of them the REL module entry points. **Fixed:
the link reports 562 undefined and zero duplicates.** How, and why the obvious fixes were wrong:

**The scale was worse than the linker showed.** `ld.bfd` stops at the first collision, so it named
three modules. There are **14 translation units that define a `RELMain`** — one per REL module we
have reimplemented:

```
CSwarmBasicsREL  CFlyerSwarmRel  CScriptCoinRel  CScriptForgottenObject  CScriptMetaree
CScriptPlayerActorMain  CScriptPlayerProxy  CScriptPufferRel  CScriptRiftPortal
CScriptRsfAudio  CScriptSafeZone  CScriptSkyRipple  CScriptWallCrawler  ScriptGuiSetup
```

On the cube each of these is a separate module, and mwldeppc's linker script turns its `RELMain` and
`RELExit` into that module's prolog and epilog. **The duplication is not a bug in the sources — it is
the module system working.** A flat host link cannot hold fourteen symbols with one name.

**What was done.** Each module's entry points take a distinct name **on the host only**:

```cpp
#ifdef __MWERKS__
#define MP_TWEAKS_MAIN RELMain
#define MP_TWEAKS_EXIT RELExit
#else
#define MP_TWEAKS_MAIN mp_relmain_tweaks
#define MP_TWEAKS_EXIT mp_relexit_tweaks
#endif
```

MWCC still compiles `RELMain`/`RELExit`, so the `Matching` units are untouched — which the gate
confirms. `src/REL/REL_Setup.cpp`'s `_prolog`/`_epilog` keep their retail names and their retail
signature, and on the host they forward to the registry instead of calling a `RELMain` that a flat
link cannot have.

**The rename alone would have been a fake fix, and that is the part worth remembering.** Nothing on
the host called `RELMain` — `platform/rel.cpp` calls a loaded module's prolog by *guest address
inside the image*, never by symbol name. So renaming them would have produced a green link with
every module's function-pointer table still null, and every loader behind one unreachable. Hence
`platform/compiled_modules.cpp`: a registry naming each compiled module with its init and shutdown,
run from `platform/main.cpp` either side of `InvokeCMain`. **A symbol that resolves but is never
called is a worse bug than an unresolved one, because nothing reports it.**

**`-Wl,--allow-multiple-definition` was rejected.** It would green the link while running one
module's entry point and silently skipping thirteen. A green link that lies is worse than a red one.

## The sixteen publish thunks — the whole of the module system's wiring

Once the modules can run, the next question is where their tables land. The answer is smaller than
expected: retail has **sixteen `Set*` functions that are eight bytes each**, and every one is the
same two instructions.

```
802187e4 <SetTweaks_FuncPtrs__FP16STweaks_FuncPtrs>:
802187e4:  stw  r3,-27080(r13)      ; the table pointer into a .sdata2 global
802187e8:  blr
```

A module's init calls its own `Set*` to publish its function-pointer table, and the DOL reads the
global afterwards. That is the entire mechanism: **one store per module.** Sixteen of the seventeen
`Set*` symbols in `symbols.txt` are 0x8; the seventeenth, `SetErrorHandlers`, is 0x5C.

Three are undefined in the port link, and they are precisely the three modules we have
reimplemented and registered:

| retail | function | store |
| --- | --- | --- |
| 0x802187E4 | `SetTweaks_FuncPtrs` | `stw r3,-27080(r13)` |
| 0x8021FAB4 | `SetLoader_CannonBall` | `stw r3,-26696(r13)` |
| 0x8022D574 | `SetSScriptForgottenObject_FuncPtrs` | `stw r3,-26536(r13)` |

`src/MetroidPrime/ModulePublish.cpp` defines all three. That is **732 → 727 → 724** at the linker.

**Scope, stated honestly.** That file is port-side and deliberately absent from `configure.py`. It
closes three link symbols and makes the publish real, and it reproduces retail's *behaviour* and
code *shape* — MWCC emits `stw r3,off(r13)` for exactly this — but it is **not** a `Matching` unit:
each store targets that file's own static, not the retail global at `_SDA_BASE_ - 27080`, so the
displacement will not match until a unit claims those ranges with the right small-data layout.

**And it does not unblock the frame loop, which is the claim it would be tempting to make.**
`TweaksInit` calls `SetTweaks_FuncPtrs` and nothing else that assigns anything; `gpTweakPlayerA` is
still only ever set to `nullptr` in `PortGlobals.cpp`, and `gpGameState` is assigned in
`src/MetroidPrime/main.cpp:493` by `CMain`'s own code, not by Tweaks at all.

The function that would create those globals is `REL_CreateTweakGlobals`, and it is measured, not
guessed: `config/G2ME01/rels/Tweaks/symbols.txt:10` gives
`REL_CreateTweakGlobals__Fv = .text:0x00000508; size:0x5AC` — **1,452 bytes**, against the `{}` in
`src/MetroidPrime/Tweaks/Tweaks.cpp:96`. It is module-side, so it is in
`config/G2ME01/rels/Tweaks/symbols.txt` and *not* in the DOL's `config/G2ME01/symbols.txt`, and
`build/G2ME01/Tweaks/asm/MetroidPrime/Tweaks/Tweaks.s:387` already holds its disassembly. **That,
not the publish thunks, is the next thing to measure on the boot path** — and it is module work, so
it does not compete with the lanes on the DOL's 724.
