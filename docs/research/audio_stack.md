# The audio stack: how much of it stands between this tree and a first frame

Written 2026-09-26 by lane `h1` from a clean build of `3be457e`. Every address, size
and reference below was read out of `build/G2ME01/main.elf` with
`build/binutils/powerpc-eabi-objdump -d -r`, or out of the objects
`tools/link_gap.py --rebuild` configures. Nothing here is recalled.

**The answer first, because it is the thing a later session should not have to
derive: of the 29 audio symbols the port's link asks for, 11 are reached before the
game's first frame and 18 are not.** The 11 come from exactly two call sites -
`CGameArchitectureSupport`'s constructor (`docs/research/boot_path.md` step 17) and
`CGameOptions::EnsureOptions` (step 19) - and they are reached through only **two of
the six objects** that reference the whole 29. That is a much smaller hole than
`tools/link_reach.py`'s "every one of them is referenced by a reachable object"
suggests, and it is the number that should size the rest of the audio work.

## The 29, and who asks for them

`tools/link_reach.py` reports the whole 342 remaining undefined symbols as
"referenced by a reachable object", which is a **whole-object, branch-blind**
upper bound. The referencing object per symbol is a stronger statement and is
cheap: `nm --undefined-only` over the 251 objects in `build-port`, with both sides
through `c++filt`. Six objects reference the 29:

| referencing object | symbols | on the path to a first frame? |
| --- | --- | --- |
| `src/MetroidPrime/main.cpp` | 7 | **yes** - `CGameArchitectureSupport`'s constructor, boot_path step 17. One of the seven (`~CAudioSys`) is only in `~CGameArchitectureSupport`, i.e. step 22, teardown |
| `src/MetroidPrime/Player/CGameOptions.cpp` | 3 | **yes** - `EnsureOptions`, boot_path step 19 |
| `src/Kyoto/Audio/CStaticAudioPlayer.cpp` | 3 | no. `CStaticAudioPlayer` is streamed audio; nothing constructs one before a frame |
| `src/MetroidPrime/CActor.cpp` | 5 | no. No actor exists before a frame |
| `src/MetroidPrime/Player/CPlayerGun.cpp` | 1 | no |
| `src/MetroidPrime/ScriptObjects/CScriptStreamedMusic.cpp` | 9 | no. Needs a level, a pak and a script object |

So the boot path's audio requirement is `main.cpp` + `CGameOptions.cpp`, and within
those two the *call sites* are:

| boot_path step | function | audio calls |
| --- | --- | --- |
| 17 | `CGameArchitectureSupport::CGameArchitectureSupport` (`main.cpp:329`) | `audioSys(0x30,0x30,0x30,0x30,0x5fc000)` in the member init list, then in the body `CAudioSys::SysSetVolume`, `SetDefaultVolumeScale`, `SetVolumeScale`, `GetDefaultVolumeScale`, `CStreamAudioManager::SetMusicVolume`, `CAudioSys::TrkSetSampleRate` |
| 19 | `CGameOptions::EnsureOptions` -> `SetSfxVolume` / `SetMusicVolume` / `SetSurroundMode` | `CAudioSys::SysSetSfxVolume`, `CStreamAudioManager::SetSfxVolume` (both behind `if (fn_80161C84()) ... else`), `CStreamAudioManager::SetMusicVolume`, `CAudioSys::SetSurroundMode` |
| 22 | `~CGameArchitectureSupport` | `~CAudioSys` - teardown, and the port's `RsMain` returns immediately, so not even that |

**That is the 11: 1 constructor + 6 calls in step 17 + 4 calls in step 19.** None of
the other 18 is reachable before a rendered frame, which means for those the right
first move is a real implementation later, and the right thing to record now is that
the boot does not need them.

### A caveat that is a wall, not a detail

Step 17 is where these 11 are called, and step 17 is the wall
`docs/research/boot_path.md` describes: `CGameArchitectureSupport`'s constructor does
`gpTweakPlayerA->GetLeftAnalogMax()` with no null test, and `gpGameState` is null
until the paks load. So in the tree as it stands **all 11 are one fix away from being
reached and none is reached today** - the member init list runs before the faulting
line, so `CAudioSys::CAudioSys` itself *does* execute, and the six body calls do not.
That is a reason to make them correct now rather than a reason to wait.

## What each subsystem phase would call, for the other 18

Grouped by the phase that first wants them, so a later lane knows which block a
symbol belongs to rather than finding it one undefined-reference at a time.

| phase | symbols | first caller |
| --- | --- | --- |
| streamed audio / music | `CStreamAudioManager::Start`, `Stop`, `FadeBackIn`, `TemporaryFadeOut`, `SetDefaultAudio`, `SetCurrentAudio`, `sub_803653f8`, `sub_80365424`, `sub_8036590c` (all 9) | `CScriptStreamedMusic`, i.e. a level's script objects. **9 of the 29, and all 9 are one object.** Nothing in the boot map reaches a script object |
| 3D sound emitters | `CSfxManager::AddEmitter`, `RemoveEmitter`, `UpdateEmitter`, `SfxStart`, `TranslateSFXID`, `PitchBend` (6) | `CActor`'s constructor and destructor, and `CPlayerGun` for `PitchBend`. First reached when an actor is spawned, which is step 13's world loading |
| the AI DMA callback handshake | `CAudioSys::EnableAICallback`, `IsAICallbackEnabled`, `GetSurroundMode` (3) | `CStaticAudioPlayer::InstallAICallback` / `MixCallback` / `StartMixOut` |

## Where each of the 29 went, and why

Two routes, and the dividing line is not "hard to decompile" - it is **whether the
body calls a symbol that exists outside `main.dol`**.

| route | count | what it means |
| --- | --- | --- |
| `Matching` decompilation unit | **12** | retail's bytes, reproduced exactly, claimed range and all. Not in `files.cmake`, so it cannot affect the port |
| port-side implementation | **14** | `src/MetroidPrime/PortAudio.cpp`, in `files.cmake` and absent from `configure.py`, so mwcceppc never sees it. Includes the 2 that are only reachable through `CStaticAudioPlayer` |
| neither | **3** | see below |

The 12 `Matching` units, all `flip_test.sh` `PASS -> kept as Matching`, all
byte-exact, all `.text`-only:

| unit | retail range | bytes | symbols (of the 29) | on the frame path |
| --- | --- | --- | --- | --- |
| `Kyoto/Audio/CAudioSysVolume.cpp` | `0x80307864`-`0x8030787C` | 24 | `GetDefaultVolumeScale`, `SetDefaultVolumeScale`, `SetVolumeScale` | 3 |
| `Kyoto/Audio/CAudioSysSurround.cpp` | `0x8030787C`-`0x803078FC` | 128 | `SetSurroundMode`, `GetSurroundMode` | 1 |
| `Kyoto/Audio/CAudioSysAICallback.cpp` | `0x8030780C`-`0x80307864` | 88 | `IsAICallbackEnabled`, `EnableAICallback` | 0 |
| `Kyoto/Audio/CAudioSysSysVolume.cpp` | `0x80308840`-`0x8030889C` | 92 | `SysSetSfxVolume`, `SysSetVolume` | 2 |
| `Kyoto/Audio/CAudioSysTrkSampleRate.cpp` | `0x803083A8`-`0x803083C8` | 32 | `TrkSetSampleRate` | 1 |
| `Kyoto/Audio/CStreamAudioManagerSfxVolume.cpp` | `0x80321740`-`0x80321758` | 24 | `SetSfxVolume` | 1 |
| `Kyoto/Audio/CStreamAudioManagerMusicVolume.cpp` | `0x80321758`-`0x80321790` | 56 | `SetMusicVolume` | 1 |

**392 bytes of retail, 12 of the 29, 8 of them on the path to a frame.** The three
that are *not* on the path are the three a `CStaticAudioPlayer` call reaches, and they
came along because they share a contiguous `.text` run with ones that are.

The other 14 are port-side. Two of them (`CAudioSys`'s constructor and destructor,
512 and 148 bytes) are not decompilation candidates at all: retail's bodies are almost
entirely calls into unnamed DSP/AUDIO configuration code at `0x803899xx` and
`0x8039E1xx` that has no host counterpart. The remaining 12 *are* `Matching` in retail
- they are the 12 above - and **that overlap is the point, not a contradiction:** the
decompilation and the port are different builds, exactly as
`src/MetroidPrime/PortBoot.cpp` is for `CMain::OpenWindow` and `CMain::RsMain`.

Seven of the twelve could not be a port source whatever was done: their bodies call
`fn_80389964`, `fn_803899C4`, `fn_803078FC`, `fn_80389A58` and `fn_803212C8`, which
are `main.dol`-only symbols, and `CStreamAudioManager::SetSfxVolume`'s writes the guest
word 0x80418C30. The other five - the three volume-scale accessors,
`TrkSetSampleRate`, and the two AI-callback methods - **could** have gone into
`files.cmake` as their own `Matching` units, and deliberately did not: those units read
and write retail addresses (.sdata `0x80418BEA`/`0x80418BEC`/`0x80418BEE`, .sbss
`0x80419B84`), so host-compiling them trades four real undefined symbols for four guest
ones. One file, one alphabet, no guest addresses in a host binary is the better
arrangement, and it is the one to keep if a later lane adds to this block.

The 3 that got neither route: the 6 `CSfxManager` methods and the 9 remaining
`CStreamAudioManager` methods. Every one of the 6 `CSfxManager` bodies and 5 of the 9
`CStreamAudioManager` bodies are **unnamed in `config/G2ME01/symbols.txt`**, so their
addresses have to be recovered from the disassembly before a range can even be
claimed; and `CSfxManager`'s whole emitter machinery needs a `CAudioSys` with working
emitter tables, which does not exist yet. Writing them as port code is the cheaper
order and the decompilation should follow the port, not lead it.

## The three findings that cost a build each

**1. A `Matching` unit in this block must claim `.text` only.** Not a style
preference - each of the two attempts fails.

* Claiming the seven bytes of `.sdata` `CAudioSys` really owns
  (`[0x80418BE8, 0x80418BEF)`) is **refused**:
  `dtk dol split` fails with `Invalid alignment for split:
  Kyoto/Audio/CAudioSysVolume.cpp .sdata expected 8, but starts at 0x80418BEA`. Only
  the whole eight-byte slot passes. The nine non-aligned claims already in
  `splits.txt` do not contradict this: every one belongs to a `NonMatching` unit, and
  dtk only checks the alignment of an object it can read - for a `NonMatching` unit it
  downgrades the same condition to a `WARN`.
* Owning the slot then needs **all five** bytes defined under retail's names, because
  retail text *outside* these functions reads them: `lbl_80418BE8`, `lbl_80418BEA` and
  `lbl_80418BEF` are read by the base object dtk splits for `0x8030787C` onwards, and
  `lbl_80418BEE` by its base object for `0x80307030`. A class static does not satisfy
  them - the link fails with `undefined: 'lbl_80418BEA'`.

Leaving the slot unclaimed costs nothing: dtk keeps retail's copy, the DOL link
resolves against it, and nothing in the port references those names, so the port's
link gap does not move.

**2. `clrlwi` at a call site comes from a conversion in the source, not from the
callee's prototype.** Retail's `SysSetSfxVolume` masks all four arguments
(`clrlwi r3,r3,24`, `clrlwi r4,r4,16`, `clrlwi r5,r5,24`, `clrlwi r6,r6,24`). With
`void fn_803899C4(uchar, ushort, uchar, uchar);` and no casts, MWCC emits **none** of
them. One `static_cast<ushort>` produced exactly one mask. All four casts, with the
parameters declared `uint`, produce all four masks in retail's order. The masks are
also the only reliable statement of the callee's real prototype: `SysSetVolume`'s
public signature takes an `unsigned int` and retail still masks its second argument to
16 bits, so the narrowing happens at the call.

**3. `cmplwi` vs `cmpwi` is decided by the left operand's type, and it is one
instruction.** `CStreamAudioManager::SetSfxVolume` needs `cmpwi r3,127`; written as
`if (volume > 0x7F)` on a `uint` parameter, MWCC emits `cmplwi r3,127` - same size,
different opcode, and it is *not* a cosmetic difference: `cmplwi` sign-extends the
immediate, so the two disagree for `volume >= 0x8000`. `if (static_cast<int>(volume)
> 0x7F)` gives `cmpwi`. Nine spellings were tried (`>= 0x80u`, the `?:` form both
ways, a named `static const uint`, a local copy, an explicit `uint` cast); only the
`int` cast changes the opcode. Same fix in `SetMusicVolume`.

A fourth, smaller: `IsAICallbackEnabled` returns `lbl_80418BEE` directly, and the
declared type of that extern must be `bool`. Returning a `uchar` - with or without
`!= 0` - makes MWCC normalise the result with `neg`/`or`/`srwi` and the function is
three instructions too long.

## What this does to the port's numbers

`python3 tools/link_check.sh` on the port's real link, before and after:

| | before | after |
| --- | --- | --- |
| unique undefined symbols | **342** | **328** |
| duplicate definitions | 0 | 0 |
| | NOT LINKED | NOT LINKED |

`python3 tools/link_gap.py --rebuild` over its 244 objects: **311 MISSING -> 297
MISSING**, 14 entries gone from `docs/research/port_link_gap_list.md` and none added.

`./tools/gate.sh`: DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, 86/86 RELs
unchanged, per-function diff `matched 3129 -> 3141   linked 1752 -> 1764   (+12
functions at 100%, 7 units newly linked)`.

**The port still does not link and no frame has been rendered.** Fourteen fewer
undefined symbols is not a link, and the wall in `boot_path.md` step 17 is untouched
by any of this.
