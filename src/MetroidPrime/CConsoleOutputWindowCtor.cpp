/**
 * `CConsoleOutputWindow::CConsoleOutputWindow(int, float, float)` - retail
 * `__ct__20CConsoleOutputWindowFiff`, `.text:0x800D63F0`, `size:0x1B4` = 436 bytes, 109
 * instructions (`config/G2ME01/symbols.txt:3751` carries the size). The function's own object is
 * `build/G2ME01/obj/auto_03_800D50B8_text.o` at +0x1338, and its twenty-two relocations are the
 * twenty-two named things this body needs.
 *
 * ## The three numbers that had to be measured before anything could be written
 *
 * 1. **`r2` is `_SDA2_BASE_` (0x804223C0) and `r13` is `_SDA_BASE_` (0x8041FD80) in this
 *    function.** `tools/sda.py` resolves against `_SDA_BASE_` unless the operand is spelled
 *    `s2:<off>`, and reading both registers against one base makes two of this function's four
 *    small-data operands land in `.sbss` where there are no bytes to read. The object's own
 *    relocation records settle it without any arithmetic: the two `r2` operands relocate to
 *    `lbl_8041B568` and `lbl_8041B570`, both `.sdata2`, and the two `r13` operands to
 *    `lbl_804181D8` (.sdata) and `lbl_804190D0` (.sbss).
 * 2. **`0x43300000` is not a float and not 176.0.** It is the **high word of the double
 *    `lbl_8041B570` = `0x4330000080000000` = 2^52 + 2^15**, and the pair of `stw`es into
 *    `r1+0x30`/`r1+0x34` followed by one `lfd` is **mwcceppc's int-to-float conversion**: the
 *    integer is biased into the high word of a double whose mantissa is zero, and the bias is
 *    then subtracted back out. Proved by compiling `float f4(int n) { return (float)n; }` with
 *    the unit's own flags: it emits exactly `xoris r3,r3,32768 ; lis r0,17200 ; lfd f1,<bias> ;
 *    stw r3,12(r1) ; stw r0,8(r1) ; lfd f0,8(r1) ; fsubs f0,f0,f1`, and the object's own
 *    `.sdata2` shows the 8-byte constant at the relocation is `43300000 80000000`. The same
 *    8-byte pattern appears 711 times in the DOL, which is why `.sdata2` is full of
 *    `0x4330000000000000` and `0x4330000080000000` pairs. **Those bytes are the constant the
 *    compiler generates, which is why this unit claims `.sdata2 0x8041B570..0x8041B578` and
 *    nothing else: the 632.0f at 0x8041B568 is a reference, not a definition.**
 * 3. **`fsubs f1,f1,f2` is the conversion, not the function's own arithmetic.** It is tempting to
 *    read 0x800D64C0 as a subtraction in this function's expression; it is not. There are only
 *    two floating-point operations in the body, and they are `fsubs` (the conversion) and
 *    `fdivs`, so the source is `x40_ = (int)(632.f / (float)h)` with **one** operation of its
 *    own. Getting there needed a probe: every spelling that casts the operand to `double` first
 *    emits a double `fsub` plus an `frsp`, and only `float g = n;` - the int-to-float conversion
 *    mwcceppc does on its own - produces the single `fsubs` retail has.
 *
 * And the same Gekko quirk that puts the integer in the **high** word of an `fctiwz` result
 * (`fctiwz ; stfd ; lwz +4`) is why `stfd f0,56(r1)` / `lwz r5,60(r1)` is `(int)` of a float rather
 * than a double round trip. It is visible out of line in `__ct__5CFontFf` at 0x802BAD7C-0x802BAD84.
 *
 * ## The shape
 *
 * Base constructor from a temporary name, the `CFont` constructor at +0x14 with the third
 * argument, the font's own line-height query, two out-of-line `rstl::vector` resizes, and one
 * loop that fills both containers `n` times. The constructor's third argument builds a font at
 * scale 0.75, whose `int` size is `(int)(16.f * 0.75)` and whose line height is
 * `(int)(15.f * 0.75)` - the two out-of-line helpers at 0x802BAD6C and 0x802BAD0C.
 *
 * ## Why this file declares its own class rather than including the header
 *
 * **`CConsoleOutputWindow` has no key function anywhere in the tree, so no vtable is emitted for
 * it, and `__vt__20CConsoleOutputWindow` would be an undefined symbol in the DOL link.** A vtable
 * is only emitted by the translation unit that defines the class's key function - the first
 * non-pure, non-inline virtual - and the only candidates are `~CConsoleOutputWindow`
 * (0x800D6360, `fn_800D6360`) and `OnMessage` (0x800D62F0), neither written. Retail's vtable is
 * at 0x803B37F0, is `lbl_803B37F0` in `symbols.txt`, and is an unclaimed gap that `dtk` fills; its
 * five slots point at 0x800D6360, 0x800D62F0, 0x80049E18, 0x800D61A4 and 0x80049E10, three of
 * which are functions this tree has no body for. So it cannot be generated here either, and the
 * vtable bytes are not this unit's to produce: the unit claims `.text` only, and the vtable stays
 * filled.
 *
 * What the constructor needs is therefore *a store of a pointer to that object*, and retail's own
 * relocation for it is `R_PPC_ADDR16_HA/LO lbl_803B37F0` - not `__vt__20CConsoleOutputWindow`.
 * The way to get that is to make the class **not polymorphic in this translation unit**: `CIOWin`
 * is then a *member* rather than a base, its own constructor (which is `Matching`, and which
 * stores `__vt__6CIOWin` itself) is still called by its real mangled name, no vtable is needed,
 * and the derived-vptr store is written by hand against the gap object. A member of the real
 * `CIOWin` is not a legal declaration - it is an abstract class, and mwcceppc reports "illegal use
 * of abstract class" - so `CIOWin` is redeclared here without its virtuals and with its
 * constructor **declared but not defined**, which is what keeps the call relocating to
 * `__ct__6CIOWinFRCQ24rstl66basic_string<...>`, exactly retail's target and defined by
 * `src/MetroidPrime/CIOWinCtor.cpp`. Composition rather than inheritance is layout-identical
 * here: `CIOWin` is a vtable pointer plus a 16-byte `rstl::string`, so it is 0x14 bytes, `mFont`
 * lands at 0x14, and the object is 0x4C bytes - which is what `lwz r5,60(r1) ; stw r5,64(r30)` and
 * the final `stw r0,72(r30)` describe.
 *
 * The two containers are the same story: `rstl::vector<T>` has a declared destructor, so a member
 * of that type would make the compiler emit a destructor call this function does not have. The
 * local `SVec<T>` is `rstl::vector<T>`'s four fields - `rmemory_allocator` at +0, `int` count at
 * +4, `int` capacity at +8, `T*` items at +0xC - which is what the out-of-line callees read:
 * `fn_80052150` tests `8(r3)` against the argument and strides by 16, `fn_800D65A4` strides by 4,
 * and both read the count at +4 and the data pointer at +0xC.
 *
 * ## The string temporaries
 *
 * `rstl::string`'s only constructor for a `char const*` in `include/rstl/string.hpp` takes three
 * arguments with two defaults, and spelling `rstl::string(lbl_803A89E8)` therefore materialises
 * `li r5,-1` and an allocator address - two instructions retail does not have. Retail's call is
 * to the one-argument `string_l__4rstlFPCc` at 0x802FF418, which is a separate symbol, so the
 * name is built through `SName`, whose constructor calls that symbol and whose conversion
 * operator hands the temporary to `CIOWin`. Its destructor is what produces retail's third call,
 * `internal_dereference__Q24rstl66basic_string<...>Fv`.
 */
#include "types.h"

#include "Kyoto/Alloc/CMemory.hpp"

#include "rstl/construct.hpp"
#include "rstl/rmemory_allocator.hpp"
#include "rstl/string.hpp"
#include "rstl/vector.hpp"

namespace rstl {
/**
 * `rstl::string_l(char const*)` at 0x802FF418. It is retail's **one-argument** name-string
 * factory: it takes the character pointer in r4 and the destination `rstl::string` in r3, which
 * is the hidden return pointer of a by-value return - `addi r3,r1,28 ; mr r4,r0 ; bl
 * string_l__4rstlFPCc` is exactly that, and declaring it as a function *returning* `rstl::string`
 * by value reproduces both argument registers. `include/rstl/string.hpp` has no such declaration,
 * and spelling `rstl::string(lbl_803A89E8)` instead materialises the two defaulted arguments
 * (`li r5,-1` and an allocator address) and relocates against the three-argument constructor,
 * which is not what retail calls.
 */
extern "C" string string_l__4rstlFPCc(const char* data);
} // namespace rstl

class CConsoleOutputWindow;

// Retail's own relocations, in the order the object lists them.
extern "C" {
extern const char lbl_803A89E8[];         // .rodata 0x803A89E8, "ConsoleOutputWindow", 20 bytes
extern const char __vt__20CConsoleOutputWindow[]; // .data 0x803B37F0, the vtable object dtk fills
extern const float lbl_8041B568;           // .sdata2 0x8041B568, 632.0f
extern float lbl_804181D8;                 // .sdata  0x804181D8, 0.0f
extern CConsoleOutputWindow* mInstance__20CConsoleOutputWindow; // .sbss 0x804190D0, the static instance pointer
/** `rstl::string::string_l(char const*)`, 0x802FF418. Named in symbols.txt, defined by dtk. */
} // extern "C"

/** `rstl::vector<T>` without its constructor and destructor - see the header. */
template < typename T >
struct SVec {
  rstl::rmemory_allocator x0_allocator;
  int x4_count;
  int x8_capacity;
  T* xc_items;
};

/** `CFont`'s two fields: `int mFontSize` at +0, `float mScale` at +4. */
struct SFont {
  int mFontSize;
  float mScale;
};

/** `CIOWin` without its virtuals, its name being 16 bytes of storage this unit never reads. */
class CIOWin {
public:
  void* x00_vtable;
  char x04_name[16];
  CIOWin(const rstl::string& inName);
};

/** The one-argument string temporary; see the header for why it cannot just be a `rstl::string`. */
class CConsoleOutputWindow {
  CIOWin x00_base;
  SFont x14_font;
  float x1c_unk;
  SVec< rstl::string > x20_text;
  SVec< float > x30_floats;
  int x40_;
  int x44_;
  int x48_;

public:
  CConsoleOutputWindow(int n, float a, float b);
};

extern "C" {
/** `CFont::CFont(float)`: `x00 = (int)(16.f * scale) ; x04 = scale`. Unnamed in symbols.txt. */
void __ct__5CFontFf(SFont* self, float scale);
/** `CFont`'s line height: `(int)(15.f * self->mScale)`. Its `r4` is dead. */
int CharWidth__5CFontCFc(SFont* self, int unused);
/** `rstl::vector<rstl::string>::resize(int)`: capacity at +8, 16-byte elements, data at +0xC. */
void fn_80052150(SVec< rstl::string >* self, int n);
/** `rstl::vector<float>::resize(int)`: same shape, 4-byte elements. */
void fn_800D65A4(SVec< float >* self, int n);
} // extern "C"

#if defined(__MWERKS__)
// Upstream names 0x80052150 `rstl::vector<rstl::string>::reserve(int)`. Declaring the
// specialization keeps the template body out of this unit, so the call is the only thing emitted.
template <>
void rstl::vector< rstl::string >::reserve(int size);
// And 0x800D65A4 `rstl::vector<float>::reserve(int)`, the same way: its mangled name has `<>` in
// it, so it cannot be spelled as an `extern "C"` identifier.
template <>
void rstl::vector< float >::reserve(int size);
#endif

CConsoleOutputWindow::CConsoleOutputWindow(int n, float a, float b)
: x00_base(rstl::string_l__4rstlFPCc(lbl_803A89E8)) {
  // 0x800D6454-0x800D6464. The store is the derived vtable pointer; see the header for why it is
  // written here and why the offset is 0.
  *reinterpret_cast< void** >(this) = const_cast< char* >(__vt__20CConsoleOutputWindow);
  // 0x800D6468, then 0x800D646C.
  __ct__5CFontFf(&x14_font, b);
  x1c_unk = a;
  x20_text.x4_count = 0;
  x20_text.x8_capacity = 0;
  x20_text.xc_items = 0;
  x30_floats.x4_count = 0;
  x30_floats.x8_capacity = 0;
  x30_floats.xc_items = 0;
  // 0x800D6494: the line height, whose live range is the conversion below.
  // 0x800D6494-0x800D64D8. The cast is what makes the whole expression single precision: it is
  // one `fsubs` and one `fdivs`, and the `fsubs` is mwcceppc's own int-to-float conversion, not
  // an operation of this expression.
  const float lineHeight = static_cast< float >(CharWidth__5CFontCFc(&x14_font, 48));
  x40_ = static_cast< int >(lbl_8041B568 / lineHeight);
  x44_ = 0;
  x48_ = 0;
#if defined(__MWERKS__)
  reinterpret_cast< rstl::vector< rstl::string >* >(&x20_text)->reserve(n);
#else
  fn_80052150(&x20_text, n);
#endif
#if defined(__MWERKS__)
  reinterpret_cast< rstl::vector< float >* >(&x30_floats)->reserve(n);
#else
  fn_800D65A4(&x30_floats, n);
#endif
  // 0x800D64F0-0x800D6568. `r29` holds the name object and the string starts 20 bytes into it,
  // which is the byte after the 19-character name and its terminator, so every element is a run
  // of terminators `x40_ + 1` long.
  rstl::rmemory_allocator alloc;
  const char* nameObject = lbl_803A89E8;
  for (int i = 0; i < n; ++i) {
    rstl::construct(x20_text.xc_items + x20_text.x4_count++,
                    rstl::string(nameObject + 20, x40_ + 1, alloc));
    // 0x800D6544-0x800D6560, and the statement is split in two **for the register allocator, not
    // for the arithmetic**: spelled as one `x30_floats.xc_items[x30_floats.x4_count++] =
    // lbl_804181D8` the body is the same 109 instructions in the same registers but mwcceppc
    // schedules the constant's `lfs f0,lbl_804181D8(r13)` four slots early, immediately after
    // `addi r28,r28,1` instead of immediately before the `stfsx` that consumes it - which is the
    // whole of the 1.83% this file used to score. Taking the destination address into a local
    // first sinks the load to the use without moving a register: retail's
    // `lwz r4,52(r30) ; addi r28,r28,1 ; lwz r5,60(r30) ; addi r3,r4,1 ; slwi r0,r4,2 ;
    // stw r3,52(r30) ; lfs f0,... ; stfsx f0,r5,r0` is then reproduced exactly. The two
    // neighbouring spellings that do not work: `float* d = xc_items + x4_count;` before the
    // increment puts the count in r3 and the items in r4 (the address temporary claims a register
    // first), and pre-incrementing the store's index reorders the whole tail.
    float* const dst = &x30_floats.xc_items[x30_floats.x4_count++];
    *dst = lbl_804181D8;
  }
  // 0x800D656C.
  mInstance__20CConsoleOutputWindow = this;
}
