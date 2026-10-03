// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:6865-6867`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_8019B988_text.s`, and the body below is the C
// those bytes are the compilation of.
//
// .text 0x8019C394..0x8019C40C, 0x78 = 120 bytes, 2 functions:
//
//   fn_8019C394  0x8019C394  0x50 = 80 bytes   20 instructions
//   fn_8019C3E4  0x8019C3E4  0x28 = 40 bytes   10 instructions
//
// **Both are byte-shape twins of matched code elsewhere in this tree, apart from the call
// targets and the `.sdata2` addresses, and both are reproduced instruction for instruction:**
//
//   fn_8019C394  is `CBSLocomotion::ComputeWeightPercentage`
//                (`src/MetroidPrime/BodyState/CBSLocomotion.cpp:136-144`, retail
//                `0x800F4864`, 0x50 bytes) with the data addresses retargeted.  The two are
//                identical instruction for instruction - same `lfs` order
//                (`0x4(r5)` then `0x4(r4)`), same `fsubs f4,f2,f3`, same `fcmpo cr0,f4,f0` /
//                `ble`, same `fcmpo cr0,f2,f0` / `bge` / `b` / `fmr f2,f0` clamp shape, same
//                `fcmpo cr0,f2,f1` / `bltlr` tail.
//   fn_8019C3E4  is `CBSLocomotion::Shutdown(CBodyController&)`
//                (`src/MetroidPrime/BodyState/CBSLocomotion.cpp:73`, retail `0x800F4C28`, 0x28
//                bytes) with `bl MultiplyPlaybackRate__15CBodyControllerFf` retargeted to
//                `fn_8019AB18`.  Retail's own order is reproduced: the `lfs` of the rate sits
//                between `mflr r0` and `stw r0,0x14(r1)`, and the `mr r3,r4` sits between the
//                `lfs` and the `stw`.
//
// **Both copies are shifted one integer register against their twins, and both take the leading
// unused parameter that shift explains.**  The twins are members, so their `this` sits in `r3`
// and their explicit arguments start at `r4`; these copies read `a` out of `r4` and `b` out of
// `r5` and never touch `r3`, and `fn_8019C3E4` forwards `r4` - not `r3` - as the callee's
// `this`.  Both are therefore written with an unused first pointer parameter, which is what puts
// the loads on `r4`/`r5` and emits the `mr r3,r4`; measured, the shorter spelling puts them on
// `r3`/`r4` and comes out with no `mr` at all.  `fn_8019C394`'s float is unaffected either way -
// `f1` is the only float register either way, and it is `velocity`.
//
// The three `.sdata2` words are the ones retail loads, read out of `build/G2ME01/main.elf`
// (`.sdata2` VMA 0x8041A3C0, file offset 0x3C3C20): `lbl_8041CB6C` = `3F800000` = 1.0f,
// `lbl_8041CB70` = `00000000` = 0.0f, `lbl_8041CB80` = `34000000` = 1.1920929e-07 =
// `FLT_EPSILON` - the same three values the twin pools at `lbl_8041B8D4`, `lbl_8041B8D0` and
// `lbl_8041B8E8`.  They are `extern` rather than literals because they live in
// `auto_11_8041CB40_sdata2.o`, a retail split object that is already in the link; `CAi.cpp`'s
// `extern const float kCAiSplashDenom` is the same arrangement and that unit is `Matching`.
// A literal compiles to the same instruction order in `fn_8019C3E4` - measured - but it would
// make this unit pool its own copy, and no `.sdata2` range in `splits.txt` claims this unit, so
// that pool would land somewhere inside retail's `.sdata2` and move it.  The `extern` spelling
// keeps the section out of the object altogether.
//
// `fn_8019AB18` (0x8019AB18, 0x28) is `CBodyController::MultiplyPlaybackRate(float)` - it takes
// `this` in `r3` and the rate in `f1` and forwards to `CAnimData::MultiplyPlaybackRate(float)` -
// and it is defined in `auto_03_801997B4_text`, so declaring it extern does not open a link gap.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp` (`tools/project.py:1020` gives a `.c` unit `-lang=c`).
//
// The claim ends exactly at `fn_8019C40C`, which `symbols.txt:6867` gives a size of 0x58, so
// nothing above the item's two functions is taken.
//
// Its own unit because a claim may not span unclaimed bytes: below this range the run
// 0x8019B988..0x8019C394 is still dtk's `auto_03_8019B988_text`, and above it
// 0x8019C40C..0x8019CE6C is the rest of the same unclaimed run, whose far end
// `MetroidPrime/Carve8019CE6C.c` already owns.
//
// The directory is retail's own, taken from the nearest claimed ranges: `Carve801997B0.c`,
// `Carve8019AC78.c` and `Carve8019CE6C.c` bracket this address and all three are in
// `MetroidPrime/`.

/** `1.0f` - `lbl_8041CB6C`, `.sdata2:0x8041CB6C`, value `3F800000`.  Retail loads it into `f1`
 *  in `fn_8019C3E4` (the playback rate) and into `f2` in `fn_8019C394` (the upper clamp).
 *  **`const` is load-bearing.**  Retail loads the rate between `mflr r0` and `stw r0,0x14(r1)`,
 *  ahead of the `mr r3,r4`; an unqualified `extern float` puts the load *after* the frame store
 *  instead (measured - same ten instructions, one slot out).  A `const` object is a value mwcceppc
 *  can schedule, a mutable one is a load it sinks to the call. */
extern const float lbl_8041CB6C;
/** `0.0f` - `lbl_8041CB70`, `.sdata2:0x8041CB70`, value `00000000`.  Both functions return it,
 *  and `fn_8019C394` loads it into `f2` only in the `range > FLT_EPSILON` path. */
extern const float lbl_8041CB70;
/** `FLT_EPSILON` - `lbl_8041CB80`, `.sdata2:0x8041CB80`, value `34000000`.  `fn_8019C394`
 *  compares the range against it before dividing. */
extern const float lbl_8041CB80;

/** `fn_8019AB18` - retail `.text:0x8019AB18`, 0x28 bytes: `CBodyController::MultiplyPlaybackRate`
 *  (float), which loads two vtable words and forwards to `CAnimData::MultiplyPlaybackRate(float)`.
 *  Declared here because `fn_8019C3E4` calls it; it is defined in dtk's
 *  `auto_03_801997B4_text`, so this adds nothing to the port's link gap. */
extern void fn_8019AB18(void* self, float rate);

/* The port's link is the half of this unit that `tools/link_check.sh` gates, and it has neither
 * of the two things the DOL build gets from elsewhere: the three `.sdata2` words live in dtk's
 * `auto_11_8041CB40_sdata2.o` and `fn_8019AB18` in `auto_03_801997B4_text`, both *retail* split
 * objects that only `mwldeppc` links.  A host build asks for four symbols nothing defines, and
 * `tools/probe_sources.sh --strict` fails when that count grows - measured 291 against the
 * recorded 287 before this block, all four named here.  So the host definitions go here, next to
 * the declarations they complete, and not in `PortLinkStubs.cpp`: that file is generated
 * (`tools/gen_link_stubs.py`), and `docs/research/boot_path_stubbable.tsv` is the input a
 * regeneration would need.  This is the same "a claim may not open a link gap" argument
 * `src/MetroidPrime/Carve8019AC78.c:37-51` makes for taking `fn_8019ACC0` with it.
 *
 * The three floats are not stubs: they are retail's own measured words, spelled so the compiler
 * folds them to the exact bit patterns read out of `main.elf` (`0x1p-23f` rather than a decimal
 * `FLT_EPSILON`, which is the same value but leaves the bit pattern to the rounding).  Only
 * `fn_8019AB18` is a stand-in, because its body is two vtable loads and a forward - there is no
 * honest way to run that on the host - and like every stand-in in this port it logs its own name
 * on entry rather than returning quietly. */
#ifdef TARGET_PC
#include <stdio.h>
const float lbl_8041CB6C = 1.f;             /* 0x3F800000 */
const float lbl_8041CB70 = 0.f;             /* 0x00000000 */
const float lbl_8041CB80 = 0x1p-23f;        /* 0x34000000, FLT_EPSILON */
void fn_8019AB18(void* self, float rate) {
  printf("[port-stub] fn_8019AB18 rate=%f\n", (double)rate);
}
#endif

/** The 4-byte-aligned `{int, float}` pair both `0x4(r4)` / `0x4(r5)` loads address.  Retail
 *  reads only `second`, at +0x04, so `first` is declared only to hold the offset there. */
struct SPair8019C394 {
  int first;
  float second;
};

/** `fn_8019C3E4` - retail `.text:0x8019C3E4`, 0x28 = 40 bytes, 10 instructions.  Byte-shape
 *  twin of `CBSLocomotion::Shutdown(CBodyController&)` (0x800F4C28, 0x28 bytes): one `1.0f`
 *  load, the `this` pointer moved from `r4` into `r3`, one call, and the frame.
 *  **The `mr r3,r4` is why this takes two pointers and not one.**  Retail reads the callee's
 *  `this` out of `r4`, so the argument it forwards is the *second* parameter and the first is
 *  never touched; the one-parameter spelling compiles to the same call without the `mr` and comes
 *  out 4 bytes short.  The first parameter is the unused `this` of the twin's member function. */
void fn_8019C3E4(void* unusedThis, void* bodyController);

/** `fn_8019C394` - retail `.text:0x8019C394`, 0x50 = 80 bytes, 20 instructions.  Byte-shape
 *  twin of `CBSLocomotion::ComputeWeightPercentage` (0x800F4864, 0x50 bytes), which is matched in
 *  `src/MetroidPrime/BodyState/CBSLocomotion.cpp:136-144`.  The leading parameter is unused and
 *  is what puts `a` in `r4` and `b` in `r5` rather than `r3`/`r4`; the two `rstl::min_val` /
 *  `rstl::max_val` calls are spelled the way `include/rstl/math.hpp:7-14` spells them -
 *  `min_val(a,b)` is `(b < a) ? b : a` and `max_val(a,b)` is `(a < b) ? b : a` - because the
 *  order of the `fcmpo` operands is what makes retail load the upper clamp into `f2` and keep the
 *  lower one in `f1`, and the tail branch is `bltlr` returning the lower clamp straight out of
 *  `f1` instead of moving it. */
float fn_8019C394(void* unusedThis, const struct SPair8019C394* a,
                  const struct SPair8019C394* b, float velocity);

void fn_8019C3E4(void* unusedThis, void* bodyController) {
  fn_8019AB18(bodyController, lbl_8041CB6C);
}

float fn_8019C394(void* unusedThis, const struct SPair8019C394* a,
                  const struct SPair8019C394* b, float velocity) {
  const float range = b->second - a->second;
  if (range > lbl_8041CB80) {
    const float pct = (velocity - a->second) / range;
    const float high = (lbl_8041CB6C < pct) ? lbl_8041CB6C : pct;
    return (high < lbl_8041CB70) ? lbl_8041CB70 : high;
  }
  return lbl_8041CB70;
}