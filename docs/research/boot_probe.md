# How far does the port get? A boot probe, and what it found

## The tool, and what it is for

`tools/boot_probe.sh` links the port with `-Wl,--warn-unresolved-symbols`, which turns the 342
missing symbols into warnings and still produces a binary, then runs it. **That binary is not a
port.** Every unresolved call is a jump to address 0. It exists to be crashed, and it must never be
committed, shipped, or reported as working.

It is here because every plan to close the link gap so far has been argued from a static upper
bound, and a bound cannot tell you what to write first.

## The flag choice, and the one that is wrong

`--unresolved-symbols=ignore-all` was tried first. **It does not work here**, and not for a reason
that has anything to do with the game: it sends `ld` down a different archive-resolution path, and
the final link then dies with

```
hidden symbol `crc32' in _deps/nod_prebuilt-src/lib/nod_prebuilt.a(f0389296f42960e9-crc32.o)
  is referenced by DSO
final link failed: bad value
```

`crc32` is zlib's, inside Aurora's prebuilt `libnod.a`, and a DSO wants it. `--allow-shlib-undefined`
does not suppress it. This is third-party build behaviour, not port code, and it is worth knowing
before someone else spends an hour on it.

## The result that matters: two instruments agree on 342

`tools/link_reach.py` predicted, from object reachability alone, that 181 symbols were safely
stubbable and **342 would remain**. The stub file supplies 181. The partial link then names
**exactly 342** undefined symbols.

That is worth stating plainly because it is the first time in this project that a prediction made
by a cheap static instrument has been confirmed by the expensive real one, to the symbol. It also
means `link_reach.py`'s reachability split is not approximately right - it is exactly right on this
tree, and the next run can be trusted to say which stub has become unsafe.

## The deepest boot-path dependencies, named

The partial link attributes every undefined symbol to an object **and a source line**, which the
aggregate count throws away. Seven of the 342 are referenced from a constructor or a Math unit -
i.e. from code that must run before anything is drawn:

| symbol | referenced from |
| --- | --- |
| `vtable for CMainFlow` | `CMainFlowCtor.cpp:40` |
| `vtable for CIOWin` | `CIOWinCtor.cpp:21` |
| `vtable for CResFactory` | `CResFactoryCtor.cpp:81` |
| `CGraphics::SetExternalTimeProvider(CTimeProvider*)` | `CTimeProvider.cpp:15` (and 2 more) |
| `CDamageVulnerability::NormalVulnerabilty()` | `CActor.cpp:478` |
| `CIOWinManager::RemoveIOWin(rstl::rc_ptr<CIOWin> const&)` | `CIOWinManagerRemoveAllIOWins.cpp` |
| `fn_8004935C` | `CIOWinManagerPumpMessages.cpp:86` |

### The three vtables are the sharpest item, and they are not decompilation

`vtable for CMainFlow`, `vtable for CIOWin` and `vtable for CResFactory` are all missing, and all
three classes **the port constructs during initialisation**. A zero or absent vtable means the
first virtual call through it is a jump to 0 - so these are not "some time during gameplay", they
are frame 0.

This is also the blind spot `tools/link_check.sh`'s own docstring names: *a vtable is only emitted
by the translation unit defining a class's key function*, so `vtable for X` is invisible to `nm`
until that function exists. Three of them have surfaced anyway, because the linker asks.

**And the answer is not retail's bytes.** Retail's vtable holds GameCube addresses; a host vtable
must hold pointers to the port's own compiled member functions. So the work is: declare the
classes' virtual functions, define them, and let the compiler emit the vtable - which means the
missing piece is the *declarations*, not 24 bytes of data. `CMainFlow`, `CIOWin` and `CResFactory`
are each small enough to check by hand.

## What the probe could not establish

**It never ran.** The link did not complete, so there is no faulting address, no crash site, and no
evidence about how far the game's own initialisation gets. Everything above is from the linker's
symbol attribution, not from execution. Claiming otherwise would be the exact failure this project
keeps recording: a measurement that was never taken, reported as if it had been.

To actually run it, the `crc32` archive problem above has to be solved first - most likely by making
`libnod.a`'s `crc32` non-hidden, or by ensuring the DSO's own `crc32` is found first on the link
line. That is a concrete, bounded task and nobody has attempted it.

## The three frame-0 vtables: measured, and the obvious fix makes the link worse

`vtable for CMainFlow`, `vtable for CIOWin` and `vtable for CResFactory` are missing, and all
three classes are constructed during initialisation, so these are frame 0.

**The cause is narrow and it is not what it looks like.** For all three classes the *only* member
defined anywhere in `src/` is the **constructor**:

```
CIOWin:     CIOWin::CIOWin(          <- and nothing else
CMainFlow:  CMainFlow::CMainFlow(    <- and nothing else
CResFactory: CResFactory::CResFactory(  <- and nothing else
```

Every other member, **destructors included**, is declared in the header and defined nowhere. A
vtable is emitted by the translation unit defining the class's **key function** - the first
non-pure, non-inline virtual - so with none of them defined, no vtable is emitted, and the
constructor's vptr initialisation references a symbol that does not exist.

### Defining the destructor is the obvious fix, and it is a net loss

I measured it rather than assuming, per class, by compiling a one-line TU that defines only the
destructor and reading what the object then needs:

| class | vtable emitted? | members still undefined | net on the link |
| --- | --- | --- | --- |
| `CIOWin` | **yes** | `GetIsContinueDraw`, `Draw`, `PreDraw` | **+2 worse** |
| `CMainFlow` | **yes** | `OnMessage`, `GetIsContinueDraw`, `Draw` | **+2 worse** |
| `CResFactory` | **no** | - | 0, nothing happens |

The vtable is emitted, and then its slots relocate against members that are *also* undefined:

```
RELOCATION RECORDS FOR [.data.rel.ro._ZTV6CIOWin]:
0000000000000028 R_X86_64_64   _ZNK6CIOWin17GetIsContinueDrawEv
0000000000000030 R_X86_64_64   _ZNK6CIOWin4DrawEv
0000000000000038 R_X86_64_64   _ZNK6CIOWin7PreDrawEv
```

So one missing vtable becomes three missing methods. **A gross is not a net**, for the fifth time
in this project's history.

`CResFactory` is a different failure: its destructor is `~CResFactory() {}`, already inline, so
its key function is `Build` - the first declared non-inline virtual, overriding `IFactory`'s pure
`Build`. Defining the destructor changes nothing at all; `CResFactory::Build` has to be written
before any vtable can exist.

### Why stubbing them is not available here, specifically

`CIOWin::Draw` **draws**. `CMainFlow::OnMessage` **drives the flow state machine**. `CResFactory::
Build` **builds the resource the frame is made of**. These are not incidental members that a
missing body would go unnoticed on - they are the frame. A stub here is the unsound stub that
`docs/research/port_link_stubs.md` refuses by construction, and unlike the 181 in that file these
symbols are on the path.

**So the three vtables are blocked on decompilation, not on a port workaround**, and the
unblocking action is specific: `CIOWin`'s three accessors and `CMainFlow`'s three overrides, then
`CResFactory::Build` and its four siblings. `CIOWin`'s are small const accessors and a `Draw`, and
`CMainFlow`'s are the four functions that make the main flow a state machine - all of it
decompilation with retail addresses to match, not port code.

A trap vtable - one whose every slot calls a function that prints the class and slot and aborts -
is available and would make the failure **loud instead of silent** at frame 0. It is deliberately
not done here: it links, which would make `link_check.sh` report success while the game cannot
draw, and a tool that says "not linked" is worth more than a binary that aborts on purpose.


## RESULT: the port links, opens a window, and asks for the disc

After the two fixes above, `tools/boot_probe.sh` runs unattended:

```
boot_probe: linked 86399752 bytes with unresolved symbols warned, not ignored
boot_probe: started Xvfb on :77 (pid 3111566)
boot_probe: using Mesa lavapipe (software Vulkan) as the ICD
boot_probe: disc image: /run/media/odran/Leo/Portable/roms/gc/Metroid Prime 2 - Echoes.iso
[info] [aurora] Aurora initializing
[info] [aurora::gpu] Using surface format RGBA8Unorm, present mode Mailbox
```

An earlier run of the same binary went one step further and printed **`Using framebuffer size
854x480 scale 1`** - a created window with a real size - before the port stopped and said,
correctly and without crashing:

```
metroid_prime2_port: no disc image given.
  usage: ./metroid_prime2_port <path to Metroid Prime 2: Echoes (USA) (v1.00).iso>
```

**This is the furthest the port has ever run.** Until now the answer to "does it boot" was "it does
not link". It is now "it links, it initialises Aurora, it creates a surface, and it asks for the
disc".

**And a G2ME01 disc image is present on this machine**, at
`/run/media/odran/Leo/Portable/roms/gc/Metroid Prime 2 - Echoes.iso` (1.46 GB). That is the same
input `rel_module_manager.md` records as the thing that would unblock the REL module table, so
**one input unblocks two of this project's oldest blockers.**

## Two real fixes, neither of them ours

1. **Nod's hidden `crc32`.** `libnod.a` defines a *global* `crc32` that its visibility attributes
   mark hidden; its own `deflate.o`/`inflate.o` reference it and `libpng.so` needs a `crc32` too,
   so ld pulls Nod's member and then refuses it. `--allow-shlib-undefined` does **not** suppress
   this - that is about *undefined* DSO symbols and this one is defined-but-hidden. Dropping the
   member makes those two resolve `crc32` from libz, which is the same function. The tool does it
   itself with `ar d`, because `--rebuild` re-fetches the archive and a manual step is not
   reproducible.
2. **`SDL_VIDEODRIVER=dummy` cannot create a window.** With it Aurora tries Vulkan, then OpenGLES,
   then its own Null backend, and **all three fail with `Failed to create surface`**, so the run
   dies in the windowing layer and never reaches the game. The surface is what is missing, not a
   driver to choose. `Xvfb` plus Mesa's `lvp_icd` (lavapipe) gives a real one, and neither needs a
   physical display.

   A related trap in my own tool: **a `DISPLAY` that is set is not a `DISPLAY` that works.** This
   machine exports `DISPLAY=:0` with nothing listening, and SDL then reports `x11 not available`,
   which reads like a missing driver. The tool probes for a live server with `xdpyinfo` instead of
   trusting the variable.

## The probe's ceiling, measured - and it is lower than I wanted

`--warn-unresolved-symbols` gives every unresolved symbol a PLT slot with **no GOT entry and no
stub: sixteen zero bytes.** A call to one lands in the hole:

```
0x55555563ac47  call  0x555555616cf0 <__cxa_throw_bad_array_new_length@plt+16>
=> 0x55555563ac4c <CGameAllocator::Initialize+380>:  xor %r8d,%r8d
$ objdump -d .../__cxa_throw_bad_array_new_length@plt
  00000000000c2cf0 <__cxa_throw_bad_array_new_length@plt+0x10>:
    ...
```

The call target is `plt+16`, sixteen bytes past the entry, so the program jumps into zeroes and
faults on `add %al,(%rax)`.

**So the probe cannot tell me which symbol is missing.** Its first fault is at
`CGameAllocator::Initialize` -> `CMemorySys::CMemorySys` -> `CMemory::Startup` -> `main`
(`platform/main.cpp:117`), which reads like a finding about the game's allocator and is not one:
`__cxa_throw_bad_array_new_length` **does not appear in `link_check.sh`'s undefined list**, so the
real link resolves it and the crash is entirely the flag's doing.

**The rule, and it is the tool's most important line: everything printed before the first call
into a PLT hole is real evidence; the first crash identifies a hole, not a defect.** A probe that
reports its own tooling as a bug in the port is worse than no probe, which is why this is written
down rather than left as a crash trace.

## What this means for the next step

**The next step is an honest link, not more probing.** The probe has said everything it can: the
port links, initialises, and creates a surface. Everything past the first unresolved call needs the
319 symbols to be real - which is the decompilation, and which is what the lanes are on.
