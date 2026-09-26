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

- **`x4_len` is a 64-bit `size_t` holding a stale upper half** (block 3 reads
  `0x00007fff0002bffc` where the true length is 180,220). `FindFreeBlock` still computes
  `x4_len - len` in 64 bits, so a dirty upper half would still reject a good block. It does not
  fire today only because this particular block's arithmetic lands below `2^32` when the low word is
  used. **The correct fix is `uint x4_len`, as retail has it** - and that was tried and **reverted**:
  it changes the code `FindFreeBlock` emits and costs that function its 100% match
  (100.00% -> 99.46%, `matched 3186 -> 3185`). The gate is the authority; this needs a way to fix the
  width that does not perturb a Matching function, not a header edit.
- **`x10_last` is not stride-aligned**: with `x8_heapSize = 0x17FBF60`, `heapSize mod 0x40 == 0x20`,
  so the tail sits at a `0x20`-aligned address whose low 5 bits are `0x20`. Harmless for the flag
  mask now that it is `0x1F`, but the tail is still not a valid block address. Rounding the span in
  `CGameAllocator::Initialize` was tried - it works and it is a no-op on retail - but it perturbs
  `Initialize`'s Matching state, so it was reverted in favour of the mask fix. **If a unit-movement
  report is ever authorised, this is the change to authorise.**
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
