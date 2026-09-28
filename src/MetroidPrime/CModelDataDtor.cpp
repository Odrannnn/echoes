#include "MetroidPrime/CModelData.hpp"
#include "MetroidPrime/CAnimData.hpp"

// `CModelData::~CModelData()` - retail `__dt__10CModelDataFv`, 0x800E6810, 0xF0 = 240 bytes
// (the next symbol is `fn_800E6900`, so 0xF0 is retail's own size, not a gap-to-next).
//
// **Port-only file**: it is in `files.cmake` and in no `configure.py` unit, because no split in
// `config/G2ME01/splits.txt` covers 0x800E6810 - measured over the file's 719 `.text` ranges,
// the neighbours are `0x800E5D20..0x800E5DE8` and `0x800E6AD0..0x800E6B68`, so the destructor
// sits in an unclaimed gap and there is no unit for it to be `Matching` in. What it closes is
// the port's link: `_ZN10CModelDataD1Ev`, referenced by CActor.cpp.o, CPlayer.cpp.o,
// CScriptCannonBall.cpp.o, CScriptPickup.cpp.o and CScriptSkyRipple.cpp.o.
//
// ## The body is empty because retail's is - and that is measured, not assumed
//
// `tools/dis.sh 0x800E6810 0xF0` spends every instruction on a member. Four guarded blocks in
// **reverse declaration order**, then the deleting tail:
//
//   0x800E6830  lbz r0,72(r30)  ... li r4,0;  bl __dt__6CTokenFv   x3c_infraModel, m_valid +0x48
//   0x800E6864  lbz r0,56(r30)  ... li r4,0;  bl __dt__6CTokenFv   x2c_xrayModel,  m_valid +0x38
//   0x800E6890  lbz r0,40(r30)  ... li r4,0;  bl __dt__6CTokenFv   x1c_normalModel, m_valid +0x28
//   0x800E68BC  lbz r0,12(r30)  lwz r3,16(r30); li r4,1; bl fn_8002C340   xc_animData, +0x0C
//   0x800E68D4  extsh. r0,r31; ble; bl Free__7CMemoryFPCv                deleting path, r4 > 0
//
// Each block reads the member's own valid flag and calls that member's destructor and nothing
// else, and no flag is written back - a body statement would leave a store behind, and there is
// none. This is what an empty destructor compiles to over these members, so the emptiness here
// is retail's behaviour rather than an omission.
//
// ## What the real one must free, member by member
//
//  * `x3c_infraModel`, `x2c_xrayModel`, `x1c_normalModel` - three
//    `rstl::optional_object< TLockedToken< CModel > >`. `optional_object::~optional_object()`
//    (include/rstl/optional_object.hpp) tests `m_valid` and `rstl::destroy`s the slot; the token
//    is `TLockedToken` -> `TToken<CModel>` -> `CToken::~CToken()`
//    (src/Kyoto/CToken.cpp:21), which unlocks and `RemoveRef`s the `CObjectReference` - that is
//    the model token released. Retail's three calls are `__dt__6CTokenFv`, non-deleting.
//  * `xc_animData` - `rstl::auto_ptr< CAnimData >`. `auto_ptr::~auto_ptr()`
//    (include/rstl/auto_ptr.hpp) tests `x0_has` and does `delete x4_item`, which is retail's
//    `bl fn_8002C340` with `r4 = 1`. **`fn_8002C340` is `CAnimData::~CAnimData`**: the member
//    ladder at the bottom of include/MetroidPrime/CAnimData.hpp fixes that identity from that
//    function's own call sites, one destructor call per member. So the anim data's members are
//    what this line frees, and they are not this file's to write - see the `NEW:` line in
//    build/goal/notes/port-modeldata-dtor.md.
//  * `this`, when the caller passes the deleting flag (`r4 > 0`): `operator delete`, which
//    include/Kyoto/Alloc/CMemory.hpp:46 defines as `CMemory::Free` - retail's
//    `Free__7CMemoryFPCv` at the same point in the tail.
//
// `x0_scale`, `x14_flags` and `x18_ambientColor` have trivial destructors; retail spends no
// instruction on them and neither does this.
//
// ## Why CAnimData.hpp is included
//
// `MetroidPrime/CModelData.hpp` forward-declares `CAnimData` only. A `delete` on an incomplete
// type still compiles - it drops the destructor call and frees the block - so without this
// include the symbol would close while the anim data leaked, which is the plausible lie this
// repo rejects: retail calls `CAnimData::~CAnimData` at 0x800E68D0, and this include is what
// makes the port emit that call. The cost is that the port's undefined list gains
// `_ZN9CAnimDataD1Ev` (nothing in the tree defines it; include/MetroidPrime/CAnimData.hpp:43 is
// the only declaration) in exchange for losing `_ZN10CModelDataD1Ev`: net 0 on the count, and
// the dependency is retail's own.

CModelData::~CModelData() {}
