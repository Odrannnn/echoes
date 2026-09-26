/* tools/probe_unroll_store_form.cpp - the experiment behind the 85.20% in
 * `src/MetroidPrime/Player/SGameStateMemcardBufFill.cpp` (`fn_80009AC0`).
 *
 * WHAT THIS IS. `fn_80009AC0` is 76 instructions, all of them right, and 27 of them differ - all
 * nine byte stores. Retail spells one store
 *
 *     lwz r0,0(r3) ; lbz r6,byte ; add r5,r3,r0 ; stb r6,4(r5)
 *
 * and mwcceppc spells the same store
 *
 *     lwz r5,0(r3) ; lbz r6,byte ; addi r0,r5,4 ; stbx r6,r3,r0
 *
 * - it folds the constant +4 into the index and uses the indexed store instead of materialising
 * `this + count` and carrying +4 in the displacement. 73 spellings of the loop body did not move
 * it (listed in that file's header). This file is the measurement that shows the fold belongs to
 * the 8-wide loop unroller and not to the source: **`p01` is the shipped body verbatim and it
 * folds; `p02` is the same two statements with no loop and it does not.**
 *
 * HOW TO RUN IT (not part of any build; compile it by hand and read the disassembly):
 *
 *   mkdir -p build/probe && <toolchain>/build/tools/wibo build/tools/sjiswrap.exe \
 *     <toolchain>/build/compilers/GC/2.7/mwcceppc.exe -nodefaults -proc gekko -align powerpc \
 *     -enum int -fp hardware -Cpp_exceptions off -O4,p -inline auto -pragma "cats off" \
 *     -pragma "warn_notinlined off" -maxerrors 1 -nosyspath -RTTI off -fp_contract on -str reuse \
 *     -i include -i libc -i build/G2ME01/include -DBUILD_VERSION=0 -DVERSION_G2ME01 -multibyte \
 *     -DNDEBUG=1 -use_lmw_stmw on -str reuse,pool,readonly -gccinc -inline deferred,noauto \
 *     -common on -lang=c++ -c tools/probe_unroll_store_form.cpp -o build/probe/
 *   build/binutils/powerpc-eabi-objdump -d build/probe/probe_unroll_store_form.o
 *
 * `MP_TOOLCHAIN_DIR` is the sibling `MetroidPrimePort` tree; the flags are the ones
 * `configure.py` gives the DOL's `MetroidPrime` library, and they are load-bearing - see the last
 * case. Count the `stb ` and `stbx` lines per function:
 *
 *   p01 76-trip loop, member subscript        stbx=9  stb=0   <- what the unit ships
 *   p02 same two statements, NO loop          stbx=0  stb=1   <- retail's form
 *   p03 4-trip loop, same body                 stbx=0  stb=4   <- fully unrolled, no remainder
 *   p04 12-trip loop, same body                stbx=6  stb=0
 *   p05 trip 76, byte hoisted into a local     stbx=9  stb=0
 *   p06 trip 76, count through a u32*          stbx=9  stb=0
 *   p07 trip 76, buffer through a nested struct stbx=9 stb=0
 *   p08 trip 76, `void` return                 stbx=9  stb=0
 *
 * p02 and p03 are the finding: the *identical* statements give retail's `add`+`stb` as soon as
 * the loop is not one mwcceppc unrolls eight-wide with a remainder, and give the fold as soon as
 * it is. `-O2` and `-O4,p` at trip 4 both keep `stb`; plain `-O4` does not unroll and folds - so
 * the optimisation level is not the lever either.
 */
#include "types.h"

struct A {
  u32 n;
  u8 buf[76];
};

extern "C" unsigned char g_byte;

/* p01: the shipped body. stbx=9 */
extern "C" void p01(A* s) {
  s->n = 0;
  for (int i = 0; i < 76; ++i) {
    s->buf[s->n] = g_byte;
    s->n = s->n + 1;
  }
}

/* p02: the same two statements, once. stb=1 - this is retail's store form. */
extern "C" void p02(A* s) {
  s->n = 0;
  s->buf[s->n] = g_byte;
  s->n = s->n + 1;
}

/* p03: a 4-trip loop, which mwcceppc unrolls completely with no remainder. stb=4 */
extern "C" void p03(A* s) {
  s->n = 0;
  for (int i = 0; i < 4; ++i) {
    s->buf[s->n] = g_byte;
    s->n = s->n + 1;
  }
}

/* p04: a 12-trip loop, unrolled with a remainder. stbx=6 */
extern "C" void p04(A* s) {
  s->n = 0;
  for (int i = 0; i < 12; ++i) {
    s->buf[s->n] = g_byte;
    s->n = s->n + 1;
  }
}

/* p05: the byte hoisted into a local. stbx=9 */
extern "C" void p05(A* s) {
  s->n = 0;
  for (int i = 0; i < 76; ++i) {
    u8 b = g_byte;
    s->buf[s->n] = b;
    s->n = s->n + 1;
  }
}

/* p06: the count through a pointer. stbx=9 */
extern "C" void p06(A* s) {
  u32* n = &s->n;
  *n = 0;
  for (int i = 0; i < 76; ++i) {
    s->buf[*n] = g_byte;
    *n = *n + 1;
  }
}

/* p07: the buffer as a nested struct's member, i.e. the +4 as a second offset. stbx=9 */
extern "C" void p07(A* s) {
  s->n = 0;
  for (int i = 0; i < 76; ++i) {
    reinterpret_cast< unsigned char* >(s)[4 + s->n] = g_byte;
    s->n = s->n + 1;
  }
}

/* p08: void rather than returning the pointer. stbx=9 */
extern "C" void p08(A* s) {
  s->n = 0;
  for (int i = 0; i < 76; ++i) {
    s->buf[s->n] = g_byte;
    s->n = s->n + 1;
  }
}
