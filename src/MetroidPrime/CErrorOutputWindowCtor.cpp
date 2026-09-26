/**
 * `CErrorOutputWindow::CErrorOutputWindow(bool)` - retail `__ct__18CErrorOutputWindowFb`,
 * `.text:0x8018169C`, `size:0xB4` = 180 bytes, 45 instructions
 * (`config/G2ME01/symbols.txt:6389` carries the size and the name).
 *
 * ## The shape
 *
 * A base `CIOWin` built from a `rstl::string` temporary, the derived vtable pointer, `x14_state`
 * and `x1c_msg` set to zero, and four `bool : 1` fields set from their declaration order: three to
 * `true` and the fourth to `!arg`.
 *
 * ## Why this file declares its own class rather than including the header
 *
 * The same reason `CConsoleOutputWindowCtor.cpp` does, and for the same class of reason:
 * `CErrorOutputWindow` has no key function anywhere in the tree, so mwcceppc emits no vtable for
 * it, and `__vt__18CErrorOutputWindow` would be an undefined symbol in the DOL link. Retail's own
 * store is `R_PPC_ADDR16_HA/LO lbl_803B5910` - retail's vtable object, an unclaimed gap `dtk`
 * fills, not `__vt__18CErrorOutputWindow`. So the base is made a *member* and the store is
 * written against the gap object. `CIOWin` is a vtable pointer plus a 16-byte `rstl::string`, so
 * it is 0x14 bytes, `x14_state` lands at 0x14, the `bool : 1` group at 0x18 and `x1c_msg` at 0x1C
 * - which is what `stw r6,20(r30)`, `lbz r0,24(r30)` and `stw r6,28(r30)` describe.
 *
 * ## The `rstl::string` temporary
 *
 * `addi r3,r1,8 ; bl string_l__4rstlFPCc` is retail's **one-argument** name-string factory: the
 * character pointer arrives in r4 and the destination is the hidden return pointer in r3.
 * `include/rstl/string.hpp` declares `rstl::string_l(const char*)` returning by value, which
 * reproduces exactly that pair of argument registers, so the member-initialiser argument is a call
 * to it. Retail's third call is `internal_dereference__Q24rstl66basic_string<...>Fv`, the
 * temporary's destructor, which the base-constructor argument temporary produces by itself.
 *
 * ## Why this is at 78.56% and not `Matching` - the `clrlwi`
 *
 * Five of forty-five instructions differ, and **four of the five are a knock-on shift**: this body
 * is 0xB8 = 46 instructions against retail's 45, and the one extra instruction is
 *
 *     clrlwi  r0,r31,24        (ours)       where retail has  cntlzw  r0,r31
 *
 * `cntlzw r0,r31 ; srwi r4,r0,5` is mwcceppc's `!x` for a **word** x. For a **`bool`** x it first
 * widens the byte to a word with `clrlwi r0,rX,24`, because the MW ABI only promises the low byte
 * of a bool argument. Everything else - the `srwi`, the four `lbz`/`rlwimi`/`stb` read-modify-write
 * pairs, the two zero stores, the frame, the three calls and their argument registers - is
 * byte-identical to retail, and the `bool : 1` group already has the right declaration order
 * (bits 24, 25, 26 are `li r5,1` and bit 27 is the negation, which is the fourth field).
 *
 * **The mask is not removable from a bool-typed operand.** Measured with the unit's own flags
 * (`tools/probe_cc.sh`), `!x` on an `int`/`unsigned` parameter is `cntlzw r0,rX ; ...` with no
 * mask, and the same function with an extra call and a callee-saved copy of the parameter still
 * has none - that is the shape `void f(SB*, int) { g(); s->d = !p; }` compiles to, and it is
 * byte-identical to retail's idiom here. For a `bool` the mask appears in *every* shape tried:
 * leaf / after a call / result returned / result stored to an `int` / to a `bool` / to a
 * **`bool : 1` field** / through a pointer / in a `const bool` local / as a ternary. 22 in-place
 * spellings of the fourth field were measured with `tools/try_batch.py` (`!arg`, `arg == 0`,
 * `0 == arg`, `!(arg != 0)`, `!static_cast<int>(arg)`, `!w` for `const int w = arg`, `arg ? false
 * : true`, `~static_cast<int>(arg) & 1`, `(arg & 1) == 0`, `(arg ^ 1) != 0`, `!(arg | 0)`,
 * `!(arg + 0)`, `!*(const int*)&arg`, `!*(const unsigned*)&arg`, `!*bp`, `arg == false`,
 * `arg == 0u`, `((arg + 1) & 1) == 0`, literals `1` and `true` for the other three fields, and a
 * `bool`/`int`/pointer destination); the best is still 4 differing instructions and the mask is
 * in all of them. Taking the address of the parameter is the only thing that removes it - the
 * value then arrives by `lbz`/`lwz` - and that costs a spill, a 48-byte frame and 32 differing
 * instructions.
 *
 * **But the mask is not a property of the construct - it is a version difference, and retail's own
 * binary proves it.** `IsOneShot__20CScriptStreamedMusicFb` at 0x8015DDD8 is four instructions
 * long and is `clrlwi r0,r3,24 ; cntlzw r0,r0 ; srwi r3,r0,5 ; blr` - i.e. retail's compiler emits
 * the very mask we cannot get rid of. So the two functions in the same binary, from the same
 * compiler, disagree: `IsOneShot(bool b) { return !b; }` masks, and this constructor - also a
 * `bool` parameter, also negated, also into a `bool : 1` field - does not. The 1287 `clrlwi
 * rX,rY,24` in the DOL and its 616 `cntlzw`s never occur within four instructions of each other.
 * The reading that fits every measurement is that mwcceppc 2.7 normalises **every** bool-to-word
 * widening of a register-resident value, and retail's compiler elided it in some contexts and not
 * others.
 *
 * **SUPERSEDED 2026-09-26, and this is now measured rather than inferred.** This header used to
 * say that only a second compiler version closed the gap. A lane built one - `Object(...,
 * mw_version="GC/3.0a3")` works, because `tools/project.py` already resolves `mw_version` and
 * `cflags` as per-object overrides - and **every available version was then swept against this
 * real source** (`tools/probe_cerror_versions.py`):
 *
 *     every GC/2.x  -> 46 instructions
 *     every GC/3.0a* -> 34 instructions
 *     retail         -> 45 instructions
 *
 * **None is byte-exact.** 3.0a* does fix the `cntlzw` this function wanted, and then makes two
 * *other* things worse: at `-O4,p` it coalesces retail's four `lbz`/`rlwimi`/`stb` read-modify-write
 * pairs, and at `-O1` it keeps those but drops a `li r3,1` the retail code CSEs. Both gaps are
 * redundant-load/store elimination, and no flag exposes them - `-no_peephole`,
 * `-optcode_speed` and `-O4,t` were tried and there is no CSE switch in `-help all`.
 *
 * So the score goes **78.56% (2.7) -> 55.44% (3.0a3, -O4,p) -> 13.11% (3.0a3, -O1)**, and
 * **78.56% is the best any compiler on this machine achieves for this function.** The version
 * hypothesis was right about retail - MP2's build did use more than one compiler, which is why its
 * binary holds both forms 22 KB apart - and wrong about the conclusion. The remaining gap is not a
 * compiler this project can obtain.
 */
#include "types.h"

#include "rstl/rmemory_allocator.hpp"
#include "rstl/string.hpp"

extern "C" {
/** `.rodata 0x803A9F38`, "Error output window" (18 characters plus a terminator). */
extern const char lbl_803A9F38[];
/** `.data 0x803B5910`, the vtable object `dtk` fills. */
extern const char lbl_803B5910[];
} // extern "C"

/** `CIOWin` without its virtuals, its name being 16 bytes of storage this unit never reads. */
class CIOWin {
public:
  void* x00_vtable;
  char x04_name[16];
  CIOWin(const rstl::string& inName);
};

class CErrorOutputWindow {
  CIOWin x00_base;
  int x14_state;
  bool x18_24_ : 1;
  bool x18_25_ : 1;
  bool x18_26_ : 1;
  bool x18_27_ : 1;
  const wchar_t* x1c_msg;

public:
  CErrorOutputWindow(bool arg);
};

CErrorOutputWindow::CErrorOutputWindow(bool arg)
  : x00_base(rstl::string_l(lbl_803A9F38)) {
  // 0x801816F0: the derived vtable store; see the header for why it is written here.
  *reinterpret_cast< void** >(this) = const_cast< char* >(lbl_803B5910);
  x14_state = 0;
  x18_24_ = true;
  x18_25_ = true;
  x18_26_ = true;
  x18_27_ = !arg;
  x1c_msg = 0;
}
