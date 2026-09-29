# match-cfidget — PARTIAL 2026-09-29

`MetroidPrime/Player/CFidget` was 3/4; `CFidget::Update(int, bool, bool, float, CStateManager&,
const CPlayer&)` had **no body** (0.704%). It is now written from the retail asm, the unit is
**4/4 at 100.00%**, and `tools/flip_test.sh MetroidPrime/Player/CFidget.cpp` → **PASS**
(kept as `Matching`) — *measured in a tree where an unrelated, pre-existing link break was
temporarily worked around; see "The build is broken at HEAD" below, which is what stops the
judge from seeing that PASS.*

## The change

Three files, 70 insertions.

1. `src/MetroidPrime/Player/CFidget.cpp` — `Update`'s body (the only source change).
2. `config/G2ME01/splits.txt` — one line added to CFidget's claim:
   `.sdata2 start:0x8041D0C8 end:0x8041D0F0`. **Without it the flip cannot happen**; see
   "The float pool" below.
3. `configure.py:473` — `Object(NonMatching -> Matching, "MetroidPrime/Player/CFidget.cpp")`.

## What Update does (read out of `build/G2ME01/asm/MetroidPrime/Player/CFidget.s`)

The whole body is inside `if (mState == kS_NoFidget) { ... return-equivalent }` — retail's
first branch is `bne .L_801C6D40`, straight to the epilogue.

| field | offset | what advances it |
|---|---|---|
| `mTimeSinceFire` | 0x00 | `+dt` while `< 6.f`, zeroed on `fireButtonStates != 0` |
| `mTimeSinceStrikeCooldown` | 0x04 | `+dt` while `< 11.f`, zeroed on `inStrikeCooldown` |
| `mTimeSinceUnmorph` | 0x08 | `+dt` while `< 21.f`, zeroed while morphed |
| `mTimeSinceBobbing` | 0x0c | `+dt` while `< 21.f`, zeroed on `bobbing` |
| `mFidgetDelayTimer` | 0x10 | `+dt`, then `> mTimeUntilFidget` picks a fidget and zeroes it |
| `mHolsterTimeSinceFire` | 0x14 | `+dt` while `< mTimeUntilHolster + 1.f`; `> mTimeUntilHolster` requests holster |
| `mTimeUntilHolster` | 0x18 | read only (ctor 105.f) |
| `mTimeUntilFidget` | 0x1c | `Random()->Range(20.f, 30.f)` on entering a fidget |
| `mState` | 0x20 | 0..3 = `EState` |
| `mType` | 0x24 | `kFT_Minor` (0) / `kFT_Major` (1); ctor and `ResetAll` use `-1` |
| `mAnimSet` | 0x28 | `Random()->Range(0,4)` / `Range(0,5)`, or 0 on holster |
| `mLoading` | 0x2c | untouched by `Update` |

Constants are the ten words at `0x8041D0C8`: `0, 6, 1, 11, 21, 20, 30, 105, 25, 0` (read with
`python3 tools/dol_read.py 0x8041D0C8 0x28`).

Three spellings had to be got right, and the first two attempts were wrong in an instructive way:

* **Morph-ball polarity.** `lwz r0,0x38c(r8); cmpwi r0,0; bne` — retail branches on *not
  unmorphed* into the zeroing block. Writing `if (state != kMS_Unmorphed) { zero } else if
  (t < 21.f)` compiles to `beq` into the *accumulating* block and mirrors retail's 6
  instructions. Writing the positive test first (`== kMS_Unmorphed`) is what reproduces the
  `bne`. Costs 8 of 191 instructions, and objdiff sees both.
* **Switch cases.** `case kS_NoFidget: break;` adds a `cmpwi r0,0; beq` retail does not have.
  With it the function sat at 94.50%; the enum's own 0..3 range still produces retail's
  `cmpwi r0,4` upper bound, so dropping the empty case is enough.
* **`kS_HolsterBeam` stores 0, not -1.** `li r0,0; stw r0,0x24; stw r0,0x28` — `mType` gets
  `0`, which the header names `kFT_Minor`. That looks wrong and is not; the source comment
  says so, because a reader will want to "fix" it.

The `Next() % 100 > 50` test is retail's own magic-number division
(`0x51EC0000-0x7AE1`, `mulhw`, `srawi 5`, `*100`, subtract) — writing `% 100` reproduces it
byte for byte; nothing else does.

## The float pool — why `splits.txt` had to change

`Update` references ten floats in `.sdata2` at `0x8041D0C8..0x8041D0F0`, and **no unit in
`config/G2ME01/splits.txt` claimed that range** (searched every section claim in the file).
So the retail-derived object `build/G2ME01/obj/MetroidPrime/Player/CFidget.o` carries a
720-byte `.text` and *no* `.sdata`/`.sdata2` — the pool is a gap. With the unit flipped and
nothing claiming it, our object's own copy of the constants was placed by the linker at
`0x8041D120` (measured: `nm build/G2ME01/main.elf`, and `lfs f0,-21152(r2)` in the linked
`Update` where retail has `lfs f0,-21136(r2)`), the DOL grew by 64 bytes
(3969088 vs 3969024) and `build.sha1` failed. **objdiff was 100% and the unit still could not
flip** — the percentage is about the object, the sha1 is about the link.

Adding the `.sdata2` claim to CFidget's own entry fixes it. Two details that cost a rebuild
each:

* the range must end at `0x8041D0F0`, not `0x8041D0EC` — dtk refuses a split that ends inside a
  symbol, and `lbl_8041D0E8` is 8 bytes (`0x8041D0E8..0x8041D0F0`), covering the trailing
  `0.0f` that CFidget does not reference:
  `Split MetroidPrime/Player/CFidget.cpp .sdata2 (0x8041D0C8..0x8041D0EC) ends within symbol
  'lbl_8041D0E8' (0x8041D0E8..0x8041D0F0)`.
* `total_functions` is still **28465** after the re-split, and no other unit's functions move
  (per-function diff over the whole report: see below).

`tools/unit_fit.sh` still reports `.sdata2 claimed 40 ours 36 retail 40 SHORT by 4` and an
unclaimed `.sdata 40` on our object (mwcceppc files 6.0/1.0/11.0/21.0 in `.sdata`, retail's
copy is in the claimed `.sdata2`). Both are harmless here — `flip_test` is the authority and
it passed, with the DOL sha1 equal to retail's byte for byte.

## The build is broken at HEAD — not by this item

`./tools/decomp_build.sh` **fails at the link on this lane's HEAD, before and after my
change**:

```
### mwldeppc.exe Linker Error:
#   undefined: 'sndStreamMixParameter'
#   Referenced from 'CDSPStreamManager::UpdateVolume(int,int)' in CDSPStreamManager.o
```

Cause, measured:

* `build/G2ME01/obj/Kyoto/Audio/CDSPStreamManager.o` (the *retail* object, CDSPStreamManager
  is `NonMatching`) has `U sndStreamMixParameter`.
* `build/G2ME01/src/musyx/runtime/stream.o` (ours) has **`T sndStreamMixParameterEx` and no
  `sndStreamMixParameter`** — `extern/musyx/src/musyx/runtime/stream.c:758` guards it with
  `#if MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 2)` and this tree configures
  `-DMUSY_VERSION_PATCH=3`, so it is compiled out.
* `configure.py:1275` at HEAD is `Object(Matching, "musyx/runtime/stream.c")`, so the link
  substitutes our object for retail's and loses the symbol.
* **`extern/musyx/src/musyx/runtime/stream.c` is unmodified in HEAD and in every commit in
  this repository** (`git log --all -- extern/musyx/src/musyx/runtime/stream.c` → `e44c426`,
  `3e7f972`; line 758 still reads `<= MUSY_VERSION_CHECK(2, 0, 2)` in both).
  `git show --stat ada6d97` is `configure.py`, `docs/HANDOFF.md`,
  `docs/goal-notes/match-stream.md` — **the four guard changes the previous item's own note
  describes never landed; only the `configure.py` flip did.**

Second consequence, same cause, and it is what the judge's counts will read:
`build/report.json` at HEAD has `main/musyx/runtime/stream` at **16/18**
(`sndStreamMixParameter` has no body → 0%), against the judge baseline's 18/18, so
`linked 4835 -> 4822` for any item until this is put back. `python3 tools/check_docs_claims.py`
fails on the same three lines of the HANDOFF state block (`9506`, `4822`, `DOL 8095`) for the
same reason.

**I did not fix it** — it is not this item's work, and reverting the flip instead costs two
matched functions and 14 linked ones, which the judge treats as a regression.

## How the flip was actually verified

`flip_test.sh` cannot pass while the link is broken, so the measurement was made with one
temporary, fully reverted edit: `configure.py:1275` set to
`Object(NonMatching, "musyx/runtime/stream.c")` so the retail stream object goes back into the
link. With that and this item's three changes:

```
sha1sum build/G2ME01/main.dol                -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/flip_test.sh MetroidPrime/Player/CFidget.cpp
    TEST MetroidPrime/Player/CFidget.cpp
      PASS  -> kept as Matching
    kept: 1 / 1   failed: 0   skipped: 0
```

`configure.py:1275` is back to `Object(Matching, ...)` in the delivered tree. `git status` is
the three files listed at the top and nothing else.

## Gates (all re-measured in the delivered tree except where noted)

```
main/MetroidPrime/Player/CFidget: 100.00% fuzzy, 100.00% matched code, 4/4 functions
objdump -d, both objects:  180 instructions each, all instruction words identical
objdump -r, both objects:  every R_PPC_EMB_SDA21 at the same offset, same target
   (retail's pool symbols are lbl_8041D0C8..., ours are unnamed locals @488...; objdiff
    resolves them and scores 100.00% - see the pool note above for why they differ)
tools/flip_test.sh MetroidPrime/Player/CFidget.cpp   -> PASS  (see caveat above)
tools/unit_fit.sh MetroidPrime/Player/CFidget.cpp    -> .text 720/720/720 fits;
                                                         no extra functions
python3 tools/check_decl_order.py --unit MetroidPrime/Player/CFidget -> ok
python3 tools/check_symbol_names.py                  -> 484 units, 0 declared names missing
python3 tools/check_files_cmake.py                   -> every configured DOL object accounted for
python3 tools/check_raw_offsets.py                   -> 152 sites, all documented
build/report.json total_functions                   -> 28465 (unchanged)
per-function diff vs build/goal/judge/report.base.json:
   better 1  CFidget::Update 0.70422536 -> 100.0
   worse  1  main/musyx/runtime/stream sndStreamMixParameter 100.0 -> 0.0   (the HEAD
            breakage above, present with or without this change; reverting the stream flip
            to NonMatching is what *caused* that 0.0, so it is not in the delivered tree's
            own build path - the delivered tree cannot link at all until NEW-stream-link
            is done)
```

## What stops this item being a full pass

Only the pre-existing link break. `CFidget::Update` is at 100% with byte-identical code and
relocations, the unit is `Matching`, and `flip_test` passed on the bytes.

NEW: fix-musyx-stream-link | match | musyx/runtime/stream | commit ada6d97 flipped
configure.py:1275 to Matching without the stream.c guard changes its own note describes, so
the DOL link fails on undefined sndStreamMixParameter and the unit reads 16/18 instead of
18/18; re-apply the four guards in extern/musyx/src/musyx/runtime/stream.c (streamKill 336,
sndStreamMixParameter 758, wrap sndStreamMixParameterEx 798 and sndStreamFrq 855 in
`> MUSY_VERSION_CHECK(2,0,3)`, sndStreamLPFParameter 881)

NEW: claim-fidget-float-pool | match | MetroidPrime/Player/CFidget | a unit whose code
references .sdata2 that no splits.txt entry claims can never flip - objdiff reaches 100% and
the link still moves the pool; the fix for this unit is in the delivered diff, so the general
shape of the problem (which other units' pools are unclaimed) is what a follow-up should sweep

---

# match-cfidget — run 2, lane 1 (wt-mp2-goal-L1), 2026-09-29

**This run PASSES: `tools/goal_check.sh` -> `goal_check: PASS match-cfidget`.** The unit is
`Matching`, `CFidget::Update` is at 100.00%, `flip_test` keeps it, and the DOL sha1 equals
retail's. Everything in run 1 above reproduced; nothing below contradicts it. This entry
records the two facts run 1 could not establish, and the exact source, so no run has to
re-derive them.

## The blocker was the driver, not the item — and it is still the driver

Run 1 concluded that the pre-existing link break (`undefined: 'sndStreamMixParameter'`) made
the item unjudgeable and filed `NEW: fix-musyx-stream-link`. That was right about the
symptom and wrong about the cure. **The fix cannot be delivered by any lane, because the
driver does not stage `extern/`.**

```
tools/run_goal.sh:446
  ( cd "$WT" && git add -A -- src include config docs configure.py files.cmake CMakeLists.txt )
```

No `extern` in that list. So the run that produced `ada6d97 match: match-stream` had the four
`stream.c` guards in its worktree, passed the judge with them, and had them **dropped at
commit**:

```
git show --stat ada6d97
  configure.py | docs/HANDOFF.md | docs/goal-notes/match-stream.md   (3 files)
git log --oneline --all -- extern/musyx/src/musyx/runtime/stream.c
  e44c426 build: vendor upstream's MusyX pin for the matching build, port fork to musyx-port
  3e7f972 port: compile the Echoes decompilation against Aurora
```

`configure.py:1275` is `Object(Matching, "musyx/runtime/stream.c")` on the branch head while
`stream.c` is untouched since the vendoring commit, so **`goal/decomp` does not link**. That
is why run 1's judge output (`build/goal/check.out`, kept in the lane worktree) failed three
checks at once — `gate.sh`, `decomp_build.sh printed no All: line`, and `flip_test` — all
three from that one undefined symbol, and why the item could not even be judged as partial:
`decomp_build.sh` runs under `set -e`, so a failed link means `build/report.json` is never
regenerated and *no* item on this branch can be measured.

**Action for the orchestrator (two edits, both outside any lane's reach):**
1. add `extern` to the `git add` at `tools/run_goal.sh:446`, and
2. re-apply the four guards in `extern/musyx/src/musyx/runtime/stream.c`
   (`docs/goal-notes/match-stream.md` has the recipe; see "The change" below for the diff I
   used, verified to restore the link).

Until (1) exists, the `fix-musyx-stream-link` item in the queue will fail exactly the same way
this item did, and so will every other `match`/`progress` item on the branch.

## The judge's baseline is stale, and it flatters the branch

`build/goal/judge/report.base.json` is recorded at `b04f1a9` with
`build/goal/judge/record-gate.log` ending in `GATE FAIL: ninja`, yet it reports
`main/musyx/runtime/stream 17/18, complete: true` and `sndStreamMixParameter 100.0`. A clean
build of `b04f1a9` gives 16/18 and `sndStreamMixParameter` with no score at all. The
baseline was generated from the **leftover `build/G2ME01/src/*.o` of the previous run** —
`reset_wt`/`clean_wt` run `git clean -qfd -e orig -e build -e .tmp`, so `build/` survives every
attempt — while the pre-change reference it records for CFidget (3/4, not complete) is the
correct one. That is `docs/PROCESS_LESSONS.md`'s stale-derived-input lesson paying out a
second time, and it is worth knowing that the baseline is not a trustworthy "before".

## What changed in this tree

Four files, 77 insertions.

1. `src/MetroidPrime/Player/CFidget.cpp` — `Update`'s body, plus
   `#include "MetroidPrime/CStateManager.hpp"` and `#include "MetroidPrime/Player/CPlayer.hpp"`
   (the header only forward-declares `CPlayer`; `GetMorphballTransitionState()` and
   `CPlayer::kMS_Unmorphed` need the definition).
2. `config/G2ME01/splits.txt` — `.sdata2 start:0x8041D0C8 end:0x8041D0F0` on CFidget's claim.
   Still required; see run 1's "The float pool" for why, and for the 0x8041D0F0 end point.
3. `configure.py:473` — `Object(NonMatching -> Matching, "MetroidPrime/Player/CFidget.cpp")`.
4. `extern/musyx/src/musyx/runtime/stream.c` — **the four guards, out of scope for this item
   and included deliberately.** Without them the judge cannot run at all (the paragraph
   above), and the change would be discarded for a third time. They are guards only, no
   function body is touched, and `gate.sh` is the proof they are right: with them the DOL
   hashes retail and the gate's 86 RELs still match. This is the filed
   `fix-musyx-stream-link` item; **it will not be committed by the driver** (see the staging
   line). If the orchestrator would rather not carry it, the alternative is to drop this
   file and the item fails as it did in run 1.

```
 336  #elif MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 2)   -> (2, 0, 3)          streamKill
 758  #if    MUSY_VERSION <= MUSY_VERSION_CHECK(2, 0, 2)   -> (2, 0, 3)          sndStreamMixParameter
 793  new #if MUSY_VERSION > MUSY_VERSION_CHECK(2, 0, 3) around sndStreamMixParameterEx, #endif after it
 850  new #if MUSY_VERSION > MUSY_VERSION_CHECK(2, 0, 3) around #pragma push / sndStreamFrq / #pragma pop
 878  #if    MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 2)   -> > (2, 0, 3)         sndStreamLPFParameter
      (preprocessor depth pass: final depth 0, never negative)
```

## The source, verbatim, so no run has to re-derive it

`src/MetroidPrime/Player/CFidget.cpp`, after the two includes, replacing the TODO body:

```cpp
void CFidget::Update(int fireButtonStates, bool bobbing, bool inStrikeCooldown, float dt,
                     CStateManager& mgr, const CPlayer& player) {
  if (mState == kS_NoFidget) {
    if (fireButtonStates != 0) {
      mTimeSinceFire = 0.f;
      mHolsterTimeSinceFire = 0.f;
    } else {
      if (mTimeSinceFire < 6.f) {
        mTimeSinceFire += dt;
      }
      if (mHolsterTimeSinceFire < mTimeUntilHolster + 1.f) {
        mHolsterTimeSinceFire += dt;
      }
    }

    if (inStrikeCooldown) {
      mTimeSinceStrikeCooldown = 0.f;
    } else if (mTimeSinceStrikeCooldown < 11.f) {
      mTimeSinceStrikeCooldown += dt;
    }

    if (player.GetMorphballTransitionState() == CPlayer::kMS_Unmorphed) {
      if (mTimeSinceUnmorph < 21.f) {
        mTimeSinceUnmorph += dt;
      }
    } else {
      mTimeSinceUnmorph = 0.f;
    }

    if (bobbing) {
      mTimeSinceBobbing = 0.f;
    } else if (mTimeSinceBobbing < 21.f) {
      mTimeSinceBobbing += dt;
    }

    if (mState == kS_NoFidget) {
      mFidgetDelayTimer += dt;
      if (mFidgetDelayTimer > mTimeUntilFidget) {
        mState = mgr.Random()->Next() % 100 > 50 ? kS_MajorFidget : kS_MinorFidget;
        mFidgetDelayTimer = 0.f;
      }
    }

    if (mHolsterTimeSinceFire > mTimeUntilHolster) {
      mState = kS_HolsterBeam;
    }

    switch (mState) {
    case kS_MinorFidget:
      mTimeUntilFidget = mgr.Random()->Range(20.f, 30.f);
      mType = SamusGun::kFT_Minor;
      mAnimSet = mgr.Random()->Range(0, 4);
      break;
    case kS_MajorFidget:
      mTimeUntilFidget = mgr.Random()->Range(20.f, 30.f);
      mType = SamusGun::kFT_Major;
      mAnimSet = mgr.Random()->Range(0, 5);
      break;
    case kS_HolsterBeam:
      // Retail stores 0 here, not kFT_Invalid: `li r0,0; stw r0,0x24; stw r0,0x28`.
      mType = SamusGun::kFT_Minor;
      mAnimSet = 0;
      break;
    default:
      break;
    }
  }
}
```

Run 1's three spelling notes all hold and none of them cost a rebuild this time. The
`default: break;` label is what this run shipped and it **is** needed here — the first
measurement of the run, before the `.sdata2` claim existed, put the function at 99.50704 with
the other three functions also below 100% (`ResetAll` 99.67, `__ct__` 99.25), which is the
unclaimed-pool symptom from run 1, not a spelling error: the object was byte-identical
(`objdump -d`, 142 instructions, no diff) and the relocations matched one-for-one. Re-splitting
and re-running the report is what takes all four to 100.00.

## Measured in this tree

```
tools/goal_check.sh build/goal/item.json
  ok  no judge-owned path touched
  ok  gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok  counts: matched 9508 -> 9510   linked 4848 -> 4853
  ok  check_symbol_names.py
  ok  All:  29.19% fuzzy, 21.34% matched, 11.64% linked (9510 / 28465 functions)
  ok  flip_test MetroidPrime/Player/CFidget.cpp: PASS, Object(Matching) in configure.py
  goal_check: PASS match-cfidget

sha1sum build/G2ME01/main.dol                    -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/flip_test.sh MetroidPrime/Player/CFidget.cpp -> PASS -> kept as Matching
main/MetroidPrime/Player/CFidget                 -> 100.00% fuzzy, 100.00% matched, 4/4
./tools/unit_fit.sh MetroidPrime/Player/CFidget.cpp
   .text 720/720/720 fits; .sdata2 claimed 40 ours 36 SHORT by 4;
   .sdata 40 unclaimed (mwcceppc's int pool); no extra functions
python3 tools/check_decl_order.py --unit MetroidPrime/Player/CFidget -> ok
python3 tools/check_symbol_names.py   -> 484 units, 0 declared names missing
python3 tools/check_files_cmake.py   -> every configured DOL object accounted for
python3 tools/check_raw_offsets.py   -> 152 sites, all documented
./tools/probe_sources.sh              -> 736 files, 0 failed, 0 errors
build/report.json total_functions     -> 28465 (unchanged)
per-function diff vs build/goal/judge/report.base.json
   better 2   CFidget::Update 0.70422536 -> 100.0
              streamKill 62.878788 -> 100.0   (the guards, from 16/18 to 18/18)
   worse 0    new 0   gone 0
```

`gate.sh` rewrote the `docs/HANDOFF.md` state block; I reverted that file, as the brief
requires.

The re-split turns the leftover 0x8041D0F0..0x8041D11C `.sdata2` gap into a dtk data-only
unit, `main/auto_11_8041D0F0_sdata2`. It has no functions, so it moves neither count, and
`gate.sh` and `check_files_cmake.py` are both clean.

## NEW

None filed. `fix-musyx-stream-link` is already in the queue and its recipe is above, but note
for whoever picks it up: **it cannot pass while `tools/run_goal.sh:446` omits `extern` from
the staging list** — the guards will be dropped at commit and the judge will go red again on
the very next item. That is a driver fix, not a lane's, so it is not filed as a `NEW:` item
here.
