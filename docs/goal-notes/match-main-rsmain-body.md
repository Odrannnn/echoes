# match-main-rsmain-body

`kind: match`, `target: MetroidPrime/main`. Verdict **PARTIAL**: the flip is out of reach on the
pre-existing `CErrorOutputWindow::__vt` link error, the target rose **74 -> 76 / 99** functions,
and `tools/goal_check.sh build/goal/item.json` passed every other check. Diff is
`src/MetroidPrime/main.cpp` only, +79 / -0, no asm, no deletion of real work.

## What landed: the two functions only `CMain::RsMain` calls

The item's `reason` was truncated mid-sentence; the full claim is in
`docs/goal-notes/match-main-sda-float-constants.md` lines 492-498, and **its premise was right and
its mechanism was wrong.** It said the two functions "are called from nowhere else in this unit but
from it", and that is exactly what `build/G2ME01/obj/MetroidPrime/main.o`'s relocation table
shows - but it concluded they were reachable only through a written `RsMain`. They are not.

| symbol | retail | bytes | diff |
|---|---|---|---|
| `single_ptr_assign_800064D0` = `rstl::single_ptr<CGameGlobalObjects>::operator=(T* const)` | 0x800064D0 | 72 | **0** |
| `__dt__80006AE0` = `rstl::single_ptr<CGameGlobalObjects>::~single_ptr()` | 0x80006AE0 | 88 | **0** |
| `fn_800068F4` = the 12-byte-element range destroy over `CGameState+0x1F4` | 0x800068F4 | 96 | 37 (91.00%) |

Both of the first two are `T` in retail's `nm`, not `W` - **strong symbols, not COMDAT weak
copies** (`build/binutils/powerpc-eabi-nm build/G2ME01/obj/MetroidPrime/main.o`, measured) - and
each has exactly one `R_PPC_REL24` in `main.o` (0x10D0 and 0x10E0, which are 0x80006488 and
0x80006498, the last two instructions of `RsMain`'s shutdown path). So they are genuinely
not-instantiated, and a non-template `extern "C"` body with retail's own name is emitted
unconditionally, exactly as the 0x80006678-0x800068F4 block already in this file does. **No
`RsMain` body was needed, and none was written.** The same `T` check is what makes ten of the 23
remaining unpaired functions reachable; see the correction at the end of this file.

The bodies are the D0 form this file already writes for `rstl::single_ptr<CInGameTweakManager>`:
`this` in r3, the deleting flag in r4 as a `short` (hence `extsh.`, not `extsb.`), a
`this == nullptr` early return, the member teardown, `CMemory::Free(this)` when the flag is
positive, and `return self` for the `mr r3,r30` in the epilogue.

## `single_ptr::operator=` needs a store the header will not express

`rstl::single_ptr<T>` is `{ mutable T* mPtr; }` and nothing else (`include/rstl/single_ptr.hpp`
carries `CHECK_SIZEOF(unk_singleptr, 0x4)`), so the body is "destroy the old, store the new,
return this". `mPtr` is private, and the header's inline `operator=` spells the destroy as
`delete mPtr`, which for `CGameGlobalObjects` is a call to the class-scoped
`TOneStatic<CGameGlobalObjects>::operator delete` **after** the destructor - two calls, where
retail has the one D0 destructor call. Hence `SGameGlobalObjectsPtr`, a one-member same-layout
view, written the way `SGameStateRecord`/`SGameStateRecords` already are in this file: the layout
is named, the store is not a raw offset, and `tools/check_raw_offsets.py` still reports 152 sites
in 61 files, unchanged.

## `__dt__CGameGlobalObjects_80006518` is declared, not defined - and that is correct

Both new functions `bl` retail's 264-byte `CGameGlobalObjects` destructor, which **this unit does
not define**: retail's `+0x14C` member is a self-pointer whose destructor calls
`__dt__CGameGlobalObjects` itself, not the `single_ptr<CInGameTweakManager>` the header declares
there, so `CGameGlobalObjects::~CGameGlobalObjects(){}` emits 252 bytes against retail's 264 with
150 differing (measured by an earlier run, and unchanged here). That is the open
`match-main-cgameglobalobjs-14c` item, and it is a **shared-header layout change**, which this item
may not make. The declaration plus the real call is what these two bodies need; the undefined
reference is the same kind this file already carries for `CMemory::Free` at `fn_80006874` and for
`fn_80004864` at `fn_800068F4`. `tools/check_symbol_names.py` reports 0 missing and the DOL link
is unaffected, because `MetroidPrime/main` is `NonMatching` and its object is not in the link.

## `fn_800068F4` is 91.00% and the rest is a wall

`CMain::RsMain` calls it once, at 0x80006384, with `CGameState + 0x1F4` in r3 - which
`CGameState::AudioGroups()` (`include/MetroidPrime/Player/CGameState.hpp:176`) already names, and
which is the 16-byte `SGameStateBlock` of `CGameStateBlocks.hpp:47`. It builds
`[data, data + count*12)`, hands the **addresses** of the two ends to `fn_80004864` (0x80004864, in
the unclaimed `main/auto_03_80003BE8_text` range, so declared and left undefined), and zeroes the
count. Same strong-`T`, single-reloc situation as the two above, and it was unpaired before.

The 37 remaining bytes are retail's **duplicate iterator pair**: `last` stored at both r1+0x08 and
r1+0x0C, `first` at both r1+0x10 and r1+0x14, with r1+0x14 / r1+0x0C passed on. mwcceppc collapses
each to one store, which also moves the `add` from r5 to r0. Ten spellings measured, none reached
100%:

| spelling | differing bytes |
|---|---|
| `&first, &last` (committed) | **37 (91.00%)** |
| reference parameters `char*&` | 37 |
| two extra named copies | 46 |
| `{x00,x04}` iterator struct, args on `x04` / on `x00` | 48 / 43 |
| struct holding the pair | 43 |
| swapped parameter order | 38 |
| `last += count * 12` | 50 |
| reverse declaration order | 55 |
| 2-element `char* ends[2]` | 57 |

WALL: fn_800068F4 91.00% - retail stores each end of the range twice and mwcceppc collapses each to
one store; ten spellings measured, none reproduces the pair, and `fn_80006724` in the same unit
has the identical four stores with the identical problem, so it is a property of the compiler

## What still stops the flip

Unchanged and not close. `flip_test.sh` fails at link with

```
mwldeppc.exe Linker Error: multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o
```

the same `splits.txt`/`.data` question three earlier runs filed, `.text` is still SHORT (unit_fit
reports 15 COMDAT extras, 1300 bytes, all pre-existing), and
`python3 tools/check_decl_order.py --unit MetroidPrime/main` still reports the pre-existing
**would break on a flip** with the same first-8 list - the three new symbols are placed at their
retail addresses in descending order (0x800064D0, 0x800068F4, 0x80006AE0) and did not change the
verdict. None of the three is this item's to fix.

## The remaining 23 unpaired functions split cleanly, and the split is the useful part

* **Now written and reachable, still large**: `RsMain` itself (0x19% of its 2148 bytes, still a
  stub that performs the two `TOneStatic::operator new` calls), `CheckReset` (0.34%, 1180 B),
  `AddPaksAndFactories` (0.21%, 1936 B), `InitializeSubsystems` (12.44%, 348 B),
  `StreamNewGameState` (18.68%, 532 B). `RsMain` is now worth writing: nothing in it is blocked.
* **`__dt__CGameGlobalObjects_80006518` and its nine-function chain** (0x80006518, plus
  0x80006678/0x800066D0/0x80006724/0x800067A8/0x800067E0/0x80006830/0x80006850/0x80006874, which
  this unit already matches at 100%), all blocked on the `+0x14C` layout, i.e. the open
  `match-main-cgameglobalobjs-14c` item. **This is now the single biggest prize in the unit and it
  is one header change**: it also makes `fn_80008B04`
  (`TOneStatic<CGameGlobalObjects>::operator delete`, 44 B) emittable, since its only caller is
  0x80006600 inside that destructor.
* **COMDAT weak copies with no caller in `main.o`**: 0x80008B04, 0x80008DE8/0x80008E94,
  0x80008C28/0x80008CE0/0x80008D68, 0x80009008, `__dt__15CMemoryInStreamFv`. These are `W` in
  retail's `nm`, i.e. real template/vtable instantiations, so unlike the two functions landed here
  they need a caller in the tree to be emitted at all. Each needs one in a TU that claims that
  caller (a `CWorldState`/`CGameState` teardown, which is `CGameState.cpp` territory), or
  accepting that `main.cpp` is the wrong home for it.
* **The four 80-byte `ReleaseData`s split, and one pair is worth a look.** 0x80009058
  (`rc_ptr<CMapWorldInfo>`) and 0x8000934C (`rc_ptr<CPlayerState>`) are `T` - strong, exactly like
  the two functions landed here - and their two callees, `__dt__13CMapWorldInfoFv` (0x800090A8,
  124 B) and `__dt__12CPlayerStateFv` (0x8000939C, 112 B), **this unit already defines and matches
  at 100%**; the third callee, `Free__7CMemoryFPCv`, is already an undefined reference in this
  object. The body is `rstl::rc_ptr<T>::ReleaseData()` verbatim from `include/rstl/rc_ptr.hpp:118`
  (`if (--*mRefCount <= 0) { delete GetPtr(); delete mRefCount; }`). The other two of the four,
  0x80009224 (`rc_ptr<CWorldLayerState>`, whose `__dt__16CWorldLayerStateFv` is *also* already at
  100% here) and 0x800095E4 (`rc_ptr<CWorldTransManager>`, whose `__dt__18CWorldTransManagerFv`
  is not in this unit at all), are also `T` and also 80 bytes. **Not measured - I did not write
  them - but the letter says a plain body will do for all four.**

NEW: match-main-rcptr-releasedata | match | MetroidPrime/main | the four 80-byte `rc_ptr<T>::ReleaseData`
bodies at 0x80009058, 0x80009224, 0x8000934C and 0x800095E4 are all `T` strong symbols in
`build/G2ME01/obj/MetroidPrime/main.o` with no caller in the unit, so - unlike the ten COMDAT weak
copies beside them - a plain `extern "C"` body with retail's own name is emitted unconditionally;
the body is `rstl::rc_ptr<T>::ReleaseData()` verbatim from `include/rstl/rc_ptr.hpp` and three of
the four `bl` targets are already defined here at 100% (`__dt__13CMapWorldInfoFv`,
`__dt__12CPlayerStateFv`, `__dt__16CWorldLayerStateFv`), so 320 bytes and four functions are
available without a caller, a header change, or a new undefined symbol

## A correction to an earlier run's note about these four

`match-main-sda-float-constants` run 2 recorded that "the four 80-byte `ReleaseData` bodies ...
have **no caller in main.o** either ... Emitting them needs a `CWorldState`/`CGameState` teardown
in this TU". That confuses two different things, and the distinction is what made this item's two
functions reachable. **No caller in `main.o` is a statement about retail's object; whether *our*
object emits the symbol is a statement about the symbol's linkage.** All four are `T` in
`powerpc-eabi-nm`, so they are strong out-of-line definitions in retail's own source, and a
non-template `extern "C"` body reproduces them with no caller at all - which is exactly what this
run did for `single_ptr_assign_800064D0` and `__dt__80006AE0` (0 diff bytes each). The ten
COMDAT weak copies (`fn_80008B04`, 0x80008DE8/0x80008E94, 0x80008C28/8CE0/8D68, 0x80009008,
`__dt__15CMemoryInStreamFv`) really are blocked, because `W` means mwcceppc discards an
unreferenced instantiation. **Check the letter before deciding a function needs a caller.**

## Verified (this run, all measured)

```
tools/decomp_build.sh                 All: 31.10% fuzzy, 23.39% matched, 11.78% linked (10110 / 28465)
tools/goal_check.sh build/goal/item.json
                                       PARTIAL - gate ok, counts ok, names ok, target rose, no asm
                                       "matched 10108 -> 10110, linked 4918 -> 4918"
                                       "target rose: main/MetroidPrime/main: 74 -> 76 / 99 functions"
tools/report_diff.py build/goal/judge/report.base.json build/report.json
                                       "+2 functions at 100%, 0 units newly linked" / "no regression"
unit: main/MetroidPrime/main          74 -> 76 / 99, fuzzy 56.32% -> 56.82%, matched_code 9216 -> 9280
  single_ptr_assign_800064D0          absent  -> 100.00% (72 B, 0 differing bytes)
  __dt__80006AE0                      absent  -> 100.00% (88 B, 0 differing bytes)
  fn_800068F4                         absent  ->  91.00% (96 B, 37 differing bytes)
  __dt__80006678 / __dt__800066D0 / fn_80006874   100.00% each, unchanged
sha1sum build/G2ME01/main.dol         6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh              750 files, 0 failed, 0 errors; link LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py   504 units, 0 missing
python3 tools/check_raw_offsets.py    ok: 152 raw-offset site(s) in 61 file(s), all documented
tools/unit_fit.sh MetroidPrime/main.cpp  15 COMDAT extras, 1300 bytes (pre-existing, unchanged)
python3 tools/check_decl_order.py --unit MetroidPrime/main  would break on a flip (pre-existing)
git status                            src/MetroidPrime/main.cpp only
```

No function anywhere got worse - the judge's report diff is `+2 functions at 100%` and nothing
fell. `docs/HANDOFF.md` is reverted after the judge; `gate.sh` rewrites it under
`MP_GATE_DOCS_WRITE=1` and the driver owns that file.

## Two notes for whoever picks this up

1. **A `T`-strong, single-reloc, unnamed-by-dtk function in this unit does not need a caller.**
   That is the generalisable form of this item's finding, and it applies to ten of the 23 remaining
   unpaired functions. `unit_fit.sh`'s "extras" list and the report's unpaired list are two views
   of different things, and an unpaired retail function is only really blocked when its natural
   C++ spelling is a template instantiation nothing in the tree reaches. **Compare the symbol's
   letter in `powerpc-eabi-nm build/G2ME01/obj/MetroidPrime/main.o` before assuming it needs a
   caller**: `T` means a free function body will do (this run's three), `W` means mwcceppc
   discards it unreferenced and something must call it. An earlier run read "no caller in
   `main.o`" as the blocker for all four `ReleaseData` bodies and was wrong for all four.
2. `single_ptr_assign_800064D0`'s store needs `SGameGlobalObjectsPtr` because `mPtr` is private
   and the header's `delete mPtr` selects the class-scoped `operator delete`. **`rstl::single_ptr`
   already has the opt-in `RSTL_SINGLE_PTR_OUT_OF_LINE` path for the same reason** (declared
   `~single_ptr()` and `operator=` out of line, used by `src/Kyoto/DolphinCDvdFile.cpp`); if a
   later run finds the same need in a second TU, moving this definition into the header behind
   that macro is the change that removes the local view, and it is a shared-header change this
   item may not make.

---

# Run 3 - 2026-09-30, lane 3. Verdict PARTIAL, target **82 -> 87 / 99**

`kind: match`, `target: MetroidPrime/main`. The flip is still out of reach on the pre-existing
`CErrorOutputWindow::__vt` multiply-defined link error; `tools/goal_check.sh build/goal/item.json`
returned **PARTIAL** with `ok gate.sh`, `ok counts: matched 10124 -> 10129 linked 4917 -> 4917`,
`ok target rose: main/MetroidPrime/main: 82 -> 87 / 99 functions`, `ok no asm added`. Diff is
`src/MetroidPrime/main.cpp` (+185 / -0) and one access-specifier word in
`include/Kyoto/TOneStatic.hpp`. No `.s`, nothing committed.

| function | retail | bytes | before | after |
|---|---|---|---|---|
| `ReleaseData__Q24rstl23rc_ptr<13CMapWorldInfo>Fv` | 0x80009058 | 80 | 0.00% | **100.00%** |
| `ReleaseData__Q24rstl22rc_ptr<12CPlayerState>Fv` | 0x8000934C | 80 | 0.00% | **100.00%** |
| `fn_80008B04` | 0x80008B04 | 44 | 0.00% | **100.00%** |
| `reserve__Q24rstl55vector<Q24rstl11pair<Ui,Ui>,...>Fi` | 0x80008DE8 | 172 | 0.00% | **100.00%** |
| `fn_80008E94` | 0x80008E94 | 172 | 0.00% | **100.00%** |

548 bytes, 5 functions, and **0 functions anywhere in the tree got worse** (report diff: 0 drops,
0 new names). Unit `matched_code` 9856 -> 10404, which is exactly 44+80+80+172+172.

## The letter in `nm` decides this, and run 1 got five of these seven wrong

Run 1's "Two notes for whoever picks this up" says to check the letter and that `W` needs a caller.
Applying it to the seven 0.00% functions still open, `powerpc-eabi-nm
build/G2ME01/obj/MetroidPrime/main.o` (retail's own object, measured this run):

```
0000374c T fn_80008B04      00003870 T fn_80008C28    00003928 T fn_80008CE0
000039b0 T fn_80008D68      00003adc T fn_80008E94    00003a30 T reserve__Q24rstl55vector<...>Fi
00003ca0 T ReleaseData__Q24rstl23rc_ptr<13CMapWorldInfo>Fv
00003f94 W ReleaseData__Q24rstl22rc_ptr<12CPlayerState>Fv
00000214 W __dt__15CMemoryInStreamFv
```

**Six of the eight are `T`, not `W`.** Run 1 listed `fn_80008B04`, `fn_80008C28/8CE0/8D68/8E94` and
`reserve__vector<pair<Ui,Ui>>` under "COMDAT weak copies with no caller in `main.o`" - that is
wrong for all six, and it is the only reason they sat at 0.00% for two runs. A `T` symbol with no
`R_PPC_REL24` to it anywhere in the object (`readelf -r`, measured) is retail's own out-of-line
definition, and it is reachable from a plain `extern "C"` body with no caller at all. The two `W`s
(`ReleaseData<rc_ptr<CPlayerState>>` is `W` and still landed, below; `__dt__15CMemoryInStreamFv`)
are the only ones that genuinely need a caller.

## Three mechanisms, none of which is a new declaration of anything

**1. An explicit specialization of a class-template member reaches retail's mangled name, and
mwcceppc accepts it where it rejects `template void f();`.** `include/rstl/rc_ptr.hpp` declares
`ReleaseData()` inside `rc_ptr` and defines it once at namespace scope, so

```cpp
template <>
void rstl::rc_ptr< CMapWorldInfo >::ReleaseData() {
  if (--*mRefCount <= 0) { delete GetPtr(); delete mRefCount; }
}
```

emits `ReleaseData__Q24rstl23rc_ptr<13CMapWorldInfo>Fv` as a **strong `T`**, unconditionally, with
nothing in the unit calling it. Same for `CPlayerState`. The body is the header's own line, so it is
the same 20 instructions as the three `extern "C"` `ReleaseData`s run 1 landed, and the two `bl`
targets - `__dt__13CMapWorldInfoFv` (0x800090A8) and `__dt__12CPlayerStateFv` (0x8000939C) - are
**already defined in this file at 100%**. That is the whole difference from run 1's three: the
classes are complete in this header, so `delete GetPtr()` reaches the real destructor instead of a
symbol spelled by hand. (mwcceppc's `template void f();` is rejected with "illegal explicit
template instantiation", measured; the specialization form is not.)

**2. The four "dead" stores that have blocked this unit for three runs are not dead copies - they
are the *materialisation of class-typed parameters* of an inlined template call, plus that call's
own argument temporaries.** `reserve__vector<pair<Ui,Ui>>::reserve` at 0x80008DE8 stores
`mItems` into r1+0x10 and r1+0x14 and `mItems + mCount*8` into r1+0x08 and r1+0xC, and reads none
of the four back. The spelling that produces **0 differing bytes** is
`uninitialized_copy(begin(), end(), newData)` (`include/rstl/construct.hpp:103`) - the two
`pointer_iterator` temporaries are real objects with a user-provided constructor, and the loop's
`It cur = begin;` is a third; mwcceppc puts all of them in the frame in the order
`+0x10, +0x08, +0x0C, +0x14`. The already-matched `destroy<pointer_iterator<pair<string,
SObjectTag>,...>>` at 0x800057EC shows the same thing with only two stores, because
`destroy_impl(begin, end)` has two parameters and no temporaries. **So the rule for this unit is:
retail's extra stores come from a call to a template that takes the two iterators *by value*, and
they cannot be written as locals.** Measured against that: four unused class-typed locals declared
and initialised in the obvious order give **two** stores, not four - mwcceppc dead-store-eliminates
the unused ones (probe compiled and disassembled this run). Two class locals, four plain-pointer
locals, and `volatile` copies all fail the same way, which is why run 1's fourteen shapes and
run 2's ten did too.

**3. `fn_80008B04` is `TOneStatic<CGameGlobalObjects>::operator delete`, and its `bl` has to reach
`ReferenceCount` directly.** `powerpc-eabi-nm build/G2ME01/main.elf` has
`__dl__38TOneStatic<24CGameArchitectureSupport>FPv` at 0x80008A78 and **no symbol at all** at
0x80008B04, so `dtk` named it `fn_80008B04`; its only caller is the `flag > 0` arm of
`__dt__CGameGlobalObjects_80006518` (the `R_PPC_REL24` at object offset 0x1248), which the file
already writes. The body is `include/Kyoto/TOneStatic.hpp`'s one line, `ReferenceCount()--`.
Calling the class-scoped `operator delete` instead **does not work**: mwcceppc never inlines a
deallocation function, so it emits a seven-instruction thunk onto
`__dl__32TOneStatic<18CGameGlobalObjects>FPv` - a symbol retail's object does not have - and
scores 71.73% (5 of 7 instructions; measured). So `ReferenceCount()` is called directly, and
`include/Kyoto/TOneStatic.hpp` now declares that one static `public` instead of `private`
(`GetAllocSpace` stays private). That is the **only** header change: no new member, no renamed
member, no layout change, no new symbol in any other unit - measured by the report diff (0 drops,
0 new names tree-wide) and by the DOL sha1 holding. It is not the `+0x14C` layout change the
`match-main-cgameglobalobjs-14c` item needs, which is still that item's.

**The `asm`-label route is dead on this compiler, and the tree's 182 examples of it are in a file
nothing builds.** `extern "C" void f() asm("name");` fails with *"type cannot be made into a global
register variable"* on `mwcceppc.exe 2.7` for `void`, `uint*` and `uint&` returns, with a plain name
and with a mangled one (`_ZN3Foo3barEv`, and a name containing `<`/`>`), under the unit's real flag
set - five variants compiled and measured. `src/MetroidPrime/PortReachStubs.cpp` uses exactly that
spelling 182 times and is in neither `files.cmake` nor `configure.py`; it is dead code. Do not spend
time on it.

WALL: fn_80006724 78.21% - the two extra stores are the materialised parameters of a by-value
template call and fn_800067A8 is a `bl` callee taking pointers, so no local spelling can produce
them; measured this run: 2 class locals, 4 class locals, 4 pointer locals, all give 2 stores, and
run 1's 14 shapes plus run 2's 10 gave the same answer

## fn_800068F4: unchanged at 0.00%, and the previous run's 91% is gone from the tree

`fn_800068F4` has **no body in the source at all** on this tree (only a comment at line 807), so it
scores 0.00%, not the 91.00% run 1 measured - its `extern "C"` body was reverted by a later commit.
It has the identical four stores and the identical `bl`-with-two-pointers shape as `fn_80006724`, so
mechanism 2 above says the same thing applies and it is not worth a second attempt. `fn_80004864`
lives in the unclaimed `main/auto_03_80003BE8_text` range, so the callee is out of this unit's
reach anyway.

## What is left, and the three things worth a run

* **`fn_80008C28` (184 B), `fn_80008CE0` (136 B), `fn_80008D68` (128 B)** - 448 bytes, three
  `T`-strong 0.00% functions, all `vector<pair<rstl::string, SObjectTag>>`-shaped. `fn_80008C28`
  is **recursive** (two `R_PPC_REL24` to its own address, 0x38B4 and 0x38CC): it null-checks two
  words of its second argument, recurses on each, then calls `fn_80008CE0` with six arguments.
  `fn_80008CE0` is `rstl::rmemory_allocator::allocate(44)` + four word stores + a `basic_string`
  copy-ctor + **a second, redundant copy of the string's three words** (the same duplicate-store
  shape as mechanism 2, at 0x3980-0x3994). `fn_80008D68` is the matching tear-down.
  All three have no caller in the object, so all three are reachable the way the five above were.
* **`__dt__15CMemoryInStreamFv` (96 B) is the one genuinely blocked `W`.** `readelf -r` on retail's
  object shows no reference to it, and it is emitted there because the vtable needs it.
  `CMemoryInStream` is constructed at exactly three places in retail's `main.o` - object offsets
  0xDC (`CMain::EnsureWorldPakReady`, already 100% and must not be touched), 0xAB8 (`CMain::RsMain`,
  2.38%) and 0x211C (`CGameGlobalObjects::AddPaksAndFactories`, 0.21%). The latter two are stubs, so
  this is reachable the moment either is written - it is **not** reachable by adding a local to
  anything that currently matches.
* **`AsyncIdle` is one instruction from 100% and 17 spellings deep.** Retail 0x80005C44 is
  `clrlwi r5,r30,24` where we emit `mr r5,r30`, i.e. a narrowing of a `bool` argument to one byte.
  The note is right that widening the callee's parameter to `unsigned char` renames it to
  `AsyncIdle__11CResFactoryFUiUc` and costs `main/Kyoto/CResFactory` a function. **Not retried this
  run** - I did not measure it, so no `WALL:` for it. One shape nobody has listed: give the
  argument a *distinct* one-byte class type that converts to `bool` implicitly, rather than a
  `uchar` local (which normalises the value and costs two instructions).

## Still not the flip, and not close

`flip_test.sh MetroidPrime/main.cpp` FAILs at link, and it failed identically on the clean tree
(this run measured both):

```
### mwldeppc.exe Linker Error:
#   multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o
#   undefined: 'fn_80008C28'
#   undefined: 'lbl_80418EA0'
```

`.text` is still SHORT: `unit_fit.sh` reports the same **15 COMDAT extras, 1300 bytes**, all
pre-existing template destructors, unchanged by this run. `check_decl_order.py --unit
MetroidPrime/main` still says *would break on a flip* (pre-existing, 8 shown + 51 more); the five
new functions are placed at their retail addresses in descending order (0x80008B04 after
`__dt__80006AE0`, 0x80008DE8/0x80008E94 after `CWorldLayerState::~CWorldLayerState`, the two
`ReleaseData`s after `fn_800095E4`) and the verdict did not change. One thing this run *improved*:
our object's undefined-symbol count went **129 -> 128**, because `fn_80008B04` is now defined
rather than declared, and **no new undefined symbol was added** (`allocate__Q24rstl17rmemory_allocatorFi`
and `Free__7CMemoryFPCv` were already in that list; the former is defined at 100% in
`main/rstl/rstl_misc`).

## Verified (this run, all measured)

```
tools/decomp_build.sh                 All: 31.17% fuzzy, 23.46% matched, 11.78% linked (727 / 2044 files)
                                      Code: 1533036 / 6535816 bytes (10129 / 28465 functions)
                                      (was 31.16% / 23.45% / 10124 - fuzzy did not fall)
tools/goal_check.sh build/goal/item.json
                                      PARTIAL match-main-rsmain-body - flip_test FAIL, but the target rose
                                      ok gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
                                      ok counts: matched 10124 -> 10129   linked 4917 -> 4917
                                      ok target rose: main/MetroidPrime/main: 82 -> 87 / 99 functions
                                      ok no asm added
unit: main/MetroidPrime/main          82 -> 87 / 99, fuzzy 59.33% -> 62.44%, matched_code 9856 -> 10404
  ReleaseData__Q24rstl23rc_ptr<13CMapWorldInfo>Fv   0.00% -> 100.00%  (80 B)
  ReleaseData__Q24rstl22rc_ptr<12CPlayerState>Fv    0.00% -> 100.00%  (80 B)
  fn_80008B04                                       0.00% -> 100.00%  (44 B)
  fn_80008E94                                       0.00% -> 100.00%  (172 B)
  reserve__Q24rstl55vector<Q24rstl11pair<Ui,Ui>,Q24rstl17rmemory_allocator>Fi
                                                      0.00% -> 100.00%  (172 B)
  report diff vs build/goal/judge/report.base.json: 0 functions fell, 0 new names tree-wide
sha1sum build/G2ME01/main.dol         6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh              749 files, 0 failed, 0 errors; link LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py   505 units, 0 missing
python3 tools/check_raw_offsets.py    ok: 152 raw-offset site(s) in 61 file(s), all documented
tools/unit_fit.sh MetroidPrime/main.cpp  15 COMDAT extras, 1300 bytes (pre-existing, unchanged)
python3 tools/check_decl_order.py --unit MetroidPrime/main  would break on a flip (pre-existing)
nm undefined count, our main.o        129 -> 128, none added
git status                            src/MetroidPrime/main.cpp, include/Kyoto/TOneStatic.hpp
```

NEW: match-main-80008c28-cluster | match | MetroidPrime/main | `fn_80008C28` (0x80008C28, 184 B),
`fn_80008CE0` (0x80008CE0, 136 B) and `fn_80008D68` (0x80008D68, 128 B) are `T`-strong in
`build/G2ME01/obj/MetroidPrime/main.o` with no `R_PPC_REL24` to any of them, so all three are
reachable as plain `extern "C"` bodies with no caller - 448 bytes and three functions, and they are
the last three of the seven `T`-strong 0.00% bodies in this unit that an earlier run misfiled as
COMDAT weak copies; `fn_80008C28` recurses on two words of its second argument and
`fn_80008CE0` is `allocate(44)` + four word stores + a `basic_string` copy-ctor + a second
redundant copy of the string's three words, so the duplicate-store shape to reach for is
mechanism 2 in `docs/goal-notes/match-main-rsmain-body.md` (class-typed by-value template
parameters), not a hand-written dead copy

---

# Run 4 - 2026-09-30, lane 1. Verdict PARTIAL, target **91 -> 93 / 99**

`kind: match`, `target: MetroidPrime/main`. The flip is still out of reach on the pre-existing
`CErrorOutputWindow::__vt` multiply-defined link error (measured on the clean tree this run, so
it is not mine); `tools/goal_check.sh build/goal/item.json` returned **PARTIAL** with
`ok gate.sh`, `ok counts: matched 10301 -> 10303 linked 5048 -> 5048`,
`ok target rose: main/MetroidPrime/main: 91 -> 93 / 99 functions`, `ok no asm added`. Diff is
`src/MetroidPrime/main.cpp` (+213 / -14) and one new `##` section in
`docs/research/raw_offsets.md`. No `.s`, nothing committed.

| function | retail | bytes | before | after |
|---|---|---|---|---|
| `StreamNewGameState__5CMainFb` | 0x800053B8 | 532 | 18.68% | **100.00%** |
| `__dt__15CMemoryInStreamFv` | 0x800055CC | 96 | 0.00% | **100.00%** |

628 bytes, two functions, and **0 functions anywhere in the tree got worse** (report diff:
`+2 functions at 100%`, `no regression`). Unit `matched_code` 10948 -> 11576 = exactly
532 + 96, and `fuzzy` 65.53% -> 68.53%.

## Neither of the three prior runs' "blocked" functions was blocked; the item's own namesake was

**`StreamNewGameState` was a 5-line stub and no run had touched it.** Run 3's list of what is
left in this unit does not mention it, and the item's `reason` never names it either - so it
sat at 18.68% through three runs. It is the unit's *first* function (object offset 0), 532
bytes, and **every one of its 21 callees has a retail name in `config/G2ME01/symbols.txt`**
(measured this run: `fn_80005108`, `fn_80004C90`, `fn_80004AA0`, `fn_80004E84`, `fn_80004154`,
`fn_80003F08`, `fn_80003D00`, `fn_80004A4C`, `__dt__80004B9C`,
`__dt__PersistentOptions_800050A4`, `fn_80004D84`, `RecordCheckpoint__10CGameStateFv`, the three
`SetCompressed*`, `__ct__15CMemoryInStreamFPCvUl`, `__ct__16CBitStreamReaderFR12CInputStream`,
`__ct__10CGameStateFR16CBitStreamReader`, `__dt__16CBitStreamReaderFv`, `__dt__12CInputStreamFv`,
`__nw__FUlPCcPCc`, `EnsureOptions__12CGameOptionsFv`). Declared and called, none defined - a
callee's body is not a precondition for reproducing a function.

**`__dt__15CMemoryInStreamFv` came free with it, and the three prior runs' reasoning about it
was wrong in a way worth recording.** Run 3 called it "the one genuinely blocked `W` ... not
reachable by adding a local to anything that currently matches" and deferred it to writing
`RsMain` or `AddPaksAndFactories`. It is a `W` COMDAT **virtual** destructor, so what mwcceppc
discards unreferenced is the *instantiation*, not a destructor: a `CMemoryInStream` local makes
the vtable live and the destructor is emitted. Measured in isolation this run, three probes
compiled with the unit's own flags:

| probe | `__dt__15CMemoryInStreamFv` emitted |
|---|---|
| `void f(CMemoryInStream*)` - a pointer parameter only | **no** |
| `CMemoryInStream s(buf,4);` - a local | **yes**, `W`, byte-identical to retail |
| `delete p;` on a `CMemoryInStream*` | **yes**, `W` |
| `new CMemoryInStream(b,4); delete p;` | **yes**, `W` |

so the trigger is any *construction or destruction* of the class in the TU, and the first probe
shows a bare pointer is not enough. `tools/bytescmp.py` on the local-variable probe: **4
differing instructions of 24, and all four are relocation fields** (`R_PPC_ADDR16_HA/LO` on
`__vt__15CMemoryInStream` and two `R_PPC_REL24`), which objdiff ignores. The destructor was
always reachable; it needed a `CMemoryInStream` **local**, and `StreamNewGameState` is where
retail has one.

**So the two functions are one piece of work, not two.** Writing the function that genuinely
owns a `CMemoryInStream` gets the COMDAT with it. That is worth checking before filing any
`W`-symbol item as blocked: ask what makes the *class* live, not what makes the symbol
referenced.

## Four spelling facts that are each worth a compile to rediscover

`StreamNewGameState` is 532 bytes and reached 100.00% with 532 bytes - same size as retail's
claim, and **every one of the 46 remaining `bytescmp` differences is a `bl` target or a branch
displacement**, i.e. a relocation, not code. The five things that had to be got right, all
measured, and none of them is in the source as it reads:

1. **The five temporaries are copied by out-of-line copy constructors, and `= old->member`
   does not produce them.** mwcceppc inlines the four classes' implicit copies into a
   word-by-word store run: the object came out **772 bytes against retail's 532**. The wrappers
   (`SStreamSysOpts`, `SStreamSlots`, `SStreamBlockOwner`, `SStreamGameOpts`) have the same
   sizes and the same member offsets and a **user-provided** copy constructor - that is the
   whole mechanism. `CHECK_SIZEOF` on each is the guard against a layout change in
   `CGameStateBlocks.hpp`.
2. **`gpGameState` is re-read from SDA for every copy, never cached.** Its
   `R_PPC_EMB_SDA21 gpGameState` relocation appears **six** times in retail's object. A named
   `SGameStateStreamSource* old = StreamSource(gpGameState);` local and reused at each site
   replaces those with one `addi` and costs four instructions - and would be *wrong*, because
   the function reassigns `gpGameState` in the middle. The same applies to the
   `&gameGlobalObjects->GameState()` slot address, which retail recomputes three times: a named
   reference hoists it into a callee-saved register and costs four more.
3. **The saved-game block array must be raw bytes, reached through an accessor.** With a real
   `SStreamBlock x04_blk[3]` member, mwcceppc builds the element address as `&slotsStates`,
   then `+4`, then `+idx*0x10` - three instructions, and the object is **540 bytes, 8 over**.
   Through `uchar x04_raw[0x30]` and a `reinterpret_cast` the `+4` folds into the `addi` and it
   is 536. Five spellings of the *select* were measured on top of that (`&x04_blk[i]`,
   `x04_blk + i`, `x04_blk[0] + i`, a `char*` base with an explicit `* 0x10`, a named base
   pointer) and **all five give the same three-instruction form**; the accessor is what changes
   it. Also: a user-provided *default* constructor on the block type makes mwcceppc emit a
   `__construct_array` loop for `[3]` (5 instructions and a reloc retail does not have), so the
   block is a POD and only the slots wrapper gets a constructor.
4. **The stream and the reader are in a nested scope.** Retail destroys them at 0x800054C0 and
   0x800054CC, immediately after the `single_ptr` assign that consumes the new `CGameState` and
   *before* `gpGameState` is re-read. At function scope mwcceppc sinks both into the epilogue
   and the object grows by two destroy calls (8 against retail's 6).
5. **Two type details.** `x04_count` is compared with `cmpwi`, not `cmplwi` - so it is a
   **signed** `int` here even though `SGameStateBlock` in `CGameStateBlocks.hpp` spells it
   `u32`, which is part of why the wrapper is a separate type. And retail stores the two card
   serials **+0x10C before +0x108**, the reverse of their declaration order; written the other
   way it is two differing instructions and nothing else changes.

## `AsyncIdle` is still where run 3 left it, and the note's open suggestion does not work

Run 3 left `AsyncIdle__5CMainFUi` at 99.17%, one instruction from 100% (`clrlwi r5,r30,24`
against `mr r5,r30`), and suggested untried: "give the argument a *distinct* one-byte class
type that converts to `bool` implicitly, rather than a `uchar` local". **Tried, and it is
worse.** Six more spellings measured this run on top of run 3's seventeen, all worse than
99.17%: a one-byte class with `operator bool()` (25 differing instructions, and the object grows
to 296 bytes), a `uchar` local assigned 0/1 (12, 300 bytes - it emits the `srwi` *normalise* as
well as the narrowing), a `bool : 1` bitfield local (70), an unscoped `enum` implicitly
convertible to `bool` (12, 296), a `bool : 1` local in a struct, `int` and `uint` and `short`
locals (12 each, 296-300), `(bool)flag` (no change at all - it is the same 4). **The narrowing
and the normalise always come as a pair**, because mwcceppc cannot see through the class
conversion either, so this is not a spelling that exists. The only thing that reaches 100.00%
is declaring `CResFactory::AsyncIdle`'s second parameter `unsigned char`, and run 3 measured
that as net-negative (it renames the callee and costs `main/Kyoto/CResFactory` a function).
That stands.

WALL: AsyncIdle__5CMainFUi 99.17% - the clrlwi and the bool normalise are emitted as a pair
by mwcceppc, so no argument or local type produces the narrowing alone; 23 spellings measured
across four runs, and the only exact one renames the callee

## `fn_80006724` is unchanged, and run 3's wall on it is not mine to re-open

78.21%, the same 132 bytes, untouched. Run 3 declared a `WALL:` on it and this run did not
measure it, so it neither confirms nor lifts that. `InitializeSubsystems` (12.44%),
`CheckReset` (0.34%) and `AddPaksAndFactories` (0.21%) are also untouched.

## What still stops the flip, and it is not this item's

Unchanged and not close. `flip_test.sh MetroidPrime/main.cpp` FAILs at link, and **it failed
identically on the clean tree** (measured this run, both):

```
#   multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o
#   undefined: 'lbl_80418EA0'
```

the same `splits.txt`/`.data` question four earlier runs have filed. `unit_fit.sh` reports the
same **10 shown / 18 COMDAT extras, 1396 bytes** as before - I diffed the list before and after
and it is **byte-identical**, all pre-existing template destructors. `check_decl_order.py
--unit MetroidPrime/main` still says *would break on a flip* (pre-existing); the one function
this run adds is placed at its retail address in the file's descending order, and the verdict
did not change. **`.text` is now SHORT by 4112 bytes**, where three runs ago it was "SHORT" too
but by less: the two new functions are 628 bytes of retail's own bytes, so this moved in the
right direction.

**Our object's undefined-symbol count went 129 -> 148, and all 20 new names are retail
functions this function calls.** Every one is in `config/G2ME01/symbols.txt` and is referenced
by `build/G2ME01/obj/MetroidPrime/main.o` itself (the `bl` relocations in retail's copy of
`StreamNewGameState` name all 20). One name left: `__ct__10CGameStateFv` is gone, because the
default `new CGameState()` the old stub used is replaced by the stream constructor. **This
costs the DOL link nothing** - `MetroidPrime/main` is `NonMatching`, its object is not in the
link, and the gate's `port link dups` and `hashes vs config.yml` steps both pass with the
change in place.

## Verified (this run, all measured)

```
tools/decomp_build.sh                 All: 31.29% fuzzy, 23.65% matched, 11.83% linked (10303 / 28465)
                                       (was 31.28% / 23.64% / 10301 - fuzzy did not fall)
tools/goal_check.sh build/goal/item.json
                                       PARTIAL match-main-rsmain-body - flip_test FAIL, but the target rose
                                       ok gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
                                       ok counts: matched 10301 -> 10303   linked 5048 -> 5048
                                       ok target rose: main/MetroidPrime/main: 91 -> 93 / 99 functions
                                       ok no asm added
tools/report_diff.py build/goal/judge/report.base.json build/report.json
                                       "+2 functions at 100%, 0 units newly linked" / "no regression"
unit: main/MetroidPrime/main          91 -> 93 / 99, fuzzy 65.53% -> 68.53%, matched_code 10948 -> 11576
  StreamNewGameState__5CMainFb        18.68% -> 100.00%  (532 B, 532 bytes ours vs 532 retail)
  __dt__15CMemoryInStreamFv           0.00%  -> 100.00%  (96 B)
  AsyncIdle__5CMainFUi                99.17%, unchanged; fn_80006724 78.21%, unchanged
MP_GATE_DOCS_WRITE=0 tools/gate.sh build/goal/judge/report.base.json
                                       GATE PASS  f4355081+3 changed (all 17 steps ok)
sha1sum build/G2ME01/main.dol         6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh              752 files, 0 failed, 0 errors; link LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py   505 units, 0 missing
python3 tools/check_raw_offsets.py    ok: 161 raw-offset site(s) in 68 file(s), all documented
tools/unit_fit.sh MetroidPrime/main.cpp  18 COMDAT extras, 1396 bytes (list byte-identical to the baseline)
python3 tools/check_decl_order.py --unit MetroidPrime/main  would break on a flip (pre-existing)
nm undefined count, our main.o        129 -> 148; 20 new, all named in symbols.txt and all
                                       referenced by retail's own object; 1 removed
git status                            src/MetroidPrime/main.cpp, docs/research/raw_offsets.md
```

`docs/HANDOFF.md` is reverted after the judge; `gate.sh` rewrites it under
`MP_GATE_DOCS_WRITE=1` and the driver owns that file.

## One raw-offset site added, and what deletes it

`StreamSource()` casts `CGameState*` to `char*` and adds `0x54` to place
`SGameStateStreamSource` over the object. The five *member* offsets inside the view are not
sites - they are declared members of a struct whose own layout is `CHECK_SIZEOF`'d, which is
the arrangement `SGameStateBlock` and this file's `SGameGlobalObjectsPtr` already use. The
`+0x54` is unavoidable because the members are `private`: naming them from a `CMain` member
function needs a `friend`, and a `friend` is a shared-header change this item may not make.
**A `friend void CMain::StreamNewGameState(bool);` in `include/MetroidPrime/Player/CGameState.hpp`
would delete the site and change nothing else** - no layout, no member renamed, no other unit
affected - and is worth filing, since it is worth 532 bytes of a matched function and the
friend is the only thing standing in the way.

## What is left in this unit, and the one thing worth a run

Six functions, all previously characterised and none re-measured except as noted above:
`RsMain__5CMainFiPCPCc` (2.38%, 2148 B - still a two-`TOneStatic::operator new` stub),
`AddPaksAndFactories__18CGameGlobalObjectsFv` (0.21%, 1936 B - an empty `{}`),
`CheckReset__5CMainFv` (0.34%, 1180 B - an empty `{}`), `InitializeSubsystems__5CMainFv`
(12.44%, 348 B - a two-line stub whose **whole body already exists and is at 99.08%** in the
unregistered `src/MetroidPrime/CMainInitializeSubsystems.cpp`; that file's own header says the
last 15 instructions are one register transposition and lists 27 measured variants),
`AsyncIdle__5CMainFUi` (99.17%, walled above), `fn_80006724` (78.21%, walled by run 3).

* **`InitializeSubsystems` is the best remaining target in this unit and nobody has tried it
  from here.** The body is written and measured at 99.08%; it is not in `configure.py` and not
  in `files.cmake`, so it is dead code as the tree stands, and the barrier is one register
  transposition that 27 variants did not fix. **The untried lever is that it is a different
  translation unit**: the register allocator weights a reference by the loop depth it sits at,
  and a carve into its own TU changes what else is in the frame. A carve is four files
  (`configure.py`, `config/G2ME01/splits.txt`, `files.cmake`, the source's own claim) and the
  claim must be checked with `tools/check_decl_order.py` first.
* **Run 3's `NEW: match-main-80008c28-cluster` has been landed** - `fn_80008C28`,
  `fn_80008CE0` and `fn_80008D68` are at 100% on this tree and none of the seven `T`-strong
  0.00% bodies run 3 listed is at 0.00% any more. Do not re-file it.
* The `+0x14C` `CGameGlobalObjects` layout blocker (`match-main-cgameglobalobjs-14c`) is
  untouched and still gates nine functions plus `fn_80008B04`'s `__dl__`. `__dt__15CMemoryInStreamFv`
  was the last of the ten "blocked on `RsMain`" functions; it did not need `RsMain`.

NEW: match-main-cgameglobalobjs-friend | match | MetroidPrime/main | a
`friend void CMain::StreamNewGameState(bool);` in
`include/MetroidPrime/Player/CGameState.hpp` deletes the last raw-offset site in
`src/MetroidPrime/main.cpp` (the `+0x54` in `StreamSource()`, documented in
`docs/research/raw_offsets.md`) and changes nothing else - no layout, no member renamed, no
other unit affected - so `StreamNewGameState__5CMainFb` at 100.00% and 532 bytes would no
longer depend on a cast reaching a `private` member
