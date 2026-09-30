# progress-prime1-cdspstreammanager-2 — `Kyoto/Audio/CDSPStreamManager`

`kind: progress`, target `Kyoto/Audio/CDSPStreamManager` (= `main/Kyoto/Audio/CDSPStreamManager`
in `build/report.json`). The unit stays `NonMatching`; `configure.py`, `config/`, `files.cmake`
and `tools/` were **not** touched. The entire change is in `src/Kyoto/Audio/CDSPStreamManager.cpp`
— one file, no header edits, so no other unit's `.text` can move.

This item is the follow-up to `progress-prime1-cdspstreammanager` (see
`docs/goal-notes/progress-prime1-cdspstreammanager.md`), which took four functions to 100 % and
left five sub-100 % with the note that they were all "address-materialisation register choice".
That diagnosis turned out to be only half right: **two of the five were a single instruction
placed in the wrong order**, and both are now at 100 %.

## Result

`tools/goal_check.sh build/goal/item.json` → **PASS**:

```
ok    no judge-owned path touched
ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
ok    counts: matched 10040 -> 10042   linked 4908 -> 4908
ok    check_symbol_names.py
ok    All:  30.96% fuzzy, 23.23% matched, 11.76% linked (10042 / 28465 functions)
ok    target rose: main/Kyoto/Audio/CDSPStreamManager: 23 -> 25 / 29 functions
ok    no asm added
```

Per-function diff between `build/goal/judge/report.base.json` and the tree now, over **all** 2041
units: **0 functions worse, 2 better**, and the two better ones are exactly the item's two:

| function | bytes | before | after |
|---|---|---|---|
| `UpdateStream__17CDSPStreamManagerFPvUlPvUlUl` | 116 | 91.03 % | **100.0 %** |
| `AllocateVoice__17CDSPStreamManagerFiUcP20SND_ADPCMSTREAM_INFOb` | 512 | 98.44 % | **100.0 %** |
| unit `matched_functions` | | 23 / 29 | **25 / 29** |
| unit `fuzzy_match_percent` | | 98.77 % | **99.05 %** |
| unit `matched_code_percent` | | 69.93 % | 77.72 % |

Independently confirmed: `sha1sum build/G2ME01/main.dol` =
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; `python3 tools/check_decl_order.py --unit
main/Kyoto/Audio/CDSPStreamManager` = ok; `tools/unit_fit.sh` prints `.text` 512 bytes over with
the same 7 COMDAT weak destructors and the same `.rodata`/`.bss`/`.sbss` shortfalls as the
unmodified source — byte-for-byte the pre-existing blocker recorded in the parent item, so it is
not mine. `tools/flip_test.sh Kyoto/Audio/CDSPStreamManager.cpp` FAILs, as it did before the
change, for that pre-existing reason.

`docs/HANDOFF.md`'s state block was rewritten by `gate.sh` (it derives the counts from the tree);
the driver discards edits to that file and I did not hand-edit it.

## The whole diff — four lines

Both fixes are in the *spelling* of an expression, not its meaning. Neither adds, removes or
reorders a statement.

```diff
@@ int CDSPStreamManager::AllocateVoice(...)
     it->mStreamId = sndStreamAllocEx(0xFF, it->mBuffer.get(), it->mNumSamples, mHeader.mSampleRate,
-                                     volume, it->mPan, 0, 0, 0, 0, 0x30001,
+                                     volume, static_cast< u8 >(it->mPan), 0, 0, 0, 0, 0x30001,
@@ u32 CDSPStreamManager::UpdateStream(void*, u32, void*, u32, u32)
   CDSPStreamManager* stream = reinterpret_cast< CDSPStreamManager* >(user);
+  u32 total = len1;
+  total += len2;
   u32 half = sVoices[stream->mVoices[0]].mNumSamples / 2;
-  if (len1 + len2 < half) {
+  if (total < half) {
```

### `UpdateStream` — 91.03 % → 100 %

The parent item's read of this function was that retail "indexes `mNumSamples` directly while ours
keeps the array base in r5", and predicted "likely the same `const T*` / named-reference fix". That
prediction was wrong: ten spellings of the reference/pointer form (measured, in the file above)
all stayed at exactly 91.03 %. The real difference was one instruction, and the diff says which:

```
retail                          ours
lis     r3,0                    lis     r5,0
add     r4,r4,r6                add     r0,r4,r6
addi    r3,r3,0                 addi    r4,r5,0
lwz     r0,724(r7)              lwz     r3,724(r7)
```

Retail computes `len1 + len2` **in place, into r4** (r4 already holds `len1`, so the add is
two-operand), and materialises the `sVoices` base into r3. Ours computes the sum into a fresh r0
and leaves r4 free for the base. The fix is to make the source say the sum is accumulated into
`len1` rather than formed as a fresh expression: `total = len1; total += len2;` gives the
allocator a dying-definition to write into, and it picks r4. `len1 += len2` written in place also
matches exactly (both are in the measured list below) — the two-statement form is used because it
does not mutate a parameter.

`u32 total = len1; total += len2;` and `len1 += len2;` both give `*** MATCH ***`; they are the
same instruction sequence. Everything else in the function was already right and is untouched.

### `AllocateVoice` — 98.44 % → 100 %

One `lbz` in the wrong slot, again a scheduling artefact of the argument list:

```
retail                          ours
stw     r0,28(r1)               stw     r0,28(r1)
lbz     r8,16(r30)      <-      lwz     r4,4(r30)
lwz     r4,4(r30)               lwz     r5,12(r30)
lwz     r5,12(r30)              lwz     r6,12(r25)
lwz     r6,12(r25)              lbz     r8,16(r30)      <-
```

`r8` is the `pan` argument (`sndStreamAllocEx`'s 6th parameter, `u8 pan`). Retail loads it first;
we load it last. Writing the argument as `static_cast< u8 >(it->mPan)` — an explicit widening of
the same `uchar` value to the parameter's declared type — is what moves the load up into the
argument-setup group. The cast is a no-op on the value (`uchar` → `u8` is `unsigned char`), so
this is a spelling, not a semantic change.

Note this is **not** symmetric: `static_cast< u8 >(volume)` on the *volume* argument scores
**worse** (2 differing instrs, no change) — `volume` is already `int` and retail widens it with
`clrlwi r7,r26,24`, which the cast does not disturb. Only the `uchar` argument responds.

## Measured, do not retry these

All via `tools/try_batch.py`, which counts *differing instructions* against dtk's retail object
(`build/G2ME01/obj/Kyoto/Audio/CDSPStreamManager.o`) rather than objdiff's size-dominated
percentage. `0` means byte-identical disassembly.

### `UpdateStream` (now 100 %)

```
0        u32 total = len1; total += len2;        <- the fix
0        len1 += len2   (mutates the parameter; same code, less readable)
10       the original `if (len1 + len2 < half)`, and every spelling below
10       sum named first / last, `const u32 sum`
10       const SDSPStreamVoice& voice = sVoices[stream->mVoices[0]];   (parent item's prediction)
10       const SDSPStreamVoice* voice = &sVoices[stream->mVoices[0]];
10       voice ref + named sum; non-const voice ref; `>> 1` instead of `/ 2`
10       const SDSPStreamVoice* const base = &sVoices[0];  base[idx].mNumSamples
10       `const int idx = stream->mVoices[0];` then sVoices[idx]
10       `const u32 samples = ...mNumSamples;` then `half = samples / 2`
10       `static_cast<u32>(len1 + len2)`
18       early-return form `if (len1+len2 >= half && mReadsPending <= 0) { ... return half; }`
32       condition and return value written out longhand (recomputes the array index)
```

The parent item's ten reference/pointer variants are the `10` rows above; they were the right
question and the wrong answer.

### `AllocateVoice` (now 100 %)

```
0        static_cast< u8 >(it->mPan) as the pan argument     <- the fix
2        the original, and: static_cast<u8>(volume) on vol; 0xFF as static_cast<u8>(0xFF);
         0x30001 as 0x30001u; `0` instead of `nullptr` as the last arg;
         SDSPStreamVoice& voice = *it (whole body through the reference);
         a `const int vol = volume` local; `(void)voice` unused marker
4        it->mUpperHalf = false and it->mFree = false swapped
15       `it->mPan = pan;` moved to just before the call
16       `const uchar p = it->mPan;` (or `const u8 p = ...`) as a named local before the call
14       `const uint rate = mHeader.mSampleRate;` as a named local
```

### `IssueRead` — 93.94 %, **2 instructions, a wall this run**

The entire remaining diff is the position of one load, in the function's prologue:

```
retail                          ours
mr      r7,r5                   mr      r7,r5
stw     r0,20(r1)               stw     r0,20(r1)
lbz     r0,736(r8)      <-      stw     r31,12(r1)
stw     r31,12(r1)              mr      r31,r3
mr      r31,r3                  lbz     r0,736(r8)      <-
cmplwi  r0,0                    cmplwi  r0,0
```

Everything after that is instruction-for-instruction identical. Retail evaluates
`stream.mPreload`'s valid flag (offset 736) *before* it spills `r31` and copies `file` into it;
ours spills first. The `736(r8)` load is `rstl::optional_object<CFilePreload>::m_valid` — the
`operator bool` test — and it is the same load either way, only the scheduler's placement differs.

**25 spellings measured, all exactly 2 differing instructions** (i.e. the diff never moved):
`operator bool()` written out; `.valid()`; `const bool hasPreload = stream.mPreload;` hoisted as
a local; the same test written twice (nested `if (stream.mPreload)`) — 12, worse; `stream` bound to
a `const CDSPStreamManager&` or a `CDSPStreamManager* const` first; the body wrapped in a bare
`{}` block; `(*stream.mPreload).Read(...)` and `stream.mPreload.data().Read(...)`;
`sDeferredCallbacks++` vs `= sDeferredCallbacks + 1`; `sDeferCallbacks` hoisted into a named local
(9, worse); the `if`/`else` arms transposed inside the inner `if` (11, worse); the outer `else`
turned into an early `return` (13, worse); `DVDReadAsyncPrio`'s priority bound to a named `int`
before the `if`; `DVDFileInfo* const f = file;`; `(void)voice;`; the `voice` parameter
explicitly unused; and a `get_ptr() != nullptr` test in place of the flag — that last one is 4,
because it loads the *data* pointer (732) rather than the *valid* flag (736) and is semantically a
different test, so it is a wrong spelling, not a near miss.

This is a CodeWarrior prologue-scheduling preference that no source form I tried reaches. Note
that `get_ptr()` is **not** the right test: retail loads 736, the flag, so `operator bool` is
right and must stay.

WALL: IssueRead 93.94% - one prologue load of mPreload.m_valid (736(r8)) is scheduled before vs after the r31 spill; 25 spellings measured, none moves it

### `ReadData` — 97.28 %, allocation *order*, not a missing instruction

`tools/try_batch.py` counts **37 differing instructions**, but they are all the same 12 values
numbered one slot apart, and the shapes match. Per-register use counts:

```
retail  r15=7  r16=8  r17=6  r18=2  r19=2  r20=3  r21=6  r22=2  r23=4  r24=4  r25=2  r26=3
ours    r15=5  r16=5  r17=8  r18=6  r19=2  r20=2  r21=3  r22=6  r23=2  r24=4  r25=4  r26=2
```

Retail's first loop temporary is **r15**, ours starts one register later and uses the freed r15
for a different value; from r16 on, every register in retail is our r(n+1). The two differences
in the middle of the loop are the same shift seen as a reassociation:

```
retail  add  r15,r3,r6 ; add  r15,r30,r15      ours  add  r16,r3,r30 ; add  r16,r6,r16
```

Both compute `left + pos - fileCur`; retail associates `((block*0x11E00 + blockOffset) + dataOffset)`
and ours `((block*0x11E00 + dataOffset) + blockOffset)`. Rewriting the source to force the other
association did **not** move it (37 either way):

```
37  the original
37  uint offset = block * 0x11E00 + blockOffset + dataOffset;   (retail's association)
37  if (right)  instead of if (right != nullptr)
42  left + (pos - fileCur) parenthesised
42  both of the above together
41  the data-size test rewritten as an integer-division ceiling
```

So the offset expression is *not* the lever; the lever is which of the 12 simultaneously-live
values the allocator picks first, and that is decided before the source-level arithmetic is even
considered. This is a bigger job than one item.

### `BufferStream` — 94.21 %, same family, also an ordering problem

Also purely register numbering: retail puts `stereo` in **r31** and the `left`/`right` pointers
in r28/r27; ours puts `stereo` in r29 and swaps which of r28/r29 holds the `loopLen` temp. The
instruction shapes are otherwise the same, including the three `subf`/`add` pairs for
`left + readLen` and `right + readLen`. **10 declaration-order and spelling variants measured**;
the best moved it by 2 instructions and nothing reached 0:

```
27  the original
25  `uchar* right = nullptr;` declared before `uchar* left = ...`   <- best, 2 better, still far
36  `int loopLen = 0;` declared first, before readLen
67  `bool stereo` computed after both buffer pointers
114 all four locals declared before `int end`
43  stereo from `int stereo = mVoices.size() - 1;` (as a count, not a bool)
39  `bool stereo = 2 <= mVoices.size();`  (rewriting the > 1 test)
27  `bool stereo = 1 < mVoices.size();`  (no change from the original)
```

`size > 1` already compiles to the same `xor`/`srawi`/`and`/`subf`/`srwi` sequence retail uses
(verified in the disassembly), so the *test* is right and only the register it lands in is wrong.
Like `ReadData`, this needs the allocator's choice of live-range order to be steered, not a
different expression.

## What still stops the flip (pre-existing, unchanged)

Identical to the parent item and reproduced from the current tree: `unit_fit.sh` reports `.text`
512 bytes over the claimed range with 7 COMDAT weak destructors retail's unit object does not
define (572 bytes total), `.rodata` 4 short, and the two `.bss` claims in `splits.txt` disagreeing
with each other (1612 short). `flip_test.sh` FAILs before and after this change for that reason.
The `progress` verdict is the 23 → 25 rise in `matched_functions`, which is a real measurement.

## Notes / caveats

* No `asm`, no `.s`, no new files, no changes to `configure.py`, `config/`, `files.cmake`,
  `tools/`, or any judge-owned path. Only `src/Kyoto/Audio/CDSPStreamManager.cpp` is modified.
* No class layout, header or Prime 1 source was touched. As the parent item established, Echoes'
  `CDSPStreamManager` shares no implementation with Prime 1's; the two fixes here are ordinary
  CodeWarrior register-allocation spellings and would read the same in either fork.
* `New:` lines: none from this run. The two remaining sub-100 % functions (`ReadData`,
  `BufferStream`) are characterised above with their measured spellings — enough for the next run
  to skip the re-measurement — but neither is a bounded one-item job I can promise, so I am not
  filing them as `NEW:` items on the strength of a guess.
* **The generalisable lesson, for whoever reads this next**: a large per-function diff in this
  codebase is very often *not* a wrong expression but a right expression whose temporaries the
  allocator numbered differently. `tools/try_batch.py`'s differing-instruction count separates
  those two cases immediately — a shift of every register by one is an ordering problem, and no
  amount of rewriting the arithmetic will touch it. Conversely, a diff of exactly 2 instructions
  means one load or one add is in the wrong slot, and that *is* reachable from the source: the two
  wins this run were both 2-instruction diffs. Rank candidates by instruction count, not by
  objdiff percentage, and try the cheapest first.

## Follow-up run on lane L9 (2026-09-30)

Re-measured the actual L9 tree before editing: `build/report.json` had `main/Kyoto/Audio/CDSPStreamManager` at **23/29**, with `UpdateStream` 91.03%, `AllocateVoice` 98.44%, `IssueRead` 93.94%, `ReadData` 97.28%, `BufferStream` 94.21%, and `UpdateVolume` 99.59%. The previous run's positive spellings were not present in this source. Applied only those two byte-neutral spellings: accumulate `len1 + len2` into a `u32 total` in `UpdateStream`, and explicitly widen `it->mPan` to `u8` in the `sndStreamAllocEx` call. `./tools/decomp_build.sh Kyoto/Audio/CDSPStreamManager` measured both as exact matches; unit became **25/29**, 99.05% fuzzy / 77.72% matched code.

Tried new `UpdateVolume` forms beyond the prior lists, using `python3 tools/try_batch.py src/Kyoto/Audio/CDSPStreamManager.cpp Kyoto/Audio/CDSPStreamManager UpdateVolume__17CDSPStreamManagerFii .tmp/opencode/cdsp-updatevolume-variants.py`: `register int index` (4 differing instructions), `volatile int index` (10), `uint index` (4), direct `OSDisableInterrupts`/`OSRestoreInterrupts` with `bool` (25) and `BOOL` (22). `try_batch.py` restored the source; none beat the existing 99.59%. Current disassembly still differs only in the r5/r6 allocation of the index multiply and interrupt-state temporary. The manual-restore variants were diagnostic only and were not retained.

`./tools/goal_check.sh build/goal/item.json` → **PASS**: matched 10303 → 10305, linked 5048 unchanged, target 23 → 25 / 29, gate / symbol check / no-asm checks all clean. No function regressed per the judge's report-diff gate.

WALL: UpdateVolume__17CDSPStreamManagerFii 99.59% - five new spellings in this run did not move the four-instruction r5/r6 allocation difference; best remained 4 differing instructions.
