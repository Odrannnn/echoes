# progress-rel-body-ingsnatching-ctorchain (lane 4, 2026-10-02) - DONE

**One new `Matching` unit, `CIngSnatchingSwarmState.cpp`, at 100.00% (2/2 functions), claiming
`.text 0x3068..0x31A4` (316 bytes): `fn_33_3068` and `fn_33_312C`.**
`module:IngSnatchingSwarm` 29 -> 31 of 102 functions; global matched 13315 -> 13317;
`./tools/goal_check.sh build/goal/item.json` = **PASS**.

```
module:IngSnatchingSwarm  29 -> 31 / 102 functions
matched 13315 -> 13317   linked 6363 -> 6365
All:  37.36% fuzzy, 30.79% matched, 13.66% linked (13317 / 28465 functions)
goal_check: PASS progress-rel-body-ingsnatching-ctorchain
```

**The item's stated blocker was wrong, and this run proved it.** The reason said the 41 functions of
0xA8..0x348C are "the module's `CActor`-derived class body" and that the entry point is `fn_33_A8`
with the blocker being `CActor`'s constructor chain at `fn_33_41CC`. Neither is true of the two
functions that landed: **`fn_33_3068` and `fn_33_312C` are the module's `DiveToTarget` and
`FollowArcPath` state-table entries**, they are `extern "C"` free functions taking
`(self, CStateManager&, int state)`, and the `CActor` constructor chain at `fn_33_41CC` (which is
**not** in this run at all - it is in the 0x3AC0..0x4F28 run) is nowhere in their bodies. They need
only the offsets. The class-body description applies to the *rest* of 0xA8..0x348C (38 functions
still unclaimed), not to this run.

## 0. Re-measured first

`build/goal/judge/report.base.json` on the clean tree at `fa01a5fb`: `module:IngSnatchingSwarm`
**29 / 102**, global matched **13315**. Unclaimed runs, re-listed from the tree (the layout is as
the previous item's notes left it):

| run | range | functions |
| --- | --- | --- |
| `auto_00_000000A8_text` | 0xA8..0x348C | 41 - the class body |
| `auto_00_000031A4_text` | 0x31A4..0x348C | 1 (`fn_33_31A4`) - only visible after this claim |
| `auto_00_0000383C_text` | 0x383C..0x3970 | 2 |
| `auto_00_00003AC0_text` | 0x3AC0..0x4F28 | 26 (run -4's 97.84% dispatcher) |
| `auto_00_00004F44_text` | 0x4F44..0x4FA0 | 3 |

So 0x3068..0x31A4 was a contiguous two-function run immediately below the already-claimed
0x348C..0x35F8, needing one line in `splits.txt`, one `Object(...)` and one `files.cmake` line.

## 1. What landed

`src/MetroidPrime/ScriptObjects/CIngSnatchingSwarmState.cpp` (new), `fn_33_312C` then `fn_33_3068`,
**100.00% fuzzy, 2/2 functions**. Carve, all four files:

```
config/G2ME01/rels/IngSnatchingSwarm/splits.txt   one claim, .text 0x3068..0x31A4, before the Update claim
configure.py            sixth Object(Matching, ...) in the Rel("IngSnatchingSwarm", ...) block
files.cmake             src/MetroidPrime/ScriptObjects/CIngSnatchingSwarmState.cpp
src/MetroidPrime/ScriptObjects/CIngSnatchingSwarmState.cpp   (new)
```

**The module's own `.data` names both.** `auto_04_00000000_data.s` holds a run of 12-byte records
`.4byte 0, 0xFFFFFFFF, <fn>` at `.data:0x100, 0x10C, 0x118, 0x124, 0x130, 0x13C`, naming
`fn_33_35F8, fn_33_348C, fn_33_31A4, fn_33_312C, fn_33_3068, fn_33_2D24`, and a run of **strings** at
`.data:0x148` - `"Start"`, `"Dead"`, `"ExitPortal"`, `"FollowArcPath"`, `"DiveToTarget"`,
`"SnatchTarget"` - **in the same order**. The previous item used the record table alone; pairing it
with the string table is what names every state entry in this module. So `fn_33_312C` is
`FollowArcPath`, `fn_33_3068` is `DiveToTarget`, `fn_33_31A4` is `ExitPortal`.

## 2. The codegen facts this run paid for

**(a) A one-case `switch` and a one-case `if` are three bytes apart, and retail wants the `switch`.**
`fn_33_3068` dispatches `cmpwi r5,1` / `beq` / `b <end>` - two branches, with the case body laid
out **after** both. A plain `if (state == 1)` emits `cmpwi r5,1` / `bne <end>` and falls into the
body: 97.76%, one branch short. The `switch (state) { case 1: {...} }` spelling reproduces both
branches byte for byte. Measured, both this run.

**(b) An abstract class cannot be a member by value.** `CIngSnatchingSwarmFsm` (the module's
`CGenericFSM2` at 0x380, all entries pure) as a `CIngSnatchingSwarmFsm x380;` member fails to
compile - `illegal use of abstract class`. The member is carried as `char x380[4]` (its vptr) and
reached through an inline accessor that `reinterpret_cast`s, which emits exactly retail's
`addi r3, r31, 0x380` / `lwz r12, 0x380(r31)`. This is the same shape `CIngSnatchingSwarmBounds.cpp`
uses for its `x1C0` sub-object.

**(c) The `CGenericFSM2` slot at 0x30 returns a float.** `fn_33_312C` never loads `f1` before the
virtual call, yet `fn_33_17C8`'s own first floating argument arrives in `f1` untouched - so the
value in `f1` is what the slot returned. That is what pins the entry's signature to
`virtual float Think() = 0;` at index 11. The other twelve slots are read off the module's own
`fn_33_1AF8` (still retail), which drives the same member through `SetupFSM__17CGenericFSM2StateFP
C12CGenericFSM2` and then slots 0x14/0x18/0x1c with this module's three `.data` tables and counts
6/9/6, and slot 0x20 with `(mgr, owner, string)`. **The tree has no `CGenericFSM2` at all** - it
appears only in `config/G2ME01/symbols.txt` as two DOL symbols - so this is a stand-in, as the other
four class-body units in this module already are.

**(d) The offset index is `(vtable_offset / 4) - 2`, i.e. two leading non-virtual words.** 0x30 ->
index 11 for the FSM table, 0x54 -> index 19 for the target table. Carried over from the measurement
`CIngSnatchingSwarmUpdate.cpp` records for `CParticleGen::SetGeneratorRate` (0x34 -> 11).

**(e) `fn_33_1670` is called two different ways and takes `CVector3f*` plus three floats.** Retail's
own `fn_33_2D24` at 0x2F70 passes `(self, &v, self->x170, dt)` and leaves `f3` alone; `fn_33_3068`
passes `(self, &v, self->x170, dt, v.GetX())`. So the declaration needs a defaulted third floating
argument. And the point is copied to a **local** at 0x18 (three `stfs`) rather than passed through
the virtual's own return slot at 0xc, which is why the parameter is `CVector3f*` and not a
reference.

**(f) A float literal in this unit puts a `.rodata` section in the object and moves the module.**
Writing `Range(-1.f, 1.f)` instead of `Range(lbl_33_rodata_54, lbl_33_rodata_30)` made mwcceppc emit
its own 12-byte `.rodata` (`bf800000 3f800000 00000000`) and hoist *that* base into r31 in the
prologue. This is the same rule the three sibling units state, hit from the other direction.

## 3. Verified, all measured on this tree

```
./tools/decomp_build.sh
  87 files OK
  All: 37.36% fuzzy, 30.79% matched, 13.66% linked (13317 / 28465 functions)
sha1sum build/G2ME01/main.dol                                6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
sha1sum build/G2ME01/IngSnatchingSwarm/IngSnatchingSwarm.rel c84839632c931841a91b29e2a30dff53bc6f2408
cmp build/.../IngSnatchingSwarm.rel orig/G2ME01/files/RelProd/IngSnatchingSwarm.rel   IDENTICAL
python3 tools/check_decl_order.py --unit IngSnatchingSwarm/MetroidPrime/ScriptObjects/CIngSnatchingSwarmState
                                                   ok: 1 unit(s) checked, none emits out of retail order
./tools/unit_fit.sh MetroidPrime/ScriptObjects/CIngSnatchingSwarmState.cpp
   .text claimed 316 ours 316 retail 316, fits; no extra functions
   (no .data/.rodata claimed - ours none, same as the module's other units)
python3 tools/audit_rel_claim.py IngSnatchingSwarm
   ok   CIngSnatchingSwarmState.cpp 0x00003068..0x000031A4  2/2 functions
   0 claim(s) with a problem;  preplf 102 text symbols, plf 102, 0 dropped
./tools/probe_sources.sh            859 files, 0 failed, 0 errors; LINKED (286 undefined, 0 duplicates)
python3 tools/check_symbol_names.py  checked 585 units; 0 declared names are missing
python3 tools/check_raw_offsets.py  ok: 185 raw-offset site(s) in 79 file(s)
python3 tools/check_files_cmake.py  every configured DOL object is either in files.cmake or excluded
./tools/goal_check.sh build/goal/item.json    PASS progress-rel-body-ingsnatching-ctorchain
```

`git checkout -- docs/HANDOFF.md docs/RUNNING_THE_DECOMP.md` after the judge: `goal_check.sh` runs
`check_docs_claims.py` in write mode and both files show as modified afterwards.

## 4. What is left in this module

`auto_00_000000A8_text` is down to 38 functions (0xA8..0x3068), `fn_33_31A4` 0x31A4..0x348C (1),
`auto_00_0000383C_text` (2), `auto_00_00003AC0_text` 0x3AC0..0x4F28 (26, headed by run -4's 97.84%
dispatcher), `auto_00_00004F44_text` (3) and `fn_33_5078` (1, the module's static initialiser).

## 5. The measured wall, and one NEW:

`fn_33_31A4` (0x31A4, 0x2E8 = 744 bytes, the `ExitPortal` entry) is decompiled in C++ and sits at
**94.91%** - 740 of our 744 bytes, the same 185 instructions against retail's 186 in the same order
with only register naming differing, plus one instruction. Everything except the constant-address
materialisation is already byte-identical. Seven spellings measured this run, all with the same
shape:

| spelling | score |
| --- | --- |
| `self->x428[self->x424]` indexed three times per point (emits `stfsx`) | 53.69% |
| same, with `CVector3f& p = ...` per point, x54 copied to a local | 64.33% |
| same, plus `CVector3f* const pts` hoisted | 64.33% |
| same, plus `mgr.Random()` hoisted into a local `rnd` | 72.60% |
| **same, `mgr.Random()` called inline (retail recomputes it before each of the 7 `Range` calls)** | **80.77%** |
| same, plus the three `Range` results of each jittered point bound to named locals first | 94.91% |
| same, plus `const float& lo/hi` aliases of the two `.rodata` constants | 94.91% |

Two further things are measured and matter for the next run:
- **A float literal must be a named `.rodata` reference**, and the three jitter values of one point
  must be bound to locals **before** the three `Set` calls - retail computes all three `Range`
  results, then all three adds, then all three stores, which is what forces the two intermediate
  jitter values into callee-saved `f27`/`f28` and takes the frame to `-0xa0` with 8 saved `fprs`.
  Without the named locals the frame is `-0x80` with 7.
- **The residual is one GPR.** Retail keeps `&lbl_33_rodata_54` in `r5` and `&lbl_33_rodata_30` in
  `r4` across each `Range` run and reloads through them (`lfs f1, 0x0(r5)`); ours hoists only one
  address at a time and re-`lis`es the other, costing one net instruction. Both use `r29`=self,
  `r30`=mgr, `r31`=array base identically. Nothing tried in this run changed that.

WALL: fn_33_31A4 94.91% - one instruction and one GPR of register allocation left in the `.rodata` constant-address materialisation; seven source spellings measured, all 94.91% or below.

NEW: progress-rel-body-ingsnatching-exitportal | progress | module:IngSnatchingSwarm | `fn_33_31A4` (0x31A4, 0x2E8, the module's `ExitPortal` state entry - the name comes from pairing `.data:0x118`'s record with `.data:0x148`'s string table) is decompiled in C++ at 94.91% and is the **only** unclaimed function immediately above what this item landed; the whole body is one case-0 block that lays a four-point path into the array at 0x428 from `x3F8`/`x404`/`x180`, six `mgr.Random()->Range(-1,1)` calls, `fn_33_1908` and a final `Range(0, x188)`, and the only thing left is the one GPR of register allocation in how the two `.rodata` constant addresses are held across each `Range` run - the source and all seven spellings with their scores are in `docs/goal-notes/progress-rel-body-ingsnatching-ctorchain.md`
