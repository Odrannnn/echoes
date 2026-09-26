/**
 * The port's `CARAMManager`: `Alloc`, `Free`, the two DMA directions, the three DMA queries, the
 * wait-for-all, and retail's initialiser `fn_80301CC4`. **This file is port-only**: `configure.py`
 * does not declare it, so mwcceppc never sees it and it is not a decompilation unit - the same
 * arrangement as `src/Kyoto/CSimplePoolPort.cpp`.
 *
 * **Echoes' ARAM manager is not Prime 1's**, and `src/Kyoto/CARAMManager.cpp` is Prime 1's (one
 * pool, `Alloc(uint)`), which is why that file is still excluded in `files.cmake`. Retail Echoes,
 * measured from `build/G2ME01/asm/auto_03_8030188C_text.s`:
 *
 *  * `fn_80301CC4(chunk1, size1, chunk0)` - called from `CMain::InitializeSubsystems` at
 *    0x800087B0 as `(0x800, 0x600000, 0x1000)` - builds **two** chunk allocators of 0x18 bytes,
 *    `{ aramStart, chunkSize, numChunks, chunksAllocated, peakChunks, bookkeeping* }`, and stores
 *    them in the two words at `lbl_80419B08`: `[1]` is `size1 / chunk1` chunks of `chunk1` (6 MB of
 *    2 KB chunks) and `[0]` is the rest of ARAM, `(ARGetSize() - lbl_80418BA8 - size1) / chunk0`
 *    chunks of `chunk0`. Each allocator's ARAM comes from `ARAlloc`, its bookkeeping from
 *    `CMemory::Alloc`, zero-filled from `lbl_8041E938` (.sdata2, `00000000`). It then zeroes the
 *    DMA id counter `lbl_80419B04` and sets `lbl_80419B00` (initialised).
 *  * `Alloc(len, pool)` (0x80301C50) and `Free(ptr, pool)` (0x80301C20) are `slwi r0,r4,2 ;
 *    lwzx r3,lbl_80419B08,r0` and a tail call on that allocator: **the `void*` second argument is a
 *    pool index**, and every caller that omits it gets pool 0. The allocator's `Alloc`
 *    (`fn_80302028`) and its first-fit search (`fn_80301F8C`) are Prime 1's `CARAMManager::Alloc`
 *    and `FindFreeBlocks` moved into a class, plus a high-water mark at +0x10; `Free`
 *    (`fn_80301E7C`) returns `false` for `GetInvalidAlloc()` and otherwise zeroes the block's run.
 *  * `DMAToARAM` / `DMAToMRAM` (0x80301B38 / 0x80301994) `new` a 0x28-byte request - an
 *    `ARQRequest`, the id at +0x20 and a done byte at +0x24 - push it on the list at
 *    `lbl_804175B8`, post it with `fn_803017C0` as the callback and priority `prio == kDMAPrio_One`,
 *    and return the id; the counter skips `kInvalidHandle` (-1). `fn_803017C0` sets the done byte.
 *  * `IsDMACompleted`, `WaitForDMACompletion` and `CancelDMA` walk that list by id. The first two
 *    free and erase a finished request; an id that is not in the list is complete.
 *
 * **The one host difference is how a DMA completes.** On the cube the ARQ callback is an interrupt.
 * Aurora's `ARQPostRequest` does the copy at once and *defers* the callback until `ARQPoll()`
 * (extern/aurora/lib/dolphin/AR.cpp: "invoking it synchronously recurses until the stack
 * overflows"), and nothing in the game calls `ARQPoll`. So every place below that retail would sit
 * waiting for the interrupt polls first. Without that, `WaitForDMACompletion` - retail's is a bare
 * spin on the done byte - never returns on the host, and `CPakFile::DataLoad` calls it for every
 * world pak.
 *
 * ARAM "addresses" are what they are on the cube and what `CDvdFile` already hands Aurora: offsets
 * into ARAM, carried in a pointer. `GetInvalidAlloc()` is `(const void*)-1` at full host width.
 */
#include "Kyoto/CARAMManager.hpp"

#include "Kyoto/Alloc/CMemory.hpp"

#include "rstl/list.hpp"

#include "dolphin/ar.h"

#include <stdio.h>

// .sdata 0x80418BA8, `00004000`. Retail's `CMain::InitializeSubsystems` adds `ARAlloc`'s answer to
// it and `fn_80301CC4` subtracts it from `ARGetSize()`: the ARAM below it is not the manager's.
// Nothing else in the host build defines it - the only other mention is the retail body in
// `mainTail.cpp`, which the host does not compile.
extern "C" uint lbl_80418BA8 = 0x4000;

namespace {

// Retail's 0x18-byte allocator. Field names are descriptive; retail's are not known.
struct SARAMChunkAllocator {
  uint x0_aramStart;
  uint x4_chunkSize;
  uint x8_numChunks;
  uint xc_chunksAllocated;
  uint x10_peakChunks;
  uint* x14_bookkeeping;

  SARAMChunkAllocator(uint chunkSize, uint numChunks)
  : x0_aramStart(ARAlloc(chunkSize * numChunks))
  , x4_chunkSize(chunkSize)
  , x8_numChunks(numChunks)
  , xc_chunksAllocated(0)
  , x10_peakChunks(0)
  , x14_bookkeeping(static_cast< uint* >(CMemory::Alloc(numChunks * sizeof(uint),
                                                        IAllocator::kHI_None))) {
    for (uint i = 0; i < x8_numChunks; ++i) {
      x14_bookkeeping[i] = 0;
    }
    CMemory::OffsetFakeStatics(x8_numChunks * sizeof(uint));
  }

  // `fn_80301F8C`: first fit for `count` free chunks in [start, end), stepping over allocated
  // runs by their recorded length. -1 when there is no room.
  uint FindFreeBlocks(uint start, uint end, uint count) const {
    while (start < end) {
      const uint run = x14_bookkeeping[start];
      if (run != 0) {
        start += run;
        continue;
      }
      if (count == 1) {
        return start;
      }
      ++start;
      uint found = 1;
      while (start < end) {
        const uint next = x14_bookkeeping[start];
        if (next != 0) {
          start += next;
          break;
        }
        if (++found == count) {
          return start - (count - 1);
        }
        ++start;
      }
    }
    return static_cast< uint >(-1);
  }

  // `fn_80302028`. The first chunk of a run records its length, the rest are marked -1.
  void* Alloc(uint len) {
    uint count = (x4_chunkSize + len - 1) / x4_chunkSize;
    const uint block = FindFreeBlocks(0, x8_numChunks, count);
    if (block == static_cast< uint >(-1)) {
      return const_cast< void* >(CARAMManager::GetInvalidAlloc());
    }
    xc_chunksAllocated += count;
    if (x10_peakChunks < xc_chunksAllocated) {
      x10_peakChunks = xc_chunksAllocated;
    }
    x14_bookkeeping[block] = count;
    for (uint i = block; --count != 0;) {
      x14_bookkeeping[++i] = static_cast< uint >(-1);
    }
    return reinterpret_cast< void* >(static_cast< uintptr_t >(x0_aramStart + block * x4_chunkSize));
  }

  // `fn_80301E7C`.
  bool Free(const void* ptr) {
    if (!CARAMManager::IsAllocValid(ptr)) {
      return false;
    }
    const uint offset = static_cast< uint >(reinterpret_cast< uintptr_t >(ptr));
    uint block = (offset - x0_aramStart) / x4_chunkSize;
    uint count = x14_bookkeeping[block];
    xc_chunksAllocated -= count;
    while (count-- != 0) {
      x14_bookkeeping[block++] = 0;
    }
    return true;
  }
};

// Retail's 0x28-byte DMA request.
struct SAramDMARequest {
  ARQRequest x0_request;
  uint x20_id;
  bool x24_complete;
};

typedef rstl::list< SAramDMARequest* > SRequestList;

SARAMChunkAllocator* sAllocators[2];  // lbl_80419B08
uint sNextDMAId;                      // lbl_80419B04
bool sInitialized;                    // lbl_80419B00
SRequestList* sActiveDMAs;            // lbl_804175B8

SARAMChunkAllocator* Allocator(const void* pool) {
  return sAllocators[reinterpret_cast< uintptr_t >(pool)];
}

// `fn_803017C0`. Retail also invalidates the destination of an ARAM -> MRAM transfer; the host
// has no data cache to invalidate, and Aurora's `ARQRequest` keeps addresses in 32 bits, so the
// host pointer is not recoverable from it anyway.
void DMACallback(uintptr_t request) { reinterpret_cast< SAramDMARequest* >(request)->x24_complete = true; }

uint PostDMA(uint type, uintptr_t source, uintptr_t dest, uint len,
             CARAMManager::EDMAPriority priority) {
  SAramDMARequest* request = rs_new SAramDMARequest();
  request->x24_complete = false;
  request->x20_id = sNextDMAId;
  sActiveDMAs->push_back(request);
  ARQPostRequest(&request->x0_request, request->x20_id, type,
                 priority == CARAMManager::kDMAPrio_One ? ARQ_PRIORITY_HIGH : ARQ_PRIORITY_LOW,
                 source, dest, len, DMACallback);
  if (++sNextDMAId == static_cast< uint >(CARAMManager::GetInvalidDMAHandle())) {
    ++sNextDMAId;
  }
  return request->x20_id;
}

SRequestList::iterator FindDMA(uint handle) {
  SRequestList::iterator it = sActiveDMAs->begin();
  for (; it != sActiveDMAs->end(); ++it) {
    if ((*it)->x20_id == handle) {
      break;
    }
  }
  return it;
}

} // namespace

extern "C" void fn_80301CC4(uint chunkSize1, uint size1, uint chunkSize0) {
  const uint available = ARGetSize() - lbl_80418BA8;
  sAllocators[1] = rs_new SARAMChunkAllocator(chunkSize1, size1 / chunkSize1);
  sAllocators[0] = rs_new SARAMChunkAllocator(chunkSize0, (available - size1) / chunkSize0);
  if (sActiveDMAs == nullptr) {
    sActiveDMAs = rs_new SRequestList();
  }
  sNextDMAId = 0;
  sInitialized = true;
}

void* CARAMManager::Alloc(uint len, const unkptr pool) {
  if (!sInitialized) {
    // Retail would dereference a null allocator here. On the host a boot that reaches a pak
    // before `PortInitializeSubsystems` is a sequencing bug, and this says which one.
    printf("CARAMManager::Alloc(%u) before fn_80301CC4 initialised the ARAM pools\n", len);
    return const_cast< void* >(GetInvalidAlloc());
  }
  return Allocator(pool)->Alloc(len);
}

void CARAMManager::Free(const void* ptr, const unkptr pool) {
  if (sInitialized) {
    Allocator(pool)->Free(ptr);
  }
}

int CARAMManager::DMAToARAM(void* src, void* dest, uint len, EDMAPriority priority) {
  return PostDMA(ARQ_TYPE_MRAM_TO_ARAM, reinterpret_cast< uintptr_t >(src),
                 reinterpret_cast< uintptr_t >(dest), len, priority);
}

int CARAMManager::DMAToMRAM(void* src, void* dest, uint len, EDMAPriority priority) {
  return PostDMA(ARQ_TYPE_ARAM_TO_MRAM, reinterpret_cast< uintptr_t >(src),
                 reinterpret_cast< uintptr_t >(dest), len, priority);
}

bool CARAMManager::IsDMACompleted(uint handle) {
  ARQPoll();
  SRequestList::iterator it = FindDMA(handle);
  if (it == sActiveDMAs->end()) {
    return true;
  }
  if (!(*it)->x24_complete) {
    return false;
  }
  delete *it;
  sActiveDMAs->erase(it);
  return true;
}

void CARAMManager::WaitForDMACompletion(uint handle) {
  SRequestList::iterator it = FindDMA(handle);
  if (it == sActiveDMAs->end()) {
    return;
  }
  // Retail spins on the done byte and the interrupt sets it. Here the poll is the interrupt.
  while (!(*it)->x24_complete) {
    ARQPoll();
  }
  delete *it;
  sActiveDMAs->erase(it);
}

bool CARAMManager::CancelDMA(uint handle) {
  ARQPoll();
  SRequestList::iterator it = FindDMA(handle);
  return it == sActiveDMAs->end() || (*it)->x24_complete;
}

// `fn_8030174C` looped by `fn_8030184C`: drop finished requests until none is left.
void CARAMManager::WaitForAllDMAsToComplete() {
  if (sActiveDMAs == nullptr) {
    return;
  }
  while (!sActiveDMAs->empty()) {
    ARQPoll();
    for (SRequestList::iterator it = sActiveDMAs->begin(); it != sActiveDMAs->end();) {
      if ((*it)->x24_complete) {
        delete *it;
        it = sActiveDMAs->erase(it);
      } else {
        ++it;
      }
    }
  }
}
