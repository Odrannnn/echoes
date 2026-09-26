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

## Two of the three are now closed, and the vtable layout is measured (lane `j3`, 2026-09-26)

`vtable for CIOWin` and `vtable for CMainFlow` are **gone from the port's link**, and the
sections above are superseded on those two. `vtable for CResFactory` is untouched: its destructor
is inline, so its key function is `Build`, as the section above says.

**What landed**, all `Matching`, all four units `flip_test` PASS and keeping, DOL sha1 unchanged:

| unit | retail range | what it is |
| --- | --- | --- |
| `MetroidPrime/CIOWinAccessors.cpp` | `0x80049E10..0x80049E20` | `PreDraw` 4 B, `Draw` 4 B, `GetIsContinueDraw` 8 B |
| `MetroidPrime/CIOWinDtor.cpp` | `0x80049E30..0x80049E98` + `.data 0x803B1BA0..0x803B1BC0` | `~CIOWin` 104 B **and the vtable** |
| `MetroidPrime/CMainFlowAccessors.cpp` | `0x8001DF48..0x8001DF54` | `Draw` 4 B, `GetIsContinueDraw` 8 B |
| `MetroidPrime/CMainFlowDtor.cpp` | `0x8001DAF4..0x8001DB54` + `.data 0x803B1770..0x803B178C` | `~CMainFlow` 96 B **and the vtable** |

The destructor is in the *same unit* as the vtable claim because it is the class's key function:
the unit that defines it is the unit that emits the vtable, and the vtable's slots relocate against
the accessors. So the net on the link is measured, not assumed - see the table in
`RUNNING_THE_DECOMP.md`.

**The net is smaller than the gross, in both directions.** Gross: 7 functions and 4 units. On the
link: `link_gap.py`'s **c++ runtime / linker bucket fell 18 -> 16** (the two vtables leave it; a
`vtable for X` is classified as runtime, not as a game method) and **MISSING rose 288 -> 289**,
the one new entry being `CMainFlow::OnMessage`. That is the honest shape: two vtables gone, one
real hole named. `CMainFlow`'s vtable cannot be defined *without* `OnMessage` existing as a symbol,
and it does exist - `config/G2ME01/symbols.txt` renames retail's unnamed `fn_8001DF54` to
`OnMessage__9CMainFlowFRC20CArchitectureMessageR18CArchitectureQueue` so dtk's fill object carries
the name the vtable's relocation needs, and **retail's own bytes are what back it**. That is not a
stub; see `port_link_stubs.md`.

### The vtable layout, measured, because the slot order is the whole problem

Reading `.data` in `build/G2ME01/main.elf` at the addresses `symbols.txt` gives for the vtables:

```
vtable for CIOWin   0x803B1BA0  size 0x20   0, 0, 0x80049E30, 0, 0x80049E18, 0x80049E14, 0x80049E10
vtable for CMainFlow 0x803B1770  size 0x1C   0, 0, 0x8001DAF4, 0x8001DF54, 0x8001DF4C, 0x8001DF48, 0x80049E10
vtable for CActor    0x803B1BC0  size 0x80   0, 0, then 29 addresses, then one 0
vtable for CEntity   0x803B1B40  size 0x20   0, 0, then 6 addresses
```

So MWCC's layout is **two zero words of header - offset-to-top, then typeinfo, which is zero
because the build is `-RTTI off` - and then one slot per virtual in declaration order, with the
destructor taking exactly ONE slot.** That last one is not the Itanium layout and it is the thing
that makes a slot count readable: `CEntity` declares a virtual destructor and five methods and has
**6** slots, not 7. And a **pure virtual gets a NULL slot**, which is how `CIOWin::OnMessage`
appears in its own vtable as the `0` above. `symbols.txt`'s object sizes are rounded up to a
multiple of 4, so 7 words reads as `0x20` and 31 words as `0x80`; that padding word is why
`unit_fit.sh` calls `CIOWinDtor`'s `.data` "SHORT by 4" and why that is not a failure.

Two independent checks say the slot order read here is right and not a coincidence:
`vtable for CMainFlow`'s **last** slot is `0x80049E10`, the same address `vtable for CIOWin` holds in
its own last slot - that is `CIOWin::PreDraw`, which `CMainFlow` does not override, so it inherits
the slot. And the two `GetIsContinueDraw` bodies **disagree** - `li r3,1` in `CIOWin` and `li r3,0`
in `CMainFlow` - so which is which is a measurement and not a guess.

### What is still missing, and why `CMainFlow::OnMessage` is not a sixth unit here

`CMainFlow::OnMessage` is retail `0x8001DF54..0x8001E008`, **180 bytes**, and it is
**structurally reproduced already** - a probe of the shape

```cpp
switch (msg.GetType()) {
case kAM_TimerTick:   AdvanceGameState(queue);                          return kMR_Normal;
case kAM_SetGameState: { CIntMsgParm p(<the parm's int>);               // two vptr stores
                         SetGameState(<that int>, queue, p);              return kMR_Exit; }
default:              return kMR_Normal;
}
```

emits retail's `cmpwi 5 / bge / cmpwi 4 / bge / b / cmpwi 7 / bge / b` dispatch **byte for byte**,
then `mr r3,r4; bl GetParm`, the two `lis`, the two vptr stores, `lwz r4,4(r3)`, `mr r3,r30`,
`stw r4,12(r1)`, `bl SetGameState`, the destructor call, `li r3,1`, `li r3,0` and the epilogue.

Three things stop it being a `Matching` unit, and all three are real.
**All three are now closed - see the next section, which supersedes the rest of this one.** They are
left here as written because two of the three needed a measurement nobody had taken, and the reason
is worth reading before the answer.

1. **The 8-byte local is a class with two vtables at fixed addresses.** It is a parm deriving from
   `IArchitectureMessageParm`: `0x803B0DD0` is the base's vtable and is **all zeros** (its dtor slot
   is NULL), `0x803B1B60` is the derived's and holds one slot, `0x800487B8` - which is also the
   vtable the header already documents for `MakeMsg`'s frame parm. `OnMessage` *stores both
   addresses*, so a `Matching` unit has to place them at exactly those two addresses, and the only
   class in the tree with that shape is in an **anonymous namespace in
   `src/MetroidPrime/main.cpp`**, whose vtable symbols are therefore local. Promoting it out moves
   `main.cpp`'s code, which is a `NonMatching` unit another lane owns.
2. **The destructor's call has to be out of line.** Retail does `addi r3,r1,8; li r4,-1;
   bl 0x800487b8`; a class that is complete in the translation unit gets its destructor inlined
   instead, and the probe shows exactly that. So the destructor must live in a *different* unit
   than the one that constructs the object - which is the same requirement as (1).
3. **`CArchitectureMessage::GetParm()` is out of line in retail** - `fn_80048CE4` and
   `fn_80048CEC`, 8 bytes each, identical, `lwz r3,8(r3); blr`, almost certainly the const and
   non-const overloads. `OnMessage` calls it (`mr r3,r4; bl 0x80048ce4`) where the port's header
   inlines it, so this accessor needs `#pragma inline_max_size(0)` - a header change that eight
   units include, each of which has to be re-measured.

`OnMessage` also calls two functions that are retail's *unnamed* `fn_8001DE68` (224 bytes, a
jumptable at `0x803B178C` switching on `this->x14_gameState`) and `fn_8001DB54` (788 bytes, which
writes the state at `+0x14` and then switches on it). Those are `CMainFlow::AdvanceGameState` and
`CMainFlow::SetGameState`; `symbols.txt` can be renamed to give dtk's fill those names, so they are
**not** what blocks the vtable - and the prediction was right: renaming them was enough, they are not
in any way the blocker. They are 1,012 bytes of hard decompilation, and they are still unwritten,
and they are where the next lane should go.

## All three blockers are closed and `CMainFlow::OnMessage` is a `Matching` unit (lane `k3`, 2026-09-26)

`OnMessage__9CMainFlowFRC20CArchitectureMessageR18CArchitectureQueue` is **100.00%** and `Matching`,
`flip_test` PASS and keeping, DOL sha1 unchanged, and the port's link now asks for
`AdvanceGameState` and `SetGameState` by name instead of for `OnMessage`. Four units, all `Matching`,
all flipping:

| unit | retail range | what it is |
| --- | --- | --- |
| `MetroidPrime/CArchitectureMessageGetParm.cpp` | `0x80048CE4..0x80048CF4` | `GetParm()` and `GetParm() const`, 8 B each (blocker 3) |
| `MetroidPrime/CFrameMsgParmDtor.cpp` | `0x800487B8..0x80048814` + `.data 0x803B1B60..0x803B1B6C` | `~CFrameMsgParm`, 92 B **and its vtable** (blocker 2) |
| `MetroidPrime/CTimerMsgParmDtor.cpp` | `0x80048834..0x80048890` + `.data 0x803B1B70..0x803B1B7C` | `~CTimerMsgParm`, 92 B **and its vtable** |
| `MetroidPrime/CMainFlowOnMessage.cpp` | `0x8001DF54..0x8001E008` | `OnMessage`, 180 B |

Blocker 1 needed no new idea, only the promotion: `CFrameMsgParm` and `CTimerMsgParm` are in
`include/MetroidPrime/CArchitectureMessageParm.hpp` now, out of `main.cpp`'s anonymous namespace,
and their vtable symbols are nameable. `symbols.txt` gains three renames the units could not exist
without - `lbl_803B1B60` -> `__vt__13CFrameMsgParm`, `lbl_803B1B70` -> `__vt__13CTimerMsgParm`,
`lbl_803B0DD0` -> `__vt__24IArchitectureMessageParm` - plus `fn_800487B8` -> `__dt__13CFrameMsgParmFv`
and `fn_80048834` -> `__dt__13CTimerMsgParmFv`, and the two `OnMessage` callees. **The vtable
`lbl_803B1B60` is 0x10 bytes in the map and 0xC in the object, and both are right:** MWCC's vtable is
two zero header words plus one slot, so the trailing zero at `0x803B1B6C` belongs to the *next*
symbol and claiming 0x10 makes dtk reject the split outright ("ends within symbol"). Three symbol
sizes in `symbols.txt` changed from `0x10` to `0xC` to say so.

### The instruction order is the copy constructor, and that is the only spelling that reaches it

The obvious body - read the parm's int, build a `CFrameMsgParm` from it, pass the int to
`SetGameState` - compiles to **86.33%**, and no amount of rearranging the int fixes it: mwcceppc
hoists `lwz r4,4(r3)` to the top of the block and reuses `r3` for the second vtable address, where
retail keeps the parm pointer in `r3` across both stores and uses `r5`/`r4` for the two `lis`/`addi`
pairs. The body that is byte-exact is the one that *is* a copy:

```cpp
CFrameMsgParm parm(*static_cast<const CFrameMsgParm*>(msg.GetParm()));
SetGameState(static_cast<EClientFlowStates>(parm.GetFrameCount()), queue);
```

mwcceppc expands the copy constructor in place, and the order then follows the class rather than the
scheduler: base vptr, own vptr, member. `tools/try_batch.py` over five spellings put this at zero
differing instructions and the other four at 4 to 8.

The `kMR_Normal` return is a second measurement: written as `return kMR_Normal` in the
`kAM_TimerTick` arm *and* in `default`, mwcceppc emits `li r3,0` in both and branches - one
instruction too many, 84.00%. Written as `break` out of both arms with a single `return kMR_Normal`
after the `switch`, the two arms share retail's `li r3,0` at `0x8001dfec`, and it is 100.00%.

### What is now the frame's next hole, and it is not small

`OnMessage`'s two callees are the honest remainder: `AdvanceGameState` (0x8001DE68, 224 bytes) and
`SetGameState` (0x8001DB54, 788 bytes), **1,012 bytes together**, both renamed in `symbols.txt` so the
unit can call them. The port's link gap moved 202 -> 203 "other game methods" for exactly this
reason - one symbol closed, two opened, and the two are named and measured rather than one being an
unnameable vtable slot. `CResFactory::Build` and its four siblings are still the last thing keeping
`vtable for CResFactory` on the link, and `CIOWin`'s and `CMainFlow`'s vtables are now entirely
inside the tree.

### Two measurements that will save the next lane a day

**MWCC's vtable is two zero header words plus one slot per virtual - and the map's symbol size is
not it.** `lbl_803B1B60` is `size:0x10` in `symbols.txt` because the next symbol is 0x10 away; the
object has 0xC in it, and *dtk refuses a split that ends inside a symbol*, so the first attempt
(`start:0x803B1B60 end:0x803B1B6C`) failed with `Split ... ends within symbol 'lbl_803B1B60'`. The
fix is in the **map**, not the split: three `.data` symbols changed from `size:0x10` to `size:0xC`
(`lbl_803B1B60`, `lbl_803B1B70`, `lbl_803B0DD0`), which is what makes the trailing zero at
`0x803B1B6C` the *next* symbol's first word. **Any future unit that claims a vtable will hit this**,
and the error names neither the cause nor the fix.

**`IArchitectureMessageParm`'s destructor must be inline and empty, not pure, and that is not a
style choice.** It is what makes `~CFrameMsgParm` 0x5C bytes and byte-exact:

| base destructor | `~CFrameMsgParm` | why |
| --- | --- | --- |
| `virtual ~IArchitectureMessageParm() {}` | **0x5C, 100.00%** | mwcceppc expands the empty base dtor; only the base-vptr store survives, and because the expansion needs no call nothing clobbers `r4`, so the deleting flag stays in `r4` and `this` alone takes `r31` - one saved register, which is retail's shape |
| `virtual ~IArchitectureMessageParm() = 0;` | 0x60, **96.33%** | `li r4,0 ; bl __dt__24IArchitectureMessageParmFv` where retail has the store, plus a second saved register |

Both were compiled with mwcceppc's real flags. The `0x10`-vs-`0xC` and the inline-vs-pure are the
same fact seen from two sides: a vtable entry is a *slot*, and an empty inline destructor still gets
one while a pure one gets a `beq` instead of a call.

### The port links and opens a window. It does not reach a frame.

Stated plainly because the numbers above look like a milestone and are not one. `link_check.sh`
still reports **NOT LINKED**, with 319 unique undefined symbols and 0 duplicate definitions -
**up one from 318, and the rise is the trade, not a regression**: `CMainFlow::OnMessage` is real
code in the port for the first time, and in exchange the linker asks for `AdvanceGameState` and
`SetGameState` by name. `link_gap.py`'s MISSING bucket is unchanged at 289. Nothing here draws a
frame. The port links and opens a window under `--warn-unresolved-symbols` (as `tools/boot_probe.sh`
does) and then asks for the disc; what stands between that and a frame is `AdvanceGameState` and
`SetGameState` for this flow, `CResFactory::Build` and its four siblings, and `CIOWin::Draw` - not
this function.
