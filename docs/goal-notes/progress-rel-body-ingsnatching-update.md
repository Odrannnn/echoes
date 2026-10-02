# progress-rel-body-ingsnatching-update (lane 2, 2026-10-02) - DONE

**One new `Matching` unit, `fn_33_348C` at 100.00%, claiming `.text 0x348C..0x35F8` (364 bytes).**
`module:IngSnatchingSwarm` 28 -> 29 of 102 functions; global matched 13098 -> 13099;
`./tools/goal_check.sh build/goal/item.json` = **PASS**.

```
module:IngSnatchingSwarm  28 -> 29 / 102 functions
matched 13098 -> 13099   linked 6196 -> 6197
All:  37.08% fuzzy, 30.52% matched, 13.47% linked (13099 / 28465 functions)
goal_check: PASS progress-rel-body-ingsnatching-update
```

## 0. Re-measured first; the item's `reason` is right, and the range is free

`build/goal/judge/report.base.json` on the clean tree at `c7f9598a`: `module:IngSnatchingSwarm`
**28 / 102**, global matched **13098**. The module's claim layout on this tree is *not* what
run -2's notes left (run -2 and run -4 were both reviewed away), so the contiguous unclaimed runs
were re-listed rather than recalled:

| run | range | functions |
| --- | --- | --- |
| `auto_00_000000A8_text` | 0xA8..0x35F8 | 42 - the class body; **holds `fn_33_348C` (0x16C) as its last member** |
| `auto_00_0000383C_text` | 0x383C..0x3970 | 2 (`fn_33_383C`, `fn_33_3970`) |
| `auto_00_00003AC0_text` | 0x3AC0..0x4F28 | 26 - run -4's `CScriptMsg` dispatcher is at 97.84% and **not landed** |
| `auto_00_00004F44_text` | 0x4F44..0x4FA0 | 3 |

So `fn_33_348C` really is claimable on its own: **0x348C..0x35F8 is a contiguous run holding
exactly this one function**, and 0x35F8 is where `CIngSnatchingSwarmAi.cpp` already claims, so it
needs one line in `splits.txt`, one `Object(...)` and one `files.cmake` line. Nothing above
0x348C was claimed, as the reason says.

## 1. What landed

`src/MetroidPrime/ScriptObjects/CIngSnatchingSwarmUpdate.cpp` (new), `fn_33_348C` only,
**100.00% fuzzy, 1/1 functions**. Carve, all four files:

```
config/G2ME01/rels/IngSnatchingSwarm/splits.txt   one claim, .text 0x348C..0x35F8, before the Ai claim
configure.py            sixth Object(Matching, ...) in the Rel("IngSnatchingSwarm", ...) block
files.cmake             src/MetroidPrime/ScriptObjects/CIngSnatchingSwarmUpdate.cpp
src/MetroidPrime/ScriptObjects/CIngSnatchingSwarmUpdate.cpp   (new, 189 lines / 9.1 kB)
```

**The function is the module's state entry point, and the module's own `.data` says so.**
`auto_04_00000000_data.s` holds a run of 12-byte records `.4byte 0, 0xFFFFFFFF, <fn>`, and the one
at `.data:0x10C` holds `fn_33_348C` - the record immediately after the one at `0x100` that holds
`fn_33_35F8`, the first function of the Ai run. That is why the third argument is a small integer
state and the body ends by sending `kSS_Dead`.

The shape, read off `auto_00_000000A8_text.s` (91 instructions):

| | |
| --- | --- |
| dispatch | `cmpwi r5,1` / `beq` / `bge` then `cmpwi r5,0` / `bge` / `b` - a two-case `switch` |
| state 0 | clear flag bits 24 and 25 of the byte at 0x73C, `fn_33_92C(self)`, `CActor::RemoveMaterial(0x28 = kMT_Target, mgr)`, `SetGeneratorRate(0.0f)` on 0x3E4 and 0x3F4 |
| state 1 | the same two `SetGeneratorRate` calls, then - only if **both** `GetParticleCount()` are 0 - `mgr.DeleteObjectRequest(id)` and `CEntity::SendScriptMsgs(0x44454144 = kSS_Dead, mgr, kInvalidUniqueId, kSM_None)` |
| returns | `void`; anything but 0 or 1 does nothing |

## 2. The three codegen facts this run paid for

**(a) A `CParticleGen` virtual call reproduces retail's slot with the real header - but the real
header cannot be used here.** `lwz r12,0x34(r12)` is slot 13, which is index 11 past the two
leading non-virtual words that `CIngSnatchingSwarmBounds.cpp`'s `AddParticleGen` measurement pins
down, i.e. `CParticleGen::SetGeneratorRate(float)` - hence the rate arriving in f1; `lwz
r12,0x74(r12)` is index 27, `GetParticleCount()`. **Both were measured**: writing the calls
against the real `CParticleGen.hpp` emits exactly retail's 0x34 and 0x74. But `SetGeneratorRate`
has a body (`{}`) in that header, and **mwcceppc emits an out-of-line weak copy of an inline
virtual into every object that mentions it**, so the object grew a four-byte
`SetGeneratorRate__12CParticleGenFf` that the 364-byte claim has no room for:

```
./tools/unit_fit.sh MetroidPrime/ScriptObjects/CIngSnatchingSwarmUpdate.cpp
   extra:    +    4  SetGeneratorRate__12CParticleGenFf
```

and the whole function dropped to **360 bytes / 98.89%**, with the module failing its sha1. The
fix is to declare the table out with **every entry pure** (`CIngSnatchingSwarmParticleGen` in the
new file, all 28 slots, the two used ones named in the real header's order). Pure changes nothing
about the slot numbers, so the two displacements are unchanged and nothing is emitted. Making the
real header's entry pure instead would fix it too but is **not this item's to do**:
`CScriptEffect.cpp:347` and `CHUDBillboardEffect.cpp:26,28` call `SetGeneratorRate`
**non-virtually** on a `CParticleGen*`, so the out-of-line copy has to keep existing and a link
gap would appear instead. General rule: **a stand-in class is required whenever the real class has
an inline virtual body and the claim has no room for its weak copy.**

**(b) A by-value 2-byte argument is spilled twice only when it is written as a functional cast.**
Retail has one `lhz r0,0x8(r30)` and **two** stores of it - `sth r0,0xc(r1)` then
`sth r0,0x10(r1)` - with the argument pointer `addi r4,r1,0x10`; the `r1+0xC` copy is never read
back. Four spellings, all measured this run:

| spelling | score | what is emitted |
| --- | --- | --- |
| `mgr.DeleteObjectRequest(self->x8)` | 98.89% | one `sth`, argument at `r1+0xC` |
| `const TUniqueId id(self->x8); mgr.DeleteObjectRequest(id);` | 99.97% | local at `r1+0x10`, argument at `r1+0xC`, **argument stored first** - the other way round |
| `const TUniqueId& id = self->x8; mgr.DeleteObjectRequest(id);` | 98.89% | one `sth` (the reference binds, no copy) |
| **`mgr.DeleteObjectRequest(TUniqueId(self->x8));`** | **100.00%** | `sth r0,0xc(r1)` then `sth r0,0x10(r1)`, argument at `r1+0x10` |

The functional cast is the only one of the four that reproduces both the slot assignment and the
store order. Copy-initialisation (`const TUniqueId id = self->x8;`) is identical to
parenthesised initialisation.

**(c) The two flag bits at 0x73C clear with `x80 = 0; x40 = 0;` and share one `li r4,0`.** Two
`rlwimi r0,r4,7,24,24` / `rlwimi r0,r4,6,25,25` pairs, in that order, from the byte's first and
second declared 1-bit fields - the same asymmetry run -2's notes record for the reads.

## 3. Verified, all measured on this tree

```
./tools/decomp_build.sh
  87 files OK
  All: 37.08% fuzzy, 30.52% matched, 13.47% linked (13099 / 28465 functions)
sha1sum build/G2ME01/main.dol                                6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
sha1sum build/G2ME01/IngSnatchingSwarm/IngSnatchingSwarm.rel c84839632c931841a91b29e2a30dff53bc6f2408
cmp build/.../IngSnatchingSwarm.rel orig/G2ME01/files/RelProd/IngSnatchingSwarm.rel   IDENTICAL
python3 tools/check_decl_order.py --unit IngSnatchingSwarm/MetroidPrime/ScriptObjects/CIngSnatchingSwarmUpdate
                                                   ok: 1 unit(s) checked, none emits out of retail order
./tools/unit_fit.sh MetroidPrime/ScriptObjects/CIngSnatchingSwarmUpdate.cpp
  .text claimed 364 ours 364 retail 364, fits; no extra functions
  (.data claimed - ours 40 <- not claimed, same as the module's other four units)
python3 tools/audit_rel_claim.py IngSnatchingSwarm
  ok   CIngSnatchingSwarmUpdate.cpp 0x0000348C..0x000035F8  1/1 functions
  0 claim(s) with a problem;  preplf 102 text symbols, plf 102, 0 dropped
./tools/probe_sources.sh            834 files, 0 failed, 0 errors; LINKED (286 undefined, 0 duplicates)
./tools/link_check.sh               286 undefined (= build/goal/judge/undef.base.count), 0 duplicates
python3 tools/check_symbol_names.py  checked 584 units; 0 declared names are missing
python3 tools/check_raw_offsets.py  ok: 176 raw-offset site(s) in 76 file(s)
python3 tools/check_files_cmake.py  every configured DOL object is either in files.cmake or excluded
./tools/goal_check.sh build/goal/item.json    PASS progress-rel-body-ingsnatching-update
```

A per-instruction comparison of our object
(`build/G2ME01/src/MetroidPrime/ScriptObjects/CIngSnatchingSwarmUpdate.o`) against dtk's
`build/G2ME01/IngSnatchingSwarm/asm/MetroidPrime/ScriptObjects/CIngSnatchingSwarmUpdate.s`:
**91 instructions on both sides, identical in every byte except the four `bl` displacements**,
which are relocations the object leaves at 0 (`bl fn_33_92C`, `bl RemoveMaterial__6CActor...`,
`bl DeleteObjectRequest__13CStateManagerF9TUniqueId`,
`bl SendScriptMsgs__7CEntityF18EScriptObjectStateR13CStateManager9TUniqueId20EScriptObjectMessage`)
and the three `lis`/`addi` pairs that carry `lbl_33_rodata_34` and `kInvalidUniqueId` in the
high half. The report's `matched_functions` reads 1/1 and `auto_00_000000A8_text` went 42 -> 41
while the new unit appeared with 1 - a split, not a loss.

**`tools/flip_test.sh` cannot test a REL unit** and says so: it prints `no source file
(extern/musyx/src/MetroidPrime/ScriptObjects/CIngSnatchingSwarmUpdate.cpp)` and FAILs. That is not
specific to this unit - `MetroidPrime/ScriptObjects/CIngSnatchingSwarmAi.cpp`, already `Matching`
and already committed, prints the identical thing. `flip_test.sh` resolves sources under
`extern/musyx/src`, which is the DOL's layout; for a REL the acceptance test is the module's sha1
against `config/G2ME01/config.yml:178`, and that holds byte for byte (`cmp` identical to the
disc file). Run `goal_check.sh` for the verdict, not `flip_test.sh`.

`git checkout -- docs/HANDOFF.md docs/RUNNING_THE_DECOMP.md` after the judge, as run -4's section 0
records: `goal_check.sh` runs `check_docs_claims.py` in write mode and both files show as modified
afterwards. This run's staged content is 10 added lines across three config files plus a
9.1 kB source file.

## 4. What is left in this module

Unclaimed: `auto_00_000000A8_text` 0xA8..0x348C (41 functions - the class body, blocked on the
`CActor` constructor chain at `fn_33_41CC`), `fn_33_383C` 0x383C..0x3970 (2), `fn_33_3970` 0x3970
(run -2's 98.89% wall), `fn_33_3AC0` 0x3AC0..0x4F28 (26, headed by run -4's 97.84% dispatcher),
`0x4F44..0x4FA0` (3) and `fn_33_5078` (1, the module's static initialiser). Nothing new was
measured in them.

## 5. NEW:

NEW: progress-rel-body-ingsnatching-msgdispatch-retry | progress | module:IngSnatchingSwarm | `fn_33_3AC0` (0x3AC0, 0x19C, the module's `CScriptMsg` dispatcher) is still at 97.84% and unclaimed from lane 6's rejection; its whole source and every spelling tried are in `docs/goal-notes/progress-rel-body-ingsnatching-msgdispatch.md`, the only untried idea there being a `CMaterialFilter` whose `exclude` is not a `u64` but a two-`u32` struct or an `rstl::pair` (retail wants the pair loaded **low half first** with the `ori` on the `+0x8` word, and mwcceppc always does high-half-first)
NEW: progress-rel-body-ingsnatching-ctorchain | progress | module:IngSnatchingSwarm | the 41 functions of 0xA8..0x348C are the module's `CActor`-derived class body and the next thing that can raise this module's count; the entry point is `fn_33_A8` (0xA8, 0x5D4 = 1492 bytes, the entity loader: `LoadEditorTransform`, `LoadTypedef*`, `LdrTo*`, `AllocateUniqueId`, `ReadFloat`, `ReadBytes`, and calls `fn_33_744`, `fn_33_41CC`, `fn_33_4DF8`), and the blocker is writing `CActor`'s constructor chain (`fn_33_41CC`, 0x690) - this item confirmed the boundary is real by landing 0x348C..0x35F8 without touching it