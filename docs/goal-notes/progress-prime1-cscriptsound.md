# progress-prime1-cscriptsound — MetroidPrime/ScriptObjects/CScriptSound

Lane 3, worktree `../wt-mp2-goal-L3` on `goal/lane-3`. One file changed:
`src/MetroidPrime/ScriptObjects/CScriptSound.cpp` (+42 / -3). No other file touched, no asm,
no `tools/`, no `docs/`.

## Measured result

`build/report.json`, unit `main/MetroidPrime/ScriptObjects/CScriptSound`:

| | before (`build/goal/judge/report.base.json`) | after |
|---|---|---|
| `matched_functions` | **2** / 15 | **3** / 15 |
| `fuzzy_match_percent` | 19.35 | 35.57 |
| `matched_code` | 180 / 6140 | 320 / 6140 |

DOL-wide: `All: 30.51% fuzzy, 22.57% matched, 11.74% linked (9909 / 28465 functions)`, i.e.
`matched_functions` 9908 -> 9909, `linked` unchanged at 4895.

`python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`:

```
matched  9908 -> 9909   linked 4895 -> 4895   (+1 functions at 100%, 0 units newly linked)
  +100%    main/MetroidPrime/ScriptObjects/CScriptSound :: StopSound__12CScriptSoundFR13CStateManager
no regression
```

`./tools/gate.sh build/goal/judge/report.base.json`: `configure ok`, `ninja + build.sha1 ok`,
`hashes vs config.yml ok`, `report ok`, `per-function diff ok`, `module wiring ok`,
`dol_read ok`, `gs offsets ok`, `raw offsets ok`, `decl order ok`, `files.cmake ok`,
`module order ok`, `port probe ok`, `port link gap ok`, `reach stubs ok`. The only FAIL is
`docs claims`, and it is only the two derived numbers the driver rewrites
(`'matched    9909 / 28465 functions'` and `'DOL units  8498 / 16726 functions'`, both up by
one) — expected, and the brief says the judge rewrites the state block itself.

The unit stays `NonMatching`; no `flip_test` was run (this is a `progress` item).

## Per function: before% -> after%, and how Prime 1's source fared

| function | before | after | Prime 1's source |
|---|---|---|---|
| `StopSound__12CScriptSoundFR13CStateManager` | 8.14% | **100%** (matched) | **matched unchanged**, one line |
| `GetOccludedVolumeAmount__12CScriptSoundFRC9CVector3fRC13CStateManager` | 0.64% | 99.70% | structure, needed edits (below); 5 instructions short |
| `Think__12CScriptSoundFfR13CStateManager` | 0.28% | 0.28% | not attempted, see below |
| `AcceptScriptMsg__12CScriptSoundFR13CStateManagerRC10CScriptMsg` | 61.64% | 61.64% | not attempted, see below |
| `PlaySound__12CScriptSoundFR13CStateManagerPC10CScriptMsg` | 0.45% | 0.45% | not attempted |
| `SetMaxVolume__12CScriptSoundFs` | 3.78% | 3.78% | no Prime 1 analogue |
| `__ct__` / `PreThink` / `__dt__` | 94.31 / 100 / 100 | unchanged | n/a |
| `fn_8009DCE8`, `fn_8009DD94`, `fn_8009E0EC`, `fn_8009EB08/18/24` | 0% | 0% | still TODO stubs, untouched |

### StopSound — Prime 1's body verbatim, 100% first try

`prime-ref/src/MetroidPrime/ScriptObjects/CScriptSound.cpp:218-227` is exactly Echoes' body
(Echoes added nothing here). Copied as-is; needed only the includes
`MetroidPrime/CWorld.hpp` and `Kyoto/Audio/CSfxManager.hpp`. 8.14% -> 100%.

Worth recording: retail passes `CSfxHandle` to `CSfxManager::RemoveEmitter` **by value through a
stack temp** (`addi r3,r1,8; stw r0,8(r1); bl RemoveEmitter`). The repo's
`static void RemoveEmitter(CSfxHandle handle);` already reproduces that, so do not "fix" it into
a reference.

### GetOccludedVolumeAmount — Prime 1's structure, three Echoes-only edits

0.64% -> 99.70%. Prime 1's body is the same algorithm; Echoes adds an early-out and a different
material filter. The edits, each measured:

1. **Early-out.** `if (mgr.fn_80036F10()) return 1.f;` — no Prime 1 equivalent.
2. **`GetCurrentCameraTransform` takes a second arg** in Echoes: `GetCameraManager(0)->GetCurrentCameraTransform(mgr, true)`. Prime 1 has the 1-arg form.
3. **`up` is the static `CVector3f::Up()`** (`sUpVector__9CVector3f`, `.bss:0x804174BC`), not Prime 1's `const CVector3f up(0.f, 0.f, 1.f)`. This is the single biggest win in the function: with the literal the compiler folds the components and never emits the `lis`/`addi`/`lfs` triple retail has (95.31% -> 99.52%). A `const CVector3f&` binding and a by-value copy both reproduce the three `lfs` from `r6`.
4. **The filter materials are not Prime 1's.** Retail's `CMaterialFilter` static initialises from two `.sdata` shift amounts at `_SDA_BASE_-32160 = 0x14 = 20` and `-32164 = 0x3b = 59`, i.e. `MakeIncludeExclude(CMaterialList(kMT_Unknown59), CMaterialList(kMT_NoPlatformCollision))` — the first `__shl2i` result lands at `+0` so 59 is *include* and 20 is *exclude*. Prime 1's `kMT_Solid` / `kMT_ProjectilePassthrough` would be 19/18 and cannot match.
5. `mgr.fn_80036F10()` and the `IsValid()` polarity (`lbz` at `+40`, increment when zero -> `!result.IsValid()`) are Echoes-specific; Prime 1 uses `result.IsInvalid()`.

**The wall (5 instructions, register allocation only).** Everything from `0x00` to `0xf4` and from
`0x16c` to the epilogue is byte-identical, and so is the whole double loop. The only difference is
which register the reciprocal gets:

```
ours                              retail
0x10c  fdivs   f8,f4,f27          0x10c  fdivs   f5,f4,f27
0x124  fmuls   f4,f8,f3           0x124  fmuls   f4,f5,f3
0x12c  fmuls   f1,f8,f0           0x12c  fmuls   f1,f5,f0
0x134  fmuls   f3,f8,f2           0x134  fmuls   f3,f5,f2
0x138  fmuls   f0,f5,f4           0x138  fmuls   f0,f7,f4      (up.y)
0x140  fmadds  f0,f7,f1,f0        0x140  fmadds  f0,f6,f1,f0    (up.x)
0x14c  fmadds  f0,f6,f3,f0        0x14c  fmadds  f0,f8,f3,f0    (up.z)
0x15c-0x164  fsubs f2,f5,f2 / f1,f6,f1 / f0,f7,f0   ->  f6,f2 / f7,f1 / f8,f0
```

Both allocate the same block `{f5,f6,f7,f8}` for the four temporaries
`(inv, up.x, up.y, up.z)`; retail gives the *lowest* to `inv`, we give it to the highest. The
`1.0` literal already lands in `f4` in both, so this is not a constant-selection difference.
`norm.x=f1, norm.y=f4, norm.z=f3, dot=f0` are already identical, i.e. the loads and the
`fmadds` chain are right — only the register the `fdivs` writes is wrong.

**Spellings tried, all 99.70% unless stated** (a next run should skip these):

| spelling | score |
|---|---|
| `soundToCam * (1.f / soundToCamMag)`, `const CVector3f up(0.f,0.f,1.f)` | 95.31% |
| + `up` as `CVector3f::Up()`, reciprocal still inline | 99.52% |
| named `invMag`, `up` as a value copy | 99.70% |
| named `invMag`, `up` as a `const CVector3f&` | 99.70% |
| named `invMag`, `up` as a `const CUnitVector3f&` | 99.70% |
| named `invMag`, `up` as a `CUnitVector3f` value | 99.70% |
| named `invMag`, `const float upDot = Dot(up, soundToCamNorm);` split out | 99.70% |
| named `invMag`, non-`const` `float invMag` | 99.70% |
| `invMag * soundToCam` (scalar-first `operator*`) | 99.70% |
| `up` declared *before* `invMag` | 99.70% |
| `thirdEdge` spelled `CVector3f::Up() - ... CVector3f::Dot(CVector3f::Up(), ...)` | 99.70% |
| explicit `CVector3f(x*inv, y*inv, z*inv)` | 99.70% |

I did **not** write a `WALL:` line: the brief scopes that to `match` items, and this is a
`progress` item whose count rose. The spellings and the exact remaining diff are above so a
requeued run does not repeat them.

## Not attempted, and why (so a requeued run does not re-derive it)

`Think` is 1428 bytes and is **not** Prime 1's function — Echoes added `IsQueued`, a moving
emitter (`mPositionSources`, `mEmitterPosition`, the static `fn_8009DD94`), the
`mDarkVisorVolume` visor blend and the `mScaleByMusicVolume` tail. It needs `CSfxManager`
entry points (`UpdateEmitter(handle, pos, dir, uchar)` — the handle goes by reference, the
`CVector3f::Zero()` second vector is `sZeroVector__9CVector3f`, `.bss:0x804174B0` — plus
`SfxVolume`, `SfxSpan`, `PitchBend`, `IsQueued`) and a reading of the visor interpolation at
`0x8009EFE0`-`0x8009F11C`, which needs a `CPlayer` visor-state field. One item's worth of work
at least.

`AcceptScriptMsg` is at 61.64% and is *nearly* Prime 1's dispatcher, but its `kSM_XALD` arm is
not: retail walks a 12-byte-element container living in `CEntity` at `+0x14` (count) / `+0x1C`
(begin) — `rstl::vector<SConnection> m_conns` is the only candidate and the header's
`// x10` comment plus the 12-byte stride do not line up with what the code does — filters each
element against the magic `0x43174E4E` and pushes `mgr.GetIdForScript(...)` results into
`mPositionSources`. Until that container is identified this arm cannot be written honestly.
The other five arms are Prime 1's verbatim (`kSM_Stop`/`kSM_Deactivate` -> `StopSound`,
`kSM_Play` -> `PlaySound`, `kSM_Activate` -> `mPlayRequested`, and `kSM_XCRT` is Prime 1's
`kSM_Registered` arm including `mSelfFree = <mgr flag at +20>`). Note the repo's message enum
uses 4-char tags; retail compares against `0x53544F50 "STOP"`, `0x44435456 "DCTV"`,
`0x41435456 "ACTV"`, `0x504C4159 "PLAY"`, `0x58435254 "XCRT"`, `0x58414C44 "XALD"`,
`0x5844454C "XDEL"`.

No `NEW:` line is filed: everything still outstanding is inside this item's own target unit, so
the driver can requeue it, and a `NEW:` target must be a unit, module or symbol.

## Codegen facts worth keeping (measured, not recalled)

- **MW tests a `bool` with `clrlwi. r0,rX,24` + a branch**, not `cmpwi`. 180 of the 188 call
  sites of `fn_80036F10` in `main.elf` are followed by exactly that, and
  `src/MetroidPrime/CRumbleManager.cpp:18` (`if (mgr.fn_80036F10())`) is a `Matching` unit — so
  `if (mgr.fn_80036F10())` is the right spelling and the masked form is not a bitfield test.
  Reading a `bool` *member* and testing it uses the same shape with `clrlwi. r0,rX,31`.
- `CVector3f::Up()` is `sUpVector__9CVector3f` at `.bss:0x804174BC`; `CVector3f::Zero()` is
  `sZeroVector__9CVector3f` at `.bss:0x804174B0`. Retail loads them from `.bss`, so any source
  that wants a *memory* vector constant must go through these accessors, not a literal.
- `__shl2i(r3=hi, r4=lo, r5=shift)` is MW's 64-bit shift helper and it ORs the `lo << (shift-32)`
  term in unconditionally, so `u64(1) << m` for `m < 32` comes out as `(1<<m) | ((1<<m)<<32)`.
  That is why a `CMaterialList` built from a single material shows up in the filter as
  `0x0800000008000000` for material 59, and it is worth remembering before concluding that a
  `.sdata` word loaded as a shift amount is not a material index.
- `CStateManager::GetCameraManager(0)` is `lwz rX,5404(mgr)`; the `+0x151C` load. `GetWorld()` is
  a different offset, and `CWorld::StopGlobalSound` takes the `ushort` sound id by value in `r4`.
- `CRayCastResult::mValid` is at `+40`, so `IsValid()` is the cheap one to write: retail
  `lbz`s it directly and increments the invalid count when it is zero.
- `CTransform4f`'s translation is matrix elements 3, 7, 11 (byte offsets `+12`, `+28`, `+44`),
  which is why retail reads the camera position with three `lfs` at those offsets instead of
  one contiguous 12-byte load.
