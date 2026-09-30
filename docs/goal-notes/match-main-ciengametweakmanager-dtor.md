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
