/**
 * `CEnvFxManager::Initialize` - retail `.text:0x80166880`, `size:0xEC` = 236 bytes, 32-byte frame,
 * r28-r31 saved. The fourth statement of `CGameGlobalObjects::PostInitialize`
 * (`src/MetroidPrime/main.cpp`), and a plain carve: nothing else claims 0x80166880..0x8016696C.
 *
 * ```
 * 80166888:  lis/addi r4,lbl_803A96FC            <- "DUMB_SnowForces", offset 0 of the pool
 * 801668A4:  lwz r3,gpResourceFactory ; vtable +0x1C ; bctrl
 *                                                <- slot 7 = IFactory::GetResourceIdByName
 * 801668B8:  lwz r6,gpResourceFactory ; mr r4,r3 ; li r5,0 ; addi r3,r6,4 ; bl fn_802FC63C
 *                                                <- GetResLoader().LoadNewResourceSync(*tag, 0)
 * 801668CC:  neg/or/srwi -> stb 8(r1) ; stw r3,12(r1)
 *                                                <- rstl::auto_ptr< CInputStream > at r1+8
 * 801668EC:  256 x (2 x ReadFloat -> stfs, +4), +8 <- lbl_803DABE0, .bss, size:0x800
 * 80166920:  lbz 8(r1) ; lwz 12(r1) ; vtable +0x8 with r4=1
 *                                                <- the auto_ptr's destructor, `delete` via
 *                                                   ~CInputStream's deleting slot
 * ```
 *
 * Both retail objects are referenced, not defined: `lbl_803A96FC` is a 0x94-byte `.rodata` string
 * pool that 0x80166980 (the next function, not this unit's) also reads, and `lbl_803DABE0` is a
 * `.bss` array no unit claims. A literal here would put a `.rodata` section in this object that
 * retail's range does not have. The port build has neither, so it defines both below.
 */
#include "MetroidPrime/CEnvFxManager.hpp"

#include "rstl/auto_ptr.hpp"

#include "Kyoto/CResFactory.hpp"
#include "Kyoto/CResLoader.hpp"
#include "Kyoto/Streams/CInputStream.hpp"

#ifdef TARGET_PC
// Only the first string of retail's pool: nothing in the port reads past it yet. If a port unit
// ever needs `lbl_803A96FC + n`, the whole 0x94 bytes belong in `PortGlobals.cpp` instead.
extern "C" const char lbl_803A96FC[] = "DUMB_SnowForces";
extern "C" float lbl_803DABE0[256][2];
float lbl_803DABE0[256][2];
#else
extern "C" const char lbl_803A96FC[];
extern "C" float lbl_803DABE0[256][2];
#endif

void CEnvFxManager::Initialize() {
  const SObjectTag* tag = gpResourceFactory->GetResourceIdByName(lbl_803A96FC);
  rstl::auto_ptr< CInputStream > stream(static_cast< CInputStream* >(
      fn_802FC63C(&gpResourceFactory->GetResLoader(), *tag, nullptr)));
  for (int i = 0; i < 256; ++i) {
    for (int j = 0; j < 2; ++j) {
      lbl_803DABE0[i][j] = stream->ReadFloat();
    }
  }
}
