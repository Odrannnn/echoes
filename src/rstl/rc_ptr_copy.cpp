/**
 * `rstl::CRcPtrData::CopyInto` - retail's out-of-line `rstl::rc_ptr` copy constructor.
 *
 * **0x80049010, 0x24 = 36 bytes, `Matching`, and the only reason this unit exists.** Retail's copy
 * constructor is out of line while retail's own compiler inlines the identical nine instructions
 * in six other places; the disassembly and the reasoning are in `include/rstl/rc_ptr.hpp` and in
 * `docs/research/rc_ptr.md`. `fn_80049010` carries **no mangled name** in the map while
 * `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv` does, which is the measurement that says retail's
 * words lived in a class that is not a template - so this symbol is `rstl::CRcPtrData`'s, not any
 * `rc_ptr<T>`'s, and it serves every `T` at once.
 *
 * `config/G2ME01/symbols.txt` renames `fn_80049010` to `CopyInto__Q24rstl10CRcPtrDataFRCQ24rstl10CRcPtrData`,
 * the name mwcceppc emits (read out of the object with `nm`, not guessed), and
 * `config/G2ME01/splits.txt` gives this unit exactly those 36 bytes. `tools/range_owner.py` said
 * `.text 0x80049010 0x80049034` was UNCLAIMED before this unit existed, and it sat inside the
 * single retail `auto_03_8004875C_text.o`, which the split shortens at both ends.
 *
 * ## Why this is a *static* member function and not the copy constructor
 *
 * Twenty spellings of a copy constructor's body all left mwcceppc allocating the AddRef to
 * **r5/r4** where retail uses **r4/r3**; the table is in `docs/research/rc_ptr.md`. The cause is
 * not the body: it is `this` in r3. A **non-static** member function keeps r3 reserved for the
 * object pointer, so the first temporary after the object pointer dies is pushed to r5. The same
 * body in a **static** member function - or in a free function - gets r4 and r3, because r3 and r4
 * are ordinary parameters there and are recycled as soon as they are dead. Measured both ways on
 * the same compiler and the same flags, on four independent spellings
 * (`rstl::CRcPtrData::CopyInto`, a namespace-scope `rstl::rc_ptr_data_copy`, and a probe that
 * varied the increment's spelling inside each).
 *
 * The ABI is identical either way - r3 is the destination and r4 the source, which is what a
 * copy constructor's `this`/`other` are - so all fifteen call sites are unchanged and
 * `CRcPtrData`'s own copy constructor is now `inline` and simply forwards here.
 *
 * ```
 *   ours = retail, byte for byte
 *   lwz  r5,0(r4) ; lwz  r0,4(r4) ; stw  r5,0(r3) ; stw  r0,4(r3)
 *   lwz  r4,4(r3) ; lwz  r3,0(r4) ; addi r0,r3,1 ; stw  r0,0(r4) ; blr
 * ```
 */

#include "rstl/rc_ptr.hpp"

void rstl::CRcPtrData::CopyInto(CRcPtrData* dest, const CRcPtrData& src) {
  dest->x0_ptr = src.x0_ptr;
  dest->x4_refCount = src.x4_refCount;
  ++(*dest->x4_refCount);
}
