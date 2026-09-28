/**
 * `fn_800098CC` - retail `.text:0x800098CC`, `size:0x164` = 356 bytes, 0x800098CC..0x80009A30.
 * The next symbol, `fn_80009A30`, starts at 0x80009A30, so that is the exact end of the range
 * this unit claims. **`NonMatching` at 99.55%** - see "The last seven instructions" below; the
 * first half of the pair, `fn_80009898`, is `SGameStateMemcardReset.cpp` and is `Matching`.
 *
 * ## What the function does
 *
 *     800098d4  stw   r4,80(r3)     +0x50 = 0
 *     800098d8  addi  r4,r3,84      the destination base: `&x54_buf[0]`
 *     800098e0  mtctr r0            9 - the main loop's trip count
 *     800098e4  lbz   r6,-32749(r13)   `lbl_80417D93`, reloaded before **every** store
 *     800098e8  addi  r5,r5,8
 *     800098ec  lwz   r0,80(r3)     +0x50
 *     800098f0  stbx  r6,r4,r0      x54_buf[+0x50] = the byte
 *     800098f4  lwz   r6,80(r3)
 *     800098f8  addi  r0,r6,1
 *     800098fc  stw   r0,80(r3)     +0x50 = +0x50 + 1
 *     ... the same five instructions, eight times ...
 *     800099a8  bdnz  800098e4
 *     800099ac  subfic r0,r5,76     76 - 72: the remainder's trip count
 *     800099b4  cmpwi r5,76
 *     800099b8  bge   800099d8
 *     800099bc  ... the same five instructions, once ...
 *     800099d4  bdnz  800099bc
 *     800099d8  lwz   r6,160(r3)    a count at `this + 0xA0`
 *     ... a 22-instruction loop whose body is empty ...
 *     80009a24  li    r0,0
 *     80009a28  stw   r0,160(r3)     +0xA0 = 0
 *
 * **The source is one loop of 76 iterations doing one byte append each, and mwcceppc has
 * unrolled it by eight.** Nothing in the source says "72" or "9"; those are the unroller's
 * arithmetic: 9 iterations of a straight-line body of eight appends (`r5` steps by 8 and is dead
 * in the body - the loop variable is never read), then a bottom-tested remainder of 76 - 72 = 4.
 * The same 8-then-remainder shape appears at the end of `fn_80142BA4` (0x80142C08) and again in
 * this function's own tail, which is how it was recognised. **Measured**: written as the obvious
 * `for (int i = 0; i < 76; ++i)` the unit came out at 77.16%, and the three changes below took
 * it to 99.19% - and 99.55% for the unit as it stands, which is the same code measured
 * against a reference frame that no longer contains `fn_80009898`.
 *
 * ## Three spelling facts, each load-bearing
 *
 * 1. **The destination has to be a named `unsigned char*` local.** Written as the member
 *    subscript, `self->x54_buf[self->x50_size] = k`, mwcceppc folds `+0x54` into the index and
 *    emits `addi r0,r5,84 ; stbx r6,r3,r0` - `self` as the base and 84 in the displacement.
 *    Retail computes the base **once**, before the loop (`addi r4,r3,84`), and indexes it with
 *    the bare count: `stbx r6,r4,r0`. 77.16% -> 99.19% on this change alone.
 * 2. **`+0x50` is re-read before every store and re-written after every store**, so it is *not*
 *    a local counter. It is `self->x50_size` written through `self` each time, which is what
 *    `base[self->x50_size] = k; self->x50_size = self->x50_size + 1;` compiles to when the
 *    compiler cannot rule out that a store through the base pointer lands on `+0x50` itself. A
 *    local counter would be held in a register and the function would be a fifth of the size.
 * 3. **`lbl_80417D93` must be a mutable `unsigned char`.** Retail reloads it before each of the
 *    eight stores in the unrolled body, which it only has to do because a byte store through an
 *    arbitrary pointer may alias a byte global. `const` lets mwcceppc hoist the load out of the
 *    body. This is the finding `CGameStateMemcardCtor.cpp` records at length for the same two
 *    loops in the same struct, and it is worth the same 12 bytes a loop here.
 *
 * `lbl_80417D93` is **not** named for the address 0x80417D8D; it is the symbol `symbols.txt`
 * gives that address. Read the relocations out of `build/G2ME01/obj/auto_03_80009880_text.o` and
 * the `lbz` is `R_PPC_EMB_SDA21 lbl_80417D93`, nine times; the 16-bit displacement the
 * instruction carries is what the linker recomputes from it. `_SDA_BASE_` is 0x8041FD80
 * (`tools/sda.py`), and 0x8041FD80 - 32749 = 0x80417D8D.
 *
 * ## The tail: a loop whose body is empty
 *
 *     800099d8  lwz   r6,160(r3)    the count, at `this + 0xA0`
 *     800099e4  ble   80009a24
 *     800099f8  srwi  r0,r0,3        (n - 8 + 7) >> 3
 *     80009a08  addi  r4,r4,8
 *     80009a0c  bdnz  80009a08
 *     80009a10  subf  r0,r4,r6
 *     80009a20  bdnz  80009a20       the remainder's body: one instruction, itself
 *     80009a24  li    r0,0
 *     80009a28  stw   r0,160(r3)
 *
 * This is the unroller again, on a loop **with an empty body**: the eight unrolled copies of the
 * body are gone but the `addi r4,r4,8` that steps the index stayed, and the remainder is a `bdnz`
 * to itself. The same shape *with* the stores present is `fn_80142BA4`'s tail at 0x80142CC4,
 * which is the comparison that identifies it. So the source really is
 * `for (int i = 0; i < *(this + 0xA0); ++i) { }` - **measured**, not guessed: putting a store in
 * the body (`base[i] = 0`, `tail->xa4_unk[i] = 0`, `self->x50_size = i`) stops the unroll
 * entirely and the unit drops to 81-88%, so the body cannot be a dead store that the unroller
 * removed; it was empty to begin with. What the loop *means* - a 0x48-byte region at `+0xA0` that
 * `include/MetroidPrime/Player/CGameState.hpp` still calls `x98_unk[0x48]` "unrecovered" - is not
 * recovered here either: the loop has no effect, so the only observable fact is that `+0xA0`
 * ends up 0.
 *
 * `+0xA0` is a **word**, not a byte: `fn_80009DBC` writes it with `stw r0,160(r31)` and this
 * function reads it with `lwz r6,160(r3)`. The header keeps the whole 0xA0..0xE7 span as
 * `u8 x98_unk[0x48]`, so the word is overlaid here as `SMemcardA0` rather than named there - the
 * same arrangement `CPersistentOptionsCtor.cpp` uses, and no header changes. 0xA0 + 0x48 = 0xE8,
 * the measured size of the struct.
 *
 * ## The last seven instructions
 *
 * Everything above is byte-exact. The last 22 instructions of the tail loop differ in **register
 * numbers only**, and the whole difference is one swap:
 *
 * ```
 * retail:  lwz  r6,160(r3) ; li  r4,0 ; ... ; addi r5,r6,-8 ; ... ; addi r4,r4,8 ; subf r0,r4,r6
 * ours:    lwz  r6,160(r3) ; li  r5,0 ; ... ; addi r4,r6,-8 ; ... ; addi r5,r5,8 ; subf r0,r5,r6
 * ```
 *
 * The bound is already in `r6`; the loop index and the `(n-8)` temporary have `r4` and `r5` the
 * other way round. Getting the bound into `r6` at all was worth 3 instructions and is a real
 * finding: a `for` loop puts the bound in `r5`, and **rewriting it as `while` with the increment
 * in the body puts it in `r6`**, as retail has it (`cmpwi r6,0`). The remaining r4/r5 pair did
 * not move for any of about thirty spellings measured with `tools/try_batch.py`: `for` and
 * `while`, `++i` / `i++` / `i += 1`, the increment in the third clause or the body, `int` / `u32` /
 * `long` for either variable, the bound hoisted into a local or read in the condition, the index
 * and the bound declared in either order or at function scope, the loop in a nested scope, the
 * destination pointer at function scope or inside a nested block, a `volatile` read, and an extra
 * unused local. The one theory that fit - that the still-in-scope destination pointer reserves
 * `r4` - was tested by putting that pointer in a nested block and it made no difference. This is
 * the register allocator, and it is the reason the unit is `NonMatching`.
 *
 * **Not in `files.cmake`** - see this file's entry in `tools/check_files_cmake.py`.
 *
 * **After the merge to upstream PrimeDecomp/echoes** the bytes this fills at `+0x50`/`+0x54` are
 * upstream's `CControlMapper::mCommandOverridden` - the same `int mCount` then 76 bytes - and the
 * `+0xA0` word whose loop is empty is `mCommandOverrides`'s count, with `+0xA4`..`+0xE3` its
 * eight `rstl::pair<ECommands, int>` slots; all at the same offsets. `SGameStateMemcard` therefore
 * survives as the named overlay in `include/MetroidPrime/Player/CGameStateBlocks.hpp` (which
 * `CGameState.hpp` includes) and the code below is unchanged, including the `SMemcardA0` overlay.
 * The retail name and the `SGameStateMemcard*` parameter are kept because the symbol is what
 * `config/G2ME01/symbols.txt` calls it and `fn_80009898` declares it that way.
 */
#include "types.h"

#include "MetroidPrime/Player/CGameState.hpp"

// `lbl_80417D93`, `.sdata:0x80417D93`, `size:0x5 data:byte` - the symbol `symbols.txt` gives
// address 0x80417D8D, and the one the `R_PPC_EMB_SDA21` relocations in
// `build/G2ME01/obj/auto_03_80009880_text.o` name. **Not `const`**, and that is load-bearing.
extern "C" unsigned char lbl_80417D93;

// +0xA0 is a word, and the header keeps it inside a `u8[0x48]`. See the header comment.
struct SMemcardA0 {
  u32 xa0_count;
  u8 xa4_unk[0x44];
};
CHECK_SIZEOF(SMemcardA0, 0x48)

extern "C" void fn_800098CC(SGameStateMemcard* self) {
  self->x50_size = 0;
  // The destination has to be a named `unsigned char*`; see the header comment.
  unsigned char* base = self->x54_buf;
  for (int i = 0; i < 76; ++i) {
    base[self->x50_size] = lbl_80417D93;
    self->x50_size = self->x50_size + 1;
  }
  // A loop whose body retail's bytes leave empty. See the header comment. The overlay goes
  // through the **named** `xa0_unk` member rather than `+ 0xA0`, so this file has no raw offset
  // and does not appear in `docs/research/raw_offsets.md` - measured, same 99.55% either way.
  // **The member's address, not its value.** This line was written when the header's +0xA0 was
  // a `u8` array, which decays to its address; when the header made it `u32 xa0_unk` the cast
  // silently became a cast of the *count* to a pointer - 99.55% fell to 98.30%, and on the host
  // `new CGameState` segfaulted here storing through it (found by `tools/boot_probe.sh`, lane
  // `frame`, 2026-09-26). The `&` restores both.
  SMemcardA0* tail = reinterpret_cast< SMemcardA0* >(&self->xa0_unk);
  int i = 0;
  int n = static_cast< int >(tail->xa0_count);
  while (i < n) {
    ++i;
  }
  tail->xa0_count = 0;
}
