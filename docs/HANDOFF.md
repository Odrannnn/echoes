# Handoff

Orientation for whoever picks this up next, human or agent. Read this first, then
`docs/RUNNING_THE_DECOMP.md` for how the work is run and `PORT_NOTES.md` for how the port
itself works. This file is the map and the current position; those two are the detail.

## The state, measured

```
matched    3128 / 28465 functions        (8.14% fuzzy, 7.21% of code, 5.05% fully linked)
linked     1747 / 28465 functions        (the one rule's count: the unit is Matching and has a source)
DOL units  2762 / 16726 functions        (main/*, including the SDK's 882; 1441 of them linked)
REL units   366 / 11739 functions        (the 86 modules. This line used to add a
                                  "313 linked" I could not reproduce from report.json
                                  with either derivation, so it is gone rather than wrong)
```

Verify all of that yourself; do not trust this file's numbers over the report:

```sh
./tools/decomp_build.sh            # ninja + objdiff + the All: line
python3 - <<'PY'
import re, hashlib, os
cfg = open('config/G2ME01/config.yml').read()
ok = 0
for name, exp in re.findall(r'object: files/RelProd/(\S+)\n\s+hash: ([0-9a-f]{40})', cfg):
    mod = name[:-4]; p = f'build/G2ME01/{mod}/{mod}.rel'
    a = hashlib.sha1(open(p,'rb').read()).hexdigest() if os.path.exists(p) else 'missing'
    ok += a == exp
print(f'{ok}/86 modules match config.yml')
PY
```

Last known good: the commit that last touched this file (`git log -1 --format=%h -- docs/HANDOFF.md`).
As of the numbers above: DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, all 86 RELs
byte-identical to `orig/G2ME01/files/RelProd/`, probe 247 files 0 failures, symbol check 0 missing.(The old form of this line pinned a commit hash, which cannot be written down in the commit that
creates it.)

## If you are picking this up (2026-09-25, end of session)

Read this section, then the rest of this file, then `docs/RUNNING_THE_DECOMP.md`. Everything below is
measured; `python3 tools/gate.sh` is the single command that tells you whether the tree is sound, and
`python3 tools/check_docs_claims.py` tells you whether these files are still true.

**What landed today** (each with the gates run and a per-function report diff, and all of it in the
history with the reasoning):

- `CAi` is **done** - 11/11, `Matching` - and the "cyclic link-order dependency" that this file used
  to call the top blocker **was never real**: the split is accepted, and what looked like a cycle is
  COMDAT weak symbols that both linkers discard. Read "CAi: landed…" in `RUNNING_THE_DECOMP.md` before
  touching `include/MetroidPrime/Enemies/`.
- `CPatterned` is a `Matching` unit too (10/10), landed as the 92-byte accessor cluster rather than its
  0xB58-byte constructor, which is still unwritten and is the largest single known item left.
- `TypesMatch` went 398 -> **508 of 511**; the three that remain are characterised in this file.
- **The frame loop's four `rc_ptr` users are now written** (2026-09-26, lane `g4`), and two of the
  four are byte-exact: `CIOWinManager::RemoveAllIOWins` 51.88% -> **100.00%**,
  `CIOWinManager::PumpMessages` **100.00%** with `CArchitectureQueue::Pop` **100.00%**,
  `CInputGenerator::Update` **98.27%**. The out-of-line copy constructor is real:
  `rstl::CRcPtrData` is a **non-template** base holding `rc_ptr`'s two words - which is what
  retail had, since `fn_80049010` has no mangled name while `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv`
  36 bytes away does - and `src/rstl/rc_ptr_copy.cpp` owns it. `CModel::Touch` also landed, 76
  bytes, `Matching`, and closes `_ZNK6CModel5TouchEi` for the port. **What is left is two mwcceppc
  code-generation differences with no source spelling**, both characterised in
  `docs/research/rc_ptr.md`: the out-of-line copy constructor allocates its AddRef to r5/r4 where
  retail uses r4/r3 (four instructions, and it is why `RemoveAllIOWins` stays `NonMatching` despite
  being byte-exact), and mwcceppc reserves 16 bytes of stack slack for a 0x30-byte aggregate local
  (all of `Update`'s remaining 1.73%).
- **`rstl::rc_ptr` now has retail's layout** (2026-09-26, lane f1) - 8 bytes,
  `{ T* x0_ptr; int* x4_refCount; }`, with the refcount a separate 4-byte `CMemory` allocation
  instead of a `CRefData` control block that retail does not have. **`docs/research/rc_ptr.md`** has
  the evidence and every number. What it bought: `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv` and
  `…<24IArchitectureMessageParm>Fv` 87.84% -> **100.00%**, `main/Kyoto/CObjectReference` 8/10 ->
  **10/10**, `CIOWinManager::AddIOWin` written to **95.24%**, `IOWinPQNode::IOWinPQNode` to
  **100.00%** - and **the DOL's sha1 and all 86 RELs unchanged**, because every unit that moved is
  `NonMatching`. Two corrections it forced: `CObjectReference` has **no member at 0x20** (the
  `x20_refData` in the header was a phantom and the class was the right size all along), and
  `CSimplePool` 0x20 -> 0x24, `CAdditiveAnimPlayback` 0x24 -> 0x28. **And it corrects a standing
  claim**: only *one* of the four frame-loop functions (`frame_loop.md` rows 7-10) was blocked by
  the out-of-line copy constructor - all 15 of `fn_80049010`'s call sites in the DOL are
  `CIOWinManager` methods, and the other three inline the copy. 1,084 of that list's 1,212 bytes are
  unblocked. The blocker that has replaced it is **mwcceppc's `operator new` operands**, which stop
  *any* function that allocates from being `Matching`.
- Three modules (`Puffer`, `WallCrawler`, `ScriptGui`) had lost their `Rel(...)` blocks to config
  clobbers and were restored; `Puffer` was then promoted for real. **16 modules now link our own code**
  (measured by `tools/check_module_wiring.py`, never from memory).
- Upstream `PrimeDecomp/echoes` is now a workstream: five units ported, `CCubeSurface` landed. The rule
  and the measured cost are in "Upstream, and what we take from it".
- Eight tools added or repaired, including `tools/gate.sh` (the whole acceptance test, ~1.9 s) and
  `tools/report_diff.py`; ripgrep the tools table for what each answers.

**The number to watch is the linked one, not the headline.** The report's `All:` line counts functions
at 100% inside units that are not in the binary; the state block above carries both. A change can
raise the headline while lowering what is linked, and `tools/report_diff.py` is what catches it.

**Unlanded work, and where it is.** Three lanes' snapshots are preserved as branches (`git log
master..wave-w2` etc.), not because they should be merged wholesale - each predates later commits and
would revert them - but because their *sources* may still be worth mining: `wave-w2` (audio/input
tools and notes), `wave-w4` (a partial `CDamageInfo` reconstruction), `wave-w7` (upstream batch config,
superseded by the landed sync). Mine them file by file; never copy their `config/` or `configure.py`.

**What I would do next, in order:**

0. **The port's link gap: 585 symbols, measured** (was 732) - and the number to plan against is
   not the total but its shape: **303 other game methods, 233 REL module loaders, 23 unmangled
   `fn_*`/`lbl_*`/globals, 12 static data members, 8 `TypesMatch` bodies, 6 `rstl` templates.**
   `docs/research/port_link_gap.md` has the method and the corrections that produced it: an earlier
   version of the tool said 63, because it filed every mangled game symbol under "C++ runtime", and
   the 20x was invisible while the tool and its document agreed with each other.
   `port_link_gap_list.md` is the generated list. **Three of my own "bulk work, one generator"
   claims have now been measured and all three were wrong** - the correction matters more than the
   count, because each would have sent a lane looking for a generator that does not exist:
   - **The 136 `SLdr*` struct constructors and destructors are closed, and "all trivial in retail"
     was wrong twice over.** **0 of the 68 constructors are no-ops** - 30 construct members and then
     store defaults, the largest is 9,716 bytes - and, decisively, **retail never defines those
     symbols at all**: it spells its implicit constructor/destructor
     `__ct__<len><Class>Fv`/`__dt__<len><Class>Fv` where GCC wants `C1Ev`/`D1Ev`, so there was never
     a retail range to claim and **no `Matching` unit could be written**. The definitions are in
     `src/MetroidPrime/ScriptLoader/SLdrStructMembers.cpp`, in `files.cmake` and **absent from
     `configure.py`**, like `PortGlobals.cpp`. Per-class table: `docs/research/sldr_ctors.md`.
   - **The REL module loaders were neither "all one shape" nor "133 unnamed".** All 159 are named in
     `symbols.txt` (as `fn_*`; dtk could not *pair* them because their only caller is a static
     initialiser), **93 of that 44-byte thunk shape exist in the DOL**, and 73 of the 159 are it.
     **72 were landed as 64 `Matching` units** - see `docs/research/rel_loaders.md`, which tabulates
     all 159. The 86 real loaders are 288 to 3,640 bytes, **77,500 bytes total, ~25x the thunk
     family**, and are the real remaining work in that group.
   - **A gap list is not a closed set of work.** Defining a default constructor constructs its
     members, so closing the `SLdr*` group *opened* 14 new gaps on the way, and a whole-tree sweep
     finds **488** classes under `include/` declaring a constructor or destructor nothing defines.
     The list is what is *reachable*, not what is left.
   **A PC link is what forces this repository's data and bodies to be complete**, and this is where
   they are not. The 31 unmangled globals and functions an earlier revision of this list was
   written about are **all closed**.
1. **Per-module config files.** Give each REL module its own object list instead of one shared
   `Rel(...)` hunk in `configure.py`; that is the structural fix for the clobbers that cost three
   modules today. `tools/check_module_wiring.py` only detects the damage afterwards.
2. ~~**`collect.sh <lane>`**~~ **Done, 2026-09-25**: `tools/collect.sh <lane>` does it in one command -
   three-way apply onto a fresh HEAD worktree, report baseline from *unmodified* HEAD, then
   `tools/gate.sh` on the merged result, then the diff stat. 6.8 s per lane, and a stale
   `config/` file becomes a visible conflict instead of a silent revert. Collection is no longer
   the bottleneck; the next constraint is that only one lane can be judged at a time.
3. **`CPatterned`'s constructor** - the largest known item. **Started, 2026-09-25**: the 82-slot
   vtable is mapped, the 2904 bytes are accounted for row by row, the header is decoded and a
   compiling skeleton stands at 7.68% in its own unit. What is left is named and bounded: the
   `CAi` argument aggregate (288 bytes) and the 26-field block at `0x420` (312 bytes). Both are in
   the blocker section above.
4. **The blocked near-complete units**, each needing the same class of fix (container/COMDAT
   emission): `CStringTable` 12/14, `CDependencyGroup` 11/13, `CObjectReference` 8/10, `NMWException`
   10/11. Two more are now characterised rather than open: `CPlayerState` is **one unreachable
   4-byte branch** short (MWCC only rotates a loop it cannot count), and `ForgottenObject` is
   **55 bytes of register allocation in 3 functions**, plus a rig defect - a REL unit defining a
   function nothing calls is dead-stripped by mwldeppc and cannot be flipped at all until dtk or
   `tools/project.py` can add a per-module FORCEACTIVE entry. That last one is worth fixing: it is
   a class of module, and it is the only item on this list that is not a matching problem. `CPakFile` was on this list and moved 22/33 -> **24/33** on 2026-09-25 from a shared-header
   fix, not from writing the functions; it still cannot flip (`.text` 1904 bytes over its range)
   and its remaining gap is characterised in `RUNNING_THE_DECOMP.md`.
5. ~~**A policy on raw-offset code.**~~ **Decided and measured, 2026-09-25**:
   `docs/research/raw_offsets.md` sorts every site into three kinds and rules on each - an opaque
   receiver (`const void* self + 0x44f`) is retail's own shape and stays; an unmodelled member of a
   modelled class is debt with a named blocker; a whole class written as raw offsets is not
   acceptable. `tools/check_raw_offsets.py` measures it (43 sites, 12 files - the review's 26 in
   `ScriptFrontEndDataNetwork` is exact) and fails the gate on a new one. **The remaining work is
   that one file**: model `CFrontendDataNetwork` so its 26 accessors become members.

**The whole review is in the repository** - `docs/reviews/2026-09-25-rig-review.md` - with what was
adopted from it and the five items still open. It is worth reading before trusting any tool here,
including the ones added today: two of the tools I landed were empty stubs, and the acceptance test
passed on nothing twice over.

## The port links now, as far as it can — measured 2026-09-25

**This supersedes the long-standing claim that the port has no executable target at all.** Until
today `MP_SDK_HEADERS_ONLY=OFF` was a `message(FATAL_ERROR)` and no link had ever been attempted, so
the link gap was `nm` set arithmetic and the 114 Aurora-attributed symbols were unverified. That is
all changed. The full measurement, and how to reproduce it, is in
`docs/research/port_link_attempt.md`; the short version:

- `-DMP_SDK_HEADERS_ONLY=OFF` **configures and generates**. `metroid_prime2_port` exists. Aurora
  configures from this tree in ~20 s because it fetches its own SDL3 and Dawn — the missing
  dependencies had looked like a blocker and are not one.
- **All 118 game units compile**, zero compile errors.
- The link then failed on **732 undefined symbols and 4 duplicate definitions**, and that is a real
  `ld.bfd` measurement, not an estimate.
- The 4 duplicates were `RELMain`/`RELExit`, and they were the **last thing standing between this
  tree and a link that fails only on missing decompilation**. Resolved: **the link now reports 724
  undefined and zero duplicate definitions.** The scale was worse than the linker first showed,
  because it stops at the first collision — **14 translation units define a `RELMain`**, one per
  REL module we have reimplemented, and on the cube each is a separate module, so the duplication is
  the module system working. A flat link cannot hold fourteen symbols with one name, so each
  module's entry points get a distinct name **on the host only** — `#ifdef __MWERKS__` still
  compiles retail's `RELMain`/`RELExit`, so those `Matching` units are untouched — and the new
  `platform/compiled_modules.cpp` is the registry that owns them, run from `platform/main.cpp`
  before `InvokeCMain`. **Renaming alone would have been a fake fix:** nothing on the host would have
  called the renamed functions, so every module's function-pointer table would have stayed null.
  `src/REL/REL_Setup.cpp`'s `_prolog`/`_epilog` keep their retail names and forward to the registry.
- **This is probably what unblocks the wall lane d3 hit.** d3 measured that
  `CGameArchitectureSupport`'s constructor dereferences `gpTweakPlayerA` and `gpGameState` with no
  null test, so the frame loop is unreachable, and that both globals "are filled only by the Tweaks
  REL module". `port::modules::InitAll()` now runs exactly that module's init before the game's
  entry. Whether that is enough is a separate measurement and is **not** claimed here.
- Two port bugs came out of it, both invisible to `nm`: `src/REL/REL_Setup.cpp` walked the cube's
  linker-synthesised `_ctors`/`_dtors` (no ELF equivalent; fixed with a host branch that leaves the
  retail path byte-identical), and `platform/ai_dma.cpp` was fully written but compiled by nothing,
  so all five AI DMA entry points were missing. Aurora declares four of them and implements none.
- **Negative result:** `src/Dolphin/*.c` — all four configured in `configure.py`, none in
  `files.cmake` — are GameCube register shims written as assembly-in-C and give 15 compile errors on
  the host. Their absence from `files.cmake` is correct. Do not retry.
- **The three module-publish thunks came next**, and they are the whole of the module system's wiring:
  retail has sixteen `Set*` functions that are **eight bytes each** and all of them are `stw r3,off(r13);
  blr` - one store publishing a module's function-pointer table. Three are undefined in the port and
  are the ones the reimplemented modules call (`SetTweaks_FuncPtrs`, `SetLoader_CannonBall`,
  `SetSScriptForgottenObject_FuncPtrs`); `src/MetroidPrime/ModulePublish.cpp` defines them, port-side
  and deliberately absent from `configure.py`, so they close symbols and are **not** yet a `Matching`
  unit. **732 -> 727 -> 724.**
- **`link_gap.py`'s blind spot is now known.** It said 724 where the linker said 727, and the
  three-symbol difference is accounted for. **It now says 559 against the linker's 525**, and the
  important part is *why*: a vtable is only emitted by the TU that
  defines a class's key function, so `vtable for CPlayer` and `typeinfo for CGunWeapon` are
  invisible to `nm` until that key function is written. Closing them needs the key function, never a
  hand-written vtable.

So the port does **not** boot yet, and the honest statement of why is now short: **525 undefined
symbols and nothing else structural** — the module-loading half of the old answer is fixed.
`tools/link_check.sh` measures that number against a recorded baseline, and
`tools/check_docs_claims.py` now fails if this paragraph and the linker disagree, because it is the
number every lane plans against and it has moved fifteen times (732 → 727 → 724 → 562 → 557 → 548 → 544 → 543 → 533 → 532 → 528 → 527 → **525**;
the last step is `CResLoader::GetPakCount` and `GetPakFile` leaving the gap in one lane - two
symbols from a header fix, not from twenty-four of decompilation).

## What is not in git (check these before blaming the tree)

A fresh checkout is **not** self-sufficient. Three things live outside version control, and every
tool fails with a confusing error if one is missing:

1. **The retail data at `orig/G2ME01`** (7.5 MB, untracked and not ignored). `dtk` needs it to split
   the DOL and the 86 RELs, and `config/G2ME01/build.sha1` hashes what it produces. On a copy of this
   directory it is already there; on a `git clone` it is not, and it comes from an owned disc via
   `python3 tools/extract_disc_file.py`. `sha1sum orig/G2ME01/sys/main.dol` should be
   `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` - the same hash everything else is measured against.
2. **The sibling toolchain tree `../MetroidPrimePort`** - MWCC compilers, `dtk`, `wibo`, and a ninja
   under `build/review-tools/bin/`. Nothing here builds it; `MP_TOOLCHAIN_DIR` points at it, and
   `tools/gate.sh`, `tools/decomp_build.sh` and `tools/flip_test.sh` all default to that sibling path
   and fail loudly if it is wrong. The lane briefing has the export line.
3. **`build/`** is ignored and regenerable - `tools/gate.sh` configures and builds it from nothing in
   about two seconds, given 1 and 2.

Also worth knowing, though nothing breaks without it: the upstream reference clone the sync
workstream reads (`git clone --depth 1 https://github.com/PrimeDecomp/echoes`), and the lane
worktrees under `/tmp/opencode`, which are disposable and can be removed (`git worktree list` currently
shows 61, holding ~11 GB of a tmpfs).

**The first command to run in a fresh session is `python3 tools/gate.sh`.** It configures, builds,
checks the DOL and all 86 RELs against `config.yml`, diffs the report per function, and checks the
module wiring, the docs' claims and the port probe - about two seconds, one verdict line, non-zero
exit on any failure. If it passes, the tree is sound and the documentation in this file is current;
if it fails, read the failing step before anything else.

## What this repository is

Two things at once, and it is easy to confuse them:

- **A port** of Metroid Prime 2: Echoes to PC, built on Aurora (the MIT GameCube SDK/GX
  replacement). The port layer is essentially complete: REL runtime, entry point, SDK shims,
  disc tools, tests. It cannot run because the decompilation is 7.95% done by fuzzy match and
  5.81% linked (1,653 functions of 28,465 are in a `Matching` unit that is really in the binary).
- **A contribution to the decompilation** (`PrimeDecomp/echoes`), which is what the remaining
  work actually is. Every rule about completion in `RUNNING_THE_DECOMP.md` comes from this half.
  The public upstream tree is reachable and **ahead of us in units we have not written** (and behind
  in others); from 2026-09-25 the rule is to port from it only where the gates pass, attributed in
  the commit. See "Upstream, and what we take from it" in `RUNNING_THE_DECOMP.md` for the measured
  cost of doing that and the units it has been done for.

## Where the research lives

Seventeen files carry what a later session would otherwise have to re-derive, and each answers one
question that used to cost a session:

| file | the question it answers |
| --- | --- |
| `docs/research/boot_path.md` | **the measured, step-by-step map from this tree to a rendered frame** — 25 steps, each with its retail address, size, current state and what it blocks. Read this before planning any port work |
| `docs/research/tweak_globals.md` | **all 1,452 bytes of `REL_CreateTweakGlobals`, store by store** — and the finding that `gpTweakPlayerA` ends up pointing at a 4-byte heap cell and *not* at a `CTweakPlayer`, so this function is not what unblocks the frame loop. **Its size-drift table is superseded** — see the next row |
| `docs/research/tweak_player.md` | **the 4-byte cell, retail's five `CTweakPlayer` thunks (address, size, the offset each reads), and the correction that `CTweakContents` is 0x3244 and not 0x37D0** — a 64-bit host probe, not a modelling gap. Also the reusable rule: never measure a layout with a host compiler |
| `docs/research/port_link_attempt.md` |
| `docs/research/rel_rename_hazard.md` | **why 16 REL modules stay out of the port build**: a host-only `#ifdef __MWERKS__` rename of their `RELMain`/`RELExit` leaves every object byte-identical and the DOL hash intact, and still changes 8 of 86 module hashes. Ten experiments, two of which were wrong |
| `docs/research/rc_ptr.md` | **retail's `rstl::rc_ptr` is 8 bytes**, seven independent lines of evidence, against this tree's 4 - and the change unblocked 1,084 of the frame loop's 2,584 bytes. Also carries the correction that the `operator new` literal is **not** a global blocker |
| `docs/research/rstl_string_member_op.md` | **done**: `basic_string`'s member `operator+(const char*)` is a `Matching` unit at 100%, claiming 0x80021634. Carries the shape measurement - the `C` is the const marker and sits after the template-id's `>`, so retail's is the non-const member - and the correction that the blast radius is **2 call sites, not the tree** |
| `docs/research/port_link_gap.md` | what the port still needs in order to link, the correction that fixed the measurement, and which kind of work closes each group |
| `docs/research/decl_order.md` | which units emit their functions out of retail order, and what else blocks each |
| `docs/research/raw_offsets.md` | every raw-offset field access, sorted into the three kinds, with a blocker each |
| `docs/research/CPatterned_vtable.txt` | all 82 slots of `CPatterned`'s vtable, with kind and owner |
| `docs/research/CPatterned_layout.txt` | the constructor's 2,904 bytes, every byte in exactly one row |
| `docs/research/TypesMatch_unnamed_ids.txt` | the 32 classes `TypesMatch` names by id, and the parent of each |
| `docs/research/rel_module_order.md` | **the 86 modules in a verified load order**, derived from their own import tables by `tools/gen_module_order.py` — 27 have dependencies, max depth 2, **0 ordering violations**. The half of the module manager that does not need a disc image |
| `docs/research/rel_module_manager.md` | **the module manager: the runtime is built and tested against all 86 retail modules, and the one input it lacks is the module descriptor table** — measured absent from the DOL (2 of 86 names, as incidental strings), from `config/` and from `orig/`. Names the input that would unblock it |
| `docs/research/rel_loaders.md` | **all 159 entity loaders**: address, size, shape and dispatch global for each, how `__sinit_ScriptLoader_cpp` yielded every address, and the 64 units that landed |
| `docs/research/sldr_ctors.md` | the 136 `SLdr*` struct constructors and destructors per class, and why **retail never defines those symbols** so no `Matching` unit could exist |

The techniques and the negative results are in `docs/RUNNING_THE_DECOMP.md`; the traps a lane
will otherwise hit are in `docs/LANE_BRIEFING.md`. **A finding that is only in a commit message
is a finding the next session pays for twice** - if you learn something the tree does not say,
put it in one of these in the same commit as the change that taught it to you.

**Two claims about the boot path were wrong and are corrected in place.** `CMain::OpenWindow`
**does not exist in retail Echoes** — 19 `CMain` methods are named in `symbols.txt` and it is not
one of them, the string occurs nowhere in the DOL, and `RsMain` (0x80005C6C, 0x864 bytes) makes no
call on `x0_osContext` at all. Retail's window/VI bring-up is in `main` (0x801EFB00) via
`fn_802BE85C` → `fn_802C329C` → `fn_802C2FD4`. And the frame loop is **not** unreachable for lack
of decompilation: its body is 2,584 bytes across **twelve symbols that are already on
`port_link_gap_list.md`**. What blocks it is two null dereferences in
`CGameArchitectureSupport`'s constructor (`gpTweakPlayerA` at 0x80007F38, `gpGameState` at
0x800081A4, neither null-tested).

**And the obvious fix for that wall is now measured, and it is not the fix.** I suggested that the
new `port::modules::InitAll()` would fix it, because d5 had reported those globals "are filled
only by the Tweaks REL module". That was half right and I should have checked before writing it
down. Lane d5 read all 1,452 bytes of `REL_CreateTweakGlobals` and established three things:
`gpTweakPlayerA` is assigned from a `new[4]` whose only word is a pointer, so it lands on a
**4-byte heap cell, not a `CTweakPlayer`** — and the two calls the wall is about,
`GetLeftAnalogMax`/`GetRightAnalogMax`, are themselves undefined; **`sizeof(CTweakContents)` is
0x37D0 here against retail's 0x31F4**, 1,500 bytes too big because the generated `SLdr*`
headers mis-size members from `TweakBall` on, so every offset read through it is wrong; and
`STweaks_FuncPtrs::CreateGlobals` is assigned in `TweaksInit` but **invoked by nobody**, since the
module's `Loader` never runs first. The order of work is therefore (a) model `CTweakPlayer` as the
4-byte wrapper with real accessors, (b) fix the `SLdrTweak*` sizes from the retail `LoadTypedef*`
bodies, (c) give the Tweaks module a caller, and only then (d) `gpGameState`, which needs
`CMain::StreamNewGameState` and therefore the paks. Details in `docs/research/tweak_globals.md`.

**Two of those four items are now done, and the second was much smaller than it looked.**
`docs/research/tweak_player.md` (lane e4, 2026-09-26):

- **(a) is done.** `include/MetroidPrime/Tweaks/CTweakPlayer.hpp` models the 4-byte cell
  (`SLdrTweakPlayer* mTweak`) and all five accessors have bodies in
  `src/MetroidPrime/PortGlobals.cpp`, written against **named members**. All five are named
  12-byte thunks in `config/G2ME01/symbols.txt` (0x80217D30/3C/48, 0x802184CC/D8) and each
  compiles to retail's bytes exactly — verified with mwcceppc, 12/12 each. `link_gap.py`
  **559 -> 554**; matched and linked **did not move** (3115/1725), which is the point of putting
  them there.
- **(b) was a measurement error, not a modelling gap.** The "0x37D0, +0x138 at `TweakPlayer`"
  came from a **64-bit host `g++`** probe: the port build is 64-bit, so `rstl::string` is 24 bytes
  there against retail's 16. Compiled with **mwcceppc (32-bit)**, all sixteen `CTweakContents`
  members are at retail's offsets and every size is retail's **except `SLdrTweakPlayerRes`
  (0x548 vs 0x4F8)**; `sizeof(CTweakContents)` is 0x3244, not 0x37D0. Two independent checks:
  our own retail-matching `__ct__14CTweakContentsFv` emits `addi r3,r31,0x10e8`, and the five
  thunks are byte-exact. **The rule now written down: never measure a layout with a host
  compiler** — emit `offsetof` with the unit's `cflags` into a `.data` array and read it with
  `objdump -s`. `tweak_globals.md`'s drift table is marked superseded, not deleted.

What is left on step 17 is therefore (b') one struct, `SLdrTweakPlayerRes`, and (c) a caller for
the Tweaks module. `gpTweakPlayerA` is still `nullptr` and the second null dereference,
`gpGameState`, still needs the paks.

## Tools, in the order you will want them

| | |
| --- | --- |
| `./tools/decomp_build.sh [unit]` | ninja, then objdiff, then that unit's unmatched functions |
| `tools/flip_test.sh <unit>` | **the acceptance test** - flip to `Matching`, rebuild, keep only if the DOL and all 86 RELs still reproduce retail |
| `tools/compare_unit.sh <unit>` | diagnostic: how our object differs from the retail-derived one |
| `tools/fast_try.sh <unit>` | rebuild one object, print only that unit's scores - the loop to use while trying source variants |
| `tools/lanediff.sh <unit> [sym]` | one function, retail against ours, addresses and branch targets stripped so only real differences show |
| `tools/collect.sh <lane>` | three-way apply a lane's diff onto a fresh HEAD worktree, baseline the report from unmodified HEAD, then run the whole gate on the merged result - collection in one command, ~7 s |
| `tools/try_batch.py <src> <unit> <sym> <variants.py>` | try N bodies for one function in one run, ranked by **differing instructions** rather than objdiff's byte percentage; always restores the source |
| `tools/check_raw_offsets.py` | every raw-offset field access, against the policy in `docs/research/raw_offsets.md` - in `gate.sh` |
| `tools/check_files_cmake.py` | **every configured, on-disk unit is in the port build or excluded with a reason.** 96 were in neither, invisibly, because `link_gap.py` derives the gap from the objects `files.cmake` produces; in `gate.sh` |
| `tools/link_gap.py` | what the port's game library still needs to link, by `nm` arithmetic, against `docs/research/port_link_gap.md`; in `gate.sh`. It cannot see a vtable before its key function exists |
| `tools/link_check.sh` | **the real link.** Configures with `MP_SDK_HEADERS_ONLY=OFF`, builds the executable, and reports the linker's own undefined and duplicate counts against `docs/research/port_link_baseline.txt`. The slow gate — run it before committing anything touching `CMakeLists.txt`, `files.cmake`, `platform/`, or the port side of a unit, which is exactly what the fast gates cannot see. `--rebuild` to force a clean configure, `--record` to move the baseline |
| `tools/check_decl_order.py` | which units emit their functions out of retail order, against the work list in `docs/research/decl_order.md` - in `gate.sh` |
| `tools/check_symbol_names.py` | every name `symbols.txt` declares vs what the retail object defines |
| `tools/find_trivial_functions.py` | unmatched functions classified by machine-code shape - the cheap-work queue |
| `tools/scaffold_rel_module.py` | the three artifacts for starting a REL module |
| `docs/research/CPatterned_vtable.txt` | all 82 slots of `CPatterned`'s vtable, with kind and owner |
| `docs/research/CPatterned_layout.txt` | the constructor's 2904 bytes, every byte in exactly one row |
| `tools/probe_sources.sh` | the port build's syntax sweep (247 files) || `build/binutils/powerpc-eabi-objdump`, `powerpc-eabi-nm` | disassemble / list symbols |

There is **no system cmake or ninja**. Use
`/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort/build/review-tools/bin/`
for cmake/ctest/ninja, and that port's `build/compilers` and `build/tools/{dtk,wibo}` for the
matching build.

## The one rule that decides whether work counts

A unit is done when the build still reproduces retail **with that unit's own object in the
link**. `configure.py` decides this: `NonMatching` links bytes `dtk` split out of the retail
binary, `Matching` links our compile. So objdiff percentages on a `NonMatching` unit are a
signal, not a result, and `87 files OK` proves nothing about a unit that is not `Matching` -
it validates the untouched parts of the binary. Two sessions were spent on this; see
"the one rule" in `RUNNING_THE_DECOMP.md`.

## Two independent workstreams, and where each stands

**1. The DOL** - 2667 of 16726 functions, ~14k left (that figure includes the SDK's 882, which are
essentially complete). Verified matches land here steadily, and the two units the whole port was
waiting on are in: `CAi` 11/11 and `CPatterned` 10/10, both `Matching`. Others:
`TypesMatch` 508/511, `CStateManager` 52/239, `CPlayerGun` 60/135, `CPlayerState` 68/72.
(Those three fell on 2026-09-26 when lane f1 made `rstl::rc_ptr` retail's 8-byte width - all
three are `NonMatching`, so none of them is in the binary and the DOL's sha1 did not move. See
`docs/research/rc_ptr.md`.)
The reachable pools are thinning; what remains is dominated by FPU register allocation,
instruction scheduling, string-pool offsets, and weak rstl instantiations whose callers are
not decompiled. All of those are documented in `RUNNING_THE_DECOMP.md` - check it before
spending a session rediscovering one.

**`TypesMatch` was gated on naming, and that is now measured and paid off** (2026-09-25).
508 of 511 functions are exact: the first 94 (`30` `TypesMatch` overrides plus `64` `TCastToPtr`
casts) were landed under the placeholder class names `CUnknown<id>`, because every naming source
in the tree - the previous versions' configs, `symbols.txt`, the DOL's strings, the RELs - was
exhausted and none of them names those 32 classes. The *shape* is not a guess: each class's parent
is the class whose `::TypesMatch` the retail override calls, and the addresses come from the vtable
holding each id. `docs/research/TypesMatch_unnamed_ids.txt` is that table;
`docs/research/rename_typesmatch_ids.py` regenerates the block from it, so identifying a class later
is one line and a re-run, and it is now known what three of those classes are: id 76 has no members,
id 63 is `optional_object<TCachedToken<T>>` at 0x158, id 50 is an empty `CScriptDamageableTrigger`
subclass. **Three functions remain**, all characterised: `fn_8009D3D8` (93.27%) and `fn_8009D45C`
(79.70%) need a stack home and an outgoing-arg copy retail has and no source shape produced, and
`fn_80097520` is a 32-byte MWCC thunk to an unnamed function that nothing references.

**2. The REL modules** - 331 of 11739 functions, 86 modules. That count is low partly because
claiming a range for a unit *removes* those bytes from the `auto_*` units that match for free -
see "why the matched total can go down" in `RUNNING_THE_DECOMP.md`. **The recipe works and is written
up**: a module may be partly decompiled, with the `Matching` unit claiming only the ranges its
own object reproduces and everything else unclaimed so `dtk` fills it from retail.

**Measure this, never recall it**: `python3 tools/check_module_wiring.py`. As of the last commit it
reports **32 units of our own code in 18 modules** - `AIMannedTurret`, `FlyerSwarm`, `Metaree`,
`Puffer`, `RubiksPuzzle`, `ScriptCoin`, `ScriptFrontEndDataNetwork`, `ScriptGui`,
`ScriptPlayerActor`, `ScriptPlayerProxy`, `ScriptPlayerTurret`, `ScriptRiftPortal`, `ScriptRsfAudio`,
`ScriptSafeZone`, `ScriptStreamedMovie`, `SwarmBasics`, `Tweaks`, `WallCrawler`. `Puffer` joined by being promoted rather than restored: with
its two units `Matching` the mutation check (change one byte of our source, the module hash must
break) proves our object really is in the link. The list this paragraph used to carry was wrong in both
directions and is exactly the kind of claim that must not be written from memory:

- `Puffer`, `WallCrawler` and `ScriptGui` had **lost their `Rel(...)` blocks** to later commits that
  copied an older `configure.py` (`33b73a3` replaced Puffer's block with WallCrawler's own; `f599488`
  dropped WallCrawler's and ScriptGui's). Their sources had been sitting in `src/` compiled by
  nothing, which is why the report showed those units at 0.00%. Restoring the blocks and the
  `Matching` states they had is worth **30 matched functions**, and `check_module_wiring.py` exists so
  the next one is caught the same day.
- `AIMannedTurret` was called "the working example" - it is not. Its unit is at 3/3 in the report, but
  promoting it to `Matching` **breaks the module's hash** (85/86), so its code is not in the link and
  never was. It stays `NonMatching`.
- `Puffer`'s units were `NonMatching` even in the commit that landed them, so its earlier "sha1
  verified" claim proved nothing - the vacuous-verification trap the rules warn about.

**The creature base classes now exist** (`CAi`, `CPatterned`), so the 75 creature and swarm modules
are no longer blocked on the hierarchy existing - only on their own code, and on `CPatterned`'s
0xB58-byte constructor if they need it.

## The pak chain: `CResLoader` is typed, the pump is linked, and `CPakFile` is the wall

**2026-09-26, lane g1.** Step 13 of `docs/research/boot_path.md` is the keystone of the resource
system and the thing every pak load goes through is `CResLoader::AddPakFileAsync` (already
`Matching`). What that reached has now been measured and two thirds of it landed:

| unit | range | score | note |
| --- | --- | --- | --- |
| `Kyoto/CResLoaderPakPump.cpp` | 0x802FCCE4..0x802FCD90, 172 B | **100.00%, `Matching`** | `AreAllPaksLoaded` and `AsyncIdlePakLoading` - the loop `AddPaksAndFactories` block 7 drives |
| `Kyoto/CResLoaderGetPakCount.cpp` | 0x802FBC60..0x802FBC70, 16 B | **100.00%, `Matching`** | closed a ratchet symbol |
| `Kyoto/CResLoaderGetPakFile.cpp` | 0x802FBA68..0x802FBB64, 252 B | 80.13%, `NonMatching` on purpose | one shape away; the range keeps retail's bytes |
| `Kyoto/CPakFile.cpp` | 0x80323038..0x80324D44, 7,436 B | 88.30%, 24/33 at 100%, `NonMatching` | **blocked on `include/rstl/`**, see below |

**`include/Kyoto/CResLoader.hpp` was wrong, and a function could not be written without fixing
it.** `CResLoader` is **0x60** bytes, not 0x58, and it is four `rstl::list< SPakLoadEntry >` at
**+0x00, +0x18, +0x30 and +0x48** - the header had four scalars at +0x48. `AreAllPaksLoaded` reads
the list's `x14_count` at +0x5C, and that is the *same word* `fn_802FD174` decrements, which is
what proves +0x48 is a list. Every offset downstream moved by 8, `CFactoryMgr` included
(0x5C → 0x64, so block 9's 36 registrations target `CFactoryMgr`+0x10 and not +0x18), and
`CResFactory` is 0xD0. `docs/research/paks.md` has the instruction-level evidence and this
corrects two claims in that file: the +0x5C, and the loop polarity in block 7 (the body runs while
`AreAllPaksLoaded()` is **false**).

**`CPakFile` is blocked, and not on decompilation.** The constructor (0xEC) and the destructor
(0xF8) - the two the chain actually needs - were **already 100%**, as are 22 others. The two
blockers are `reserve<rstl::vector<CPakFile::SResInfo>>` at **33.84%**, which needs
`include/rstl/rmemory_allocator.hpp` changed (retail inlines `CMemory::Alloc` with a `CCallStack`;
this tree's `allocate` is out of line and uses `rs_new`), and `RebuildResourceLists` at **39.63%**,
which calls an unnamed `fn_80052220` where the port calls `reserve<rstl::vector<uint>>`. A pak
lane cannot fix either. **A lane that owns `include/rstl/` unblocks 33 functions here**, and the
payoff is the whole resource system.

**The destructor can hang a host, and it is now guarded.** `~CPakFile` spins on `AsyncIdle()`
until `x2c_asyncLoadPhase == kAP_Loaded`. **The obvious reading of the hazard is wrong, and
correcting it is the useful part**: `AddPakFileAsync` does contain a same-call `delete`, but the
insert it calls *clears the caller's flag byte* (`fn_802FC378`'s `stb r0,0(r30)` with `r0 = 0`,
0x802fc3d0), so the branch is not taken; and `fn_802FD174` only destroys an entry whose pak
`IsCompletelyLoaded()`. **All three of retail's paths are safe.** The real hazard is that
`InitialHeaderLoad` (0x80323F0C) **returns without advancing the phase** when the pak's first word
is not 0x30005, and `CInputStream::ReadInt32` has no bounds check - so a foreign or truncated pak
never reaches `kAP_Loaded`, and *any* path that destroys a `CPakFile` without testing the phase then
loops forever, silently, with no frame ever drawn. A healthy pak does finish (Aurora's
`DVDReadAsync` is genuinely asynchronous, a worker thread). `src/Kyoto/CPakFile.cpp` keeps retail's
loop for mwcceppc and bounds it under `TARGET_PC`, naming the pak and the phase when it gives up -
the same arrangement as `src/Kyoto/CResLoaderAddPakFileAsync.cpp`, with the reason and the
measurement at the definition. **The wait is not dropped, and the unit's scores are identical
before and after the block was added.**

**Still missing between this and a constructed `gpResourceFactory`:** `CResFactory::AsyncIdle`
(0x802FA384, 268 bytes, on the ratchet, called by the written `CMain::AsyncIdle`), which needs a
`CResFactory` member model past +0x9C that nothing in the tree has;
`fn_802FC350`/`fn_802FC378`, the list insert that `AddPakFileAsync` calls by name; and the 33
`CPakFile` functions above.

## The blocker: CPatterned, and the base classes below it

**`CAi` is done** (2026-09-25): 11 of 11 functions, `Matching`, DOL sha1 and all 86 RELs still
reproducing retail. The "cyclic link-order dependency" that this section used to describe **was
never real** - the split is accepted, and what blocked it was ordinary symbol renaming. What looked
like a cycle was CodeWarrior COMDAT weak symbols that both linkers discard. The full account, the
rename list and the two traps (claim *all* four sections; a `.sdata2` split may not end inside a dtk
`lbl_`) are in `RUNNING_THE_DECOMP.md`, section "CAi: landed, and the cyclic link-order dependency
was never real". Read that before touching anything in `include/MetroidPrime/Enemies/`.

**`CPatterned` is landed as well** (2026-09-25): a `Matching` unit with 10 of 10 functions at 100%,
claimed as the 92 bytes of the small accessor cluster at `0x80073C58..0x80073CB4` rather than the
0xB58-byte constructor. The class exists, its vtable relocations resolve, and the creature modules
are no longer blocked on the hierarchy existing - only on their own code.

**The constructor has now been measured, mapped and started** (2026-09-25, later the same day).
It is no longer an untouched 2904 bytes:

- **`docs/research/CPatterned_vtable.txt` - all 82 slots**, read from the relocations of
  `auto_07_803B1C40_data.o` and cross-checked against the linked DOL. Slots 0 and 1 are `0x00000000`
  with no relocation, so the two RTTI words were stripped and no vtable dumper can name the type.
  20 of the 82 point at another class's code (8 at `CActor`, 5 at `CPhysicsActor`, 6 at `CAi`), which
  is what fixes the numbering: `CPatterned` owns 2-13, 16-24, 28-29, 33, 36, 38-39, 41, 46-81.
- **`docs/research/CPatterned_layout.txt` - the 2904 bytes, every byte in exactly one row.**
  47.8% is member initialisation with no call in it, 19.4% is three copies of the same anim-token
  boilerplate, 10.1% is the caller's materialisation of the aggregate `CAi`'s constructor takes, and
  11.0% is calls into unwritten bodies. Two things are genuinely unknown, and both are named: the
  `CAi` argument aggregate (288 bytes, and it caps the score) and the access width of the 26-field
  block at `0x420` (312 bytes).
- **`include/MetroidPrime/Enemies/CPatterned.hpp` is decoded**, not stubbed. The
  `x4a0_undecoded[0x2b4]` blob is now six named sub-objects plus real members out to `0x7c0`;
  `CHECK_SIZEOF(CPatterned, 0x7c0)` still holds and the unit is still 10/10 at 100%. Four header
  members were **wrong** and are corrected: `x39c_` is a heap `CToken*`, not an `int`; `x470_` is
  one `CToken`, not a 12-byte token plus 12 bytes of padding; `x34c_28_` is fed from
  `kInvalidUniqueId`, not from `moveType`; and `x448_` starts at -1.0f, not 0.
- **`src/MetroidPrime/Enemies/CPatternedCtor.cpp` compiles** as its own `NonMatching` unit, 1464
  bytes, paired by renaming `fn_80079BE4` to the mangled name MWCC emits. It measures **7.68%** -
  low, and reported as such. The unit is `NonMatching`, so **none of it is in the link and neither
  `matched` nor `linked` moved**; the value is the vtable map, the layout and the next step.

**The next step, measured rather than guessed:** find the aggregate `CAi`'s constructor takes as
its second argument. Retail passes it in a register *and* spills six words to the stack, so it is a
struct big enough that MWCC split it - read `CAi::__ct__` in the `Matching` `CAi.o` and look at how
it reads its own incoming stack words. That is 288 of the 2904 bytes and the only thing standing
between 7.68% and a comparable score. Second, and independent: the 26-field block at `0x420` is
**word**-granular in retail and no declaration tried makes MWCC 2.7 choose word granularity for a
run of one-bit fields (see "MWCC's bit-field granularity" in `RUNNING_THE_DECOMP.md`); it is 312
bytes and worth the same lane's attention.

The other hierarchy gaps, unchanged: `include/MetroidPrime/Enemies/` holds the `SwarmBasics` layer
and now `CAi`/`CPatterned` headers; there is still no `CPatterned.cpp`, no GUI hierarchy
(`src/GuiSys/` and `include/GuiSys/` are empty and unlisted), and `UnkVtable20` is resolved.

## Running lanes

The work is done by many agents in parallel, one per module or unit, each in its own git
worktree with its own real `build/` directory (not a symlink - see the rig defect in
`RUNNING_THE_DECOMP.md`). The full spawn sequence, the non-negotiable details, how to collect a
lane, and the failures that recur are all in that file's "Parallel lanes" section. The two
things that cost the most time:

- **A lane's `config/` is its own view, never a patch.** Copying a lane's `symbols.txt` or
  `splits.txt` can silently revert a later rename, or delete an existing split block. Merge by
  hand against current `HEAD` and diff. This cost a lost `UnkVtable20` rename and two lost
  `configure.py` entries.
- **Verify a lane's report independently.** They have been wrong in both directions - one
  understated its own result by ten functions, several cited verification that proved nothing
  because their unit was `NonMatching`, and one reported its completed work as missing.

## Keeping this documentation true

These three files are load-bearing: a session that trusts a stale handoff wastes its whole
budget re-deriving what the last one knew. So updating them is **part of finishing a piece of
work**, not a separate chore. The rule is short:

**If you changed the answer to a question one of these files answers, update that file in the
same commit as the change.**

**A claim that can be derived must be derived, and that now has a checker.**
`python3 tools/check_docs_claims.py` verifies the numbers this file and `RUNNING_THE_DECOMP.md` quote -
the state block, the per-unit counts in the prose, the list of modules that link our code, and the
pinned hashes - against `build/report.json` and `tools/check_module_wiring.py`. Run it before
committing anything that moves a number, and after any config merge. It exists because the rule below
was in place for a whole session while a paragraph still listed sixteen modules linking our code when
three of them had no `Rel(...)` block at all: the state block was current and the prose was not, and
prose is where the reader forms their plan. Two more stale per-unit counts were found the moment the
checker was first run.

Concretely, after any turn that changes the position or the method:

| what changed | where it goes |
| --- | --- |
| matched counts, which units/modules are done | the state block at the top of this file |
| a unit or module is now `Matching` and verified | the state block, and the module table in `RUNNING_THE_DECOMP.md` |
| a new blocker found, or an old one cleared | the blocker section here, and the relevant one in `RUNNING_THE_DECOMP.md` |
| a technique that worked, or a pattern that cannot work | `RUNNING_THE_DECOMP.md` (recipe, known-hard, gates) |
| a module attempted, whatever the outcome | the "Attempted modules" table at the end of `RUNNING_THE_DECOMP.md` |
| how the port itself works | `PORT_NOTES.md` |
| a lane-collection or lane-spawning lesson | the "Parallel lanes" section of `RUNNING_THE_DECOMP.md` |

Rules for the writing itself, learned by getting it wrong:

- **Measure numbers, do not recall them.** Every wrong figure found in these files was written
  from memory. Run `./tools/decomp_build.sh` and the config.yml hash check and quote what they
  print. If a number in a doc disagrees with `build/report.json`, the report wins.
- **Annotate stale checkpoints, do not silently rewrite history.** If a figure was right at the
  time it was written, leave it and mark it as a checkpoint pointing at the current source of
  truth. `PORT_NOTES.md` line ~340 does this.
- **Say what is not known.** "CPatterned's constructor was not attempted" is worth more than a
  confident-sounding guess, and it is what stops the next session repeating the work.
- **A negative result belongs in the docs.** The CAi link-order cycle, the assembly dead end,
  the vacuous `NonMatching` verification - each cost a session, and each is now one paragraph
  that saves the next one.
- **Do not let a doc outlive its claim.** If a statement is superseded, correct it in place and
  note that it was superseded; an earlier version of `RUNNING_THE_DECOMP.md` concluded that no
  module could be decompiled, which was wrong two sessions later.

There is a repo `AGENTS.md` that repeats the short version of this, so an agent that never
opens this file still sees it.
