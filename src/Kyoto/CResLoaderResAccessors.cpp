/**
 * The five current-resource accessors, retail `.text:0x802FCAE8..0x802FCC44`, 0x15C = 348
 * bytes, five functions in one contiguous range:
 *
 * | function | address | size | what |
 * | --- | --- | --- | --- |
 * | `fn_802FCAE8` | 0x802FCAE8 | 0x58 | `int IsResourceCompressed(const SObjectTag&)` |
 * | `fn_802FCB40` | 0x802FCB40 | 0x48 | `uint GetResourceOffset(const SObjectTag&)` |
 * | `fn_802FCB88` | 0x802FCB88 | 0x48 | `uint ResourceSize(const SObjectTag&)` |
 * | `fn_802FCBD0` | 0x802FCBD0 | 0x30 | `bool ResourceExists(const SObjectTag&)` |
 * | `fn_802FCC00` | 0x802FCC00 | 0x44 | `FourCC GetResourceTypeById(CAssetId)` |
 *
 * All five have the same 16-byte frame and the same shape, and it is a shape worth having in
 * one place: each is `fn_802FCDE8(this, id)` and then, **only if that returned non-null**, a
 * call on `this->x68_curRes` - the `CPakFile::SResInfo*` the lookup left behind.
 *
 * ```
 *   stwu r1,-16(r1) ; mflr r0 ; lwz r4,4(r4)      <- tag.id, and only the four that take a tag
 *   stw  r0,20(r1) ; stw r31,12(r1) ; mr r31,r3
 *   bl   fn_802FCDE8
 *   cmplwi r3,0 ; beq <the li r3,0>
 *   lwz  r3,104(r31)                              <- this->x68_curRes
 *   bl   GetSize / GetOffset / GetType / IsCompressed
 *   b    <the epilogue>
 *   li   r3,0
 * ```
 *
 * The `li r3,0` sits **after** the body and the body ends in an unconditional `b` over it, so
 * MWCC laid the "not found" arm out as the taken branch of the `beq`; the source below is
 * written as `if (found) { return ...; } return 0;` for that reason and the other way round
 * costs a branch.
 *
 * `fn_802FCBD0` is the odd one out in the only way that shows: it has no `x68_` read and no
 * `li r3,0` at all, because its answer *is* the lookup's - `neg r0,r3 ; or r0,r0,r3 ; srwi
 * r3,r0,31` is `(p != 0)` and nothing else. It is 0x30 bytes because it is one call and one
 * test.
 *
 * **`fn_802FCAE8` needs `IsCompressed() ? 1 : 0` and not `IsCompressed()`, and that is worth
 * 12 bytes.** It is the only one of the five whose value is a `bool`, and retail's tail is
 * `clrlwi r3,r3,24` - the callee's `bool` return being normalised - *followed by* `neg r0,r3 ;
 * or r0,r0,r3 ; srwi r3,r0,31`, which is the same test again. `return x->IsCompressed();`
 * against an `int` return type emits only the `clrlwi` and gives a 0x4C-byte function; the
 * explicit ternary emits both and gives retail's 0x58. Measured, both ways, same object.
 *
 * **`fn_802FCC00` is the only one of the five that does not read `4(r4)`**, and that is what
 * fixes its parameter as a bare `CAssetId` rather than a `const SObjectTag&`: it is
 * `CResLoader::GetResourceTypeById(CAssetId)`, which `include/Kyoto/CResLoader.hpp` already
 * declared, and it hands r4 straight to the lookup. The other four take the tag and pass
 * `tag.id` - `SObjectTag` is `{ FourCC type; CAssetId id; }`, so +4 is the id.
 *
 * All five are unnamed in `config/G2ME01/symbols.txt` and **21 dtk objects reference them**
 * (`auto_03_8004E448` through `auto_03_802F8EB0`), so they are emitted under their dtk names
 * with C linkage and no rename is attempted; a rename would break every one of those
 * references. The two members `include/Kyoto/CResLoader.hpp` declares for them
 * (`GetResourceTypeById`, `ResourceSize`) are therefore left declared and undefined, which is
 * the state they were already in - nothing in the port build calls them, so the link gap does
 * not move - and `fn_802FCDE8` stays retail's own bytes, in a dtk object this unit does not
 * claim.
 *
 * The five definitions below are in **descending retail offset** - `fn_802FCC00` at 0x802FCC00
 * first, `fn_802FCAE8` at 0x802FCAE8 last - which reads as ascending addresses, because
 * mwcceppc emits definitions in reverse source order and mwldeppc keeps the object's `.text`
 * order verbatim. `tools/check_decl_order.py` is the gate; the other way round gives five
 * functions that all still score 100% in an object that is 348 bytes and permuted.
 */
#include "types.h"

#include "Kyoto/CPakFile.hpp"
#include "Kyoto/CResLoader.hpp"

// Already declared with C linkage just above `class CResLoader` in `Kyoto/CResLoader.hpp`:
// `fn_802FCDE8` is the search, and the five functions below are its callers' results. The
// friend declarations in the same header are what lets them read `x68_curRes`.
extern "C" int fn_802FCAE8(void* resLoader, const SObjectTag& tag);
extern "C" uint fn_802FCB40(void* resLoader, const SObjectTag& tag);
extern "C" uint fn_802FCB88(void* resLoader, const SObjectTag& tag);
extern "C" bool fn_802FCBD0(void* resLoader, const SObjectTag& tag);
extern "C" uint fn_802FCC00(void* resLoader, CAssetId id);

extern "C" uint fn_802FCC00(void* resLoader, CAssetId id) {
  CResLoader* const self = static_cast< CResLoader* >(resLoader);
  if (fn_802FCDE8(resLoader, id) != nullptr) {
    return self->x68_curRes->GetType();
  }
  return 0;
}

extern "C" bool fn_802FCBD0(void* resLoader, const SObjectTag& tag) {
  return fn_802FCDE8(resLoader, tag.id) != nullptr;
}

extern "C" uint fn_802FCB88(void* resLoader, const SObjectTag& tag) {
  CResLoader* const self = static_cast< CResLoader* >(resLoader);
  if (fn_802FCDE8(resLoader, tag.id) != nullptr) {
    return self->x68_curRes->GetSize();
  }
  return 0;
}

extern "C" uint fn_802FCB40(void* resLoader, const SObjectTag& tag) {
  CResLoader* const self = static_cast< CResLoader* >(resLoader);
  if (fn_802FCDE8(resLoader, tag.id) != nullptr) {
    return self->x68_curRes->GetOffset();
  }
  return 0;
}

extern "C" int fn_802FCAE8(void* resLoader, const SObjectTag& tag) {
  CResLoader* const self = static_cast< CResLoader* >(resLoader);
  if (fn_802FCDE8(resLoader, tag.id) != nullptr) {
    return self->x68_curRes->IsCompressed() ? 1 : 0;
  }
  return 0;
}

#ifdef TARGET_PC
// The port's own `fn_802FCDE8`. It is retail's bytes in the matching build - the function is
// unnamed in `config/G2ME01/symbols.txt`, so it lives in a dtk object this unit does not claim,
// and mwcceppc does not define `TARGET_PC` so this block is not in the DOL.
//
// Retail's 0x104 bytes walk **three** of the four lists and leave the fourth alone, and the
// walk is measured, not guessed: the first loop tests `this+0x20` against `this+0x38`
// (0x802fce3c/0x802fce40), the second `this+0x60` (0x802fce48) and the third `this+0x38`
// (0x802fcebc/0x802fcec0), so +0x00 and +0x18 and +0x30 are the three it visits and `+0x48`,
// the loading list, is the one it never reads. Each step is `node->x4_pak` -
// `lwz r31,12(r30)`, the item's second word - and the probe is a `CPakFile::GetResInfo`.
//
// Retail's own per-pak probes differ between the loops - `fn_802FCF98` for `+0x00` and
// `fn_802FCF10` for the other two, 0x54 and 0x88 bytes - and both write `x64_curId` and
// `x68_curRes` on success (`stw r31,100(r30)` / `stw r3,104(r30)` at 0x802fcfd0/0x802fcfd4).
// Neither is written, and the difference between them is not derivable from this tree, so this
// is the honest simplification: one `GetResInfo` per pak, and the same two stores.
extern "C" void* fn_802FCDE8(void* resLoader, CAssetId id) {
  CResLoader* const self = static_cast< CResLoader* >(resLoader);
  rstl::list< SPakLoadEntry >* const lists[] = {
      &self->x0_aramList, &self->x18_aramFileList, &self->x30_pakList
  };
  for (int i = 0; i < 3; ++i) {
    for (rstl::list< SPakLoadEntry >::iterator it = lists[i]->begin(); it != lists[i]->end();
         ++it) {
      const CPakFile::SResInfo* const found = it->x4_pak->GetResInfo(id);
      if (found != nullptr) {
        self->x64_curId = id;
        self->x68_curRes = const_cast< CPakFile::SResInfo* >(found);
        return it->x4_pak;
      }
    }
  }
  return nullptr;
}
#endif // TARGET_PC
