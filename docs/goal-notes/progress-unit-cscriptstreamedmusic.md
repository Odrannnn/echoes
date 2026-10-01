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