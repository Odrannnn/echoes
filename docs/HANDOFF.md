# Handoff

Orientation for whoever picks this up next, human or agent. Read this first, then
`docs/RUNNING_THE_DECOMP.md` for how the work is run and `PORT_NOTES.md` for how the port
itself works. This file is the map and the current position; those two are the detail.

## The state, measured

```
matched    10065 / 28465 functions        (31.00% fuzzy, 23.30% of code, 11.78% fully linked)
linked     4918 / 28465 functions        (the one rule's count: the unit is Matching and has a source.)
DOL units  8654 / 16726 functions        (main/*, including the SDK's)
port link  250 undefined, 0 duplicates   (250 since the fifth upstream sync, 2026-09-30, which
                                   closed 11 (CAuxWeapon, the GunController set) and opened 7;
                                   see files.cmake's last block. 254 from 2026-09-29, when retail's CGraphics bring-up was
REL units   1411 / 11739 functions        (the 86 modules, counted as the complement of main/*. A REL unit only counts when its sha1 matches config/G2ME01/config.yml *and* the .rel is cmp-equal to orig/G2ME01/files/RelProd/, so this number is the module count, not an objdiff percentage.)
```

Measured 2026-09-28 on the upstream merge (`PrimeDecomp/echoes` f2dcbf4 taken as the base, our work
re-applied on top). Before the merge master stood at 3980 matched / 2557 linked / 322 undefined;
what the merge gained and the 50 master functions it still does not reproduce are in "The upstream
merge, landed" below. Linked then rose 3498 -> 3525 by flipping three units that were already all-100%
(`CGuiFrameFactory`, `CAnimTreeSingleChild`, `CInt32POINode`); see "The flip pre-pass" below.
The second upstream sync (`PrimeDecomp/echoes` c3537e0) then took matched 8099 -> 8640 and linked
3526 -> 3496, then 3497 by flipping `CStaticGeometryMap`; see "The second upstream sync" below for what was traded and where it is queued.
The fourth sync (`PrimeDecomp/echoes` 750bdca, 2026-09-29: particle-element names from the Remaster
and `CEmitterElement`/`CIntElement` matches) took matched 9117 -> 9120 with linked unchanged and
no function regressing; the other 13 of its 16 "+100%" rows are renames of already-matched functions.
Then 9120 -> 9121 by taking `rstl::vector::clear` back out of line (`include/rstl/vector.hpp`):
upstream's b0934a1 made it `inline`, which dropped `CMoviePlayer::Rewind` to 78% here - retail calls
`clear` out of line (0x80317F98). Then 9188 -> 9189 matched and 4014 -> 4047 linked by flipping
`MetroidPrime/BodyState/CBodyStateCmdMgr` (2026-09-29): an `inline_max_size(127)` pragma, three retail
vtables named in `symbols.txt` so MWLD drops our weak copies, and one stray `.sdata` byte claimed; see
`docs/RUNNING_THE_DECOMP.md`, "An unnamed retail vtable keeps our weak copy alive". Upstream's `CColor(const float, ...)` stays although it scores
`CScriptForgottenObject::RenderInternal` 95.18 -> 88.19: its instructions and relocations are
identical to retail and the REL still hashes, while reverting it costs five Tweaks ctors at 100%.
Then 9188 -> 9189 and linked 4014 -> 4059 by taking `Kyoto/Particles/CColorElement` to
`Matching` 45 / 45; its last short function was the `CCEKEYF::GetValue` the flip pre-pass below
predicted would need different work from its three siblings, and it did - see the row in
`RUNNING_THE_DECOMP.md`. That leaves `CIntElement` as the only one of the four particle elements
still queued.
Then linked 4112 -> 4276, matched unchanged, by flipping seven units that already had every function
at 100% (2026-09-29): `CIOWinManager`, `CWorldLayerState`, `CGameHintInfo`, `CAnimTreeSequence`,
`DolphinCColor`, `DolphinCDvdFile`, `CSfxHandle`. Four of them were template-pool emission order, which
is per-TU fixable after all - see `RUNNING_THE_DECOMP.md`, "An emission-order wall", the superseding note.
Then 4276 -> 4293 by flipping `CPASDatabase` the same way, plus a non-inline forward declaration of
`rstl::destroy_impl(T*)` so its out-of-line copies are weak and deduplicated as retail's were.
Then 4293 -> 4318 by flipping `CFontRenderState` with both techniques.
Then 4318 -> 4342 by flipping `CTextRenderBuffer`, whose only blocker was `align:16` missing on the
next unit's (`CCubeMoviePlayer`) `.rodata` split.
Then 4342 -> 4346 by flipping `CAnimTreeNode`, whose placement-new string had been left unclaimed.
Then 4346 -> 4370: `CStaticAudioPlayer` flipped once `DecodeMonoAndMix` matched (a declaration reorder) and its
template instantiations were placed with the two emission-order techniques. `CStateMachineFactory`
cannot flip alone: its string pool is shared with `Enemies/CStateMachine`, so the two were one TU.
Then 4370 -> 4374 by flipping `CSoundPOINode`: its vtable sat in an unclaimed `.data` blob, so our
strong copy was multiply defined until the split claimed `.data 0x803BBB58..0x803BBB68`.
Then 4374 -> 4446 by flipping `CIntElement`: declaring `CIEParticleCreationTime::GetValue` before
its destructor made `GetValue` the key function, which moved the vtable to where retail has it.
`CParticleGen` is parked: see `RUNNING_THE_DECOMP.md`, the vtable notes near the string-pool paragraph.
Then 4446 -> 4450 by flipping `CABSIdle`: 13 retail weak copies it duplicates were named in
`symbols.txt` so MWLD drops ours. `CTweakAutoMapper` cannot flip: its jumptable is 4-aligned
`.data` (`0x803B822C`), the toolchain limit in "A switch jumptable forces the unit to own the vtable".
Then 4450 -> 4454 by flipping `CFluidPlane` the same way: 4 weak copies (`optional_object<TLockedToken<CTexture>>`
assign, `CFluidUVMotion` copy ctor, its `reserved_vector` copy ctor, `uninitialized_copy_n`) named in `symbols.txt`.
Then 4454 -> 4497 by flipping `CCollisionResponseData`: one weak copy named (`0x800DAC9C`), the three
`vector::assign`s defined in the unit before the constructor, and inline `clear` specialisations (the emission-order recipe).
The fifth sync (`PrimeDecomp/echoes` 03bd14b, 2026-09-30: 15 commits, scaffolds for CAuxWeapon, the
GunController states, CCollisionActor, four ScriptObjects, CIceImpact and CConsoleOutputWindow, plus
matched CFont, CCollisionPrimitive and CCollisionInfo) took matched 9497 -> 9649 and linked 4818 -> 4859,
with no function that was at 100% before dropping. Upstream named four `CAuxWeapon` bodies and
`CConsoleOutputWindow`'s globals that our `Matching` carves referenced as `fn_`/`lbl_`, so the carves now
spell the new mangled names; `rstl::vector<float>::reserve` cannot be spelled as a C identifier, so
`CConsoleOutputWindowCtor.cpp` declares the specialisation instead (see `RUNNING_THE_DECOMP.md`). The port
takes 10 of the 21 new units, chosen by the linker (the block at the end of `files.cmake`); port link
254 -> 250 undefined.

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
byte-identical to `orig/G2ME01/files/RelProd/`, probe 750 files 0 failures, symbol check 0 missing.
byte-identical to `orig/G2ME01/files/RelProd/`, probe 750 files 0 failures, symbol check 0 missing.
(The old form of this line pinned a commit hash, which cannot be written down in the commit thatcreates it.)

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

- **(b') is done — 2026-09-28 (merge worktree).** The 0x50 was *not* a `SLdrTweakPlayerRes`
  modelling gap: `scripts/generate_script_loaders.py` listed five map-icon property ids
  (`0x5096bfa5, 0xf4e6e0eb, 0x65700ccc, 0xa0d73242, 0x5291eb5f`) in
  `SLdrTweakPlayerRes_AutoMapperIcons`, whose real size is nine `rstl::string`s. Retail's
  constructor (`Tweaks.rel` +0x1A600, 124 bytes) initialises offsets 0x00..0x80 and nothing
  else, and its loader (+0x1A28C) reads exactly nine ids; the five belong to
  `SLdrTweakPlayerRes_MapScreenIcons`, which already carries 32. Dropping them makes
  `sizeof(SLdrTweakPlayerRes)` the 0x4F8 claimed above, and `__ct__18`/`__dt__18
  SLdrTweakPlayerResFv`, `__ct__14`/`__dt__14CTweakContentsFv` and
  `DecodeAnyTweak__FUiR12CInputStream` all match. The trailing float also has to be stored
  once: it must **not** also appear in the member-initialiser list, or the constructor writes
  it twice and the offsets come out right while the code does not.

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
| `tools/wire_rel_setup.py` | claims a module's `REL_Setup` tail and names `RELMain`/`RELExit`/`Module*structors`; check the hash after |
| `docs/research/CPatterned_layout.txt` | the constructor's 2904 bytes, every byte in exactly one row |
| `tools/probe_sources.sh` | the port build's **compile and link** sweep (750 files). As of 2026-09-27 it runs the real link and reports the verdict beside the compile count; it used to compile only, which is how a broken link passed the gate || `build/binutils/powerpc-eabi-objdump`, `powerpc-eabi-nm` | disassemble / list symbols |
| `tools/probe_sources.sh` | the port build's **compile and link** sweep (750 files). As of 2026-09-27 it runs the real link and reports the verdict beside the compile count; it used to compile only, which is how a broken link passed the gate || `build/binutils/powerpc-eabi-objdump`, `powerpc-eabi-nm` | disassemble / list symbols |
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

**1. The DOL** - 8028 of 16726 functions (2026-09-29, after `CStateManager`'s `AreaLoaded`,
`AreaUnloaded`, `RayCollideWorld` and `UpdateActorInSortedLists`; the figure
includes the SDK). Verified matches land here steadily, and the two units the whole port was
waiting on are in: `CAi` 11/11 `Matching`; `CPatterned` 28/103 is `NonMatching` since the upstream
merge widened it. Others, measured after the second upstream sync (2026-09-28): `TypesMatch` 503/511,
`CStateManager` 90/239, `CPlayerGun` 68/136, `CPlayerState` 66/72 - see "The second upstream sync"
(the sync's +8 in `CStateManager` from 0x168C up is fixed: `mMapWorldInfo` belongs at 0x167C).
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

**2. The REL modules** - 662 of 11739 functions (2026-09-28), 86 modules. That count is low partly because
claiming a range for a unit *removes* those bytes from the `auto_*` units that match for free -
see "why the matched total can go down" in `RUNNING_THE_DECOMP.md`. **The recipe works and is written
up**: a module may be partly decompiled, with the `Matching` unit claiming only the ranges its
own object reproduces and everything else unclaimed so `dtk` fills it from retail.

**Measure this, never recall it**: `python3 tools/check_module_wiring.py`. As of the last commit it
reports **86 units of our own code in 66 modules** - `AIMannedTurret`, `AtomicAlpha`, `AtomicBeta`, `BacteriaSwarm`, `Blogg`, `DarkCommando`, `DarkSamus`, `DarkTrooper`, `DestructibleBarrier`, `DigitalGuardian`, `ElitePirate`, `EmperorIngStage1`, `EmperorIngStage2Tentacle`, `EmperorIngStage3`, `EyeBall`, `FishCloud`, `FlyerSwarm`, `GeomBlobV2`, `Glowbug`, `Grenchler`, `GunTurret`, `IngBlobSwarm`, `IngPuddle`, `IngSnatchingSwarm`, `IngSpaceJumpGuardian`, `IngSpiderballGuardian`, `Kralee`, `Krocuss`, `MediumIng`, `Metaree`, `MetareeSwarm`, `Metroid`, `MysteryFlyer`, `OctapedeSegment`, `Parasite`, `PillBug`, `PlantScarabSwarm`, `PuddleSpore`, `Puffer`, `Rezbit`, `Ripper`, `RubiksPuzzle`, `SandBoss`, `ScriptCoin`, `ScriptFrontEndDataNetwork`, `ScriptGui`, `ScriptPlayerActor`, `ScriptPlayerProxy`, `ScriptPlayerTurret`, `ScriptRiftPortal`, `ScriptRsfAudio`, `ScriptSafeZone`, `ScriptStreamedMovie`, `Shredder`, `SnakeWeedSwarm`, `SpankWeed`, `Splitter`, `Sporb`, `StoneToad`, `SwampBossStage1`, `SwampBossStage2`, `SwarmBasics`, `Tryclops`, `WallCrawler`, `WallWalker`, `WispTentacle`. `Tweaks` left the list in the third upstream sync (2026-09-29): upstream rewrote both its units (`Tweaks/Tweaks.cpp` and the generated `ScriptLoader/Tweaks.cpp`) and marks them `NonMatching`, and we took that as-is; the module still hashes because `dtk` fills it from retail. `Rezbit` joined on 2026-09-29 with its head at `.text 0x0..0x168` (17 functions); this 73-in-57 figure is superseded by that. `Parasite` (`.text 0x0..0x148`, 17 functions) and `ElitePirate` (`.text 0x0..0x178`, 19 functions) joined the same day, rescued from the goal loop's review queue (see `RUNNING_THE_DECOMP.md`'s rows for them); 77 in 61 is superseded by that. `Splitter` joined after them, also rescued, as **two** units - its head `.text 0x0..0xFC` (15 functions) and, because RELExit/RELMain sit at 0x8210/0x8234 rather than next to the accessors, a separate `CSplitterRelMain.cpp` at `.text 0x81F8..0x82B0` (6); 79 in 63 is superseded by that.
`Metroid` joined 2026-09-29 with its module head, `.text 0x0..0x17C`, eighteen functions - the only
head in this family whose loader record is **0x10 bytes rather than four**, because the DOL's
`OnDockTouch__13CMetroidAlpha` reads words 4..15 of it as a CodeWarrior pointer-to-member-function;
see the `Metroid` row of the Attempted modules table in `RUNNING_THE_DECOMP.md`.
`MetareeSwarm` joined on 2026-09-29 with its module head, `.text 0x0..0xD8`, five functions - see
"`CMetareeSwarmRel` is the module head, and `>> 7` is a 25-bit rotate" in `RUNNING_THE_DECOMP.md`.
`IngPuddle` joined the same day, the same way: its module head, `.text 0x0..0xA8`, five functions -
see "`CIngPuddleRel` is a module head, and a vtable call needs a class" in `RUNNING_THE_DECOMP.md`.
`IngSnatchingSwarm` joined 2026-09-29 as the **second module whose head is IngPuddle's
instruction for instruction**: same 0xA8 bytes, same two vtable accessors at vtable offsets 0x38
and 0x3C, same four-byte `.bss` loader slot, and only the two `bl` targets and the `addi r3,r3,0x1F4`
differ. See "`CIngSnatchingSwarmRel` is `CIngPuddleRel` with the loader import spelled out" in
`RUNNING_THE_DECOMP.md`.
`PlantScarabSwarm` joined the same day too, and its head is **the same 0xD8 bytes as
`MetareeSwarm`'s instruction for instruction** - the same 0xB8-byte record, the same three floats
at +0x0C/+0x1C/+0x2C, the same flag byte at +0xB2, and only the two `bl` targets differ because
each module registers its own loader. That is a fact about the two modules, not a copy of a
source file, and it is why `CPlantScarabSwarmRel.cpp` is `CMetareeSwarmRel.cpp` with the module
number changed.
`AtomicAlpha` joined on 2026-09-29 and is the first of these heads to be **larger than the loader
trio**: its `.text 0x0..0x13C` is 18 functions, because the fourteen-accessor block the REL loader
generator emits at the head of a scripted-actor module comes *before* the trio here. Twelve of those
fourteen accessors are the same bodies `AtomicBetaAccessors.cpp` already reproduces at 100% (same
three DOL relocations - `lbl_8041AAB8`, `kInvalidUniqueId`, `lbl_8041B758`); the other two are
AtomicAlpha's *leading* pair, extra, naming +0x8C8 and +0x7D8 where AtomicBeta opens with the float
store. So the block is **not** byte for byte identical to AtomicBeta's - twelve of fourteen is the
measured number - but no spelling had to be discovered. See "`CAtomicAlphaRel` is a module head, and
twelve of its fourteen accessors are shared" in `RUNNING_THE_DECOMP.md`.
`SnakeWeedSwarm` joined on 2026-09-29 as well, with a head that is *not* shaped like the other
three: `.text 0x0..0xDC`, four functions, and its registration fills a **0x1C-byte** record - an
`FScriptLoader` and two CodeWarrior pointer-to-member-functions, which are 12 bytes each because
`__ptmf_scall` reads three words. The two member-function pointers are copied out of `.data`
verbatim rather than assigned, which is what the six loads and the `stwu` in `fn_71_70` are. See
"`CSnakeWeedSwarmRel` is a module head, and a pmf is 12 bytes" in `RUNNING_THE_DECOMP.md`.
`FishCloud` joined on 2026-09-29 too, and it is the **cheapest of the eight heads**: `.text 0x0..0xAC`,
four functions, and **nothing had to be discovered at all** - no new spelling, no new stand-in class,
and **no locally spelled struct**, because `SFishCloud_FuncPtrs` is already in `ScriptLoaderRel.hpp`.
Its `fn_20_0` is not merely the same CActor `GetHealthInfo` vtable entry SnakeWeedSwarm's `fn_71_0`
is: `diff` of the two disassembly listings is **empty** over all eleven instructions, so the two heads
open with byte-for-byte identical code, and FishCloud stores it in **both** of its vtables
(`lbl_20_data_8` and `lbl_20_data_84`, 0x7C bytes each) so the link cannot dead-strip it. Its
registration fills an **8-byte** record, two `FScriptLoader`s, and it is the only one of these heads
with no member-function pointer in it at all. See "`CFishCloudRel` is a module head, and the header
already had the record" in `RUNNING_THE_DECOMP.md`.
`Tryclops` joined on 2026-09-29 with its module head, `.text 0x0..0x178`, nineteen functions. The
lane landed 0x4C..0x178 believing `fn_81_10`'s `optional_object<CAABox>` return blocked the three
functions below it; it is the MysteryFlyer `fn_45_10` shape (the converting ctor is called out of
line, at 0x4FEC), so the claim was taken to 0x0 when the change was rescued. Its thirteen-accessor
block is AtomicAlpha's (both 0x8C bytes, 35 instructions, identical multiset). See
"`CTryclopsRel` is sixteen functions, and a REL unit's 100% is not the module's verdict" in
`RUNNING_THE_DECOMP.md`.
`IngBlobSwarm` joined on 2026-09-29 with the module head only (5 functions in one `Matching`
unit claiming `.text 0x0..0xD8`); its 53 remaining class functions need the
`CIngBlobSwarm`/`CActor`/`CPatterned` hierarchy and are still retail.
`PillBug` joined on 2026-09-29 with its module head, `.text 0x0..0x130`, seventeen functions: the
loader generator's thirteen-accessor block, `fn_48_90` (a call through CAi's vtable slot 0x38, via a
thirteen-virtual stand-in), and the loader trio. It was rescued from the review queue - the lane's
code was right and only the raw-offsets gate failed, for want of a `raw_offsets.md` section. See
"`CPillBugRel` is a module head, and the accessor block is already in the DOL" in
`RUNNING_THE_DECOMP.md`.
`Blogg` joined on 2026-09-29 with its module head, `.text 0x94..0x108`, three functions: `RELExit`,
`RELMain` and the loader registration `fn_7_D8`. It has no accessor block - `fn_7_0` (0x0, 0x94) is
a `CDamageVulnerability` destructor and stays retail - so the claim starts at 0x94. Rescued from the
review queue: the code was right, and the lane's last attempt failed only because the asm guard in
`tools/goal_check.sh` matched a comment citing `build/G2ME01/asm/...` (fixed on master, 6c2d8d7).
`GeomBlobV2` joined on 2026-09-29 and is the **first head that is not at 0x0 and not the
thirteen-accessor family**: its entry-point block is `.text 0x23E8..0x2490` (four functions -
`fn_25_23E8`, `RELExit`, `RELMain`, the registration `fn_25_2460`) because `fn_25_0` (0x0, 0x1FC) is
a real bone-blend loop, and its accessor block is a **different, much simpler set** - two pointer
getters at `+0x15C` and float accessors at `+0x190` / `+0x198`, naming no DOL global at all. It is
also the first head whose accessor block needed **two** units (`CGeomBlobV2Accessors.cpp` at
`0x2544..0x255C` and `CGeomBlobV2AccessorsTail.cpp` at `0x256C..0x2584`, six accessors with the two
float getters at `0x255C..0x256C` left retail), because two of its eight accessors are not in dtk's
FORCEACTIVE list and are dead-stripped if a unit claims them. See
"`CGeomBlobV2`'s accessor block is six functions in two units, and `unit_fit.sh` said it fit" in
`RUNNING_THE_DECOMP.md`.

`DarkTrooper` joined on 2026-09-29 (goal item `progress-rel-head-darktrooper`) with its module head,
`.text 0x0..0x12C`, **sixteen functions**. Its
thirteen-function block is **PillBug's, re-ordered** and measured as such: this one opens with an
8-byte member-address accessor, moves PillBug's 0x10-byte float store to 0x08, runs one
`li r3,0; blr` predicate in the run where PillBug runs four (3 against 6 over the whole block), and
carries one function PillBug has not -
`fn_12_38`, the `+0x34C` flag bit, which transfers verbatim from AtomicBeta's `fn_5_48` over the
same byte - the same three instructions, `88 03 03 4C / 54 03 EF FE / 4E 80 00 20`. See
"`CDarkTrooperRel` is a module head, and the accessor block is a sibling's in another order" in
`RUNNING_THE_DECOMP.md`.
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

The goal loop's queue was triaged on 2026-09-29, after a measured pass rate of match 9/46 and progress
32/53. It now runs progress items first, then match items by their worst remaining function. The
eleven wall and link-level items are in the review queue, with reasons. The judge now gives an agent
one round to fix a bookkeeping-only `gate.sh` failure and, since 2026-09-30, one round to fix a
change that no longer builds or links (about a third of 88 judged failures on 2026-09-28..30). It
parks a match or progress item whose notes gained a `WALL:` line *in that run* (it used to read the
whole file, so any item with an old `WALL:` was parked after its first failure), sets aside an item
the agent marks `STALE:` (already done at the head) without counting a fail, and closes a match item
whose unit is already Matching without an agent run. Agents run `goal_check.sh` themselves before
stopping, and a retry is told the notes are hypotheses to go past, not verdicts. Agents no longer write the big docs. The judge re-derives their counts
(`check_docs_claims.py --write`), each item's notes land as `docs/goal-notes/<id>.md`, judged
failures do not back off, and an empty queue is refilled by `tools/goal_seed.py`. Nine lanes run
since 2026-09-30, all on the same goal/decomp tip as master: `mp2-goal@1..8` on space-bunny take
only never-failed items, and `mp2-goal@9` is the hard lane (`tools/goal_lanes.sh hard-lane 9
openai/gpt-6-luna`), which takes only items the free model failed once. With `MAX_FAILS` at 2, every
item's second and last attempt is GPT-6 Luna's, reviewed on the free model. The 20 review-queue
items without a wall reason were requeued at fails 1 for it (backups `*.bak-luna-*` in the goal
dir); an item Luna fails goes back to review. The drop-ins are in `~/.config/systemd/user/mp2-goal@*.d/`;
the lanes are started by hand, never enabled at boot. The details are in `RUNNING_THE_DECOMP.md`'s goal-loop section. Re-measure the pass
rate from the lanes' logs before changing the loop again.

**Prime 1 as a source donor (2026-09-29).** Echoes' engine is a fork of Metroid Prime 1's, and
PrimeDecomp/prime has most of those classes Matching; a read-only clone sits at `../prime-ref`. Two
trial items paid: `progress-prime1-cactormodelparticles` gained 14 functions and
`progress-prime1-csortedlists` passed the judge at 11 to 19 of 20 CSortedLists functions (still in
review when written), against about 2 for a typical
progress pass. `tools/goal_seed.py` now seeds these (`--only prime1 --prime1-min-same N`): a
NonMatching DOL unit whose Prime 1 counterpart is Matching, listing its unmatched functions that
share a Prime 1 symbol name, same size first. 52 of them (at least one same-size function each) were
queued that day. Their notes record per function whether the Prime 1 source matched unchanged;
read a batch of them before seeding the remaining 26 zero-same-size units.

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

## The frame-time pair is written; the loop's stop is now `fn_80049244` (2026-09-28, goal item `port-boot-frame0-fn80049244`)

`fn_80006954` (0x80006954, 0x58) and `fn_80008B60` (0x80008B60, 0xC8) both have bodies, in
`src/MetroidPrime/PortFrameTimeHistory.c` - port-only, in `files.cmake`, claiming nothing in the
DOL, because 0x80006954 is inside `MetroidPrime/main.cpp`'s `.text` claim and 0x80008B60 inside
`MetroidPrime/mainTail.cpp`'s, so a carve is two other lanes' cuts. Both `PORT_FRAME_STOP`s in
`CMain::RsMain` are replaced by retail's call on retail's line, and the two 8-byte stack locals
become `SFrameTimeTotal`, stored to `CMain`+0x40 and `CMain`+0x44 as retail does.
`fn_80008B60` is 50 instructions in 200 bytes against retail's 50 in 200, differing only in
relocations; `fn_80006954` is 22 in 88 against retail's 22 in 88, differing in the `bl` displacement
and in the order of the two stores after the call.

**It is a mean, not a sum, and 0x8041A428 is not 200.0.** That constant is
`43300000 80000000` = 2^52 + 2^31, mwcc's integer-to-double bias, and the `fsubs` cancels it and
leaves `count`; the body is `sum * (1.0f / count)`. Written with a `- 200.0f` it is 58 instructions
in 232 bytes, because mwcceppc then emits *two* subtractions of 200 - one against the double
constant and one against the float. So this correction is about **the value being a mean where
earlier passages said sum**, and each of those is annotated in place where it stands:
`docs/RUNNING_THE_DECOMP.md:3237` and `:3241` (the layout table row and the "sums, not a running
minimum" sentence), `docs/RUNNING_THE_DECOMP.md:4117`, `docs/research/boot_path.md:140`, and this
file's own `:2679` and `:2699` from the 2026-09-27 pass - all superseded, none of them about
0x8041A428.

Verified, `./tools/goal_check.sh build/goal/item.json` -> `goal_check: PASS
port-boot-frame0-fn80049244`: `gate.sh` (DOL sha1, 86 RELs, report diff, wiring, docs claims, port
probe), `matched 3980 -> 3980 linked 2557 -> 2557`, `All: 8.52% fuzzy, 7.54% matched, 5.32% linked
(3980 / 28465 functions)`, `2 path(s) changed under src/ or include/`, `port undefined 317 -> 317`,
`probe: (then 659 source) files, 0 failed, 0 errors; link: LINKED (317 undefined, 0 duplicates)`,
`verify boot-progress.sh: BOOT_PROGRESS PASS: all 2 runs got further than all 2 head runs`. The port
probe's file count moved **658 -> 659**, so every "N files" claim in these docs moved with it; the
four historical transcripts keep their own figure, reworded.

**Next wall, and it is the item's own target - a crash, not a declared stop.** `fn_80049244`
(0x80049244, 0x118) SIGSEGVs on frame 1 at `src/MetroidPrime/Carve80049244.cpp:143`
(`win->PreDraw()`), called from `CMain::RsMain` at `PortBoot.cpp:459`. The draw list holds four
IOWins; the two whose constructors are not written - `CConsoleOutputWindow` and `CAudioStateWin` -
have garbage vtable pointers, measured in gdb (node1 vptr `0x555002f239b4`, node3
`0x555002f239e4`, against real vtables `0x5555559bb360` and `0x555556c7b290` for
`CErrorOutputWindow` and `CMainFlow`). Adding the two `configure.py` units to `files.cmake` was
measured and **rejected**: the port's undefined count goes 317 -> 327, and it would not fix the
fault, because both bodies store a **retail PowerPC vtable address** (`lbl_803B37F0`,
`lbl_803B3950`) as the object's vptr. Full evidence in
`build/goal/notes/port-boot-frame0-fn80049244.md`.

## History

Earlier session narratives are in `docs/history/handoff-to-2026-09-29.md`, verbatim, one section per heading below. Their figures are not current.

- The upstream merge, landed (2026-09-28)
- The flip pre-pass: +27 linked with no agent, and the queue rebuilt around near-misses (2026-09-28)
- The second upstream sync (2026-09-28, `upstream/main` c3537e0)
- Where the port is: step 17, and the three functions in front of it
- The carve vein is the cheapest `Matching` in the tree, and it has four traps
- If you are picking this up (2026-09-25, end of session)
- The port links now, as far as it can — measured 2026-09-25
- The pak chain: `CResLoader` is typed, the pump is linked, and `CPakFile` is the wall
- What the six collected lanes added, 2026-09-26 evening
- A latent port link bug that only the shipping configuration could see
- The boot probe now heals its own link, and the stop message tells the truth
- PROVEN: `CErrorOutputWindow` is blocked by the compiler *version*, and retail's own binary proves it
- The named blocking list moved: `InitializeSubsystems` 72.36% -> 97.76%, `FillInAssetIDs` 100%
- PROVEN: `ResetGameState`'s loop is not what blocks it — the object is 4 bytes too big
- SOLVED: the second compiler is already installed, and it is GC/3.0a3 or later
- PROVEN: `CMain::ResetGameState` is blocked, by a chain with a named cause at each link
- `COsContext`'s two words were named for each other's contents
- SUPERSEDED: a second compiler version does **not** unblock `CErrorOutputWindow`
- The `mainTail.cpp` split WORKED - which is the unlock for `CMain::RsMain`
- Step 17 RUNS. The stop was a stale guard, and its own comment said so
- `CMain::RsMain`: the split works, and it is still not worth taking yet
- `AllocateRenderer` is a proven structural wall, and row 21c was wrong about what the pixels are
- The ladder now CALLS step 12, and the boot dies inside it at a named point
- Boot steps 18, 19 and 20 are now in the ladder, and adding them cost zero link symbols
- A first frame IS reachable, and the reason is that Aurora's render-target model is retail's
- The fault moved into real code. The next blocker is GX init, not a missing symbol
- The boot's crash chain, measured - and a lane's inference about it was wrong
- Two adjacent carves, two Matching units, and my brief's claim about renames was wrong
- `sizeof(CCubeRenderer)` is 1376 and the header says 860 - 516 bytes short
- The allocator crash is fixed, and the boot is 38 reach-stubs deeper
- The boot now stops for want of *data*, and the rbtree root was red
- Where the boot stops now, and it is not a code defect
- PROVEN, and it is the answer to "what would unblock a frame": the game's assets are not on this machine
- Session end state, and an honest account of what is reviewed and what is not
- CORRECTION: the port gap in the committed tree is 321, not 313
- CORRECTION: the port gap in the committed tree is 321, not 313
- The next wall, measured: Aurora's `ARInit` faults, and it is not a decompilation problem
- The ARAM wall is cleared, and it was a data symbol stubbed as a function
- `CMain::AsyncIdle`'s last instruction: a proven compiler wall, and my brief was wrong twice
- Two more lanes in, and one carve that is free but could not be wired
- The renderer work measured +2 matched and +2 linked, and I could not land it safely
- Three lanes collected: `matched` 3977, `linked` 2555, port 309 undefined / 0 duplicates
- `sizeof(CMain)` was wrong by 4 bytes, and retail says the right number itself
- The renderer work LANDED: `matched` 3979, `linked` 2556, and the vtable is real
- The constructor now completes with no paks - and the vtable costs ~70 symbols to link
- The port links: 0 undefined, 0 duplicates, and `CMain::RsMain` is on the stack
- `CCubeRenderer::EndScene` is `Matching` 100.00%, and `linked` rose to 2557
- The probe linked nothing and said "0 failed" - and the fix is a REGRESSION gate, not an absolute one
- `dol_read.py` decoded `.data` as little-endian, and the damage is provably zero
- `"DUMB_SnowForces"` was never a pool token, and the next wall needs a real pak
- 7 of 8 paks now load from the real ISO - and my `__fstLoad` diagnosis was wrong in its premise
- `tools/link_closure.sh`: both my premises were false, and the six waves were my own behaviour
- Upstream `PrimeDecomp/echoes` exists, is the same project, and is AHEAD. Reconciliation has begun.
- The upstream merge: what I measured, what I got wrong, and where it stands
- `AddPaksAndFactories` block 7 IS the pump - and the byte-order wall is now the only thing left
- The boot reaches retail's frame loop: two `src/` fixes (2026-09-27, goal item `port-boot-cpakfile-sresinfo-getsize`)
- `LoadTypedefEditorProperties` is defined for real, so one reach stub came out (2026-09-28, goal item `port-loadtypedefeditorprops`)
- Both `LdrToEntityInfo` symbols are defined, so two reach stubs came out (2026-09-28, goal item `port-ldrtoentityinfo`)
- `CModelData::~CModelData()` is defined, so its reach stub came out (2026-09-28, goal item `port-modeldata-dtor`)
- The module manager's update closure has a host body, so the frame loop moved to `fn_8030172C` (2026-09-28)
- The frame loop's DMA cleanup is written, so its stop moved to `fn_80006954` (2026-09-28, goal item `port-boot-cmain-rsmain-0eb92a1`)
