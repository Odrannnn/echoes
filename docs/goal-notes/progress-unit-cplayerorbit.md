# progress-unit-cplayerorbit

## Run 2 (this run)

`MetroidPrime/Player/CPlayerOrbit`, **seven** functions taken to an exact match. The unit went
**22/60 -> 29/60 matched functions**, fuzzy **6.36% -> 11.46%**. Judge: `goal_check: PASS`
(gate ok including DOL sha1 `6ef9b491...`, 86 RELs, port probe 747 files / 291 undefined
unchanged; matched 12103 -> 12110; linked 5849 -> 5849; no asm; decl order ok).

| function | addr | before | after | the spelling that mattered |
| --- | --- | --- | --- | --- |
| `ActivateOrbitSource` | 0x801222D8 | 4.17% | 100% | see "case order is a switch-lowering detail" below |
| `CheckOrbitDisableSourceList(const CStateManager&)` | 0x80121864 | 3.41% | 100% | p1 shape verbatim: erase + `it = begin()` restart, `!empty()` return |
| `UpdateOrbitPosition` | 0x8011EF50 | 2.00% | 100% | `SetOrbitPosition(distance)` takes no mgr here; actor branch needs both tests |
| `SetOrbitTargetId` | 0x8011EE74 | 1.82% | 100% | two **named locals** (see below) |
| `AddOrbitDisableSource` | 0x801219EC | 1.92% | 100% | `const TUniqueId orbitTarget = GetOrbitTargetId();` in a local |
| `BeginGrapple` | 0x8011E768 | 2.44% | 100% | p1 donor applies as-is |
| `fn_8011eac4` | 0x8011EAC4 | 2.94% | 100% | **unsigned** loop bound (see below) |

Files touched: `src/MetroidPrime/Player/CPlayerOrbit.cpp`,
`include/MetroidPrime/Player/CPlayer.hpp` (one enum value), `include/MetroidPrime/Cameras/
CFirstPersonCamera.hpp` (one setter), `src/MetroidPrime/PortGlobals.cpp` (one cast, see the gate
note). No `asm`.

## Things that cost time, for the next run

**Case order inside a `switch` changes the emitted decision tree, and it has to match retail's
order or the bytes differ.** Retail `ActivateOrbitSource` tests `mOrbitSource` (0x58c) as
`cmpwi 1 / beq`, then `bge`, then `cmpwi 3 / bge`, then `b` - a three-step tree over cases
{0,1,2}. MWCC builds that tree from the order the labels appear in the source. Measured, all
96-byte body otherwise identical:

| source order | score |
| --- | --- |
| `case 0: default:` first, then `case 1`, then `case 2` | **100%** |
| `case 0: default:`, `case 2`, `case 1` | 99.50% |
| `case 2`, `case 1`, `case 0: default:` | 84.17% |
| `case 1`, `case 0: default:`, `case 2` | 82.71% |
| `case 1`, `case 2`, `case 3: default:` | 82.00% |
| `case 2`, `case 1`, `case 3: default:` | 82.13% |
| `case 1`, `case 2`, plain `default:` | 78.46% |
| `case 2`, `case 1`, plain `default:` | 78.38% |
| `if / else if / else` | 57.04% |
| `case 1,2,0` each with its own body (no shared `default:`) | 66.00% |

The rule that fits: **list the cases in ascending value order and put the shared body on the
*first* (lowest) case plus a bare `default:`**. Note `SetOrbitState` (332 B, measured 98.63%,
not landed) uses the *descending* order - OrbitObject, OrbitCarcass, NoOrbit, OrbitPoint -
because retail's tree there tests 2, then 0, then 4. So the order is per-function and must be
read off retail, not guessed from p1.

**`SetOrbitTargetId` needs both casts materialised into locals, in the p1 spelling.** Writing the
`||` inline is 84.98%: retail keeps the first cast's result live in `r31` across the *second*
call, which only happens with two named locals.

```cpp
const CPatterned* patterned = TCastToConstPtr< CPatterned >(mgr.GetObjectById(target));
const CSwarmBasics* swarm = TCastToConstPtr< CSwarmBasics >(mgr.GetObjectById(target));
if (patterned || swarm) { ... }
```

Retail's Echoes version has only two casts (Prime 1 has four) and also writes `x591_` (`bool`
at 0x591): `true` whenever the id was valid, `false` again when the id is invalid at the end -
so the two writes are independent, not an else.

**A `TUniqueId` local is worth four instructions of stack traffic.** `AddOrbitDisableSource`
needs `const TUniqueId orbitTarget = GetOrbitTargetId();` as a local (100%). Inlining the getter
into the `GetObjectById` argument is 99.90%: the two `sth` to the argument-save slots land at
different displacements. Same for `mgr.GetObjectById(mOrbitTargetId)` (the member, no local):
98.06%.

**One `static_cast` on the loop bound.** `fn_8011eac4` walks `mgr.GetNumPlayers()` comparing
player unique ids and orbit target ids. Retail emits `cmplw r30,r0` (unsigned); `int i` against
`int GetNumPlayers()` emits `cmpw` and scores 98.24%. `static_cast< uint >(mgr.GetNumPlayers())`
on the bound, with `int i`, is 100%. `uint i` is 92.00%, a hoisted `const uint numPlayers` is
85.18%, and a `while` loop is 82.88%. The interesting part: it is the *bound* that must be
unsigned, not the counter.

## Measured but not matched

`SetOrbitState` (332 B, **98.63%**) is now a near-miss and is the best next target. Everything
matches except the stack displacement of the carcass-case `CVector3f` temp: retail puts it at
`24(r1)` and this repo's build puts it at `20(r1)`, one 4-byte slot lower, because retail
materialises `mOrbitNextTargetId`'s value in `16(r1)` before storing it to `0x3dc` and this
repo's does not. Spellings tried and their scores: p1 order 98.63 (best, kept), ascending case
order 56.76, `mOrbitNextTargetId` also cleared in NoOrbit 93.81, that plus OrbitPoint 95.92
(via `SetOrbitState` local-first ordering), a hoisted `const TUniqueId next` 93.81, a hoisted
`const TUniqueId invalid` used by all three `SetOrbitTargetId` calls 88.70,
`playerToPoint = mOrbitPoint; playerToPoint -= GetTranslation();` 88.89, two locals `playerToPoint`
+ `flat` 98.63, `mOrbitNextTargetId = TUniqueId(kInvalidUniqueId)` 98.63. A `SetOrbitNextTargetId`
inline setter would need adding to `CPlayer.hpp` first (there is none; the p1 donor calls one).

`BreakGrapple` (252 B, 1.59%), `UpdateOrbitOrientation` (308 B, 1.30%), `SetOrbitRequest`
(360 B), `WithinOrbitScreenBox`/`WithinOrbitScreenEllipse` (404/416 B), `UpdateOrbitSelection`
(336 B), `UpdateAimTarget` (348 B) are still stubs. `BreakGrapple` is mostly p1's body with two
Echoes substitutions and has not been tried at all yet - that is the cheapest unstarted one.
`fn_80121908` (120 B), `fn_80123334` (104 B), `fn_8012339C` (372 B) and `fn_8011c3c0` (1608 B)
remain pure stubs; `fn_80121908` disassembles as a `reserved_vector` erase helper
(`0x80121908`, takes the vector at r3 and an iterator at r4) and is presumably a template
instantiation this unit is expected to emit.

## Gate note (a NEW cost worth recording)

Adding `TCastToConstPtr<CSwarmBasics>` to `SetOrbitTargetId` **grew the port link's undefined
count by one** and failed the whole item until fixed. `TypesMatch.cpp`, which holds that
`TCastToPtr`, is deliberately not in `files.cmake`, so a new `TCastToPtr<CSwarmBasics>` reference
is a new undefined symbol. The fix is a definition in `src/MetroidPrime/PortGlobals.cpp`, next to
the existing `TCastToPtr<CPatterned>`:

```cpp
template <>
CSwarmBasics* TCastToPtr< CSwarmBasics >(CEntity* entity) {
  return reinterpret_cast< CSwarmBasics* >(TryCast(entity, kET_SwarmBasics));
}
```

`reinterpret_cast` is required, not `static_cast`: `include/MetroidPrime/Enemies/CSwarmBasics.hpp`
is a partial ABI declaration that deliberately does not name `CActor` as a base, so
`static_cast<CSwarmBasics*>(CEntity*)` does not compile. Retail's body is the ordinary type-id
wrapper with `li r4,102` (`tools/dis.sh 0x80098908 0x24`); `TypesMatch.cpp`'s
`CAST_TO_IMPL_INCOMPLETE` covers the same situation. Any future item in this repo that calls a
`TCastToPtr` for a class outside `TypesMatch`'s port-visible set will hit this the same way:
check `build-port-link/link_undefined.txt` before assuming the gate is about your unit.