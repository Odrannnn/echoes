# progress-prime1-cbswallhang

Target `MetroidPrime/BodyState/CBSWallHang` (`kind: progress`). Unit stays `NonMatching`; the
change is `src/MetroidPrime/BodyState/CBSWallHang.cpp` only. Nothing else in the tree is touched.

## Measured

`build/report.json` is the source of truth. Re-measured at the start of the run, because
`item.json`'s `reason` was stale - it named 4 unmatched functions, one of them
`Start__11CBSWallHangFR15CBodyControllerR13CStateManager`, which was already 100% when I started.

| | before | after |
|---|---|---|
| unit fuzzy | 96.08% | 99.73% |
| unit matched code | 20.04% | 41.55% |
| unit matched functions | 12 / 15 | 14 / 15 |
| project matched functions | 9977 | 9979 |

Per function:

| function | before | after | what fixed it |
|---|---|---|---|
| `CheckForLand__11CBSWallHangFR15CBodyControllerR13CStateManager` | 85.79% | **100%** | Prime 1's `bool result` spelling |
| `CheckForWall__11CBSWallHangFR15CBodyControllerR13CStateManager` | 95.78% | **100%** | `bool result` + Prime 1's `float magSq = 10.f; if (...) magSq = ...;` + a `const CPASDatabase& db` local |
| `UpdateBody__11CBSWallHangFfR15CBodyControllerR13CStateManager` | 96.03% | 99.55% | Prime 1's `if (state == kAS_Invalid) { switch }` shape, named parm locals, and dropping the `ToVec2f()` multiply |
| the other 12 | 100% | 100% | untouched |

`linked` stays 4896 (correct for a `progress` item - the unit is not flipped).

## Per-function record against Prime 1

`prime-ref/src/MetroidPrime/BodyState/CBSWallHang.cpp` is the reference. Adapted to this repo's
own names (`IsInCollision` -> `HasBlockingCollision`, `ScaleCopy` -> `GetScale`,
`DeliverScriptMsg(ent, id, msg)` -> `DeliverScriptMsg(CScriptMsg(...))`, `SendScriptMsg` has the
4-arg `CEntity*` overload) and to this repo's `CScriptMsg` / `CScriptObjectMessage` layout, which
already matched retail byte for byte.

### `CheckForLand` - Prime 1's source matched unchanged (after the two renames)

The whole 14% gap was one thing: Prime 1 writes `bool result = false; ... result = true; return result;`
and the C++ `return true;` / `return false;` form made mwceppc 2.7 spend four extra callee-saved
registers (`stw r31/r30/r29/r28` one at a time) instead of `stmw r27,...`, and emit
`li r3,1; b; li r3,0` instead of `mr r3,r27`. Retail's function is 300 bytes, the two-return form
is 304. That single change took it to 100%. The `x || y` short-circuit, the `CVector3f::Zero()`
pair and the `SendScriptMsg` argument order were already right.

### `CheckForWall` - Prime 1's source needed one extra edit

Same `bool result` fix, plus Prime 1's

```cpp
float magSq = 10.f;
if (waypoint != nullptr) {
  magSq = (waypoint->GetTranslation() - actor->GetTranslation()).MagSquared();
}
```

instead of the ternary this repo had. The ternary loads the `10.f` constant *after* the branch
(`lfs f1,10` on the taken path plus a `b`); the if-assign lets the compiler hoist the load above
the `beq`, which is what retail does. That got 99.34% (628 bytes vs retail's 632).

The last 4 bytes were `mr r3,r29; bl GetPASDatabase; mr r0,r3; addi r3,...; mr r4,r0` in retail
against our `mr r3,r29; bl GetPASDatabase; mr r4,r3; addi r3,...`. Binding the reference first

```cpp
const CPASDatabase& db = bc.GetPASDatabase();
const rstl::pair< float, int > best = db.FindBestAnimation(parms, *mgr.Random(), -1);
```

reproduces retail's round-trip through r0. 100%.

### `UpdateBody` - Prime 1's source mostly matched, except where Echoes forked

**Prime 1's `UpdateBody` does not match Echoes' retail.** Retail's call inventory
(`tools/dis.sh 2148716904 2532 | grep -o 'bl .*<...>' | sort | uniq -c`) is
`SetLaunchVelocity x2`, `CheckForWall x2`, `CheckForLand x2`, `GetCmd x2`, `FixInPlace x2`,
`LoopBestAnimation x6`, `PlayBestAnimation x1`, `SetCurrentAnimation x2`, `FindBestAnimation x2`,
`GetPASDatabase x2`, `GetObjectById x2`, `DeliverScriptMsg x2`, `SqrtF x1`, `Identity x1`, and
**no `FaceDirection`, no `AsNormalized`, no virtual call**. So Echoes dropped three things Prime 1
has, and porting all of Prime 1 verbatim made the function *worse*:

- `SetLaunchVelocity(bc)` in `kWHS_JumpArc` (Prime 1 has it; retail's 2 are JumpAirLoop +
  DetachJumpLoop) - **83.15%**, size 2592
- `SetLaunchVelocity(bc)` + `CheckForLand(bc, mgr)` in `kWHS_OutOfWallHangTurn` - same run
- the whole `cmdMgr.GetTargetVector().IsNonZero()` / `CVector3f::Dot` / `bc.FaceDirection(-lookDir, dt)`
  block in `kWHS_WallHang` - same run. It also made the compiler keep `dt` live in f31 for the
  whole switch (`fmr f31,f1` in the prologue, frame 1600 instead of 1520).

What did survive from Prime 1, and is needed:

- the outer `if (state == pas::kAS_Invalid) { switch (mState) { ... } } return state;` shape
  instead of this repo's early `return state;` (**95.73%**)
- naming the `CPASAnimParmData` / `CAnimPlaybackParms` temporaries `parms` / `playParms` /
  `loopParms` rather than inlining them into the call - `LoopBestAnimation x6` lands only with
  the named locals (**96.20%**)
- `CPASAnimParm::FromEnum(pas::kWHS_JumpArc)` in place of Prime 1's literal `FromEnum(1)` (same
  value, `pas::EWallHangState`, `CharacterCommon.hpp:164`)

**The one edit Prime 1 does not explain, and the whole remaining 3.8%.** This repo's
`CVector3f(invTime * toWaypoint.ToVec2f(), zVel)` needs `operator*(const float&, const CVector2f&)`
and `CVector2f(float,float)`, both of which are out-of-line in `Kyoto/Math/CVector2f.hpp` - the
header is byte-identical to Prime 1's, but mwceppc 2.7 does not inline them here while 1.3.2 did.
Retail inlines the whole thing into `fdivs`/`fmuls` (0x8012D440-0x8012D46C). Writing the components
out by hand inlines it and the function drops to **exactly retail's 2532 bytes**:

```cpp
mLaunchVel = CVector3f(invTime * toWaypoint.GetX(), invTime * toWaypoint.GetY(), zVel);
```

**96.20% -> 99.55%**, and the prologue (which had 3 extra `psq_st` pairs and a 12-byte shift)
lines up.

### Wall

```
WALL: CBSWallHang::UpdateBody 99.55% - the only remaining diff is the two CScriptMsg
      constructions (IntoJump and OutOfWallHang); the struct contents are identical to retail
      (kInvalid, kInvalid, uid, 0x584c4155, -1) but ours keeps kInvalidUniqueId in r6 where
      retail keeps it in r7, so the seven `sth`s land in the other registers and one temp slot
      is off by 4. Pure register allocation.
```

Spellings measured against that residual, all leaving it at 99.55% or worse:

| spelling | UpdateBody |
|---|---|
| `const CScriptMsg jumped(...); mgr.DeliverScriptMsg(jumped);` (named local) | 99.55% |
| `const CPASDatabase& db = bc.GetPASDatabase();` at both UpdateBody sites | **99.21%** |
| Prime 1 verbatim (all three Echoes forks added) | 83.15% |

The `db` local is worth remembering in the other direction: it is what takes `CheckForWall` from
99.34% to 100%, and it *loses* 0.34% in `UpdateBody`. Same trick, opposite sign, same compiler.

## Gates

Run from the worktree root, `MP_TOOLCHAIN_DIR=.../MetroidPrimePort`:

- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (the pinned hash)
- `./tools/decomp_build.sh` -> `All: 30.73% fuzzy, 22.93% matched, 11.74% linked (9979 / 28465 functions)`
- `./tools/probe_sources.sh` -> `749 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)`
- `python3 tools/check_symbol_names.py` -> `checked 503 units; 0 declared names are missing`
- `./tools/gate.sh build/goal/judge/report.base.json` -> every line `ok` except
  `docs claims`, which reports the HANDOFF state block still says `9977`; the brief says the
  driver rewrites that block from the tree, so it is expected and is why `HANDOFF.md` was not
  edited. Its per-function diff line is the one that matters here:
  `matched 9977 -> 9979   linked 4896 -> 4896   (+2 functions at 100%, 0 units newly linked)`
- no `asm` added: the judge's own grep over the added lines finds none.

`git status --porcelain --untracked-files=all` lists only
`M src/MetroidPrime/BodyState/CBSWallHang.cpp`, so the "paths the agent may not edit" check is
clean. `configure.py` is unchanged - the unit stays `NonMatching`, as a `progress` item requires.

## Lessons / traps

- **Take the retail address from `report.json`'s `virtual_address`, not from a hand subtraction.**
  I read `UpdateBody` once at 0x8012B9F8 (derived by adding the four preceding sizes to
  `IsMoving`) when the report says 2148716904 = 0x8012D168. `tools/dis.sh` happily printed *our*
  build's bytes for a range we own, the diff was 100% noise, and it briefly looked as though
  adding Prime 1's code had made the function worse for a completely different reason.
  `tools/bytescmp.py` reads the DOL directly and is the only one of the two that is retail.
  Its `addr` argument is **hex** (`8012d168`), `size` is decimal; passing a decimal address
  fails with "is in no loaded section".
- **Count the calls in the retail function before porting a sibling's version.** Prime 1's
  `UpdateBody` is 3 statements longer than Echoes' and each one is visible as a missing `bl` in
  retail. That is also what proves `kWHS_WallHang` in Echoes has no face-direction code, which no
  percentage would have told me.
- A `bool result` local instead of `return true` / `return false` is worth 4 bytes of prologue and
  14% on a small function. Check it before assuming a unit is stuck on its real logic.
- A non-inlined out-of-line operator/ctor in a shared header can cost 60 bytes and 4% on a
  caller. Spreading the expression out in the caller is a local fix; making the header's operator
  inline would move every other unit that uses it, which is the one thing not to do here.
