# The 40 gun/player symbols are not on the boot path, and that is measurable

Written 2026-09-26 from a clean build of `3be457e` in lane `h3`. Every site below is read out of
`build-port-link/build.log` and the objects on the link line, not recalled. The instrument is
`tools/link_fn_reach.py`, which is new and is described at the end.

## The answer first

**0 of the 40 are reached before the first frame. 0 are reached during initialisation.
All 40 are gameplay-only.** Not one of the 40 is even *referenced* before `main`: of 69
relocation sites across the 40 symbols, **7 are static data** (a pointer in `sStateFuncs` /
`sTriggerFuncs`), **62 are calls inside `CPlayerGun`, `CStateManager` or `CScriptCannonBall`
methods**, and **0 are in a `_GLOBAL__sub_I*` static initialiser**.

(Measured before this lane landed anything. Two of the 40 are now defined - see "What was
landed" - and re-running the instrument gives 67 sites over 38 symbols: 60 `call`, 7 `data`,
still 0 `pre-main`.)

That last number is the load-bearing one, and it is what makes the answer cheap rather than
an argument. `tools/link_reach.py` treats *any* object carrying a static initialiser as a
root, because those run before `main`. `src/MetroidPrime/Player/CPlayerGun.cpp` carries one -
it builds the two function tables - so `CPlayerGun.cpp.o` is a root, and **every** symbol it
references is "reachable", including the 22 `CPlayerGun` methods that only run once Samus
exists. The same blindness applies to `CStateManager.cpp.o` and `CScriptCannonBall.cpp.o`.
The block was never stubbable and never was on the path; the instrument could not tell the
difference, and neither could a reader of its output.

## What each of the 40 is referenced from

`kind` is the site kind: `data` = a relocation in `.data.rel`, so the linker must resolve the
symbol but no instruction at that point uses it; `call` = a call instruction inside the named
function. Object column is the referencing object, from `ld.bfd`'s own attribution.

### `CPlayerGun.cpp.o` - 33 symbols

| symbol | kind | referenced from |
| --- | --- | --- |
| `CGunStateMachine::GetCurrentStateName() const` | call | `CPlayerGun::ResetStateMachine` |
| `CGunStateMachine::SetState(CStateManager&, CPlayerGun*, string const&)` | call | `CPlayerGun::ResetStateMachine` |
| `CGunStateMachine::SetStateFuncs(SGunStateFunc const*, int)` | call | `CPlayerGun::InitStateMachine` |
| `CGunStateMachine::SetStateMachine(CStateMachine const*)` | call | `CPlayerGun::InitStateMachine` |
| `CGunStateMachine::SetTriggerFuncs(SGunTriggerFunc const*, int)` | call | `CPlayerGun::InitStateMachine` |
| `CGunWeapon::GetWeaponInfo() const` | call | `CPlayerGun::UpdateNormalShotCycle` |
| `CGunWeapon::IsChargeAnimOver() const` | call | `CPlayerGun::ChargeDone` |
| `CGunWeapon::fn_801D8EC0()` | call | `CPlayerGun::fn_801D0700` |
| `CGunWeapon::fn_801D8F2C()` | call | `CPlayerGun::fn_801CEAEC` |
| `CGunWeapon::fn_801D8F64()` | call | `CPlayerGun::fn_801CEBB0` |
| `CGunWeapon::fn_801DA364(CStateManager&, bool)` | call | `CPlayerGun::fn_801CA8C8` |
| `CPlayer::PlaySfxForPlayer(uint, short, int, bool, int)` | call (x3) | `ActivateMissile`, `UpdateNormalShotCycle`, `fn_801cdca0` |
| `CPlayer::fn_8000BC44(CStateManager&)` | call | `CPlayerGun::UpdateNormalShotCycle` |
| `CPlayerGun::ComboActive(CStateManager&, EStateMsg, float)` | **data** | `sStateFuncs[]` |
| `CPlayerGun::EnableChargeFx(CStateManager&, bool)` | call | `CPlayerGun::Charging` |
| `CPlayerGun::FidgetOver(CStateManager&, CTriggerData const&)` | **data** | `sTriggerFuncs[]` |
| `CPlayerGun::Fidgeting(CStateManager&, EStateMsg, float)` | **data** | `sStateFuncs[]` |
| `CPlayerGun::GetBeamAmmoTypeAndCosts(...)` | call (x2) | `StartCharge`, `UpdateNormalShotCycle` |
| `CPlayerGun::GetPlayer(CStateManager&) const` | call (x13) | 13 of the state/trigger handlers |
| `CPlayerGun::GetPlayerFromAll(CStateManager&) const` | call (x3) | `UpdateChargeState`, `UpdateNormalShotCycle`, `fn_801CDD28` |
| `CPlayerGun::GetTargetId(CStateManager&)` | call | `CPlayerGun::UpdateNormalShotCycle` |
| `CPlayerGun::InPhazon(CStateManager&, CTriggerData const&)` | **data** | `sTriggerFuncs[]` |
| `CPlayerGun::InitiateCombo(CStateManager&, CTriggerData const&)` | **data** | `sTriggerFuncs[]` |
| `CPlayerGun::IsOutOfAmmoToShoot(CStateManager&) const` | call (x3) | `Charging`, `StartCharge`, `UpdateNormalShotCycle` |
| `CPlayerGun::Main(CStateManager&, EStateMsg, float)` | **data** | `sStateFuncs[]` |
| `CPlayerGun::PlayAnim(CStateManager&, int, int)` | call (x5) | `Charging`, `MissileActive`, `MissileClosing`, `UpdateNormalShotCycle`, `fn_801D19CC` |
| `CPlayerGun::Recoil(CStateManager&, EStateMsg, float)` | **data** | `sStateFuncs[]` |
| `CPlayerGun::ResetCharge(CStateManager&, bool)` | call (x5) | `Charging`, `EventHandler`, `MissileActive`, `UpdateNormalShotCycle`, `fn_801D19CC` |
| `CPlayerGun::StopChargeSound(CStateManager&, bool)` | call (x2) | `Charging`, `fn_801C71F8` |
| `CPlayerGun::fn_801C72B4(CStateManager&, float)` | call | `CPlayerGun::MissileActive` |
| `CPlayerGun::fn_801CA734(CStateManager&)` | call | `CPlayerGun::InMorphball` |
| `CPlayerGun::fn_801CD55C(CStateManager&, bool)` | call | `CPlayerGun::fn_801C71F8` |
| `CPlayerGun::fn_801CE0DC(CStateManager&)` | call | `CPlayerGun::fn_801CE4FC` |
| `CPlayerGun::fn_801CE5C0(CFinalInput const&, CStateManager&)` | call | `CPlayerGun::fn_801CE800` |
| `CPlayerGun::fn_801DE430(CStateManager&)` | call | `CPlayerGun::fn_801D19CC` |

The **earliest** of these is `CGunStateMachine::SetStateMachine`, reached from
`CPlayerGun::InitStateMachine` (retail 0x801C9544, 0xA0). `InitStateMachine` is called from
exactly one place, `CPlayerGun::UpdateStateMachine` (0x801C966C, 0x64) - and that is `virtual`,
reached through `CPlayerGun`'s vtable slot +0x58 (`lwz r12,88(r12); mtctr; bctrl` at
0x801C96A8), so there is no `bl` to it anywhere. So in the port's own object graph the whole
class is dark: of the functions in the table above, the only callers are inside
`CPlayerGun.cpp.o` itself.

### `CStateManager.cpp.o` - 3 symbols, all from one function

| symbol | kind | referenced from |
| --- | --- | --- |
| `CPlayer::GetTweakPlayer() const` | call (x3) | `CStateManager::ApplyLocalDamage` |
| `CPlayer::fn_8000d3ac(CVector3f const&, CStateManager&)` | call | `CStateManager::ApplyLocalDamage` |
| `CPlayer::fn_8000d40c(CVector3f const&, CStateManager&)` | call | `CStateManager::ApplyLocalDamage` |

`CStateManager::ApplyLocalDamage` is retail 0x8003D838, 0x550 bytes. Something has to take
damage first.

### `CScriptCannonBall.cpp.o` - 2 symbols

| symbol | kind | referenced from |
| --- | --- | --- |
| `CPlayer::GetPlayerIndex() const` | call | `CScriptCannonBall::AcceptScriptMsg` |
| `CPlayer::fn_8000BE98() const` | call | `CScriptCannonBall::TrackedShot::Think` |

A `CScriptCannonBall` exists only where a level placed one.

## The gate: `CPlayer::CPlayer`, and it is one function

`CPlayerGun`'s constructor is reached from exactly one place in the whole DOL, and the chain
is short enough to state (all from `build/G2ME01/main.elf`, every `bl` scanned over 985,921
lines of disassembly):

```
fn_8001EE58  (0x8001EE58, 0xD10)   -- unnamed, no direct caller: reached indirectly
  fn_800401D8  (0x800401D8, 0x940)
    fn_80040D88  (0x80040D88, 0x564)
      CPlayer::CPlayer  (0x8001B018, 0x15C8)   at 0x800411B8
        CPlayerGun::CPlayerGun (0x801D2230, 0x714)  at 0x8001B490
```

and separately

```
fn_801F42A0  (0x801F42A0, 0x4B0)   -- unnamed, no direct caller
  fn_800401D8 ...
```

`CPlayer::CPlayer` takes `CPlayerState*`, `CCameraManager*` and a `CMaterialList`, so it cannot
run before a world exists, and per `docs/research/boot_path.md` a world cannot exist: step 13
(`CGameGlobalObjects::AddPaksAndFactories`, 1,936 bytes) is an empty body, and `gpGameState` is
null until `CMain::StreamNewGameState` runs, which needs those paks. **So the 40 are behind
step 13, not on the way to it.**

## What this says about stubbing, and what it does not

It is tempting to read "gameplay-only" as "safe to stub". **Do not, and this document is the
reason why.** The justification for every one of the 181 stubs in
`docs/research/port_link_stubs.md` is that the symbol is *unreachable*. These 40 are
unreachable *before a frame*, which is a weaker statement, and two of the mechanisms that
would make them reachable later are invisible to a static instrument:

- the 7 `data` sites are function pointers in tables that a `CStateMachine` dispatches
  indirectly, and an indirect call is not a relocation;
- `CPlayerGun::UpdateStateMachine` and `EventHandler` are `virtual` and reached through
  `CPlayer`'s vtable, which likewise is not a call site in any object.

A stub for any of the 40 would therefore link, and would crash the first time a player picked
up a beam. The correct use of this document is to decide **where to spend effort**, not to
retire symbols.

## The instrument: `tools/link_fn_reach.py`

New. It refines `link_reach.py`'s object granularity to the **referencing site** and reports
`data` / `pre-main` / `call` for each. On the 342:

```
$ python3 tools/link_fn_reach.py --out all_sites.tsv     # before this lane landed anything
wrote all_sites.tsv: 498 row(s) over 342 symbol(s)
  call     457
  data     41

$ python3 tools/link_fn_reach.py --out all_sites.tsv     # after: 342 -> 340
wrote all_sites.tsv: 496 row(s) over 340 symbol(s)
  call     455
  data     41
```

**Zero `pre-main` for the whole 342** - and for the 340 after this lane - which is the
generalisation worth having: not one undefined symbol in the port's link is referenced from a
static initialiser, so the `link_reach.py` roots that make these 342 look dangerous are, on
this evidence, not the reason they are pinned. Its stated weakness ("whole-object granularity,
branch-blind") is real and it is now measured rather than assumed.

It deliberately does **not** decide that a symbol is safe to stub, and its docstring says so:
the hops column is the distance of the *object* from a root, which is `link_reach.py`'s number
and is not the distance of the site. A function inside a root object reads 0 however late it
runs. `docs/research/boot_path.md` is the other half of any partition, and the two together are
what answer the question; neither alone is enough.

## What was landed from this block

Two of the 40, both `Matching`, both byte-exact, both chosen because they have no callees and
so no blast radius:

| unit | range | bytes | flip_test |
| --- | --- | --- | --- |
| `MetroidPrime/Player/CPlayerGetPlayerIndex.cpp` | `0x8000D084..0x8000D08C` | 8 | `PASS -> kept as Matching` |
| `MetroidPrime/Player/CPlayerGetTweakPlayer.cpp` | `0x8000BF94..0x8000BFAC` | 0x18 | `PASS -> kept as Matching` |

They are two units rather than one because they are 0x10D8 apart and a `configure.py` unit
cannot claim two discontiguous ranges. Both needed `include/MetroidPrime/Player/CPlayer.hpp`
to name two fields that were inside a single `char m_pad_6[0x1A8]`, and that split turned up
something the header got wrong independently of this work: **the comment put `m_pad_6` at
0x1320 and it was actually at 0x131C.** The four bytes of alignment in front of it were not in
the comment. The offsets in the tail of `CPlayer.hpp` are not reliable and a mwcceppc probe is
the only source.

The same pair also corrected a wrong C++ model that a future lane will hit: retail's
`CStateManager::ObjectById` (0x80041968) begins `lhz r0,0(r4)` - it **dereferences** its
argument - so retail's parameter is a reference or pointer, not the `TUniqueId` by value that
`include/MetroidPrime/CStateManager.hpp:133` declares. Every caller in retail copies the id to a
stack slot and passes its address, which is what MWCC does for a reference bound to a
temporary. `CPlayerGun::GetPlayer`/`GetPlayerFromAll` (0x801DD758, 0x801DD724, 104 contiguous
bytes) cannot be written byte-exactly until that declaration is fixed, and the fix touches
every call site in the tree, so it was not attempted here.

## Negative results

- **The 40 are not the bottleneck for a first frame, and spending the budget here would have
  been wrong.** `docs/research/boot_path.md`'s cheapest-order list (1-5) does not mention any of
  them, and measuring that took one tool run.
- **`CPlayerGun::CPlayerGun` is a stub.** `src/MetroidPrime/PortLinkStubs.cpp:243` stubs it. That
  is sound today only because nothing in the tree calls `CPlayer::CPlayer`; it is the same
  object-level blindness described above, and it is worth knowing that a future lane which
  writes a caller of `CPlayer::CPlayer` will find a 0-byte constructor already satisfied.
- `CPlayer::GetPlayerIndex` reads +0x13B8 and `CPlayer::GetTweakPlayer` reads +0x1320. These are
  **different** fields, so `GetPlayerIndex` does not return the value `GetTweakPlayer` tests.
  What either one means is not established - the constructor parameter that reaches +0x1320
  (`stw r22,4896(r31)` at 0x8001BBD8) was not traced - so both are named by offset.
- The port still does not link and no frame has been rendered. See the `link_check.sh` numbers
  in the lane report.
