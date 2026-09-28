#include "Kyoto/CFactoryFunctions.hpp"

// The port's bodies for the 33 factory functions `CGameGlobalObjects::AddPaksAndFactories`
// registers and that retail leaves unnamed. **This file is port-only**: `configure.py` does not
// declare it, so mwcceppc never compiles it and it cannot affect `main.dol` or any of the 86 REL
// modules. It exists because `src/MetroidPrime/main.cpp` *is* in the port build, and the
// registration block written there calls all 36.
//
// It is not a decompilation and it does not claim to be one. **Each body returns an empty
// `CFactoryFnReturn`**, which is the honest minimum: a resource factory's whole job is
// `operator new(sizeof(T))`, `T(stream)`, and hand the result back, and for 33 of the 36 the
// resource class `T` does not exist in this tree and its `operator new` size is the only part of
// the factory that is knowable from here. Returning nothing is a *correct* failure - a resource
// load reports "no object" - and it is strictly better than a link error, because the alternative
// is leaving 864 bytes of retail's boot path unwritten.
//
// The sizes, so a lane writing the real bodies does not have to re-derive them. Each is the
// `li r3, N` at the factory's own address; `new == 0` means retail's factory does not allocate
// at all (those seven build into storage the transfer already owns - a 32-byte frame, no
// `operator new`, and a call to `fn_80031D98` on the way out):
//
//   TXTR 0x68   CSKR 0x28   ANIM 0x9C   CINF 0x7C   ANCS 0x7C   CRSC 0x38
//   SWHC  -     PART  -     ELSC  -     SPSC  -     SRSC  -     WPSC  -
//   FRME 0x334  FONT 0x94   SCAN 0x1A4  AFSM 0x20   FSM2 0x40   DCLN 0x38
//   DPSC  -     ATBL 0x10   MAPW 0x44   MAPA 0x7C   MAPU 0x30   CSNG 0x10
//   SAVW 0x84   HINT 0x10   CSPP 0x20   PTLA 0x50   STLC 0x10   EGMC 0x10
//   CMDL 0x34   AGSC 0x18   PATH 0x1EC  (the three owner-keyed ones)
//
// The other three of the 36 - `FStringTableFactory`, `FDependencyGroupFactory` and
// `FRuleSetFactory` - are the real factories, out of `src/Kyoto/Text/CStringTableFactory.cpp`,
// `src/Kyoto/CDependencyGroup.cpp` and `src/MetroidPrime/CRuleSet.cpp`, and nothing here defines
// them.

#ifndef __MWERKS__

// `FStringTableFactory` is the 34th of the 36 and the only other one retail names. Its real
// body is in `src/Kyoto/Text/CStringTable.cpp`, which the port build does not compile
// (files.cmake excludes it for two pointer-to-`uint` casts that lose precision on a 64-bit
// host), so without a body here the port asks for
// `_Z19FStringTableFactoryRK10SObjectTagR12CInputStreamRK15CVParamTransfer` and the gap grows
// by one. Measured, and the reason the exclusion cannot simply be lifted.
CFactoryFnReturn FStringTableFactory(const SObjectTag&, CInputStream&, const CVParamTransfer&) {
  return CFactoryFnReturn();
}

#define PORT_FACTORY(sym)                                                                \
  extern "C" CFactoryFnReturn sym(const SObjectTag&, CInputStream&,                     \
                                  const CVParamTransfer&) {                             \
    return CFactoryFnReturn();                                                          \
  }

#define PORT_FACTORY_OWNER(sym)                                                         \
  extern "C" CFactoryFnReturn sym(const SObjectTag&, const rstl::auto_ptr< uchar >&, int, \
                                  const CVParamTransfer&) {                             \
    return CFactoryFnReturn();                                                          \
  }

PORT_FACTORY(fn_802C4878) // TXTR
PORT_FACTORY(fn_8030FEAC) // CSKR
PORT_FACTORY(fn_802B3200) // ANIM
PORT_FACTORY(fn_802AC2D8) // CINF
PORT_FACTORY(fn_8028E7BC) // ANCS
PORT_FACTORY(fn_8025DD1C) // CRSC
PORT_FACTORY(fn_802ED864) // SWHC
PORT_FACTORY(fn_802E7A78) // PART
PORT_FACTORY(fn_8031B4D8) // ELSC
PORT_FACTORY(fn_8032B5DC) // SPSC
PORT_FACTORY(fn_8032F0D4) // SRSC
PORT_FACTORY(fn_8025DB38) // WPSC
PORT_FACTORY(fn_80274FD4) // FRME
PORT_FACTORY(fn_802B514C) // FONT
PORT_FACTORY(fn_80110B18) // SCAN
PORT_FACTORY(fn_8019405C) // AFSM
PORT_FACTORY(fn_801FD314) // FSM2
PORT_FACTORY(fn_80254414) // DCLN
PORT_FACTORY(fn_802601D0) // DPSC
PORT_FACTORY(fn_8029AB80) // ATBL
PORT_FACTORY(fn_80093638) // MAPW
PORT_FACTORY(fn_8007E32C) // MAPA
PORT_FACTORY(fn_801545F0) // MAPU
PORT_FACTORY(fn_80314DC4) // CSNG
PORT_FACTORY(fn_80182830) // SAVW
PORT_FACTORY(fn_8017F988) // HINT
PORT_FACTORY(fn_8028B1F4) // CSPP
PORT_FACTORY(fn_80255600) // PTLA
PORT_FACTORY(fn_802FF4BC) // STLC
PORT_FACTORY(fn_801EF598) // EGMC

PORT_FACTORY_OWNER(fn_80311340) // CMDL
PORT_FACTORY_OWNER(fn_80307544) // AGSC
PORT_FACTORY_OWNER(fn_8013FDB8) // PATH

#undef PORT_FACTORY
#undef PORT_FACTORY_OWNER

#endif // !__MWERKS__
