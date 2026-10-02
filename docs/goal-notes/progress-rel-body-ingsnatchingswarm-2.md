# progress-rel-body-ingsnatchingswarm-2 (lane 10) - DONE

`module:IngSnatchingSwarm` (module 33). **The state-table run landed as one new `Matching` unit,
`.text 0x35F8..0x383C` (580 bytes, 11 functions)** - the ten functions the module's own `.data`
record tables name plus the box-snapshot copier above them. This re-does lane 13's work on the
current tip: that run passed on `a3bbe16`, was carried to `23a843a` and released because
**L13's port probe failed on an unrelated file left in that lane's tree**
(`src/Kyoto/Graphics/CGraphicsGetPerspectirc/Kyoto/Graphics/CCubeMoviePlayer.cpp`, 764 files vs the
763 this tree has). Nothing of that change was in this worktree at `23a843aa` (`git status` clean,
no `CIngSnatchingSwarmAi.cpp`), so the unit was written from the disassembly again, not recovered.

```
tools/fast_try.sh MetroidPrime/ScriptObjects/CIngSnatchingSwarmAi
  IngSnatchingSwarm/.../CIngSnatchingSwarmAi: 100.00% fuzzy, 100.00% matched code, 11/11 functions
sha1sum build/G2ME01/IngSnatchingSwarm/IngSnatchingSwarm.rel
  c84839632c931841a91b29e2a30dff53bc6f2408   == config/G2ME01/config.yml:178
```

## 1. What landed

| function | retail | size | what it is |
| --- | --- | --- | --- |
| `fn_33_37F4` | `.text 0x37F4` | 0x48 | copies the `CAABox` at 0x308 into a 0x1C-byte destination, guarded by the byte at 0x320 |
| `fn_33_37E0` | `.text 0x37E0` | 0x14 | `x3D4 == 2`, as `bool` |
| `fn_33_37AC` | `.text 0x37AC` | 0x34 | 0x73C bit 0x80 && `x1F8 <= .rodata:0x34` (0.0f), bool in a named local |
| `fn_33_3788` | `.text 0x3788` | 0x24 | `x304 >= x420 + x16C` |
| `fn_33_3770` | `.text 0x3770` | 0x18 | `x304 < x420` |
| `fn_33_3708` | `.text 0x3708` | 0x68 | `GetObjectById(x412)`, `TCastToPtr<CPatterned>`, one bit of its 0x420 byte |
| `fn_33_3678` | `.text 0x3678` | 0x90 | squared distance to `GetObjectById(x412)` against `x190` |
| `fn_33_3638` | `.text 0x3638` | 0x40 | `TCastToPtr<CPlayer>(GetObjectById(x410)) != nullptr` |
| `fn_33_3620` | `.text 0x3620` | 0x18 | `x410 == x412` (two `TUniqueId`s) |
| `fn_33_3614` | `.text 0x3614` | 0x0C | 0x73C bit 0x10, returned as `int` |
| `fn_33_35F8` | `.text 0x35F8` | 0x1C | `if (msg == 0) set 0x73C bit 0x40` |

Files: `src/MetroidPrime/ScriptObjects/CIngSnatchingSwarmAi.cpp` (new),
`config/G2ME01/rels/IngSnatchingSwarm/splits.txt` (trailing one claim `.text 0x35F8..0x383C`
between the head and `CIngSnatchingSwarmGenAccessors.cpp`), `configure.py` (fourth
`Object(Matching, ...)` in the `Rel("IngSnatchingSwarm", ...)` block), `files.cmake` (the source
listed next to `CIngSnatchingSwarmGenAccessors.cpp`). `docs/HANDOFF.md` /
`docs/RUNNING_THE_DECOMP.md` show as modified because `goal_check.sh` rewrites the derived counts
(`MP_GATE_DOCS_WRITE=1`); not agent edits.

**The whole run the module's own data names is now claimed**: the ten records at
`.data:0x4..0x70` + 0x100 name `fn_33_37E0..fn_33_35F8`, contiguous 0x35F8..0x37F4, and the claim
reaches 0x383C to include `fn_33_37F4`, the box snapshot the state functions feed. Nothing inside
the range is left retail.

## 2. Verified

```
./tools/fast_try.sh .../CIngSnatchingSwarmAi   100.00% fuzzy, 100.00% matched code, 11/11
sha1sum build/G2ME01/IngSnatchingSwarm/IngSnatchingSwarm.rel
                                               c84839632c931841a91b29e2a30dff53bc6f2408 == config.yml:178
python3 tools/check_decl_order.py --unit IngSnatchingSwarm/MetroidPrime/ScriptObjects/CIngSnatchingSwarmAi
                                               ok: 1 unit(s) checked, none emits out of retail order
./tools/unit_fit.sh MetroidPrime/ScriptObjects/CIngSnatchingSwarmAi.cpp
                                               .text claimed 580 ours 580 retail 580, fits;
                                               no extra functions
python3 tools/audit_rel_claim.py IngSnatchingSwarm
                                               CIngSnatchingSwarmAi.cpp 0x35F8..0x383C 11/11;
                                               0 claim(s) with a problem; preplf 102, plf 102, 0 dropped
python3 tools/check_symbol_names.py            checked 525 units; 0 missing names
```

`unit_fit.sh` also reports a 0x28-byte `.data` in our object that the claim does not cover
(`SolidMaterial` and `@3xx` statics pulled in by the `CPatterned.hpp`/`CPlayer.hpp` include
graph). It is not placed in the module - the sha1 above is the measurement - and the port probe
sees no duplicate from it.

## 3. The bit map, measured (this is where the previous notes mislead)

**mwcceppc 2.7, byte bitfield, field at MSB-first index `i` (i=0 is the byte's 0x80):**

```
store   `s->f = 1`   ->  rlwimi r0, r4, 7-i, 24+i, 24+i
read    `return s->f` -> rlwinm rA, r0, 25+i, 31, 31
```

So the two encodings for one field are **not** the same shift, and only the retail word tells you
which index a function used:

| site | retail word | encoding | index | mask |
| --- | --- | --- | --- | --- |
| `fn_33_35F8` store | `50 80 36 72` | `rlwimi r0,r4,6,25,25` | 1 | 0x40 |
| `fn_33_3614` read | `54 03 E7 FE` | `rlwinm r3,r0,28,31,31` | 3 | 0x10 |
| `fn_33_37AC` read | `54 00 CF FF` | `rlwinm. r0,r0,25,31,31` | **0** | **0x80** |
| `fn_33_3708` read of the target's 0x420 | `54 03 D7 FE` | `rlwinm r3,r0,26,31,31` | 1 | 0x40 |
| `fn_33_39C0/3A34/3A60` read (still retail, word from the queued item's measurement) | `rlwinm. r0,r0,30,31,31` | `rlwinm. r0,r0,30,31,31` | 5 | 0x04 |

**dtk's `extrwi` line prints a different bit from the encoding and it cost this run one build.**
`fn_33_37AC`'s line reads `extrwi. r0, r0, 1, 24`, which looks like `rlwinm r0,r0,25,31,31`; it is
`rlwinm r0,r0,25,31,31` only if you read `b` as `SH-1`, and my first attempt took the printed `24`
for index 5 (`rlwinm. r0,r0,30,31,31`) and scored **99.62% on that one function, 10/11**. The
previous notes' "`fn_33_35F8`'s store and `fn_33_37AC`'s read are spelled x40 and x80" is true in
the *sense that matters* - the store is index 1 (0x40) and the read is index 0 (0x80) - but its
parenthetical "(bit 25, the byte's 0x40)" labels both as bit 25 and would put the read one field
too far along. Read the raw word or `objdump`, never the dtk `.s` line.

**A second measured trap, worth its own line: the 0xC-byte hole between 0x414 and 0x420.**
`TUniqueId` is 2 bytes (`TGameTypes.hpp:56`, `CHECK_SIZEOF(TUniqueId, 0x2)`), so a stand-in class
with `TUniqueId x410; TUniqueId x412; float x420;` puts the float at 0x414 and **every member from
there up shifts 12 bytes down**, including the 0x73C bitfield (which lands at 0x730). The unit
still compiles, still links, and scores 99.5-99.9% on five functions with the bit tests reading
correctly and only the offsets wrong; the fix is `char x_pad[0x420 - 0x414];`.

Two more facts this run confirmed, both from the previous notes and both true:

- **An early return is not a tail return.** `fn_33_3708`'s `li r3,1` block sits *before* its
  `li r3,0` block (retail), which is the nested-if spelling
  `if (p != nullptr) { return <expr>; } return false;`; the early-return spelling emits
  `bne`+`li r3,0`+`b` and is a different 6 bytes.
- **A 1-bit field returned as `bool` costs `neg`/`or`/`srwi`.** `fn_33_3614` returns `int` (three
  instructions fewer, matching retail) while `fn_33_3708` returns the field as `bool` and keeps
  them - both spellings are in retail, in the same unit.

The loop that found all of the above, and the one to use for the next range: `fast_try.sh` prints
the per-function percentage, and one `objdump -d --no-show-raw-insn` of
`build/G2ME01/IngSnatchingSwarm/obj/MetroidPrime/ScriptObjects/<unit>.o` beside
`build/G2ME01/src/.../<unit>.o` names the differing instruction in one line.

## 4. What is left - the next contiguous range, 0x383C..0x3AC0 (6 functions)

Everything the next run needs is measured and unclaimed; nothing in the range was attempted here
because the lane's budget went on re-doing 0x35F8..0x383C and on the gates.

- `fn_33_39B8` (0x8): `addi r3,r3,0x1c0 / blr` -> `return (char*)self + 0x1C0;`. A one-liner.
- `fn_33_3970` (0x48): `out` in r3, object in r4, scale in f1:
  `out->x = o->0x54 + o->0x358*f1*o->0x28`, same for 0x58/0x38 and 0x5c/0x48.
- `fn_33_39C0` (0x74): tests **0x73C index 5 (0x04)** then calls `gpRender`'s vtable **slot 0x44**
  twice with `self->0x3E4` and `self->0x3F4`. Slot 0x44 is
  `CCubeRenderer::AddParticleGen(const CParticleGen&)` - `CParticleGenInfoGeneric.cpp:25` spells
  the same call `gpRender->AddParticleGen(*mSystem.GetPtr())` - and retail loads r4 straight from
  0x3E4, i.e. the pointer is passed as the reference.
- `fn_33_3A34` (0x2C): the same bit, then `PreRender__6CActorFR13CStateManager(self, mgr)`.
- `fn_33_3A60` (0x60): the same bit, then `Render__6CActorCFRC13CStateManager(self, mgr)`, then
  vtable **slot 0x10** on 0x3F4 and 0x3E4 (`CParticleGen::Render()`, index 2, no argument).
- `fn_33_383C` (0x134, the big one): `TCastToPtr<CPhysicsActor>` called with the **reference**
  overload - the import is `TCastToPtr<13CPhysicsActor>__FR7CEntity` - tests `self->0x1F0`'s byte
  **index 1 (0x40, `cmplwi r0,0x1`)**, `TCastToPtr<CPlayer>`, then `phys->GetBoundingBox()` into a
  stack box, builds a `CAABox` from `self->0x54/0x58/0x5C` **- `lbl_33_rodata_30` (1.0f) as min and
  + it as max** (`__ct__6CAABoxFRC9CVector3fRC9CVector3f`), `DoBoundsOverlap` against the
  GetBoundingBox result, then `TCastToPtr<CCollisionActor>` and stores `cast->0x2D4` (or
  `entity->0x8` when the cast fails) into `self->0x410`.

All twelve imports those six need are already in
`build/G2ME01/IngSnatchingSwarm/IngSnatchingSwarm.preplf` (`__ct__6CAABoxFRC9CVector3fRC9CVector3f`,
`AccumulateBounds__6CAABoxFRC9CVector3f`, `TCastToPtr<10CPatterned>__FP7CEntity`,
`GetObjectById__13CStateManagerCF9TUniqueId`, `TCastToPtr<7CPlayer>__FP7CEntity`,
`TCastToPtr<13CPhysicsActor>__FR7CEntity`, `GetBoundingBox__13CPhysicsActorCFv`,
`DoBoundsOverlap__6CAABoxCFRC6CAABox`, `TCastToPtr<15CCollisionActor>__FR7CEntity`, `gpRender`,
`PreRender__6CActorFR13CStateManager`, `Render__6CActorCFRC13CStateManager`), so no `symbols.txt`
rename is needed and the DOL does not move.

## 5. NEW:

NEW: progress-rel-body-ingsnatchingswarm-3 | progress | module:IngSnatchingSwarm | the claim is now 0x35F8..0x383C with 11/11 at 100.00% in `src/MetroidPrime/ScriptObjects/CIngSnatchingSwarmAi.cpp`; the next contiguous range is 0x383C..0x3AC0 - `fn_33_383C` (0x134), `fn_33_3970` (0x48), `fn_33_39B8` (0x8), `fn_33_39C0` (0x74), `fn_33_3A34` (0x2C), `fn_33_3A60` (0x60) - section 4 above has every body, the bit indices and the import list, so no re-measuring is needed

## 6. Judge, on this tree

```
./tools/goal_check.sh build/goal/item.json          PASS progress-rel-body-ingsnatchingswarm-2
  ok  no judge-owned path touched
  ok  gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok  counts: matched 12519 -> 12530   linked 5892 -> 5903
  ok  check_symbol_names.py
  ok  All:  35.34% fuzzy, 29.18% matched, 12.92% linked (12530 / 28465 functions)
  ok  target rose: module:IngSnatchingSwarm: 13 -> 24 / 102 functions
  ok  no asm added
```

One process note, not an item problem: a `build/goal/check.out` in this worktree was overwritten
mid-run with another lane's judge output (L3's `progress-cgamestate-worldstate-arg`, 12:02). The run
above was re-done and its output copied over that file; nothing judge-owned is touched by the
change itself (`git status` lists only `config/G2ME01/rels/IngSnatchingSwarm/splits.txt`,
`configure.py`, `files.cmake`, the new `src/.../CIngSnatchingSwarmAi.cpp`, and the two docs
`goal_check.sh` rewrites itself under `MP_GATE_DOCS_WRITE=1`).
