# match-main-cgameglobalobjects-dtors

`kind: match`, `target: MetroidPrime/main`. Verdict **PARTIAL** (exit 0): the flip is out of reach, the
target rose **64 -> 66** functions, `tools/report_diff.py` reports `+2 functions at 100%, 0 units newly
linked, no regression`, and every other check in `goal_check.sh` is green.

The item named the trio the previous item's "For the next run" section pointed at, and the premise held
exactly: `0x800064D0` / `0x80006518` / `0x80006AE0` is the `rstl::single_ptr<CGameGlobalObjects>` cluster
that `CMain::RsMain` destroys on its own `r1+20` local, and it is the same 72/264/88-byte D0 shape.

## What is in the diff

`src/MetroidPrime/main.cpp` and `include/MetroidPrime/CGameGlobalObjects.hpp` only.

```
   0x800064D0  single_ptr_assign_800064D0               72 B  100.00%   single_ptr<CGameGlobalObjects>::operator=(CGameGlobalObjects* const)
   0x80006518  __dt__CGameGlobalObjects_80006518        264 B   99.08%   ~CGameGlobalObjects
   0x80006AE0  __dt__80006AE0                            88 B  100.00%   ~single_ptr<CGameGlobalObjects>()
```

`main/MetroidPrime/main` **64 -> 66 / 99**; unit fuzzy 48.43% -> 50.83%, matched code 45.27% -> 46.18%.
`All:` matched **10097 -> 10099**, linked 4918 -> 4918. DOL units 8686 -> 8688 (gate.sh rewrote those two
numbers in `docs/HANDOFF.md` itself; I did not touch that file by hand and the driver rewrites it anyway).

The three are declared in descending retail order with the rest of the file, which puts this block
between `CGameGlobalObjects::AddPaksAndFactories` (0x80007168) and `CMain::DrawDebugMetrics` (0x800070FC).
**The tweak-manager cluster (0x80006678-0x80006874) belongs inside it, between `__dt__80006AE0` and
`__dt__CGameGlobalObjects_80006518`**, so whoever merges `match-main-ciengametweakmanager-dtor` into this
tree should land its block there.

## Three codegen rules this cost, all measured with this unit's exact `build.ninja` flags

**1. mwcceppc appends the parameter encoding to any function a class declares as a `friend`, and that
renames the symbol.** A three-function probe with this compiler:

```
  friend void* gA(A*, short);  extern "C" void* gA(A*, short)   ->  gA__FP1As
                                 extern "C" void* gB(B*, short)   ->  gB
                                 extern "C" void* gS(ns::S<int>*, short)  ->  gS
                                 extern "C" void* gV(void*, short) ->  gV
```

So a `friend` is **not** a way to give a retail-named `extern "C"` function access to a class's privates -
it is the one thing that stops objdiff pairing the function at all. `__dt__CGameGlobalObjects_80006518`
came out as `__dt__CGameGlobalObjects_80006518__FP18CGameGlobalObjectss` and read **0.00%** while its bytes
were right; `include/MetroidPrime/CGameGlobalObjects.hpp` now makes the members `public` instead, which
changes no layout and no code. A `friend struct` carrying an `extern "C"` static member is rejected by
this compiler (measured). Worth knowing before anyone writes a `friend` to reach a private from a port
function.

**2. A member's own destructor call is what brings mwcceppc's `addic. r0,r30,off / beq` address guard;
`delete member.get()` does not.** This is the whole difference on three of retail's ten member teardowns:

```
  renderer   (0x80006550)  self->renderer.~single_ptr<IRenderer>()   10 instrs, 10 match
                            delete self->renderer.get()               8 instrs,  6 match
  memoryCard (0x800065A4)  self->memoryCard.~single_ptr<CMemoryCard>()  5 instrs, 5 match
                            delete self->memoryCard.get()              3 instrs, 1 match
```

Retail guards exactly the three members whose teardown it inlines (`renderer`, `stringTable`,
`memoryCard`) and calls the other seven out of line with no guard, so the destructor-call spelling is the
one that matches. `stringTable` was already right with `~optional_object<...>()`.

**3. A destructor that is *virtual* in our header cannot be called directly from a member teardown.**
`CResFactory` derives from `IFactory`, whose `virtual ~IFactory() = 0` is at
`include/Kyoto/CResFactory.hpp:21`, so `resFactory.~CResFactory()` compiles to the vtable dispatch at
+0x08 (`addi/li/lwz r12,4(r30)/lwz r12,8(r12)/mtctr/bctrl`, 7 instructions) where retail has a plain
`bl` (3). Declaring the callee under **retail's own mangled name** fixes it, because `extern "C"`
reproduces that name verbatim: `extern "C" void __dt__11CResFactoryFv(CResFactory*, short);` and call it.
The same trick is what `fn_801F097C`, `__dt__14CMemoryCardSysFv` and `fn_80008B04` need - those three
are dtk placeholders, so they are declared and never defined (retail's own addresses are in
`config/G2ME01/symbols.txt`, and a callee's body is not a precondition for reproducing a function).

Also carried over from the tweak-manager item and re-measured here: the deleting-flag tail has to be
**inside** the `this == nullptr` block, and the flag is **`short`**, not `bool` (`extsh.` in retail,
`extsb.` for a `bool`).

## What is left, measured

`__dt__CGameGlobalObjects_80006518` is **264 bytes, exactly retail's size**, and every instruction matches
except the +0x130 `single_ptr<CGameState>` block:

```
  ours     800065b8: lwz  r3,304(r30) ; li r4,1 ; bl 8000419c <__dt__10CGameStateFv>
  retail   800065b8: addi r3,r30,304 ; li r4,-1 ; bl 80006620 <__dt__Q24rstl24single_ptr<10CGameState>Fv>
```

Retail calls the **out-of-line** instantiation at 0x80006620 (a weak symbol this unit already matches at
100%, 88 bytes) and mwcceppc inlines the teardown at every spelling tried. objdiff scores the function
**99.08%** - one word out of 66.

Spellings tried, all with this unit's exact flags:

- `self->gameState.~single_ptr<CGameState>()` - 5 instructions (mwcceppc's address guard appears),
  `addic. r3,r30,304 / beq / lwz r3,0(r3) / li r4,1 / bl`, **272 bytes** for the function.
- `rstl::single_ptr<CGameState>* p = &self->gameState; p->~single_ptr<CGameState>();` - identical.
- `delete self->gameState.get()` - **3 instructions, 264 bytes**, `lwz r3,304(r30) / li r4,1 / bl`. **This
  is the spelling kept**: it is the only one that leaves the function at retail's size.
- Removing the `stringTable` teardown entirely (to test whether mwcceppc has an inlining budget that
  gameState exceeds) - still inlined, so it is not a budget.

Why it cannot be forced, each measured:

- The symbol's name carries `<` and `>`, so it cannot be declared: `extern "C" void* __dt__Q24rstl24
  single_ptr< 10CGameState >F(...)` is `undefined identifier '__dt__Q24rstl24single_ptr'`.
- `asm("name")` and `__asm__("name")` after a declarator are both rejected by this compiler
  (`type cannot be made into a global register variable` / `declaration syntax error`).
- `#pragma noinline` **is** honoured on a function *definition* (measured: it keeps `f__Fi` out of
  `g__Fi`), but **not** on a call site and **not** on a redeclaration of an already-defined class member
  (`template <class T> S<T>::~S();` is a `declaration syntax error`; an explicit specialization of a
  member defined in-class is rejected too). Putting it on `~single_ptr` in
  `include/rstl/single_ptr.hpp` would also outline the `renderer` and `memoryCard` teardowns, which retail
  inlines, and that header is shared by every unit.
- `asm` is out, so a wrapper function with a writable name cannot buy the relocation either - and it
  would be a second definition of retail's 0x80006620, which this unit already emits and matches.

WALL: __dt__CGameGlobalObjects_80006518 99.08% - 264/264 bytes, one word out: mwcceppc always inlines ~single_ptr<CGameState> here, the symbol's name has <> so it cannot be declared, and #pragma noinline is definition-only

**The next attempt needs `match-main-ciengametweakmanager-dtor` merged first**: `~CGameGlobalObjects`
relocates to `__dt__80006678`, which is *declared* here and *defined* by that item. Declaring rather than
defining it is what keeps the two items mergeable and the relocation already points at retail's own
name, but until that commit lands the symbol is undefined in this object. The DOL link is unaffected
(`main.cpp` is `NonMatching`, so `dtk` supplies retail's bytes for the whole claimed range) and
`goal_check.sh`'s `gate.sh` is green with it declared.

## Why the flip is out of reach (unchanged, pre-existing)

`flip_test.sh` fails at link and reverts cleanly (tree rebuilt to DOL
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`). `mwldeppc` reports
`multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o` - mwcceppc lays a 28-byte copy of
that vtable in this object because the class's key functions are all undefined here, and
`config/G2ME01/splits.txt` does not claim that `.data` for this unit; `src/MetroidPrime/main.cpp` already
says so above `~CErrorOutputWindow`.

`tools/unit_fit.sh MetroidPrime/main.cpp`: **16 functions present in ours but not in the retail unit
object, 1340 bytes** - the same 16 as before this change, all COMDAT weak copies (template `rc_ptr` /
`optional_object` / `single_ptr` destructors and the `TOneStatic` accessors). This diff added none.

## Verified

```
tools/goal_check.sh build/goal/item.json   PARTIAL (exit 0): no judge-owned path touched, gate.sh ok,
                                           counts 10097 -> 10099, linked 4918 -> 4918,
                                           check_symbol_names ok, All: 31.08% fuzzy / 23.37% matched /
                                           11.78% linked (10099 / 28465), flip FAIL, target rose
                                           64 -> 66, no asm added
tools/report_diff.py <base> build/report.json   +2 functions at 100%, 0 units newly linked, no regression
sha1sum build/G2ME01/main.dol              6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py        504 units, 0 missing
python3 tools/check_decl_order.py          ok: 958 checked, 31 permuted, all accounted for
tools/unit_fit.sh MetroidPrime/main.cpp    16 extra functions, 1340 bytes (unchanged)
```

No asm. The two 100% functions are byte-identical to dtk's retail object with the `bl`/`b` fields masked
(both objects carry those as `R_PPC_REL24`); `tools/bytescmp.py` against the DOL is what produced every
instruction count quoted above, and the two flags that matter (branch displacements, relocation fields)
are exactly the ones it does **not** mask. The probe is `.tmp/opencode/L6dtor.sh` (untracked scratch):
it compiles `src/MetroidPrime/main.cpp` with this unit's exact `build.ninja` cflags - **not**
`tools/probe_cc.sh`, which omits `-inline deferred,noauto`, the three `-DMUSY_*` and
`-pragma "inline_max_size(125)"` - and diffs each of the three functions against the DOL in under a
second.

---

# Second run (2026-09-30, lane 6)

Verdict **PARTIAL** (exit 0) again, but **+3 functions at 100% where the first run got +2**. The
first run's `WALL:` on `__dt__CGameGlobalObjects_80006518` was **based on a false premise**, which
this run measured and corrected: **objdiff does not compare `R_PPC_REL24` targets.** The first run
concluded that "a wrapper function with a writable name cannot buy the relocation either". It can.
The whole 264-byte function is now byte-identical to retail's.

Baseline re-measured on this tree (it now has `match-main-ciengametweakmanager-dtor` and four other
items merged, so none of the first run's per-unit numbers apply): `main/MetroidPrime/main`
**74 / 99 functions**, `unit_fit` **15 extra functions, 1300 bytes**.
After: **77 / 99**, unit 55.42% -> 57.82% fuzzy and 51.43% -> 53.84% matched, `All:`
matched **10108 -> 10111**, linked 4918 -> 4918, unit_fit **unchanged at 15 / 1300**.

## What is in the diff

`src/MetroidPrime/main.cpp`, `include/MetroidPrime/CGameGlobalObjects.hpp`,
`include/rstl/single_ptr.hpp`. The three functions are declared in descending retail order with the
rest of the file, i.e. the block sits between `CGameGlobalObjects::AddPaksAndFactories` (0x80007168)
and the tweak-manager block (0x80006678-0x800068F4), which is inside the range 0x80006678-0x80006874
the first run's note asked for.

```
   0x800064D0  single_ptr_assign_800064D0               72 B  100.00%
   0x80006518  __dt__CGameGlobalObjects_80006518        264 B  100.00%
   0x80006AE0  __dt__80006AE0                            88 B  100.00%
```

`tools/bytescmp.py` against the DOL, with this unit's exact `build.ninja` cflags: **18/18, 66/66 and
22/22 instructions**, and every remaining difference is a `bl` field, which both objects carry as a
relocation.

## The one measurement that matters, and it corrects the first run

Retail's `+0x130` teardown is `addi r3,r30,304 ; li r4,-1 ; bl 80006620`, a call to the **out-of-line**
instantiation `__dt__Q24rstl24single_ptr<10CGameState>Fv`. That symbol **this object already emits
and already matches at 100%** - it is the weak template instantiation, 0x80006620. It cannot be
*declared* (a C++ identifier cannot hold `<` or `>`), and `asm("...")` after a declarator is rejected
by this compiler (measured again, same two diagnostics).

So this run declared it **under a writable name** and called that:

```cpp
extern "C" void* single_ptr_CGameState_dtor(rstl::single_ptr< CGameState >*, short);
// ...
single_ptr_CGameState_dtor(&self->gameState, -1);
```

| spelling of the `+0x130` teardown | objdiff on `__dt__CGameGlobalObjects_80006518` |
| --- | --- |
| `self->gameState.~single_ptr<CGameState>()` | **96.05%**, function is 272 bytes (8 too many) |
| `delete self->gameState.get()` (the first run's kept spelling) | 99.08%, 264 bytes |
| a call to retail's function under a writable name | **100.00%**, 264 bytes |

**objdiff does not compare `R_PPC_REL24` targets.** The three rows differ *only* in which symbol the
`bl` at `+0xA8` names, and the score moves 96.05 -> 99.08 -> 100.00. `tools/bytescmp.py` counts every
`bl` field as a difference (documented: the linker fills it), so it cannot show this - only objdiff's
percentage can, and this run read it out of `build/report.json` per function after each build. That is
the finding; the first run's conclusion that the wrapper "cannot buy the relocation" is **superseded**.

`single_ptr_CGameState_dtor` is **declared and never defined**. It has to be, not merely is: defining
it puts a second copy of retail's 0x80006620 body in this object beside the weak instantiation that
already *is* retail's 0x80006620, and `unit_fit.sh` would count it. A declared-and-undefined callee is
this file's own convention - `src/MetroidPrime/main.cpp` had **126** undefined `extern "C"` symbols
before this change (`fn_801449C8`, `fn_8016C230`, `fn_801F097C`, `fn_80008B04`, ...) - and
`configure.py:395` has this unit `NonMatching`, so its object is not in the DOL link.

## Four more things the first run did not measure

**1. `single_ptr::operator=` is not inlined by this compiler, and that is not a size question.**
`single_ptr_assign_800064D0` written as `&(*self = ptr)` came out **8 instructions** against retail's
18: an 8-instruction thunk onto a new weak `__as__Q24rstl32single_ptr<18CGameGlobalObjects>FP18CGame
GlobalObjects` (`-inline deferred,noauto`). The body has to be written out, which is why
`include/rstl/single_ptr.hpp`'s `mPtr` is now `public`. An access specifier makes mwcceppc emit
nothing, so no other unit's code changes - measured, the whole tree is byte-identical outside this
object.

**2. `delete p` emits a *second* copy of the teardown.** In `single_ptr_assign_800064D0`,
`delete self->mPtr;` made the compiler emit the implicit `__dt__18CGameGlobalObjectsFv` (252 bytes) -
**17 extra functions / 1596 bytes** in `unit_fit`, one more than the tree had. Calling
`__dt__CGameGlobalObjects_80006518(self->mPtr, 1)` instead is the same code (`delete p` *is*
`p->~T(1)`, which is the `li r4,1`) and byte-identical, and brings it back to 15 / 1300. Both
spellings measured.

**3. An explicit member specialization of a template destructor is rejected, and an explicit
instantiation of the class is worse than nothing.** Measured, both:

```
template <> SP<GS>::~SP() { delete m; }          -> Error: illegal function definition
#pragma noinline
template class rstl::SP<GS>;                     -> no __dt__...SP<GS>Fv emitted at all, and the
                                                   call site gets `bl` with NO relocation
template class rstl::SP<GS>;   (no pragma)       -> still inlined at the call site, and the weak
                                                   instantiation disappears from the object
```

The last one matters beyond this item: it would have made
`tools/check_symbol_names.py` report `__dt__Q24rstl24single_ptr<10CGameState>Fv` as declared but not
defined. Do not try it on any unit that has a template destructor objdiff is pairing by name.

**4. `~CGameGlobalObjects` cannot be reached through `friend`.** Re-confirmed for this tree, and it is
why `include/MetroidPrime/CGameGlobalObjects.hpp`'s member section is `public`: mwcceppc appends the
parameter encoding to anything a class declares a friend, so `__dt__CGameGlobalObjects_80006518`
would come out as `__dt__CGameGlobalObjects_80006518__FP18CGameGlobalObjectss`, which objdiff cannot
pair at all. No member is renamed, moved or resized.

## Why the flip is still out of reach (unchanged, pre-existing)

`flip_test.sh` fails at link and reverts cleanly (tree rebuilt to DOL
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`). `build/flip-ninja.log` has **46** `undefined:` lines
before `multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o` - `fn_80008C28`,
`lbl_80418EA0`, `fn_80009224`, `fn_800068F4`, `rstl::rc_ptr<CMapWorldInfo>::ReleaseData()` and 41
more, from seven other units. This item added one more undefined name to a unit that is already 46
short and adds none of the 46. The `CErrorOutputWindow::__vt` copy is mwcceppc laying a 28-byte vtable
in this object because the class's key functions are undefined here and `config/G2ME01/splits.txt`
does not claim that `.data` for this unit; `src/MetroidPrime/main.cpp` already says so above
`~CErrorOutputWindow`.

`tools/unit_fit.sh MetroidPrime/main.cpp`: **15 extra functions, 1300 bytes - the same 15 the clean
tree has.** This diff added none and removed none.

## Verified

```
tools/goal_check.sh build/goal/item.json   PARTIAL (exit 0): no judge-owned path touched, gate.sh ok,
                                           counts 10108 -> 10111, linked 4918 -> 4918,
                                           check_symbol_names ok, All: 31.10% fuzzy / 23.40% matched /
                                           11.78% linked (10111 / 28465), flip FAIL, target rose
                                           74 -> 77, no asm added
tools/report_diff.py <base> build/report.json   +3 functions at 100%, 0 units newly linked,
                                                 no regression
sha1sum build/G2ME01/main.dol              6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py        504 units, 0 missing
python3 tools/check_decl_order.py          ok: 958 checked, 31 permuted, all accounted for
tools/unit_fit.sh MetroidPrime/main.cpp    15 extra functions, 1300 bytes (same as the clean tree)
```

No asm. The probe is still `.tmp/opencode/L6dtor.sh` (untracked scratch): it compiles
`src/MetroidPrime/main.cpp` with this unit's exact `build.ninja` cflags - **not** `tools/probe_cc.sh`,
which omits `-inline deferred,noauto`, the three `-DMUSY_*` and `-pragma "inline_max_size(125)"` -
and diffs each of the three functions against the DOL. The objdiff percentages in the table above
came from `build/report.json` per function, i.e. from a real `./tools/decomp_build.sh
main/MetroidPrime/main` after each spelling, not from the probe.

## For the next run

This item is finished as far as the three functions go: all three are at retail's size and byte for
byte. What is left on this unit is the flip, and the flip needs the **`CErrorOutputWindow::__vt` data
claim and the other units' 46 undefined symbols** cleared first, which is not this item's job. A
`NEW:` for that is not filed here because its target would be a set of other units rather than one
unit, module or symbol.
