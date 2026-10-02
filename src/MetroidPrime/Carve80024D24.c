// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:639-640`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80024C18_text.s:91-113` before the claim existed (that run
// is now two files, `auto_03_80024C18_text.s` and `auto_03_80024D68_text.s`), and the body below is
// the C those bytes are the compilation of.
//
// .text 0x80024D24..0x80024D68, 0x44 = 68 bytes, 2 functions:
//
//   fn_80024D24    0x80024D24  0x20     8 instructions   `rstl::destroy< CFontImageDef >(T*)`
//   fn_80024D44    0x80024D44  0x24     9 instructions   `rstl::destroy_impl< CFontImageDef >(T*)`
//
// **What the two are: `CFontImageDef`'s element-destruction pair.**  The unclaimed function
// directly below them, `fn_80024CD4` (0x80024CD4, 0x50), is the element walk that calls this file's
// `fn_80024D24` once per element, stepping its cursor by **0x1c** (`addi r31,r31,0x1c`) -
// `sizeof(CFontImageDef)`, which `include/Kyoto/Text/CFontImageDef.hpp:41` pins as
// `CHECK_SIZEOF(CFontImageDef, 0x1c)` - so the element is `CFontImageDef` and the pair is
// `rstl::destroy` over `rstl::destroy_impl`.  Each function is read off its call edge and its
// argument registers:
//
//   fn_80024D44  materialises `li r4,-1` and calls `__dt__13CFontImageDefFv` (0x80024D68,
//                `symbols.txt:641`, 0x58, `weak`) with the receiver untouched, taking nothing else
//                from its own argument - that is `destroy_impl`'s `in->~T()`
//                (`include/rstl/construct.hpp:84-90`), and the -1 is MWCC's "destroy, do not free
//                me afterwards" flag, which the callee's own `extsh. r0,r31` proves is 16-bit, so
//                the callee is the element's *deleting* destructor.  **The twin is exact**:
//                `fn_80004C6C` (0x80004C6C, 0x24) in `src/MetroidPrime/Player/Carve80004C4C.c`, a
//                `Matching` unit, is these nine instructions word for word with `bl fn_80004A4C`
//                where the `bl` here is, and `destroy_impl< 11CTweakValue >__4rstlFP11CTweakValue`
//                (0x80006850, 0x24) is a third copy of the same nine.  Retail's instruction order
//                is reproduced too: `li r4,-1` sits between `mflr r0` and the `stw r0,0x14(r1)`.
//   fn_80024D24  is a frame and one unconditional `bl fn_80024D44` - no load, no test, nothing else
//                taken from its arguments - so it is `destroy`'s `destroy_impl(in)`
//                (`include/rstl/construct.hpp:92-95`).  **Its twin is `fn_80004C4C`** (0x80004C4C,
//                0x20, the same file), the same 8 instructions with the `bl` retargeted, and the
//                item pairs it with `fn_80004D3C` (0x80004D3C, 0x20), which is that shape again;
//                `__sys_free` (0x80008A28, `src/MetroidPrime/main.cpp`) is the fourth copy this
//                repo already documents.
//
// **Why the claim is exactly these two.**  The next symbol, at 0x80024D68, is the element's
// real-named `weak` destructor `__dt__13CFontImageDefFv` - it is not part of this pair and would
// need a mangled definition in a `.cpp` of its own.  Below 0x80024D24, the run
// 0x80024C18..0x80024D24 is `fn_80024C18`, `fn_80024C9C` and `fn_80024CD4`, none of which is
// written (`fn_80024C18` is the recorded `WALL` of `carve-80024b70`), and a claim may not span
// unclaimed bytes.  So 0x80024D24..0x80024D68 is the contiguous run that is both written and
// byte-exact.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`, and why the `.c` is compiled as C by the port's host build (where C++ would mangle
// it) and syntax-checked as C++ by `tools/probe_sources.sh`; both accept this file as written.
//
// Its own unit because a claim may not overlap unclaimed bytes: below this range, 0x80024C18..0x80024D24
// is still dtk's `auto_03_80024C18_text`, and above it 0x80024D68..0x80025D3C is the tail of the
// same unclaimed run.
//
// The directory is retail's own, taken from the nearest claimed range below:
// `MetroidPrime/Carve80024B70.c` (0x80024B70..0x80024C18) sits in `MetroidPrime/`, so this address
// is in the `MetroidPrime/` neighbourhood.

/** 0x80024D68, `symbols.txt:641`, 0x58 = 88 bytes: `CFontImageDef`'s deleting destructor, the
 *  callee of `fn_80024D44` below - it destroys the texture vector at `+0x04` (`addi r3,r30,4`,
 *  `include/Kyoto/Text/CFontImageDef.hpp:37`) and then frees the receiver if the 16-bit flag is
 *  positive.  It is unclaimed by any unit, so dtk's own `auto_*` object supplies its bytes in the
 *  DOL link; this file only declares it. */
extern void __dt__13CFontImageDefFv(void* self, short flag);

/** `fn_80024D44` - retail `.text:0x80024D44`, 0x24 = 36 bytes: `rstl::destroy_impl< CFontImageDef >`,
 *  i.e. the element's destructor called with the "do not free me" flag.  The twin is `fn_80004C6C`
 *  (0x80004C6C, 0x24), these nine instructions word for word with `bl fn_80004A4C` in place of the
 *  `bl` here. */
void fn_80024D44(void* self);

void fn_80024D44(void* self) { __dt__13CFontImageDefFv(self, -1); }

/** `fn_80024D24` - retail `.text:0x80024D24`, 0x20 = 32 bytes: `rstl::destroy< CFontImageDef >`, a
 *  frame and one call to `fn_80024D44` above and nothing else, as
 *  `include/rstl/construct.hpp:92-95` spells it.  Its measured twin is `fn_80004C4C` (0x80004C4C,
 *  0x20), byte for byte apart from the `bl` target. */
void fn_80024D24(void* self);

void fn_80024D24(void* self) { fn_80024D44(self); }
