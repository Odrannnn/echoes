/**
 * `rstl::CRcPtrData`'s copy constructor - retail's out-of-line `rstl::rc_ptr` copy constructor.
 *
 * **0x80049010, 0x24 = 36 bytes, `Matching`, and the only reason this unit exists.** Retail's copy
 * constructor is out of line while retail's own compiler inlines the identical nine instructions
 * in six other places; the disassembly and the reasoning are in `include/rstl/rc_ptr.hpp` and in
 * `docs/research/rc_ptr.md`. `fn_80049010` carries **no mangled name** in the map while
 * `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv` does, which is the measurement that says retail's
 * words lived in a class that is not a template - so this symbol is `rstl::CRcPtrData`'s, not any
 * `rc_ptr<T>`'s, and it serves every `T` at once.
 *
 * `config/G2ME01/symbols.txt` renames `fn_80049010` to `__ct__9CRcPtrDataFRC9CRcPtrData`, the name
 * mwcceppc emits (read out of the object with `nm`, not guessed), and
 * `config/G2ME01/splits.txt` gives this unit exactly those 36 bytes. `tools/range_owner.py` said
 * `.text 0x80049010 0x80049034` was UNCLAIMED before this unit existed, and it sat inside the
 * single retail `auto_03_8004875C_text.o`, which the split shortens at both ends.
 *
 * The body is the two words copied and an AddRef through the *second* word - nothing else. The
 * spelling is the one that reproduces retail's register allocation: the mem-init list, and
 * `++(*x4_refCount)` in the body, which re-reads `4(r3)` rather than reusing the value it just
 * stored and therefore lands the refcount in r4 and the count in r3, as retail does.
 */

#include "rstl/rc_ptr.hpp"

rstl::CRcPtrData::CRcPtrData(const CRcPtrData& other)
    : x0_ptr(other.x0_ptr), x4_refCount(other.x4_refCount) {
  ++(*x4_refCount);
}
