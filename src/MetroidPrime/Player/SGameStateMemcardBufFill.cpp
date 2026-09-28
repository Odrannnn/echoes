/**
 * `fn_80009AC0` - retail `.text:0x80009AC0`, `size:0x130` = 304 bytes,
 * 0x80009AC0..0x80009BF0. The next symbol, `fn_80009BF0`, starts at 0x80009BF0, so that is the
 * exact end of the range this unit claims - and it is the one contiguous, uncontested range in
 * this block: `MetroidPrime/main.cpp` stops at 0x80009880 and nothing between 0x80009A30 and
 * 0x80009DBC is claimed at all, so nothing had to be re-split to claim this.
 *
 * It is the first of the two calls `fn_80009898` (`SGameStateMemcardReset.cpp`, Matching) makes,
 * and it builds `SGameStateMemcard`+0x00..+0x4F - the same object the other call rebuilds at
 * +0x50..+0x9F:
 *
 *     80009ac0  li    r4,0
 *     80009ac4  li    r0,9          9 = 72 / 8: the unroller's trip count, nothing else
 *     80009ac8  stw   r4,0(r3)      +0x00 = 0
 *     80009acc  li    r4,0          the loop index, stepped by 8 per unrolled iteration
 *     80009ad0  mtctr  r0
 *     80009ad4  lwz   r0,0(r3)      +0x00
 *     80009ad8  addi  r4,r4,8
 *     80009adc  lbz   r6,-32750(r13)  `lbl_80417D92`, reloaded before **every** store
 *     80009ae0  add   r5,r3,r0      `this + count`
 *     80009ae4  stb   r6,4(r5)      x04_buf[count] = the byte
 *     80009ae8  lwz   r5,0(r3)      +0x00
 *     80009aec  addi  r0,r5,1
 *     80009af0  stw   r0,0(r3)      +0x00 = +0x00 + 1
 *     ... the same seven instructions, eight times ...
 *     80009bb8  bdnz  80009ad4
 *     80009bbc  subfic r0,r4,76      76 - 72: the remainder's trip count
 *     80009bc0  mtctr  r0
 *     80009bc4  cmpwi r4,76
 *     80009bc8  bgelr
 *     80009bcc  ... the same six instructions, once ...
 *     80009be8  bdnz  80009bcc
 *     80009bec  blr
 *
 * **The source is one loop of 76 iterations doing one byte append each, and mwcceppc has
 * unrolled it by eight** - 9 iterations of a straight-line body of eight appends, then a
 * bottom-tested remainder of 76 - 72 = 4. Nothing in the source says "72", "9" or "4"; those are
 * the unroller's arithmetic. This is the same body as `fn_800098CC`
 * (`SGameStateMemcardFill.cpp`, NonMatching at 99.55%) with the members moved from +0x50/+0x54
 * to +0x00/+0x04, and the same 8-then-remainder shape appears at the end of `fn_80142BA4`
 * (0x80142C08) and in that function's own empty-bodied tail, which is how it was recognised.
 *
 * ## `NonMatching` at 85.20%, and the reason is one instruction *pair* repeated nine times
 *
 * All 76 instructions are right and the object is the right size; **27 instructions differ and
 * all 27 are the nine stores.** Retail spells one store
 *
 *     lwz   r0,0(r3) ; lbz r6,byte ; add r5,r3,r0 ; stb r6,4(r5)
 *
 * and mwcceppc spells the same store
 *
 *     lwz   r5,0(r3) ; lbz r6,byte ; addi r0,r5,4 ; stbx r6,r3,r0
 *
 * - i.e. it folds the constant +4 into the index and uses the indexed store instead of
 * materialising `this + count` and carrying +4 in the displacement. Everything around it, in
 * both the unrolled body and the remainder loop, is byte-identical.
 *
 * **The mechanism is measured, and it is the fold, not the source.** `tools/probe_unroll_store_form.cpp`
 * settles it, compiled with this unit's own flags and disassembly counted per function:
 *
 * - **Outside an unrolled loop, the fold does not happen.** `s->buf[s->n] = k; s->n = s->n + 1;`
 *   with no loop emits exactly retail's `add rA,rB,rC ; stb rD,4(rA)`. So the source spelling
 *   that reproduces retail's store form *exists* - it is the one below.
 * - **Inside mwcceppc's 8-wide unroll of a 76-trip loop, it always happens.** **65 body variants
 *   were measured with `tools/try_batch.py` across five batches, plus 58 more loop bodies in a
 *   probe - 123 spellings - and not one changed the store form.** The eight worth keeping a record
 *   of are in `tools/variants_fn_80009AC0.py`; the rest were: the member subscript and its
 *   `->`/`(*)`/array-then-index variants; a named `unsigned char*` base (which hoists the base and
 *   emits `addi r4,r3,4 ; stbx` instead, and comes out *worse* at 81 differing instructions); the
 *   count through a pointer, a reference, a `u32`/`int`/`long` cast and a `volatile` member;
 *   `++`/`++x`/`x++`/`x += 1`; `for`/`while`/`do`; the bound as 76, `0x4C` and `sizeof(buf)`;
 *   the buffer reached as a nested struct's member, a two-level member offset, an array of one
 *   struct, a base class, a derived class, a union overlay, and as `char`/`unsigned char`/
 *   `short`/`u32` arrays at offsets 4, 8, 16 and 0x54; a named `u8*` recomputed every iteration;
 *   extra dead locals; an `if` around the loop; two loops in the function; and a `void` return.
 * - The trip count is what selects it, not the source: at 76 (and 12, 16, 24, 64, 68, 72, 77, 100)
 *   the fold happens; a **4-trip** loop of the identical body, which mwcceppc fully unrolls with
 *   no remainder, keeps retail's `add`+`stb`. Retail's own bytes have the 8-wide unroll *and*
 *   the unfolded store, which is the one combination this compiler does not produce.
 * - Optimisation level is not the lever either: `-O2` and `-O4,p` at trip 4 both keep `stb`, and
 *   plain `-O4` (which does not unroll) folds.
 *
 * So this is recorded as a compiler limit, not as an unfinished body: the source below is the
 * one that reproduces retail's store form outside the unrolled loop, and the unit is
 * **`NonMatching` because the unroll undoes it**. The claim is kept so objdiff measures the
 * 85.20% rather than the function reading as "not started"; a `NonMatching` object is not in the
 * DOL link, so the claim costs nothing and `dtk` fills the range with retail's own bytes.
 *
 * ## Two spelling facts that are load-bearing anyway
 *
 * 1. **`lbl_80417D92` must be a mutable `unsigned char`.** Retail reloads it before each of the
 *    eight stores in the unrolled body, which it only has to do because a byte store through an
 *    arbitrary pointer may alias a byte global; `const` lets mwcceppc hoist the load out of the
 *    body and `const volatile` hoists it too, so what stops the hoist is precisely the missing
 *    `const`. Worth the same 12 bytes a loop as in the other two fills in this struct.
 * 2. **`+0x00` is re-read before every store and re-written after every store**, so it is not a
 *    local counter. It is `self->x00_size` written through `self` each time, which is what
 *    `x04_buf[x00_size] = k; x00_size = x00_size + 1;` compiles to when the compiler cannot rule
 *    out that the store lands on `+0x00` itself. A local counter would live in a register and the
 *    function would be a fifth of this size.
 *
 * ## The third of the four fill bytes, and what the other two are
 *
 * `lbl_80417D92` is `.sdata:0x80417D92`, `size:0x1 data:byte`, and it is the byte this function
 * fills with - the `R_PPC_EMB_SDA21 lbl_80417D92` relocations in
 * `build/G2ME01/obj/auto_03_80009A30_text.o` (nine of them, 0x80009ADC + 0x1C * n and the tail)
 * name it exactly, with no addend, so no arithmetic is involved: `_SDA_BASE_` is 0x8041FD80
 * (`tools/sda.py`) and 0x8041FD80 - 32750 = 0x80417D92. This is the **third** byte of the four-byte
 * run at 0x80417D90 that this struct's three other fills draw from - `lbl_80417D90`
 * (0x8041FD80 - 32752) and `lbl_80417D91` (- 32751) in `fn_80009DBC`, and `lbl_80417D93`
 * (- 32749) in `fn_800098CC` - so the four are 0x80417D90..0x80417D93, one `.sdata` run of four
 * one-byte objects, and the last of them is the 5-byte `lbl_80417D93`.
 *
 * ## What the function is *for*, and a correction to two existing headers
 *
 * `fn_80009AC0` writes `+0x00 = 0`, appends 76 bytes at `+0x04`, and leaves `+0x00 = 76`; then
 * `fn_800098CC` does exactly the same to `+0x50`/`+0x54`. So after `fn_80009898` returns, both
 * counts read 76. `fn_80009DBC` had already stored 76 at both counts before calling it, so **the
 * whole of `fn_80009898` is idempotent on a `SGameStateMemcard` that `fn_80009DBC` has just
 * built** - it refills the first buffer's 76 bytes of `lbl_80417D90` with 76 bytes of
 * `lbl_80417D92`.
 *
 * **Corrected while writing this, 2026-09-26 (lane v4): the two existing headers say the counts
 * end at 72, and they end at 76.** `include/MetroidPrime/Player/CGameState.hpp`'s
 * `SGameStateMemcard` comment and `src/MetroidPrime/Player/CGameStateMemcardCtor.cpp`'s header
 * both say `fn_800098CC` "stores 72 at +0x50 and fills +0x54..+0x9B". Both misread the unroller:
 * 72 is only the *main* loop (9 * 8), and the remainder loop at 0x800099BC runs
 * `subfic r0,r5,76` = 4 more times. 76 bytes is also what makes the two ends agree - `x04_buf`
 * and `x54_buf` are `u8[76]`, and 72 would leave four bytes of each buffer unwritten. **No header
 * was edited**: the miscount is in prose only, the layout it supports (`+0x00`, `+0x04..+0x4F`,
 * `+0x50`, `+0x54..+0x9F`) is already the right one, and `CHECK_SIZEOF(SGameStateMemcard, 0xe8)`
 * is unaffected.
 *
 * `extern "C"` for the reason every retail-named function in this area has one: retail's symbol
 * table calls this `fn_80009AC0`, and a C++ spelling would mangle to a name objdiff has nothing
 * to pair against. It returns `this` (`r3` is never overwritten on the way out), so it is
 * declared returning the pointer even though `fn_80009898`, its only caller, discards it.
 *
 * **Not in `files.cmake`** - see this file's entry in `tools/check_files_cmake.py`.
 *
 * **After the merge to upstream PrimeDecomp/echoes** the 0xE8 bytes this fills at `+0x00`..`+0x4F`
 * are upstream's `CControlMapper::mCommandEnabled` - the `int mCount` at `+0x00` and the 76 bytes
 * at `+0x04` - at the same offsets, so `SGameStateMemcard` survives as the named overlay in
 * `include/MetroidPrime/Player/CGameStateBlocks.hpp` (which `CGameState.hpp` includes) and the
 * loop below is unchanged. The retail name and the `SGameStateMemcard*` parameter are kept because
 * the symbol is what `config/G2ME01/symbols.txt` calls it and `fn_80009898` declares it that way.
 */
#include "types.h"

#include "MetroidPrime/Player/CGameState.hpp"

// `lbl_80417D92`, `.sdata:0x80417D92`, `size:0x1 data:byte` - the symbol the `R_PPC_EMB_SDA21`
// relocations name, at the address exactly. **Not `const`**, and that is load-bearing; see the
// header comment.
extern "C" unsigned char lbl_80417D92;

extern "C" {
SGameStateMemcard* fn_80009AC0(SGameStateMemcard* self) {
  self->x00_size = 0;
  for (int i = 0; i < 76; ++i) {
    self->x04_buf[self->x00_size] = lbl_80417D92;
    self->x00_size = self->x00_size + 1;
  }
  return self;
}
} // extern "C"
