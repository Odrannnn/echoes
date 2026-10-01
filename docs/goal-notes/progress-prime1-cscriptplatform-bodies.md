# progress-prime1-cscriptplatform-bodies

`MetroidPrime/ScriptObjects/CScriptPlatform` — `progress` item, unit stays `NonMatching`.

## Result

| | before | after |
|---|---|---|
| unit `matched_functions` | **42** / 60 | **44** / 60 |
| unit `matched_code` | 5208 B | **5352 B** (of 18000) |
| unit `fuzzy_match_percent` | 40.82% | **44.94%** |
| tree `matched_functions` | 11350 / 28465 | **11352** / 28465 |
| `linked` | 5507 | 5507 (unchanged) |
| `DecayRiders` | 1.33% / 0 B | **99.51%** / 300 B (does not count; see the wall) |

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11350 -> 11352   linked 5507 -> 5507
  ok    check_symbol_names.py
  ok    All:  32.64% fuzzy, 25.39% matched, 11.94% linked (11352 / 28465 functions)
  ok    target rose: main/MetroidPrime/ScriptObjects/CScriptPlatform: 42 -> 44 / 60 functions
  ok    no asm added
goal_check: PASS progress-prime1-cscriptplatform-bodies
```

Two new functions at 100%: **`UpdateSlaveTransforms__15CScriptPlatformFR13CStateManager`** (0x800A1250,
224 B) and **`fn_800a0200__15CScriptPlatformFfR13CStateManager`** (0x800A0200, 232 B). Nothing in
the unit or the tree went below a score it had.

## Re-measure first

`reason` quoted the previous run's state (40/60). The tree was already at **42/60**: two later items
(`...-erase-pair`, `...-callers`) had landed `fn_800A359C` and `AddSlave`. Everything `reason` named
as blocked was still blocked, so the TODO bodies were the work, as it says.

Retail bodies were read out of `build/G2ME01/obj/MetroidPrime/ScriptObjects/CScriptPlatform.o` with
`objdump -d -r` (the relocation at each `bl` names the callee). Loop: `./tools/fast_try.sh
MetroidPrime/ScriptObjects/CScriptPlatform` (~3 s) plus `./tools/bytescmp.py <obj> <sym> <addr>
<size>`. **Prime 1's own decomp is on disk** at `../../prime-ref` (relative to this worktree) and is
the fastest route into `DecayRiders` / `MoveRiders` / `PreThink` / `Move` / `AcceptScriptMsg`; it
was not mentioned in any earlier note and it is what made this run's work go quickly.

## `UpdateSlaveTransforms` — 0.00% -> 100.00%

```c
CTransform4f invXf = GetTransform().GetQuickInverse();
for (SRiders* it = mDynamicSlaves.mItems;
     it != mDynamicSlaves.mItems + mDynamicSlaves.mCount; ++it) {
  if (CActor* actor = TCastToPtr< CActor >(mgr.ObjectById(it->mUid))) {
    CTransform4f xf = invXf * actor->GetTransform();
    it->mTransform = xf;
    if (CScriptPlatform* platform = TCastToPtr< CScriptPlatform >(actor)) {
      platform->UpdateSlaveTransforms(mgr);
    }
  }
}
```

Two things decide it, and both are "the temporary needs a **name**":

- the quick inverse is copied (`__ct__12CTransform4f`, 0x800A1288) into `r1+156` while the call's own
  return slot stays at `r1+60`, and that copy is **before** the loop, so it is a hoisted local;
- the `__ml__` product lands at `r1+12`, is copied to `r1+108`, and `__as__` assigns from `r1+108`,
  so the product is a second named local too. `it->mTransform = invXf * actor->GetTransform();`
  assigns straight out of the return slot and loses both copies.

`tools/bytescmp.py` on the kept body reports 9 differing instructions of 56, and all 9 are the `bl`
relocation fields (objdiff resolves those by symbol name); the score is 100.00%.

`TCastToPtr< CScriptPlatform >` is already defined (`src/MetroidPrime/TypesMatch.cpp:762`,
`CAST_TO_IMPL(CScriptPlatform, kET_ScriptPlatform)`), so this adds no undefined symbol.

## `fn_800a0200` — 0.00% -> 100.00%

```c
CTransform4f xf = mInitialTransform;
xf.SetTranslation(GetTranslation());
SetTransform(xf);
mPreviousRotation = xf.GetRotation();
mPreviousRotation.Orthonormalize();
mCurrentRotation = mPreviousRotation;
if (!mSplineController.null()) {
  float dur = mSplineController->PositionTimeSpline().GetDuration();
  SetMotionTime(0.f > time ? 0.f : (dur < time ? dur : time), mgr);
}
```

- the rotation copy is `mPreviousRotation` (0x308) **first** and `mCurrentRotation` (0x338) second —
  the opposite of the declaration order, and it is the whole of the difference between a 91% and a
  100% body;
- the clamp is **one expression**, not two statements: there is a single `bl SetMotionTime` and the
  outgoing float is computed into `f1` (`fmr f1,f0` at 0x800A02a8, `fmr f1,f31` at 0x800A02bc), which
  is what a nested ternary produces. Two `if`s that assign `time` put the value in `f31` instead and
  the function is **one instruction** short;
- `SetMotionTime` is **inside** the null test: `beq c8` at 0x800A008c goes to the epilogue, so with a
  null controller retail never calls it;
- `dur < time` must keep that operand order. `time > dur` compiles to `fcmpo cr0,f31,f1` where
  retail has `fcmpo cr0,f1,f31`, and the score drops to **99.74%** (one instruction).

Spelling, all measured this run on this body:

| spelling | score |
|---|---|
| `SetMotionTime(0.f > time ? 0.f : (dur < time ? dur : time), mgr)` (kept) | **100.00%** |
| `... : (time > dur ? dur : time)` | 99.74% |
| `... : (dur >= time ? time : dur)` | 96.38% |
| `... : (time <= dur ? time : dur)` | 96.21% |
| two `if`s assigning `time`, then `SetMotionTime(time, mgr)` in both arms | 91.29% |
| two `if`s assigning `time`, `SetMotionTime` after the `if` | 91.03% |
| the `time < 0.f` spelling of the first clamp | 90.78% |
| the `dur` clamp dropped | 89.22% |
| `GetDuration()` called and discarded, `float dur = time` | 91.72% |
| the value bound to a named local first | 96.38% |

`CGameSpline::PositionTimeSpline()`, `CGameSpline::GetPositionSpline()` and `CMayaSpline::GetDuration()`
are all already used by `src/MetroidPrime/Cameras/CPathCamera.cpp` and
`src/MetroidPrime/ScriptObjects/CScriptEffect.cpp`, so this adds no undefined symbol either.

## `DecayRiders` — 1.33% -> 99.51% (does not count)

```c
rstl::vector< SRiders >::iterator it = riders.begin();
while (it != riders.end()) {
  if ((*it).mDecayTimer.valid()) {
    (*it).mDecayTimer.data() -= dt;
    if ((*it).mDecayTimer.data() <= 0.f) {
      TUniqueId riderId = (*it).mUid;
      it = fn_800A1004(riders, it);
      mgr.DeliverScriptMsg(CScriptMsg(kInvalidUniqueId, kInvalidUniqueId, riderId,
                                      static_cast< EScriptObjectMessage >(0x584f4e50),
                                      kSS_InvalidState));
      continue;
    }
    ++it;
  } else {
    ++it;
  }
}
```

Three measured facts, all of them the previous run's `WALL`-free zone:

- **`(*it)` and not `it->`.** With the arrow spelling MWCC loads the loop cursor into r6 and the
  body is 20 bytes short: **89.57%** against **94.11%**. Nothing else differs.
- **The `++it` must appear in both arms.** Retail has two copies of the three-instruction increment,
  0x800A3754 (after `cror eq,lt,eq; bne`) and 0x800A3764 (after `cmplwi r0,0; beq`), and only an
  if/else with `++it` in each arm produces two. A single trailing `++it` gives 94.11%; the if/else
  gives **99.51%**. Same trap as the constructor's two `stw`s: retail's source *did* write it twice.
- **The rider's id is a local read before the erase** (`lhz r5,0(r3)` -> `sth r5,24(r1)` at
  0x800A36e4, reloaded with `lhz r6,24(r1)` at 0x800A370c after the call), so the `CScriptMsg` is
  built from that local rather than from the erased element.

`fn_800A1004` is defined at the bottom of this file, so a **forward declaration** had to be added next
to the existing `fn_800A14DC` one (line 51). Without it the body does not compile — and a failed
`fast_try` is easy to miss, because `fast_try.sh` redirects ninja to `/dev/null` and the report it then
prints is the **previous** build's.

What is left is 7 instructions, all inside the two `CScriptMsg` store blocks, and it is the same wall
the previous run hit on `AddRider(vector)`: retail has r7 = `kInvalidUniqueId` and r6 = the rider id,
we have r6 and r7 exchanged, and the four *dead* frame stores at `r1+8..r1+20` (the `CScriptMsg`
argument scratch MWCC emits) put the rider id in slot 3 where we put it in slot 2. Same registers, same
stores, same offsets — only the assignment. 15 spellings measured:

| spelling | score |
|---|---|
| the kept body | **99.51%** |
| the message bound to a named `CScriptMsg` local first | 99.51% |
| `const TUniqueId riderId`, `TUniqueId riderId((*it).mUid)`, `static_cast<EScriptObjectState>(0xffffffff)` | 99.51% |
| the message built by a `static inline` helper | 99.49% |
| `!(value > 0.f)` instead of `value <= 0.f` | 98.11% |
| `kInvalidUniqueId` hoisted into a local | 94.07% |
| `CScriptMsg(kInvalidUniqueId, riderId, kInvalidUniqueId, ...)` | 94.47% |
| `0x584f4e50` written as `'X'<<16 \| 'O'<<8 \| 'N' \| 'P'` | 94.08% |
| the rider id read before the timer test | 92.44% |
| `CScriptMsg(TUniqueId(), TUniqueId(), riderId, ...)` | 92.44% |
| fields assigned one by one through `SetMessage` | 81.16% |
| `if (value > 0.f) { ++it; } else { ... }` | 87.31% |
| `if (!valid) { ++it; continue; }` then a flat `++it` | 88.71% |

WALL: DecayRiders 99.51% - the last 7 differing instructions are the r6/r7 exchange inside the two
CScriptMsg store blocks and the slot the dead argument scratch gives the rider id; 15 spellings
measured this run, best is 7 differing instructions.

## Two things that will be needed again (not `NEW:` lines)

**The bitfield encoding in this class, measured.** `mwcceppc` writes a 1-bit field as
`lbz rA,B ; rlwimi rA,rV,SH,MB,MB ; stb rA,B` and tests one as `lbz r0,B ; rlwinm. r0,r0,SHt,31,31`
(the equivalent `clrlwi. r0,r0,31` when `SHt == 0`). The eleven fields declared at the end of
`include/MetroidPrime/ScriptObjects/CScriptPlatform.hpp` pack **lowest declared field into the lowest
bit**, `0x48c` first:

| declared | field | byte | bit | `MB` | store `SH` | test `SH` |
|---|---|---|---|---|---|---|
| 1 | `mDead` | 0x48c | 0 | 24 | 7 | 25 |
| 2 | `mControlledAnimation` | 0x48c | 1 | 25 | 6 | 26 |
| 3 | `mDetectCollision` | 0x48c | 2 | 26 | 5 | 27 |
| 4 | `mSquishedRider` | 0x48c | 3 | 27 | 4 | 28 |
| 5 | `mMotionActive` | 0x48c | 4 | 28 | 3 | 29 |
| 6 | `mPassedMotionEnd` | 0x48c | 5 | 29 | 2 | 30 |
| 7 | `mPassedMotionStart` | 0x48c | 6 | 30 | 1 | 31 |
| 8 | `mMotionForward` | 0x48c | 7 | 31 | 0 | 0 (`clrlwi`) |
| 9 | `mPreviousMotionForward` | 0x48d | 0 | 24 | 7 | 25 |
| 10 | `x48d_25_` | 0x48d | 1 | 25 | 6 | 26 |
| 11 | `mMotionTransformed` | 0x48d | 2 | 26 | 5 | 27 |

The store's `MB` and the test's `SH` are the only reliable way to read a bitfield body: the note in
`...-slavevec` that "`mDead` = byte `0x48c` bit 7" is **wrong** (it is bit 0), and following it will
mis-decode every function that touches one. The table was derived with a probe
(`.tmp/opencode/probe_bits.cpp`, eleven one-field functions, reverse emission order) and checked
against all four fields this tree already has a 100% body for: `mDead` (`fn_800a1df8`),
`mMotionActive` (`fn_800a1df8`, `StopMotion`), `x48d_25_` (`fn_800a1df8`),
`mMotionTransformed` (`TranslateMotion`). `MB` is relative to the byte, so the same `MB` appears in
both bytes.

**A `bl` into an `auto_*` object is not a free call.** `fn_801FAC1C` (0x801FAC1C) and `fn_801FAC14`
(0x801FAC14) live in `build/G2ME01/obj/auto_03_801FA3CC_text.o`, so the DOL resolves them and the
decomp build is fine. But nothing in `src/` defines them, so **calling them adds a new undefined
symbol to the host port link**, and `tools/gate.sh`'s `port probe` (which runs
`tools/link_check.sh --strict`) and `port link gap` (which checks
`docs/research/port_link_gap_list.md`) both fail when the count rises above
`docs/research/port_link_baseline.txt`'s 250 / the list's entries. That is why
`TeleportToWaypoint`, `SetMotionTime` and `DragSlaves` are left as `// TODO` bodies below even though
all three are decoded:

| function | address | size | the one callee that blocks it |
|---|---|---|---|
| `TeleportToWaypoint__15CScriptPlatformF9TUniqueIdR13CStateManager` | 0x800A2030 | 160 B | `fn_801FAC1C` |
| `SetMotionTime__15CScriptPlatformFfR13CStateManager` | 0x800A20B0 | 304 B | `fn_801FAC14` |
| `DragSlaves__15CScriptPlatformFR13CStateManagerRQ24rstl24reserved_vector<Us,1024>` | 0x800A2490 | 484 B | `fn_800B8038` |
| `DragSlave__15CScriptPlatformFR13CStateManagerRQ24rstl24reserved_vector<Us,1024>RC7SRiders` | 0x800A2534 | 736 B | calls `DragSlaves` |

`TeleportToWaypoint` decoded (one `bl` each, in order): `GetObjectById(id)`, `TCastToPtr<CScriptWaypoint>`,
skip if null; skip if `mWaypointTracker` (0x440) is null; `time = fn_801FAC1C(mWaypointTracker, &idCopy, mgr)`
— the id is **copied to `r1+8` first**, as with `AddRider`'s timer — then
`if (time >= 0.f) SetMotionTime(time, mgr)`, written `fcmpo cr0,f1,f0 / cror eq,gt,eq / bne <end>`
where `f0` is `lbl_8041B01C` = 0.0f. `fn_801FAC1C` itself is a linear scan of 0x50-byte entries
comparing the `TUniqueId` at +4 against the id and returning `fn_801FAD68(entry)` for the match, or
`lfs -19756(r2)` for no match.

`SetMotionTime` decoded: `fn_801FAC14(...)` then `Stop()`, then
`mSplineController->GetPositionKnotCount()` / `GetPositionByTime(...)`, then
`SetTranslation(...)`, then `DragSlaves(mgr, moved)` with a fresh `moved`.

## The rest of the unit, re-measured

Still unwritten: the constructor (42.52%, 1388 B), `PreThink` (0.24%, 1688 B), `Move` (1.58%,
2088 B), `AcceptScriptMsg` (1.98%, 1608 B), `MoveRiders` (0.45%, 888 B), `DragSlave` (0.54%, 736 B),
`Think` (0.75%, 536 B), `AdvanceMotionTime` (0.92%, 436 B). Plus `AddRider(vector)` 99.61%,
`fn_800A31A0` 93.45%, `fn_800A1CE8` / `fn_800A1D4C` 0.00% — the last two still blocked on
`lbl_803B32B0`, exactly as `...-slavevec` recorded, and `fn_800A359C`'s blocker is gone (it is at 100%).

`AdvanceMotionTime` (0x800A3B64, 436 B) is the next one I would take: every callee it needs is
already available (`CGameSpline::GetPositionSpline`, `fn_800a3d18`), so it adds no undefined symbol.
Decoded from retail, using the bitfield table above:

```c
mPreviousMotionForward = mDead;               // 0x800A3B80/8c/94, a bit copy across the byte boundary
mPassedMotionEnd = false;                      // 0x800A3B98/a0
mPassedMotionStart = false;                    // 0x800A3BA4/ac
if (!mMotionForward) { dt = -dt; }             // 0x800A3BB4 (clrlwi. r0,r0,31) / 3BBC (fneg)
float duration = 0.f;
if (!mMotionSpline.null())  duration = mMotionSpline->mDuration;              // 0x800A3BC0/d0
if (!mSplineController.null()) duration = mSplineController->GetPositionSpline().mInitialTime;  // 0x800A3BD4/e4
if (mMotionFlags & 0x400) duration = mMotionDuration;                        // 0x800A3BE8/f4
if (mMotionActive || (mMotionFlags & 0x400)) {                                // 0x800A3BF8..0x800A3C08
  if (duration > 0.f) {
    mMotionTime += dt;
    if (mMotionTime >= duration) {
      if (mMotionFlags & 0x20000000) {
        mMotionTime = <the fmod-shaped expression at 0x800A3C48..0x800A3C7c>;
        mPassedMotionEnd = true;
      } else {
        mMotionTime = duration;
        fn_800a3d18();
      }
      mPassedMotionStart = true;
    } else if (mMotionTime < 0.f) {
      if (mMotionFlags & 0x20000000) {
        mMotionTime = -mMotionTime;
        mPassedMotionStart = true;
      } else {
        mMotionTime = 0.f;
        fn_800a3d18();
      }
      mMotionForward = true;
    }
  }
}
```

What I did **not** finish is that `fmod`-shaped expression at 0x800A3C48, and the constants it needs
are now measured (`./tools/dol_read.py <addr> 8`, retail DOL — `build/G2ME01/main.elf` cannot answer a
`.data` question):

| symbol | address | bytes | value |
|---|---|---|---|
| `lbl_8041B01C` | 0x8041B01C | `00 00 00 00` | `0.0f` |
| `lbl_8041B028` | 0x8041B028 | `3f 80 00 00` | `1.0f` |
| `lbl_8041B044` | 0x8041B044 | `bf 80 00 00` | `-1.0f` |
| `lbl_8041B048` | 0x8041B048 | `43 30 00 00 80 00 00 00` | double `0x4330000080000000` |

So the expression is `f5 = 1.0f / duration` (0x800A3C20), then
`fctiwz` of `mMotionTime * f5` into a **double** at `r1+8`, `lis r0,17200` stored at `r1+16` as the
high word of a second double, `xoris` of the high word of `(double)n` with 32768 stored as that
double's low word at `r1+20`, then `f0 = <that double> - 4.5036e15` and
`fnmsubs f0,f0,f4,f3` — i.e. `mMotionTime - (<double> - 0x4330000080000000) * duration`, which is
CodeWarrior's `fmodf`. Reading it as a plain `fmodf(mMotionTime, duration)` is the obvious first
spelling to try and I did not get to measure it.

## Verified

```
sha1sum build/G2ME01/main.dol                 -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/decomp_build.sh                       -> All: 32.64% fuzzy, 25.39% matched, 11.94% linked (11352 / 28465 functions)
./tools/probe_sources.sh                     -> (inside goal_check's gate step: ok, `port probe`)
python3 tools/check_symbol_names.py           -> checked 514 units; 0 declared names are missing from their object
python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptPlatform
                                              -> ok: 1 unit(s) checked, none emits its functions out of retail order
./tools/unit_fit.sh MetroidPrime/ScriptObjects/CScriptPlatform.cpp
                                              -> the same 32 pre-existing weak COMDAT __ct__/__dt__ instantiations, 3128 B; no new name
./tools/goal_check.sh build/goal/item.json    -> PASS (the block quoted at the top)
```

`gate.sh` is **GATE PASS** on every step, `docs claims` included — the judge rewrites the state block
itself (`tools/gate.sh:115` runs `check_docs_claims.py ${MP_GATE_DOCS_WRITE:+--write}` and
`goal_check.sh` invokes the gate with `MP_GATE_DOCS_WRITE=1`), so the `docs/HANDOFF.md` hunk in this
tree is its doing, not mine. All 86 RELs `cmp`-equal with sha1s matching `config/G2ME01/config.yml`
is the gate's own `hashes vs config.yml ok` step.

Diff is **one source file**, `src/MetroidPrime/ScriptObjects/CScriptPlatform.cpp` (+57/-5): three
bodies and one forward declaration. No `.s`, no `asm`, no `tools/`, no `config/`, no `build/goal/`,
no hand edit to `docs/`. Not committed.

## NEW:

NEW: progress-prime1-cscriptplatform-teleport | progress | MetroidPrime/ScriptObjects/CScriptPlatform | TeleportToWaypoint (0x800A2030, 160 B) and SetMotionTime (0x800A20B0, 304 B) are fully decoded in docs/goal-notes/progress-prime1-cscriptplatform-bodies.md but cannot be written: they call fn_801FAC1C / fn_801FAC14, which no src/ defines (they live in build/G2ME01/obj/auto_03_801FA3CC_text.o, so the DOL resolves them), so the call adds a new undefined symbol to the host port link and fails the gate's `port probe` and `port link gap` steps - the three symbols have to be defined in the port first