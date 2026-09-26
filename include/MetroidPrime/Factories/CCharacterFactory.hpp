#ifndef _CCHARACTERFACTORY
#define _CCHARACTERFACTORY

#include "types.h"

#include "Kyoto/SObjectTag.hpp"
#include "Kyoto/TToken.hpp"

class CAnimCharacterSet;
class CSimplePool;

// **Declared, not written.** Retail's constructor is `fn_80030410` (0x80030410, 0x560 bytes and
// twenty-five callees, the start of a 114-function subtree), and the only thing this tree needs
// from the class so far is what `CCharacterFactoryBuilder::CDummyFactory::Build` does with it:
// `new` 0x8C bytes, construct, and hand the pointer to `CFactoryFnReturn`. The shape of the
// constructor is Build's call at 0x800321A8 - `r4 = gpSimplePool`, `r5` = a 12-byte
// `TLockedToken<CAnimCharacterSet>` built in Build's frame at r1+40, `r6` = the tag's id.
//
// The destructor is virtual, and that is measured too: the owner that
// `TToken<CCharacterFactory>::GetIObjObjectFor` wraps it in (retail `fn_800322F4`) deletes it
// with `lwz r12,8(r12); bctrl` and `r4 = 1` - slot 0x8, the first virtual, the deleting form.
// Nothing here defines it, and nothing needs to: a virtual call names no symbol.
class CCharacterFactory {
public:
  CCharacterFactory(CSimplePool& store, const TLockedToken< CAnimCharacterSet >& ancs,
                    CAssetId selfId);
  virtual ~CCharacterFactory();

private:
  uchar x4_unk[0x88];
};
CHECK_SIZEOF(CCharacterFactory, 0x8C)

#endif // _CCHARACTERFACTORY
