// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt:10157-10158`, the instructions
// are the ones dtk itself emitted into
// `build/G2ME01/asm/auto_03_8023C998_text.s:2066-2092` while this range was still unclaimed,
// and the bodies below are the C those bytes are the compilation of.
//
// .text 0x8023E5A8..0x8023E5F4, 0x4C = 76 bytes, 2 functions:
//
//   fn_8023E5A8    0x8023E5A8  0x3C = 60 bytes  15 instructions  deleting-destructor shape
//   fn_8023E5E4    0x8023E5E4  0x10 = 16 bytes   4 instructions  zero two floats at +0/+4
//
// **What the two are.**  Retail names neither, so the twins identify them, and both twins are
// exact - same instructions, same operand schedule, only the `bl`/data displacement differs:
//
//   fn_8023E5A8  is `__dt__5CMainFv` (0x800087DC, 0x3C = 60 bytes,
//                `src/MetroidPrime/mainTail.cpp:135` `CMain::~CMain() {}`) word for word,
//                `bl Free__7CMemoryFPCv` included: the receiver is tested (`mr. r31,r3` /
//                `beq`), the flag is re-tested as a **short** (`extsh. r0,r4` / `ble` - a
//                `bool` would give `extsb.`) and `if (flag > 0)` sits **inside** `if (self)`,
//                so the receiver's `beq` lands on the epilogue.  That epilogue returns the
//                receiver in r3 (`mr r3,r31`), and that is what says the return is `void*`
//                and not `void`: as `void` mwcceppc drops the move and the body compiles to
//                14 instructions / 56 bytes, so the function is 12 bytes short and does not
//                fit the claim.  It is the **third** copy of this exact body in this tree -
//                `fn_8023C950` and `fn_8023C860` (`Carve8023C950.c`, `Carve8023C860.c`) are the
//                first two, and this file reproduces it a third time; the instructions are
//                identical, so the identifying is already done.
//   fn_8023E5E4  is `__ct__15CControllerAxisFv` (0x8030BF8C, 0x10 = 16 bytes, the inline
//                `CControllerAxis() : mRelative(0.f), mAbsolute(0.f) {}` at
//                `include/Kyoto/Input/CControllerAxis.hpp:6`) exactly:
//                `lfs f0, <zero> ; stfs f0, 0x0(r3) ; stfs f0, 0x4(r3) ; blr` - one load and
//                two stores, no frame.  The twin reads its zero from **this translation
//                unit's own literal pool** (`"@244"@sda21(r0)`, `build/G2ME01/asm/Kyoto/
//                Input/CDolphinController.s:798`); this copy reads it from a **named .sdata2
//                object**, `lbl_8041DCB0` (`symbols.txt:24608`, `.sdata2:0x8041DCB0`,
//                `data:float`), whose value dtk recorded as `.float 0`
//                (`build/G2ME01/asm/auto_11_8041DAC8_sdata2.s:567-570`).  Same value, same four
//                instructions; only the address differs, and both live in .sdata2 so the
//                `sda21` relocation is the same shape.  The offset is exactly retail's:
//                `_SDA2_BASE_` = 0x804223C0 (`tools/sda.py`) minus 0x8041DCB0 = 0x4710 =
//                -18192, which is the `b8f0` half of `c0 02 b8 f0`.
//
// **Who calls them, and with what.**  Every call site is in dtk's unclaimed
// `auto_03_8009D644_text`, so nothing of ours needs them and no count depends on them:
//
//   0x8009DABC and 0x8009DC14  `mr r3,r29 ; li r4,-1 ; bl fn_8023E5A8`, either side of
//                `addi r3,r1,0x88 ; li r4,-1 ; bl __dt__20SLdrEditorPropertiesFv` in
//                `fn_8009D644` (`symbols.txt:3106`, 0x5F8 = 1528 bytes).  `-1` is MWCC's
//                "destroy, do not free me afterwards", so the free-through-to-self is dead at
//                those two sites and is not dead in the source - the same reading
//                `Carve8023C950.c` records for its two sites.
//   0x8009DC60  `addi r3,r31,0x58 ; bl fn_8023E5E4` in `fn_8009DC3C`
//                (`symbols.txt:3107`, 0xAC = 172 bytes), immediately after
//                `bl __ct__20SLdrEditorPropertiesFv` on the **same** object and after
//                `stw r0,0x3c(r31)` with `li r0,-1`.  So this is a constructor's own
//                zero-initialisation of a two-float member at +0x58, not a load from data.
//
// **Its own unit because a claim may not span an unclaimed gap, and because the neighbours
// are not writable.**  The function below is `fn_8023E4E4` (`symbols.txt:10156`, 0xC4 = 196
// bytes) and the function above is `LoadTypedefPatternedAITypedef`
// (`build/G2ME01/asm/auto_03_8023C998_text.s:2094-2095`, 0x730 = 1840 bytes) - retail names
// that one, so it needs its own mangling and a `.cpp` and cannot be claimed by a `.c` anyway.
// Neither is twin-shaped, so 0x8023E5A8..0x8023E5F4 is the whole writable run and this unit
// claims exactly it.  The range is inside dtk's `auto_03_8023C998_text`; the nearest claimed
// boundaries are `Carve8023C950.c` ending at 0x8023C998 and `ScriptLoader.cpp` starting at
// 0x80242894, and this claim starts 0x1B10 bytes below that upper boundary, so nothing here
// is adjacent to a unit boundary (which is what creates a dtk link-order cycle).
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function, `unit_fit.sh` still saying
// "fits" and a broken DOL.  Only `tools/flip_test.sh` catches that.  `fn_8023E5E4`
// (0x8023E5E4) is the *higher* address and therefore comes first here; the object then leads
// with `fn_8023E5A8` at offset 0, which is where retail has it.
//
// Retail names none of these, so `symbols.txt:10157-10158` carries the `fn_<addr>`
// placeholders and this file reproduces those symbols verbatim, which is why the definitions
// have to stay C: a C++ one would mangle to `_Z<len>fn_<addr>v` and objdiff would pair
// nothing.  That is also why the unit is a `.c` rather than a `.cpp`.
//
// The directory is retail's own, taken from the nearest claimed range: the `ScriptLoader/`
// units own the range below this one (`Carve8023C950.c` at 0x8023C950..0x8023C998, same auto
// unit's lower part) and `ScriptLoader.cpp` the range above it.  For an anonymous function
// that is the only evidence there is, and it beats a lane picking the directory it happened
// to own.
//
// The one callee, `Free__7CMemoryFPCv` (0x802CE388, `symbols.txt:12992`), is claimed by
// `Kyoto/Alloc/CMemory.cpp` in the DOL and defined under this name for the host link by
// `src/Kyoto/Alloc/PortMwccNew.cpp`, so this unit needs no host stand-in for it and it adds no
// undefined symbol: this file declares the symbol and never defines it.  The one **data**
// reference, `lbl_8041DCB0`, does need the `#ifndef __MWERKS__` block below: it is retail's own
// .sdata2 pool, which the port's flat link does not carry, so the first attempt - declaring it
// `extern` and nothing more, as `Carve80003858.c` does for `lbl_8041A3C0` - left the port one
// undefined symbol heavier and `tools/gate.sh`'s `port link gap` step failed with
// "gap grew: lbl_8041DCB0 is not in port_link_gap_list.md".  Measured: with the host block the
// port is back at its baseline, 286 undefined and 0 duplicates.

/** 0x802CE388, `symbols.txt:12992`, 0x64 = 100 bytes: retail's `CMemory::Free(void const*)`.
 *  Claimed by `Kyoto/Alloc/CMemory.cpp` in the DOL and defined for the host link by
 *  `src/Kyoto/Alloc/PortMwccNew.cpp`.  Declared, never defined here. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** 0x8041DCB0, `.sdata2`, 4 bytes: dtk recorded `.float 0`
 *  (`build/G2ME01/asm/auto_11_8041DAC8_sdata2.s:567-570`); `data:float` in `symbols.txt:24608`.
 *  It is the pool entry the twin `__ct__15CControllerAxisFv` would have got from its own
 *  translation unit.  The matching build takes the real bytes from dtk's own
 *  `auto_11_8041DAC8_sdata2.o`; the host-only definition below carries **retail's own value**,
 *  not a placeholder. */
extern const float lbl_8041DCB0;

void* fn_8023E5A8(void* self, short flag);
void fn_8023E5E4(float* self);

#ifndef __MWERKS__
/* Host-only definition of the one symbol this unit references that nothing else in `src/`
 * defines.  The matching build does not compile this block, so `main.dol` still takes the real
 * 0x8041DCB0 from dtk's own `auto_11_8041DAC8_sdata2.o` and the function keeps its retail bytes.
 * Without it the port's flat link - which carries our sources and not dtk's objects - loses
 * `lbl_8041DCB0`, and `tools/link_check.sh --strict`, whose verdict `tools/probe_sources.sh`
 * reports, names it as a new undefined symbol.  The value is retail's own, `.float 0` as dtk
 * recorded it, not a placeholder.  The guard is `__MWERKS__` rather than `TARGET_PC` for the
 * reason `Carve80229EAC.c` records - the thing that must not happen is a second definition in
 * the matching build, where dtk's `auto_11_8041DAC8_sdata2.o` owns the pool entry. */
const float lbl_8041DCB0 = 0.f;
#endif

/** `fn_8023E5E4` - retail `.text:0x8023E5E4`, 0x10 = 16 bytes: store retail's shared zero
 *  float at +0 and at +4.  Four instructions, no frame, the register `f0` loaded once and
 *  stored twice.  Twin of `__ct__15CControllerAxisFv`, whose members are `float mRelative`
 *  and `float mAbsolute` in that order (`include/Kyoto/Input/CControllerAxis.hpp:13-14`).
 *  Called on `this + 0x58` at 0x8009DC60, in the constructor of the object whose own
 *  constructor is `__ct__20SLdrEditorPropertiesFv`. */
void fn_8023E5E4(float* self) {
  self[0] = lbl_8041DCB0;
  self[1] = lbl_8041DCB0;
}

/** `fn_8023E5A8` - retail `.text:0x8023E5A8`, 0x3C = 60 bytes: a deleting-destructor-shaped
 *  step.  `self` is tested and kept for the return, the flag is a **short**, and only a
 *  positive flag reaches the free; the receiver is returned in r3 whatever the flag was.  The
 *  twin `__dt__5CMainFv` is these 15 instructions word for word, and its own two call sites at
 *  0x8009DABC / 0x8009DC14 pass `li r4,-1`, dead as a free and live as a destroy. */
void* fn_8023E5A8(void* self, short flag) {
  if (self) {
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}