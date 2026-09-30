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

---

# Lane 8, worktree `../wt-mp2-goal-L8` on `goal/lane-8` — 2026-09-30

One file changed: `src/MetroidPrime/ScriptObjects/CScriptSound.cpp` (+184 / -11). No other file
touched by me, no asm, no `tools/`, no `config/`, no `configure.py`. (`docs/HANDOFF.md` shows as
modified in `git status` because `tools/goal_check.sh` rewrites its derived state block; the brief
says the driver/judge does that itself.)

## Measured result — `tools/goal_check.sh build/goal/item.json` → **PASS** (exit 0)

| | before (`build/goal/judge/report.base.json`) | after |
|---|---|---|
| `matched_functions` | **3** / 15 | **8** / 15 |
| `fuzzy_match_percent` | 35.574593 | 42.071663 |
| `matched_code` | 320 / 6140 | 692 / 6140 |

DOL-wide `All: 30.51% fuzzy, 22.57% matched, 11.74% linked (9909 / 28465)` on the item's recorded
baseline; **measured on this tree after the change**: `All: 31.50% fuzzy, 23.95% matched, 11.83%
linked (10371 / 28465 functions)`, i.e. `matched` 10366 → 10371 (+5), `linked` unchanged at 5048.

`python3 tools/report_diff.py <base> build/report.json`:

```
matched  10366 -> 10371   linked 5048 -> 5048   (+5 functions at 100%, 0 units newly linked)
  +100%  main/MetroidPrime/ScriptObjects/CScriptSound :: SetMaxVolume__12CScriptSoundFs
  +100%  main/MetroidPrime/ScriptObjects/CScriptSound :: fn_8009EB08
  +100%  main/MetroidPrime/ScriptObjects/CScriptSound :: fn_8009EB18
  +100%  main/MetroidPrime/ScriptObjects/CScriptSound :: fn_8009EB24
  +100%  main/MetroidPrime/ScriptObjects/CScriptSound :: fn_8009E0EC
no regression
```

Every gate `ok`, including `docs claims` this time (lane 3's run had that one FAIL on the two
derived numbers the judge rewrites; here the judge rewrote them first). `python3
tools/check_symbol_names.py` → `checked 505 units; 0 declared names are missing from their
object`. `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CScriptSound` → `ok:
1 unit(s) checked, none emits its functions out of retail order`. `tools/unit_fit.sh` reports 4
COMDAT-weak extras (`__dt__16CActorParametersFv`, `GetHealthInfo__6CActorCFv`, …, 280 bytes), the
harmless kind the tool itself documents.

The unit stays `NonMatching`; no `flip_test` was run (this is a `progress` item).

## The finding that unlocked this: objdiff pairs by name, and four of these have no mangled name

**The previous run treated all four `fn_*` bodies as unreachable "TODO stubs" and wrote 0.00% for
them. They were not unreachable — they were *unnamed*.** `config/G2ME01/symbols.txt:3108-3117`
carries only dtk's `fn_8009…` placeholders for these addresses, and objdiff pairs functions **by
name**, so a `static`/mangled definition pairs with nothing no matter how exact its bytes are. The
repo already documents the fix and the precedent: `src/MetroidPrime/main.cpp:1723-1785` spells
`fn_80009224`, `fn_80009008` and `fn_800095E4` as `extern "C"` under retail's own names *for this
reason*, and says so in the comment. Re-spelling four functions that way took the unit from 3 to 8
with no other change.

This is the general lesson and it is the expensive one: **before writing a body for a function that
reads 0.00%, check whether our object emits a symbol with that name at all.** `build/binutils/
powerpc-eabi-nm -n build/G2ME01/src/<unit>.o | grep fn_` answers it in one second. Lane 3 spent
its run on the register-allocation wall in `GetOccludedVolumeAmount` while four whole functions
scored 0.00% purely for want of a name.

## Per function: before% → after%, and what produced it

| function | before | after | what it took |
|---|---|---|---|
| `SetMaxVolume__12CScriptSoundFs` | 3.78% | **100%** | read off the disassembly; no Prime 1 analogue exists (Prime 1's `CScriptSound` has no `SetMaxVolume`) |
| `fn_8009EB08` | 0.00% | **100%** | `extern "C"` + the field it sets |
| `fn_8009EB18` | 0.00% | **100%** | `extern "C"` + the field it reads |
| `fn_8009EB24` | 0.00% | **100%** | `extern "C"`; it is a four-byte `CSfxHandle` assignment |
| `fn_8009E0EC` | 0.00% | **100%** | `extern "C"`; the out-of-line `CQuad` copy constructor |
| `fn_8009DD94` | 0.00% | 3.56% | `extern "C"` only — it now **pairs**; body still a TODO |
| `fn_8009DCE8` | 0.00% | 2.33% | `extern "C"` only — it now **pairs**; body still a TODO |
| `__ct__` | 94.31% | 94.31% | untouched |
| `GetOccludedVolumeAmount` | 99.70% | 99.70% | untouched — lane 3's wall, not re-attempted (see below) |
| `Think` / `PlaySound` / `AcceptScriptMsg` | 0.28 / 0.45 / 61.64% | unchanged | not attempted |

### `fn_8009EB08` / `fn_8009EB18` / `fn_8009EB24` — 16 + 12 + 12 bytes, the cheapest three in the unit

All three are 3-4 instructions and were fully decodable. The only real work was identifying which
member each touches, which needs this repo's own measured MWCC 2.7 bitfield convention:

* **write** of a 1-bit field at word bit N: `lbz` / `rlwimi rD,rS,31-N,N,N` / `stb`, value in
  `rS` bit 0. `rlwimi r0,r3,7,24,24` in this unit's 100%-matched `StopSound` clears
  `mPlayRequested` (bit 24) — N=24, 31-N=7. ✓
* **read** of a 1-bit field at word bit N: `lbz` / `rlwinm. rD,rS,N+1,31,31`, with the `.` record
  form when the result is branched on and the plain form when the `bool` is returned.
  `rlwinm. r0,r3,31,31,31` in `StopSound` tests `mWorldSfx` (bit 30) — N=30, N+1=31. ✓
  Cross-checked against the constructor's fourteen stores, which walk `+0x1A8` bits 24..31 then
  `+0x1A9` bits 24..29 in exactly the header's declaration order, so the header's bit order is
  retail's and needs no change.

So:
* `rlwimi r0,r4,4,27,27` → N=27, 31-N=4: writes word bit 27 of the **second** bool byte, the field
  the header already names `x1a9_27_` (the header's own name records the offset, which is what made
  this a two-minute job). Setter, takes the `bool` in `r4`.
* `rlwinm r3,r0,27,31,31` → N=26: reads word bit 26 of the **first** bool byte = `mNonEmitter`.
  Returns the `bool` in `r3` (plain `rlwinm`, no `.`, because it is returned not branched).
* `lwz r0,364(r4)` / `stw r0,0(r3)` → copies the one word at `+0x16C` = `mSfxHandle` from the
  second argument into the first. A four-byte `CSfxHandle` assignment, out of line because retail
  defines it here (`nm` calls it `T`, a strong global, not the weak `__a` the header would emit).

**A same-layout view struct (`SCscriptSoundTail`) rather than `friend` declarations**, following
`src/MetroidPrime/main.cpp:1761` (`SPairRcPtr`): the members are `private` and the header is shared
with `TypesMatch.cpp`. No layout changes; `CHECK_SIZEOF(CScriptSound, 0x1b0)` still holds.

### `fn_8009E0EC` — 132 bytes, the out-of-line `CQuad` copy constructor

Called twice from `fn_8009DD94` as `fn_8009E0EC(&scratch, &box.GetQuad(face))`. The body is 16
`lfs`/`stfs` pairs and nothing else — `CHECK_SIZEOF(CQuad, 0x40)` read as 64 bytes = 16 floats.

**Worth recording: `*dest = *src` on the class is *not* enough — 56.36%, and it is worse in a way
that is easy to misread.** The class copy gets the first sixteen bytes right and then falls back to
`lwz`/`stw` word moves from `+0x10` on, because MWCC's member-wise copy of the four `CVector3f`
corners goes through the copy it emits elsewhere. Retail's is a flat float copy of all 64 bytes.
Spelling the sixteen floats through a `struct SQuadFloats { float f[16]; }` view gets 100%. So a
sub-100% score here was a *codegen-shape* difference, not a wrong field list.

### `SetMaxVolume` — 3.78% → 100%, four measured steps

Prime 1 has no `SetMaxVolume` at all (`prime-ref`'s `CScriptSound` is Matching and stops at
`StopSound`), so this is Echoes-only and was read straight off the instruction. Retail's 0xC8 bytes:

```
sth  r4,406(r3)          mVolume = volume
lwz  r0,364(r3)          mSfxHandle, early-out if zero
  bit 26 (mNonEmitter) -> SfxVolume(handle, volume)          [non-emitter: volume only]
  bit 29 (mScaleByMusicVolume) -> fn_8009DCE8(mVolume) first
  else (emitter): mMaxVolume = mCurrentMaxVolume = mVolume, then
                  UpdateEmitter(handle, position, CVector3f::Zero(), volume)
```

The measured ladder, all four of which are real and all of which the previous notes did not have:

| spelling | score |
|---|---|
| `if (mSfxHandle) { if (mNonEmitter) { SfxVolume(..., mScaleByMusicVolume ? (uchar)fn_8009DCE8(mCurrentMaxVolume) : (uchar)mCurrentMaxVolume); } else { mCurrentMaxVolume = mVolume; mMaxVolume = mCurrentMaxVolume; UpdateEmitter(..., GetTranslation(), ...); } }` — i.e. passing `mCurrentMaxVolume` | 77.36% |
| …but the *argument* is `mVolume` (`+0x196`), not `mCurrentMaxVolume` (`+0x172`), and the ternary assigned to a `const short` | 86.52% |
| `const CVector3f pos = GetTranslation();` **hoisted above** the two stores, and `UpdateEmitter(..., pos, ...)` | 90.00% |
| …with the stores **un-chained** into `mCurrentMaxVolume = mVolume; mMaxVolume = mCurrentMaxVolume;` | 91.80% |
| …and the volume argument hoisted into `const uchar emitterVolume = (uchar)mVolume;` | **100%** |

Three things cost real time and are worth keeping:

1. **The argument is `mVolume`, not `mCurrentMaxVolume`.** Both are `short`s 4 bytes apart
   (`+0x196` and `+0x172`) and the function has just written `mVolume`, so the two read alike; retail
   reloads `+0x196` three separate times (`lha r6,406(r31)` for the argument). Taking the argument
   from the local parameter the function was handed scores 77.36% and looks like a register
   allocation problem.
2. **A chained assignment `mMaxVolume = mCurrentMaxVolume = mVolume;` is a real 90% → 91.80%
   difference.** The chain compiles to one load and two stores; retail reloads `+0x172` between the
   stores. Two statements, not one.
3. **The 91.80% → 100% step is pure register selection.** The only remaining diff was `lha r0,…` /
   `clrlwi r6,r0,24` where retail has `lha r6,…` / `clrlwi r6,r6,24` — the same two instructions,
   one register apart, because the value was materialised into `r0` instead of landing directly in
   the argument register. Hoisting it into a named `const uchar` fixed it. **A 8-point gap that is
   one register is worth chasing; do not file it as a wall without diffing.**

## Decl order: the trap this item walks into, and the fix

`tools/check_decl_order.py` is the only thing that catches this, and it fires as soon as a function
starts *pairing*: a `static` definition that pairs with nothing is invisible to the tool, so a unit
can sit permuted for months and then report "would break on a flip" the moment someone fixes a
name. Measured on this tree, three separate times:

* With `fn_8009DCE8` still `static` near the top of the file: `ok`. With it re-spelled `extern "C"`
  and left there: `would break on a flip`, and the *whole* list shifted by one.
* Moving it to the end fixed `fn_8009DCE8` but left `fn_8009E0EC`/`fn_8009DD94` misplaced — they
  were still near the top, so they emitted last.
* The final source order of the three lowest offsets is **`fn_8009E0EC`, `fn_8009DD94`,
  `fn_8009DCE8`**, written in that order at the foot of the file, which reverse-emits as retail's
  `fn_8009DCE8`, `fn_8009DD94`, `fn_8009E0EC`. Note this is *not* "write them in retail order" —
  each swap of a neighbouring pair is silent.

There is a forward declaration of `fn_8009DCE8` near the top of the file (line 16) because
`SetMaxVolume` calls it and the definition is now last; without it the build fails with
`undefined identifier 'fn_8009DCE8'`.

## `fn_8009DCE8` and `fn_8009DD94`: names settled, bodies still TODO — and that is the point

Both are now `extern "C"` under retail's names, so they **pair** (0.00% → 2.33% and → 3.56%) and
their bodies are honest stubs with a `TODO`, not fake matches. Their signatures are pinned exactly
by their call sites, which is real information this unit did not have before:

* `fn_8009DCE8` — `short` in, `short` out (`Think`/`PlaySound` pass `mCurrentMaxVolume` at `+0x172`
  and feed the result straight to `SfxVolume(handle, (uchar)…)`; the callee's own epilogue is
  `lha r3,8(r1)`). Retail's body reads the game-state music-volume float through a `double`
  round-trip, hands it to `fn_80216CC4` (0x80216CC4, 0xC bytes) then `CMayaSpline::EvaluateAt`, and
  divides the product by the float **127.0** at `.sdata2:0x8041AFF4` — measured, not recalled:
  `python3 tools/dol_read.py 0x8041AFF4 0x10` prints `f32: 127 176 -0 3.402823e+38`. That needs the
  Tweaks block layout and `fn_80216CC4`, which this unit does not own;
  `src/MetroidPrime/Tweaks/CTweakGameHardModeDamageMultiplier.cpp` is the precedent for not
  guessing it here.
* `fn_8009DD94` — walks the sound sources, `CAABox::GetQuad`s each connected volume, `CQuad::GetTri`s
  it, takes the point closest to the listener. Needs the `CScriptTriggerOrientated` cast path and
  the tweak helpers `fn_801B81C0` / `lbl_8041B000`, none of which this unit owns.

Both are honest remaining work **inside this item's own target unit**, so I file no `NEW:` line —
same reasoning lane 3 gave, and the brief requires a `NEW:` target to be a real unit/module/symbol
whose success raises a count.

## Not attempted, and why (so a requeued run does not re-derive it)

* **`GetOccludedVolumeAmount`, 99.70%.** Lane 3 measured twelve spellings, all 99.70% except the
  two early ones (95.31%, 99.52%), and characterised the remainder as five instructions of register
  allocation: both allocate `{f5,f6,f7,f8}` for `(inv, up.x, up.y, up.z)` but retail gives the
  *lowest* to `inv` and we give it to the highest. I did **not** re-attempt it — the notes list the
  spellings and the exact diff, and re-trying them is the most expensive way to spend a lane. The
  one thing not in lane 3's list that I noticed and did not have budget to test: retail's
  `fdivs f5,f4,f27` vs ours `fdivs f8,f4,f27` is a *division* register, and the surrounding code
  computes `1.f / mag` as a named `invMag`; a spelling that makes the reciprocal a **separate
  out-of-line helper** (or forces it through `CVector3f::operator/` on the scalar, which
  `include/Kyoto/Math/CVector3f.hpp:93` defines as `return *this *= (1.f / v)` — a *different*
  expression tree from `* (1.f / v)`) is untried. Prime 1's `#if VERSION >= VERSION_GM8E_02` arm
  uses `soundToCam / soundToCamMag`; our `operator/` is the multiply form, so that is not it.
* **`Think`, 0.28%, 1428 bytes.** Lane 3's analysis stands and is not improved on here: Echoes added
  `IsQueued`, the moving emitter, the visor blend and the music-volume tail. It needs
  `CSfxManager` entry points and a `CPlayer` visor field. One item's worth of work at least. Note
  that `fn_8009DCE8`'s signature is now settled, which removes one of lane 3's stated blockers.
* **`PlaySound`, 0.45%, 892 bytes.** Needs the same `CSfxManager` surface.
* **`AcceptScriptMsg`, 61.64%.** Lane 3's `kSM_XALD` blocker stands: retail walks a 12-byte-element
  container in `CEntity` at `+0x14`/`+0x1C` and the header's `// x10` comment plus the stride do not
  line up with `rstl::vector<SConnection> m_conns`.

## Codegen facts worth keeping (measured, not recalled)

- **MWCC 2.7 1-bit bitfields, both directions** (the convention above). Derive it from a unit's own
  100%-matched functions before guessing; it is not documented anywhere in the repo.
- **objdiff pairs by name, and `config/G2ME01/symbols.txt` has dtk `fn_` placeholders wherever retail's
  own map had no mangled name.** A definition of such a function that is not `extern "C"` under
  that exact name scores 0.00% *forever*, with perfect bytes. `src/MetroidPrime/main.cpp` has five
  worked examples. This is the highest-leverage thing in this note: it converts "unreachable
  function" into "one-line rename".
- **`*dest = *src` on a 0x40-byte class of `CVector3f` members is not a flat float copy.** MWCC falls
  back to word moves partway through. A `float[16]` view forces the shape retail has.
- **`CQuad` is `CPlane` (0x10) + four `CVector3f` (0xC each) = 0x40**, and `CPlane` is
  `CUnitVector3f` + `float` — so all sixteen words really are floats and retail's `lfs`/`stfs` chain
  is a plain block copy, not a per-member loop.
