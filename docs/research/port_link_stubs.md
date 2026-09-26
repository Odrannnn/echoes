# The port's link stubs: 181 symbols that are provably not on the boot path

## The problem this solves

The port's real link asked for **523 symbols that nothing in the tree defines**. The
decompilation cannot retire them one Matching unit at a time in any useful time frame, and the
port has never produced a binary. So the question is not "how many are undefined" but **"which
of them can be given a definition without changing what the game does"** - and that is a
reachability question, which had never been asked.

`tools/link_reach.py` asks it. Answer: **181 of the 523 are referenced only by objects that are
not reachable from the program's roots.** `tools/gen_link_stubs.py` turns those into
`src/MetroidPrime/PortLinkStubs.cpp` - **177 functions and 4 data objects** - and the port's
undefined count goes **523 -> 342**.

**The 342 that remain are exactly the 342 the analysis predicted must be real.** The instrument
and the linker agree to the symbol, which is the only reason to believe either.

## Why the two roots matter, and the one that is easy to miss

A walk seeded only from the entry object calls almost nothing reachable, because
`mp_port_entry` and `mp_platform` reference few game symbols directly. The second root is the
one that matters:

> **Every object that emits a static initialiser runs before `main`.**

Getting this wrong in the other direction - treating the 159 REL module loaders as optional
because no C++ code calls them - is exactly the mistake this analysis exists to prevent. They are
registered from a data table, not called.

Granularity is per **object**, not per CMake target. `mp_game` is one target holding 200 objects,
so a target-level walk marks all 200 reachable and answers nothing. That was the first version of
this tool and it reported 42 reachable symbols, which was wrong and would have been believed.

## Two bugs this instrument had, both caught by making it check itself

1. **It compared two different alphabets.** `ld.bfd` prints *demangled* undefined names
   (`CAudioSys::IsAICallbackEnabled()`) and `nm` prints *mangled* ones
   (`_ZN9CAudioSys18IsAICallbackEnabledEv`). Set-intersecting them matched almost nothing, and the
   tool reported "the linker asks for 457 symbols nm never saw" - which reads like a missing-file
   bug and is actually a string-encoding bug. Both sides now go through `c++filt`, which leaves
   Metroid's `Name__F...` spellings alone because they are already unique.
2. **It would have produced a confident wrong answer.** The first version had no self-check and
   printed a clean-looking split of 342/181. It now asserts that the linker's undefined set is
   almost entirely contained in what `nm` reports (`ld_undef - all_undef` must be ~0) and **stops**
   if not. That assertion is what exposed bug 1.

A third, smaller one: a tentative definition with no initialiser is **discarded as unused**, so
the 4 vtable/typeinfo stubs never reached the object file and the linker's count only fell by 177
rather than 181. `= {}` on the definition is load-bearing. A stub that silently does not exist
looks identical to a stub that does not work.

## What a stub is, and how it is built

**It is not decompilation and it is not claimed to match retail.** `configure.py` does not mention
`PortLinkStubs.cpp`, so it cannot affect `main.dol` or any of the 86 REL modules. It exists only
in the port's build.

The linker resolves the *mangled* name, which is not a legal C++ identifier, so `extern "C"` plus
an `asm` label carries it verbatim:

```cpp
extern "C" void stub_0() asm("_ZN6CActor12CreateShadowEb");
extern "C" void void_0() {}
```

The signature is deliberately the emptiest available. **That is only sound because the symbol is
unreachable** - a wrong signature would corrupt the stack if the function were ever called, and
the entire justification for the file is that it is not. If one of these ever becomes reachable,
`tools/link_reach.py` moves it into the reachable set on the next run and **the stub becomes a
bug**, which is the correct and loud failure.

Breakdown of the 181: **87 REL loaders, 72 game methods, 18 unmangled `fn_*`/`lbl_*`, 4
vtable/typeinfo.**

## The port gap the loaders exposed: `g_LoaderFuncs` is dead

85 of the 87 stubbed loaders are referenced by exactly one object, `ScriptLoader.cpp.o`, and that
object **defines one symbol and no initialiser**:

```
$ nm --defined-only .../ScriptLoader.cpp.o
0000000000000000 D g_LoaderFuncs
$ nm --undefined-only .../ScriptLoader.cpp.o | wc -l
184
```

`src/MetroidPrime/ScriptLoader.cpp` does build the table - `g_LoaderFuncs[]` is 100+ `{'ACTR',
&LoadActor}` pairs. But **nothing in the port ever reads it.** The linker must still resolve the
table's relocations, which is the entire reason those 85 symbols are asked for at all.

So the loaders are **dead data, not dead code**, and stubbing them is correct. But the finding
underneath is a real port gap, not a decompilation one: **the script loader table is never handed
to the script system.** A first frame does not need it; a loading world does. It is recorded here
so the next session does not read "181 symbols stubbed" as "the script system is fine".

## What is left, and it is not stubbable

**342 undefined symbols, every one of them referenced by a reachable object.** `configure.py` will
not grow them, `flip_test` will not make them Matching, and no amount of stubbing is sound - they
are on the path by the analysis's own definition.

That set is *game* code, not SDK: the GameCube SDK side is already satisfied by Aurora (there are
**zero** `OS*`/`AX*`/`AL*`/`DSP*`/`PAD*` symbols in the linker's list). So the remaining 342 is
decompilation work, and the honest statement is that the port cannot link until a substantial part
of the game's own code exists.
