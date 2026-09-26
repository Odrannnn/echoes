# The allocator's flag mask was host-derived, and that was the boot's first crash

The port's first real failure was `CGameAllocator::Alloc` returning null for **135,168 bytes
(0x21000) out of a 24 MB free block**, followed by a `SIGSEGV` in `DumpAllocations` dereferencing
a null iterator. This is the measurement that found it, and it is worth keeping because every
step of it was a hypothesis that measurement killed.

## What is proven

`FindFreeBlock` walks bins from `GetFreeBinEntryForSize(len)` up to 15. For the failing request it
starts at bin 13, finds 13 and 14 empty, reaches the block in bin 15, and **rejects it**:

```
  +262  bin 13 is empty -> binIndex++
  +262  bin 14 is empty -> binIndex++
  +107  IsAllocated()==0     candidate=0x7fff9ccc5120   bin=15
  +116  x4_len >= len        x4_len=180220  len=135168          <- passes
  +130  delta < bestDelta    delta=4295012348  bestDelta=0x10000000   <- FAILS
```

The block is free, large enough, and has a successor. The best-fit difference reads **4,295,012,348**
where the true difference is `180220 - 135168 = 45052`. `0x100000AF4` is `45052 + 2^32`: the
subtraction borrowed across a 32-bit boundary because `x4_len` was read as a 64-bit quantity whose
upper half was not zero.

That upper half is not a length. It is the top of a heap address:

```
block 3  0x00007fff9d4c6120
   +0x00 x0_priorGuard  0x00007fff9d4b0080   <- not 0xefefefef
   +0x08 x4_len         0x00007fff9d4b00c0   <- not 180220
   +0x38 x1c_postGuard  0x00005555565f934c   <- a code address
```

**The walk had left the block list and was reading payload as headers.** The cause is in
`GetNext()`:

```cpp
SGameMemInfo* GetNext() const {
  return (SGameMemInfo*)((uintptr_t)x14_next & ~(kAllocatorPointerBits - 1));
}
```

with `kAllocatorPointerBits = sizeof(void*) * 8`. On retail that is 32 and the mask is `0x1F` -
**five flag bits**. On a 64-bit host it is 64 and the mask is `0x3F` - **six**. The sixth bit is
`0x20`, and with a `0x40` block stride `0x20` is *address, not a flag*:

```
block 2 at 0x7fff9d4c60c0
  x4_len        = 32
  x14_next      = 0x7fff9d4c6100
  expected next = block + sizeof(SGameMemInfo) + x4_len = 0x7fff9d4c6120
  short by      = 32 bytes
```

`x14_next` was *stored* correctly. The read stripped the `0x20`, so every step was 32 bytes short
and landed on the block's own payload. The setters hide this completely - they OR the old low bits
back in (`SetNext` keeps `ptr & mask | new & ~mask`) - so nothing looks wrong until a read.

## The fix

`include/Kyoto/Alloc/AllocatorCommon.hpp`: fix the flag count at retail's value instead of
deriving it from the host's pointer width.

```cpp
static const int kAllocatorPointerBits = 32;
```

**This is not a host-specific patch and must not be written as one.** The reasoning to preserve:

- The number of flag bits is a property of **retail's allocator protocol**, not of `sizeof(void*)`.
  Retail's block pointers are `0x20`-aligned, so its `0x20` bit is always clear and
  `sizeof(void*) * 8 == 32` happens to describe the flag field correctly. That coincidence is what
  made the host-derived version look right for years.
- Decoupling the two is what makes the fix hold on any host. The derivation is precisely the defect:
  it makes the mask silently a function of the machine being debugged.
- It is a **no-op for the decomp build** - MWCC pointers are 32-bit, so `sizeof(void*) * 8` was
  already 32 - which is why it cannot move the DOL. Verified, not assumed: `GATE PASS`,
  `matched 3186 -> 3186`, `linked 1802 -> 1802`, DOL sha1 `6ef9b491...`, 86/86 RELs.
- `kAllocatorPointerSize` stays `sizeof(void*)`. It is genuinely about the host, and it still drives
  `EXPAND_PATTERN` and the top-nybble mask, which *should* widen with the pointer. Only the flag
  count was wrong to widen.

## What the fix changed at runtime

The old first crash is gone. `Alloc(135168)` no longer returns null, `DumpAllocations` is no longer
reached, and the probe walks past the allocator into the next two boot requirements:

```
[reach-stub 0012] mp_cswarmbasics   (mp_cswarmbasics)
[reach-stub 0013] mp_swarm          (mp_swarm)
```

The heap chain is now arithmetically consistent end to end, read with the mask the code uses:

```
  #    addr               len          alloc    next               guards
  0    0x00007fff9d400040 720896       1        0x00007fff9d4b0080 both intact
  1    0x00007fff9d4b0080 90112        1        0x00007fff9d4c60c0 both intact
  2    0x00007fff9d4c60c0 32           1        0x00007fff9d4c6120 both intact
  3    0x00007fff9d4c6120 180220       0        0x00007fff9ebfbf60 SMASHED
  4    0x00007fff9ebfbf60 0            0        0x0000000000000000 both intact
```

Block 3 is the free 180,220-byte block and its successor is the tail. **Two defects remain in it**,
neither of which blocks the search now that the mask is right, and both recorded as open below.

## Still open, measured

- **The free block's header is not fully initialised - a SECOND, independent defect.** With the
  mask fixed the walk is arithmetically consistent, and the invariant
  `(next_header - this_header - 64) == x4_len` holds for every allocated block. It fails for the one
  free block, which is the block the failing 135,168-byte request actually wants:

```
  #   addr               len          len_hi     guard    next               verdict
  0   0x00007fff9d400040 720896       0x00000000 intact   0x00007fff9d4b0080 ok
  1   0x00007fff9d4b0080 90112        0x00000000 intact   0x00007fff9d4c60c0 ok
  2   0x00007fff9d4c60c0 32           0x00000000 intact   0x00007fff9d4c6120 ok
  3   0x00007fff9d4c6120 180220       0x00000001 SMASHED  0x00007fff9ebfbf60
                                     DIRTY high=0x1 GUARDS-BAD  [delta=24337920 != len]
```

  Block 3 reports **180,220** bytes and must cover **24,337,920** - a factor of 135 short, and the
  request is 135,168. Its prior guard is smashed and the high half of its length is `0x1`, which is
  the signature of a **32-bit store into a 64-bit field**: the low word is a real value and the
  garbage above it is left over. It is also *not* simply uninitialised memory - at an earlier point
  in the boot the same block is perfect (`len=24428192`, upper half 0, both guards intact), so
  something writes it and something later overwrites it.

  **The obvious fix is wrong, and this is now measured twice rather than assumed.** Narrowing
  `x4_len` to `uint` was tried twice - once changing the ctor parameter and `SetLength` as well, and
  once changing **only** the member declaration, to isolate it. Both times
  `FindFreeBlock` fell **100.00% -> 99.46%** (`matched 3186 -> 3185`) and the change was reverted.
  The second, minimal attempt is the informative one: it shows the loss is caused by the *member
  type alone*, so the cause is not a widened parameter or a mismatched setter.

  **What that tells us, and it revises the obvious reading of the disassembly.** Retail's
  `FindFreeBlock` compares `candidate->x4_len - len < bestDelta` with `bestDelta` a `uint`. In this
  tree `x4_len` is a `size_t`, so that one expression promotes to 64-bit and mwcceppc emits a
  64-bit subtract and compare - the mixed-width pair I originally read as a host artifact. **It is
  not an artifact: it is retail's own shape, and it is what makes the function match at 100%.**
  Retail's `SGameMemInfo` is 0x20 with 4-byte words, so retail's `x4_len` is genuinely 32-bit there
  and the comparison is genuinely 32-bit - and the host's 64-bit `size_t` is what reproduces those
  bytes. So the two builds want *different* widths for the same declaration, and no single member
  type satisfies both. That is the real constraint, and it is why this is a port-side problem
  rather than a header edit.

  **What would actually unblock it** - none of these tried:
  - Fix the *writer*, not the width: find the 32-bit store that leaves the high half dirty. A
    hardware watchpoint on the block's length field from before it is constructed gives the exact
    `FixupAllocPtrs`/`Release` line. Two attempts to place the watchpoint computed the block address
    wrongly, so this is still open - do the arithmetic from the *measured* block addresses above
    rather than by re-deriving them.
  - Or give the port a build-time width: `x4_len` is `uint` under the port's own macro and
    `size_t` under MWCC. That is a real option precisely because the two builds want different
    widths, but it needs a decision on how the port macro is spelled, and it must not perturb the
    MWCC side.
  - Or accept it: the block is *found* now, and only its reported length is wrong. A request under
    180,220 bytes would succeed against it. That is a workaround, not a fix, and it is stated here
    so the next session does not mistake it for one.
- **`x10_last` is not stride-aligned**: with `x8_heapSize = 0x17FBF60`, `heapSize mod 0x40 == 0x20`,
  so the tail sits at a `0x20`-aligned address whose low 5 bits are `0x20`. Harmless for the flag
  mask now that it is `0x1F`, but the tail is still not a valid block address. Rounding the span in
  `CGameAllocator::Initialize` was tried - it works and it is a no-op on retail - but it perturbs
  `Initialize`'s Matching state, so it was reverted in favour of the mask fix. **If a unit-movement
  report is ever authorised, this is the change to authorise.** Note the *shape* of this one is
  different from the `x4_len` blocker above and the difference matters: this is a **value** change on
  a path retail computes identically, whereas `x4_len` is a **width** the two builds disagree about.
- The committed `PortReachStubs.cpp` had drifted from HEAD's sources: a rebuild surfaced undefined
  references including `CMain::StreamNewGameState`, one of the named port blockers. The set is
  regenerated from `tools/link_reach.py`, and the reachable count moved **318 -> 332**.

## The transferable lesson

`a measurement of an artefact is not a measurement of a task` had a sibling here. The first
hypothesis - arena too small, allocation failed, block not binned, bin search misses it - were all
eliminated by measurement, and the fifth, "the candidate is too small", was *also* eliminated before
the real cause appeared. Ten gdb sessions went into this because **every intermediate measurement
was correct and none of them was the measurement that mattered.**

The one that mattered was dumping the raw 0x40 bytes of consecutive blocks and asking *is this a
header at all?* The guards answered it immediately - `0xefefefef` absent, a code address in the post
guard - and that reframed the whole problem from "the length arithmetic is wrong" to "the walk is
not on the block list". `PROCESS_LESSONS.md` has this as: a wrong measurement of the right quantity
costs more than no measurement, because it is believed.

The second lesson is narrower and cost the same: **`sizeof(T)` and `sizeof(void*)` are not
interchangeable stand-ins for a protocol constant.** Deriving a retail protocol field from a host
property compiles, passes review, reproduces retail exactly on retail's own word size, and is wrong
on every other one.
