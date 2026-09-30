# progress-prime1-cbshurled — `MetroidPrime/BodyState/CBSHurled`

`kind: progress`. The unit stays `NonMatching` in `configure.py`; I did not run `flip_test.sh`
(per the brief) and did not touch `configure.py`.

## Result (measured)

`build/report.json`, before from `build/goal/judge/report.base.json` (the branch head, 9941
matched functions project-wide):

```
unit fuzzy 93.65 -> 99.71 ; matched_functions 6 -> 12 / 13
All:  30.62% fuzzy, 22.74% -> 22.78% matched, 11.74% linked (9941 -> 9947 / 28465 functions)
```

Per function, before% -> after%, and what Prime 1's source did here:

| function | before | after | Prime 1 source |
|---|---|---|---|
| `ShouldStartStrikeWall` | 91.30 | **100.00** | verbatim; needed the `bool` + `if` form instead of a `return a && !b` |
| `ShouldStartLand` | 86.02 | **100.00** | near-verbatim; one edit (see below) |
| `UpdateBody` | 98.05 | **100.00** | Prime 1's shape, one edit (`CRelAngle::FromRadians`) |
| `PlayStrikeWallAnimation` | 89.98 | **100.00** | needed one edit (hoist `db`) |
| `Recover` | 86.52 | **100.00** | needed one edit (hoist `db`) |
| `PlayLandAnimation` | 93.62 | **100.00** | Prime 1's shape verbatim apart from this repo's `SendScriptMsg` 4-arg overload |
| `Start` | 93.31 | 98.95 | Prime 1's shape; **still not 100**, see below |

Six of the seven queued functions went to 100%, and no function anywhere got worse
(`python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json` ->
`matched 9941 -> 9947  linked 4896 -> 4896  (+6 functions at 100%)`, `no regression`).

## The three edits that were not "copy Prime 1"

1. **`ShouldStartLand` - `const CVector3f&`, not `const CVector3f`.** Prime 1 writes
   `CVector3f translation = actor->GetTranslation();`. Under 1.3.2 that copy is elided; under
   2.7 it is materialised: a 12-byte copy on the stack, a bigger frame (64 vs 48) and an extra
   `xxsel vs31,vs1,vs0,vs32` in the prologue - 86.02% went *down* to 89.38% relative to the old
   short-circuit form. Binding a reference instead (`const CVector3f& translation = ...`) is what
   retail actually emits: `addi r3,r31,84` (the address of `mPosition`) rather than a copy.
   The surrounding nested-negation shape from Prime 1 is what produces retail's
   `clrlwi / cntlzw / rlwinm. / bne` for the `close_enough` test; the old
   `if (close_enough(...) && ...)` produced `clrlwi. / beq`.
2. **`UpdateBody` - no early return.** The old `if (state != kAS_Invalid) return state;` compiles
   to `beq` + `b`; retail has a single `bne` and the whole body nested inside
   `if (state == pas::kAS_Invalid) { ... } return state;`. Also `bc.GetFallState() == kFS_Zero ?
   ... : ...` had to become Prime 1's if/else to get retail's
   `cmpwi r3,0 / bne / li r31,5 / b / li r31,2` instead of `li r0,2 / bne / li r0,5 / mr r31,r0`.
   And `CRelAngle angle(mRotateSpeed * dt)` does not compile here - the float ctor is protected
   in this repo's `CRelAngle`, so it is `CRelAngle::FromRadians(...)`, which is byte-identical.
3. **`PlayStrikeWallAnimation` / `Recover` - hoist the database.** Retail calls
   `GetPASDatabase()` *first*, before building the `CPASAnimParmData`; the old
   `bc.GetPASDatabase().FindBestAnimation(...)` called it after, which changed the whole
   callee-saved allocation (`stmw r27` vs five `stw`s, and r27..r30 instead of r28..r31).
   A named `const CPASDatabase& db = bc.GetPASDatabase();` as the first statement is all it took.

Two more Prime 1 details that were needed verbatim and are easy to miss: a named
`CAnimPlaybackParms playParms` / `CPASAnimParm parm0`/`parm1`/`parm3` local. MWCC 2.7 elides
`CAnimPlaybackParms(...).SetCurrentAnimation(...)` and
`GetAnimParmData(...).GetInt32Value()` into the call site, which changes the register allocation
of the whole function; keeping the temporaries as named locals restores it. `flippedAngle` also
has to re-evaluate `CMath::ClampRadians(angle - animAngle)` rather than reuse a `delta` local.

## What still stops `Start` (98.95%)

One thing, and it is only the two `mgr.DeliverScriptMsg(CScriptMsg(...))` calls. The object passed
to `DeliverScriptMsg` is already correct - retail stores `(kInvalidUniqueId, kInvalidUniqueId,
owner.GetUniqueId())` at `+0/+2/+4` and our object stores the same three values in the same three
slots. What differs is (a) which register holds `kInvalidUniqueId` versus the owner's id (retail
loads the SDA constant into r7 first, we load `owner.GetUniqueId()` first) and (b) the offset of
one of the five scratch stores inside the temporary (`temp+8` in retail, `temp+4` here; the four
stores at 40/44/48/52 are at the same offsets in both). About 20 of Start's 272 instructions.
The `CPhysicsActor` guard is already right: retail's `beq` skips both `DeliverScriptMsg` calls
*and* the `SetConstantForceWR` block, so the two messages are inside the cast, not outside it as
in Prime 1.

Spellings tried for `Start` and their scores (do not repeat these):

| variant | score |
|---|---|
| Prime 1 shape (named `parm0`/`parm1`/`playParms`, re-evaluated `ClampRadians`, inline `CScriptMsg(...)`) | **98.95** |
| same, with `CActor* owner = &bc.GetOwner();` instead of `CActor&` | 98.95 (no change) |
| same, plus `const CScriptMsg falling(...); const CScriptMsg jumped(...);` named locals | 93.47 |
| same, plus `const TUniqueId ownerId = owner.GetUniqueId();` hoisted above the cast | 97.32 |
| same, with the 5-arg `CScriptMsg` reduced to 4 args (state defaulted) | does not compile - this repo's `CScriptMsg` ctor has no default for `state` |

Precisely what is left: each `CScriptMsg` is stored twice (a scratch temporary plus the object
handed to `DeliverScriptMsg`), and the five stores into the scratch are
`(kInvalidUniqueId, kInvalidUniqueId, ownerId, ownerId, kInvalidUniqueId)` in retail versus
`(kInvalidUniqueId, ownerId, ownerId, kInvalidUniqueId, kInvalidUniqueId)` here, with the odd
one out at `temp+8` in retail and `temp+4` in ours. The object actually passed to
`DeliverScriptMsg` is already byte-equal.

NEW: progress-cbshurled-start-cscriptmsg | progress | MetroidPrime/BodyState/CBSHurled | Start is at 98.95% and the only remaining diff is the scratch-temporary store order of the two CScriptMsg temporaries in CBSHurled::Start; four source spellings tried (see notes), none reached 100%.

## Verification

All green on this tree, with `MP_TOOLCHAIN_DIR=.../MetroidPrimePort`:

```
sha1sum build/G2ME01/main.dol                 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010   (expected)
./tools/probe_sources.sh                      749 files, 0 failed, 0 errors; link LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py           503 units; 0 declared names are missing from their object
./tools/decomp_build.sh                       All: 30.62% fuzzy, 22.78% matched, 11.74% linked (9947 / 28465 functions)
python3 tools/check_decl_order.py --unit main/MetroidPrime/BodyState/CBSHurled
                                             ok: 1 unit(s) checked, none emits its functions out of retail order
MP_GATE_DOCS_WRITE=0 ./tools/gate.sh build/goal/judge/report.base.json
                                             GATE PASS  1d1797a+2 changed
```

`gate.sh` covers the DOL sha1, all 86 REL hashes against `config/G2ME01/config.yml`, the
per-function report diff, module wiring, docs claims, decl order, `files.cmake` and the port probe.
`tools/unit_fit.sh MetroidPrime/BodyState/CBSHurled.cpp` still reports 14 extra functions
(316 bytes of `.text` over the claimed range, all `__dt__` / inline-virtual COMDAT copies) - that
is unchanged from before my edit, and it is a `match`-item gate, not a `progress`-item one.

## Other notes worth keeping

- The CPASAnimParmData constructor called by these functions is `fn_80079A60` in retail's
  relocations and `__ct__16CPASAnimParmDataFQ23pas15EAnimationStateRC12CPASAnimParmS4_...` in
  ours (it is still a `PortReachStubs` reach stub, not decompiled). **This does not block a
  function reaching 100%**: `fuzzy_match_percent` compares instruction bytes and an unresolved
  `bl` is the same 4 bytes either way. So the five functions that call it are all at 100.00.
- `SDA21` reloc names (`lbl_8041BA20` vs `@777`) are likewise cosmetic in the report; the
  remaining text diffs after 100.00 are reloc-name-only.
- MWCC 2.7 does not elide a named temporary the way 1.3.2 does, and it does not elide a
  `const CVector3f` copy the way 1.3.2 does. Both cost a function here; when porting a Prime 1
  body, expect to *remove* the elision assumptions rather than add them.
- Prime 1's `pas::EHurledState` has `kHS_Six`/`kHS_Seven` where Echoes has
  `kHS_RecoverFromKnockLoop`/`kHS_RecoverFromStrikeWall`. Keep this repo's names; the enum
  values line up because the surrounding instructions matched unchanged.
- `powf` vs Prime 1's `pow` is irrelevant: both emit `bl pow`.
