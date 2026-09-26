# Handoff

Orientation for whoever picks this up next, human or agent. Read this first, then
`docs/RUNNING_THE_DECOMP.md` for how the work is run and `PORT_NOTES.md` for how the port
itself works. This file is the map and the current position; those two are the detail.

## The state, measured

```
matched    3973 / 28465 functions        (8.47% fuzzy, 7.53% of code, 5.31% fully linked)
linked     2550 / 28465 functions        (the one rule's count: the unit is Matching and has a source.
DOL units  3308 / 16726 functions        (main/*, including the SDK's 892)
port link  323 undefined, 0 duplicates   (tools/link_check.sh --rebuild; the linker is the
                                   ground truth for the port, and docs/research/
                                   port_link_baseline.txt is recorded at the same 322)
REL units   665 / 11739 functions        (the 86 modules. This line used to add a
                                   "313 linked" I could not reproduce from report.json
                                   with either derivation, so it is gone rather than wrong)
```

That block must appear **exactly once**, and `tools/check_docs_claims.py` now fails if it
does not. Three copies were fused together inside one fence by successive lane merges,
carrying three different sets of numbers (`3241/1831`, `3240/1830`, `3240/1830`) - and the
checker passed the whole time, because it looked for the correct figure and *found it among
the contradictions*. A check that cannot fail on the most obvious way this file goes wrong is
not a check; see `docs/PROCESS_LESSONS.md` #1.

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
byte-identical to `orig/G2ME01/files/RelProd/`, probe 636 files 0 failures, symbol check 0 missing.
byte-identical to `orig/G2ME01/files/RelProd/`, probe 636 files 0 failures, symbol check 0 missing.
(The old form of this line pinned a commit hash, which cannot be written down in the commit thatcreates it.)

## Where the port is: step 17, and the three functions in front of it

**The boot advanced.** It used to stop at `boot stopped: gpGameState (DOL 0x80418EB8) is null`; it
now runs the `CGameState` constructor, fills both globals, reaches
`CGameArchitectureSupport`'s constructor and stops there - step 17. The full before/after, the
`+6` the link gap cost for it, and why acceptance is the stop message rather than the count, are
in `docs/research/boot_probe.md` under "The boot reached step 17".

Three functions stand in the way, and the port's own stop message names them:

| function | retail | state |
| --- | --- | --- |
| `CMain::ResetGameState` | 0x80003A48, 0x1A0 | **98.61%, `NonMatching`** (body landed; the two fixes are described above) - the unit is in the tree but did not promote. Two fixes were reported as needed *together* and were not landed: the empty loop is the inlined destructor of a sub-object at +0x10, and the allocation is spelled `new CGameState`. **This was claimed `Matching` at 100.00% until an independent review checked; see the correction below.** |
| `CErrorOutputWindow::CErrorOutputWindow(bool)` | 0x8018169C, 0xB4 | **no landed source** - only `PortReachStubs.cpp` defines it, and it is still on the link-gap list. A lane wrote it to 78.56% in its own worktree; that work was never landed. **Also claimed as landed until the same review; see the correction below.** |
| `CConsoleOutputWindow` | 0x801816D8-ish, 109 instr | **not started** - from scratch; its member map and callees are in this file's port section |

**`CErrorOutputWindow` is a compiler wall, not a source problem, and that is now proven rather than
suspected.** Retail computes a `bool` with `cntlzw r0,r31`; ours emits `clrlwi r0,r31,24` then
`cntlzw`, and the `clrlwi` is what forces the `srwi r4,r0,5` that follows. **mwcceppc 2.7 masks every
`!` applied to a `bool`-typed operand.** Nine operand spellings and three destination types
(`int:1`, `u32:1`, all-`int`) were measured and all mask. The fix is a second compiler version, not
a source change, so do not spend budget here.

### Correction, 2026-09-26: two of the three were claimed landed and were not

An independent review of commit `e7337ed` checked its claims against the tree instead of its
prose, and found that **`CMain::ResetGameState` is 98.60577% and `NonMatching`**
(`configure.py:512`), and that **`CErrorOutputWindow` has no landed source at all** - only
`src/MetroidPrime/PortReachStubs.cpp` defines `_ZN18CErrorOutputWindowC1Eb`, and the symbol is
still on the port's link-gap list.

Both errors were the same one: **a lane reported a result and this file repeated it as fact
without checking the tree.** The lanes were accurate about their own worktrees. So the
correction is not "the lanes were wrong" - it is that *a report is not a measurement*, and the
cheapest possible check would have caught it: read `report.json` and `configure.py`.

**Step 17 is therefore still open, and all three of the functions it names still need work.**
`CConsoleOutputWindow` is not started; `CErrorOutputWindow` needs its body landed; and
`ResetGameState` needs 1.39% and the two fixes below. The compiler wall in
`CErrorOutputWindow` is real and still worth knowing, because it is what a lane would hit
*after* landing the body.

## The carve vein is the cheapest `Matching` in the tree, and it has four traps

**11 units / 14 functions** landed in one batch and **62 units / 188 functions** in another, all at
100.00%, all `flip_test` PASS, by carving single retail functions out of dtk `auto_*` ranges. The
method, the traps (contiguous runs are one unit because `linked` counts functions; a comparison's
operand order decides register allocation; a carve that `PortLinkStubs.cpp` also defines is a
duplicate; and a carve adjacent to an existing *unit boundary* fails `dtk dol split` with a
link-order cycle before anything compiles) are written up in
`docs/RUNNING_THE_DECOMP.md` under "The carve vein".

**Supply remaining, measured: 4,329 candidates <= 64 B**, of which 198 further isolated
`li r3,N; blr` / `blr` singles would each be a +1/+1 unit, and **279 `single-load`/`single-store`
accessors** are real methods (they have `R_PPC_REL24` relocations) that need their owning class
identified from a caller first - that is research, not a carve, and it is where the next volume
is. `tools/mine_carve.py` ranks the candidates by joining `symbols.txt` x `report.json` x the port
gap list.

**A `Matching` unit is not evidence about the header it reads.** `CAnimData` was **0x78 bytes
wrong** (`sizeof` is 0x5B8, not 0x578) and `CModelDataModelSlots` stayed `Matching` at 100.00%
throughout, because it reads those members through a *local duplicate shape* and is therefore
layout-immune. `CHECK_SIZEOF` cannot catch it either - `check_sizeof<T,n>` passes for any `n`. The
measurement that works is retail's own `li r3,1464` before `operator new`, and the next class to
audit that way is **`CCharacterInfo`, which is 0x38 short** (retail's `CAnimData` member at 0x0C is
0xF8; this tree has 0xC0).

## If you are picking this up (2026-09-25, end of session)

Read this section, then the rest of this file, then `docs/RUNNING_THE_DECOMP.md`. Everything below is
measured; `python3 tools/gate.sh` is the single command that tells you whether the tree is sound, and
`python3 tools/check_docs_claims.py` tells you whether these files are still true.

**What landed today** (each with the gates run and a per-function report diff, and all of it in the
history with the reasoning):

- **`CGameGlobalObjects`'s constructor was integrated, measured, and kept out** (2026-09-26, lane
  `frame`). With it, `CGameState`'s whole default-construction chain and the boot call listed,
  `tools/link_check.sh` goes **325 -> 338** and `tools/boot_probe.sh` gets **past `gpGameState`
  to step 17** (`CGameArchitectureSupport`'s constructor: `CConsoleOutputWindow`,
  `CErrorOutputWindow`, `CMain::ResetGameState`). Three of the thirteen are lanes `v1`/`v4`'s in
  flight; ten are not written, and only three of those run at boot. The integration is
  `docs/research/patches/cgameglobalobjects_integration.patch`; the list is in
  `docs/research/cgameglobalobjects_ctor.md`. On the way: nine new units (five `Matching`:
  `fn_8016C230`, `fn_801F0A44`, and `SGameStateBlock`'s construct/clear/fill), `CCharacterFactoryBuilder`
  at 8/10 functions, five `CGameState` units that became net zero and are now listed, and a
  `SGameStateMemcardFill.cpp` regression (98.30%, and a host segfault) that a header change had caused.
- **`CGameState`'s five remaining constructor callees are written** (2026-09-26, lane `cal3`).
  Four are **`Matching` at 100.00%**, `flip_test.sh` PASS: `fn_80009898` (0x80009898, 52 bytes -
  **the next thing the boot waits on**, and the unit that lets `CGameStateCtor.cpp`'s own
  `fn_80009DBC` chain close), `fn_80142CF8`, `fn_80142DD4` and `fn_801440C0`. `fn_800098CC`, the
  other half of the `fn_80009898` pair, is written and **`NonMatching` at 99.55%** - seven
  register-allocation instructions in a loop retail's own bytes leave without a body, and the file
  records the ~30 spellings that did not move them. The fifth, `fn_8000934C`
  (`rstl::rc_ptr<CPlayerState>::ReleaseData`, 80 bytes), **is written and still is not registered**,
  but the reason changed on 2026-09-26 (lane `v4`): it is **not** a `dtk` link-order cycle - that
  claim was wrong. The 24-function re-split of `main.cpp` it needs **builds and links cleanly** and
  puts `fn_8000934C` in a `Matching` unit at 100.00% with the DOL sha1 and all 86 RELs unchanged.
  It is declined on the price: the two carve-out units would have no source, so **`matched` 3195 ->
  3187 against `linked` 1811 -> 1812**, four of the eight lost functions having been at 100.00%
  inside `main.cpp`; and `tools/link_check.sh` goes **326 -> 327** with the file listed, closing
  nothing. `RUNNING_THE_DECOMP.md`'s attempted-modules table has all three rows with the
  measurements, and its `fn_8000934C` row is marked superseded. **Two new spelling rules**, both worth more than the functions:
  a retail **`.sdata` global's address needs a NON-`const` declaration** (`const` moves the object
  to the read-only small-data area and mwcceppc emits `lis`+`addi` with `R_PPC_ADDR16_HA`/`LO`
  where retail has one `R_PPC_EMB_SDA21` - 100% -> 88.44%; the mirror image of the missing-`const`
  reload rule, same root cause), and **an array element's address strength-reduces the other way
  round if the array base goes into its own local first** (83.05% -> 100.00%, same object size).
- **Both frame-0 constructors on the port's critical path now have bodies** (2026-09-26, lane
  `pool`). `fn_802FB154` = `CResFactory::CResFactory()` (0x802FB154, 168 bytes) is **93.86%** and
  `fn_80301008` = `CSimplePool::CSimplePool(IFactory&)` (0x80301008, 336 bytes) is **94.32%**; both
  `NonMatching`, both in units that `unit_fit.sh` reports as **fitting exactly with no extra
  functions**, so each is one instruction placement from a `flip_test`. **A mis-attribution three
  sessions had recorded as settled is fixed**: `src/Kyoto/CResFactoryCtor.cpp` was the port-only
  home of `fn_803096C4` and called *it* `CResFactory::CResFactory()`, but `fn_802FB154` is the real
  one; `fn_803096C4` is the constructor of the four bytes at `CGameGlobalObjects`+0x00 and is now
  `src/MetroidPrime/CGameGlobalObjectsPad0Ctor.cpp`, under its own retail name. Three new spelling
  rules, in `RUNNING_THE_DECOMP.md`'s table with the measurements:
  **(a) mwcceppc picks small-data addressing from the declared _size_ of a global**, so a retail
  `.data` operand compiles to `lwz rX,0(r13)` / `R_PPC_EMB_SDA21` unless the symbol is declared as a
  **sized array** - `extern "C" char lbl_803B19B8[0x20];` gets retail's `lis`+`addi`. This is the
  same root cause as the non-`const` rule above, from the other end, and it applies to every
  constructor that stores a vtable. **(b) A constructor's dead `mr r3,r31` is the return-value copy
  and it is load-bearing**: retail's `CResFactory` epilogue has none, MWCC puts the copy in the
  *middle* of the body, and "no `mr r3,r31` before the `blr`, therefore the function returns void" is
  the wrong inference - 91.36% declared `void` against 93.86% declared `CResFactory*`. **(c) The two
  vtable stores in a constructor must be data operands, not a derived class**, or the object emits
  `__vt__8IFactory`, `__vt__11CResFactory` and a weak `__dt__8IFactoryFv` into three unclaimed
  ranges - the same wall `docs/research/paks.md` records for `~CResFactory`. **Port: both units are
  excluded from `files.cmake` with the measurement** (net +4 and net +8 on `link_check.sh`), so the
  port's `CResFactory::CResFactory()` is now the *default* one - still a win, because the old body
  called `CARDInit` on a `CResFactory`.
- **`fn_80009AC0` is written - the last unwritten body in the `CGameState` default-constructor
  chain** (2026-09-26, lane `v4`). 0x80009AC0..0x80009BF0, 304 bytes, in
  `src/MetroidPrime/Player/SGameStateMemcardBufFill.cpp`; it is `fn_80009898`'s first call and it fills
  `SGameStateMemcard`+0x00..+0x4F with 76 copies of `lbl_80417D92`, the **third** of the four one-byte
  `.sdata` objects at 0x80417D90..0x80417D93 that this struct's four fills draw from. The range needed
  no re-split - it is the one contiguous unclaimed range in that block - and `unit_fit.sh` reports
  `claimed 304, ours 304, retail 304, no extra functions`. **`NonMatching` at 85.20%**, and the finding
  is worth more than the 15%: **all 27 differing instructions are the nine byte stores, and the fold is
  mwcceppc's loop unroller rather than anything about the source.** Retail emits
  `add r5,r3,r0 ; stb r6,4(r5)` (the +4 in the store's displacement) and mwcceppc emits
  `addi r0,r5,4 ; stbx r6,r3,r0` (the +4 folded into the index). `tools/probe_unroll_store_form.cpp`
  isolates it: **the identical two statements with no loop emit retail's form**, and a **4-trip** loop
  of the same body - which mwcceppc unrolls completely, with no remainder - also emits it, while trip
  counts 12, 16, 24, 64, 68, 72, 76, 77 and 100 all fold. 73 spellings were measured
  (`tools/variants_fn_80009AC0.py` keeps the eight worth recording) and **not one changed the store
  form** (65 body variants through `tools/try_batch.py` plus 58 more loop bodies in a probe,
  123 in all); `-O2`, `-O3` and `-O4` do not rescue it either. Retail has the 8-wide unroll *and* the
  unfolded store, and that is the one combination this compiler does not produce. The unit is registered
  `NonMatching` so objdiff measures the 85.20% rather than the function reading as not started; a
  `NonMatching` object is not in the link, so the claim is free. **Corrected here too:** the
  `SGameStateMemcard` comment in `CGameState.hpp` and `CGameStateMemcardCtor.cpp`'s header both say
  `fn_800098CC` stores 72 at +0x50; it stores **76** - 72 is only the unrolled main loop and the
  remainder loop at 0x800099BC adds 4, which is what makes it fill `u8[76]` exactly.
- **`CMainFlow` is a state machine now** (2026-09-26, lane `k2`). `SetGameState` (0x8001DB54, 788
  bytes) and `AdvanceGameState` (0x8001DE68, 224 bytes) are 3/3 at 100.00% in
  `MetroidPrime/CMainFlowDtor`, `flip_test` PASS, DOL sha1 unchanged, and both are renamed in
  `config/G2ME01/symbols.txt`. **They are in the destructor's unit and that was forced**: a
  `Matching` unit has to carry its own switch jumptable, mwcceppc puts `.data` in an 8-byte-aligned
  section, and `jumptable_803B178C` is 4-byte aligned - so the only range that can hold it starts
  with the vtable, which belongs to the key function's unit. Read "A switch jumptable forces the
  unit to own the vtable next to it" in `RUNNING_THE_DECOMP.md` before writing the next one; it
  also carries four spelling rules and the `addis`/`cmplwi` constant decomposition that two of
  these constants needed. `CMainFlow::OnMessage` is still the hole, and it is still the only one
  between the port and a frame.
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

0. **`CGameGlobalObjects::AddPaksAndFactories` - boot-path step 13, and the gate on most of what
   is left.** 1,936 bytes, 23.17% written, and **864 of them (44.6%) are 36 factory
   registrations** that cannot be written until the factories exist (measured: writing them today
   is a **34-symbol regression**). A lane just measured that **all 40 gun/player symbols are
   gameplay-only purely because they sit downstream of this step** - `CPlayer::CPlayer` needs a
   loaded world. So this one function is worth more than its size suggests, and it is named by the
   objective. `docs/research/paks.md` has the FourCC table; `CFactoryMgr` is already real and all
   36 registrations are expressible, so the work is the 33 factory classes.
   **And the 40 gun symbols should NOT be worked next**, which is the negative result that
   reorders the list: **0 of 69 relocation sites are inside a static initialiser**, so none of
   them is touched before `main`. See `docs/research/gun_boot_path.md`.
0b. **The three frame-0 vtables** - `vtable for CMainFlow`, `vtable for CIOWin`,
   `vtable for CResFactory`, all for classes the port constructs during initialisation. **The
   obvious fix is a net loss**: defining only the destructor emits the vtable and leaves three
   members still undefined (+2 worse per class, measured), and `CResFactory` does not even emit
   one because its key function is `Build`. `CIOWin::Draw` *draws* and `CResFactory::Build`
   *builds the frame*, so a stub is unsound here. **This is decompilation work, not a port
   workaround** - the unblocking action is named in `docs/research/boot_probe.md`.
1. **The port's link gap: 309 symbols, measured** (was 732) - and the number to plan against is
   not the total but its shape: **289 other game methods, 159 REL module loaders, 39 unmangled
   `fn_*`/`lbl_*`/globals, 1 `TypesMatch` body.** The other three groups this list used to name -
   12 static data members, 8 `TypesMatch` bodies, 6 `rstl` templates - **are closed.**
   `docs/research/port_link_gap.md` has the method and the corrections that produced it: an earlier
   version of the tool said 63, because it filed every mangled game symbol under "C++ runtime", and
   the 20x was invisible while the tool and its document agreed with each other.
   `port_link_gap_list.md` is the generated list, and `check_docs_claims.py` fails if the table and
   the list disagree - a table row has gone missing twice in three collections, so **read the
   generated list, not the table.**
   **Four of my own "bulk work, one generator" claims have now been measured and all four were
   wrong** - the correction matters more than the count, because each would have sent a lane looking
   for a generator that does not exist:
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
   - **And the 85 unwritable real loaders are blocked on a `symbols.txt` spelling, not on the
     classes** (measured 2026-09-26, and it reversed two sessions of planning). It was recorded
     that a loader is unwritable until "the constructor's address lies in a range a unit with
     source claims" - **false.** The constructor's bytes come from dtk's object whether or not this
     tree has written them. `LoadRelay` and `LoadTimeKeyframe` were both landed **with no ctor unit
     and no vtable claim**, the first `Matching` at 100%. What each of the other 84 needs is: the
     loader's name in `symbols.txt` **as MWCC spells it**
     (`LoadRelay__FR13CStateManagerR12CInputStreamRC11CEntityInfo`, *not* the GCC
     `_Z9LoadRelayR13CStateManagerR12CInputStreamRK11CEntityInfo` the port gap list uses - using the
     wrong one costs a link), the constructor's name in the same spelling, and the body. So:
     **75 class headers of about 20 minutes each, then up to 75 loaders**, and the 8-loader
     `CScriptSpecialFunction` cluster is not the cheapest way to any of them.
     `docs/research/missing_classes.md` has the 76-vtable table, the recipe, and four measured
     facts that make the rest mechanical - **`r2` is 0x804223C0**; **`addi` sign-extends**, so
     `lis -32709 ; addi -15000` is 0x803AC568 and *not* the 256-byte identity table at
     0x803BC568 that reads convincingly like a bug; **a default float must be retail's 8-byte
     symbol**, because no instruction in the DOL reaches its second word; and **all 201 generated
     `SLdr*` headers declare a ctor and dtor retail's loader never calls.**
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
5. **Two one-line header defects that are each worth more than a week of function-writing**, both
   measured, and the first of which is now **fixed** (2026-09-26, lane j4):
   - ~~**`include/MetroidPrime/CGameGlobalObjects.hpp`'s `char pad0[4]` is spurious.**~~ **It is
     not, and deleting it is a measured regression.** Three sessions in a row read only the
     *order* of `CGameGlobalObjects::CGameGlobalObjects`'s calls; order says what is built first,
     not where the object begins. The instruction that settles it is a **store to a global** in
     the same function: `addi r0,r31,4` / `stw r0,-28380(r13)` at 0x80008528/0x80008534 stores
     **`gpResourceFactory = this+0x04`**, and `gpResourceFactory` is the `CResFactory*` (the 36
     registrations address `CFactoryMgr` as `gpResourceFactory`+0x74, and `fn_802FB154` builds
     `+0x04` and `+0x74` in itself). The four bytes are a real member with a constructor
     (`fn_803096C4`, 0x800084A0) and a destructor (`fn_80309660`, 0x800065F0). **Deleting it
     moved no unit and made two functions worse** — `PostInitialize` 99.88 -> 98.37 and the
     `CGameGlobalObjects` ctor 28.35 -> 22.11, both in the `NonMatching` unit
     `main/MetroidPrime/main` — because the fourth argument to `AllocateRenderer` becomes
     `mr r6,r29` where retail has `addi r6,r29,4`.
   - **What *is* defective, and is now fixed: `CResFactory` is 0xE0, not 0xE4.** That is what made
     **every offset measured from `CGameGlobalObjects` read 4 too high**, the symptom both earlier
     passes described and mis-attributed to the pad. `CSimplePool` compiled to `this+0xE8` against
     retail's `this+0xE4`. `CHECK_SIZEOF(CResFactory, 0xe0)` and `uchar xac_[0x34]` are landed;
     `PostInitialize` is 99.88 -> **99.91** and **no unit moved**. `CResLoader` is 0x70 bytes
     (four 0x18 lists plus four words) and `CResFactory` is 0xE0; `CResLoader` and `CFactoryMgr`
     are members *inside* `CResFactory`, at +0x04 and +0x74. Two traps to carry forward:
     **`CHECK_SIZEOF` never measures a size, it only checks a model against itself** — it passed
     for `0xe4` and would pass for `0xd0`; and **a constructor's call order is not a base offset.**
     The full layout, the evidence, and both movement reports are in `paks.md`'s third correction.
   - **`include/rstl/rmemory_allocator.hpp` does not inline `CMemory::Alloc` with a `CCallStack`**
     as retail does, and its `allocate` is out of line and uses `rs_new`. This is what blocks
     **`CPakFile`, all 33 functions** - `reserve<rstl::vector<CPakFile::SResInfo>>` is at 33.84%
     and `RebuildResourceLists` at 39.63% for this reason, not for want of effort. A lane owning
     that header unblocks the whole resource system, and it is the pak chain's last wall.
6. ~~**A policy on raw-offset code.**~~ **Decided and measured, 2026-09-25**:
   `docs/research/raw_offsets.md` sorts every site into three kinds and rules on each - an opaque
   receiver (`const void* self + 0x44f`) is retail's own shape and stays; an unmodelled member of a
   modelled class is debt with a named blocker; a whole class written as raw offsets is not
   acceptable. `tools/check_raw_offsets.py` measures it (45 sites, 14 files - the review's 26 in
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
- The link then failed on **342 undefined symbols and 4 duplicate definitions**, and that is a real
  `ld.bfd` measurement, not an estimate. *(342 is the figure at the time of writing; lane `h1` closed 14
  of them on 2026-09-26 and the current measurement is 328 - see `docs/research/audio_stack.md`.)*
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

So the port does **not** boot yet, and the honest statement of why is now short: **315 undefined
symbols and nothing else structural** - one fewer than the 332 above, because
`REL_loader_CannonBall` stopped being missing (see the `REL` loader fix below: it was declared
`extern` with no initialiser, so the linker put it in `.text` and the port faulted *writing* it) - and it went **up** 12 this wave, which is the
point worth understanding: a decompiled body *names* the retail callees it reaches, so
finishing a function turns an unnamed gap into several named ones. A named hole is
cheaper than an unnamed one, and the wave landed 15 functions at 100% to pay for it.

But that is no longer the whole story, because the port
**does link and does open a window** when linked with `--warn-unresolved-symbols` and run under
`Xvfb` with Mesa's software Vulkan: `Using framebuffer size 854x480 scale 1`, then it asks for
the disc.

**And the boot path's requirements are now measured *in order*, by running it.** The first
three symbols the port asks for are **all `CCallStack`** - `CCallStack::CCallStack(uint,
char const*, char const*)`, `GetFileAndLineText()`, `GetTypeText()` - because
`CMemory::Alloc` and `CGameAllocator::Alloc` both take a `const CCallStack&`, so every
allocation in the game constructs one. Nothing else is reachable before that. It then
faults at `CGameAllocator.cpp:587`, `iter = iter->GetNext()` in `DumpAllocations`, which is
the allocator's **failure** path: `Alloc` failed and the diagnostic walk dereferences a
null iterator. **So the second requirement was a port bug, not a missing symbol** - and
chasing it found and **fixed** the port's real first failure. **It is now root-caused:
`kAllocatorPointerBits` was `sizeof(void*) * 8`, so the allocator's pointer flag mask was
`0x3F` on a 64-bit host where retail's is `0x1F`.** The sixth bit, `0x20`, is *address* under
a `0x40` block stride, so `GetNext()`'s `x14_next & ~mask` truncated every block pointer by
32 bytes, the free-list walk left the block list and read payload as headers, and
`FindFreeBlock` rejected a good block on a length read out of what was really a code address.
`include/Kyoto/Alloc/AllocatorCommon.hpp` now fixes the count at retail's value - the flag
width is a property of **retail's allocator protocol**, not of the host's pointer size, and
decoupling the two is what makes the fix hold on any host rather than the one being debugged.
`Alloc(135168)` now succeeds, `DumpAllocations` is no longer reached, and the probe walks past
the allocator to the next two requirements. The flag count is a **no-op for the decomp build**
(MWCC pointers are 32-bit), verified rather than assumed: `GATE PASS`, `matched 3186 -> 3186`,
`linked 1802 -> 1802`, DOL sha1 `6ef9b491...`, 86/86 RELs.
**Two defects in the same structure remain open, with evidence, in
`docs/research/allocator_flag_mask.md`.** The first is a **second, independent** bug: the one
free block's header is not fully initialised. Every allocated block satisfies
`(next_header - this_header - 64) == x4_len`; the free block reports **180,220** bytes and must
cover **24,337,920**, with a smashed prior guard and a dirty high half of `0x1` - the signature of
a 32-bit store into a 64-bit field. It was healthy earlier in the boot (`len=24428192`, guards
intact), so it is written and then overwritten. **Narrowing `x4_len` to `uint` was tried twice and
reverted both times** - once changing the ctor and `SetLength` too, once changing **only** the
member - and both cost `FindFreeBlock` its 100% match. The minimal attempt is the informative one,
and it revises the obvious reading: **the mixed 32/64-bit compare in `FindFreeBlock` is retail's own
shape, not a host artifact.** Retail's `x4_len` is a 4-byte word and compares in 32 bits; this
tree's `size_t` is what reproduces those bytes. **The two builds want different widths for the same
declaration, so no single member type satisfies both** - which is why this is a port-side problem,
not a header edit. The second is `x10_last` not being stride-aligned, a plain value fix that is a
no-op on retail. The committed `PortReachStubs.cpp` had also drifted from HEAD's sources; the
reachable set is regenerated and moved 318 -> 332.
Method and both of its own tooling bugs are in `docs/research/boot_probe.md`.

**The port's own "eight functions with no body" diagnostic was stale; it is three.** Step 17's
wall is `CGameArchitectureSupport`'s constructor, and `src/MetroidPrime/PortBoot.cpp` printed that
eight of the functions it calls have no body. Re-measured: **five now have `Matching` units** -
`CAudioSys` (four siblings), `CInputGenerator` (`CInputGeneratorCtor.cpp`), `CIOWinManager`
(`CIOWinManagerCtor.cpp`), `CMainFlow` (`CMainFlowCtor.cpp`) and **`CGameOptions::EnsureOptions`**,
which is written at `CGameOptions.cpp:207` and reproduces retail's `0x801612C4..0x801613D0`: sixteen
setter calls, `li r5,1` each, the last six extracting one bit apiece from the flags byte at `+0x24`
with `rlwinm r4,r0,N,31,31` at bit numbers **25, 26, 27, 29, 28, 30** - not sequential, and bit 24
is the one flag in that byte the function never touches. The remaining three are
`CConsoleOutputWindow`, `CErrorOutputWindow` and `CMain::ResetGameState`, all three confirmed in
`port_link_gap_list.md`. The port now says three, with the evidence in the comment.

**A caution worth carrying, because it cost this session an attempt.** Those five were "found
missing" by grepping for `CGameOptions::SetHudAlpha` and similar across `src/` and finding no
*definition* - but the matches were **call sites inside `ResetToDefaults` and its siblings**, and
the definitions sit in the same file further down. Writing an `EnsureOptions` on the strength of
that grep produced `object 'CGameOptions::EnsureOptions()' redefined` and the file was restored.
**A grep that finds a name proves the name appears, not that the function is absent.** The
discriminator is whether the *definition* exists, which for a member function is a line beginning
with `void CGameOptions::` and a body, not any mention.

**All 15 modules now call REAL entry points, and that closes the flat-link blocker two previous
attempts had failed on.** Twelve module TUs are in `files.cmake` and
`platform/compiled_modules.cpp` calls `mp_relmain_*`/`mp_relexit_*` for them. Measured: **86/86 RELs
byte-identical, DOL sha1 `6ef9b491…`, `GATE PASS`, `matched 3186 -> 3186`, `linked 1802 -> 1802`,
and 331 -> 326 undefined with 0 duplicate definitions.**

The constraint that makes it work is one line in `configure.py`: **ten of the twelve edited units
are `Matching`, so mwcceppc's output for them must reproduce retail's bytes exactly, and every
change has to live in the `#else` branch of an `#ifdef __MWERKS__`.** `CScriptSkyRipple` and
`CScriptCannonBall` are `NonMatching` - and they, with `Tweaks`, are the only module TUs that were
in `files.cmake` before this change. That is not a coincidence, and it is the fact the first two
attempts missed while treating `CScriptCannonBall` as the working control: it is `NonMatching`, so
its object is not in the module link and nothing it does to itself can move a module hash.

Attempt 1 (all 17 files) moved 8 module hashes with every object byte-identical. Attempt 2 (this
session, 11 files) hit `mwldeppc`'s `internal linker error: 'ELF_linker.c' Line: 5083` on
`ScriptRsfAudio` and `ScriptPlayerProxy` - the two units that were `Matching` *and* had their
loader changed from `extern` to a definition. **The fix for a `Matching` unit is therefore a shape,
not a value**: keep `extern FScriptLoader lbl_65_bss_0;` under `__MWERKS__` and define it only in
the `#else` branch, because the host build has no dtk split object to supply it. Full account, with
the one hypothesis that was **disproved by measurement** rather than argued down, is in
`docs/research/rel_rename_hazard.md`.

Three things that are not collisions of `RELMain`/`RELExit` and cost real time: `SetFuncPtrs()` is
defined by both `CScriptRiftPortal` and `ScriptGuiSetup`; `__ct__10CModelDataFv` by both
`CScriptSkyRipple` and `CScriptScriptStreamedMovie`, **which was already in the host build and so
could not be renamed**; and `CScriptPlayerActorMain` must define **no** exit symbol, because
retail's module has a prolog and no epilog and defining one collides with `CScriptPlayerActor.o`'s
real `RELExit` - `port::modules::ShutdownAll` already skips a null shutdown.

**The port now gets through all 15 module initialisations.** `port::modules::InitAll` runs the
registry in `platform/compiled_modules.cpp`; the probe previously died at the **third** entry,
`CScriptCannonBall`, on a *write* to `&REL_loader_CannonBall`. The cause was a declaration, not a
missing body: `extern FScriptLoader REL_loader_CannonBall;` with no initialiser is a tentative
definition, the linker bound it as a `FUNC` and placed it in **`.text`** (measured:
`FUNC GLOBAL DEFAULT .text`), and `.text` is read-only. Sibling modules that work use a real
definition - `REL_loader_Metaree = nullptr` in `.bss`, `REL_loader_Tweaks` as
`OBJECT GLOBAL DEFAULT .bss`. With `= nullptr` the symbol is no longer missing, the link gap went
**332 -> 331** with a real body rather than a stub, and the probe's ordered stub log runs to 38
entries instead of 13. **The other two tentative definitions of the same shape are
`lbl_62_bss_0` in `CScriptPlayerProxy.cpp:7` and `lbl_65_bss_0` in `CScriptRsfAudio.cpp:5`, plus
`REL_loader_SkyRipple` at `CScriptSkyRipple.cpp:31` inside an `extern "C"` block - all three are
fixed in the module lanes in flight.** The general rule: a global that is written must be a
*definition with an initialiser*, and `readelf -sW` is the check - a data symbol showing as `FUNC`
in `.text` is this bug.

**`CGameGlobalObjects`'s constructor was written at 100.00% Matching and then REVERTED, with the
measurement kept.** A lane produced `src/MetroidPrime/CGameGlobalObjectsCtor.cpp` claiming
`.text:0x8000848C-0x80008570` (0xE4) at 100.00%, `flip_test.sh` PASS, GATE PASS, and it required
a three-way re-split of `main.cpp`'s claim plus a new `mainTail.cpp`. **It was reverted anyway, for
two measured reasons and not for taste** — and a second pass then established that the second reason was wrong (`~CGameArchitectureSupport` is back at 95.27%; the weak copies were never the cause) while the **first** reason held under every attempt: listing it takes the port's gap 325 -> 333 and no amount of work in that lane could get it to neutral. The full accounting, the per-symbol cost of all eight, and the three-way split mechanics are in `docs/research/cgameglobalobjects_ctor.md`, with the working patch preserved at `docs/research/patches/cgameglobalobjects_ctor.patch`.

1. **Listing it makes the port's link gap worse, not better** - `tools/link_check.sh` goes
   **325 -> 333** with it listed and closes nothing, because nothing in the port calls it yet. The
   project counts `linked` because the port needs it; +1 linked that the port cannot use is not
   progress against the objective.
2. ~~**It cost a function.** `__dt__24CGameArchitectureSupportFv` went **95.27% -> 0.00%**.~~ **Superseded 2026-09-26:** a second pass put the destructor back in `main.cpp` and got it to **95.27%** again, and showed the 476 bytes of weak member-destructor copies were never the cause - `flip_test` passes with them in the object. This reason does not hold; the first one does.

The cost of doing it later, so nobody re-derives it: a DOL unit may **not** claim two ranges in one
section (dtk fails with a link-order cycle), so `main.cpp` must be cut three ways and **`.ctors` and
`.sbss` must move to the last unit** or dtk reports "Mismatched splits for .ctors". Roughly 200
lines move out of `main.cpp`. `fn_802FB154` must be renamed to `__ct__11CResFactoryFv` and
`fn_80301008` to `__ct__11CSimplePoolFR8IFactory`; `CGameGlobalObjects.hpp` needs two members
(+0x108, +0x150) and `CInGameTweakManager` needs its measured 0x10 size. And declaring
`~CGameGlobalObjects()` does **not** suppress the weak member-destructor copies.

**The finding that matters more than the unit, and it is a port bug:** `PortBoot.cpp` says the
constructor "initialises `simplePool` from an uninitialised `resFactory`", and that was true only
of the stub. Retail constructs `resFactory` first (`bl fn_802FB154` at 0x800084A8) and only then
calls `fn_80301008(this+0xE4, this+0x04)`. On the port it needs **`fn_802FB154`, which is
`CResFactory::CResFactory()` and which `src/Kyoto/CResFactoryCtor.cpp` currently provides for the
*wrong* function** - that file implements `fn_803096C4` - **and `fn_80301008`, which is
`CSimplePool::CSimplePool(IFactory&)` and has no body anywhere in the tree.** That
mis-attribution is the real port-side defect on this path, and it is where the next lane should
start.

**A G2ME01 image is on this machine** at
`/run/media/odran/Leo/Portable/roms/gc/Metroid Prime 2 - Echoes.iso` - the same input the REL
module table needs. `tools/boot_probe.sh` runs it unattended; its ceiling and why the crash it
reports is its own artefact are in `docs/research/boot_probe.md`. — the module-loading half of the old answer is fixed.
`tools/link_check.sh` measures that number against a recorded baseline, and
`tools/check_docs_claims.py` now fails if this paragraph and the linker disagree, because it is the
number every lane plans against and it has moved twenty-five times (732 → 727 → 724 → 562 → 557 → 548 → 544 → 543 → 533 → 532 → 528 → 527 → **525**;
the last step is `CResLoader::GetPakCount` and `GetPakFile` leaving the gap in one lane - two
symbols from a header fix, not from twenty-four of decompilation → 523 → **342 → 340 → 337 → 333 → 319 → 318 → 331**).
**The last step is the wrong direction and is worth reading as the rule, not as a regression:**
`CMainFlow::SetGameState` and `AdvanceGameState` becoming `Matching` put thirteen new retail
callees on the link - the six window constructors, the message factory `fn_80048EA4`, the four
game-state helpers, `StreamNewGameState`, and `__dt__24IArchitectureMessageParmFv`. A symbol goes
from *not referenced* to *referenced and missing* the moment the caller that needs it is written,
so a decompilation session can move this number up. What it must never do is leave a *name* out:
all thirteen are identified, and `docs/research/port_link_gap.md` says which of them close how.

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

23 files carry what a later session would otherwise have to re-derive, and each answers one
question that used to cost a session. (Digits above twenty on purpose:
`check_docs_claims.py` matches `(\w+) files carry` and has no spelled-out word past
twenty, so a hyphenated "twenty-two" or a spelled "Twenty-Two" both fail the check.)
question that used to cost a session:

| file | the question it answers |
| --- | --- |
| `docs/research/boot_globals.md` | **the two globals `CGameArchitectureSupport`'s constructor dereferences with no null test, who writes each, and the correction that `gpGameState` does *not* need `StreamNewGameState` or the paks - it needs boot step 7.** Also what the port now does instead of faulting |
| `docs/research/boot_path.md` | **the measured, step-by-step map from this tree to a rendered frame** — 25 steps, each with its retail address, size, current state and what it blocks. Read this before planning any port work |
| `docs/research/tweak_globals.md` | **all 1,452 bytes of `REL_CreateTweakGlobals`, store by store** — and the finding that `gpTweakPlayerA` ends up pointing at a 4-byte heap cell and *not* at a `CTweakPlayer`, so this function is not what unblocks the frame loop. **Its size-drift table is superseded** — see the next row |
| `docs/research/tweak_player.md` | **the 4-byte cell, retail's five `CTweakPlayer` thunks (address, size, the offset each reads), and the correction that `CTweakContents` is 0x3244 and not 0x37D0** — a 64-bit host probe, not a modelling gap. Also the reusable rule: never measure a layout with a host compiler |
| `docs/research/port_link_attempt.md` |
| `docs/research/gun_boot_path.md` | **all 40 gun/player symbols are gameplay-only** - 0 of 69 relocation sites sit in a static initialiser - and all 40 wait on boot step 13. The negative result that reorders the next-step list. Also: `link_fn_reach.py` measures what `link_reach.py` only asserted, and `CStateManager::ObjectById`'s model is wrong |
| `docs/research/boot_probe.md` | **links with `--warn-unresolved-symbols` to get a binary and crash it deliberately**: the linker's own list confirms `link_reach.py`'s 342 to the symbol, and names the 7 deepest boot-path dependencies with file and line. Three of them are `vtable for CMainFlow`/`CIOWin`/`CResFactory` - frame 0, and not retail's bytes. The probe never **ran**: a third-party `libnod.a` `crc32` problem stops the link |
| `docs/research/port_link_stubs.md` | **181 of the port's 523 undefined symbols were provably not on the boot path** and are now stubbed: 523 -> 342. `tools/link_reach.py` walks object reachability from the entry **and every static initialiser**, and the 342 that remain are exactly the ones it predicted must be real. Also exposes that `g_LoaderFuncs` is dead - the script loader table is never handed to the script system |
| `docs/research/rel_rename_hazard.md` | **why 16 REL modules stay out of the port build**: a host-only `#ifdef __MWERKS__` rename of their `RELMain`/`RELExit` leaves every object byte-identical and the DOL hash intact, and still changes 8 of 86 module hashes. Ten experiments, two of which were wrong |
| `docs/research/allocator_flag_mask.md` | **the port's first real crash, root-caused and fixed: `kAllocatorPointerBits` was `sizeof(void*) * 8`, so the allocator's flag mask was `0x3F` on a 64-bit host where retail's is `0x1F`, and the sixth bit is address under a `0x40` stride - so `GetNext()` truncated every block pointer by 32 bytes and the free-list walk read payload as headers.** The gdb trace that eliminated six hypotheses first, the reason the fix is protocol- rather than host-derived, the two defects left open, and the two header fixes that were tried and **reverted** for costing a Matching function its 100% |
| `docs/research/rc_ptr.md` | **retail's `rstl::rc_ptr` is 8 bytes**, seven independent lines of evidence, against this tree's 4 - and the change unblocked 1,084 of the frame loop's 2,584 bytes. Also carries the correction that the `operator new` literal is **not** a global blocker |
| `docs/research/rstl_string_member_op.md` | **done**: `basic_string`'s member `operator+(const char*)` is a `Matching` unit at 100%, claiming 0x80021634. Carries the shape measurement - the `C` is the const marker and sits after the template-id's `>`, so retail's is the non-const member - and the correction that the blast radius is **2 call sites, not the tree** |
| `docs/research/audio_stack.md` | **only 11 of the port's 29 audio symbols are reached before a first frame**, and they come from two objects (`main.cpp`, `CGameOptions.cpp`) and two call sites (`CGameArchitectureSupport`'s constructor, `CGameOptions::EnsureOptions`) - so `link_reach.py`'s "reachable" is a whole-object upper bound and the audio stack is a much smaller hole than it looks. Also the route split (12 `Matching`, 14 port-side, 3 neither) and three MWCC traps: an eight-byte alignment rule on `.sdata` claims, `clrlwi` coming from a source conversion rather than the callee's prototype, and `cmplwi` vs `cmpwi` |
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
| `docs/research/CPatterned_layout.txt` | the constructor's 2904 bytes, every byte in exactly one row |
| `tools/probe_sources.sh` | the port build's syntax sweep (636 files) || `build/binutils/powerpc-eabi-objdump`, `powerpc-eabi-nm` | disassemble / list symbols |
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

**1. The DOL** - 2846 of 16726 functions, ~14k left (that figure includes the SDK's 882, which are
essentially complete). Verified matches land here steadily, and the two units the whole port was
waiting on are in: `CAi` 11/11 and `CPatterned` 10/10, both `Matching`. Others:
`TypesMatch` 508/511, `CStateManager` 72/239, `CPlayerGun` 61/135, `CPlayerState` 69/72.
(Those three fell on 2026-09-26 when lane f1 made `rstl::rc_ptr` retail's 8-byte width - all
three are `NonMatching`, so none of them is in the binary and the DOL's sha1 did not move. See
`docs/research/rc_ptr.md`.) `CStateManager`, `CPlayerGun` and `CPlayerState` are back up to
72/239, 61/135 and 69/72 on 2026-09-26: `CStateManager`'s `pad2_2` was `0x34` where retail has
`0x2C`, which pushed every member from `x1684` up by 8 and put `m_isDarkWorld` at 0x2954 instead
of 0x294C. Fixing the one pad took 17 functions to 100% in a single edit and broke none; see
`RUNNING_THE_DECOMP.md`, "A header comment that recorded mwcceppc's own output".
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
reports **56 units of our own code in 38 modules** - `AIMannedTurret`, `AtomicBeta`, `DarkSamus`, `DigitalGuardian`, `EmperorIngStage1`, `EmperorIngStage2Tentacle`, `EyeBall`, `FlyerSwarm`, `Glowbug`, `GunTurret`, `IngSpiderballGuardian`, `Kralee`, `Krocuss`, `Metaree`, `OctapedeSegment`, `PuddleSpore`, `Puffer`, `Ripper`, `RubiksPuzzle`, `ScriptCoin`, `ScriptFrontEndDataNetwork`, `ScriptGui`, `ScriptPlayerActor`, `ScriptPlayerProxy`, `ScriptPlayerTurret`, `ScriptRiftPortal`, `ScriptRsfAudio`, `ScriptSafeZone`, `ScriptStreamedMovie`, `Shredder`, `SpankWeed`, `Sporb`, `StoneToad`, `SwarmBasics`, `Tweaks`, `WallCrawler`, `WallWalker`, `WispTentacle`.
`Puffer` joined by being promoted rather than restored: with
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

**Corrected again by lane h5, 2026-09-26, and this time measured rather than inferred.**
`CResLoader` is **0x70** bytes, not 0x60 - the four lists are 0x60 and four unnamed words follow
them - so `CFactoryMgr` is at `CResFactory`+**0x74** and is `CFactoryMgr`+**0x00** there, not
+0x10. `include/Kyoto/CResFactory.hpp` and `include/Kyoto/CResLoader.hpp` now carry those
measured numbers and the gate's per-function diff was unmoved by the change (3131 -> 3131
matched), which is the measurement that says nothing else in the tree read those members.

**Corrected a third time by lane j4, 2026-09-26, on the factory's own size and on where the
factory sits.** `CResFactory` is **0xE0**, not 0xE4 (h5's `0xE4`), and it is at
`CGameGlobalObjects`+**0x04**, not +0 — `gpResourceFactory` is stored as `this+0x04` at
0x80008528/0x80008534, and the header's `char pad0[4]` is therefore correct and stays.
`CHECK_SIZEOF(CResFactory, 0xd0)` is now `0xe0`. `main/MetroidPrime/main ::
PostInitialize__18CGameGlobalObjectsFR10COsContextR10CMemorySys` is 99.88 -> **99.91** and no
unit moved. `paks.md`'s third correction has the full layout and both movement reports.

**All 36 factory registrations are now written** - the 864 bytes of block 9, 44.6% of
`AddPaksAndFactories` - and `AddPaksAndFactories` is **57.15%**, up from 23.17%. The net on the
port's link gap is **0**, not the +34 a bare transcription costs, because
`include/Kyoto/CFactoryFunctions.hpp` declares all 36 with C linkage and typed parameters - so
the emitted symbol name is retail's spelling and **no `symbols.txt` rename is needed and no REL
module can be disturbed** - and `src/Kyoto/CFactoryFunctionsPort.cpp` gives the 33 that retail
leaves unnamed a body. `link_gap.py` 309 before and 309 after; `link_check.sh` 340 undefined
before and after. The block itself lands at about 72% of itself: the only two instructions per
entry that differ are `addi r3, r31, 116` (MWCC hoists the `+0x74` into a single `mr r3, r31`)
and the `bl`, which is a *named* `CFactoryMgr::RegisterFactoryByTypeIdx` where retail has
`fn_802F96E0`. `docs/research/paks.md` has the whole table plus the recipe, the per-factory
`operator new` sizes, and two negative results worth more than the 33 factories.

**`CPakFile` is blocked, and not on decompilation.** The constructor (0xEC) and the destructor
(0xF8) - the two the chain actually needs - were **already 100%**, as are 22 others. The two
blockers are `reserve<rstl::vector<CPakFile::SResInfo>>` at **33.84%**, which needs
`include/rstl/rmemory_allocator.hpp` changed (retail inlines `CMemory::Alloc` with a `CCallStack`;
this tree's `allocate` is out of line and uses `rs_new`), and `RebuildResourceLists` at **39.63%**,
which calls an unnamed `fn_80052220` where the port calls `reserve<rstl::vector<uint>>`. A pak
lane cannot fix either. **A lane that owns `include/rstl/` unblocks 33 functions here**, and the
payoff is the whole resource system.

**Update 2026-09-26 (lane `k4`): half of that is answered.** `rstl/rmemory_allocator.hpp`'s
`allocate` was *already* macro'd per translation unit (`RSTL_INLINE_RESERVE_HELPERS`), so
`reserve<rstl::vector<CPakFile::SResInfo>>` is at **99.74%** and the unit is at **90.49%**,
24 of 33. The other half, `RebuildResourceLists`, is now at **41.02%** and the missing piece is
identified: it is **`rstl::construct`'s spelling for an 11-byte element**, the same placement
`new` that `fn_802FC378` needed, and the same negative result - replacing it with an assignment
breaks five `Matching` units. `rstl/construct.hpp` now carries that warning at the definition.
So the `reserve` blocker is gone and what is left is one function plus the two percent.

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

## What the six collected lanes added, 2026-09-26 evening

All six are in, `GATE PASS`, and the port's stop message is **unchanged** - still step 17,
`CGameArchitectureSupport`'s constructor. Nothing in this batch moved the frontier, and the
reason is uniform: every one of them is `NonMatching` or not on the boot path.

| lane | what landed |
|---|---|
| `memctx` | **both open allocator defects closed.** The heap corruption was a placement-new asking for retail's `0x20` when `sizeof(CSmallAllocPool)` is `0x30` on a 64-bit host, so the ctor wrote 16 bytes over the *next* block's header. `Alloc(135168)` succeeds and the invariant holds for all six arena alignments |
| `cmain` | `~CGameArchitectureSupport` to 100.00%, ctor 90.78 -> 93.10%, `UpdateTicks` 91.47 -> 95.04%; two undefined-behaviour sites removed (`CheckReset` and `RsMain` were non-void with no `return`) |
| `chain` | two `Matching` units, and **`lbl_803A9208` now defined** - 0x1C8 bytes byte-identical to the DOL's `.rodata`. It was a guest data address the host needed as real strings, and the comment claiming `PortGlobals.cpp` defined it was wrong |
| `display` | 7 carve units + **the 82-slot `CCubeRenderer` vtable**, which answers `boot_path.md` step 21c: the per-frame call is `BeginScene` at slot 35, and it is **not a draw** - a third reason the first frame renders nothing |
| `kyinput` | 6 `CGraphics` carve units. `subf rA,rB,rC` computes **`rC - rB`**, and reading it backwards wrote a *behavioural* bug a percentage would have hidden |
| `ceil` | `CConsoleOutputWindowCtor` written to **98.17%**; the `lis 0x4330` / `lfd` question settled - it is mwcceppc's int-to-float conversion, the integer biased into the high word of a zero-mantissa double, **not 176.0f** |

**The stop message is a hard-coded `printf` in `src/MetroidPrime/PortBoot.cpp`, printed before
the call.** So "paste the stop message before and after" is not an observable acceptance
criterion for any decompilation work, and two lanes were told to use it. The frontier's real
instrument is `boot_path.md`'s step list plus `tools/link_reach.py`; the message only changes
when someone edits that `printf`. **Fixing the message to reflect the tree is a small, real
improvement and nobody has done it** - it still names `CMain::ResetGameState` as having no
body, and it has one at 98.61%.

## A latent port link bug that only the shipping configuration could see

`platform/sdk_stubs.cpp` is in `mp_platform` and calls `PortDebug::RequestReset()`. The only
definition was in `platform/debug_ui.cpp`, **which is not in `CMakeLists.txt`** - it is the debug
UI, and building it would drag imgui and the whole menu into the shipping binary. So the reference
had no definition.

**Nothing caught it, and the reason is the interesting part.** `MP_SDK_HEADERS_ONLY=ON` - the
configuration `tools/link_check.sh` and therefore `tools/gate.sh` link - uses a *different source
list*, and in that configuration the reference does not exist at all. The configuration the port
will actually ship in, `MP_SDK_HEADERS_ONLY=OFF`, has it. `tools/boot_probe.sh` is the only thing
that builds that configuration, so the failure appeared there and nowhere else: `port link gap`
passed, `port link dups` reported 0, and the symbol was not in the undefined count.

**The general check is one line: build the configuration you are going to ship, not only the one
your gate happens to use.** Two other failures this session had the identical shape - the
`#ifdef TARGET_PC` host definitions in the `CGraphics` carves, and a stale `PortReachStubs.cpp` -
and all three were invisible to every instrument except the one that builds the other
configuration.

Fixed by moving `RequestReset`, `ConsumeResetRequest` and the flag into
`platform/port_reset.cpp`, which *is* built, and declaring the flag `extern` in `port_debug.h` so
adding the debug UI later cannot produce a duplicate definition.

## The boot probe now heals its own link, and the stop message tells the truth

**`tools/boot_probe.sh` stubs what its own link asks for, then relinks, once.** Without it every
new `Matching` unit that drags in a callee breaks the probe invisibly - three landed that way on
2026-09-26, and the symptom was a probe that had not run since 18:15 while the gate stayed green.
The set is read from the probe's own build log rather than from `port_link_gap_list.md`, because
the gap list answers "which decompilation symbols have no body" and the linker asks a different
question. Only identifiers that are declarable as C are stubbed; anything else is reported and
left for a human, because the first version emitted `extern "C" void PortDebug::RequestReset()(void)`
and the stub file stopped compiling.

**A third `gen_link_stubs.py` mode was added and removed the same day.** It read the gap list, which
still lists symbols the port has since given real definitions, so it emitted a stub for one and
produced a duplicate definition of `lbl_80418AFF`. That is the "loose source that is not loose"
mistake a third time in this project, and the note saying so is in the tool.

**And the stop message was wrong.** It claimed "three of the functions it calls still have no body".
All three have bodies: `CMain::ResetGameState` at 98.61%, `CErrorOutputWindow` at 78.56%,
`CConsoleOutputWindow`'s constructor at 98.17%. It now says what is true, which is a materially
different thing to read: **the wall is matching quality, not missing code.** A missing body means
nobody has written it; a near-matching body means the algorithm is right and one register decision
is left. They call for completely different work.

## PROVEN: `CErrorOutputWindow` is blocked by the compiler *version*, and retail's own binary proves it

This is no longer a lane's opinion. It is measured, and the evidence is retail's own inconsistency.

**The claim, corrected.** The previous lane said *mwcceppc 2.7 masks every `!` applied to a
`bool`-typed operand.* That is **not the mechanism**. Probed with the unit's own flags:

- `!x` on an **`int`/`unsigned`** parameter emits bare `cntlzw r0,rX` - **no mask** - even with a
  call and a callee-saved copy of the parameter in the way. So the *shape* is reachable; only the
  *type* blocks it.
- `!x` on a **`bool`** masks in **every** shape tried: leaf, after a call, returned, stored to an
  `int`/`bool`/**`bool : 1`** field, through a pointer, as a `const bool` local, via a ternary.
- **22 in-place spellings** via `tools/try_batch.py` - `!arg`, `arg==0`, `0==arg`, `!(arg!=0)`,
  `!static_cast<int>(arg)`, `~x&1`, `&1==0`, `^1`, `|0`, `+0`, a `*(int*)&arg` pun, `==false`,
  `==0u`, literals, and three destination types - best result **4 differing instructions**, mask
  present in all of them. Taking the address removes the mask and costs a spill, a 48-byte frame
  and 32 differing instructions.

**The decisive evidence.** Retail's own binary is **not self-consistent**. At
`IsOneShot__20CScriptStreamedMusicFb` (0x8015DDD8) retail contains
`clrlwi r0,r3,24 ; cntlzw r0,r0 ; srwi r3,r0,5 ; blr` - the exact mask, on a `bool` parameter,
negated - produced by the compiler that **omitted** it 22 KB away in this constructor. Across the
binary, 1287 `clrlwi ...,24` and 616 `cntlzw` instructions **never co-occur within 4 instructions**
of each other.

**So the mechanism is a compiler *version* difference, not a source puzzle**, and mwcceppc
normalises *every* bool-to-word widening of a **register-resident** value.

**But the conclusion drawn from that was wrong, and is superseded - see "the 20-version sweep"
below. A second compiler version does not unblock it.** The version difference is real; the hope
that another version would close *this* function was not, and that was measured rather than
assumed.

**The five differing instructions**: ours is 46 instructions to retail's 45. Four are knock-on
register shifts; the only real defect is `clrlwi r0,r31,24` where retail has `cntlzw r0,r31`.
`flip_test` FAILs and reverts cleanly - it cannot be `Matching`, and it is recorded as
`NonMatching` deliberately.

**Why that is still worth having.** The body is written and 104 of 109 instructions are real, so
the port gets a real `CErrorOutputWindow` constructor instead of a stub, for `+1` on the link gap.
The two data objects it names are now accounted for: **`lbl_803A9F38` is retail's own bytes** -
`"Error output window"`, 0x14 exactly, defined in `PortGlobals.cpp` from main.elf rather than
guessed - and **`lbl_803B5910` is retail's `vtable for CErrorOutputWindow`**, two zero header words
followed by five *guest code addresses*. That one is deliberately not defined: writing guest
addresses as host data would be a lie, and the correct answer is a real key function, which buys a
function that cannot be `Matching` anyway.

**The transferable part: check the binary before believing a "the compiler always does X" claim.**
Retail's inconsistency *is* the proof, and it is a five-minute check that would have saved the
first lane's whole budget.

## The named blocking list moved: `InitializeSubsystems` 72.36% -> 97.76%, `FillInAssetIDs` 100%

`CMain::InitializeSubsystems` (retail 0x80008680, 348 B) went from **72.36% to 97.76%** because all
five of its unwritten callees were written, and `CMain::FillInAssetIDs` (0x80006B38, 72 B) reached
**100.00%**, as did `PostInitialize`, `LoadStringTable` and `InfiniteLoopAlarm`. One new `Matching`
unit: `fn_80003858` (0x80003858, 0x24), which is **boot step 21g** - its only caller is at 0x80006368
inside `RsMain`.

**`InitializeSubsystems` is 16 instructions short and cannot promote from `mainTail.cpp`,** for two
reasons, both structural rather than a matter of effort:

- the range a `Matching` carve needs is 0x80008570..0x800087DC, which includes
  **`CMain::ShutdownSubsystems`** (272 bytes, currently 1.47%). **That single function is now the
  only thing between this unit and `Matching`** - it is the shortest path to a `Matching`
  boot-path function in the tree.
- the remaining 16 instructions are an r4/r5 swap in a loop word, and **mwcceppc normalises `+`**,
  so `add r0,r0,r3` cannot be made to come out as `add r0,r3,r0`. Fifteen spellings measured.

**Two corrections to facts the tree had wrong**, both found by writing the body:
the stack-guard fill word is **`0x7337D00D`**, not `0x7338D00D` as `boot_path.md` and the shared lane
notes said; and the `ARInit` argument is a **relocation against `lbl_803C5AB8`**, not a literal.
`stackBase` also has to be read *before* `OSProtectRange`, or the range is computed from a value
the call has already changed.

**And the port was omitting the one callee it could have run.** `PortInitializeSubsystems` in
`src/MetroidPrime/PortBoot.cpp` skips five of retail's six calls, correctly and with reasons
written down - two of them genuinely cannot run on a host. It also skipped
**`CFrameDelayedKiller::Initialize()`**, the sixth and the **only one of the six that is written**.
That asymmetry is the point: the impossible ones were skipped deliberately and documented, and the
possible one was skipped by accident. It is now called, after the printfs, because that is retail's
order and order is the whole of what a boot sequence is.

**`CMain::RsMain` (2148 B) and `CheckReset` (1180 B) cannot be carved** - they sit inside
`main.cpp`'s single claimed range, and a second discontiguous range fails `dtk dol split` with a
link-order cycle. Both stay in `main.cpp` until something splits that unit.

## PROVEN: `ResetGameState`'s loop is not what blocks it — the object is 4 bytes too big

`CMain::ResetGameState` has sat at 98.61% with a header documenting six register-swapped
instructions. A lane was given the one untried route the header named — retail's loop is an inlined
`~reserved_vector`, not a spelled-out `for` — and the result is worth more than a unit:

**The hypothesis was right and the route still does not pay.** `~reserved_vector` over a 16-byte
record with a declared-and-empty destructor **does** emit retail's exact register assignment
(`li r3,0` counter, `addi r4,r5,-8` unroller temp). A spelled-out loop provably cannot, and the
mechanism is now named: **a spelled-out loop keeps the register holding the tested address reserved
for the rest of the `if` block**, so the counter is pushed to the next register and the temporary
reuses the address register. That is the whole of the six-instruction difference — and it cannot be
reached from inside this function, because every way to get that destructor in costs more:

| route | differing instrs |
| --- | --- |
| spelled-out loop (the incumbent) | **9** |
| hand-written `~()`, 12 bodies measured | 10 (a plateau) |
| `reserved_vector` member, ctor zeroes the count | 22 |
| the same, wrapped in a union (MWCC still runs the member ctor) | 22 |
| any destructor route | an extra out-of-line weak `__dt__`, so never `Matching` |

**And the loop is irrelevant to promotion anyway.** `tools/unit_fit.sh` measures our object at
**420 bytes against a claimed 416, retail 416 — over by 4.** The overshoot is the single extra
instruction `mr r0,r3 ; mr r4,r0` where retail has the record form `mr. r4,r3`. **`flip_test`
cannot pass at 420 bytes whatever the loop does**, so the six-instruction difference is not the
wall and never was.

**Which makes this a trade, not a wall, and an admissible one.** The four bytes go away only if the
allocation is spelled `new CGameState`, because mwcceppc's `new` expansion puts the result straight
into the argument register of the following call and tests it there, emitting `mr. r4,r3`. That
relocates against `__ct__10CGameStateFv`, which nothing defines, because
`src/MetroidPrime/Player/CGameStateCtor.cpp` is an `extern "C"` function named for its address
(`fn_801449C8`) — **a C++ constructor cannot `return self;`**, which is the very reason that unit
has that shape.

Measured: **`CGameStateCtor` is `Matching` with exactly 1 function at 100.00%, and
`ResetGameState` is 1 function at 98.61%.** So the trade is **1 for 1** — and because the project
already renames symbols to match (`fn_801462DC` -> `__ct__23SPersistentOptionsValueFiii`,
`fn_801449C8` -> `__ct__10CGameStateFv` is available the same way), the ctor unit may not even have
to *lose* `Matching`. Either way `linked` does not fall, which is the constraint that decides it.

## SOLVED: the second compiler is already installed, and it is GC/3.0a3 or later

The question "what unblocks `CErrorOutputWindow`" was *"a second compiler version"*, and that
framing was wrong in a way worth correcting: **twenty GameCube compilers are already on this
machine** (`build/compilers/GC/1.0` .. `GC/3.0a5.2`). The project builds with `GC/2.7`. So this was
never a purchase, it was a sweep.

`tools/probe_cntlzw_versions.py` compiles a four-function probe with **each** version, using
`configure.py`'s exact flag list, and looks for `clrlwi` in the output:

| version | `!x`, bool in a register | after a call | stored to a field | `const bool` local |
| --- | --- | --- | --- | --- |
| `GC/1.3` .. `GC/2.7` (10 versions, incl. **the one we build with**) | **MASK** | MASK | *bare* | MASK |
| **`GC/3.0a3` .. `GC/3.0a5.2` (7 versions)** | **bare** | **bare** | MASK | **bare** |

**So `GC/3.0a3` and later emit retail's exact shape** - a bare `cntlzw` on a register-resident
`bool` - which is what `CErrorOutputWindow::CErrorOutputWindow(bool)` (0x8018169C) has. The mask is a
`GC/2.x` behaviour and `GC/3.0` dropped it.

**This confirms the version hypothesis from the other direction, and it explains retail's
inconsistency:** MP2's build used more than one compiler version, which is why the binary contains
both forms 22 KB apart.

**The open question is no longer "which compiler" but "can one unit be built with it".** `GC/3.0` is
a different code generator overall - it requires `-enc SJIS` where 2.x wants `-multibyte`, which
`configure.py` already knows (`if version_num >= 3`), but the DOL is otherwise built entirely with
2.7. A mixed build is the thing to test, and it is a *build-system* question, not a codegen one.

**Three of my own mistakes got into this tool, and each is recorded in it**, because a probe that
cannot reproduce the failure cannot rank anything:

1. A reduced flag set made **every** version look unmasked - it had dropped `-inline auto`, and
   "register-resident" is exactly the condition under which the mask appears.
2. Rebuilding the flags as a Python argv list reproduced **none** of the working invocations:
   `-RTTI off` split into two arguments and every version aborted with *"Specified file 'off' not
   found"*. The fix is to build a **shell string**, like the gate's own command line.
3. `OBJDUMP` pointed at the sibling port's toolchain rather than this repo's, and `ROOT` resolved to
   `tools/`. And when all twenty compiles failed, the first version printed *"no version emits a
   bare cntlzw"* as though that were a measurement - **a tool that reports a finding when it measured
   nothing is worse than one that fails**, because it is indistinguishable from a real negative. It
   now prints "NO VERSIONS COMPILED - this is not a result" and exits 2.

Five versions (`GC/1.0` .. `GC/1.2.5`) do not compile these flags at all and are not a result either
way.

## PROVEN: `CMain::ResetGameState` is blocked, by a chain with a named cause at each link

The trade looked admissible — 1 `Matching` function for 1 — and it was. It still fails, and the
reason is a four-step chain, each step measured:

1. **The 4 bytes are recoverable.** `new CGameState` is the *only* spelling that loses them:
   `unit_fit.sh` goes 420 -> **416, "fits"**, and 98.61% -> **99.62%**. Eleven hand-written
   allocation spellings were measured and all are worse (9-13 differing instructions against 7).
2. **But `new CGameState` needs `CGameState::CGameState()` to resolve**, and making that a real C++
   constructor costs **8 bytes on a `Matching` unit**: 740 -> **748, "over by 8"**.
3. **The 8 bytes are a duplicated call**, and this is the useful part. `CGameState` has
   `CGameOptions gameOptions` at +0x80, and `CGameOptions` declares a default constructor. A real
   `CGameState()` therefore makes mwcceppc **hoist implicit member construction into the prologue**
   *and* keep the body's explicit `CTOR_GAMEOPTIONS(this)` call — so `__ct__12CGameOptionsFv` is
   emitted **twice**, once at `.text+0x1C` and once where the body had it.
4. **Retail calls it once, at `.text+0xA8`** — between `fn_80145950(&this->x54)` and
   `fn_80180738(&this->hintOptions)`, i.e. **in the middle of the body**. A C++ constructor
   constructs members in its prologue or its member-initialiser list; it cannot place one mid-body.
   Deleting the body's explicit call does give 740 bytes that "fit" — at **93.90%**, because the
   call then sits in the wrong place.

**So the wall is: `CGameStateCtor.cpp` must stay `extern "C"`, because retail initialises a member
mid-body and no C++ constructor can express that; and `new CGameState` cannot be spelled without a
real C++ constructor; and without `new CGameState` the 4 bytes cannot be recovered.** The unit is
`NonMatching` and stays that way. 1 `Matching` function for 0 is not a trade.

**Also settled, negatively:** the six register-swapped instructions do **not** interact with the
allocation. After the `new` change `try_batch.py` reports the identical **7 differing instructions**,
so the premise that fixing one might move the other is false. 31 further loop spellings measured (7
or worse), operand order on the null test is a no-op here, `if (&local)` folds the guard away, a
`static inline` wrapper is 22. **And there is no vtable** — `CGameState` declares no virtual and has
no base, so the "a real key function is good for the port" argument does not apply.

**What is left, and it is one idea rather than another spelling:** the loop's mechanism is still
unidentified, but the evidence has narrowed it — **a `~reserved_vector` inside a `Matching` unit
*does* produce retail's register order**, while a spelled-out loop provably cannot. So the remaining
route is to put the loop in its own unit rather than to re-spell it here. That is untested.

## `COsContext`'s two words were named for each other's contents

A lane decompiling `COsContext`'s constructor found that the header had the pair backwards:
**+0x10 is the console type and +0x14 is a language.** Retail's constructor stores
`OSGetLanguage() & 0xF` at +0x14 *before* `CBasics::Init`, and the `EConsoleType` at +0x10 *after*
it. The header called them `x10_format` (a TV-format code) and `x14_consoleType`, so each word was
named for the other one's contents.

Fixing it in the tree turned up a second thing: `COsContext.cpp` wrote the `OSGetConsoleType()`
switch into the *language* word, and wrote a TV-format code into the *console type* word. The first
is now correct - the console type goes to +0x10, where retail keeps it. The second has **no home in
retail's layout at all**, and nothing in the tree reads it, so the store is dropped rather than
relocated. There is no offset left for it, and inventing one would be exactly the kind of guess
`PROCESS_LESSONS.md` #17 is about.

**A header rename with in-tree users is the hazard that bit here**: the lane renamed the members and
did not update `COsContext.cpp`, so the port build failed with `x10_format was not declared in this
scope` - and a *rename* turned it into `multiple initializations given for x10_consoleType` when I
retargeted the switch, because the init list had been renamed too. **Grep the tree for the old
names before you assume a lane's header change is self-contained.**

Landed: `Kyoto/Basics/COsContextAllocFromArena` - `COsContext::AllocFromArena(unsigned long)`,
retail 0x8028BFFC, 0x5C = 92 bytes, **`Matching` 100.00%**, `flip_test` PASS. Plus two renames of
dtk-anonymous symbols: `fn_8028BFFC` -> `AllocFromArena__10COsContextFUl` and `fn_8028C09C` ->
`__ct__10COsContextFbb`.

**Neither new file is in `files.cmake`, on purpose**: `COsContext.cpp` already defines both methods
for the host, so listing either is a **duplicate definition that `link_gap.py` structurally cannot
see**. Both are in `check_files_cmake.py`'s `EXCLUDED` with that reason.

**And the lane's own headline was a negative that corrects a premise in its brief**: zero link
symbols closed, because `COsContext` and `CMemorySys` were *already fully defined*. The only
gap-list entry naming them is `AllocateRenderer(...)`; carving that closes 1 and opens 2, so it was
not done. **`x10_last`'s misalignment is not a decompilation defect either** - retail's inlined
`GetBaseFreeRam` is byte-identical to ours, and `CGameAllocator.cpp` is `NonMatching` and therefore
not linked, so a change there provably cannot move `main.dol`.

## SUPERSEDED: a second compiler version does **not** unblock `CErrorOutputWindow`

I wrote "what unblocks it is a second compiler version" above, on the strength of a lane's
version hypothesis. **That was then measured, and it is false.** A lane built the mixed-compiler
build and swept every version against the real source.

**The mechanism works, and that part is a real capability.** `Object(..., mw_version=..., cflags=...)`
resolves as per-object overrides - `tools/project.py:65` carries `mw_version` in `Object`'s options,
line 665 turns it into the compiler path, and line 1025 collects a *set* of them - so a mixed build
needs **no change to `project.py` at all**. `dtk`, ninja and objdiff all accepted it. The flag
difference is one substitution, so it cannot drift:

```python
cflags_gc30 = [("-enc SJIS" if f == "-multibyte" else f) for f in cflags_retro]
Object(NonMatching, "MetroidPrime/CErrorOutputWindowCtor.cpp",
       mw_version="GC/3.0a3", cflags=cflags_gc30),
```

**And 3.0a3 makes this function worse.** `tools/probe_cerror_versions.py`, sweeping all twenty
versions against the real body:

| | instructions emitted |
| --- | --- |
| every `GC/2.x` | 46 |
| every `GC/3.0a*` | **34** |
| **retail** | **45** |

**None is byte-exact.** 3.0a* does fix the `cntlzw` this function wanted, and then breaks two other
things: at `-O4,p` it coalesces retail's four `lbz`/`rlwimi`/`stb` read-modify-write pairs, and at
`-O1` it keeps those but drops a `li r3,1` that the retail code common-subexpressions. **Both
remaining gaps are redundant-load/store elimination, and no flag exposes them** - `-no_peephole`,
`-optcode_speed` and `-O4,t` were tried, and there is no CSE switch in `-help all`.

So the score is **78.56% (2.7) -> 55.44% (3.0a3, `-O4,p`) -> 13.11% (3.0a3, `-O1`)**, and
**78.56% is the best any compiler on this machine achieves.** The version hypothesis was *right about
retail* - MP2's build did use more than one compiler, which is why its binary holds both forms 22 KB
apart - and *wrong about the conclusion*. The unit stays `NonMatching`, and the reason is now
measured rather than inferred.

**A mechanism, not a policy.** No unit in this tree wants a 3.0 object: `CFrustumPlanes::__ct__` goes
from 17 differing instructions to **293**; `CVector3f::Cross` is 13/16 under both, and 3.0a3 *loses*
one that 2.7 gets; and `CGX.cpp` **will not compile** at all under 3.0a3 (`illegal reference type
'void &'`, `single_ptr.hpp:35`). **The capability is unlocked and currently unused** - which is the
right state for it, and worth knowing before anyone assumes the toolchain is the limitation.

**A methodological note, because this is the second time in two days.** The first
`tools/probe_cntlzw_versions.py` probe said "every version emits a bare `cntlzw`", which was an
artefact of too few flags. The lesson generalises: **a probe on a synthetic four-line function ranks
compilers; only a probe on the real body settles one.** The second probe
(`tools/probe_cerror_versions.py`) runs the actual unit, and it gave the opposite answer to the
first - which is exactly why the first was not trusted for a decision.

## The `mainTail.cpp` split WORKED - which is the unlock for `CMain::RsMain`

`CMain::ShutdownSubsystems` (retail 0x80008570, **0x110 = 272 bytes**) is **`Matching` at 100.00%**,
up from an empty `{}` body at 1.47%. It was inside `mainTail.cpp`'s single claimed range, and
**`CMain::RsMain` (2148 bytes) and `CMain::CheckReset` (1180 bytes) are still inside `main.cpp`'s** -
which is why they have been described as uncarvable all session.

**That excuse is now measured false.** Cutting `mainTail.cpp` at 0x80008680 - the new unit takes
0x80008570..0x80008680, `mainTail.cpp` keeps 0x80008680..0x80009880 **plus `.ctors` and `.sbss`**,
which must stay on the last unit - is **accepted by `dtk dol split` with no link-order cycle.** The
`fn_8000934C` re-split's cost does not apply: nothing between the two is a unit boundary, so **only
one function moves**, and it was the worst in the range. The gate reports it as
`SPLIT ... exact count match - a split, not a loss`.

**So the next target is the same operation on `main.cpp`**, which would let 3,328 bytes of
boot-path code be carved and worked per-function instead of as one 6,000-line unit.

**`CMain::InitializeSubsystems` now waits on one thing only.** The structural blocker - that a
`Matching` carve for it needed 0x80008570..0x800087DC, spanning both units - is gone. What remains
is 16 instructions of r4/r5 swap in a loop word that mwcceppc normalises.

**Three register-allocation findings from that loop, and the third is the transferable one:**

1. **Retail's mask is a BYTE mask.** The encoding `54 60 06 3f` is `& 0xFF`; `& 0xFF000000` and
   twelve other spellings emit `54 60 00 0f` - one instruction wrong. **`clrlwi` versus `clrrwi` is
   a red herring; compare the encodings.**
2. `+ 0x400/4` on a `uintptr_t` is a **byte** add (`addi ...,256`), and the cast must precede the
   add. `(uint)(limit - p)` on two `uint*` makes mwcceppc emit a spurious `srawi r0,r0,2; addze`.
3. **About seventy register spellings all held at exactly 5 differing instructions** - types
   (`uint`/`u32`/`int`/`long`/`uchar*`), six declaration orders, the bound hoisted in and out of the
   `for`, `while` against `for`, `++p`/`p += 1`/`p++`, `& 0xFFFFFC00`, a named aligned value,
   `uint*`/`char*`/integer loops, unused locals, `-pragma "inline_max_size(125)"`, hoisting `limit`
   to function scope (all *worse*). **The fix was splitting one expression into two statements**
   (`uint* p = (uint*)(...); p += 0x100;`) so the allocator coalesces the masked value into `p`'s
   register. *When seventy spellings of an expression all land on the same instruction count, the
   answer is not another spelling - it is a different statement decomposition.*

**One caveat, recorded by the lane and correct:** the host path is `PortShutdownSubsystems()`, which
is just `CFrameDelayedKiller::ShutDown()`, because eleven of the twelve callees are unwritten retail
functions. **Nothing in the port calls `CMain::ShutdownSubsystems` yet**, so `linked` rising by one
with the link gap unchanged is expected - it is a teardown nothing reaches yet.

## Step 17 RUNS. The stop was a stale guard, and its own comment said so

`new CGameArchitectureSupport(*osContext)` - **0x80007EC4, 1016 bytes of retail code** - now
**executes to completion on the host**, and the boot probe says so. Not one percentage moved to
get here.

`src/MetroidPrime/PortBoot.cpp` ended step 16 with a hard-coded `printf` and `return 1`, and the
message listed three functions as the wall. **That guard's own comment refuted it two paragraphs
earlier**: it stood because the constructor "makes two unguarded global dereferences",
`gpTweakPlayerA` at 0x80007F38 and `gpGameState` at 0x800081A4 - and then recorded that
`CGameStateCtor.cpp` is "`Matching` and since 2026-09-26 in the port build", which is what fills the
second, while `CreateStandInTweakPlayers()` supplies the first. Both conditions met, guard not
removed. **A hard-coded stop message cannot distinguish "this faults" from "this was never
tried", and those need completely different work.**

The three functions it named were never the wall either, and two are now closed outright:
`ResetGameState` by a proven four-step chain, `CErrorOutputWindow` by a 20-compiler sweep.

### Three real defects were hiding behind that guard

**1. `TOneStatic<T>::operator new(size_t, const char*, const char*)` was declared and never
defined.** `operator delete` has always been there, so the type was half-formed, and the 1-arg form
at line 14 forwards to the missing 3-arg one. Retail's definition lives in its CRT; the header
carries it **commented out as a hint on the declaration** - `ReferenceCount()++; return
GetAllocSpace();` - and it is now defined from that hint, not invented. `CGameArchitectureSupport`
is the one class here deriving from it, and its sibling `CGameGlobalObjects` is a plain class,
which is why only this one failed. No-op for the DOL: a template instantiates only where a unit
writes a `new`, and the hash confirms it.

**2. A reach stub was aliased onto a symbol that now has a real definition.**
`PortReachStubs.cpp` does `extern "C" void reachstub_40() asm("_ZN18CErrorOutputWindowC1Eb")`, so
the linker resolved retail's name to a stub. Listing `CErrorOutputWindowCtor.cpp` - which *defines*
that name - made the two collide, and the shipping build failed with `multiple definition of
'CErrorOutputWindow::CErrorOutputWindow(bool)'`. There are **182 such aliases in that file**, so
this was a class of defect, not an instance.

**3. The gate's duplicate check could not fail, and that is how (2) reached the link.** The three
counts are *scraped out of the build log* by regex, and `link_check.sh` exited 0 unless there were
compile errors - **a duplicate definition was printed and did not fail the run.** A run that never
reaches the linker's duplicate pass scrapes to `0` and reads as a clean result, so a count from it
is not a small number, it is **no number**. The same shape as a tautological check in
`PROCESS_LESSONS.md`.

Two changes, and the second is the one that matters: `link_check.sh` now fails on a duplicate as
well as on a compile error, and it reports **`the LINKER NEVER RAN`** when the log contains no
linker diagnostic at all - which `gate.sh`'s `port link dups` step treats as a failure instead of a
zero.

### Where the duplicate check has to live, which is not where I first put it

I got this wrong twice, and both errors are worth recording because the second one looked right.

**First error:** I wrote that `link_check.sh` "only ever measures `MP_SDK_HEADERS_ONLY=ON`". It does
not - line 119 passes `-DMP_SDK_HEADERS_ONLY=OFF`, the shipping configuration, and always has. I
inferred it from the flag name without reading the invocation.

**Second error, and it is the interesting one:** I then assumed the gate's `port link dups` step
missed the duplicate because of the `ON`/`OFF` source-list divergence, and added a "the LINKER NEVER
RAN" guard to `link_check.sh` on that basis. **I injected the colliding alias to test it and the
gate passed.** The guard is sound and stays, but it does not cover this case, and the reason is
structural:

**`src/MetroidPrime/PortReachStubs.cpp` is added by `CMakeLists.txt:66` only under
`-DMP_BOOT_STUBS=ON`, and only `tools/boot_probe.sh` passes that option.** So the file holding the
182 `asm("_ZN...")` aliases **is not compiled in the build the gate measures at all.**
`duplicate definitions 0` is therefore a *true* statement about a build in which the offender is
absent - which is exactly why reading it as "the tree has no duplicates" is the trap. Nothing failed;
the number was answering a different question than the one I asked of it.

So the check belongs in **`tools/boot_probe.sh`**, the one build where a stale alias can collide, and
it is there now: it counts `multiple definition of` lines, names the symbols, and says the fix is
to delete the stale alias rather than to drop the decomp unit. Verified by re-injecting the exact
alias that broke the shipping link and watching it report, then reverting.

**The general form, and it is the third time this session:** *a number produced by a tool is a
measurement of the configuration that tool ran in, not of the tree.* The gap list, the dups count
and the probe's undefined count are all configuration-specific, and reading one as a property of the
source is how a gate passes over a real defect.

### And one that was not a defect at all

`fflush(stdout)` put `stdout` in the link gap - the only new gap symbol in the whole change. `printf`
has never appeared in that list, and the difference is why: `--allow-shlib-undefined` satisfies a
*function* from libc's dynamic table but not a *data* symbol, because `stdout` wants a copy
relocation. **`fflush(nullptr)` is the same diagnostic with no data symbol**, and the markers are
worth keeping - their whole job is to survive a fault, which needs the flush.

## `CMain::RsMain`: the split works, and it is still not worth taking yet

`mainTail.cpp`'s split recipe **generalises to `main.cpp`** - the thing I said was measured false an
hour ago is now measured true in the other direction too. `dtk dol split` accepted three cuts
(0x800053B8-0x80005C6C / 0x80005C6C-0x800064D0 / 0x800064D0-0x8000848C) with **no link-order cycle**,
both new boundaries being function boundaries that are not another unit's boundary, moving **38
functions**. So `RsMain` (2148 B) and `CheckReset` (1180 B) are now reachable by a carve.

**And I am not taking it.** Three reasons, all measured:

1. **It gains nothing.** `matched 3957` and `linked 2534` are *identical to baseline*. `RsMain` stayed
   at 0.26% and `CheckReset` at 0.47%.
2. **It costs fidelity.** `__ct__24CGameArchitectureSupport` went **93.10% -> 87.99%** and
   `AddPaksAndFactories` 57.15% -> 57.04%, because 38 functions changing units moved mwcceppc's
   `@stringBase0`. **That constructor is the one step 17 now executes**, so this is a quality loss on
   the function the port actually runs. Both cut directions give 87.99% and bisection does not
   converge, so it is not a placement mistake - it is a property of how much moved. The one-function
   `mainTail` split cost nothing, and that contrast is carve-vein rule 2c.
3. **The blocker is a header job, which the split cannot touch.** `CMain`+0x18..+0x48 holds two
   20-byte frame-time histories that `include/MetroidPrime/CMain.hpp` does not model - they sit
   inside `char x10_pad[0x38]` at line 137 - and `fn_800069AC`, the bounded insertion-sorted float
   push that `RsMain` calls **six times**, is **308 bytes and unwritten**. `unit_fit` says it plainly:
   claimed 2148, ours 8, **short by 2140**. Writing a partial body makes it *worse*, because the
   empty 8-byte frame already matches retail's prologue exactly.

**So the order is: model the two histories, write `fn_800069AC`, and only then split.** A split is not
progress on its own - it is a permission slip. Patch preserved at `/tmp/lane-keepers/rsmain.patch`.

**Two side findings worth keeping.** The port link is *unchanged* by all of this, because
`CMainRsMain.cpp` keeps `#ifndef TARGET_PC` and the host body of `CMain::RsMain` is `PortBoot.cpp` -
`boot_path.md` step 6. And `decl_order.md`'s stale `main` bullet had to be **deleted rather than
annotated**, because the checker reads an annotation as the entry itself: a doc tool that treats
"this is superseded" as "this is the claim" is a trap worth writing down before it costs someone an
hour.

## `AllocateRenderer` is a proven structural wall, and row 21c was wrong about what the pixels are

Both pixels-chain functions are written. **Neither is `Matching`, and for `AllocateRenderer` the
reason is structural and measured rather than a spelling.**

| unit | retail | size | objdiff | state |
| --- | --- | --- | --- | --- |
| `MetaRender/Carve8026EF54` (`AllocateRenderer`) | 0x8026EF54 | 156 B | **100.00% fuzzy, 100.00% matched code, 1/1** | **`NonMatching`** - see below |
| `MetroidPrime/Carve80049244` | 0x80049244 | 280 B | 97.14% fuzzy, 0/1 | `NonMatching` - over by 180 B of `rc_ptr<CIOWin>` COMDATs |

`AllocateRenderer` reproduces retail's bytes exactly and **still cannot be `Matching`**, because of a
three-link chain:

1. Retail's 2nd argument is **`&.rodata[86]` = 0x803AE412**, materialised as
   `lis r3,0x803AE3BC ; addi r3,r7,-7236 ; addi r4,r3,86`. **Only** the spelling
   `lbl_803AE3BC + 86` produces those three instructions - measured 156 bytes, 39 instructions,
   **100.00%**.
2. `lbl_803AE3BC` has to be *defined*, `.rodata` is the only section covering it, and
   `symbols.txt` gives it `size:0xFC` - so **the whole 252 bytes is this unit's claim or nothing**.
   Claiming at 0x803AE3BC and at 0x803AE3B8 are both refused. The nearest 8-aligned `.rodata`
   symbol is `lbl_803AE130`, which is **904 bytes of unrelated tables**.
3. **0x803AE3BC is 4 (mod 8) and every MWCC data input section is 8-aligned**, so mwldeppc moves the
   object 4 bytes and the built DOL then differs in **856 `.text` and 6651 `.rodata` bytes**. The
   sha1 fails.

**So this is a linker-alignment wall, not a source puzzle, and the fix is not a 71st spelling.** The
unit is `NonMatching` deliberately and its header carries the full chain.

**`fn_80049244` is blocked by 180 bytes of `rc_ptr<CIOWin>` COMDAT instantiations** - the same cause
as `CIOWinManagerRemoveAllIOWins.cpp`, which cannot flip for the same reason - plus two prologue
instructions. Its 2nd argument needed no reinterpretation; string literals, a ternary for
`p ? p + 4 : p`, assigning the global twice, and a 4-byte `.sdata` claim were all measured worse.

### Row 21c was wrong about what the pixels are, and the correction moves the target

`boot_path.md` said the draw at 21c is `fn_80049244`. **It is not.** Vtable slot **+0x94 is
`CCubeRenderer::BeginScene` at 0x8026FBFC (0x180 = 384 bytes)**, and `fn_80049244` walks
`x0_drawRoot` **twice** - a PreDraw and then a Draw. Row 21c is corrected in place.

### And the more important thing the lane found: the port never calls the code that sets `gpRender`

`gpRender` is non-null **in the compiled code** after step 12 - and the boot **never runs step 12**,
because `PortBoot.cpp` stops after step 17 by its own choice and never calls
`CGameGlobalObjects::PostInitialize`. **So "gpRender stops being null" was true of the object file
and false of the running program, and only the second matters.** No frame rendered; none claimed.

**The next renderer target is `fn_80271238` (0x80271238, 0x59C = 1436 bytes)**, which is
`CCubeRenderer`'s actual constructor - `AllocateRenderer` returns a real pointer to an
**unconstructed** object. That is a large unit, and it is the first thing on the pixels path that is
a matching problem rather than an alignment wall.

## The ladder now CALLS step 12, and the boot dies inside it at a named point

`PortBoot.cpp` stopped after step 16 for the whole session, so **the boot never ran step 12** -
`CGameGlobalObjects::PostInitialize`, which is `Matching` 100.00% and has been in the port build
all along. Nobody was calling it. That is why a lane could correctly report "`gpRender` is no-null
in compiled code" and the running program still had a null `gpRender` at the frame loop: **the
statement that assigns it was never executed.**

`PostInitialize` is now called, with a null check on `gpRender` immediately after. Retail's order
inside it (`src/MetroidPrime/main.cpp:235-241`) is `AddPaksAndFactories()`, `LoadStringTable()`,
`AllocateRenderer(...)`, `gpRender = renderer.get()`, then `CEnvFxManager::Initialize()`.

**What the probe now does, which is new information rather than a new percentage:**

```
boot: step 12 - CGameGlobalObjects::PostInitialize(*osContext, *memorySys)
[reach-stub 0024] CGraphics::SetViewPointMatrix(CTransform4f const&)
[reach-stub 0025] CGraphics::SetModelMatrix(CTransform4f const&)
boot_probe: died on signal 11
```

**It runs, it gets past `AllocateRenderer`, and it faults inside `CEnvFxManager::Initialize()`** -
the last line of `PostInitialize` - after asking for two `CGraphics` methods it does not have. It
requests **25** symbols in total. So `gpRender` *is* assigned before the fault; the frame loop's
null vtable is no longer the first problem, and a fault deep inside step 12 has replaced it.

**That is a strictly better position than "never called", and it is the second time a hard-coded
stop was the thing standing in the way.** The first was the step-17 guard; this one was simply a
ladder that stopped writing itself.

**The work list this produces, in order, is short and each item is named:**

1. **`fn_80271238` (0x80271238, 0x59C = 1436 bytes)** - `CCubeRenderer`'s real constructor.
   `AllocateRenderer` returns a pointer to an **unconstructed** object, so every `gpRender->`
   virtual is a call into whatever the allocator handed back. **This is the first thing on the
   pixels path that is a matching problem rather than an alignment wall.**
2. **`CGraphics::SetViewPointMatrix` and `CGraphics::SetModelMatrix`** - the two it asked for by
   name. `SetModelMatrix` is already written and `Matching` 100.00% as
   `src/Kyoto/Graphics/Carve802C24AC.cpp`, but the signature the port asks for is
   `CTransform4f const&` while that unit defines the 12-float form - **check whether that is one
   function or an overload before assuming it is already there.**
3. **`CEnvFxManager::Initialize`** - the function that faults, and the reason 25 symbols are asked
   for at all.

**A note on the stub mechanism, because it is easy to misread.** The 25 `[reach-stub]` lines are
`boot_probe.sh`'s self-heal: it stubs what the link asks for, relinks once, and re-runs. **Those
stubs are no-op printers, not implementations** - the file says so and the probe prints
"the reachability stubs are DIAGNOSTIC - this is not the port". A fault *after* a stub line means
the fault is downstream of a function that does nothing, so the stubbed callee is part of the
cause, not the cause itself.

## Boot steps 18, 19 and 20 are now in the ladder, and adding them cost zero link symbols

`PortBoot.cpp` now calls, in retail's order and with a marker around each:

- **18** `CIOWinManager` - `Matching`, and `src/MetroidPrime/CIOWinManagerCtor.cpp` is in
  `files.cmake`.
- **19** `CGameOptions::EnsureOptions` (retail 0x801612C4, 0x10C) via
  `gpGameState->GameOptions()` - the `CGameOptions` member at +0x80, on the object step 7 filled.
  Already written at `src/MetroidPrime/Player/CGameOptions.cpp:207`. **Retail reads its bitstream
  out of a pak, which needs step 13, so on the port this is expected to do less than retail's does -
  the marker is there so that is visible rather than assumed.**
- **20** `CDvdFile::FileExists(const char*)` (retail 0x8030C04C, `static`), needing nothing. On a PC
  there is no disc, so `false` is the expected answer, **and a `true` would mean the port is reading
  a real retail pak** - which is why the result is printed rather than discarded.

**Step 18's four IOWin constructors are deliberately *not* called.** `CMainFlow` is `Matching` and in
the port build; `CConsoleOutputWindow` and `CAudioStateWin` are `Matching` 100.00% but deliberately
`EXCLUDED` from it. **Calling one would add a symbol the port does not define, and the undefined count
is the number the port is being planned against** - so the remaining step-18 constructors are a
*listing* decision, not a ladder decision, and they stay a lane's problem rather than becoming a
silent regression here.

### The A/B that says so, because the number said 323 and I expected 322

After adding the block, `link_check.sh --rebuild` reported **323** against a baseline of 322. My first
instinct was to look for a vtable, because `link_gap.py` showed no change and the file notes say a
vtable is invisible to `nm` until its key function exists. **That instinct was a guess and the
measurement contradicted it**, so I A/B'd it rather than writing it down:

| configuration | undefined | symbol set |
| --- | --- | --- |
| with steps 18/19/20 | **323** | identical |
| without them | **323** | identical |
| without them *and* without the step-12 call | **323** | identical |

**The ladder additions add nothing**, and the sets are byte-identical - so the design intent held. The
`322 -> 323` came from **316fc42**, the `pixels` collection, not from the ladder: that commit moved
`IRenderer`'s declaration out of `namespace Renderer` and corrected the fourth parameter from
`IResFactory&` to `IFactory&`, so **one mangled name changed**. The gap *list* cannot see it (it
derives from `nm`, and no named vtable appears), which is the documented reason the two counts differ
by a few.

**Baseline re-recorded at 323.** Worth stating the method rather than the number, because the first
thing I did here was compare two counts from two *different builds* and believe neither: `link_gap.py
--rebuild` had overwritten `build-port-link/build.log` between the two measurements. **A number
produced by a tool is a measurement of that tool's run, and a rebuild in between makes the two logs
uncomparable - take the diff from one build, or take neither.**

## A first frame IS reachable, and the reason is that Aurora's render-target model is retail's

I went looking for the wall that would make a frame impossible, expected to find it, and found the
opposite. **The evidence, because "no frame yet" is not by itself a reason to stop.**

**What I checked, in order.** `COsContext::OpenWindow` allocates two framebuffers
(`x24_frameBuffer1 = OSAllocFromArenaLo(x2c_frameBufferSize, 32)` and the same for
`x28_frameBuffer2`), sizes them from the render mode, and calls `VIConfigure(&x30_renderMode)` then
`VIFlush()`. **Nothing ever binds their address** - there is no `GXSetDRYFB`, no
`GXSetDispMemOffset`, no `GXSetPixelDepth` anywhere in the tree, and **Aurora's GX exports none of
them either.** Aurora's `GXRenderModeObj` is pure mode metadata: `viTVmode`, `fbWidth`, `efbHeight`,
`xfbHeight`, origins, `viWidth`/`viHeight`, `xFBmode`, `field_rendering`, `aa`, the sample pattern and
the vfilter. **There is no address field in it.**

So the first reading looks like a wall: retail binds an external framebuffer, the port cannot, and a
perfectly decompiled draw would still render into arena memory nobody presents.

**It is the opposite, and Aurora's own header says so.** Aurora provides `GXCreateFrameBuffer(u32
width, u32 height)` and `GXRestoreFrameBuffer(void)`, documented as *"Restore rendering to the main
EFB framebuffer. Must be called after `GXCreateFrameBuffer()` to resume normal rendering."*

**Aurora's default render target is already the window's framebuffer.** The GameCube's model - bind
an XFB address, draw into it, VI scans it out - is replaced by "draw into the EFB, and the EFB is
composited to the window". The `GXCreateFrameBuffer` pair exists precisely to go *off* that default.

**Three consequences, and they change what is worth doing:**

1. **`COsContext`'s two framebuffers are dead but harmless on the port.** They are retail's
   double-buffered XFBs, correctly allocated and correctly sized, and nothing reads them. **They do
   not need binding, and a lane should not spend budget trying.** `GetFramebuf1/2` answering a real
   pointer is not a prerequisite for a frame.
2. **The port does not need a framebuffer porting task at all.** `src/Kyoto/Graphics/CGX.cpp` already
   references **51 distinct `GX*` entry points** and is in the port build, and those names resolve to
   `libaurora_gx.a` at link time. Retail's `GX*` calls are already landing on Aurora. A
   `CCubeRenderer::BeginScene` at 100.00% would therefore put pixels in the window, because the draw
   it programs is already the draw Aurora performs.
3. **`platform/smoke.cpp` is not evidence for this and should not be cited as it.** Its
   `[area-smoke]` and `[morph-smoke]` paths call `GXDrawDone()` and pass - but they drive Aurora's own
   machinery, **not `COsContext`'s framebuffers**, so they prove GX calls reach Aurora and nothing
   about retail's XFB path.

**The one remaining risk, and it is a per-symbol cost rather than a wall:** does Aurora implement
**every** `GX*` function retail's draw path calls? If the draw uses one Aurora lacks, the failure is a
link error naming it, and the cost is one shim. `CCubeRenderer::BeginScene` (0x8026FBFC, 0x180 = 384
bytes, vtable slot +0x94) is the function to write, and **`link_check.sh` after it lands is the
instrument that answers this** - it already exists, it already names every undefined symbol, and it is
the gate's own `port link gap` step.

**So the route to a first frame is: the draw (one 384-byte function), the frame loop that calls it,
and whatever `GX*` symbols the link then asks for.** None of that needs a new idea, and the thing I
was about to record as an architectural impossibility is not one.

## The fault moved into real code. The next blocker is GX init, not a missing symbol

`fn_802C2614` (0x802C2614, **0x9C = 156 bytes**) is **`Matching` 100.00% on the first try**, and it
closed a port symbol: **`CGraphics::SetModelMatrix(CTransform4f const&)` left the link, 323 -> 322
undefined, 0 added.** The brief's claim was verified rather than assumed -
`src/Kyoto/Graphics/Carve802C24AC.cpp` does define exactly
`_ZN9CGraphics14SetModelMatrixERK12CTransform4f`, and `fn_802C2614` was the only missing piece. Its
five SDK callees cost nothing: Aurora's `C_MTXCopy`/`Concat`/`InvXpose` and
`GXLoadPosMtxImm`/`GXLoadNrmMtxImm` all resolve, confirmed with `nm` on the probe binary.

**The boot probe now reads:**

```
boot: step 12 - CGameGlobalObjects::PostInitialize(*osContext, *memorySys)
[reach-stub 0023] CStaticInterference::CStaticInterference(int)
[reach-stub 0024] CGraphics::SetViewPointMatrix(CTransform4f const&)
boot_probe: died on signal 11
```

**`SetModelMatrix` is gone from that list, which means it ran as real code - and the crash is now
inside it**, in `fn_802C2614` -> `GXLoadPosMtxImm` with no GX state behind it. **So the boot has
stopped asking for symbols and started crashing in code it has.** Those are different problems with
different fixes, and this is the first time the boot has been the second kind.

### Two more proven walls, both with the measurements

**`CGraphics::SetViewPointMatrix`** (retail 0x802C2534, 0xE0 = 224 B) is written and **`NonMatching` at
99.11% - 10 wrong bytes, every one of them a float register.** Retail used a pooled literal `0.f` for
the matrix's zero column. Reading the guest `lbl_8041E508` instead makes MWCC create that temporary
*first*, so it takes `f12` and `m[i][0]` drops to `f11`/`f10`/`f9`. All 55 instructions are otherwise
identical. **Claiming the 4-byte `.sdata2` at 0x8041E508 does not rescue it**, because that removes
dtk's definition of the symbol which `fn_802C229C`, `fn_802C27C4`, `fn_802C2B38`, `fn_802C2CC0` and
`CGraphics::LoadDolphinSpareTexture` all reference **by name** - `mwldeppc` then reports
`undefined: 'lbl_8041E508'` six times and the DOL does not link. Measured and do not retry: `= 0.f`
(the right allocation, but the DOL will not link), `const float zero = lbl_8041E508` and
`*(&lbl_8041E508)` (identical to the plain read), an in-unit `extern "C" float lbl_8041E508 = 0.f`
(lands in `.sbss`, so the allocation rotates anyway), and the literal spellings `0`, `0.0f`,
`(float)0`, `+0.f`, `1.f-1.f` - **all of which give retail's allocation.** The blocker is
*placement*, not spelling: it needs MWCC to place a `lbl_8041E508` definition in `.sdata2`, and it puts
`float x = 0.f` in `.sbss`.

**`CStaticInterference::CStaticInterference(int)`** (retail 0x8013CD54, 0x40 = 64 B) is fully decoded -
`: sources()` then `sources.reserve(n)`, where the header's `vector(int)` would add a store after the
call - and MWCC does emit the `__ct__` mangling, so it was claimable. **It is blocked by the tree's
`rstl::vector<T>::reserve`**, which instantiates three extra weak functions (0x98+0x44+0x54 = **304
bytes over**) where retail has 0x98+0x3C+0x8. The DOL grew to 3,969,248 and **every REL hash broke**,
so it was reverted. This is the same COMDAT-instantiation cause as `CIOWinManager::RemoveAllIOWins`
and `fn_80049244` - **it is now the third instance, and it is a property of the host `rstl`, not of
any one unit.**

**`CEnvFxManager::Initialize`** (retail `Initialize__13CEnvFxManagerFv`, **0x80166880, 0xEC = 236 B**)
was decoded but not attempted. Its shape: a virtual call on `*(0x80418EA4)` slot 7,
`fn_802FC63C(this+4, stream, 0)`, then a 256x2 `ReadFloat` loop filling `lbl_803DABE0`
(`size:0x800`), then a virtual call on the result's slot 2. **It needs the 0x80418EA4 global named
and a class to hang the call on** - which is a naming job, not a decompilation wall.

### `CCallStack` is noise on a PC port, and the lane's judgement is right

Three symbols, all `RAssert` scaffolding whose entire value is formatting `__FILE__:__LINE__` for a
crash that on PC should be a real assert with a real backtrace. **A `Matching` one buys nothing the
linker needs, and would then have to *not* format anything to be useful.** Skip; the two remaining
stubs are already in the ordered list if it ever matters.

## The boot's crash chain, measured - and a lane's inference about it was wrong

`platform/main.cpp` now installs a SIGSEGV/SIGBUS/SIGILL/SIGFPE handler that prints
`backtrace_symbols_fd` and then re-raises with the default disposition, and `CMakeLists.txt` adds
`-rdynamic` to `metroid_prime2_port` **because without it the frames come back as bare addresses**,
which is the one form of this output that is not worth having. The re-raise is deliberate: a handler
that swallowed the signal would make `boot_probe.sh` report a clean exit, **and a clean exit from a
segfaulting boot is the one lie worth being unable to tell.**

`tools/boot_probe.sh` reported "died on signal 11" and nothing else. That is enough to know the boot
died and not enough to fix it, because the last `printf` says which *step* and everything inside that
step is a guess. The handler has to work in the exact binary the probe builds - reach stubs linked,
no debugger - because that is the configuration that reproduces the fault.

**What it says:**

```
CMain::RsMain
  CGameGlobalObjects::PostInitialize(COsContext&, CMemorySys&)
    CGameGlobalObjects::AddPaksAndFactories()
      CResLoader::AddPakFileAsync(rstl::string const&, bool, bool)
        CMemory::Free(void const*)
          CGameAllocator::FreeNormalAllocation(void const*)      <-- SEGFAULT
```

**And it refutes what the last lane reported.** That lane said the crash was in
`fn_802C2614` -> `GXLoadPosMtxImm` "with no GX state", and that `CEnvFxManager::Initialize` was where
it died. **Both were inferred from the probe's stub log rather than measured, and both are wrong.**
`CEnvFxManager::Initialize` is the *last* statement of `PostInitialize` and was never reached; the
first statement is what kills the boot. `CGameAllocator::FreeNormalAllocation` is retail 0x8030DF94,
`size:0x1C0` = 448 bytes, in `main/Kyoto/Alloc/CGameAllocator` (22 of 25 functions, in the port
build).

**`CGameGlobalObjects::AddPaksAndFactories` is named in this project's objective as a port-blocking
function to prefer**, so the boot's real blocker is on the priority list already.

**Also measured while looking: `src/Dolphin/` is dead.** It is in neither `files.cmake` nor
`CMakeLists.txt`, so the tree's own retail `src/Dolphin/gx/GXInit.c` and the rest of that directory
compile nothing and **the port uses Aurora's GX.** That kills a whole line of enquiry before anyone
spends a lane on it - the earlier orphan sweep skipped `src/Dolphin/` as "the SDK", and the SDK turned
out to be *retail's* SDK, superseded by Aurora's. **An assumption baked into a check is worse than no
check, because it stops the question being asked.**

**The general form, and this is the third time this session:** the ordered stub list says what the boot
*asked for*, which is not the same as where it *died*. Two of the last three lanes reasoned from the
stub list as if it were a stack trace. It is not - it is a list of unresolved symbols in call order,
and code that resolves fine can still fault three frames deeper. **`boot_probe.sh`'s backtrace is the
instrument for "where", and the stub list is only the instrument for "what next".**

## Two adjacent carves, two Matching units, and my brief's claim about renames was wrong

**`fn_80193E08` (0x80193E08, 0x2C = 44 bytes) is `Matching` 100.00% on the first try**, `flip_test`
PASS - and it is not a mystery function any more. It is the **constructor of `CGameState`'s 12-byte
`+0x19C` object**. It stores a base vtable (`lbl_803B0D68`: two zero header words and one slot, so a
class whose only virtual is its destructor - `fn_80004798` is exactly a deleting destructor), then its
own (`lbl_803B5CB0`, **24 slots**), then zeroes **a byte at +4, a byte at +5 and a word at +8**.

**So the tree's `SGameStateMarker` (`u32, u32, u32`) is wrong in two of three members** - it is a byte,
a byte and a word, not three words. `fn_80193C8C` shows `+8` is a difficulty tier, read by
`CPlayerState::GetItemPercentageRatio`. The class is still unnamed.

**`fn_80049A98` = `CIOWinManager::RemoveIOWin`** (0x80049A98, 0x144 = 324 B) is written and
**`NonMatching` at 99.63%**, 14 differing instructions, `.text` exactly 324 B. The 14 are one register
choice - walk 1's `prev`/result hold r29/r27 where retail holds r27/r29 - and **~70 variants were
measured; 14 is the floor.** The `+180` in `unit_fit` is the two COMDAT weak `rc_ptr<CIOWin>`
instantiations `RemoveAllIOWins` already carries. Its `volatile` read of the argument's first word is
load-bearing (76 -> 14) and is a codegen hack, documented in the file.

### `CIOWinManager::RemoveAllIOWins` is `Matching`, and the rename is what did it

**The brief told this lane that `symbols.txt` already carries the retail name, "so objdiff will pair
them and you need no rename". That is wrong, and measurably so.** With the `fn_80049A98` placeholder,
`build/report.json` shows `"address": "0"` and no `fuzzy_match_percent` at all - **0/0, not 99.63%** -
because the call site inside `RemoveAllIOWins` emits the *mangled* name, which the placeholder never
supplied. So the rename was **mandatory**, and with it `RemoveAllIOWins` flips:

```
fn_80049A98 -> RemoveIOWin__13CIOWinManagerFRCQ24rstl15rc_ptr<6CIOWin>
```

**General form, and it is the mirror of the one above:** a symbol already in `symbols.txt` is not the
same as a symbol objdiff can pair. The placeholder `fn_<address>` is a *file name*, and the unit still
scores 0/0 until something emits the name its caller expects. **When a unit scores 0/0 and its file
is right, the rename is the missing half - not the body.**

### The port gained 1, and it is a vtable again

**322 -> 321 undefined, 0 duplicates**, and `link_gap.py`'s list is **unchanged** - because the symbol
that left is a **vtable**, which `nm` cannot see until its key function exists. This is the same
mechanism as `~CIOWin`/`~CMainFlow` earlier, and it is why the two counts differ by a few. The honest
reading: **listing `CIOWinManagerRemoveIOWin.cpp` took `IOWinPQNode`'s vtable off the link**, which
needed the `~IOWinPQNode()` the lane added to `CIOWinManager.hpp`.

### `Carve80193E08` is `Matching` and deliberately **not** in the port build

Listing it makes the probe die *earlier*, inside this function, because **its byte-exact body stores
retail `.data` vtable pointers at 0x803B0D68 and 0x803B5CB0, which are unmapped on the host.** That
is a host artefact, not a defect in the decompilation, so the unit is in `EXCLUDED` with the recipe:
give the port host definitions for both vtables - or better, name the class and give it a real key
function - and it becomes listable. **A `Matching` unit the port cannot run is not progress, and this
is the same trade as `CConsoleOutputWindow` and `CAudioStateWin`.**

**The boot's crash did not move** - still `CGameAllocator::FreeNormalAllocation`, which is the
measured chain in the section above.

## `sizeof(CCubeRenderer)` is 1376 and the header says 860 - 516 bytes short

The headline of the renderer lane is a **header measurement**, and retail supplies both halves of
the proof without any disassembly of ours:

- **`AllocateRenderer` takes `li r3,1376` from the pool** (0x8026EF70). That is the block size, so
  **1376 = 0x560 is the object's size.**
- **The constructor's highest store is `stw r6,1372(r30)`** (0x55C) - four bytes below the end, which
  is what a 1376-byte object with a word member at the top looks like.

`include/MetaRender/CCubeRenderer.hpp` ends at `CVector3f x350_normal`, so `sizeof` is
**0x35C = 860. It is 516 bytes short.** mwcceppc independently agrees with the 0x560 shape
(`sizeof` of the local shape is `00000560` in `.sdata2`).

**This is the same class of error as `CCharacterInfo` (0xF8 vs 0xC0, wrong for four days) and it is
the third time the tree's headers have been measured against retail rather than reasoned about.** The
lane did the right thing: it read the object's own size out of retail's allocator rather than
believing the header, and the header was wrong by 60%.

`fn_80271238` itself is written - `NonMatching` at 3.56% with **1 function paired** (`extern "C"` in a
`.cpp` avoids the anonymous-`.c` 0/0 trap, so objdiff does pair it) - and `unit_fit` reports
`.text ours 1880 vs claimed 1436, over by 444`, plus six extra COMDAT/ctor functions. Two measured
reasons: the header being 516 bytes short, and three `.data` vtables it stores that are **defined as
zeros under `TARGET_PC`, so on the host every `gpRender->` virtual is a jump to 0.** It is not a wall:
`CFrustumPlanes` shows the tree's own prototype mangles to a byte-identical
`__ct__14CFrustumPlanesFRC12CTransform4ffffbf`.

**It is configured but deliberately not listed in `files.cmake`**, because the cost is measured:
listing it takes the port's undefined count **321 -> 331** and gains **0 matched and 0 linked**. It is
still claimed in `splits.txt` and configured in `configure.py`, so objdiff measures it - it is only
withheld from the port build. `src/Kyoto/Graphics/CTexturePortStub.cpp` is port-only and was needed
because `CTexture`'s constructor and destructor are demangled names the probe's self-heal skips, which
left the relink with no binary at all.

### Two corrections to the lane's own report, because both would have cost the next lane time

**`tools/dol_read.py` is not buggy.** The lane reported its `.rodata` file offset as 0x3A27A0 where
the DOL header says 0x3A26C0, and every `.rodata` VA reading 224 bytes late. **The two trees' copies
are byte-identical and already correct** - `(".rodata", 0x803A56C0, 0xB530, 0x3A26C0)`, which is what
`dtk dol info` prints. It misread its own working copy. **A reported bug in a tool is a claim like any
other, and this one would have sent the next lane to "fix" correct code.** With the offset right, the
eight pool names it recovered are believable: `TXTR_BigRing`, `TXTR_DarkWorldCloud`,
`TXTR_ScanSweepBar`, `CMDL_FlatSphere`, `CMDL_FlatSphereLow`, `CMDL_FlatCylinder`,
`CMDL_FlatCylinderLow`, `TXTR_DarkLightworldPalette`.

**And the boot's wall is step 13, not the renderer - which is what the backtrace said.** The lane
found independently that the two remaining stubs, `SetViewPointMatrix` and `SetModelMatrix`, are the
**first two statements of `AddPaksAndFactories()`** - the *first* call in `PostInitialize` - while
`AllocateRenderer` is the *fourth*. **There are zero `[auto-stub]` lines in the run log, so
`AllocateRenderer` and therefore the constructor never ran at all.** That is the same conclusion the
backtrace reached by a different route: `AddPaksAndFactories` -> `CResLoader::AddPakFileAsync` ->
`CMemory::Free` -> `CGameAllocator::FreeNormalAllocation`. **Two independent instruments, one
answer.**

Smaller corrections, so they are not repeated: `0x804173D4` is **`CTransform4f::sIdentity`** in `.bss`
and not a zeroed matrix; `CHECK_SIZEOF(CCubeRendererCtor, 0x560)` fails with mwcceppc's "illegal
constant expression" because the class has a mem-init list, so use a `.data` int; and `p->Ctor()`
compiles under mwcceppc but is rejected by clang as `invalid use of 'CToken::CToken'`, with `<new>`
unavailable to mwcceppc under `-nosyspath -i libc`.

## The allocator crash is fixed, and the boot is 38 reach-stubs deeper

**The boot's depth went from 32 reach-stubs to 70, and the crash moved from
`CGameAllocator::FreeNormalAllocation` to a null deref in `rstl::rbtree_rebalance`.** It was found by
instrumenting the allocator and printing the pointer, not by reasoning about it - and **it was none of
the three candidates I offered.**

The measurement:

```
[FREE] ptr=0x63eca31d3390 first=0x7c9452803040 last=0x7c9453ffef60
       smallMain=0x7c9452803080 smallNumBlocks=0x2c000 heapSize=0x17fbf60
[FREE-OUT-OF-HEAP] bytes[ptr-0x40..): 0b 00 00 00 00 00 00 00 | 4e 6f 41 52 41 4d 2e 70 61 6b 00
                   as text: <"/0000:03:00.0/0000:04:0c.0\0.0\0...">
```

**`ptr` is 21 MB inside the PIE image, not in the heap. `ptr+0` is `[capacity=0x0b][refcount=0]` and
`ptr+8` is `"NoARAM.pak"`** - the `control` block of a `malloc`'d `rstl::string`. The allocator
computed a length of `0x30302f302e30303a` (`"0.000:/0"`) from those bytes and dereferenced it.

**So a crash in `Free` was the allocator working correctly.** The bug was upstream of it, and it is a
host-only defect of exactly the kind this project keeps finding:

1. **`src/rstl/rstl_misc.cpp:55-58` - `rs_new` degraded to host `new[]` on the host.** The
   `CMemory.hpp` `operator new` overloads are `#if __MWERKS__`, so on a PC build every string buffer
   was **host-`malloc`'d** while `internal_dereference` freed it with `CMemory::Free`, which reads a
   retail block header out of it. Now allocated through
   `CMemory::Alloc(size, kHI_None, kSC_Unk1, kTP_Array, CCallStack(-1,"??(??)",0))` - retail's own
   `__nwa__FUlPCcPCc`. Five call sites, four of which hand-pair it with `CMemory::Free`.
2. **`include/Kyoto/Alloc/AllocatorCommon.hpp:112` - a host-width pointer in a size computation.**
   `kAllocatorSmallBlockIndexSize` is now a named `int` = 4 rather than `sizeof(void*)`, which is 8 on
   a 64-bit host and 4 under mwcceppc. The printed proof of the damage:
   `[SMALL] Alloc size=56 ... extent(numBlocks*unit)=0x160000` against a `0xb0000` allocation. **This
   is the second allocator defect here and the same shape as the guard-constant one already fixed -
   a 32-bit quantity in a `size_t` field.** Pinned by `Alloc(0xb0000)`, `Alloc(0x16000)` and
   `CSmallAllocPool(0x2c000, ...)`.
3. **`CGameAllocator`'s `AddPuddle` recursed forever on the host.** The host's `rstl::list` node is
   **64 bytes, over the 56-byte small-pool ceiling**, where retail's is 52 - so `AddPuddle` ->
   `allocate` -> `CMemory::Alloc` -> `Alloc` -> `AddPuddle` looped. A re-entrancy guard removes the
   cycle, and **it is not a weakened check**: it only stops the recursion.

**DOL `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, 86/86 RELs, GATE PASS, port 321 undefined / 0
duplicates** - all unchanged, and **that is the point**: these are `#ifdef TARGET_PC` or no-op changes,
so a fix that moves the boot 38 stubs deep and the DOL not at all is exactly what a host-only port
defect should look like. `matched` and `linked` do not move, and claiming otherwise would be wrong.

### The next blocker, named: `rstl::rbtree_rebalance` on the first factory registration

```
rstl::rbtree_rebalance(void*, void*)
CGameGlobalObjects::AddPaksAndFactories()  +0x1ee
CGameGlobalObjects::PostInitialize(COsContext&, CMemorySys&)  +0x1a
CMain::RsMain(int, char const* const*)
```

Reached by the first `factoryMgr.RegisterFactoryByTypeIdx('STRG', ...)`, and the mechanism is
identified. **`include/rstl/red_black_tree.hpp:241`: the empty-tree insert makes the root with
`mParent == nullptr` and colour red, and returns *without* calling `rebalance`.** Then
`rstl_map.cpp:108`'s loop condition
`while (node->mParent != nullptr && node->mParent->mColor == kNC_Red)` walks to
`mParent->mParent == nullptr` and dereferences it - the faulting `mov (%rdx),%rax`.
**`class header` (`red_black_tree.hpp:50-68`) has no parent or colour member for a root to point at.**

**And one correction to my brief, which named the wrong three functions.** `CGameAllocator`'s
non-`Matching` functions are **`Initialize(COsContext&)` 94.90% (908 B), `Alloc(size_t, EHint, EScope,
EType, const CCallStack&)` 98.90% (884 B) and `FixupAllocPtrs(...)` 95.74% (540 B)** - not
`GetMemInfoFromBlockPtr` or the ctor/dtor pair, all three of which are at 100.00%. The other 22 are
100.00%.

Also worth carrying: **`sizeof(SGameMemInfo)` is 0x40 on the host against 0x20 on retail.** It is
self-consistent - allocation and free both use it - so it is not this crash, **but it halves the block
count in the heap**, and it is the same host-width family as item 2 above.

## The boot now stops for want of *data*, and the rbtree root was red

`rstl::rbtree_rebalance`'s loop guards `node->mParent != nullptr` and then dereferences
`node->mParent->mParent` **unguarded**. `include/rstl/red_black_tree.hpp:241` created the root with
**`kNC_Red`**, so the *second* insert into an empty tree entered that loop and dereferenced the root's
null parent - the `mov (%rdx),%rax` the backtrace showed, on the first
`factoryMgr.RegisterFactoryByTypeIdx('STRG', ...)` of step 13.

**The root is now `kNC_Black`, and the argument is not stylistic.** `rbtree_rebalance` is **retail's
own byte-exact code** - 8 of `node`'s 12 bytes are the parent and colour it reads - so retail cannot
have this bug. **Therefore retail's root is black and ours was red**, and "a root is black" is the
same invariant the red-red fix-up is defined against.

`insert_into` is a **template member**, so it is host-only reimplementation code and **not** one of
the five functions `main/rstl/rstl_map` claims (`rbtree_rotate_left`, `rbtree_rotate_right`,
`rbtree_rebalance`, `rbtree_rebalance_for_erase`, `rbtree_traverse_forward` - all 100.00%). **The DOL
hash is unchanged, which is the proof and not the argument.** Worth stating as a method: *the cheapest
way to settle "is this host-only?" is to change it and read the hash.*

**Effect: 70 -> 103 reach-stubs.**

## Where the boot stops now, and it is not a code defect

```
CSimplePool::GetObj(SObjectTag const&, CVParamTransfer)
CSimplePool::GetObj(char const*, CVParamTransfer)
CSimplePool::GetObj(char const*)
CGameGlobalObjects::LoadStringTable()
CGameGlobalObjects::PostInitialize(COsContext&, CMemorySys&)
CMain::RsMain(int, char const* const*)
```

**`PostInitialize`'s first statement now completes** - `AddPaksAndFactories()`, the one that was
killing the boot - and the third, `LoadStringTable()`, faults inside `CSimplePool::GetObj`. That
statement is `stringTable = gpSimplePool->GetObj(lbl_803A56C0 + 0x146)`: it asks the object pool for
a named string table.

**With no pak loaded there is no such object, so the boot has reached the point where it needs the
retail pak data rather than more code.** That is a materially different stopping place from this
morning's, and it is worth being precise about what it does and does not mean:

- **It does not mean the port is nearly done.** `GetObj` returning nothing is the *correct* answer
  for an empty pool, and retail's own code does not test for it.
- **It does mean the remaining path is short and known**: a frame needs a non-empty object pool, and
  the project's own pattern for that already exists - `port::tweaks::CreateStandInTweakPlayers()`
  supplies real cells for `gpTweakPlayerA`/`B` with **no tweak data behind it**, and says so. A
  stand-in for the string table and the other step-13 objects is the same move.
- **`CMain::LoadStringTable` is named in the objective's own priority list**, so this is squarely the
  right place to be stopped.

**The honest summary of the whole boot arc, in one place:** 32 reach-stubs at the start of this
sequence, **103 now**, across two fixes - a host-width allocator constant plus a host `new[]`/`Free`
mismatch, and a red root in a host-only template. Both were found by instrumenting and printing, not
by reading code, and **both left `main.dol` bit-for-bit identical**, which is what a port defect
should look like. The wall is now data, and the two `GX*` questions from the frame path have not been
reached because `LoadStringTable` comes first.
