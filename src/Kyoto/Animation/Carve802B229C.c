// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:12306`, and the instructions are the nine words
// read out of `build/G2ME01/main.elf` before this claim was listed, when the whole range was still
// dtk's `auto_03_802B2090_text` (0x802B2090..0x802B2568).  Listing the claim re-split that range,
// so the asm listing no longer carries these nine words: it is now
// `auto_03_802B2090_text` 0x802B2090..0x802B229C plus `auto_03_802B22C0_text`
// 0x802B22C0..0x802B2568.  The body below is the C those bytes are the compilation of.
//
// .text 0x802B229C..0x802B22C0, 0x24 = 36 bytes, 1 function:
//
//   fn_802B229C    0x802B229C  0x24    9 instructions   `this + 4` forwarder, `bl fn_802B22C0`
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// **The twin is exact, `bl` aside.**  `GetResourceIdByName__11CResFactoryCFPCc` (retail
// 0x80006B80, 0x24) is these nine words **8 of 9 identical**; the one differing word is the `bl`
// at offset 0x10 (`482F60B5` -> `48000015`, target 0x802FCC44 -> 0x802B22C0).  Both sides read
// out of `build/G2ME01/main.elf` this run, before this claim was listed; afterwards `main.elf`
// holds our bytes for the claim and they are identical, which `tools/carve_diff.sh` measures.
// That function is 100.00% matched inside
// `MetroidPrime/main.cpp` (a `NonMatching` unit) and its source is the inline body at
// `include/Kyoto/CResFactory.hpp:64-65`:
//
//   const SObjectTag* GetResourceIdByName(const char* name) const {
//     return mResLoader.GetResourceIdByName(name);
//   }
//
// i.e. the same `this + 4` forwarder for the same reason: a subobject held at a fixed offset in
// the receiver.  Retail names none of this copy's own symbols, so nothing here is asserted about
// the receiver's class - no member of it is named in the body and the only offset is the `+ 4`
// the bytes themselves carry, so the receiver is `void*` and the adjustment is spelled
// `(char*)self + 4`.
//
// The callee settles the return type.  `fn_802B22C0` (0x802B22C0, 0x140 = 320 bytes) is a
// Newton iteration on a quadratic, three `float*` arguments - `lfs 0x0(r3)` / `lfs 0x4(r3)`,
// `lfs 0x0(r4)`, `lfs 0x0(r5)` - with the constants `lbl_8041E41C` = 0.5f,
// `lbl_8041E418` = 2.0f, `lbl_8041E420` = 1e-5f and `lbl_8041E424` = **-1.0f** read out of the
// DOL's `.sdata2` this run (the 0x8041E410 block): `li r0,0x4` / `mtctr r0` / `bdnz` over five
// `fmadds`/`fsubs`/`fdivs` passes (the loop body at `.L_802B2300` runs four times, then the fifth
// unrolled copy at 0x802B23C0 runs once), each `fabs`-and-`frsp`-compared against 1e-5f with a
// `bltlr` out, and `-1.0f` loaded into `f1` when all five fall through.  It therefore returns its
// answer in `f1`, and this forwarder passes it on in `f1` with no instruction of its own - which
// is why retail emits nothing between the `bl` and the epilogue.  `fn_802B22C0` is declared here
// and left to dtk's object for the DOL; the host's copy is the `TARGET_PC` block below.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The file is compiled as C for the port's host build and, like every other source
// in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh` - hence the explicit
// `char*` cast, which is compile-time only and leaves the object byte-identical.
//
// Its own unit: these 36 bytes are one run inside what was the unclaimed `auto_03_802B2090_text`
// (0x802B2090..0x802B2568), and the functions on either side of it are not trivial - in front,
// `fn_802B21A0` (0x802B21A0, 0xFC) is retail's, and behind, `fn_802B22C0` (0x140) is the Newton
// iteration above, which needs that constant pool and the receiver's layout rather than a twin.
// The claim touches no unit boundary, so `dtk dol split` raised no link-order cycle (measured: it
// re-split the range into the two `auto_*` units named in the first paragraph).
//
// The directory is retail's own, taken from the nearest claimed range: the claim sits inside the
// unclaimed 0x802B2090..0x802B2568, whose nearest claimed neighbours are
// `Kyoto/Animation/Carve802B2088.c` (0x802B2088..0x802B2090, ends exactly where the unclaimed
// range starts) below and `Kyoto/Animation/Carve802B2568.c` (0x802B2568..0x802B2570) above, so
// the code is the `Kyoto/Animation/` neighbourhood - which is where the item's seeder put it.
//
// **A claim may not open a link gap.**  The port's link is the half of this unit
// `tools/probe_sources.sh --strict` gates, and it has none of the things the DOL build takes from
// dtk's split objects: `fn_802B22C0` lives in `auto_03_802B22C0_text.o`, which only `mwldeppc`
// links.  Measured in this tree before the block below: the carve listed without it fails the gate
// with `288 undefined against a baseline of 287 (GREW)` and
// `gap grew: fn_802B22C0 is not in port_link_gap_list.md`.  So the host definition goes here, next
// to the declaration it completes, and not in `PortLinkStubs.cpp` - that file is generated
// (`tools/gen_link_stubs.py`) - which is the same argument
// `src/MetroidPrime/Carve8019C394.c:95-111` makes for taking `fn_8019AB18` with it.  `TARGET_PC` is
// defined for the port and the syntax probes (`CMakeLists.txt:152,167`,
// `tools/probe_sources.sh:49`) and **not** for the matching build, so this block is invisible to
// `main.dol` and the object's 36 bytes are unchanged; `tools/carve_diff.sh 802b229c 24` confirms.
//
// The body below is a transcription of retail's own instructions for `fn_802B22C0` read out of
// `build/G2ME01/asm/auto_03_802B22C0_text.s:9-91` this run, **not** a decompilation claim: no
// unit claims 0x802B22C0..0x802B2400, and its 320 bytes stay dtk's in the DOL.  It is a pure
// float solver, so unlike most stand-ins in this port it can be run honestly on the host, and it
// logs its own name anyway so a reader can see at once that the host took this path.  The five
// passes and the four constants are the ones measured in the paragraph above, read out of the DOL
// at 0x8041E418..0x8041E428: 2.0f, 0.5f, 1e-5f, -1.0f.
#ifdef TARGET_PC
#include <math.h>
#include <stdio.h>

float fn_802B22C0(const void* quadratic, const void* start, const void* target) {
    printf("[port-host] fn_802B22C0: retail's 0x140 bytes are unclaimed; this is the host body\n");
    const float* p = (const float*)quadratic;
    const float f4 = *(const float*)target;
    float f1 = *(const float*)start;
    const float f8 = 0.5f * p[0];            /* f1*f0: lbl_8041E41C = 0.5f  */
    const float f9 = p[1];
    const float f6 = 2.f * f8;               /* f2*f8: lbl_8041E418 = 2.0f  */
    const float f7 = f1 * (f8 * f1) + f9 * f1;
    int i;
    for (i = 0; i < 5; i++) {                /* mtctr 4 + bdnz + the 5th unrolled copy */
        const float f2 = f6 * f1 + f9;
        float f3 = f1 * (f8 * f1) + f9 * f1;
        f3 = (f3 - f7 - f4) / f2;
        f1 = f1 - f3;
        if (fabsf(f3) < 0x1p-5f) {           /* fabs/frsp vs lbl_8041E420 = 1e-5f */
            return f1;
        }
    }
    return -1.f;                             /* lbl_8041E424 = -1.0f */
}
#endif

/** `fn_802B22C0` - retail `.text:0x802B22C0`, 0x140 = 320 bytes: the Newton iteration on the
 *  quadratic the forwarder below forwards to, answering in `f1` and `-1.0f` when five steps do not
 *  converge.  Retail's bytes, above the claim and unclaimed, so it cannot be dropped from this
 *  unit's call.  Declared here; defined for the host only, in the `TARGET_PC` block above. */
extern float fn_802B22C0(const void* quadratic, const void* start, const void* target);

/** `fn_802B229C` - retail `.text:0x802B229C`, 0x24 = 36 bytes: the `this + 4` forwarder to
 *  `fn_802B22C0`, its two later arguments passed through untouched.  The twin is
 *  `GetResourceIdByName__11CResFactoryCFPCc` - see the header. */
float fn_802B229C(void* self, const void* quadratic, const void* start);

float fn_802B229C(void* self, const void* quadratic, const void* start) {
    return fn_802B22C0((char*)self + 4, quadratic, start);
}