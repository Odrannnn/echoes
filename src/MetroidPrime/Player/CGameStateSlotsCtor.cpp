#include "types.h"

#include "MetroidPrime/Player/CGameStateBlocks.hpp"

// Retail .text:0x80144924..0x801449C8 - **164 contiguous bytes, two functions**, and the whole
// of the constructor machinery of the two `{count; block[3]}` members at `CGameState+0x110` and
// `+0x144`.
//
//   fn_80144924(r3 = this, r4 = n, r5 = src)   0x80144924, 0x38 = 56 bytes, 14 instructions
//     x00_count = n;                            stw r4,0(r3)   0x80144938
//     fn_8014495C(this + 4, n, src);            addi r3,r31,4 / bl  0x8014493C..0x80144940
//
//   fn_8014495C(r3 = elems, r4 = n, r5 = src)  0x8014495C, 0x6C = 108 bytes, 27 instructions
//     for (uint i = 0; i < n; i++) fn_80142A10(elems + i, src);
//
// `fn_80144924` does not re-materialise `n` or `src` for the call: `r4` and `r5` are still live
// from its own entry, which is why the body is a store and an `addi` and nothing else. `r4` is
// the count the *caller* asked for, so this is a `(count, element)` fill constructor, not a copy
// of `n` existing elements - and that is what distinguishes it from `fn_80004C90` (0x80004C90),
// the block's copy constructor, which `main.cpp:725`/`742` call and which walks an *array* of
// sources instead (its inner callee `fn_80004CD4` takes `r3 = src->elems`).
//
// The loop in `fn_8014495C` is retail's own rotation: the test sits at the bottom of the block
// and an unconditional `b` at the top jumps into it, so the increment pair
// (`addi r30,r30,1` / `addi r31,r31,16`) is the only thing between the call and the compare.
// The stride is a literal 16 in the `addi` because the element is 16 bytes; the pointer itself is
// kept in `r31` and *incremented* rather than re-indexed, which is what an array-element
// expression in a `for` gives here.
//
// The caller in `CGameState::CGameState(CInputStream&, int)` (retail fn_80144140) passes
// `n = 3` and a 16-byte stack temporary it has just zeroed at `+0x04`, `+0x08` and `+0x0C`
// (0x80144220..0x80144228) - the element's default constructor, inlined, which is the reason
// `SGameStateBlock::x00_unk` is not in the constructor's store list.
//
// Declared **descending by retail offset** - `fn_8014495C` first - because mwcceppc emits function
// definitions in reverse source order and mwldeppc keeps `.text` order verbatim; ascending order
// permutes the bytes and the unit's hash breaks with every function still at 100%.
// `tools/check_decl_order.py` enforces it.
//
// Neither function calls anything unnamed. `fn_80142A10` is the block element's copy
// constructor, and it has its own unit (`MetroidPrime/Player/CGameStateBlockCopy.cpp`) because
// it is 0x848 bytes away from this range and a `configure.py` unit may claim only one range; a
// call to it is a relocation and nothing more, so the two units are independent.
// The loop's two spellings that matter, both measured (see tools/try_batch.py's shape):
// the counter is an `int` and the element pointer is a *separate* variable stepped in the
// increment clause. `for (int i = 0; i < n; i++) fn_80142A10(&elems[i], src);` gives the right
// instructions in the wrong order (`addi r31,r31,16` before `addi r30,r30,1`) and a `cmplw`
// against retail's signed `cmpw`; adding `, p` to the increment clause fixes both, and the
// element-pointer form is what makes MWCC keep a running pointer instead of re-indexing.
// `int n` matters for the same reason: `uint n` gives `cmplw`, retail has `cmpw`.
extern "C" void fn_8014495C(SGameStateBlock* elems, int n, const SGameStateBlock* src) {
  SGameStateBlock* p = elems;
  for (int i = 0; i < n; i++, p++) {
    fn_80142A10(p, src);
  }
}

// **It returns `this`**, and that is not decoration: retail's epilogue is
// `mr r3,r31 ; lwz r31,12(r1)`, so the pointer has to survive the call in a callee-saved
// register and be handed back. That is what a C++ constructor's implicit `return this` compiles
// to, which is how this function was recognised as the block's `(count, element)` constructor
// rather than a free function. No caller uses the value - `fn_80144140` and `fn_801449C8` both
// ignore r3 after the call - and retail kept it anyway, so returning it is the only spelling
// that reproduces the two extra instructions.
extern "C" SGameStateSlots* fn_80144924(SGameStateSlots* self, int n, const SGameStateBlock* src) {
  self->x00_count = n;
  fn_8014495C(self->x04_blk, n, src);
  return self;
}
