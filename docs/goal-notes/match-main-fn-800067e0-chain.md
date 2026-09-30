# match-main-fn-800067e0-chain (lane L7, 2026-09-30)

Verdict **PARTIAL**: `main/MetroidPrime/main` **73 -> 74 / 99**, `tools/goal_check.sh
build/goal/item.json` passed every check it can pass, and the flip still fails on exactly the
pre-existing `CErrorOutputWindow::__vt` link error that attempts 2-5 of
`match-main-cmain-0x91-bitfield` recorded. Diff is **one file, 21 insertions, 6 deletions**, no
asm, no deletions of real work.

## The item was three-quarters stale, and the fourth was not the one the item said

Measured on a fresh `./tools/decomp_build.sh` of the clean tree first, as attempt 4 warned
(`build/report.json` in a lane worktree is a leftover, not a baseline; the driver's
`build/goal/judge/report.base.json` agreed at 73 / 99):

| retail | size | at HEAD | now |
|---|---|---|---|
| `0x800067E0` | 80 | 90.00% | **100.00%** |
| `0x80006830` | 32 | 100.00% | 100.00% |
| `0x80006850` | 36 | 100.00% | 100.00% |
| `0x80006874` | 128 | 100.00% | 100.00% |

The three thunks and the destructor are already at 100% in this tree, written by a later run as
`extern "C" void* fn_80006874(STweakValue*, short)` over a file-local `STweakValue` /
`STweakAudio` pair rather than as `CTweakValue::~CTweakValue()`. So the item's premise - "all
four measured at 100% in attempt 5 ... and lost when that run was discarded" - is true of attempt
5 and false of HEAD, and the only work left was `fn_800067E0`. All four are at 100% now.

## `fn_800067E0`: the `const` on `first`'s pointee, and nothing else

Retail (0x800067E0, 80 B) and the tree's previous spelling differ in **three words**, all in the
prologue, and the diff is a single load's position:

```
retail   stw r31,12(r1) | lwz r31,0(r3) | stw r30,8(r1) | mr r30,r4
at HEAD  stw r31,12(r1) | stw r30,8(r1) | mr r30,r4      | lwz r31,0(r3)
```

`src/MetroidPrime/main.cpp` changes one signature and one local:

```cpp
extern "C" void fn_800067E0(STweakValue* const* first, STweakValue** last) {
  STweakValue* it = *first;
  STweakValue* const* end = last;
```

**Why the const is real and not a fit-to-the-bytes trick.** mwcceppc only interleaves a *load*
with the two callee-save `stw`s; when one register's definition is a `mr` from an incoming
parameter it emits the `mr` first and the load after. `*first` is read exactly once, to seed the
iterator, and this function never writes through it; `last` is re-read on every comparison. So
`STweakValue* const*` is the honest signature - the first argument is a pointer to something this
function does not modify - and it is also the only spelling of the fifteen measured that puts the
load back in retail's slot. Verified by byte comparison against the DOL with `bl`/`b`/conditional-
branch targets masked (`.tmp/opencode/cmpfn.py`, untracked):

```
python3 .tmp/opencode/cmpfn.py build/G2ME01/src/MetroidPrime/main.o 0x800067E0 0x50 fn_800067E0
   ours 20 words, retail 20 words
   IDENTICAL (branch targets masked)
```

The same script on HEAD's object reports the three differing words, so the check can fail.

## Correction to the item's premise, and to attempt 5's note: the reference spelling cannot compile

The item reason and attempt 5's note both say the winning signature is
`(CTweakValue* const&, CTweakValue*&)`. **That is not reachable with mwcceppc 2.7.** Its front end
parses a *reference to a pointer* as the plain pointer: given `T* const& first`, `*first` has type
`T` and `&first` is `T* const*`, so both of the natural spellings are hard errors -

```
T* it = *first;          # Error: illegal implicit conversion from 'T' to 'T *'
h(&first);               # Error: function call 'h(T *const *)' does not match 'h(T *)'
```

The same holds for `T*&`. Attempt 5's claim that this spelling was measured "first try" to
byte-identical cannot have been produced by this compiler. The reference-to-pointer reading is
still a correct *description* of retail's call shape (begin read once, end re-read), but it is not
a source spelling, and it is what sent this run's predecessor down a dead end. `CIOWin* const&`
appears in `src/MetroidPrime/Decode.cpp` / `PortMakeMsg.cpp`; those parameters are only ever used
as values, never dereferenced or address-taken, so they do not exercise the bug.

## Measured: 22 spellings, 15 compiled

`.tmp/opencode/b1 b2 b3` (untracked), each compiled with `main.cpp`'s exact `build.ninja` flags
(`.tmp/opencode/probe_main.sh`; note `tools/probe_cc.sh` omits `inline_max_size(125)` and the
musyx `-D`s). Rejected by the front end, 7: `T* const&`, `T* const& / T*&`, `T*&`, by-value `T*`,
`const T** first` (twice), and one `const`-local variant. Compiled, 15 - the four that compile
and are the same code are listed here so the next run does not repeat them:

| spelling | prologue order |
|---|---|
| `(T** first, T** last)`, locals in either order, `for`/`while`/`do-while` (8 variants) | `save31 save30 mr30 lwz31` - 90% |
| `(T** first, T** last)` walking `T**` (one variant) | `save31 mr31 save30 mr30` - 76 B, wrong code |
| `(T** const first, T** last)` | `save31 save30 mr30 lwz31` |
| **`(T* const* first, T** last)`** | **`save31 lwz31 save30 mr30` - retail, 100%** |
| `(T** first, T* const* last)` / `(T* const* first, T* const* last)` / `(T* const* first, T* const* last)` with either local order (4 variants) | `save31 lwz31 save30 lwz30` - retail's shape with r31/r30's sources swapped, 3 words off |

The four that load *both* registers are the only ones that interleave a load with the saves at
all, which is the whole of the mechanism. Nothing in the loop body, the stride, the comparison or
the epilogue changes anything: every variant that compiles has retail's 80 bytes and the same loop.

## Not touched, deliberately

`fn_800067A8` (69.79%) and `fn_80006724` (78.21%) sit next to this chain and are **separate queued
items** (`match-main-fn-800067a8`, `match-main-ciengametweakmanager-dtor`). Both keep the exact
same score after this change - verified in `build/report.json` - so the signature change did not
disturb them, and this diff is left at one function.

The `STweakValue` / `STweakAudio` file-local structs are also left as they are. They are what
makes `fn_80006874` 100% today, and rewriting them into `CTweakValue` with a header-declared
destructor is a separate change with its own risk to the three functions already at 100%.

## Still not stopping the flip (unchanged from attempts 2-5)

* `flip_test.sh` fails at link with
  `mwldeppc.exe Linker Error: multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o`
  - a `config/G2ME01/splits.txt` question (that `.data` is not claimed for this unit), the first
  error and not the whole story.
* 25 of the 99 functions are not matched - 17 with no body at all, 8 with a body below 100%
  (`StreamNewGameState` 18.68%, `AsyncIdle` 99.17%, `RsMain` 2.38%, `fn_80006724` 78.21%,
  `fn_800067A8` 69.79%, `CheckReset` 0.34%, `AddPaksAndFactories` 0.21%,
  `InitializeSubsystems` 12.44%). `.text` is **6440 bytes short** of the 17608 claimed
  (was 6520 before this change, i.e. exactly `fn_800067E0`'s 80). `.sbss` is 61 against 36
  claimed, 16 unclaimed COMDAT functions.
* `check_decl_order.py --unit MetroidPrime/main` still reports **would break on a flip** - our
  `.text` is descending where retail's is ascending. Pre-existing, and it wants the whole file
  reversed, which is its own item.

## Verified (this run, all measured)

```
tools/decomp_build.sh                 All: 31.09% fuzzy, 23.39% matched, 11.78% linked
                                       (10106 -> 10107 / 28465 functions)
unit: main/MetroidPrime/main          73 -> 74 / 99, fuzzy 55.19% -> 55.23%,
                                       matched_code 9016 -> 9096 (= 80, this function exactly)
fn_800067E0                           90.00% -> 100.00% at 80 B
fn_800067A8 / fn_80006724             69.78571% / 78.21212%, unchanged
tools/report_diff.py judge base       matched 10106 -> 10107, linked 4918 -> 4918,
                                       "+1 functions at 100%", "no regression"
tools/goal_check.sh build/goal/item.json
                                       PARTIAL - gate ok, counts ok, names ok, target rose, no asm
sha1sum build/G2ME01/main.dol         6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py   504 units, 0 missing
python3 tools/check_raw_offsets.py    ok: 152 raw-offset site(s) in 61 file(s)
tools/unit_fit.sh MetroidPrime/main.cpp
                                       .text SHORT by 6440; .sbss 61 vs 36; 16 extras
git status                            src/MetroidPrime/main.cpp only
```

`docs/HANDOFF.md` was reverted after the judge - `gate.sh` rewrites it under
`MP_GATE_DOCS_WRITE=1` and the driver owns that file.

## No `NEW:` line

Nothing new is blocked. The remaining functions of this unit are already queued
(`match-main-ciengametweakmanager-dtor`, `match-main-fn-800067a8`, `match-main-getaveragevalue-f`,
`match-main-cgameglobalobjects-dtor`, the `fn_80009xxx ReleaseData` copies, `fn_800068F4`'s 12-byte
element), and the one measurement here that a future run could want - that mwcceppc 2.7 drops the
`&` on a reference-to-pointer - is a codegen rule, which belongs in this file and in the header
comments, not in the queue.

---

# match-main-fn-800067e0-chain, attempt 2 (lane L7, 2026-09-30, later run)

Verdict **PARTIAL**: `main/MetroidPrime/main` **76 -> 79 / 99**, tree-wide matched
**10110 -> 10113**, `tools/goal_check.sh build/goal/item.json` clean on everything it can check,
flip still fails on the same pre-existing `CErrorOutputWindow::__vt` link error. Diff is **one
file, 64 insertions, 0 deletions**, no asm.

## The premise of attempt 1 is now fully stale: the 0x800067E0 chain is 4/4 and stayed 4/4

Re-measured first, on a fresh `./tools/decomp_build.sh` of the clean tree (the driver's
`build/goal/judge/report.base.json` at `7a417cf` agrees at **76 / 99**, not the 73 of attempt 1 -
two later runs landed `fn_800067A8` and the `__dt__80006678`/`__dt__800066D0` pair since):

| retail | at attempt-1 HEAD | at this run's HEAD | now |
|---|---|---|---|
| `fn_800067E0` 0x50 | 100.00% | 100.00% | 100.00% |
| `fn_80006830` 0x20 | 100.00% | 100.00% | 100.00% |
| `fn_80006850` 0x24 | 100.00% | 100.00% | 100.00% |
| `fn_80006874` 0x80 | 100.00% | 100.00% | 100.00% |

**Nothing in this item's named four functions was left to do**, and the `STweakValue` /
`STweakAudio` rewrite attempt 1 declined is still declined - it is what makes `fn_80006874` 100%
and rewriting it is its own item with its own risk. So the unit's 23 remaining functions are the
only work, and the note below is what this run did about them.

## What landed: `fn_80009008`, `fn_80009224`, `fn_800095E4` - three `rc_ptr<T>::ReleaseData`

All three are `rstl::rc_ptr<T>::ReleaseData` and all three are now **100.00% at 0x50 = 80 bytes**:

| retail | retail's own symbol | the destructor its `delete` reaches |
|---|---|---|
| `0x80009008` | `fn_80009008` (unnamed in the map) | `fn_800B8CA0` (unnamed, other unit) |
| `0x80009224` | `fn_80009224` (unnamed) | `__dt__16CWorldLayerStateFv` (**in this unit**, 100%) |
| `0x800095E4` | `fn_800095E4` (unnamed) | `__dt__18CWorldTransManagerFv` (other unit) |

`include/rstl/rc_ptr.hpp` already defines the template and this unit already emits **five** copies
of it as weak COMDATs (`ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv` and friends, all 100%). **None
of these three was one of those five, and the reason is the one this file's other `extern "C"`
blocks give:** `config/G2ME01/symbols.txt:185` and `:191` are `fn_80009008` / `fn_80009224`,
retail's placeholders, and objdiff pairs functions **by name** - the weak
`ReleaseData__Q24rstl23rc_ptr<16CWorldLayerState>Fv` the template would emit for `fn_80009224`
scores against nothing. Retail's own names are the only ones that pair. (`fn_800095E4` is not in
`symbols.txt` at all and dtk names it by address.)

### 0x50 vs 0x64 is the virtual destructor, and nothing else

The body is `if (--*mRefCount <= 0) { delete GetPtr(); delete mRefCount; }` verbatim from the
header, and the three differ **only** in the `bl`. 80 rather than the 100 bytes of
`ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv` is not a different body: `CIOWin`'s destructor is
**virtual**, so MWCC's `delete` goes through the vtable
(`lwz r3,0(r31) / cmplwi r3,0 / beq / lwz r12,0(r3) / li r4,1 / lwz r12,8(r12) / mtctr r12 /
bctrl` - four extra instructions plus a null guard), while all three of these classes declare a
plain **non-virtual** destructor, for which `delete p` is the direct `li r4,1 / bl ~D0` that the
class's own D1 form ends in. Measured, not assumed: 20 of 20 instructions in each.

`SPairRcPtr` is `rc_ptr<T>`'s two words (`{ const T* mPtr; int* mRefCount; }` at +0/+4) read
through a same-layout view, because both members are private and retail's refcount is a separate
four-byte `CMemory` allocation. That is the same reason `rstl::CRcPtrData` exists; the view is
eight bytes and changes no layout.

### The check that can fail

```
tools/bytescmp.py build/G2ME01/src/MetroidPrime/main.o <fn> <retail> 0x50
   fn_80009008  2 differing instructions of 20   (the two bl displacement fields)
   fn_80009224  2 differing instructions of 20   (ditto)
   fn_800095E4  2 differing instructions of 20   (ditto)
objdump -r: 0000037c fn_800B8CA0 / 00000384 Free__7CMemoryFPCv
           000003cc __dt__16CWorldLayerStateFv / 000003d4 Free__7CMemoryFPCv
           0000032c __dt__18CWorldTransManagerFv / 00000334 Free__7CMemoryFPCv
```

All six relocations land on retail's own symbol names, and the two `bl` fields are the only
differing bytes, so the check fails if either is wrong.

## Still not stopping the flip (all three pre-existing, none made worse)

* `flip_test.sh` fails at link with
  `mwldeppc.exe Linker Error: multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o`
  - a `config/G2ME01/splits.txt` question, unchanged by this diff.
* 20 of the 99 functions are still not matched: **13 with no body in our object at all** and
  **7 with a body below 100%** (`InitializeSubsystems` 12.44%, `AddPaksAndFactories` 0.21%,
  `CheckReset` 0.34%, `fn_80006724` 78.21%, `RsMain` 2.38%, `AsyncIdle` 99.17%,
  `StreamNewGameState` 18.68%). `.text` is now
  **6200 bytes short** of the 17608 claimed (was 6440 - exactly these three 80-byte functions).
  `.sbss` 61 against 36 claimed. `unit_fit.sh` reports 15 unclaimed COMDAT functions, 1300 bytes
  (was 16).
* `check_decl_order.py --unit MetroidPrime/main` still reports **would break on a flip** - our
  `.text` is descending where retail's is ascending, which wants the whole file reversed and is
  its own item.

## What is left, and what stops each (measured, not guessed)

* **`ReleaseData__Q24rstl22rc_ptr<12CPlayerState>Fv` (0x8000934C) and
  `ReleaseData__Q24rstl23rc_ptr<13CMapWorldInfo>Fv` (0x80009058)**, 80 bytes each, and these two
  are the *only* remaining functions in the unit that need no new body at all - their names are
  already exactly right, so only the instantiation is missing. **Their only callers are outside
  this unit**: `objdump -d build/G2ME01/main.elf | grep 8000934c` gives 0x800044B0-0x80004554 (a
  different unit) plus 0x8001ED64 / 0x80022070 / 0x8003ECA8 / 0x80042A98 / 0x800454EC..., and
  0x80009058 is reached from 0x800044C8 / 0x800380DC / 0x80042AC8 / 0x80087710 / 0x80087EC8.
  Nothing in this unit's range calls either, so mwcceppc never instantiates the template. The
  only ways to force it are a namespace-scope object with such a member (the
  `SForceTailWeakCopies` trick `src/MetroidPrime/mainTail.cpp:300` uses, and which that file
  documents as "a measurement aid and nothing else") or a `delete` inside a function of this
  unit. **Neither is retail work**, and this is the unit that eventually has to flip, so this run
  did not do it: it would put 16 bytes of unclaimed `.sbss` and a ctor/dtor pair into the object
  that has to match byte for byte. Recorded so the next run does not go looking.
* **`fn_80008B04` (0x80008B04, 44 B) is `TOneStatic<CGameGlobalObjects>::operator delete`** and
  the body is *already written* - `include/Kyoto/TOneStatic.hpp`'s `operator delete` is
  `ReferenceCount()--`, and the identical 44 bytes are already emitted and at 100% as
  `__dl__38TOneStatic<24CGameArchitectureSupport>FPv`. The only reason it is missing is that
  **nothing in this unit references it**: `objdump -d` shows retail's only caller is
  `bl 80008b04` at 0x80006600, inside `~CGameGlobalObjects` (0x80006518), which is itself
  unwritten. So it is blocked behind the next bullet, not behind a spelling.
* **`__dt__CGameGlobalObjects_80006518` (0x80006518, 264 B)** gates three functions -
  itself, `single_ptr_assign_800064D0` (72 B) and `__dt__80006AE0` (88 B) - which are all
  `single_ptr<CGameGlobalObjects>`'s D1/dtor/`operator=`. Reading retail's tail
  (`0x800065B0`-`0x80006604`) gives the member order for free: `+0x14C` inGameTweakManager,
  `+0x134` memoryCard, `+0x130` gameState, `+0x108` characterFactoryBuilder, `+0xE4` simplePool,
  `+0x04` resFactory, then **`bl __dt__14CMemoryCardSysFv` with r3 = this**, i.e. **`CMemorySys`
  is a base of `CGameGlobalObjects` at +0x00**, and the header's `CGameGlobalObjectsCardInit
  pad0` at +0x00 is not it. That base is a header change affecting every `CGameGlobalObjects`
  member offset the constructor and `PostInitialize` depend on, which is why this is
  `match-main-cgameglobalobjs-14c`'s work and not a rider here.
* **`fn_80008C28` / `fn_80008CE0` / `fn_80008D68` / `fn_80008E94`** (184 + 136 + 128 + 172 B) are
  one structure, not four functions: `fn_80008CE0` is `allocate(44)` + four pointer stores + a
  `rstl::string` copy-construct at +16, `fn_80008C28` recurses on `0(r29)` and `4(r29)` and then
  calls it, and `fn_80008D68` tears the same 44-byte node down. That is a union-find/merge shape
  and **the class is not named anywhere in the map**, so it is a fresh reverse-engineering job,
  not a spelling job. `fn_80008E94` is `rstl::vector<rstl::pair<Ui,Ui>, rmemory_allocator>
  ::reserve` (172 B; `slwi r3,r30,3`, `allocate`, `cmpw` against `8(r3)`), and its named twin
  `reserve__Q24rstl55vector<Q24rstl11pair<Ui,Ui>,Q24rstl17rmemory_allocator>Fi` has the same
  instantiation problem as the two `ReleaseData`s above.
* **`__dt__15CMemoryInStreamFv` (0x800055CC, 96 B)** is retail's deleting destructor of a class
  whose only base is `CInputStream` and is **byte-for-byte the shape
  `CErrorOutputWindow::~CErrorOutputWindow() {}` already produces at 100% in this file**
  (`stw __vt / li r4,0 / bl ~CInputStream / extsh. / ble / mr r3,r30 / bl CMemory::Free`).
  The obstacle is not the body: `include/Kyoto/Streams/CMemoryInStream.hpp:15` already declares
  `virtual ~CMemoryInStream() override {}` **in class**, so the only way to get the symbol is to
  take the definition out of line, which edits a header the port also builds and can only be
  shown safe by `link_check.sh`'s undefined count staying at 250. Not attempted here.

## Verified (this run, all measured)

```
tools/decomp_build.sh                 All: 31.10% fuzzy, 23.39% matched, 11.78% linked
                                       (10110 -> 10113 / 28465 functions)
unit: main/MetroidPrime/main          76 -> 79 / 99, fuzzy 55.56% -> 56.92%,
                                       matched_code 9192 -> 9432 (= 240 = 3 x 80, these three)
fn_80009008 / fn_80009224 / fn_800095E4   0.00% -> 100.00% at 80 B each
tools/report_diff.py judge base       matched 10110 -> 10113, linked 4918 -> 4918,
                                       "+3 functions at 100%", "no regression"
tools/goal_check.sh build/goal/item.json
                                       PARTIAL - gate ok, counts ok, names ok, target rose, no asm
sha1sum build/G2ME01/main.dol         6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py   504 units, 0 missing
tools/unit_fit.sh MetroidPrime/main.cpp
                                       .text SHORT by 6200 (was 6440); .sbss 61 vs 36; 15 extras
link_check (gate)                     250 undefined, 0 duplicates - unchanged from baseline
git status                            src/MetroidPrime/main.cpp only
```

`docs/HANDOFF.md` was reverted after the judge - `gate.sh` rewrites it under
`MP_GATE_DOCS_WRITE=1` and the driver owns that file.

## No `NEW:` line

Everything still unmatched in this unit is named in "What is left" above with the measurement
that blocks it, and four of the six entries are already queued as their own items
(`match-main-cgameglobalobjs-14c`, `match-main-cgameglobalobjects-dtors`, `match-main-rsmain-body`,
`match-main-fn-80009274`). The one general fact worth carrying forward is a codegen rule, so it
belongs here and not in the queue:

> **MWCC's `delete p` is `li r4,1 / bl ~D0` for a class with a non-virtual destructor and
> `lwz r12,0(r3) / lwz r12,8(r12) / mtctr r12 / bctrl` (plus a null guard) for a virtual one.**
> That is the entire 80-vs-100-byte difference between retail's two `rc_ptr::ReleaseData` shapes,
> and it is worth checking before concluding that a `ReleaseData` body is wrong.

---

# attempt 3 (lane L1, 2026-09-30)

Verdict **PARTIAL**: `main/MetroidPrime/main` **85 -> 86 / 99**, tree-wide matched
**10127 -> 10128**, `tools/goal_check.sh build/goal/item.json` clean on everything it can check
(`ok target rose: main/MetroidPrime/main: 85 -> 86 / 99 functions`, exit 0). The flip still fails
on the same pre-existing `multiply-defined: 'CErrorOutputWindow::__vt' in CErrorOutputWindow.o`.
Diff is **one file, 33 insertions, 0 deletions**, no asm, no deletions of real work.

Measured first, on a fresh `./tools/decomp_build.sh` of the clean tree: the unit was **85 / 99**,
**not** the 79 attempt 2 finished on and **not** the 76 its HEAD was - two later runs landed
`fn_800067A8`, the `__dt__80006678`/`__dt__800066D0` pair, `__dt__CGameGlobalObjects_80006518`,
`single_ptr_assign_800064D0`, `__dt__80006AE0` and `fn_80008B04`. The driver's
`build/goal/judge/report.base.json` agrees at 10127. **The item's own four functions
(0x800067E0/0x80006830/0x80006850/0x80006874) were already 4/4 and still are**; the chain is
closed and this run did not touch it.

## What landed: `reserve__Q24rstl55vector<Q24rstl11pair<Ui,Ui>,Q4rstl17rmemory_allocator>Fi`

`0x80008DE8`, 0xAC = 172 bytes, **0.00% -> 100.00%**. Nothing was written by hand: **the header's
own body is retail's**, and the only thing missing was the instantiation.

`include/rstl/vector.hpp:158`'s `vector<T,A>::reserve`, compiled for
`T = rstl::pair<uint,uint>` (retail's `Ui`) with this unit's exact `build.ninja` flags, is
**43 of retail's 43 instructions, mnemonic and operand identical** - the `cmpw r30,r0 / ble`
capacity test, `slwi r3,r30,3`, the 8-byte copy loop `lwz/stw/lwz/addi/stw/addi`, `cmplw r5,r3 /
bne`, the `CMemory::Free` of the old buffer and the two member stores. Verified by disassembly
side-by-side, so the check can fail:

```
./.tmp/opencode/probe.sh .tmp/opencode/bres.cpp .tmp/opencode/bres.o   # main.cpp's exact flags
python3 .tmp/opencode/cmpv.py .tmp/opencode/bres.o 0x80008E94 0xAC \
   'reserve__Q24rstl55vector<Q24rstl11pair<Ui,Ui>,Q24rstl17rmemory_allocator>Fi' -v
   43 instrs (retail 43), 0 differing
```

Nothing in this unit's 0x800053B8-0x80009880 range calls either 0x80008DE8 or 0x80008E94
(retail's callers of the mangled one are 0x80003E9C, 0x800562F4, 0x80160D24, 0x80176E3C; of the
placeholder 0x80003FB0, 0x80142244, 0x801EFA88 - all other units), so mwcceppc never emitted the
COMDAT. `reserve` is 172 bytes against this unit's `-pragma "inline_max_size(125)"`, so **a plain
call is not inlined and the weak COMDAT is emitted** - which is the whole of the change:

```cpp
extern "C" void fn_80008E94(TUiPairVec* self, int n) { self->reserve(n); }
```

Two spellings that do **not** compile with mwcceppc 2.7, both measured:
`extern "C" void f(rstl::vector<rstl::pair<uint,uint>, rmemory_allocator>*, int)` and
`typedef rstl::vector<TPair, rmemory_allocator> TVec;` are both *declaration syntax error* /
*illegal function definition*. The one-argument form (`typedef rstl::vector<TPair> TVec;`, the
default allocator) compiles and mangles to the retail name. Also: `Ui` does not exist in this
tree - `rstl/pair.hpp` spells it `uint`, and `is_trivially_destructible<pair<uint,uint>>` is
already specialised there.

`fn_80008E94` itself is left as the 32-byte thunk and now scores **18.40%** where it scored
**0.00%** (no body) - a rise in percentage, not a match, and no regression. Retail's
`fn_80008E94` is 172 bytes, i.e. a **second copy** of the body rather than a call, and this
compiler emits a `bl`. Nothing recoverable from the DOL says what source shape made MWCC emit a
copy instead of a call, so it was not faked.

## The mechanism behind the "four dead stores" three previous runs measured

`docs/goal-notes/match-main-ciengametweakmanager-dtor.md:212-243` closed `fn_80006724` on the
claim that retail's extra two stores need **four address-taken locals** whose 2nd and 4th are dead
copies, and that no non-`volatile` spelling keeps a dead store. **That conclusion is wrong, and
`fn_80008E94` is the counter-example**: the same four stores appear there with **no address taken
anywhere in the function** (the only two calls are `rmemory_allocator::allocate` and
`CMemory::Free`, both by value), and they are not dead.

They are `uninitialized_copy(begin(), end(), newData)`'s **two eight-byte `pointer_iterator`
temporaries**, at `r1+0x08..0x0F` and `r1+0x10..0x17`, each of which mwcceppc stores **twice with
the same value** in its two words. Retail's store order, `stw r5,16 / stw r3,8 / stw r3,12 /
stw r5,20`, is exactly that: `begin()`'s temp first word, `end()`'s temp both words, `begin()`'s
temp second word. The same four slots with the same doubled values are in `fn_80006724` and
`fn_800068F4` (`r1+0x08` = `last`, `r1+0x0C` = `last`, `r1+0x10` = `first`, `r1+0x14` = `first`),
and in both of those the call is handed **`&tmp1.word1` and `&tmp2.word0`** - `addi r3,r1,20` and
`addi r4,r1,12` - which is `&beginIt.mCurrent` and `&endIt.mCurrent` if the iterator is
`{owner, current}`. That is also why retail re-reads `self->mUnkC` (`lwz r5,12(r30)` then
`lwz r0,12(r30)`): **the stores into an aggregate local are an aliasing barrier**, so the second
read cannot be CSE'd - which is exactly the effect every spelling in the previous three runs was
missing, because all of them used scalar locals and mwcceppc *does* disambiguate those from
`*self`.

Measured, this run, all with `main.cpp`'s exact flags (`.tmp/opencode/probe.sh`, untracked):

| shape | stores kept | slots |
|---|---|---|
| 4 scalar address-taken locals, all live | 4 | 1st -> `r1+0x14`, 2nd -> `0x10`, 3rd -> `0x0C`, 4th -> `0x08` |
| 2 scalar address-taken locals (the shipped spelling) | 2 | `r1+0x0C`, `r1+0x08` |
| 4 scalars where 2 are dead (address never taken) | **2** - mwcceppc drops them | - |
| 2 `volatile` dead copies | **0** - a `volatile` local whose address is never taken is deleted | - |
| 2 x 2-element `T*` array locals, one element passed | 4, block-ordered (0x10,0x14 then 0x08,0x0C) | - |
| **two 8-byte class temporaries from `begin()` / `end()`** | **4, interleaved - retail's** | see above |

So: **the four stores are not address-taken locals at all, they are two 8-byte aggregate
temporaries, and no spelling built out of scalar locals can produce them.** The way to reach
`fn_80006724` is to give it a container with `begin()`/`end()` returning an 8-byte
`rstl::pointer_iterator`-shaped class and call `fn_800067A8(&it.mCurrent, ...)`. That is *not*
attempted here: `CInGameTweakManager::mUnkC` is a `uint` in `include/MetroidPrime/
CInGameTweakManager.hpp:52`, not a pointer, so the spelling needs an invented container class on
top of a reinterpret_cast, and a wrong guess there is a reviewer rejection, not a match.

## `__dt__15CMemoryInStreamFv` is closed, with the mechanism measured

Attempt 2 called this "only the definition has to go out of line". Measured this run: the
out-of-line definition **is** emitted with nothing in the TU odr-using the class, and the 24
instructions are retail's - but it drags the **vtable** out with it.

```
class CBase { public: virtual ~CBase(); int mX; };
class CDer : public CBase { public: virtual ~CDer(); int mY; };
CDer::~CDer() {}
->  __dt__4CDerFv  T (strong), 0x5C = 92 bytes, retail's exact shape
    __vt__4CDer    D (strong, .data)          <-- the blocker
```

Two further measurements close it:
* an in-class body plus an out-of-line definition is a hard error, `'CDer::~CDer()' redefined`,
  so the header **must** lose its `{}` - a header the port also builds;
* `__vt__15CMemoryInStream` is already `V` (weak) in `CGameArea.o`, `CMemoryCardDriver.o` and
  `CWorld.o`, and `__dt__15CMemoryInStreamFv` is already `W` (weak) in all three, so a **strong**
  out-of-line definition in `main.o` wins the ELF weak/strong race cleanly - but it would place a
  strong `__vt__15CMemoryInStream` in a unit that claims **no `.data` at all**, at whatever
  address the linker picks, which moves `main.dol` and fails `sha1sum`. Dead end, and the reason
  is the vtable, not the body.
* Also: `virtual ~X() override {}` does not parse in mwcceppc 2.7 (`';' expected`), so
  `include/Kyoto/Streams/CMemoryInStream.hpp:15` cannot even be included from this unit as it
  stands. Its 14 includers all get past it, so something in their include order changes the
  parse; worth knowing before anyone tries.

## `fn_80008C28` / `fn_80008CE0` / `fn_80008D68` - read out, not attempted

Left alone this run, and the reading is here so it is not repeated. One structure, 448 bytes,
three functions; the class is **not named anywhere in the map**, so this is reverse engineering,
not a spelling job. Measured from retail:

* **node = 44 bytes**: `+0x00` child A, `+0x04` child B, `+0x08` a link the parent overwrites
  with the new node (the creator is called with `li r6,0` for it), `+0x0C` an `int` the creator
  copies from the source node, `+0x10` a **28-byte** member = `rstl::basic_string` (0x10 bytes,
  `CHECK_SIZEOF(string, 0x10)`) followed by **12 more bytes**, which the copy moves as three
  `lwz`/`stw` pairs *after* calling the string's copy constructor. So the member is
  `{ rstl::string; char[12]; }` and the creator's `bl __ct__basic_string` is the first member's
  constructor, not a manual byte copy.
* `fn_80008CE0` (136 B) is a member of something whose **base is `rmemory_allocator`**: `r3` is
  both the `li r3,44` size for `allocate` and the incoming `this`, and there are five more
  parameters in `r4`-`r8`. It is `node->(A,B,link,0,int,name)` and returns the node, with
  `mr. r30,r3 / beq` (MWCC's `new`-returns-null check) around the body.
* `fn_80008C28` (184 B) is the merge: `if (!n) return 0;` then recurse on `+0` and on `+4`,
  then call the creator with `(leftResult, rightResult, 0, n->+0x0C, n+0x10)`, then
  `if (leftResult) leftResult->+8 = r3;` and the same for the right.
* `fn_80008D68` (128 B) is the teardown: recurse on `+0` and `+4`, then **a `cmplwi r31,0` guard
  around the member destructor only** (not around the `Free`), whose body is
  `addic. r0,r31,16 / beq` **twice** then `addi r3,r31,16 / bl internal_dereference<string>` -
  two null checks for the two destructor levels - then `CMemory::Free(n)`.

The `if (n)` guard around the destructor and the doubled `addic.` are the two things that would
have to be reproduced exactly, and neither is reachable from a spelling experiment. Rejected as
out of scope for this item's budget, not as impossible.

## Still not stopping the flip (unchanged, none made worse)

* `flip_test.sh` fails at link with `multiply-defined: 'CErrorOutputWindow::__vt' in
  CErrorOutputWindow.o` - the `splits.txt` question attempts 2-5 recorded, untouched by this diff.
* 13 of the 99 functions are still not matched: `fn_80008C28`, `fn_80008CE0`, `fn_80008D68`,
  `fn_80008E94` (now the thunk), `reserve__...` is **no longer** among them, plus
  `__dt__15CMemoryInStreamFv`, `fn_800068F4`, `fn_80006724` 78.21%, `RsMain` 2.38%,
  `AddPaksAndFactories` 0.21%, `CheckReset` 0.34%, `InitializeSubsystems` 12.44%,
  `StreamNewGameState` 18.68%, `AsyncIdle` 99.17%.
* `unit_fit.sh`: `.text` gap, `.sbss` 61 vs 36, and **15** unclaimed COMDAT functions /
  1300 bytes - all three the same as attempt 2, so this diff added no unclaimed function.
* `check_decl_order.py --unit MetroidPrime/main` still reports **would break on a flip**
  (pre-existing; the whole file wants reversing). `fn_80008E94` was inserted **above**
  `fn_80008B04`, i.e. at the correct descending-by-retail-offset place, so it does not add to it.

## Verified (this run, all measured)

```
tools/decomp_build.sh                 All: 31.17% fuzzy, 23.45% matched, 11.78% linked
                                       (10128 -> 10128 / 28465 functions)
unit: main/MetroidPrime/main          85 -> 86 / 99, fuzzy 60.49% -> 61.64%,
                                       matched_code 10060 -> 10232 (= 172, this function exactly)
reserve__Q24rstl55vector<Q24rstl11pair<Ui,Ui>,Q24rstl17rmemory_allocator>Fi
                                       0.00% -> 100.00% at 172 B
fn_80008E94                          0.00% -> 18.40% (32 B thunk vs retail's 172)
fn_80006724 / fn_800067A8 / fn_800068F4   78.21% / 100% / 0.00%, unchanged
tools/report_diff.py judge base      matched 10127 -> 10128, linked 4917 -> 4917,
                                       "+1 functions at 100%", "no regression"
tools/goal_check.sh build/goal/item.json
                                       PARTIAL (exit 0) - gate ok, counts ok, names ok,
                                       target rose 85 -> 86, no asm
sha1sum build/G2ME01/main.dol        6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
python3 tools/check_symbol_names.py  505 units, 0 missing
python3 tools/check_raw_offsets.py   ok: 152 raw-offset site(s) in 61 file(s)
tools/unit_fit.sh MetroidPrime/main.cpp
                                       15 extras / 1300 bytes (unchanged)
git status                           src/MetroidPrime/main.cpp only
```

`docs/HANDOFF.md` was reverted after the judge - `gate.sh` rewrites it under
`MP_GATE_DOCS_WRITE=1` and the driver owns that file.

## No `NEW:` line

Nothing new is blocked in a way a lane could pick up: `fn_80008C28`/`CE0`/`D68` need the class
reverse-engineered before any spelling question exists, and the 4-store mechanism above turns
`fn_80006724` and `fn_800068F4` into a *source-shape* problem in this unit rather than a new
target. The one general fact worth carrying forward is a codegen rule, so it belongs here and
not in the queue:

> **mwcceppc 2.7 materialises the two eight-byte `pointer_iterator` temporaries of a
> `f(begin(), end())` call in the caller's frame, storing the pointer into both words of each,
> and the resulting aggregate stores are an aliasing barrier that defeats CSE of a later load
> from the object.** Retail's four apparently-dead `stw`s at the bottom of the local area in
> `fn_80006724`, `fn_800068F4` and `fn_80008E94` are exactly that, and no combination of scalar
> locals, `volatile`, structs or arrays reproduces their interleaved store order - only the
> aggregate temporaries do.
