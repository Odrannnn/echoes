# match-main-ciengametweakmanager-dtor

`kind: match`, `target: MetroidPrime/main`. Verdict **PARTIAL**: the flip is out of reach, the target
rose **63 -> 68** functions, and every other check in `goal_check.sh` is green.

## The item's premise, corrected

The item names `CIEngineTweakManager::~CIEngineTweakManager`. **There is no such symbol in retail.**
`config/G2ME01/symbols.txt` and the DOL's own symbol table (`powerpc-eabi-nm build/G2ME01/main.elf`)
have `CInGameTweakManager` (`GetTweakValue__19CInGameTweakManager...` at 0x8016BDEC,
`gpTweakManager` at 0x80418EC0) and nothing resembling `CIEngineTweakManager`. The cluster the
referring item found is real, though, and it is the tweak manager's:

`CGameGlobalObjects::~CGameGlobalObjects` at 0x80006518 tears down `+0x14C` with
`addi r3,r30,332 ; li r4,-1 ; bl 80006678`, and `+0x14C` is
`rstl::single_ptr<CInGameTweakManager> inGameTweakManager`
(`include/MetroidPrime/CGameGlobalObjects.hpp`; the constructor allocates it with `li r3,16` at
0x80008508 and runs `fn_8016C230` on the result at 0x80008514). So 0x80006678 is that
`single_ptr`'s destructor and 0x800066D0 is `~CInGameTweakManager`. That is the whole of
0x80006678-0x800068D4, and it is what this item lands.

## What is in the diff

`src/MetroidPrime/main.cpp` only, +173 lines: eight `extern "C"` definitions, retail's own
placeholder names, in descending retail-address order (0x80006874 first).

```
   0x80006678  __dt__80006678    88 B  100%    single_ptr<CInGameTweakManager>::~single_ptr()
   0x800066D0  __dt__800066D0    84 B  100%    CInGameTweakManager::~CInGameTweakManager()
   0x80006724  fn_80006724      132 B   78.21% its body: destroy the tweak table, free it
   0x800067A8  fn_800067A8       56 B   69.79% passes the two iterators on
   0x800067E0  fn_800067E0       80 B   90.00% the destroy loop
   0x80006830  fn_80006830       32 B  100%
   0x80006850  fn_80006850       36 B  100%
   0x80006874  fn_80006874      128 B  100%    CTweakValue's destructor
```

`main/MetroidPrime/main` **63 -> 68 / 99**; `All:` matched 10096 -> 10101, linked 4918 -> 4918.
`tools/report_diff.py` against the judge's baseline: `+5 functions at 100%, 0 units newly linked`,
`no regression` - nothing anywhere got worse.

**Why the `extern "C"` names.** objdiff pairs functions **by name**, and dtk could not demangle any
of these, so retail's DOL symbol table literally holds `__dt__80006678` and friends. The natural C++
spelling is already emitted by this unit as the weak
`__dt__Q24rstl33single_ptr<19CInGameTweakManager>Fv`, which objdiff cannot pair with
`__dt__80006678`; that is why retail's 0x80006678 read 0.00% for ever. Retail's bytes written under
retail's own name is what fixes it. This is the "naming the undefined functions" step
`docs/goal-notes/match-main-cmain-0x91-bitfield.md` ended with, and `tools/report_diff.py`'s own
docstring calls it one of the two cheapest real improvements in the project.

The five at 100% are **byte-identical** to dtk's retail object, `bl`/`b` fields masked (both objects
carry those as `R_PPC_REL24`), every other field compared exactly - including the displacement of
each conditional branch, which is a real value in both. Two probes did that:
`.tmp/opencode/dtor/cmp.sh` (instruction text, this unit's exact `build.ninja` flags) and
`.tmp/opencode/dtor/bytecmp.py` (raw words). Both are untracked scratch; the diffs and the sizes are
recorded here so the next run does not have to rebuild them.

## Two codegen rules this cost, both worth keeping

**1. This unit's `-pragma "inline_max_size(125)"` outlines the implicit `CTweakValue` destructor.**
`build.ninja`'s `mwcc_sjis` rule carries `-inline deferred,noauto`, `-i extern/musyx/include`,
`-DMUSY_*` and `-pragma "inline_max_size(125)"`; **`tools/probe_cc.sh` carries none of the last
three**, and probing with it inlines a destructor the real unit does not. With the real flags,
`self->~STweakValue()` turns `fn_80006874` into a 7-instruction thunk calling
`__dt__11STweakValueFv`, and the unit scored **56.09%** where the same source is 100%. `inline` and
`__inline` on the destructor change nothing (both measured). **Naming the three members' destructors
explicitly** emits retail's shape exactly, because each call brings its own `addic. r0,r30,off /
beq` guard - and the two dead tests in retail's first group are exactly `&mAudio` and
`&mAudio.mFileName`. Probe with the wrong flags and you will "prove" a spelling works that does not.

**2. The deleting-flag tail has to be inside the `this == nullptr` check.** Spelled as a sibling
`if (flag > 0) CMemory::Free(self)`, the null branch lands on the `extsh.` instead of on the
epilogue, because the tail is no longer part of the guarded block: `beq`'s displacement comes out
`0x0c` where retail has `0x1c`. That is **one nibble of one word** - 99.76% instead of 100%, on a
function that is otherwise byte-identical, and invisible to any instruction-text diff that masks
branch targets. It bit `__dt__80006678`, `__dt__800066D0` and `fn_80006874` (all three measured at
99.76-99.84% before the fix). **Masking `b`/`beq` targets when diffing a function hides exactly this
class of miss**; compare the raw words.

`short`, not `bool`, for the flag: `extsh.` in retail and `extsb.` for a `bool`, which is what
`CMain`'s flag-taking accessors use.

## What is left, measured

`fn_80006724`, `fn_800067A8` and `fn_800067E0` are the same code as retail with mwcceppc scheduling
the two outgoing arguments differently. Spellings tried, all with this unit's exact flags:

- `fn_800067E0` (90.00%) - the only difference is where `lwz r31,0(r3)` lands in the prologue;
  retail puts it between the two `stw`s, mwcceppc puts it after `mr r30,r4`. Ten spellings, all
  90.00%: `it = *first; end = last` / `end = last; it = *first` / both with `const` on `end` /
  separate `it` declaration and assignment / `for` with the init in the header / `++it` vs `it += 1`
  vs `it = it + 1` / pointer-arithmetic stride instead of `++` / `fn_80006830` defined rather than
  forward-declared. r30 must hold the `STweakValue**` (the loop reloads `0(r30)`), which rules out
  caching the end *value*.
- `fn_800067A8` (69.79%) - retail loads r4's target into **r5** first and r3's into r0, and
  interleaves `mflr`/`stw r0,20(r1)` with those loads; mwcceppc loads r3 first and reuses r0.
  `l = *last; f = *first` (reverse order) and `f = *first; l = *last` both tried.
- `fn_80006724` (78.21%) - retail stores the two iterators **twice each** (r5 = `data + count*0x48`
  into r1+0x0C *and* r1+0x08, r0 = `data` into r1+0x10 and r1+0x14) and passes r1+0x14 / r1+0x0C.
  Five spellings tried, none produce the duplicate pair: `first`/`last` locals, a `data` local,
  a `count` local, a second pair of locals, and reassigning `first`/`last` after the call. The
  version kept is 20 bytes short of retail's 132.

WALL: fn_800067E0 90.00% - ten spellings, all identical but for one prologue load's position
WALL: fn_800067A8 69.79% - two operand orders, retail needs r5 for the first load and r0 for the second
WALL: fn_80006724 78.21% - retail stores each iterator twice into two stack slots; five spellings emit one store each

## Why the flip is out of reach (unchanged, and now measured)

`flip_test.sh` fails at link and reverts cleanly (tree rebuilt to DOL
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`). Two independent blockers, both pre-existing:

- `multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o` - mwcceppc lays a 28-byte
  copy of that vtable in this object because the class's key functions are all undefined here, and
  `config/G2ME01/splits.txt` does not claim that `.data` for this unit. `src/MetroidPrime/main.cpp`
  already says so above `~CErrorOutputWindow`.
- `undefined: fn_80008C28`, `fn_80009224`, `lbl_80418EA0` and the rest of the cluster the previous
  item listed.

`tools/unit_fit.sh MetroidPrime/main.cpp`: 16 functions present in ours but not in the retail unit
object, 1340 bytes, all COMDAT weak copies (template/`rc_ptr`/`optional_object` destructors and the
`TOneStatic` accessors). That alone makes the unit unflippable.

## Verified

```
tools/goal_check.sh build/goal/item.json   PARTIAL (exit 0): gate.sh ok, counts 10096 -> 10101,
                                           linked 4918 -> 4918, check_symbol_names ok, All: 31.08%
                                           fuzzy / 23.37% matched / 11.78% linked (10101 / 28465),
                                           flip FAIL, target rose 63 -> 68, no asm added
tools/report_diff.py <base> build/report.json   +5 functions at 100%, no regression
sha1sum build/G2ME01/main.dol              6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py        504 units, 0 missing
python3 tools/check_decl_order.py          ok: 958 checked, 31 permuted, all accounted for
```

No asm. The diff is `src/MetroidPrime/main.cpp` plus the state-block counts in `docs/HANDOFF.md`,
which `gate.sh` rewrote under `MP_GATE_DOCS_WRITE=1` (10096 -> 10101 matched, DOL units
8685 -> 8690); I did not touch that file by hand and the driver rewrites it anyway.

## For the next run

`+0x14C`'s sibling at `+0x148` and the tail of the same destructor are the obvious continuation:
`single_ptr_assign_800064D0` (72 B, `rstl::single_ptr<CGameGlobalObjects>::operator=(CGameGlobalObjects*
const)` - `delete mPtr; mPtr = ptr; return *this;`, which `include/rstl/single_ptr.hpp` already has,
just inlined), `__dt__CGameGlobalObjects_80006518` (264 B, `~CGameGlobalObjects` - retail's bytes are
readable straight off dtk's object at 0x80006518) and `__dt__80006AE0` (88 B, that
`single_ptr<CGameGlobalObjects>`'s destructor, calling the 264-byte one). All three are the same D0
shape this item landed at 100%, and all three are in the same neighbourhood.

NEW: match-main-cgameglobalobjects-dtors | match | MetroidPrime/main | the 0x800064D0 / 0x80006518 / 0x80006AE0 single_ptr<CGameGlobalObjects> trio is the same 72/264/88-byte D0 shape this item landed at 100% for the tweak-manager pair

---

# Second run (2026-09-30): the two sub-100% functions in this block are now byte-identical

Verdict **PARTIAL** again, but a different pair: **`fn_800067A8` (69.79% -> 100%) and
`fn_800067E0` (90.00% -> 100%)**. `main/MetroidPrime/main` **72 -> 74 / 99**,
`All:` **10105 -> 10107** matched, linked 4918 -> 4918, `report_diff.py` against the judge's
baseline: `+2 functions at 100%, no regression`. So the previous run's two `WALL:` lines for
`fn_800067E0` and `fn_800067A8` are **superseded** - the spellings they list are real failures, but
the two effects below are what actually moved the code, and neither is in that list.

Diff: `src/MetroidPrime/main.cpp` only, and only three things in it - two signatures, one body, and
the comments around them. No asm, no other file, no judge-owned path.

## Effect 1: a `const` on the *pointer* parameter decides register liveness (both functions)

`fn_800067A8` was 69.79% purely because mwcceppc gave both `lwz`s of the two locals the **same
register** (r0) and emitted load/store/load/store, so only one value was live at a time; retail
keeps both live, in r5 and r0. Declaring the parameters `STweakValue* const*` (pointer to
*const* pointer, so the callee cannot write through them) is what makes the two values
simultaneously live, and the two stores then keep the two different registers. Measured on the
plain-`STweakValue**` signature: 7 of 14 instructions differ; with `const*`: 4, and with the body
below, 0.

`fn_800067E0` needs the same trick on **one parameter only**: `STweakValue* const* first,
STweakValue** last`. The whole 90% miss was the position of one `lwz r31,0(r3)` in the prologue
(retail puts it between the two callee-save `stw`s, mwcceppc put it after `mr r30,r4`), and
qualifying only `first` hoists it into the prologue exactly where retail has it. `const` on `last`
as well is **not** it: that loses the `mr r30,r4` entirely (19 instructions, 13 differ) because the
end pointer can then be re-read from the frame instead of being cached in r30. `const*` on both
parameters in `fn_800067E0` is the same failure. So: one `const`, on the parameter whose target is
loaded once into a callee-saved register.

## Effect 2: declaration order sets the frame slot, assignment order sets the load order

Measured, and it is the reusable half of this run:

* **mwcceppc gives frame slots to address-taken locals in declaration order, from the top of the
  local area down.** Four such locals in a 32-byte frame: 1st -> `r1+0x14`, 2nd -> `r1+0x10`,
  3rd -> `r1+0x0C`, 4th -> `r1+0x08`; two in a 16-byte frame: 1st -> `r1+0x0C`, 2nd -> `r1+0x08`.
  Two locals in a 32-byte frame still get `r1+0x0C` / `r1+0x08` - the frame size does not move them.
* **The order of the *stores* follows the order of the *assignments*, not of the declarations.**

`fn_800067A8` needs both at once, which is why every spelling in the previous run's list failed: it
has to declare its locals in call-argument order (`f` first, so `f` lands in `r1+0x0C` as retail has
it) and *assign* them in the opposite order (`l` first, so the loads come out
`lwz r5,0(r4)` then `lwz r0,0(r3)` as retail emits them). Written either way round with
initialisers it is 4-7 instructions out; written this way it is 0 out:

```
extern "C" void fn_800067A8(STweakValue* const* first, STweakValue* const* last) {
  STweakValue* f;
  STweakValue* l;
  l = *last;
  f = *first;
  fn_800067E0(&f, &l);
}
```

## `fn_80006724` re-measured, still 78.21% (the one this run could not close)

The 33-vs-30 instruction gap is **not** "retail stores each iterator twice" as a free-standing
quirk: the slot rule above says retail's source has **four** address-taken locals, declared
`first`, dead-copy-of-`first`, `last`, dead-copy-of-`last`, and assigned in the order
`last, copy, copy, first` (retail's store order is `r1+0x0C`, `r1+0x08`, `r1+0x10`, `r1+0x14`).
The two dead copies are what the extra stores are; nothing ever reads them.

Fourteen shapes measured, all with this unit's exact `build.ninja` flags
(instructions out of 33, differing instructions out of 33):

* `first`/`last` from one `data` local, either declaration order - 30 insns, 25 dif.
* `last` declared and computed **before** `first`, and `self->mUnkC` read **twice** (two separate
  source expressions, which is what forces retail's second `lwz r0,12(r30)`) - 31 insns, 20 dif.
  This is the best honest spelling; it is not shipped because it is still not a match and the
  shipped two-liner is the same code.
* A second pair of locals assigned from the first pair (`f2`/`l2`), either order - 30-31 insns,
  21-22 dif. mwcceppc forwards the copies.
* The four locals as a struct or as a 2-element array, `&p.f`/`&p.l` or `&p[0]`/`&p[1]` - 30 insns,
  25 dif (a struct member's address is one slot, not two).
* `volatile STweakValue*` copies - **33 insns, 4 dif**, i.e. the only shape that produces the
  duplicate stores at all. Still 2 instructions out: retail stores the `first` pair
  `r1+0x10` then `r1+0x14`, and every assignment order that gets the slot order right
  (`cfirst` before `first`) also splits the two `lwz r0,12(r30)` reloads and costs a register
  (`add r0,r5,r0` where retail has `add r5,r5,r0`), which is 8 dif. Not shipped: it needs
  `volatile` dead copies to keep stores that nothing reads, and it does not reach 100% anyway.
* Four `const` copies (with `const_cast` at the call) - 31 insns, 25 dif; `const` copies
  initialised from the member - 25 dif. A `const` local's dead store is dropped like any other.
* Making the copies volatile *and* reading them back after the call, or declaring them before their
  originals - 38-39 insns, 34 dif.

WALL: fn_80006724 78.21% - retail's two extra stores need four address-taken locals (2nd and 4th declared are dead copies) and no non-volatile spelling keeps a dead store; the volatile one reaches 33/33 insns but is 2 instructions out of retail's store order

## Verified

```
tools/goal_check.sh build/goal/item.json   PARTIAL (exit 0): gate.sh ok, counts 10105 -> 10107,
                                           linked 4918 -> 4918, check_symbol_names ok,
                                           All: 31.09% fuzzy / 23.39% matched / 11.78% linked
                                           (10107 / 28465), flip FAIL,
                                           target rose 72 -> 74, no asm added
tools/report_diff.py build/goal/judge/report.base.json build/report.json
                                           +2 functions at 100%, no regression
sha1sum build/G2ME01/main.dol              6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py        504 units, 0 missing
python3 tools/check_decl_order.py          ok: 958 checked, 31 permuted, all accounted for
```

The flip still fails for the two pre-existing reasons in the section above
(`multiply-defined: 'CErrorOutputWindow::__vt'`, then the undefined cluster at `fn_80008C28` et al),
both of which predate this item; the unit is also still not a flip candidate
(`tools/unit_fit.sh`: 16 COMDAT weak copies in ours that retail's object does not define).

**Byte-identity, checked on the raw words** of `build/G2ME01/src/MetroidPrime/main.o` against
`build/G2ME01/main.elf`, masking only the LI field of `b`/`bl` (both objects carry those as
`R_PPC_REL24`); conditional-branch displacements are compared exactly, per the lesson in the
section above. `fn_800067A8` 14/14 words equal, `fn_800067E0` 20/20 words equal, no conditional
branch anywhere in either. The scratch harness is `.tmp/opencode/dt2/` in this worktree
(`cc.sh` compiles one source with this unit's exact `mwcc_sjis` flags, `cmp.py` does the word diff,
`try.py` runs a batch of spellings and prints instruction/differing counts) - all untracked, but
`cmp.py` is the cheap way to re-measure any of this.

## For the next run

The two `const`-qualification effects above are general and probably the cheapest remaining
lever in the whole DOL: any function in this tree that loads a parameter's target into a local
whose address is taken, and any function whose address-taken locals' frame slots are off, is a
candidate. `fn_80006724` is the one left in this block and its notes above are exhaustive enough
that it should be treated as closed unless someone finds a way to keep a dead store without
`volatile`.

The `NEW:` line at the end of the previous run's section is untouched and still unclaimed.

---

# Third run (2026-09-30): the item's block is closed; three functions outside it, +3

Verdict **PARTIAL** (exit 0), and the first run of this item whose diff does **not** touch
0x80006678-0x800068F4 at all. `main/MetroidPrime/main` **76 -> 79 / 99**; `All:` **10117 -> 10120**
matched, linked **4917 -> 4917**; `report_diff.py` against the judge's baseline: `+3 functions at
100%, no regression`. All three are **byte-identical** to dtk's retail object, measured on the raw
words with only the `b`/`bl` LI masked (`fn_80008B04` 11/11, each `ReleaseData` 20/20).

## Why this run left `fn_80006724` alone

It is the one function in the item's own block still under 100%, and both earlier runs measured it
exhaustively: fourteen shapes, the best of which is 33 of 33 instructions but two store *slots* out,
and the only spelling that produces its duplicate stores at all needs `volatile` dead copies. The
second run's conclusion - treat it as closed - stands and this run re-confirms it without spending
a spelling on it. **The block 0x80006678-0x800068F4 is now 7 of 8 byte-identical and closed.**

## What this run actually did: three 0.00% functions, all "named but unreachable"

The unit had **16** functions at 0.00%. Reading them off the report rather than guessing found a
class of cause none of the previous two runs had hit, and it is the cheapest kind there is: **the
retail body is known, the class is known, and the function is absent from our object only because
nothing in *this* translation unit reaches it.** Three landed:

| addr | size | what it is | why it was missing |
|---|---|---|---|
| 0x80008B04 | 44 | `TOneStatic<CGameGlobalObjects>::operator delete` | dtk's placeholder name |
| 0x80009058 | 80 | `rstl::rc_ptr<CMapWorldInfo>::ReleaseData()` | no instantiation here |
| 0x8000934C | 80 | `rstl::rc_ptr<CPlayerState>::ReleaseData()` | no instantiation here |

### 1. `template class` is the lever, and it is one line per function

`include/rstl/rc_ptr.hpp` defines `rc_ptr<T>::ReleaseData()` **out of line**, and this unit's flags
carry `-inline deferred,noauto` (`build.ninja`'s `mwcc_sjis` rule), so mwcceppc never inlines it -
but it only *emits* it when something instantiates `rc_ptr<T>` **in this translation unit**. The
`rc_ptr<CMapWorldInfo>` and `rc_ptr<CPlayerState>` holders are `CWorldState`'s and `CGameState`'s,
in other units, so the two definitions were simply not in `main.o`.

```
template class rstl::rc_ptr< CMapWorldInfo >;
template class rstl::rc_ptr< CPlayerState >;
```

**`template class`, not `template void ...::ReleaseData();`** - MWCC 2.7 rejects the member form
outright (`Error: illegal explicit template instantiation`, measured, `.tmp/opencode/g1/p1.cpp`).
The *class* form is accepted, emits **only** the two `ReleaseData` bodies, and **no extra symbols**
(`nm` on the probe object shows exactly two `T` entries and nothing else - no `~rc_ptr`, no
`operator=`, no ctor).

**Why retail's object has them here, which is the part worth remembering:** `dtk`'s
`auto_03_80003BE8_text` - the range whose `CGameState` destructor calls `ReleaseData` at 0x800044C8
- is a *split of the same original object* as this unit. Retail's compiler emitted the definition
and its caller together; the decomp split them across two units and the definition stayed behind.
**So a 0.00% named function in a unit whose *caller* is in an `auto_*` unit is a split artefact,
not a reverse-engineering problem.** That is the general form of this run's result, and it is worth
checking first on any 0.00% function: `grep 'bl <addr>' build/G2ME01/main.elf` and see whether the
only callers are outside the unit.

### 2. The `ReleaseData` body was already right; only its *emission* was missing

`include/rstl/rc_ptr.hpp`'s existing body is byte-exact for both, with no change:

```
addic. r0,r3,-1 ; stw r0,0(r4) ; bgt      ->  if (--*mRefCount <= 0) { ...
lwz r3,0(r31) ; li r4,1 ; bl __dt__13CMapWorldInfoFv   ->  delete GetPtr()
lwz r3,4(r31) ; bl CMemory::Free                       ->  delete mRefCount
```

`delete GetPtr()` gives `li r4,1 ; bl __dt__...` because `~CMapWorldInfo` / `~CPlayerState` are
MWCC deleting destructors whose names *are* retail's (`__dt__13CMapWorldInfoFv`,
`__dt__12CPlayerStateFv`, both already 100% here). **The 0.00% was never a codegen problem on these
two** - which is exactly why a percentage on a `NonMatching` unit is a signal and not a result, and
why the report's `None` (unpaired) had to be read as "not emitted" rather than "wrong code".

### 3. `fn_80008B04`: retail's placeholder name, and one access-specifier change

0x80008B04 is `TOneStatic<CGameGlobalObjects>::operator delete` and `config/G2ME01/symbols.txt:174`
has `fn_80008B04` for it with **no** `__dl__32TOneStatic<18CGameGlobalObjects>FPv`, so objdiff pairs
it on the placeholder and the body has to be emitted under that name - the same conclusion the first
run reached for `__dt__80006678`/`__dt__800066D0`, now with a second independent instance of it.
The body is `ReferenceCount()--` and nothing else (11 words, `r3` is the reference the call returns,
the incoming `ptr` is never read), **byte-identical to 0x80008A78** -
`__dl__38TOneStatic<24CGameArchitectureSupport>FPv`, matched at 100% by an earlier item - apart from
the one `bl`.

**The one non-obvious cost: `operator delete` is defined out of line, so calling it emits
`bl __dl__32TOneStatic<18CGameGlobalObjects>FPv` - a weak copy under the wrong name - instead of
retail's `bl ReferenceCount__32TOneStatic<18CGameGlobalObjects>Fv`.** The body's only name is
`ReferenceCount()`, which was `private`. It is now `public` in `include/Kyoto/TOneStatic.hpp`, with
the reason in a comment there; **access control does not affect code generation**, and the five
other `TOneStatic` members keep their linkage - verified by the fact that `__dl__38TOneStatic`,
`__nw__32TOneStatic`, `__dt__12CPlayerStateFv` and `__dt__13CMapWorldInfoFv` are all still
byte-identical after the change.

This is the sixth member of the group this unit already matches five of (0x80008AD4 `__nw__`,
0x80008B30 `GetAllocSpace`, 0x80008B3C `ReferenceCount` for `CGameGlobalObjects`; 0x80008A48/78/A4/B0
for `CGameArchitectureSupport`), and the 0x80006600 `bl` that reaches it is in
`__dt__CGameGlobalObjects_80006518` - this item's own block, one level up.

## A stale premise found, and left alone on purpose

`src/MetroidPrime/Player/CPlayerStateRefRelease.cpp` writes the same 0x8000934C 80 bytes as an
`extern "C"` `fn_8000934C`, on the stated premise that "retail instantiates `ReleaseData` per type
and leaves this one unnamed in the symbol table, so this is an `extern "C"` free function under
retail's own name". **That premise is no longer true**: `config/G2ME01/symbols.txt` now carries
`ReleaseData__Q24rstl22rc_ptr<12CPlayerState>Fv = .text:0x8000934C`, so the mangled name is retail's
and only the instantiation pairs. The file is deliberately left untouched: it is **not in
`files.cmake`**, nothing compiles it, its `-8 matched / +1 linked` measurement against a re-split
still stands, and editing it would be an unrelated change. Noted here so the next run does not
re-derive it.

## The remaining 13 at 0.00%, measured, with what blocks each

Read off `build/report.json` (all in `main/MetroidPrime/main`; none of these is a codegen guess):

* **Unpairable by name - the map has only a `fn_<addr>` placeholder**, so *no* spelling can score
  them and they need a `symbols.txt` rename or nothing: `fn_80008C28` (184), `fn_80008CE0` (136),
  `fn_80008D68` (128), `fn_80008DE8`/`reserve__...` (172), `fn_80008E94` (172), `fn_80009008` (80),
  `fn_80009224` (80), `fn_800095E4` (80). **Measured for three of them:** `fn_80009008`,
  `fn_80009224` and `fn_800095E4` are all the *same* 80-word `ReleaseData` body, differing only in
  which destructor the `li r4,1 ; bl` names (`fn_800B8CA0`, `__dt__16CWorldLayerStateFv`,
  `__dt__18CWorldTransManagerFv`) - i.e. `rc_ptr<T>::ReleaseData` for three more `T`, already
  correct in the header, unpairable here.
* **Blocked behind a function that does not exist**: `__dt__CGameGlobalObjects_80006518` (264),
  `single_ptr_assign_800064D0` (72) and `__dt__80006AE0` (88) - the trio the first run's `NEW:` line
  names. Their bodies are fully readable off `main.elf` and are the same D0 shape this item landed
  at 100%, **but all three call `fn_801F097C`** (0x801F097C, 0x54, the +0x150 member's destructor),
  which is itself undefined and **is in no unit's `.text` claim at all** (measured: no entry in
  `config/G2ME01/splits.txt` covers 0x801F097C or 0x801F09D0; they fall in the unclaimed gap). So
  landing the trio means landing `fn_801F097C` too, and that one calls `fn_801F09D0` (0x801F09D0,
  ≥0x58) - two more unclaimed functions to reverse-engineer first. `~CGameGlobalObjects`' own
  member list is otherwise complete and correct in `include/MetroidPrime/CGameGlobalObjects.hpp`
  (offsets +0x150, +0x14C, +0x148, +0x138, +0x134, +0x130, +0x108, +0xE4, +0x04 all check out
  against the 264 bytes).
* **`__dt__15CMemoryInStreamFv` (96)** - not read this run.

## For the next run

* **`fn_80006724` is closed.** Seven of the item's eight functions are byte-identical and the
  eighth has fourteen measured spellings; do not spend a run on it.
* **This item is done as far as the target unit goes** unless a run wants the trio above, and that
  is a *carve-first* job in the 0x801F097C-0x801F09D0 unclaimed gap, not a `main.cpp` job.
* **The general lever, wider than this item**: on any 0.00% function, ask *why it is absent from
  our object* before asking *what its body is*. Three answers exist and all three are cheap -
  `template class` for an uninstantiated template member, `extern "C"` under dtk's placeholder for
  an unpairable name, and the `auto_*`-split explanation for a definition whose callers are all
  outside the unit. `template class` is the one that had not been tried here at all.

## Verified

```
tools/goal_check.sh build/goal/item.json   PARTIAL (exit 0): gate.sh ok, counts 10117 -> 10120,
                                           linked 4917 -> 4917, check_symbol_names ok,
                                           All: 31.14% fuzzy / 23.43% matched / 11.78% linked
                                           (10120 / 28465), flip FAIL,
                                           target rose 76 -> 79, no asm added
tools/report_diff.py build/goal/judge/report.base.json build/report.json
                                           +3 functions at 100%, no regression
sha1sum build/G2ME01/main.dol              6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py        505 units, 0 missing
python3 tools/check_decl_order.py          ok: 958 checked, 31 permuted, all accounted for
```

`fn_80008B04` and both `ReleaseData` are byte-identical on the raw words of
`build/G2ME01/src/MetroidPrime/main.o` against `build/G2ME01/main.elf`, masking only the LI field of
`b`/`bl`; neither contains a conditional branch. Harness rebuilt in **this** worktree at
`.tmp/opencode/g1/` (untracked): `cc.sh` compiles one source with this unit's exact `build.ninja`
`mwcc_sjis` flags - **not** `tools/probe_cc.sh`, which omits three of them - and `cmp.py` does the
word diff against the linked ELF and prints the first differing words.

Diff: `src/MetroidPrime/main.cpp` (two `template class` lines + `fn_80008B04` + comments),
`include/Kyoto/TOneStatic.hpp` (one access specifier + comment), and the state-block counts in
`docs/HANDOFF.md`, which `gate.sh` rewrote under `MP_GATE_DOCS_WRITE=1` (10117 -> 10120 matched,
DOL units 8706 -> 8709); I did not touch that file by hand and the driver rewrites it anyway.

---

# Fourth run (2026-10-02): `CMain::CheckReset` 80.29% -> 100%, `main` 96 -> 97 / 99

Verdict **PARTIAL** (`goal_check.sh`: gate ok, matched 12482 -> 12483, linked 5863 -> 5863, target 96 -> 97,
no asm; flip still fails on `multiply-defined: 'CErrorOutputWindow::__vt'`). Diff: `src/MetroidPrime/main.cpp` only.
Measured with `.tmp`-free scripts: `decomp_build.sh MetroidPrime/main` (1 s) + objdump diff of `main.o` vs `main.elf`.

What fixed `CheckReset` (all four were needed, none is in earlier notes):
1. **Button loop as a `switch`**, not `expected = i==B||i==X||i==Start`: retail's `cmpwi 3 / bge / cmpwi 1 / bge / cmpwi 5`
   ladder is a `switch (i)` with `case kBU_B/kBU_X/kBU_Start: if (!pressed) chord=false; default: if (pressed) chord=false`.
   Bound is `i <= kBU_R` (`cmpwi 11 / bgt`), not `< kBU_MAX` (`cmpwi 12 / bge`).
2. `resetPressed` is `const int` (`mr r31,r3`, `cmpwi r31,0`), not `bool`.
3. **Tail shape**: `if (!busy && (req||card||exit)) { ...; return true; } mResetButtonHeld = resetPressed; return false;`
   - the old early-return form duplicated the tail (`b 22dc`); retail has one shared tail at 0x80007010.
4. **`WriteBits(!!x, 1)`** for both args (retail emits `neg/or/srwi 31` on the `lbz`'d bool); and the
   `if (written >= size) { rs_debugger_printf(...) } else { OSReport(...) }` order (retail's `blt` skips the first block).
5. **The two format strings were misspelled**: retail's are `"Reset failed: Tried %d"` (colon) and `"Wrote: %d\n"`.
   Comparing retail's pool at 0x803A56C0 with our `.rodata` is what found it; the pool offsets of every later literal moved.

**General lesson (STALE earlier claim, superseded):** the previous note that `FillInAssetIDs` *needs* `lbl_803A56C0 + 0x7C`
rather than a literal was only true while our pool lacked `"sound_lookup_ATBL"` ahead of CheckReset's strings. With the literal
`"sound_lookup_ATBL"` the pool is byte-identical to retail's through `"Wrote: %d\n"` and the function is 100%. When a
literal "names the wrong pool", diff the pool contents before blaming the pool.

Remaining in the unit: `AddPaksAndFactories` 83.53% (1936 B) and `RsMain` 72.81% (2148 B) - not touched this run.
Flip blockers unchanged (CErrorOutputWindow vtable copy; unit_fit COMDAT weak copies).
