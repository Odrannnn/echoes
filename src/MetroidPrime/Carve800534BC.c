// Carved out of an unclaimed dtk `auto_*` range (`auto_03_800534B4_text`,
// 0x800534B4..0x80053594).  Every number here is measured: the address and size come from
// `config/G2ME01/symbols.txt:1616`, the instructions are the ones dtk itself emitted into
// `build/G2ME01/asm/auto_03_800534B4_text.s` and the ones
// `build/binutils/powerpc-eabi-objdump -d build/G2ME01/main.elf` shows, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x800534BC..0x800534C4, 0x8 = 8 bytes, 1 function:
//
//   fn_800534BC    0x800534BC  0x8    lfs f1,lbl_8041A920@sda21(r0) ; blr
//
// **What it is.** A byte-shape twin of the matched
// `GetIngSnatchingModelOverlapSize__10CPatternedCFv` (0x80074AF8, also 0x8 bytes, also
// `lfs f1,<.sdata2 float> ; blr`), whose C is one line in
// `src/MetroidPrime/Enemies/CPatterned.cpp`: `return 0.f;`. The two differ only in which
// `.sdata2` word they load: retail's twin loads `lbl_8041AAC0`, which
// `build/G2ME01/asm/auto_11_8041AA90_sdata2.s` types `.float 0`, and this copy loads
// `lbl_8041A920`, which `auto_11_8041A900_sdata2.s` types `.float 1`. So this is the same
// accessor shape returning **1.0f**, not 0.0f. A twin's shape is evidence about the
// *instruction sequence*, not about the constant, and the constant is what this unit claims.
//
// The neighbours confirm the class. `0x800534B4` is `SetDrawFlags__12CParticleGenFUi` and
// `0x800534C4` is `GetDrawFlags__12CParticleGenCFv`, both `CParticleGen` accessors of 0x8
// bytes each, and `include/Kyoto/Particles/CParticleGen.hpp` has the pair
// `SetDrawFlags(uint) { mDrawFlags = flags; }` / `GetDrawFlags() const { return mDrawFlags; }`
// around a `virtual float GetGeneratorRate() const { return 1.f; }` that retail compiles to
// exactly this two-instruction body. Retail carries no name for it, so `symbols.txt` has the
// `fn_800534BC` placeholder; the identification above is recorded and not relied on.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` order verbatim,
// so an ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  One function, so the rule is trivially met here; it is
// written down because every file in this vein carries it.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is
// a `.c` rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because the functions on
// either side of this run are named `CParticleGen` accessors that belong to a class, not to
// an anonymous free function's neighbourhood.
//
// The directory is retail own, taken from the nearest claimed range: this address sits
// between `MetroidPrime/Carve800534B0.c` (0x800534B0..0x800534B4) and
// `MetroidPrime/Carve80053594.c` (0x80053594..0x8005359C).

/* 0x8041A920, `.sdata2`, typed `data:float` in `config/G2ME01/symbols.txt:21860` and typed
 * `.float 1` in `build/G2ME01/asm/auto_11_8041A900_sdata2.s` (8 bytes: 1.0f then a padding
 * 0.f, which is why the object is 8 bytes and the claim is a single 4-byte read).
 * **Declared, not defined**: the bytes come from dtk's own `auto_11_8041A900_sdata2.o`,
 * which is in the link for the rest of the DOL.  Defining it here would emit a second
 * `.sdata2` pool this unit does not claim, and a literal `return 1.f;` would emit exactly
 * that.  Reading retail's own word is what keeps this unit's claim to `.text` alone. */
extern const float lbl_8041A920;

float fn_800534BC(void) { return lbl_8041A920; }
