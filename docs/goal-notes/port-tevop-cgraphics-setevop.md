# port-tevop-cgraphics-setevop (port, `CGraphics::SetTevOp`)

## Result

**PASS**, judge run in the worktree, unedited:

```
$ ./tools/goal_check.sh build/goal/item.json
goal_check: item port-tevop-cgraphics-setevop (port) target=CGraphics::SetTevOp
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12193 -> 12193   linked 5860 -> 5860
  ok    check_symbol_names.py
  ok    All:  34.44% fuzzy, 27.69% matched, 12.89% linked (12193 / 28465 functions)
  ok    1 path(s) changed under src/ or include/
  ok    CGraphics::SetTevOp was undefined at the branch head and is not now
  ok    port undefined 289 -> 288
  ok    probe: probe: 751 files, 0 failed, 0 errors; link: LINKED (288 undefined, 0 duplicates)
goal_check: PASS port-tevop-cgraphics-setevop
```

| | before | after |
|---|---|---|
| port undefined symbols (`link_check.sh`) | 289 | **288** |
| DOL `matched_functions` / `linked` | 12193 / 5860 | 12193 / 5860 (unchanged) |
| `main.dol` sha1 | `6ef9b491…` | `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged) |
| port probe | 750 files | **751** files, 0 failed, 0 errors, 0 duplicates |

Set difference of the two undefined lists, **not** a count (`MP_UNDEF_LIST=… python3
tools/link_undef_refs.py`, sorted, against `build/goal/judge/undef.base.txt`):

```
ADDED  : []
REMOVED: ['CGraphics::SetTevOp(ERglTevStage, CTevCombiners::CTevPass const&)']
```

## What I did

One new port-only file plus its one-line registration, and the two generated/derived
docs that follow from it. No header, no `configure.py`, no `splits.txt`, no carve, no asm.

- **`src/Kyoto/Graphics/CGraphicsSetTevOp.cpp`** (new, 12 functions). `CGraphics::SetTevOp`
  and the whole `CTevCombiners` closure behind it — every body read off
  `build/G2ME01/main.elf`, none simplified, none stubbed:

  | function | retail | size |
  |---|---|---|
  | `CGraphics::SetTevOp(ERglTevStage, CTevPass const&)` | `SetTevOp__9CGraphicsF12ERglTevStageRCQ213CTevCombiners8CTevPass` 0x802BFA18 | 0x20 |
  | `CTevCombiners::SetupPass(int, CTevPass const&)` | `fn_802BE47C` | 0x5C |
  | `CTevCombiners::DeletePass(int)` | `fn_802BE4D8` | 0x44 |
  | `CTevCombiners::SetPassCombiners(int, CTevPass const&)` | `fn_802BE44C` | 0x30 |
  | `CTevCombiners::RecomputePasses()` | `fn_802BE588` | 0x40 |
  | `CTevCombiners::CTevPass::Execute(int) const` | `fn_802BE398` | 0xB4 |
  | `CTevCombiners::ColorVar::ColorVar(EColorSrc)` / `AlphaVar::AlphaVar(EAlphaSrc)` | inlined by retail; see below | – |
  | `CTevCombiners::sNextUniquePass`, `sValidPasses[2]`, `sNumEnabledPasses` | small-data statics | – |

- **`files.cmake:665-673`** — the path, with a comment giving the reason.
- **`docs/research/port_link_gap_list.md`** — regenerated with
  `python3 tools/link_gap.py --write-list` (the file's own header calls it generated, and
  `link_gap.py`'s failure message says to run it): one deleted line and the group count
  212 -> 211.
- **`docs/research/port_link_gap.md:109`** — the hand-written table's `other game methods`
  row, 212 -> 211, which `check_docs_claims.py` cross-checks against the generated list.
- **`PORT_NOTES.md:277-288`** — the fourth port-only `CGraphics` file, its contents, the
  measured undefined delta, and what `CTevCombiners::Init` still is.

**Why a carve-out and not `DolphinCGraphics.cpp`.** Retail's `SetTevOp` is in
`src/Kyoto/Graphics/DolphinCGraphics.cpp`, a `configure.py` `NonMatching` unit (22452 bytes of code, 96/102
functions matched) that `files.cmake` EXCLUDES for the reason
`CGraphicsHostStartup.cpp`'s comment gives: four compile errors that are not local to it.
`check_files_cmake.py`'s EXCLUDED accounting is unchanged by this item.

**The closure is closed, which is why the count falls rather than merely holding.** The
five `CTevCombiners` members reach seven `CGX` members, and all seven already had bodies
in the listed `src/Kyoto/Graphics/CGX.cpp` (lines 79, 93, 117, 141, 165, 189, 197). A
one-line `SetTevOp` that forwarded to `SetupPass` alone would have moved the undefined
symbol rather than closing it (+1 `CTevCombiners::SetupPass` for -1 target).

## Measured, not recalled

- `./tools/link_check.sh` — `unique undefined symbols 288`, `duplicate definitions 0`,
  `compile errors 0`. **A first attempt rose to 292** and said so; see the next section.
- `python3 tools/link_gap.py --rebuild` — `283 MISSING`, and after `--write-list`:
  `ok: 283 MISSING symbol(s), all accounted for`.
- `python3 tools/check_docs_claims.py` — `docs claims agree with the tree` (run with
  `--write`, exactly as the judge does, so the `750 -> 751` probe-count claims in
  `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` were rewritten by the tool and then
  reverted: the brief says the driver discards edits to those two files).
- `python3 tools/check_symbol_names.py` — `checked 525 units; 0 declared names are missing`.
- `python3 tools/check_files_cmake.py` — `744 sources; … every configured DOL object is
  either in files.cmake or excluded with a reason`.
- `python3 tools/check_decl_order.py` — `ok: 981 unit(s) checked, 29 permuted, all 29
  accounted for` (nothing to do here; the file is not a `configure.py` unit, so it cannot
  permute a DOL unit's `.text`).
- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, the pinned
  hash. Expected: the new file is not a `configure.py` unit, so mwcceppc never compiles it.
- Standalone host syntax check with the probe's exact flags
  (`-std=c++20 -DTARGET_PC -DAURORA -include platform/compat.h …`) — clean.
- `git status --short` — the five paths above and nothing else. Not committed.

## The first attempt, and what it taught

`link_check.sh` reported `undefined went 291 -> 292` on the first run. The set difference
named all four: `CTevCombiners::AlphaVar::AlphaVar(EAlphaSrc)`,
`CTevCombiners::ColorVar::ColorVar(EColorSrc)`, `CTevCombiners::sNumEnabledPasses` and
`CTevCombiners::sValidPasses`. Two separate causes, both worth writing down:

- **`sValidPasses` / `sNumEnabledPasses` had been written as file-scope statics.** They are
  the *private class members* `include/Kyoto/Graphics/CTevCombiners.hpp:189-190` declares,
  so retail's names are `_ZN13CTevCombiners10sValidPassesE` and
  `_ZN13CTevCombiners15sNumEnabledPassesE` and a file-scope pair with matching C++ names
  mangles to something else entirely. Defined as class members instead. (Access control
  does not apply to a static data member's definition, so `private` is fine.)
- **`ColorVar`/`AlphaVar` have declared constructors and no definition anywhere in the
  tree.** Retail never emits them as symbols: its `CTevPass` constructor at 0x8025AA1C
  inlines both into a flat run of `lwz`/`stw` pairs, so the definitions here are the
  one-field stores that expansion already is.

That disassembly is worth keeping, because it also **pins the `CTevPass` layout** that
`Execute` relies on, field for field: `mId`@0, `ColorVar`×4@4/8/12/16, `AlphaVar`×4@20/24/28/32,
then `CTevOp` twice with the `bool` first at 36 and 56 (`lbz` reads exactly those two bytes)
and four 4-byte enums after it at 40/44/48/52 and 60/64/68/72. 76 = 0x4C bytes, which is
`sizeof(CTevPass)` as the header lays it out — and the same size as retail's
`lbl_80416B48`.

## What I did not do, and why

- **`CTevCombiners::Init` (`fn_802BE51C`, 0x6C bytes) is still undefined.** It is the sixth
  member of this family and `CGraphicsHostStartup.cpp:449` calls it on the boot path, but
  nothing in `SetTevOp`'s closure reaches it, so it is outside this item's diff. Its body
  is now fully determined by what this file defines — see the `NEW:` line below.
- **No doc count outside the two generated/derived ones.** `docs/HANDOFF.md`,
  `docs/RUNNING_THE_DECOMP.md` and `docs/LANE_BRIEFING.md` are the driver's to rewrite.
- **`src/MetroidPrime/PortReachStubs.cpp:1344-1346` still carries `reachstub_429` for
  `SetTevOp`.** It is now a duplicate under `-DMP_BOOT_STUBS=ON`, which only
  `tools/boot_probe.sh` uses, so no gate step sees it and `check_boot_stubs.py` passes
  as it stands. That file says GENERATED / do not hand-edit, so the alias is left for the
  next person who re-runs `tools/gen_link_stubs.py` (there is a precedent for retiring
  one by hand in its own header, for `AllocateRenderer`).
- **`lbl_80416B48`'s upstream name is still unknown.** It is retail's reset pass (see below)
  and nothing in the port can hold its address, so the port names it `sResetPass` and says
  so in the file's header rather than pretending the identity is settled.

## Codegen / port lessons worth keeping

- **The comparison in `SetupPass` is by address, and that is load-bearing.** Retail emits
  `cmplw r4,r0` against `lbl_80416B48` — *the address* — so the reset pass and
  `CGraphics::kEnvPassthru`, which have identical fields, are different objects. A
  value-comparing spelling would silently take the wrong branch.
- **`lbl_80416B48` is identified, not guessed**, from three independent facts:
  `size:0x4C` = `sizeof(CTevPass)`; `CTevCombiners::ResetStates` (0x802BE31C) loads that
  address into r3, zeroes the two valid-pass bytes, then calls `fn_802BE398(r3, 0)` — the
  same call `CTevPass::Execute` is; and it is `.bss`, so every field is zero, which is
  passthrough on every field. `stbx r0,r3,r31` with `r3 = r13-29352` and `r31 = stage` is
  `sValidPasses[stage]`, and 29348 (four earlier) is `sNumEnabledPasses`.
- **Only stage 1 decides the enabled-pass count.** `RecomputePasses`'s `neg`/`or`/`srwi`
  triple is MWCC's `!!` applied to `sValidPasses[1]` alone; stage 0 never enters it.
- **`CGraphics::SetTevOp` needs no enum conversion**, and that is a measurement rather than
  a convenience: retail hands r3 straight to `SetupPass` and each `CTevPass` field straight
  to a `GXTevColorArg`/`GXTevOp`/`…` parameter, which only works because the two enum
  families are identical term for term. Every cast in the file is of an already-correct
  value.

## NEW items

NEW: port-tevcombiners-init | port | fn_802BE51C | retail `CTevCombiners::Init`, 0x802BE51C, 0x6C bytes, and `CGraphicsHostStartup.cpp:449` already calls it on the boot path; its body is `sNumEnabledPasses = 2`, both valid-pass bytes set to 1, `DeletePass(0)`, `DeletePass(1)`, both bytes cleared, `RecomputePasses()` — every callee is now defined in `src/Kyoto/Graphics/CGraphicsSetTevOp.cpp`, so this file plus a one-line rename is the whole change.

**STALE:** nothing. `CGraphics::SetTevOp(ERglTevStage, CTevCombiners::CTevPass const&)` was
line 74 of `build/goal/judge/undef.base.txt` and line 81 of
`docs/research/port_link_baseline.txt` on the clean tree, and it is not in
`build-port-link/link_undefined.txt` now.
## Review rejected run 25 (2026-10-01 22:46:27Z, reviewer worker)

The judge passed this attempt; the reviewer rejected it:

`src/Kyoto/Graphics/CGraphicsSetTevOp.cpp:189-190` mistranscribes retail's `fn_802BE588` by dropping the `addi r3,r3,1` at 0x802BE5A8 — which the file's own comment block quotes at line 108 — so `RecomputePasses` stores and passes 0/1 to `CGX::SetNumTevStages` where retail stores and passes 1/2; that reaches the host through `CGX::SetNumTevStages`/`fn_802BCC74` and leaves the boot path (every `SetTevOp(kTS_Stage1, …)`, and `CTevCombiners::Init` via `CGraphicsHostStartup.cpp:449`) with one fewer TEV stage than retail. `sNumEnabledPasses = 1 + (sValidPasses[1] != 0); CGX::SetNumTevStages(static_cast<uchar>(sNumEnabledPasses));` is the fix. Secondarily, `sResetPass` at lines 181-186 must not take the defaulted `CTevOp()`: retail's `lbl_80416B48` is `.bss`, so both `mClamp` bytes are 0, and the port's `clamp = true` default would be handed to `CGX::SetTev{Color,Alpha}Op` the moment the item's own NEW `CTevCombiners::Init` item lands. Everything else in the diff checks out against `build/G2ME01/main.elf`.

Rejected diff: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/review/port-tevop-cgraphics-setevop-L4-25.patch
Review transcript: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/agent/port-tevop-cgraphics-setevop-L4-25-review1-20261001T224413.jsonl

## Fix round 1

Two corrections in `src/Kyoto/Graphics/CGraphicsSetTevOp.cpp`, both from the rejection above; no
other hunk touched.

1. **`RecomputePasses` (lines 197-201)** - the `addi r3,r3,1` at 0x802BE5A8 was missing, so the
   port stored/passed 0 or 1 where retail stores/passes 1 or 2. Now
   `sNumEnabledPasses = 1 + (sValidPasses[1] != 0);` followed by the unchanged
   `CGX::SetNumTevStages(static_cast< uchar >(sNumEnabledPasses));`. Re-read from
   `build/G2ME01/main.elf` before editing:

       802be598:  88 63 00 01   lbz   r3,1(r3)        ; sValidPasses[1]
       802be59c:  7c 03 00 d0   neg   r0,r3
       802be5a0:  7c 00 1b 78   or    r0,r0,r3
       802be5a4:  54 03 0f fe   srwi  r3,r0,31        ; !!sValidPasses[1]
       802be5a8:  38 63 00 01   addi  r3,r3,1         ; +1
       802be5ac:  54 60 06 3e   clrlwi r0,r3,24
       802be5b0:  90 0d 8d 5c   stw   r0,-29348(r13)  ; sNumEnabledPasses
       802be5b4:  4b ff fa fd   bl    802be0b0 <SetNumTevStages__3CGXFUc>

   `addi` was always quoted correctly in the file's own header block (line 108); the prose under
   it and the code both dropped the `+1`. Both prose updated to say `sValidPasses[1] ? 2 : 1`.

2. **`sResetPass` (lines 184-197)** - both `CTevOp` arguments are now spelled out as
   `CTevOp(kTO_Add, kTB_Zero, kTS_Scale1, false, kTO_Previous)`. Retail's `lbl_80416B48` is
   `.bss` (`config/G2ME01/symbols.txt:19364` = `.bss:0x80416B48; size:0x4C`, and `nm` reports
   `80416b48 B lbl_80416B48`), so `mClamp` at +0x24 and +0x38 is 0. The header's defaulted
   `CTevOp()` has `clamp = true` (`include/Kyoto/Graphics/CTevCombiners.hpp:120`), which would
   have reached `CGX::SetTevColorOp`/`SetTevAlphaOp` as soon as the NEW `CTevCombiners::Init`
   item executes this pass. Every other field's default already equals zero, so `false` is the
   only argument that has to be named.

Verified: `./tools/decomp_build.sh` -> `All: 34.44% fuzzy, 27.69% matched, 12.89% linked
(12193 / 28465)`; `./tools/goal_check.sh build/goal/item.json` -> `PASS`, port undefined
289 -> 288; `python3 tools/check_raw_offsets.py` -> `ok: 167 raw-offset site(s) in 71 file(s)`.
