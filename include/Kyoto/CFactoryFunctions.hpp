#ifndef _CFACTORYFUNCTIONS
#define _CFACTORYFUNCTIONS

#include "Kyoto/CFactoryMgr.hpp"
#include "Kyoto/CVParamTransfer.hpp"
#include "Kyoto/SObjectTag.hpp"
#include "Kyoto/Streams/CInputStream.hpp"

/**
 * The 36 factory functions `CGameGlobalObjects::AddPaksAndFactories` registers
 * (retail 0x80007504-0x80007864, 864 bytes, 24 bytes per entry).
 *
 * **All 36 return `CFactoryFnReturn` through a hidden first argument**, because
 * `CFactoryFnReturn` holds an `rstl::auto_ptr` and so cannot come back in registers. That is
 * measured from the two dispatch sites, which are the only callers of these function pointers:
 *
 * ```
 *   fn_802F94D8  (the FourCC-keyed table)   0x802F9528  lwz r12,20(r3)
 *                                            0x802F952C  addi r3,r1,12
 *                                            0x802F9530  mtctr r12 / bctrl
 *        -> four words in r3, r4, r5, r6: out, tag, stream, transfer
 *   fn_802F8EB0  (the owner-keyed table)    0x802F8FD8  mr  r12,r29
 *                                            0x802F8FDC  mr  r4,r31   ... 0x802F8FE4 mr r7,r28
 *                                            0x802F8FEC  addi r5,r1,112
 *                                            0x802F8FF0  mtctr r12 / bctrl
 *        -> five words: out, tag, transfer, stream, owner
 * ```
 *
 * So the FourCC-keyed table holds `FFactoryFunc` (`Kyoto/CFactoryMgr.hpp`), and 33 of the 36 use
 * it. The second table is upstream's `CFactoryMgr::mMemFactories`, holding `FMemFactoryFunc`
 * (tag, buffer, size, transfer - the five words above, read by upstream as a memory factory
 * rather than the "owner" argument this header used to name); only CMDL, AGSC and PATH are
 * registered through it. **Superseded 2026-09-28** by the upstream merge: the older reading
 * (transfer, stream, owner) is gone.
 *
 * **Why these are declared and not defined here.** Every one of the 36 lives in the DOL and its
 * bytes belong to retail, so the mwcceppc build must reference them and let dtk's object supply
 * them - the same "a constructor declared, not defined" arrangement
 * `docs/research/missing_classes.md` works to end to end. Three are named in retail and are
 * ordinary declarations: `FStringTableFactory` (0x80312320, a `Matching` unit at
 * `src/Kyoto/Text/CStringTableFactory.cpp`), `FDependencyGroupFactory` (0x80320B6C) and
 * `FRuleSetFactory` (0x801F6AE4). The other 33 are `fn_<address>` in
 * `config/G2ME01/symbols.txt`, so they take C linkage, which is what makes the emitted symbol
 * name retail's spelling verbatim - the mechanism `src/MetroidPrime/PortLinkStubs.cpp` uses, and
 * the reason no rename in `symbols.txt` is needed and no REL module can be disturbed. The
 * parameters are typed rather than `void*` so the registration block type-checks; C linkage does
 * not change the ABI, and in particular not the hidden return slot.
 *
 * **The port build needs bodies for all 36**, because `main.cpp` is in `files.cmake` and calls
 * them through the registration block. Those bodies are in
 * `src/Kyoto/CFactoryFunctionsPort.cpp`, which `configure.py` never claims and so mwcceppc never
 * compiles. They are not a decompilation claim: each news up one resource of retail's measured
 * size and hands it back, and none of those resource classes exists in this tree yet, so the
 * bodies are behind `#ifndef __MWERKS__`.
 */

CFactoryFnReturn FStringTableFactory(const SObjectTag& tag, CInputStream& in,
                                     const CVParamTransfer& xfer);
CFactoryFnReturn FDependencyGroupFactory(const SObjectTag& tag, CInputStream& in,
                                         const CVParamTransfer& xfer);
CFactoryFnReturn FRuleSetFactory(const SObjectTag& tag, CInputStream& in,
                                 const CVParamTransfer& xfer);

// FourCC-keyed, 33 entries, in retail's registration order.
extern "C" CFactoryFnReturn fn_802C4878(const SObjectTag&, CInputStream&, const CVParamTransfer&); // TXTR
extern "C" CFactoryFnReturn fn_8030FEAC(const SObjectTag&, CInputStream&, const CVParamTransfer&); // CSKR
extern "C" CFactoryFnReturn fn_802B3200(const SObjectTag&, CInputStream&, const CVParamTransfer&); // ANIM
extern "C" CFactoryFnReturn fn_802AC2D8(const SObjectTag&, CInputStream&, const CVParamTransfer&); // CINF
extern "C" CFactoryFnReturn fn_8028E7BC(const SObjectTag&, CInputStream&, const CVParamTransfer&); // ANCS
extern "C" CFactoryFnReturn fn_8025DD1C(const SObjectTag&, CInputStream&, const CVParamTransfer&); // CRSC
extern "C" CFactoryFnReturn fn_802ED864(const SObjectTag&, CInputStream&, const CVParamTransfer&); // SWHC
extern "C" CFactoryFnReturn fn_802E7A78(const SObjectTag&, CInputStream&, const CVParamTransfer&); // PART
extern "C" CFactoryFnReturn fn_8031B4D8(const SObjectTag&, CInputStream&, const CVParamTransfer&); // ELSC
extern "C" CFactoryFnReturn fn_8032B5DC(const SObjectTag&, CInputStream&, const CVParamTransfer&); // SPSC
extern "C" CFactoryFnReturn fn_8032F0D4(const SObjectTag&, CInputStream&, const CVParamTransfer&); // SRSC
extern "C" CFactoryFnReturn fn_8025DB38(const SObjectTag&, CInputStream&, const CVParamTransfer&); // WPSC
extern "C" CFactoryFnReturn fn_80274FD4(const SObjectTag&, CInputStream&, const CVParamTransfer&); // FRME
extern "C" CFactoryFnReturn fn_802B514C(const SObjectTag&, CInputStream&, const CVParamTransfer&); // FONT
extern "C" CFactoryFnReturn fn_80110B18(const SObjectTag&, CInputStream&, const CVParamTransfer&); // SCAN
extern "C" CFactoryFnReturn fn_8019405C(const SObjectTag&, CInputStream&, const CVParamTransfer&); // AFSM
extern "C" CFactoryFnReturn fn_801FD314(const SObjectTag&, CInputStream&, const CVParamTransfer&); // FSM2
extern "C" CFactoryFnReturn fn_80254414(const SObjectTag&, CInputStream&, const CVParamTransfer&); // DCLN
extern "C" CFactoryFnReturn fn_802601D0(const SObjectTag&, CInputStream&, const CVParamTransfer&); // DPSC
extern "C" CFactoryFnReturn fn_8029AB80(const SObjectTag&, CInputStream&, const CVParamTransfer&); // ATBL
extern "C" CFactoryFnReturn fn_80093638(const SObjectTag&, CInputStream&, const CVParamTransfer&); // MAPW
extern "C" CFactoryFnReturn fn_8007E32C(const SObjectTag&, CInputStream&, const CVParamTransfer&); // MAPA
extern "C" CFactoryFnReturn fn_801545F0(const SObjectTag&, CInputStream&, const CVParamTransfer&); // MAPU
extern "C" CFactoryFnReturn fn_80314DC4(const SObjectTag&, CInputStream&, const CVParamTransfer&); // CSNG
extern "C" CFactoryFnReturn fn_80182830(const SObjectTag&, CInputStream&, const CVParamTransfer&); // SAVW
extern "C" CFactoryFnReturn fn_8017F988(const SObjectTag&, CInputStream&, const CVParamTransfer&); // HINT
extern "C" CFactoryFnReturn fn_8028B1F4(const SObjectTag&, CInputStream&, const CVParamTransfer&); // CSPP
extern "C" CFactoryFnReturn fn_80255600(const SObjectTag&, CInputStream&, const CVParamTransfer&); // PTLA
extern "C" CFactoryFnReturn fn_802FF4BC(const SObjectTag&, CInputStream&, const CVParamTransfer&); // STLC
extern "C" CFactoryFnReturn fn_801EF598(const SObjectTag&, CInputStream&, const CVParamTransfer&); // EGMC

// Owner-keyed, 3 entries, and the only three that use `fn_802F963C`.
extern "C" CFactoryFnReturn fn_80311340(const SObjectTag&, const rstl::auto_ptr< uchar >&, int,
                                         const CVParamTransfer&); // CMDL
extern "C" CFactoryFnReturn fn_80307544(const SObjectTag&, const rstl::auto_ptr< uchar >&, int,
                                         const CVParamTransfer&); // AGSC
extern "C" CFactoryFnReturn fn_8013FDB8(const SObjectTag&, const rstl::auto_ptr< uchar >&, int,
                                         const CVParamTransfer&); // PATH

#endif // _CFACTORYFUNCTIONS
