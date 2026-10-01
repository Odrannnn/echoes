# Where the research lives

Moved verbatim from `docs/HANDOFF.md` on 2026-10-01. One row per file, and the question it answers.

| file | the question it answers |
| --- | --- |
| `docs/research/trilogy_name_pairing.md` | **what the Trilogy disc can and cannot give Echoes**: about 690 unplaced real names, why its code never byte-matches, and what `tools/trilogy_pair_names.py` recovers today (69 proposals, to be checked) |
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
