/* Carve target: `fn_800069AC`, retail 0x800069AC, 0x134 = 308 bytes.
 *
 * PROVENANCE AND STATUS - read this before wiring the unit up.
 *
 * `fn_800069AC` is **not** an unclaimed dtk `auto_*` range.  It sits at 0x800069AC, which is
 * inside `MetroidPrime/main.cpp`'s `.text` claim of 0x800053B8-0x8000848C
 * (`config/G2ME01/splits.txt`), and it is the same function `tools/fnmap.py MetroidPrime/main`
 * lists as "retail 308, ours nothing".  So this file cannot be given a `splits.txt` range of
 * its own without cutting `main.cpp`'s claim in two - one range per unit per section, or
 * `dtk dol split` fails with a link-order cycle - and that cut is the `mainsplit` lane's, not
 * this one's (`src/MetroidPrime/main.cpp` is not this lane's file either).
 *
 * It **is** in `files.cmake` - the port compiles it, so it is not a dead file and its host
 * build is checked - and it is **not** in `configure.py`, so objdiff does not measure it yet
 * and `unit_fit`/`flip_test` have nothing to say until the claim lands.  It is not in EXCLUDED
 * either: it is built, just not claimed.  The three lines the claim still needs, once that cut
 * has been agreed, are:
 *
 *   config/G2ME01/splits.txt, in address order between main.cpp and whatever takes the tail:
 *     MetroidPrime/Carve800069AC.c:
 *         .text       start:0x800069AC end:0x80006AE0
 *   configure.py, as one line, next to the other carves:
 *     Object(Matching, "MetroidPrime/Carve800069AC.c"),
 *
 * and a `NonMatching` there instead, until the `+4` below is placed where retail puts it.  A
 * hand-written claim copied out of another tree is not a substitute for any of this.
 *
 * It compiles, and the shape is right; the measurement is:
 *
 *     ./tools/probe_c.sh src/MetroidPrime/Carve800069AC.c build/carve/o.o
 *     ./tools/carve_diff.sh 0x800069AC 0x134 build/carve/o.o
 *     retail: 77 instructions, 308 bytes
 *     ours  : 76 instructions, 304 bytes
 *     differing instructions: 62
 *
 * `tools/probe_c.sh` is `tools/probe_cc.sh` with `-lang=c`, which is what a `.c` unit gets
 * (`build.ninja` appends it to the `.c` rule's cflags).  Without it the compiler runs in C++
 * mode and mangles the symbol to `fn_800069AC__FP17SFrameTimeHistoryPCf`, which objdiff cannot
 * pair with retail's `fn_800069AC` and the unit would silently score 0/0 with a right file.
 * That is also why this is a `.c` and not a `.cpp`.
 *
 * WHAT IS STILL WRONG, and it is one decision, not the body:
 *
 * The first 15 instructions are byte-identical to retail.  Then retail materialises the
 * destination pointer with one extra instruction,
 *
 *     retail  slwi r0,r9,2 ; mr r5,r9 ; add r8,r3,r0 ; addi r8,r8,4
 *     ours    slwi r0,r9,2 ; mr r5,r9 ; add r8,r3,r0
 *
 * so retail's `r8` is `&values[i]` (the `+4` is the member offset folded into the register) and
 * its stores are `0(r8), -4(r8), -8(r8)`, while ours keeps the `+4` as a store displacement and
 * stores at `4(r8), 0(r8), -4(r8)`.  The same choice on the load side makes ours emit
 * `slwi r6,r7,2 ; addi r0,r6,4 ; lfsx f0,r3,r0` where retail emits
 * `slwi r0,r7,2 ; add r6,r3,r0 ; lfs f0,4(r6)`.  **Every one of the 62 differing instructions is
 * a consequence of that single `+4` placement** - the schedule, the register assignment
 * (`r5` keeping the trip count for `andi. r5,r5,7`, `r9` the unrolled induction variable, `r8`
 * the walking store pointer, `r7` the per-element index), the 8x unroll, the
 * `srwi. r0,r9,3 / mtctr / beq` split, the `andi. r5,r5,7` remainder, the 1x tail loop and the
 * `lfs f0,0(r4) ; stfs f0,4(r3) ; blr` epilogue all match retail's.
 *
 * The `j = i - 1` temporary is what produces that register split at all: without it mwcceppc
 * strength-reduces the load into a walking pointer and the object is 184 bytes / 46
 * instructions (variant `A_baseline` in the batch below).  Forty source spellings and six flag
 * sets were measured (`tools/try_carve.py`); 304 bytes is the plateau and nothing reached 308.
 * The list is kept so the next lane does not repeat it:
 *
 *   plain `for (i = h->count-1; i > 0; i--) h->values[i] = h->values[i-1];`   184 B / 46 insn
 *   plus a `float* v = h->values` local                                      184 B
 *   local `n` for the count                                                  176 B
 *   `for (i = 1; i < n; i++) v[n-i] = v[n-i-1];`                             336 B
 *   `do { } while (i > 0)` / `while (--i > 0)`                                 92 B
 *   `float* d` and `const float* s` as separate bases                        184 B
 *   a `static` helper called with `(h, n)`                                     244 B
 *   pointer arithmetic, `*(&h->values[i])`, `float (*v)[4] = &h->values`,
 *   a `union`, a `float` temporary, `register int`, two induction variables,
 *   `i >= 1` instead of `i > 0`, `--i` in the for-header: all 184-336 B, none 308
 *   `-pragma "inline_max_size(125)"`, `-pragma "unroll_factor(8)"`,
 *   `-pragma "unroll loops on"`, `-O4,-s,speed`: all still 304 B - the flags are not it
 *
 * So this is a **compiler-wall result, not a decompilation wall**: the algorithm, the
 * statement order and the register assignment are all reproduced, and the four bytes that are
 * missing are one peephole's choice of where to put a constant.  Per this project's rule, a
 * `Matching` unit has to be byte-exact, so this body is `NonMatching` until someone finds the
 * spelling that moves the `+4`.
 *
 * What the 308 bytes are, entirely (`tools/dis.sh 0x800069AC 0x134`):
 *
 *     r3 -> SFrameTimeHistory*      r4 -> const float*
 *
 *     if (h->count < 4) { h->values[h->count] = *src; h->count++; }   // 0x800069AC..0x800069D0
 *     for (i = h->count - 1; i > 0; i--) h->values[i] = h->values[i - 1];
 *                                                                   // 0x800069D4..0x80006AD0
 *     h->values[0] = *src;                                           // 0x80006AD4..0x80006AD8
 *
 * **It does not sort.**  There is no `fcmpo`, no `fcmpu` and no branch on a float comparison
 * anywhere in the 308 bytes - the loop is a pure shift, unrolled eight times by mwcc (the
 * `srwi. r0,r9,3 ; mtctr r0` at 0x800069F0 is the trip count for eight elements, and the
 * `andi. r5,r5,7` at 0x80006AA8 is its remainder).  `docs/research/boot_path.md` row 10 called
 * it "a bounded, insertion-sorted float push"; the bytes say a newest-first ring history with
 * `values[0]` the most recent sample.  Corrected here rather than propagated.
 *
 * Its two callers in `CMain::RsMain` are what fixes the meaning of the fields:
 *   0x80006108 - pushes the pre-render half-frame time into `CMain`+0x18, and 0x80006114 hands
 *                `CMain`+0x18 to `fn_80006954`, whose sum lands at `CMain`+0x40 (0x80006120).
 *   0x80006228 - the same for `CMain`+0x2C -> `CMain`+0x44 (0x8000623C).
 * See `include/MetroidPrime/CMain.hpp`'s private section for the field-by-field evidence.
 */

/* A local duplicate of `CMain::SFrameTimeHistory`, and deliberately so.  This is a C
 * translation unit, so it cannot include the C++ header, and `CMain` is not a C++ class as far
 * as this file is concerned - the object arrives as a pointer.  Spelling the shape here makes
 * the unit immune to anything that happens in `CMain.hpp` later, the same property
 * `src/MetroidPrime/Player/CModelDataModelSlots.cpp` relies on.  `tools/sizeprobe_cmain.cpp`
 * measures `sizeof(CMain::SFrameTimeHistory)` as 0x14 = 20 with the same compiler, and the
 * member offsets retail writes to are +0x00 (the count) and +0x04..+0x10 (the four floats). */
struct SFrameTimeHistory {
  int count;
  float values[4];
};

void fn_800069AC(struct SFrameTimeHistory* h, const float* src)
{
  int i;
  int j;

  if (h->count < 4) {
    h->values[h->count] = *src;
    h->count++;
  }

  for (i = h->count - 1; i > 0; i--) {
    j = i - 1;
    h->values[i] = h->values[j];
  }

  h->values[0] = *src;
}
