# CScriptStreamedMusic (progress, MetroidPrime/ScriptObjects/CScriptStreamedMusic)

## Result

**Partial progress, as the item intends: the unit stays `NonMatching`.** One function was matched
outright; the other six are documented below with the spellings tried.

| | before | after |
|---|---|---|
| unit fuzzy | 96.10442% | 96.37126% |
| **matched functions** | **16 / 23** | **17 / 23** |

`tools/goal_check.sh build/goal/item.json` -> **PASS** (`matched 11800 -> 11801`, `linked` held at
5727, no function anywhere got worse, no asm added, DOL sha1 and all 86 RELs unchanged).
`./tools/decomp_build.sh MetroidPrime/ScriptObjects/CScriptStreamedMusic` after the change:

```
main/MetroidPrime/ScriptObjects/CScriptStreamedMusic: 96.37% fuzzy, 66.79% matched (17 / 23 functions)
   StopNonDsp__20CScriptStreamedMusicFv                  77.78%  36 bytes
   StartStream__20CScriptStreamedMusicFv                 74.29%  112 bytes
   StopStream__20CScriptStreamedMusicFv                  73.33%  60 bytes
   internal_search<...const_linear_iterator..., const char*>  96.69%  260 bytes
   __ct<...const_linear_iterator...>                     99.56%  216 bytes
   LoadStreamedAudio__FR13CStateManagerR12CInputStreamRC11CEntityInfo  87.38%  728 bytes
```

## What I changed

Only `src/MetroidPrime/ScriptObjects/CScriptStreamedMusic.cpp`, and only inside
`SetStereoPair()`. No header, no `configure.py`, no carve, no asm, no deleted initialisation -
every edit computes the same value as before.

**`SetStereoPair` 97.15% -> 100.00% (400 B)**, from two spelling changes, both taken from the
Prime 1 donor `prime-ref/src/MetroidPrime/ScriptObjects/CScriptStreamedMusic.cpp`:

1. `mFileName.size() >= 5` -> `static_cast< int >(mFileName.size()) >= 5`. `size()` returns
   unsigned, so our build emitted `cmplwi r3,5`; retail has `cmpwi r3,5`, i.e. the compare is
   **signed**. Same operands, one opcode byte.
2. `if (CStringExtras::CompareCaseInsensitive(...) == 0)` -> bind it to `const int cmp` first and
   test `if (cmp == 0)`. Our build canonicalised the int result to a bool before the two
   temporary `rstl::string` destructors ran (`cntlzw r0,r3 ; srwi r31,r0,5 ; clrlwi. r0,r31,24`),
   retail keeps the raw int in `r31` across both destructor calls and compares afterwards
   (`mr r31,r3 ... cmpwi r31,0 ; bne`). The Prime 1 donor has exactly the `const int cmp` local,
   so retail's Echoes fork kept it. That local was worth **-2 instructions** and fixed the 4-byte
   size overflow (ours 404 B -> 400 B, exactly retail's).

`tools/bytesdiff.sh src/MetroidPrime/ScriptObjects/CScriptStreamedMusic.cpp SetStereoPair
0x8015D0B8 0x190` now reports 22 differing instructions of 100, **all of them relocation fields**
(the `bl` displacements and the `@stringBase0` `lis`/`addi` pair, which objdiff resolves) - which
is why objdiff scores it 100.00%.

## What is still short, and what I tried (for the next run)

`tools/bytesdiff.sh` counts a `bl` displacement as a difference, so the "real" counts below
discount relocation fields.

### `StopNonDsp` (36 B, 77.78%) - 2 real diffs, a wall

Retail 0x8015D944: `stwu; mflr r0; lfs f1,60(r3); stw r0,20(r1); bl FadeBackIn` - the argument load
sits **between `mflr` and the LR spill**. Ours puts it after. Identical instruction multiset.

| spelling | bytescmp |
|---|---|
| `FadeBackIn(mFadeOut)` (baseline) | 3 of 9 (2 real) |
| `const float fadeOut = mFadeOut;` then the call | 3 of 9 (2 real) |
| `FadeBackIn(static_cast<float>(mFadeOut))` | 3 of 9 (2 real) |
| `const float& fadeOut = mFadeOut;` | 3 of 9 (2 real) |
| `float fadeOut = mFadeOut;` (non-const) | 3 of 9 (2 real) |

Every spelling produced **byte-identical** code. Prime 1's `sub_8020c3f0` is the same one-liner, so
the source is right and the difference is a scheduler decision this TU cannot reach from source.

WALL: CScriptStreamedMusic::StopNonDsp 77.78% - the only diff is `lfs f1,60(r3)` vs
`stw r0,20(r1)` order; five source spellings produce byte-identical code.

### `StopStream` (60 B, 73.33%) - 2 real diffs, a wall

Retail hoists `lbz r0,52(r3)` (the `mLoop` bitfield read) above `stw r31,12(r1); mr r31,r3`; ours
emits the whole prologue first. Same multiset otherwise.

| spelling | bytescmp |
|---|---|
| `StopSoftwareAudio(IsOneShot(mLoop), mFileName)` (baseline) | 5 of 15 (2 real) |
| `const ESoftwareChannel channel = IsOneShot(mLoop);` then the call | 5 of 15 (2 real) |
| `const bool loop = mLoop;` inline at the call | 5 of 15 (2 real) |
| `this->mLoop`, `this->mFileName` | 5 of 15 (2 real) |

WALL: CScriptStreamedMusic::StopStream 73.33% - retail hoists the `mLoop` `lbz` above the r31
prologue slot; four spellings produce byte-identical code.

### `StartStream` (112 B, 74.29%) - 6 real diffs, a wall

Two separate scheduling-only differences: the same prologue hoist as `StopStream`, and the
argument block of `PlaySoftwareAudio` is built in a different order (retail `lbz r0,52(r31)`,
`addi r4`, `lwz r5,64`, `lfs f1`, `rlwinm r6`, `lfs f2`, `clrlwi r5`; ours interleaves `lwz r5`,
`lbz r0`, `addi r4`, `lfs f1`, `clrlwi`, `rlwinm`, `lfs f2`). **The logic already matches retail**:
`lbz r0,72(r3)` is `optional_object<CFilePreload>::m_valid` at `+0x48`, so `!mPreload` is right.

| spelling | bytescmp |
|---|---|
| baseline | 10 of 28 (6 real) |
| all five arguments bound to `const` locals | 10 of 28 (6 real) |
| `!mPreload.valid()` instead of `!mPreload` | 10 of 28 (6 real) |

WALL: CScriptStreamedMusic::StartStream 74.29% - prologue load hoist plus call-argument block
order; three spellings produce byte-identical code.

### The iterator-range `basic_string` constructor (216 B, 99.56%) - 5 real diffs, a wall

1. The four outgoing spill slots before `bl internal_allocate` are allocated pairwise the other
   way: retail `r4->16(r1), r6->20(r1), r5->8(r1), r0->12(r1)` (that is, `first`'s words get the
   low slots); ours `r4->8, r6->12, r5->16, r0->20`.
2. In the copy loop the two bases are swapped: retail `lwz r5,0(r7)` (the source string data
   pointer) and `lwz r6,0(r31)` (`this->mPtr`); ours `r6`/`r5`.

| spelling | bytescmp |
|---|---|
| baseline | 9 of 54 (5 real) |
| `const char c = *it;` then `mPtr[i] = c` | 9 of 54 |
| comma operands `++i, it = it + 1` | 9 of 54 |
| `while (it != last)` instead of `for` | 9 of 54 |
| `internal_allocate(static_cast<int>(len + 1))` | 9 of 54 |

All four are **byte-identical** to the baseline: MWCC fixes this loop's registers and spill slots
below the source level (they come out of `const_linear_iterator::operator*`,
`include/rstl/linear_iterator.hpp:16`, and `basic_string::operator[]`).

WALL: rstl::basic_string iterator-range ctor 99.56% - 4 spill slots and 2 loop registers differ;
four spellings produce byte-identical code.

### `internal_search` (260 B, 96.69%) - 28 real diffs, a wall

Structurally identical to retail (same rotated loop, same `r12`/`r29` inner-loop induction), but
MWCC's numbering is shifted: retail hoists `r9 = (schar)*searchFirst`, `r10 = first.mPtr`,
`r11 = first.mIndex`, `r31 = last.mPtr`, `r30 = index`; ours uses `r8, r9, r10, r12, r31`.

| spelling | bytescmp |
|---|---|
| baseline | 28 of 65 |
| `const char firstChar = *searchFirst;` | 28 of 65 |
| `const const_iterator end = last;` and compare against `end` | 60 of 62, **248 B - 12 short of retail** |
| comma order `++index, ++it` and `++search, ++nextData, ++next` | 28 of 65 |

WALL: rstl::basic_string::internal_search 96.69% - pure register numbering; four spellings, the
only structural one loses 12 bytes.

### `LoadStreamedAudio` (728 B, 87.38%) - a real modelling difference, not a wall

Ours is **648 bytes against retail's 728** (162 instructions against 182), so ~20 instructions of
work are missing, and the shape differs from the first loop body. The relocation lists pin it:

| | retail | ours |
|---|---|---|
| ctor call | `__ct__20SLdrEditorPropertiesFv` | `__ct__17SLdrStreamedAudioFv` |
| dtor call | `__dt__20SLdrEditorPropertiesFv` | `__dt__17SLdrStreamedAudioFv` |
| `rstl::string` dtor (`internal_dereference`) | **2** | 1 |

`include/MetroidPrime/ScriptLoader/SLdrStreamedAudio.hpp` declares
`SLdrStreamedAudio(); ~SLdrStreamedAudio();` (defined empty in
`src/MetroidPrime/ScriptLoader/SLdrStructMembers.cpp:139-140`), so constructing the local calls the
out-of-line `__ct__17SLdrStreamedAudioFv` and the members' destructors are hidden behind
`__dt__17SLdrStreamedAudioFv`. Retail calls the **base's** ctor/dtor directly and destroys **two**
strings, which is what an *implicitly-default-constructed* aggregate (or a type **derived from**
`SLdrEditorProperties`) would produce. The case bodies themselves match: both sides have
`LoadTypedefSLdrEditorProperties`, 2x `ReadFloat`, `ReadBytes`, `assign`, one
`rstl::string(CInputStream&, const rmemory_allocator&)`, `AllocateUniqueId`,
`LdrToEntityInfo`, the `CScriptStreamedMusic` constructor and one placement `new`.

**I did not chase this**: removing the declared ctor/dtor from a generated header also changes
what `SLdrStructMembers.cpp` emits, and `SLdrStreamedAudio` is not this unit's to reshape. Left for
a lane that owns the script-loader structs. Not filed as `NEW:` - it is this same unit.
---

# Second run (lane 7, 2026-10-02): `LoadStreamedAudio` 87.38% -> 100.00%

The section above ends with "`LoadStreamedAudio` ... a real modelling difference, not a wall ...
**I did not chase this**". It was worth chasing: that one function is now matched, and the unit
rises 17 -> 18 of 23.

| | before this run | after |
|---|---|---|
| unit fuzzy | 96.37253% | **98.53%** |
| **matched functions** | **17 / 23** | **18 / 23** |
| `LoadStreamedAudio` | 87.38% (648 of 728 bytes) | **100.00%** (728 bytes) |

`./tools/goal_check.sh build/goal/item.json` -> **PASS** (`matched 12273 -> 12274`, `linked` held at
5863, no function anywhere got worse, no asm added, DOL sha1 `6ef9b491...` and all 86 RELs
unchanged). The five functions the previous run walled are re-measured at the same scores and are
untouched; see "what I did not change" below.

## First: a correction to the reading method, which is what made this possible

The previous run's evidence is right but its *labels* are inverted, and that is why it read as a
job for "a lane that owns the script-loader structs":

- `build/G2ME01/obj/<unit>.o` is **retail** (dtk's split; it carries `.note.split`), and
  `build/G2ME01/src/<unit>.o` is **ours** (`tools/compare_unit.sh` header says so).
- `objdiff.json` for this unit has `target_path: build/G2ME01/obj/...` and
  `base_path: build/G2ME01/src/...` - **reversed** relative to that. objdiff matches by symbol so
  the score is unaffected, but in `objdiff-cli diff` output **`left` is retail and `right` is
  ours**. The previous run read the "ours" ctor call `__ct__17SLdrStreamedAudioFv` out of the wrong
  column's neighbour and concluded retail called the base's ctor.
- `config/G2ME01/symbols.txt` gives `LoadStreamedAudio` `size:0x2D8` = 728 (gap to the next
  symbol). Retail really is 728 bytes and **we were emitting 648** - 20 instructions short, not 80.
  A `build/binutils/powerpc-eabi-nm -S` on each object settles it in one command.

Getting the per-instruction answer needs objdiff's own one-shot JSON, which `decomp_build.sh` does
not use:

    ./build/tools/objdiff-cli diff -p . -u main/MetroidPrime/ScriptObjects/CScriptStreamedMusic \
        -o ldr.json --format json-pretty LoadStreamedAudio__FR13CStateManagerR12CInputStreamRC11CEntityInfo

`left.symbols[].instructions[]` carries `diff_kind`; the twenty entries that are `DIFF_DELETE` with
no `instruction` are exactly retail's twenty extra instructions.

## The twenty missing instructions, and what they are

They are a **default-initialisation block retail writes in its own frame** before the property
loop. With `data` at r1+44 (0x2c), retail's layout of the aggregate is
`editorProperties` 0..59, `songFile` 60..75, `defaultAudio` 76, `fadeInTime` 80, `fadeOutTime` 84,
`volume` 88, `softwareChannel` 92, `softwareIsMusic` 96 - which is exactly the layout our generated
header already produces (`sizeof` probe: `SLdrEditorProperties` 60, `SLdrStreamedAudio` 100), so
nothing about the struct's *shape* was wrong. What was wrong was the constructor:

| | retail | ours (before) |
|---|---|---|
| ctor | `__ct__20SLdrEditorPropertiesFv` (the member's) | `__ct__17SLdrStreamedAudioFv` (the aggregate's) |
| dtor | `__dt__20SLdrEditorPropertiesFv` | `__dt__17SLdrStreamedAudioFv` |
| `songFile` | default-constructed in place: `mNull`, 0, 0 at r1+104/108/112 | not constructed |
| scalars | `3`, 0, false, 0.0f, 0.0f, 127, 0, true | untouched |
| tail | `cmplwi r29,0` / `beq` / `mr r3,r29` / `bl internal_dereference` | inside `__dt__17...` |

`grep 17SLdrStreamedAudio config/G2ME01/symbols.txt` is **empty**: retail has no such constructor
at all. The declared pair in `include/MetroidPrime/ScriptLoader/SLdrStreamedAudio.hpp` (a
`s/scripts/generate_script_loaders.py` artefact) was the whole bug - it forced a `bl` to a symbol
retail does not define and hid every member's own construction. Removing it is the fix; the
aggregate's members are then constructed in place, which is retail's shape.

## The `3` at r1+100 (aggregate +56) is redundant, and still has to be there

`3` is stored at aggregate+56, which is `editorProperties.unknown_0x5d298a43` - and
`__ct__20SLdrEditorPropertiesFv` (0x8023F0E4) *already* stores 3 at +0x38. So retail writes the
value twice. It is not an in-class initialiser that MWCC would fold into the constructor: retail
**calls** that constructor out of line, so only a statement in the caller can produce the store.
Dropping it costs 4 bytes and 170 differing instructions (measured).

The other six defaults are the aggregate's own members and are written the same way, which is this
repo's existing convention for these generated structs - see `src/MetroidPrime/ScriptObjects/
CScriptRelay.cpp:18-19`, `SLdrRelay sldrThis; sldrThis.oneShot = false;`.

## Two spellings that fixed the last 21 instructions

With the constructor fixed, 21 instructions still differed, all register allocation:

| spelling | differing instrs (of 182) |
|---|---|
| `const int propertyCount`, `const uint propertyId = input.ReadInt32()` | 21 |
| `const uint propertyId = input.Get< uint >()` | 7 |
| `const u16 propertyCount` alone | 14 |
| `const u16 propertyCount` + `Get< uint >()` | **0** |
| `Get< uint >()` + `Get< ushort >()` for the size | 21 |
| `u16 i` for the loop counter | 64 instrs, 183 total - wrong size |

`Get< uint >()` is `return ReadInt32();`, so this is not about the call - it is about the `int` ->
`uint` narrowing at the assignment, which stops MWCC materialising the id in a sixth register
(r6 instead of r4). `CScriptRelay.cpp:23-24` records the same finding for the same line of code.
`const u16` for the count matters for the same reason one step later: it moves `propertyCount` and
`&data.songFile` between r29 and r30.

## Files changed (four; no `configure.py`, no carve, no `splits.txt`, no asm, no deleted work)

- `include/MetroidPrime/ScriptLoader/SLdrStreamedAudio.hpp` - dropped `SLdrStreamedAudio();` and
  `~SLdrStreamedAudio();`, with a comment recording that retail has no such symbols.
- `src/MetroidPrime/ScriptLoader/SLdrStructMembers.cpp` - dropped the two now-undeclared
  definitions. (That file is in `files.cmake` but in no `splits.txt` entry and no `build.ninja`
  rule, so it is not compiled by the matching build and nothing in the DOL moves.)
- `src/MetroidPrime/ScriptObjects/CScriptStreamedMusic.cpp` - the eight default stores and the two
  spellings inside `LoadStreamedAudio`, nothing else.
- `docs/HANDOFF.md` - the state block, rewritten by `tools/gate.sh` during `goal_check.sh`, not by
  hand.

## What I did not change, and one new wall of my own

Re-measured this run, unchanged from the run above: `StopNonDsp` 77.78%, `StopStream` 73.33%,
`StartStream` 74.29%, the iterator-range `basic_string` ctor 99.56%, `internal_search` 96.69%.

I re-tried `StopNonDsp` with eight spellings the earlier run did not list - `this->mFadeOut`,
`mFadeOut + 0.f`, `&mFadeOut` then `*f`, a trailing bare `return`, a comma expression, a local
assigned and then re-assigned - and all eight are **byte-identical** to the baseline. One of them
(`FadeBackIn(mFadeOut), mFadeOut = mFadeOut;`) does move the instruction, and it moves it the wrong
way (it adds the `r31` save the baseline does not have), so the schedule is reachable from source
and retail's is not one of the shapes that reaches it.

WALL: CScriptStreamedMusic::StopNonDsp 77.78% - retail's `lfs f1,60(r3)` sits between `mflr` and
the LR spill; eight further spellings tried this run all produce byte-identical code, and the one
that does reschedule moves the load later, not earlier.

No `NEW:` line: nothing here is a separate unit's blocker - the script-loader struct fix landed in
this unit.
