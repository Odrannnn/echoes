/**
 * `fn_80009DBC` - retail `.text:0x80009DBC`, `size:0xB8` = 184 bytes, 0x80009DBC..0x80009E74.
 * The next symbol, `fn_80009E74`, is the switch that starts at 0x80009E74, so that is the exact
 * end of the range this unit claims.
 *
 * It is the constructor of the 0xE8-byte `SGameStateMemcard` that sits at `CGameState+0x204`.
 * `CGameState::CGameState()` calls it as `fn_80009DBC(&self->x204, 0)` (0x801442DC) and
 * `CGameState::CGameState(CInputStream&, int)` calls it as `fn_80009DBC(&self->x204, -1)`
 * (0x801442B4), so it takes the block's flag as a second argument - and retail passes it straight
 * through to `+0xE4` (`stw r4,228(r31)`, 0x80009E54).
 *
 * The body is three things and one non-logic item.
 *
 * 1. `x00_size = 76` and `x50_size = 76`, two `stw` of a `li r3,76` (0x80009DD4, 0x80009E0C), and
 *    then each of the two 76-byte buffers is filled with **one byte from `.sdata`, four bytes at a
 *    time, 19 times**: `li r0,19 ; mtctr r0` and `bdnz` at 0x80009DE0/0x80009E08 (buffer 1) and
 *    0x80009E1C/0x80009E44 (buffer 2), so 19 * 4 = 76, which is why the two numbers agree.
 *    The source byte is a different global for each buffer: `lbz r0,-32752(r13)` and
 *    `lbz r0,-32751(r13)`, i.e. `.sdata:0x80417D90` and `.sdata:0x80417D91`
 *    (`lbl_80417D90` and `lbl_80417D91`, both `data:byte`, values 1 and 0 - the pair reads
 *    `01 00 01 00` out of the file). `_SDA_BASE_` is 0x8041FD80 (`tools/sda.py`), and both
 *    displacements are the 16-bit sign-extended distance to it, which is what pins the address:
 *    0x8041FD80 - 32752 = 0x80417D90 and 0x8041FD80 - 32751 = 0x80417D91.
 * 2. The two word writes `xA0 = 0` and `xE4 = flag` (0x80009E50, 0x80009E54).
 * 3. `fn_80009898(self)` - and its result is **discarded**: the epilogue is `mr r3,r31` at
 *    0x80009E60, after the `bl`, so what this function returns is `this`, not the callee's value.
 *
 * **The non-logic item is the four `lbz` per iteration, and it is decided by the *const-ness* of
 * the two globals.** Retail re-reads the same byte before every one of its four byte stores
 * (`lbz/stb, lbz/stb, lbz/stb, lbz/stb`) - 8 loads for 8 stores in the whole function - which it
 * only has to do because it cannot rule out that the buffer it is writing *is* that global. So
 * the declaration below must be a **mutable** `unsigned char`: declaring either of them `const`
 * lets mwcceppc hoist the load out of the loop entirely, and it then emits one `lbz` before the
 * `mtctr` and four `stb`s per iteration against retail's four of each - measured, both ways, with
 * `tools/try_batch.py`. A `const volatile` reading does *not* work either: it gives the same
 * hoisted load, so what stops the hoist is precisely the missing `const`, not the reload.
 *
 * Three more register facts the loops pin. The destination pointer is **stepped** (`addi r3,r3,4`
 * inside the body, 0x80009E04/0x80009E40) and never re-indexed, and the trip count is carried in
 * `ctr` with no counter register in the body at all - so the loop variable is dead inside the body
 * and its bound is the literal 19. Both the pointer variable and the four straight-line stores
 * matter: written as a nested `for (j = 0; j < 4; j++) p[j] = k;` it also comes out byte-exact
 * (mwcceppc unrolls the inner loop), but written as a loop over a byte count it does not.
 * `fn_800098CC` (0x800098CC), the same author's other fill loop in the same struct, strides by 8
 * with `li r0,9` off a third byte at `lbl_80417D8D`, which is the same shape at a different width.
 *
 * **`fn_80009898` is called, never defined** - and it is not harmless. Its inner `fn_800098CC`
 * stores 72 at `+0x50` and fills `+0x54..+0x9B`, so the 76 this constructor puts there does not
 * survive its own last call. That is a fact about the *pair*, recorded here because
 * `include/MetroidPrime/Player/CGameState.hpp` now says so; the two functions are 0xDC bytes apart
 * and a `configure.py` unit may claim only one range, so they cannot be one unit yet.
 *
 * **Not in `files.cmake`, measured.** Listing it puts one more undefined symbol in the port's link
 * (`fn_80009898`) and closes none, because nothing in the port calls this constructor yet -
 * `CGameGlobalObjects`' constructor in `src/MetroidPrime/main.cpp` is still a stub. It becomes
 * worth listing together with that caller and with `fn_80009898`'s own body. See this file's entry
 * in `tools/check_files_cmake.py`.
 */
#include "types.h"

#include "MetroidPrime/Player/CGameState.hpp"

// The two one-byte `.sdata` objects the two buffers are filled with: 0x80417D90 is 1 and
// 0x80417D91 is 0, and nothing in the DOL reads either by name, which is why they are declared
// here rather than typed into the header.
//
// **Not `const`, and that is load-bearing** - see the file comment. `const` here is worth a
// measured 12 bytes per loop.
extern "C" unsigned char lbl_80417D90;
extern "C" unsigned char lbl_80417D91;

// Called last, result discarded - see item 3 above.
extern "C" void fn_80009898(SGameStateMemcard* self);

// C linkage because retail's symbol table has no name for it; a C++ spelling would mangle to
// something objdiff has nothing to pair with. It returns `this` (`mr r3,r31` after the call).
extern "C" {
SGameStateMemcard* fn_80009DBC(SGameStateMemcard* self, int flag) {
  self->x00_size = 76;
  unsigned char* p = self->x04_buf;
  for (int i = 0; i < 19; i++, p += 4) {
    p[0] = lbl_80417D90;
    p[1] = lbl_80417D90;
    p[2] = lbl_80417D90;
    p[3] = lbl_80417D90;
  }

  self->x50_size = 76;
  p = self->x54_buf;
  for (int i = 0; i < 19; i++, p += 4) {
    p[0] = lbl_80417D91;
    p[1] = lbl_80417D91;
    p[2] = lbl_80417D91;
    p[3] = lbl_80417D91;
  }

  self->xa0_unk = 0;
  self->xe4_flag = flag;

  fn_80009898(self);
  return self;
}
} // extern "C"
