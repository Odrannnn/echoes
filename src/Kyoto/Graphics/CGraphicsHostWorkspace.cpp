/**
 * Retail's `fn_8032F6EC` (0x8032F6EC, 0x88 bytes): the skinned-model workspace set-up that
 * `CGraphics::ConfigureVideo` hands its 0x40000-byte arena slice to, and that
 * `CGraphics::Shutdown` / `ConfigureVideo` call with `(nullptr, 0)` to tear down.
 *
 * **Port-only**: `configure.py` does not declare this file, so the DOL objects are byte-identical
 * with or without it. Retail's copy lives in the unsplit `auto_03_8032EE68_text` object; this is
 * a hand-written body of the same behaviour, read from `build/G2ME01/asm/auto_03_8032EE68_text.s`:
 *
 *   1. `lbl_80418C78 = size`
 *   2. empty the list at `lbl_803E0574` (`fn_8032F648` -> `fn_8032F688`: erase from begin to end
 *      with `fn_8032F884`, each of which unlinks a node, `CMemory::Free`s it and decrements the
 *      count at +0x14)
 *   3. `lbl_80419C58 = buffer`, `lbl_80419C64 = 0`
 *   4. if `buffer`: build a `CCircularBuffer(buffer, size, kOS_NotOwned)` temporary
 *      (`__ct__15CCircularBufferFPviQ215CCircularBuffer10EOwnership`, ownership argument 1) and
 *      assign it into the `optional_object<CCircularBuffer>` at `lbl_803E054C`
 *      (`fn_8032F774` -> `fn_8032F7A4`, which is `rstl::optional_object::operator=`); the
 *      temporary's own free is dead because `NotOwned` releases the pointer.
 *
 * The three globals and the two objects are referenced by nothing else the port compiles, so they
 * get host-typed storage rather than guest layouts; the list is walked through a struct that
 * mirrors the node shape the asm shows (+0 prev, +4 next, payload after).
 */
#include "Kyoto/Alloc/CCircularBuffer.hpp"
#include "Kyoto/Alloc/CMemory.hpp"

#include <rstl/optional_object.hpp>

namespace {
// Node shape of the list at `lbl_803E0574`: +0 prev, +4 next (fn_8032F884), then a 10-byte
// payload with no destructor.
struct SWorkspaceNode {
  SWorkspaceNode* x0_prev;
  SWorkspaceNode* x4_next;
  uchar x8_payload[10];
};

// The list header, zero until something builds it: fn_8032F688 loops `while (it != end)`, so an
// untouched list is empty. +4 begin, +8 end, +0x14 count in retail.
struct SWorkspaceList {
  SWorkspaceNode* x4_begin;
  SWorkspaceNode* x8_end;
  int x14_count;
};

SWorkspaceNode* EraseNode(SWorkspaceList& list, SWorkspaceNode* node) { // fn_8032F884
  SWorkspaceNode* next = node->x4_next;
  if (node == list.x4_begin) {
    list.x4_begin = next;
  }
  node->x4_next->x0_prev = node->x0_prev;
  node->x0_prev->x4_next = node->x4_next;
  CMemory::Free(node);
  --list.x14_count;
  return next;
}

void ClearList(SWorkspaceList& list) { // fn_8032F648 / fn_8032F688
  SWorkspaceNode* it = list.x4_begin;
  SWorkspaceNode* end = list.x8_end;
  while (it != end) {
    it = EraseNode(list, it);
  }
  list.x4_begin = it;
}

SWorkspaceList sWorkspaceList;                                  // lbl_803E0574
rstl::optional_object< CCircularBuffer > sWorkspaceBuffer;      // lbl_803E054C
} // namespace

extern "C" {
int lbl_80418C78 = 0;      // .sdata, the size argument
void* lbl_80419C58 = nullptr; // .sbss, the buffer argument
int lbl_80419C64 = 0;      // .sbss, zeroed here

void fn_8032F6EC(void* buffer, uint size) {
  lbl_80418C78 = size;
  ClearList(sWorkspaceList);
  lbl_80419C58 = buffer;
  lbl_80419C64 = 0;
  if (buffer != nullptr) {
    CCircularBuffer tmp(buffer, size, CCircularBuffer::kOS_NotOwned);
    sWorkspaceBuffer = tmp;
  }
}
}
