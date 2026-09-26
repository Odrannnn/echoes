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
