# carve-801e7c14 - `MetroidPrime/Cameras/Carve801E7C14` (match, lane 10)

**Result: PASS.** `./tools/goal_check.sh <abs>/build/goal/item.json` ->
`goal_check: PASS carve-801e7c14` (gate.sh ok - DOL sha1, 86 RELs, report diff, wiring, docs
claims, port probe; `counts: matched 12599 -> 12602   linked 5960 -> 5963`;
`check_symbol_names.py` ok; `All:  35.46% fuzzy, 29.28% matched, 12.99% linked
(12602 / 28465 functions)`; `flip_test MetroidPrime/Cameras/Carve801E7C14.c: PASS,
Object(Matching) in configure.py`).

## What was done

Wrote 3 unsourced functions in `src/MetroidPrime/Cameras/Carve801E7C14.c`, claiming
`.text 0x801E7C14..0x801E7CB0` (0x9C = 156 bytes) out of dtk's `main/auto_03_801E782C_text` gap.
A carve is four files, all four in this change:

- `configure.py` - `Object(Matching, "MetroidPrime/Cameras/Carve801E7C14.c"),` (one line) between
  `ScriptObjects/Carve801E3E34.c` and `ScriptObjects/Carve801E8AEC.c` (address order).
- `config/G2ME01/splits.txt` - `MetroidPrime/Cameras/Carve801E7C14.c:` /
  `.text start:0x801E7C14 end:0x801E7CB0`, placed after `Cameras/CCameraShakerData.cpp`
  (0x801E70F0..0x801E782C) and before `ScriptObjects/Carve801E8AEC.c` (0x801E8AEC..0x801E8AF4);
  `total_functions` is still **28465**.
- `files.cmake` - `src/MetroidPrime/Cameras/Carve801E7C14.c`, same position.
- `src/MetroidPrime/Cameras/Carve801E7C14.c` - definitions **descending by address**:
  `fn_801E7C58`, `fn_801E7C34`, `fn_801E7C14`.

Two more files, both measured rather than decorative:

- `src/MetroidPrime/PortLinkStubs.cpp` - `stub_188` for `__dt__17CCameraShakerDataFv` plus the
  header's counts (161 -> **162** supplied, 157 -> **158** functions, breakdown's
  CodeWarrior-mangled 1 -> **2**).
- `docs/research/raw_offsets.md` - the new file's one raw offset (`+12`, Kind A) and the summary
  line, re-derived from the tool (see below).

The item's three twins are exact, re-measured here with
`build/binutils/powerpc-eabi-objdump -d --start-address=<a> --stop-address=<b> build/G2ME01/main.elf`.
None of the three touches a data address, so **only the `bl` destination differs from the twin**:

| function | addr | size/insns | twin | body written |
|---|---|---|---|---|
| `fn_801E7C14` | 0x801E7C14 | 0x20 / 8 | `__sys_free` 0x80008A28 0x20, `src/MetroidPrime/main.cpp:396` | `fn_801E7C34(self);` |
| `fn_801E7C34` | 0x801E7C34 | 0x24 / 9 | `destroy_impl<11CTweakValue>...` 0x80006850 0x24, `symbols.txt:131` | `fn_801E7C58(self, -1);` |
| `fn_801E7C58` | 0x801E7C58 | 0x58 / 22 | `__dt__Q212CPlayerState16SPersistentStateFv` 0x80009508 0x58, `symbols.txt:199` | member dtor at `+0xC` with `-1`, then `if (flag > 0) Free(self)`, `return self` |

So the group is a deleting-destructor chain: `destroy`-shaped forwarder -> `destroy_impl`-shaped
step -> the destructive step. `fn_801E7C58`'s flag is a **short** (its bytes test it with
`extsh. r0,r31`, not the `extsb.` a `bool` gives) and `if (flag > 0)` sits **inside** `if (self)`,
so the receiver's `beq` lands on the epilogue; both spellings cost a nibble of one word otherwise.

**What the object at `+0xC` is:** `__dt__17CCameraShakerDataFv` (0x8009D174, 0x70, `scope:weak`,
`symbols.txt:3093`) calls `bl __dt__11CMayaSplineFv` on `addi r3,r30,{0xa0,0x5c,0x18}` - the three
`CMayaSpline` members of `include/MetroidPrime/Cameras/CCameraShakerData.hpp`
(`CHECK_SIZEOF(CCameraShakerData, 0xf4)`), so `+0xC` holds a `CCameraShakerData`. `0xC + 0xF4 =
0x100` plus the byte `fn_801E7CB0` copies at `+0x100` is the 0x104 stride the two loops step by:
`fn_801E7B5C` calls `fn_801E7C14` at 0x801E7BD0, `fn_801E7D88` (0x801E7D88, 0x60) calls it at
0x801E7DB4 in a count-at-+0 / first-element-at-+4 walk, and `fn_801E7EC0` (0x801E7EC0, 0x120,
`CCameraShakeManager`'s add-a-shake entry, `include/MetroidPrime/CCameraShakeManager.hpp:11`)
calls `fn_801E7C58` directly at 0x801E7FAC on the stack temporary at `r1+0xFC` with `li r4,-1`.

## Verification (all measured in this lane's tree, nothing recalled)

- `./tools/decomp_build.sh -r` -> `All:  35.46% fuzzy, 29.28% matched, 12.99% linked
  (12602 / 28465 functions)`.
- `build/report.json`: `main/MetroidPrime/Cameras/Carve801E7C14` is **3 / 3**, all `100.00%`
  (88, 36, 32 bytes). The old gap split cleanly: `main/auto_03_801E782C_text` 30 -> **3**,
  `main/auto_03_801E7CB0_text` **24** (3 + 3 + 24 = 30); `total_functions` still **28465**.
- `./tools/unit_fit.sh MetroidPrime/Cameras/Carve801E7C14.c` -> `.text claimed 156, ours 156,
  retail 156, fits`; `no extra functions`.
- `python3 tools/check_decl_order.py --unit MetroidPrime/Cameras/Carve801E7C14` -> ok (descending).
- `python3 tools/check_symbol_names.py` -> `checked 528 units; 0 declared names are missing`.
- `./tools/carve_diff.sh 0x801E7C14 0x9C build/G2ME01/obj/MetroidPrime/Cameras/Carve801E7C14.o`
  -> `retail: 39 instructions, 156 bytes`, `ours: 39 instructions, 156 bytes`,
  `differing instructions: 4`, and all four are the `bl` sites (unrelocated in a `.o`, which is
  why the tool prints `NOT byte-exact`; the link resolves them and `flip_test` is the verdict).
- `./tools/flip_test.sh MetroidPrime/Cameras/Carve801E7C14.c` -> `PASS -> kept as Matching`,
  `kept: 1 / 1   failed: 0   skipped: 0` (DOL sha1 and all 86 RELs hold with our object in the
  link); `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
- `python3 tools/check_raw_offsets.py` -> `ok: 170 raw-offset site(s) in 74 file(s), all
  documented in raw_offsets.md` (it measured 169 in 73 before this file; the doc's summary line
  read 168 in 72, so it was re-derived rather than incremented).

## The port stub, and why it is needed

`__dt__17CCameraShakerDataFv` is in retail's bytes of `fn_801E7C58`
(`addi r3,r30,0xC / li r4,-1 / bl` at 0x801E7C78..0x801E7C80), so the carve cannot drop the call.
For the DOL nothing is needed - the symbol sits inside `MetroidPrime/TypesMatch.cpp`'s claim
(`.text` 0x800972BC..0x8009D644) and that unit defines it. The **port** does not have it:
`src/MetroidPrime/TypesMatch.cpp` is a deliberate `files.cmake` exclusion
(`tools/check_files_cmake.py:136`; it does not build on the host), and `grep -rn
__dt__17CCameraShakerDataFv src/ include/` matches only this carve. Measured, without the block:
`./tools/link_check.sh --strict` prints `STRICT FAIL - regression gate: 292 undefined against a
baseline of 291 (GREW)`, `0 duplicate(s)`, `0 compile error(s)` and names
`NEW  __dt__17CCameraShakerDataFv`; with `stub_188` in place the same command prints
`unique undefined symbols 291`, `duplicate definitions 0` and
`STRICT PASS - regression gate: 291 undefined against a baseline of 291 (no growth)`. The stub is
an empty stand-in (the `PortLinkStubs.cpp` convention) and is **not** a claim that
`CCameraShakerData`'s destructor is decompiled - it is not.

## Why this item came back, and what differs from the previous attempt

This is a re-run: an earlier attempt on the same tree passed the judge at `337d276f`
(`matched 12595 -> 12598`) and the driver could not carry it onto `3fcf5f2e`
(`build/goal/run.log`: `carve-801e7c14 does not apply on 3fcf5f2 - releasing it for a fresh
attempt`), with `U src/MetroidPrime/PortLinkStubs.cpp` among the conflicts. The reason is visible
in `build/goal/rebase.patch`: that attempt appended its stub as **`stub_187`**, and the commit
that had just landed on the branch head (`3fcf5f2e`, `carve-801b9be0`) appended **its own
`stub_187`** (`fn_801B9C68`) to the same place. This run appends **`stub_188`** after that block,
so the two cannot collide. Everything else is the same change re-derived in the new tree:
the twins, the 0x104 stride, the `+0xC` member and the port measurement were all re-measured
here, not copied.

## Caveats / for the next run

- `gate.sh` rewrites `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` (`MP_GATE_DOCS_WRITE=1`
  in the judge, which re-derives the state block, the module list and the probe count); both
  writes were reverted, so the change is exactly the four carve files plus the stub and the
  raw-offsets entry. `python3 tools/check_docs_claims.py` without `--write` therefore reports the
  HANDOFF state block stale (`matched 12602`, `linked 5963`, `DOL units 11023`, `781 files`) -
  that is the judge's own rewrite, not a claim this change makes.
- Untracked leftovers in `build/` (`build/G2ME01/obj/MetroidPrime/Cameras/Carve801E7C14.o`,
  `build/G2ME01/asm/MetroidPrime/Cameras/Carve801E7C14.s`, `build/G2ME01/asm/
  auto_03_801E7CB0_text.s`) are output of the released attempt; the build regenerates them, and
  the fresh listing for this unit is byte-identical to retail's range.
- Still unsourced and now a port stand-in: `__dt__17CCameraShakerDataFv` (0x8009D174, 0x70). It is
  the deleting-destructor shape of its own class with three `CMayaSpline` member calls, so it is a
  real spelling job, but it is **inside `TypesMatch.cpp`'s claim**, so a carve of it would need
  that unit's cooperation - no `NEW:` is filed for it.

No `WALL:`, no `STALE:`, no `NEW:`.

---

# Re-run, lane 10, base `16744df4` (2026-10-02) - **PASS**, and the one thing that had to change

`./tools/goal_check.sh build/goal/item.json` ->
`goal_check: PASS carve-801e7c14`, all six lines `ok` (no judge-owned path touched; gate.sh;
counts `matched 12610 -> 12613   linked 5971 -> 5974`; check_symbol_names; `All:  35.47% fuzzy,
29.29% matched, 13.00% linked (12613 / 28465 functions)`; `flip_test ...: PASS`).

The carve itself is the same three functions in the same range; every number was re-measured here
(`build/report.json`: `main/MetroidPrime/Cameras/Carve801E7C14` 3/3, 100.00%; the old 30-function
`auto_03_801E782C_text` is now `auto_03_801E782C_text` 3 + mine 3 + `auto_03_801E7CB0_text` 24 =
30; `total_functions` **28465**; `unit_fit` `claimed 156, ours 156, retail 156, fits`, no extra
functions; `carve_diff` 39 instructions / 156 bytes with only the four `bl` sites differing, as
before; `check_decl_order.py --unit ...` ok). Not `STALE:` - the unit did not exist on this base.

**What is different, and it is the whole reason this attempt can land: the port stand-in for
`__dt__17CCameraShakerDataFv` is no longer in `src/MetroidPrime/PortLinkStubs.cpp`.** The last two
attempts were released by the driver's carry, not by the judge: `build/goal/run.log` shows
`carve-801e7c14 does not apply on 3fcf5f2` and again `... does not apply on 16744df`, each time with
`U src/MetroidPrime/PortLinkStubs.cpp` (the docs conflicts are union-merged; a non-docs conflict is
not). The cause is structural: every carve that needs a port stand-in appends it at the same place,
so the patch's context is rewritten by the next lane's commit - item 4 appended `stub_188` after
`stub_187`, and the tip that landed meanwhile had made `stub_188` its own (`fn_801FE7E8` for
`Carve801FFA20.cpp`). The stub now lives **in the carve's own file**, which no other lane touches,
under the same `#ifndef __MWERKS__` port-only pattern `src/MetroidPrime/ScriptObjects/CUnknown90.cpp`
and `src/MetroidPrime/CWorldSaveGameInfo.cpp` use, with a comment saying it is an announced empty
stand-in and is not a claim that `CCameraShakerData`'s destructor is decompiled. `flip_test` and the
DOL are unaffected by construction (`__MWERKS__` is defined by mwcceppc; `nm` of the DOL object has
exactly the three `T fn_801E7C*` and `U __dt__17CCameraShakerDataFv`, and there is no longer an
`stub_189`/header-count edit anywhere). `git status --porcelain` on the judged tree is therefore
`configure.py`, `config/G2ME01/splits.txt`, `files.cmake`, `docs/research/raw_offsets.md` and the new
source - no contended file.

**Port measurement, done twice in this tree, and a trap worth knowing.** `tools/link_check.sh
--strict` with the block: `unique undefined symbols 291`, `duplicate definitions 0`,
`STRICT PASS - regression gate: 291 undefined against a baseline of 291 (no growth), 0
duplicate(s), 0 compile error(s), linker_ran=1`, rc 0; with the block's guard forced to `#if 0`,
the same command prints `292 undefined ... (GREW)` and names `NEW  __dt__17CCameraShakerDataFv`.
The trap: the first invocation after the carve printed `291 ... STRICT PASS` **without building
anything** - ninja had nothing to do, so `link_check.sh` re-derived its verdict from the retained
`build-port-link/build.log` (its own comment explains this). A `--strict` PASS is only evidence when
the object it names has been rebuilt; check `build-port-link/CMakeFiles/mp_game.dir/...` mtimes or
run the failing variant.

**A stale committed baseline list, and why the NEW list is noisy (not this change's doing).**
`docs/research/port_link_baseline.txt` compares by symbol *name*, and its `sym` list predates a
rename wave in the port's sources: it still lists `LoadActorParameters`, `LoadEchoParameters`,
`LoadEditorTransform`, `LoadTypedefSLdrActorParameters`, `LoadTypedefSLdrDamageInfo`,
`LoadTypedefSLdrEchoParameters`, `CAnimData::GetLocatorSegId`, `CGraphics::SetTevOp`,
`SPersistentOptionsValue::SPersistentOptionsValue`, `fn_80145ACC`, `fn_802BE51C` and
`CGameStateEnvVarManager::LoadFields`, while the tree now asks for `LdrToActorParameters`,
`LdrToEchoParameters`, `LdrToTransform4f`, `LoadTypedefActorParameters`, `LoadTypedefDamageInfo`,
`LoadTypedefEchoParameters`, `CBallCamera::SetState`, `CGameCollision::RayStaticLineOfSightTest`,
`CGameLight::CGameLight`, `CGraphics::mModelMatrix`, `CPlayerKnockBackMgr::fn_801C0124` and
`fn_802CB918`. The gate's verdict is the *count* (`undefined 291`, unchanged here), so this is not a
failure - but a run that grows the count today reports twelve renames as "NEW" alongside the real
one. The list is judge-owned (`docs/research/port_link_baseline.txt`, `tools/link_check.sh
--record` is forbidden to lanes), so it is a note, not a fix, and no `NEW:` is filed for it.

Nothing else was touched: `tools/`, `docs/HANDOFF.md`, `docs/RUNNING_THE_DECOMP.md` are back at
HEAD after the gate rewrote them, and `docs/research/raw_offsets.md` now reads **170 sites in 74
files** - re-derived from `python3 tools/check_raw_offsets.py` (which measured 169 in 73 before this
file and printed 168/72 in the prose), with the new file's one site (`+12`, Kind A, opaque
receiver) documented.

No `WALL:`, no `STALE:`, no `NEW:`.
