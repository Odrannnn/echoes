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
| Unique undefined symbols at link | **727** |
| Duplicate definitions at link | **4** (`RELMain` and `RELExit`, twice each) |
| Binary produced | no — the link fails, so the port does not boot yet |

So the port is not "blocked on an unimplemented build system". The build system
works, every game source compiles, and the whole remaining problem is 727
symbols plus one structural issue.

## Cross-checking `link_gap.py` against the linker

`link_gap.py` reported **724**. The linker reports **727**. The deltas are not
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

## The remaining structural blocker: `RELMain`/`RELExit`

Four duplicate definitions, all of them the REL module entry points:

- `src/MetroidPrime/Tweaks/Tweaks.cpp` — `RELMain`, `RELExit`
- `src/MetroidPrime/ScriptObjects/CScriptCannonBall.cpp` — `RELMain`, `RELExit`
- `src/MetroidPrime/ScriptObjects/CScriptForgottenObject.cpp` — `RELMain`, `RELExit`

On the cube each of these is a **separate REL module**, loaded at runtime, so the
duplication is not a bug in the sources — it is the module system working. The
flat host link cannot have it, because all three land in one `mp_game`. The fix
is the game-side module manager: compile each module separately and have
`platform/rel.cpp` load it, which is what `PORT_NOTES.md` has been tracking.

`-Wl,--allow-multiple-definition` would make the link go green while running one
module's entry point three times over and never running the other two. That
converts a loud failure into a silent wrong answer, so it is not acceptable here.
