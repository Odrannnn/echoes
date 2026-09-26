/**
 * `CIOWinManager::PumpMessages` (retail 0x800496A0, 0xC4 = 196 bytes) and
 * `CArchitectureQueue::Pop` (retail `fn_800495F0`, 0x800495F0, 0xB0 = 176 bytes).
 *
 * **`NonMatching`, and the reason is not `rc_ptr`.** The two words of an `rc_ptr` are what f1's
 * layout change bought; everything else here is written. What is left is that
 * `rstl::list<CArchitectureMessage>::do_erase` is a *template* member, so this compiler emits it
 * `W` (COMDAT) where retail has `fn_80048F78` as a strong `T`, and a `Matching` unit that has to
 * define one will not reproduce retail's binding. `Pop` calls it, so `Pop` cannot be `Matching`,
 * and `PumpMessages` calls `Pop`, so `PumpMessages` cannot be either. Both claim their retail
 * range in `splits.txt` so objdiff measures them, which is safe: a `NonMatching` object is not in
 * the link.
 *
 * ## `PumpMessages`' shape
 *
 * A bottom-tested loop on the queue's `x14_count` (`lwz r0,20(r29) ; cmpwi r0,0 ; bne`), entered
 * by an unconditional branch to the test - mwcceppc's loop rotation, not a spelling.
 *
 * Inside, two 16-byte `CArchitectureMessage`s, not one, and that is not a choice: mwcceppc 2.7 does
 * not elide the copy out of a return value. `CArchitectureMessage msg = queue.Pop();` puts `Pop`'s
 * return slot at `r1+8` and copy-initialises `msg` at `r1+0x18` from it, and the return slot's
 * destructor runs at the end of the full expression - which is *between* the copy and the use of
 * `msg`. That is the order in retail's bytes:
 *
 *   800496d0  mr      r4,r29            ; &queue
 *   800496d4  addi    r3,r1,8           ; Pop's return slot
 *   800496d8  bl      fn_800495F0
 *   800496dc  lwz     r5,8(r1)          ; the 16-byte copy: four words out of the slot ...
 *   ...
 *   800496fc  stw     r4,36(r1)         ; ... into r1+0x18
 *   80049700  lwz     r3,0(r4)          ; AddRef through the *fourth* word, i.e. +0x0c
 *   80049704  addi    r0,r3,1
 *   80049708  stw     r0,0(r4)
 *   8004970c  beq     80049718          ; DEAD: tests r31 = &slot.x8, never null
 *   80049710  mr      r3,r31
 *   80049714  bl      ReleaseData__Q24rstl34rc_ptr<24IArchitectureMessageParm>Fv
 *   80049718  mr      r3,r28            ; this
 *   8004971c  mr      r5,r29            ; &queue
 *   80049720  addi    r4,r1,24          ; &msg
 *   80049724  bl      fn_8004935C
 *   80049728  cmplwi  r30,0             ; DEAD again, on &msg.x8
 *   8004972c  beq     80049738
 *   80049730  mr      r3,r30
 *   80049734  bl      ReleaseData__Q24rstl34rc_ptr<24IArchitectureMessageParm>Fv
 *   80049738  lwz     r0,20(r29) ; cmpwi r0,0 ; bne 800496d0
 *
 * **The two dead `beq`s are retail's and are reproduced here on purpose.** `r31` and `r30` are
 * `&slot.x8` and `&msg.x8`, hoisted out of the loop because they are loop-invariant addresses, and
 * `cmplwi <address>,0` is never true. They are what a `NonMatching` unit gets for free and what a
 * `Matching` one must not "fix".
 *
 * `fn_8004935C` (0x8004935C, 0x1D0) is retail's and is **unnamed in the map**, so it is called here
 * through an `extern "C"` declaration rather than guessed at: it reads `x4(this)` (the pump list),
 * calls the out-of-line copy constructor on each node's `x0_iowin` and walks, and the caller
 * discards its result. `include/MetroidPrime/CIOWinManager.hpp` declares a
 * `bool DistributeOneMessage(const CArchitectureMessage&, CArchitectureQueue&)` whose shape fits,
 * but nothing measured pins the name, so the port-side spelling stays the map's.
 *
 * ## `Pop`'s shape, for whoever writes `do_erase`
 *
 * The local is at `r1+8`, its initialising copy reads the *node's* item (`lwz r5,4(r4)` is
 * `x4_start`, and the four loads are at `+8`, `+0xc`, `+0x10`, `+0x14` on it, i.e. the item at node
 * +8), `pop_front` is `bl fn_80048F78` with `r4 = 4(queue)`, and the return copy is four loads out
 * of `r1+8` into the caller's slot followed by an AddRef through the slot's fourth word and the same
 * dead `beq`. `fn_80048F78` is `rstl::list<CArchitectureMessage>::do_erase`: relink, destroy the
 * item through `ReleaseData`, `CMemory::Free(node)`, `--x14_count`.
 */

#include "MetroidPrime/CIOWin.hpp"

#include "MetroidPrime/CIOWinManager.hpp"

// Retail's `fn_8004935C`, unnamed in config/G2ME01/symbols.txt. See above.
extern "C" void fn_8004935C(CIOWinManager* self, const CArchitectureMessage* msg,
                            CArchitectureQueue* queue);

// **Descending by retail offset, because mwcceppc emits definitions in reverse source order.**
// `Pop` is retail 0x800495F0 and `PumpMessages` is 0x800496A0, so `PumpMessages` is declared
// first here and lands second in the object. Ascending, the two are permuted and
// `tools/check_decl_order.py` says so - which is the only thing that reports it, and it is
// harmless only because the unit is `NonMatching`.

void CIOWinManager::PumpMessages(CArchitectureQueue& queue) {
  while (!queue.IsEmpty()) {
    CArchitectureMessage msg = queue.Pop();
    fn_8004935C(this, &msg, &queue);
  }
}

CArchitectureMessage CArchitectureQueue::Pop() {
  CArchitectureMessage result = *x0_queue.begin();
  x0_queue.pop_front();
  return result;
}
