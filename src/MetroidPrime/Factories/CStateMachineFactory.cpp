// Retail's own `operator new[]` placement string, `.rodata:0x803AA230` (`symbols.txt:17242`,
// 0x10 bytes) - the `R_PPC_ADDR16_HA`/`R_PPC_ADDR16_LO` pair both `new` sites in this range feed
// `__nw__FUlPCcPCc` with (0x80194064/0x8019406C, and 0x80194394/0x8019439C in
// `GetNewDerivedObject`). **Declared, never defined**: a literal of our own is routed through
// mwcceppc's per-translation-unit `@stringBase0` pool, which makes this object emit a 7-byte
// `.rodata` section that the linker appends to the global string pool, shifting every later pool
// entry by 8 bytes. That is the whole reason the unit is 9/9 at 100% and still fails
// `flip_test.sh` on the DOL sha1. Retail's copy lives in `auto_06_803A9F4C_rodata.o`, which
// precedes this object in the link order, so naming it resolves without adding a section.
extern "C" const char lbl_803AA230[];

// Must be set *before* any include: `rs_new` is expanded inside
// `TObjOwnerDerivedFromIObj<T>::GetNewDerivedObject` in `Kyoto/IObj.hpp`, which is the second of
// the two `new` sites. See the `CMEMORY_NEW_FILE` branch in `Kyoto/Alloc/CMemory.hpp`.
#define CMEMORY_NEW_FILE lbl_803AA230

#include "MetroidPrime/Factories/CStateMachineFactory.hpp"

#include "MetroidPrime/Enemies/CStateMachine.hpp"

CFactoryFnReturn FAiFiniteStateMachineFactory(const SObjectTag& tag, CInputStream& in,
                                              const CVParamTransfer& params) {
  return rs_new CStateMachine(in);
}
