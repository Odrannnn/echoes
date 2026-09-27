/**
 * `CResFactory::AsyncIdle(unsigned int, bool)` - retail `AsyncIdle__11CResFactoryFUib`,
 * `.text:0x802FA384`, `size:0x10C` = 268 bytes, the one function between a constructed
 * `gpResourceFactory` and `CMain::AsyncIdle` (boot-path step 21e, `src/MetroidPrime/main.cpp:307`).
 *
 * **Port-only**: `configure.py` does not declare this file, so mwcceppc never sees it and it is
 * not a decompilation unit - the same arrangement as `src/Kyoto/CResFactoryPortVirtuals.cpp`.
 * It exists because the port's link has asked for `_ZN11CResFactory9AsyncIdleEjb` since the
 * written `CMain::AsyncIdle` started calling `gpResourceFactory->AsyncIdle(time, flag)`, and it
 * is written from retail's own instructions rather than stood in for: every construct below is a
 * numbered instruction of 0x802FA384..0x802FA490 and the two callees are retail's own two.
 *
 * Retail's body, in order (`tools/dis.sh 0x802FA384 0x10C`), `r26` = this, `r27` = time,
 * `r28` = flag, `r25` = the `stop` byte:
 *
 * ```
 * 802fa3a0  bl    OSGetTime                  ; start = now, saved in r30/r29
 * 802fa3a4  lwz   r31,204(r26)               ; it = xc8_active.x4_start  (this+0xCC)
 * 802fa3b4  mr    r25,r31 / lwz r31,4(r31)   ;   cur = it; it = cur->x4_next   <- read BEFORE the
 * 802fa3bc  lwz   r3,20(r25)                 ;   request = cur->x8_item+0x0C       erase, which
 * 802fa3c0  lwz   r12,0(r3) / lwz r12,16(r12);   request->IsComplete()             frees cur
 * 802fa3d8  mr    r4,r25 / addi r3,r26,200 / bl fn_802FB2E4   ; if complete: erase from +0xC8
 * 802fa3e4  lwz   r0,208(r26) / cmplw r31,r0 / bne 802fa3b4   ; while (it != x8_end)
 * 802fa400  li    r25,1                     ; stop = true
 * 802fa404  bl    OSGetTime
 * 802fa408  subfc/subfe r4,r3 over r29,r30   ; now - start (r3:hi, r4:lo)
 * 802fa40c  lwz   r5,8(r31) / lwz r6,12(r31); divisor = mData+8 = x8_timerFreqO1M
 * 802fa418  bl    __div2i                   ; elapsed = (now - start) / ticksPerMicro
 * 802fa424  bge   802fa468                   ; if ((uint)elapsed >= time) -> the header
 * 802fa428  lwz   r0,176(r26) / beq 802fa468 ; if (x9c_loading.size() == 0) -> the header
 * 802fa434  lwz   r0,160(r26) / stw r0,8(r1) ; entry = x9c_loading.x4_start, on the stack
 * 802fa440  subf  r5,r5,r27                  ; budget = time - elapsed
 * 802fa448  bl    fn_802FA1BC                ; done = pump(this, &entry, budget)
 * 802fa44c  clrlwi r3,r3,24 / cntlzw / srwi  ; stop = !done
 * 802fa460  beq   802fa468                   ; if (!flag) -> the header
 * 802fa464  li    r25,0                      ; if (flag) stop = false
 * 802fa468  clrlwi. r0,r25,24 / bne 802fa47c ; while (!stop)
 * 802fa470  lwz   r0,176(r26) / bne 802fa400 ;   && x9c_loading.size() != 0
 * ```
 *
 * Three things in that are load-bearing and none of them were obvious before measuring:
 *
 *  * **`r31` at 0x802FA3F8 is `&CStopwatch::mData`, not a member of this class.** `lis
 *    r3,-32703 / addi r31,r3,4176` is `0x80411050`, which `config/G2ME01/symbols.txt` names
 *    `mData__10CStopwatch` (`.bss`, `size:0x18`), and the two words read out of it are
 *    `x8_timerFreqO1M` - `CStopwatch::CSWData`'s `s64` at +0x08, ticks per microsecond, written
 *    by retail's own `CStopwatch::CSWData::Initialize` as `stw r3,8(r31)` / `stw r4,12(r31)`
 *    (0x8028C1C0-0x8028C1C4). It is `__div2i`'s divisor with `r5` the high word and `r6` the low,
 *    which is why the load is `8(r31)` then `12(r31)` rather than a single word. That is also
 *    what fixes `time`'s unit: `CMain::AsyncIdle` passes 500, 5000 and 1000000, so the elapsed
 *    count has to be **microseconds**, which is exactly `GetElapsedMicros()`'s arithmetic
 *    (`include/Kyoto/Basics/CStopwatch.hpp:47`).
 *  * **The element's type is `CDvdRequest`, and the slot is `IsComplete`.** `lwz r3,20(r25)`
 *    with `r25` = the node is `x8_item+0x0C`; both `fn_802FAF1C` (the enqueue) and this
 *    function vcall through the pointer it holds. The vtable offsets only close if MWCC's vptr
 *    points at the **vtable symbol's base**, two zero words before the first slot - which
 *    `CResFactory`'s own constructor states outright by storing `0x803BAF08` (the `__vt__`
 *    symbol, not `+8`) - and then `vptr+0x10` is `CDvdRequest::IsComplete` and `vptr+0x18` is
 *    `CDvdRequest::GetMediaType`. `include/Kyoto/CDvdRequest.hpp`'s own slot comments say the
 *    same thing, and `src/MetroidPrime/mainMid.cpp:434` already relies on it: "vtable slot 4 =
 *    CDvdRequest::IsComplete - not a guess". `fn_802FC898`, which fills the slot at
 *    `item+0x0C` (`fn_802FA140`'s fifth argument, `stw r5,12(r3)`), is the loader's async read
 *    and `CDvdFile::AsyncSeekRead` returns `CDvdRequest*`
 *    (`include/Kyoto/CDvdFile.hpp:53`).
 *  * **The list is walked through `x4_start`/`x8_end`, not `rstl::list`'s API.** The header
 *    models both lists as `SLoadList`, six words with no element type, so this file spells the
 *    node itself. `+0x14` for the request and `+0x04` for the next node are the node's own
 *    header words, and nothing else in the element is read.
 *
 * What this file costs the port's link, and why it is a trade rather than a win: retail's erase
 * is out of line, so the sweep needs `fn_802FB2E4`, and that symbol is not in the port's
 * undefined list today. `_ZN11CResFactory9AsyncIdleEjb` is, and defining it removes exactly one
 * symbol, so the two cancel: the port's link was **322 undefined with `AsyncIdle` open** and is
 * **322 undefined with `fn_802FB2E4` open instead**. That is the only new hole, it is a named
 * one (`fn_802FB2E4`, 0x8C = 140 bytes, body in `docs/research/paks.md`'s neighbourhood and
 * printed by `tools/dis.sh 0x802FB2E4 0x80`), and `fn_802FA070` - the item destructor it calls -
 * is *not* referenced by this object, so it does not appear twice.
 */
#include "Kyoto/CResFactory.hpp"

#include "Kyoto/Basics/CStopwatch.hpp"
#include "Kyoto/CDvdRequest.hpp"

// `fn_802FA1BC` - .text:0x802FA1BC, `size:0x12C` - the pump, declared exactly as
// `src/Kyoto/CResFactoryBuild.cpp` declares it (that file calls it with a budget of 0, this one
// with the frame's remaining microseconds). Its second argument is the *address* of a local
// holding the loading list's head node, which is why retail writes `x9c_loading.x4_start` to
// `r1+8` first and passes `r1+8`.
extern "C" bool fn_802FA1BC(const void* self, void* entry, unsigned int frames);

// `fn_802FB2E4` - .text:0x802FB2E4, `size:0x8C` - `rstl::list`'s erase for this element type:
// `x4_start = node->next` when the node is the head, unlink, destroy the item
// (`fn_802FA070(item, -1)`), `CMemory::Free(node)`, `--x14_count`, return the next node. Retail
// calls it here at 0x802FA3E0; `CancelBuild` has its own copy (`fn_802FA514`).
extern "C" void fn_802FB2E4(void* list, void* node);

namespace {
// The node retail's loop walks. `rstl::list`'s own header, then the item - `fn_802FA140`
// (retail's entry ctor, 0x802FA140) writes the tag's two words at item+0x00/+0x04, a `bool` at
// item+0x08 and the request at item+0x0C, and the rest of it out to item+0x38. This function
// reads exactly one of those fields, so exactly one is modelled.
struct SBuildRequest {
  uchar x0_unknown[0x0C];
  CDvdRequest* xc_request; // +0x0C
};

struct SLoadNode {
  SLoadNode* x0_prev;
  SLoadNode* x4_next;
  SBuildRequest x8_item;
};
} // namespace

void CResFactory::AsyncIdle(uint time, bool flag) {
  // Retail's first instruction after the prologue is `bl OSGetTime` (0x802FA3A0), so the clock
  // is read before the sweep and the sweep's cost counts against `time`.
  const OSTime start = OSGetTime();

  // The sweep, 0x802FA3A4-0x802FA3EC. `it` advances to `cur->x4_next` *before* the possible
  // erase, because the erase frees `cur` - retail loads `r31` at 0x802FA3B8 and only then tests
  // and erases, which is the difference between this and a use-after-free.
  for (SLoadNode* it = static_cast< SLoadNode* >(xc8_active.x4_start);
       it != static_cast< SLoadNode* >(xc8_active.x8_end);) {
    SLoadNode* cur = it;
    it = it->x4_next;
    if (cur->x8_item.xc_request->IsComplete()) {
      fn_802FB2E4(&xc8_active, cur);
    }
  }

  // The timed half, 0x802FA3F0-0x802FA47C. Retail's divisor is `CStopwatch::mData`'s
  // `x8_timerFreqO1M`; the accessor is the public route to the same word.
  const s64 ticksPerMicro = CStopwatch::GetGlobalTimerFreqO1M();

  // **The one line retail does not have, and it says so.** PPC's `div` with a zero divisor does
  // not trap - it answers an unpredictable value and the loop simply misjudges its budget - but
  // the host's does, with SIGFPE, and `mData` is only initialised by
  // `CStopwatch::InitGlobalTimer()` -> `Reset()` (`src/Kyoto/Basics/CStopwatch.cpp:8`, called on
  // the boot path from `src/Kyoto/Basics/COsContext.cpp:95`). So this test is for a factory that
  // is idling before the stopwatch has ever run, and with the lists in the empty state the
  // constructor gives them - the state nothing enqueues into until `BuildAsync` has a real body
  // - both answers reach the same place: no pump call, immediate return.
  bool stop = false;
  while (!stop && x9c_loading.x14_count != 0) {
    stop = true;
    const OSTime now = OSGetTime();
    const uint elapsed =
        ticksPerMicro > 0 ? static_cast< uint >((now - start) / ticksPerMicro) : 0u;
    if (elapsed >= time) {
      continue;
    }
    // Retail has this test even though nothing between here and the loop header mutates the
    // list (0x802FA428-0x802FA430, with `r25` already 1, so it exits through the header either
    // way). It is kept because it is what guards the pump call below.
    if (x9c_loading.x14_count == 0) {
      continue;
    }
    void* entry = x9c_loading.x4_start; // 0x802FA434, spilled to `r1+8` at 0x802FA444
    const bool done = fn_802FA1BC(this, &entry, time - elapsed);
    stop = !done;
    if (flag) {
      // 0x802FA464: with the flag up the loop does not stop on a finished pump, it goes round
      // again and lets the `time` and `count` tests end it.
      stop = false;
    }
  }
}
