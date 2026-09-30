# progress-prime1-cdspstreammanager — `Kyoto/Audio/CDSPStreamManager`

`kind: progress`, target `Kyoto/Audio/CDSPStreamManager` (= `main/Kyoto/Audio/CDSPStreamManager` in
`build/report.json`). The unit stays `NonMatching`; `configure.py` was **not** touched and
`flip_test.sh` was run only to confirm the flip is out of reach (it FAILs, as it did before the change,
for the pre-existing reason recorded below). The whole change is in
`src/Kyoto/Audio/CDSPStreamManager.cpp` — one file, no header edits, so no other unit's `.text` can
move.

## Result

`tools/goal_check.sh build/goal/item.json` → **PASS**:

```
ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok    counts: matched 9993 -> 9996   linked 4896 -> 4896
ok    All:  30.77% fuzzy, 23.03% matched, 11.74% linked (9996 / 28465 functions)
ok    target rose: main/Kyoto/Audio/CDSPStreamManager: 20 -> 23 / 29 functions
ok    no asm added
```

Per-function diff between `build/goal/judge/report.base.json` and the tree now, over **all** 2042
units: **0 functions worse, 4 better**, and the four better ones are exactly the item's four:

| function | bytes | before | after |
|---|---|---|---|
| `UpdateVolume__17CDSPStreamManagerFii` | 196 | 95.31 % | **99.59 %** |
| `IsStreamAvailable__17CDSPStreamManagerFi` | 136 | 99.12 % | **100.0 %** |
| `CanStop__17CDSPStreamManagerFi` | 116 | 98.97 % | **100.0 %** |
| `GetStreamState__17CDSPStreamManagerFi` | 128 | 99.06 % | **100.0 %** |
| unit `matched_functions` | | 20 / 29 | **23 / 29** |
| unit `fuzzy_match_percent` | | 98.59 % | 98.77 % |

Independently confirmed: `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`tools/probe_sources.sh` = 749 files, 0 failed, 0 errors, LINKED (250 undefined, 0 duplicates);
`python3 tools/check_symbol_names.py` = 503 units, 0 missing names;
`python3 tools/check_decl_order.py --unit main/Kyoto/Audio/CDSPStreamManager` = ok;
`tools/unit_fit.sh` shows `.text` over by 512 bytes / 7 extra destructors, which is the **pre-existing**
blocker (see "What still stops the flip"), unchanged by this item.

## Metroid Prime 1's source was **not** usable for any of the four

The item's hint ("Echoes' engine is a fork of it") is right about the *file*, but the four functions
share a name with Prime 1 and nothing else. Measured, by reading Prime 1
(`/run/.../prime-ref/src/Kyoto/Audio/CDSPStreamManager.cpp`, 381 lines, commit `62ef50b`):

* Prime 1 has `g_Streams[4]` indexed by a *handle* returned from `FindClaimedStreamIdx`, with
  `mUnclaimed`, `mHeaderReadState`, `mCompanionLeft/Right`, `mOneshot`, `mHandleId`, `mStreamId`, and
  an eight-argument `CDSPStreamManager(rstl::string, int, int, bool)` ctor that probes
  `CDvdFile::FileExists`. Echoes has `sStreams[2]` indexed by a caller-supplied slot, no handle table,
  no companions, and a five-argument default ctor.
* Prime 1's `UpdateVolume`, `IsStreamAvailable`, `CanStop` and `GetStreamState` are all built on
  `FindClaimedStreamIdx` plus `CDSPStream::UpdateVolume/IsStreamAvailable/IsStreamActive` — helpers
  Echoes does not have at all (Echoes drives `sndStream*` directly). Prime 1's `GetStreamState` even
  switches on `mHeaderReadState` and returns `kCDSPSM_Preparing`, an enum value Echoes' `EState`
  (`kS_Looping`, `kS_Oneshot`, 2 values) cannot express.
* Prime 1's `CInterruptGuard` has `bool mEnabled` declared **before** the `public:` label; Echoes'
  (in this file) has it after. Cosmetic, but it is a different class, not a copy to bring over.

So for all four functions: **Prime 1's source did not match, and copying it would not have helped.**
No Prime 1 header was copied and no class layout was changed. What the item's hint was right about is
the *register-allocation habit* the fork kept: in these four functions retail materialises the
`&sStreams[handle]` address into a callee-saved register instead of re-deriving it, and a C++ source
that says so reproduces it.

## Per function

All four were at 95–99 % before, and in every one the entire remaining diff was register allocation,
not semantics.

### `CanStop__17CDSPStreamManagerFi` — 98.97 % → **100 %**

Retail's whole body:

```
mr. r31,r3 ; bge/blt on handle ; li r3,1 ; b out        <- handle < 0 || handle >= 2 -> true
bl OSDisableInterrupts ; neg r5,r3 ; or r3,r5,r3 ; srwi r3,r3,31 ; stb r3,8(r1)
mulli r5,r31,768 ; lis r4,0 ; addi r0,r4,0 ; add r4,r0,r5
lwz r0,716(r4) ; cntlzw r0,r0 ; srwi r31,r0,5
bl OSRestoreInterrupts ; mr r3,r31 ; blr
```

Ours computed the same thing but allocated the address into **r0** and the `mulli` into r5, so four
bytes differed. One edit — bind the element to a reference — fixes it:

```cpp
CInterruptGuard guard;
CDSPStreamManager& stream = sStreams[handle];
return stream.mState == kSS_Idle;
```

### `IsStreamAvailable__17CDSPStreamManagerFi` — 99.12 % → **100 %**

Same cause, same fix, same shape. The `mState` load was already hoisted into a local by the previous
author; adding the reference for the *address* is what was missing.

### `GetStreamState__17CDSPStreamManagerFi` — 99.06 % → **100 %**

Same, on `stream.mHeader.mLoopFlag` (offset 20, a `ushort`, hence retail's `lhz`).

### `UpdateVolume__17CDSPStreamManagerFii` — 95.31 % → **99.59 %** (wall)

Two diffs, both register allocation:

1. `mulli r6` vs our `mulli r5` (and the matching `neg`/`or` swap between r5 and r6) — the
   interrupt-guard's boolean temporary and the index temporary are allocated in the opposite order.
2. In the loop, retail loads `mPan` (`lbz r5,16(r3)`) *before* `mStreamId` (`lwz r3,24(r3)`) and
   addresses the voice through **r3**; ours went through **r5** and loaded `mStreamId` first. Changing
   `SDSPStreamVoice& voice = sVoices[*it]` to `const SDSPStreamVoice* voice = &sVoices[*it]` fixed
   diff 2 exactly (retail's argument order is right-to-left here, which is what a pointer forces).

Diff 1 survives every spelling I tried. **Measured, do not retry these** (all via
`ninja build/G2ME01/src/Kyoto/Audio/CDSPStreamManager.o` + `objdiff-cli report generate`, i.e. the
same measurement the report uses):

```
99.592  guard first, CDSPStreamManager& stream; loop uses const SDSPStreamVoice*     <- current
99.592  same, but CDSPStreamManager* stream = &sStreams[handle]
99.592  same, but CDSPStreamManager* stream = sStreams + handle
99.592  same, but const CDSPStreamManager& / CDSPStreamManager* const
99.592  same, but a separate `const int idx = handle` before the element expression
99.592  same, but `static_cast<int>(static_cast<long>(handle))` as the index
99.592  same, but the state compared as `kSS_Playing == stream.mState`
99.592  same, but `static_cast<int>(stream.mState) == kSS_Playing`
99.592  same, but the state read into a named `const int state` first
99.592  same, but the stream and its loop in a nested `{}` scope
99.592  same, but the loop written `while (it != end) { ... it = it + 1; }`
99.592  same, but the range check written `0 <= handle && 2 > handle` / `handle > -1 && handle < 2`
99.592  same, but the voice bound as `SDSPStreamVoice* const` / `const SDSPStreamVoice&`
99.082  state checked on `sStreams[handle]` directly, pointer bound inside the `if`
99.082  `voice.mStreamId` and `voice.mPan` hoisted into named locals (any order)
97.347  range check `handle > -1 && handle < 2` with a pointer
97.327  range check `handle >= -1 && handle <= 1`
97.224  range check `handle < 2 && handle >= 0`, or `handle <= 1 && handle >= 0`
97.224  the array indexed directly, no named stream at all
97.143  early `return` form, with reference or with pointer
95.306  non-const `SDSPStreamVoice&` in the loop (the pre-change state, plus pointer variants)
94.208  hoisting `mVoices.end()` into a named `int*` / `int* const`
93.939  position loop `for (uint i = 0; i < mVoices.size(); ++i)`
93.367  range check `static_cast<uint>(handle) < 2` or `handle >= 0u && handle < 2u`
92.939  direct indexing with a non-const voice reference
92.857  early `return` form with the nested `if (handle >= 0 && handle < 2)`
91.224  range check hoisted into a `const bool inRange` before the `if`
86.327  position loop over `mVoices[i]`
85.306  index multiplied by an explicit `sizeof(CDSPStreamManager)`
83.408  position loop with `stream.mVoices[i]`
76.327  early `return` with the pointer bound before the guard
75.184  `CInterruptGuard guard = CInterruptGuard();`
74.694  the pointer bound **before** the guard (reorders the whole prologue)
73.755  `rstl::reserved_vector<int,2>& voices = stream.mVoices` before the state check
73.653  the vector taken through a `reserved_vector* const`
67.776  `begin`/`end` hoisted into `int* const` before the loop
67.449  guard declared at function scope
67.224  `switch (stream->mState)` instead of `if`
```

The split is clean: **anything that changes the shape of the guard or the range check scores
67–97 %, everything that leaves them alone sits at exactly 99.592 %.** The last 0.41 % is two
instructions whose only difference is which of r5/r6 holds the `mulli` result versus the guard's
boolean — an allocation-order preference I could not move from the source without disturbing the
prologue. This is the wall for this function.

## What still stops the flip (pre-existing, not this item)

`tools/unit_fit.sh Kyoto/Audio/CDSPStreamManager.cpp` reports `.text` 512 bytes over the claimed
range with 7 extra functions retail's unit object does not define — all COMDAT weak destructors
(`~SDSPStreamVoice`, `~auto_ptr<uchar>`, `~optional_object<CFilePreload>`, `~CInterruptGuard`,
`~basic_string<char>`, `~reserved_vector<int,2>`, `__arraydtor$306`), 572 bytes total, and
`.rodata` 4 bytes short plus `.bss` 1612 bytes short (two `.bss` claims in `splits.txt` disagree with
each other). None of that is mine: the same numbers come out of `unit_fit.sh` on the unmodified
source, and the judge counts 23/29 functions rather than 29/29. **Only `flip_test.sh` decides**, and it
FAILs both before and after this change.

Six functions remain below 100 % in the unit and are *not* covered by this item (it named only the
four that share a Prime 1 name; the other two of the six are listed for the next run):

| function | bytes | now |
|---|---|---|
| `AllocateVoice__17CDSPStreamManagerFiUcP20SND_ADPCMSTREAM_INFOb` | 512 | 98.44 % |
| `ReadData__17CDSPStreamManagerFPUcPUcUiiUi` | 404 | 97.28 % |
| `BufferStream__17CDSPStreamManagerFv` | 732 | 94.21 % |
| `IssueRead__17CDSPStreamManagerFP11DVDFileInfoPviiRC15SDSPStreamVoiceR17CDSPStreamManager` | 132 | 93.94 % |
| `UpdateStream__17CDSPStreamManagerFPvUlPvUlUl` | 116 | 91.03 % |

Two of them I read closely and can characterise, for whoever takes them next:

* **`UpdateStream` (91.03 %)** — retail loads `sVoices` into r3/r4 and then walks
  `724(r7)` → `slwi 5` → `add` → `lwz 16(r3)` → `srwi r31,r0,1`, i.e. it indexes
  `mNumSamples` (offset 16 of `SDSPStreamVoice`) directly, while ours keeps the array base in r5 and
  loads the same field. Same family as the diffs above: the address is materialised in a different
  register. Likely the same `const T*` / named-reference fix.
* **`IssueRead` (93.94 %)** — the *only* difference is that retail tests `stream.mPreload` (offset
  736, `lbz`) **before** spilling `file` to `12(r1)`, and ours spills first. Instruction-for-
  instruction identical otherwise. This is one statement's evaluation order.

## Notes / caveats

* `docs/HANDOFF.md`'s state block was refreshed by `tools/gate.sh` (it rewrites the derived counts);
  the driver discards edits to that file, and I did not hand-edit it.
* `New:` lines: none from this run. The one thing worth a follow-up is filed below.
* No `asm`, no `.s`, no new files, no changes to `configure.py`, `config/`, `files.cmake` or
  `tools/`.

NEW: progress-prime1-cdspstreammanager-2 | progress | Kyoto/Audio/CDSPStreamManager | UpdateVolume is 0.41% short (r5/r6 allocation order only, 45+ spellings measured in the notes) and IssueRead/UpdateStream/ReadData/BufferStream/AllocateVoice are all 91-98% on address-materialisation register choice
