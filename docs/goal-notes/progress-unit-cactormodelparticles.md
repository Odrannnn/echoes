# progress-unit-cactormodelparticles

**Unit: `MetroidPrime/CActorModelParticles` — 52/77 → 57/77 functions matched (+5).**
Verified with `./tools/goal_check.sh build/goal/item.json` → `PASS`, every check green:

```
ok  no judge-owned path touched
ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok  counts: matched 12162 -> 12167   linked 5860 -> 5860
ok  check_symbol_names.py
ok  All:  34.35% fuzzy, 27.60% matched, 12.89% linked (12167 / 28465 functions)
ok  target rose: main/MetroidPrime/CActorModelParticles: 52 -> 57 / 77 functions
ok  no asm added
```

Only `src/MetroidPrime/CActorModelParticles.cpp` changed (+53/-4). No header, no config,
no `configure.py`, no `asm`.

## Re-measured first

`item.json`'s reason said 50/77. The clean tree's own judge baseline
(`build/goal/judge/report.base.json`, `main/MetroidPrime/CActorModelParticles`) says **52/77**,
25 functions unmatched:

```
 98.6 1160  UpdateOnFire__Q220CActorModelParticles5CItemFfP6CActorR13CStateManager
 90.9  248  StartBurnDeath__20CActorModelParticlesFR6CActorR13CStateManager
 42.0  508  __ct__Q220CActorModelParticles5CItemFRC7CEntityR20CActorModelParticles
  0.5 1224  UpdateImplosion__Q220CActorModelParticles5CItemFfPC6CActorR13CStateManager
  0.2 1676  GeneratePoints__Q220CActorModelParticles5CItemFRC13CSkinnedModelRC18SSkinningWorkspacei
  0.0  ...  fn_8014FD70 140, fn_8014F9CC 72, fn_8014CEF0 112, fn_8014CD30 12,
            fn_8014CCE4 76, fn_8014CC98 76, fn_8014C554 88, fn_8014C4A0 180,
            fn_8014C450 80, fn_8014C3EC 100, fn_8014C208 484, fn_8014C15C 100,
            fn_8014C0EC 112, fn_8014C0AC 64, fn_8014BAD4 76, fn_8014BAAC 40,
            fn_8014C1E0 40, fn_8014C1C0 32, fn_8014BA8C 32, fn_8014BA8C 32, fn_8014BA44 72
```

`Render` and `AddStragglersToRenderer` (the reason lists them at 0.7%/1.0%) are already at 100%;
the reason is stale on those two.

## The finding: 20 of the 25 are dtk-named template instantiations we already emit

`nm` on the retail-derived object vs ours:

```
retail-only names : fn_8014BA44 fn_8014BA8C fn_8014BAAC fn_8014BAD4 fn_8014C0AC fn_8014C0EC
                    fn_8014C15C fn_8014C1C0 fn_8014C1E0 fn_8014C208 fn_8014C3EC fn_8014C450
                    fn_8014C4A0 fn_8014C554 fn_8014CC98 fn_8014CCE4 fn_8014CD30 fn_8014CEF0
                    fn_8014F9CC fn_8014FD70
ours-only names   : construct<CItem>, construct<CSystem>, construct_impl<CSystem>,
                    destroy<CSystem>, destroy/destroy_impl<pointer_iterator<CRainSplashGenerator::
                    SRainSplash>>
```

**objdiff pairs functions by NAME, and nothing else.** Measured, not assumed: of the 212
fully-matched units in `build/report.json` that contain `fn_*` functions, 243 of 244 have a
literal symbol of that name in `build/G2ME01/src/<unit>.o` (the single exception is
`CGunController`'s `fn_801DC820`). So an unnamed retail function can only ever match if our
object exports that name — which is why `report_diff.py`'s own docstring calls "giving an
unpaired 0.00% function the name its body actually has" one of the two cheapest possible
improvements.

Six of them turned out to be byte-identical (same size, same instruction sequence, only the
`bl` displacements differ) to functions this file **already emits under their mangled names**:

| retail  | size | our mangled name                  | what it is |
|---------|------|-----------------------------------|------------|
| `fn_8014BA44` | 0x48 | `push_back__Q24rstl51reserved_vector<Q220CActorModelParticles7CSystem,8>FRC...` | `rstl::reserved_vector<CSystem,8>::push_back` |
| `fn_8014BA8C` | 0x20 | `construct<Q220CActorModelParticles7CSystem>__4rstlFPvRC...` | `rstl::construct<CSystem>` |
| `fn_8014BAAC` | 0x28 | `construct_impl<Q220CActorModelParticles7CSystem>__4rstlFPvRC...` | `rstl::construct_impl<CSystem>` |
| `fn_8014C1C0` | 0x20 | `construct<Q220CActorModelParticles5CItem>__4rstlFPvRC...` | `rstl::construct<CItem>` |
| `fn_8014C1E0` | 0x28 | `construct_impl<Q220CActorModelParticles5CItem>__4rstlFPvRC...` | `rstl::construct_impl<CItem>` |

The retail `bl` chain confirms it independently: `fn_8014BA44 → fn_8014BA8C → fn_8014BAAC →
fn_8014BAD4` and `fn_8014C15C → fn_8014C1C0 → fn_8014C1E0 → fn_8014C208`, i.e.
`push_back → construct → construct_impl → copy-ctor` and
`create_node → construct → construct_impl → CItem copy ctor`.

`include/rstl/reserved_vector.hpp` already prescribes exactly this treatment and says why:

> retail's `reserved_vector<T, N>::operator=` is an out-of-line symbol that no caller can name (a
> template instantiation is emitted under its mangled name, so objdiff never pairs it with the
> retail symbol), so it has to be written out by hand in a .cpp under an `extern "C"` name

so `mCount`/`mData` are public. Each of the five is now written out under its retail name, with
the real body — no transcription, no assembly:

* `fn_8014BA8C` / `fn_8014C1C0` — `rstl::construct<T>(dest, src)`. mwcceppc emits `construct`
  out of line and calls it, so a one-line call here is retail's 8-instruction forwarder. 100%.
* `fn_8014BAAC` / `fn_8014C1E0` — `new (dest) T(src)`. Spelled as a bare placement new on
  purpose: mwcceppc's placement new carries the null test, so `if (dest != nullptr) new (dest)
  T(src);` emits **two** `beq`s (measured 90.00%, 44 bytes) and `rstl::construct_impl<T>(dest,
  src)` alone emits an 8-instruction forwarder with no test (80.00%, 32 bytes). The bare
  placement new is the only spelling that lands on retail's 10 instructions / 40 bytes. 100%.
* `fn_8014BA44` — `rstl::construct(vec->data() + vec->mCount, src); ++vec->mCount;`, which is
  `reserved_vector<CSystem,8>::push_back`'s body verbatim and explains retail's `lwz r0,0(r3)`
  (`mCount` at +0) / `mulli 24` / `add r3,r31,r0` / `addi r3,r3,4` (`mData` at +4). Forwarding to
  `vec->push_back(src)` instead would be a 0x20-byte forwarder. 100%.

Placed in descending retail-offset order, checked with
`python3 tools/check_decl_order.py --unit MetroidPrime/CActorModelParticles` →
`ok: 1 unit(s) checked, none emits its functions out of retail order`.

## Per-function, before → after

| function | before | after |
|---|---|---|
| `fn_8014BA44` | 0.00% (72 B) | **100.00%** |
| `fn_8014BA8C` | 0.00% (32 B) | **100.00%** |
| `fn_8014BAAC` | 0.00% (40 B) | **100.00%** |
| `fn_8014C1C0` | 0.00% (32 B) | **100.00%** |
| `fn_8014C1E0` | 0.00% (40 B) | **100.00%** |
| `StartBurnDeath` | 90.87% (248 B) | 93.55% (248 B) — real bug fixed, see below |
| everything else | unchanged | unchanged |

Unit: fuzzy 71.78% → 72.99%, matched_functions 52 → 57, total_functions 77.

## `StartBurnDeath`: a real sfx bug, fixed (90.87% → 93.55%, still not 100%)

Retail (0x8014B174-0x8014B18C) is

```
cntlzw r0,r0 ; rlwinm r0,r0,27,31,31 ; neg r3,r0 ; addi r0,r3,9602 ; clrlwi r29,r0,16
```

i.e. `state == 0 ? 9601 : 9602`, with `neg` + the *false* arm added. `kMS_Unmorphed` is 0
(`include/MetroidPrime/Player/CPlayer.hpp:70`), so **unmorphed plays 9601**. The old source had
`== kMS_Unmorphed ? 9602 : 9601` — the two ids swapped, and the compiler emitted
`srwi r3,r0,5 ; addi r0,r3,9601`, which is 4 bytes shorter than retail's (90.87%, 244 B vs 248 B).
Swapping the arms gives retail's `rlwinm`/`neg`/`addi 9602` and the right size. Kept.

## WALL: StartBurnDeath 93.55% and UpdateOnFire 98.62% - argument scheduling on `CSfxManager::AddEmitter`

Both are down to the *same* two-instruction permutation, and nothing else. Retail
(0x8014B1B0 and 0x80142908) is

```
lwz r6,4(r30)      ; area id, from the actor
addi r3,r1,8       ; sret slot for the discarded CSfxHandle
lha r9,0(0)        ; kMedPriority
clrlwi r4,r29,16 ; addi r5,r30,84 ; li r7,1 ; li r8,0 ; bl
```

ours emits `lha r9` first and the `lwz r6` after the `addi r3`. `UpdateOnFire`'s *only*
difference in 290 instructions is this swap (`python3 .tmp/fndiff.py` on the two objects; every
other diff line is a branch target that moved with the function's own address). The relocations
are the same function and the same constant in both
(`AddEmitter__11CSfxManagerFUsRC9CVector3fibbs`, `kMedPriority__11CSfxManager`).

16 spellings tried in this run, all 93.55% / 98.62%, none moving the `lha` past the sret slot:
named `const ushort sfx16`; named `const uint areaId`; named `const CVector3f& pos`; all three
together; `const short priority = kMedPriority` and passing `priority`; `static_cast<int>` on the
area id; `static_cast<short>` on the priority; dropping the `static_cast<ushort>` (92.58%, worse);
`static_cast<short>(sfx)`; `int sfxInt`; `1, 0` instead of `true, false`; named bools; a named
`const CSfxHandle handle` plus `(void)handle`; and omitting `kMedPriority` to take the default
argument. Only the *relative* order of `lwz r6` and `addi r3` ever moved; the `lha r9` never did.
This is mwcceppc's scheduler, not the source.

## Measured, left for a later run

* `fn_8014BAD4` (76 B) is `CSystem::CSystem(const CSystem&)` — its only `bl` is
  `__ct__vector<CToken>(const vector&)`, then `mRefCount`/`mLoaded`. Written out as
  `new (&self->mTokens) rstl::vector<CToken>(src.mTokens);` + the two field copies it reaches
  **88.68%**: mwcceppc's placement new adds a `mr. r30,r3 ; beq` retail does not have, and
  `self->mTokens = src.mTokens` is `vector::operator=`, a different (much larger) body. Not kept
  in the diff because it does not reach 100%.
* `fn_8014C0AC` (64) / `fn_8014C0EC` (112) / `fn_8014C15C` (100) are byte-identical to our
  `insert<list<CItem>>`, `do_insert_before<list<CItem>>` and `create_node<list<CItem>>`.
  `rstl/list.hpp`'s members are public for exactly this reason, so these three are writable by
  hand; not attempted here for time.
* `fn_8014CD30` (12 B, `lbz r0,1116(r3) ; rlwinm r3,r0,25,31,31 ; blr`) is the weak out-of-line
  copy of a `CParticleElectric` virtual: the only `fn_*` in the unit with a data reference
  (DOL file offset `0x3b81c8` = `0x803BB1C8`, which is entry 20 of `__vt__17CParticleElectric` at
  `0x803BB178`). `1116` = `0x45C`, one past the end of `CParticleElectric` (`CHECK_SIZEOF` 0x460)
  and bit 7 of that byte is the first bitfield of the trailing `bool mEmitting : 1` cluster —
  mwcceppc allocates bitfields from the MSB — so it returns `mEmitting`, i.e. it is
  `GetParticleEmission()`. Our header already has that accessor inline, so our object never
  emits the weak copy; forcing it needs the TU to instantiate the vtable. Not attempted.
* `fn_8014CC98` / `fn_8014CCE4` (76 B each) are `CParticleElectric::SetOverrideIPos` /
  `SetOverrideFPos`: `if (flag) { vec = v; } else { vec = v; flag = 1; }` at `+420/+432` and
  `+388/+400`, matching `rstl::optional_object<CVector3f>`'s shape. They are only reachable from
  `GeneratePoints`, which is still the `// TODO` stub (0.24%), so nothing calls them in our build.
* `fn_8014C208` (484 B) is `CItem::CItem(const CItem&)` (our `__ct__…FRC…` is 0x1E4 = 484 bytes,
  byte-identical) and `fn_8014F9CC` (72 B) is `rstl::optional_object`'s steal-if-empty
  `operator=`; both need a 484-byte / 72-byte body written out by hand.
* `__ct__CItem` (42.04%, 508 B): retail is 127 instructions against our 166 — retail loops over
  the eight `mOnFireGens` pairs where our source's `rstl::pair(auto_ptr(), 0)` argument unrolls
  all eight. Prime 1's donor at `/run/media/odran/Leo/projects/Restored-projects/Chatgpt/
  prime-ref/src/MetroidPrime/CActorModelParticles.cpp:94-116` has the same unrolled shape, so the
  loop has to come from the C++ spelling, not from Echoes' extra members.
* `UpdateImplosion` (0.46%, 1224 B) and `GeneratePoints` (0.24%, 1676 B) are the two TODO stubs;
  both are multi-hundred-instruction bodies and out of reach for one item.

No `NEW:` filed: everything left in this unit is this item's own remainder, and a `NEW:` whose
target is `MetroidPrime/CActorModelParticles` would be a restatement of it.