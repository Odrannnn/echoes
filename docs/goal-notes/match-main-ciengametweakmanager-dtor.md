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
