#ifndef _CCHARACTERINFO
#define _CCHARACTERINFO

#include "types.h"

#include "Kyoto/Animation/CEffectComponent.hpp"

#include "Kyoto/Animation/CPASDatabase.hpp"
#include "Kyoto/IObjectStore.hpp"
#include "Kyoto/Math/CAABox.hpp"

#include "rstl/pair.hpp"
#include "rstl/string.hpp"
#include "rstl/vector.hpp"

// One element of `CCharacterInfo::xe8_unk`. Retail's element is 0x1C bytes, copied a word at a
// time (`mulli r3,r0,28` in the copy constructor `fn_8002DF1C`, the word-copy tail advancing
// `addi r6,r6,28`), so it is trivially copyable: seven longs and nothing else. The *field* names are
// not recoverable - nothing in the binary compares any of them against a string, and the stream
// reader (`CCharacterInfo::CCharacterInfo(CInputStream&)`, `fn_8029269C`) only stores them - and
// **the type's name is this tree's, not retail's.** A struct is spelled out rather than
// `uint[7]` because the 0x1C element size is the thing being asserted.
struct CAnimIdEntry {
  uint x0;
  uint x4;
  uint x8;
  uint xc;
  uint x10;
  uint x14;
  uint x18;
};
CHECK_SIZEOF(CAnimIdEntry, 0x1c)

// **Every offset below is retail's, for the G2ME01 (Echoes) binary, and every one is pinned by a
// named instruction; the evidence table is at the bottom of this file. Do not "fix" a member from
// the output of mwcceppc - the host is 64-bit and MWCC is 32-bit, so a host `sizeof` measures our
// layout, not retail's.** `CHECK_SIZEOF(..., 0xc0)` was the **Metroid Prime 1** size: this header
// came from the Prime 1 tree unchanged, and Echoes grew the class by 0x38.
class CCharacterInfo {
public:
  // **Six** `vector<CAssetId>`, not four. Retail's destructor `fn_8002CAC0` calls
  // `fn_8000917C` (`vector<Ui>::~vector()`) six times, at +0x00, +0x10, +0x20, +0x30, +0x40 and
  // +0x50; the copy constructor `fn_8002E310` calls `fn_8002E0EC` (`vector<Ui>`'s copy
  // constructor) at the same six offsets; and the stream constructor `fn_80293454` calls
  // `fn_80056098` - which is `read_vector_uint(CInputStream&, vector<Ui>&)`: it `reserve<Ui>`s the
  // count and reads that many longs - six times. So the class is **0x60**, not 0x40.
  //
  // Retail names none of them. The Prime 1 tree has names for four, but they come from that tree's
  // own source rather than from a retail symbol, and they contradict each other: `x10_swhc` is
  // swoosh according to the only consumer (`CParticleDatabase::CacheParticleDesc` feeds it to
  // `x14_swooshDescs`, a `map<CAssetId, rc_ptr<TLockedToken<CSwooshDescription>>>`), while that same
  // tree's constructor parameter list calls the *fourth* one `swoosh`. The two Echoes added have no
  // name at all. The four existing names are left exactly as they are - including the duplicated
  // `elsc`, which is a real defect in the name but not something retail can adjudicate - and the
  // two new ones are left unnamed.
  //
  // The stream constructor reads them in **this** order, which is the only positional evidence
  // there is: +0x00, +0x10 and +0x50 unconditionally, +0x20 only when the ANCS version > 5, and
  // +0x30 and +0x40 only when it is > 8. So +0x20 keeps Prime 1's `ver > 5` gate, +0x30 and +0x40
  // are new in Echoes, and the field Prime 1 had at +0x30 is the one now at +0x50. That last step
  // is an inference from the read order, not an instruction, so it is recorded here and **not**
  // written into a member name.
  class CParticleResData {
  private:
    rstl::vector< CAssetId > x0_part;
    rstl::vector< CAssetId > x10_swhc;
    rstl::vector< CAssetId > x20_elsc;
    rstl::vector< CAssetId > x30_unk;
    rstl::vector< CAssetId > x40_unk;
    rstl::vector< CAssetId > x50_unk;
  };

  const CPASDatabase& GetPASDatabase() const { return x30_pasDatabase; }
  const rstl::vector< rstl::pair< rstl::string, CAABox > >& GetAnimBBoxList() const {
    return xa8_aabbs;
  }
  const rstl::vector< rstl::pair< rstl::string, rstl::vector< CEffectComponent > > >&
  GetEffectList() const {
    return xb8_effects;
  }
  // Prime 1 spells the element `pair<int, pair<string, string>>` (0x24, with the animation name at
  // +0x14 - `GetAnimationIndex` compares `element + 0x14` against its argument). **Echoes' element
  // is 0x14: an int and ONE string**, and the stream reader writes the int at +0 and
  // copy-constructs a single `rstl::string` at +4 (`fn_802933F0` -> `stw r0,0(r5)` then
  // `__ct__string` at `r5+4`). The old type was 0x10 too big per element and had no business
  // being in this class.
  const rstl::vector< rstl::pair< int, rstl::string > >& GetAnimInfoList() const {
    return x20_animInfo;
  }

private:
  // `x0_tableCount` is the **ANCS version**, not a table count: the stream constructor
  // (`fn_8029269C`) gates every member from 0xA8 up on it - `cmplwi r0,1 / 2 / 3 / 4 / 6 / 7 / 9 ;
  // ble` - and passes it to the `CParticleResData` constructor as its `ushort` argument. That is
  // what makes the layout below exact rather than inferred: each member's presence is a version.
  ushort x0_tableCount;                                          //!< 0x000, 0x02
  rstl::string x4_name;                                          //!< 0x004, 0x10
  CAssetId x14_cmdl;                                             //!< 0x014, 0x04
  CAssetId x18_cksr;                                             //!< 0x018, 0x04
  CAssetId x1c_cinf;                                             //!< 0x01C, 0x04
  rstl::vector< rstl::pair< int, rstl::string > > x20_animInfo;  //!< 0x020, 0x10, element 0x14
  CPASDatabase x30_pasDatabase;                                  //!< 0x030, 0x14
  CParticleResData x44_partRes;                                  //!< 0x044, 0x60 (six vectors)
  // 0xA4 is a member of **this** class, not the tail of 0x44: the copy constructor moves 0x44 with
  // one call (`fn_8002E310`) and then does a *bare* `stw r0,164(r30)` for 0xA4. It is read
  // unconditionally from the stream right after `CParticleResData`, so it is an asset id - and
  // Prime 1's `x84_unk` is this same member, which moved +0x20 along with everything else.
  CAssetId xa4_unk;                                              //!< 0x0A4, 0x04
  // element 0x28 = `rstl::string` + six floats at +0x10..+0x24, i.e. a `CAABox`
  rstl::vector< rstl::pair< rstl::string, CAABox > > xa8_aabbs;  //!< 0x0A8, 0x10
  // element 0x20 = `rstl::string` at +0 and a 0x10 sub-object with its own destructor at +0x10
  rstl::vector< rstl::pair< rstl::string, rstl::vector< CEffectComponent > > > xb8_effects; //!< 0x0B8, 0x10
  CAssetId xc8_cmdlOverlay;                                      //!< 0x0C8, 0x04, ANCS > 3
  CAssetId xcc_cksrOverlay;                                      //!< 0x0CC, 0x04, ANCS > 3
  rstl::vector< CAssetId > xd0_animIdxs;                         //!< 0x0D0, 0x10, ANCS > 4
  CAssetId xe0_unk;                                              //!< 0x0E0, 0x04, ANCS > 6
  bool xe4_unk;                                                  //!< 0x0E4, 0x01, ANCS > 7
  uchar xe5_unk[3];                                              //!< 0x0E5 - 0x0E8, nothing writes these
  // 0xE8 is a seventh, Echoes-only table. It is **not** Prime 1's `vector<uint>` - that is 0xD0,
  // which is where this header's `xb0_animIdxs` belongs, +0x20 up with the rest.
  rstl::vector< CAnimIdEntry > xe8_unk;                          //!< 0x0E8, 0x10, ANCS > 9
};

// ## Which of these names are evidence and which are carried over
//
// The **offsets, the sizes and the types** above are all pinned by the instructions in the table
// below. Three of the **names** are not, and are Prime 1's, moved +0x20 with their members:
// `xc8_cmdlOverlay` / `xcc_cksrOverlay` (a pair of adjacent `CAssetId`s, both defaulted to -1 and
// both read from the stream when ANCS > 3 - a pair of *optional* asset ids, which is what
// `GetIceModelId` / `GetIceSkinRulesId` are in Prime 1) and `xd0_animIdxs` (the `vector<CAssetId>`
// that Prime 1 has at 0xB0, `GetAnimationIndexList`). They are kept because they are the tree's
// existing names for those members, not because retail confirms them. `xa4_unk`, `xe0_unk`,
// `xe4_unk` and `xe8_unk` are unnamed in retail and named here only by their offset.

// CHECK_SIZEOF(CCharacterInfo, 0xc0)  // **wrong**: the Metroid Prime 1 size. See the note above.
CHECK_SIZEOF(CCharacterInfo, 0xf8)  // **measured against G2ME01 retail, 2026-09-26**
// `CParticleResData` gets no CHECK_SIZEOF: the macro pastes `cls##_check`, so a nested class
// produces `CCharacterInfo::CParticleResData_check`, which is a member name and will not compile.
// Its 0x60 is asserted by `tools/cchar_probe.cpp` instead.

// ## The ladder, and the instruction that fixes each member
//
// Two functions carry the whole layout, and they agree: the copy constructor
// `fn_8002DE3C` (0x8002DE3C, 0xE0) and the **stream constructor**
// `CCharacterInfo::CCharacterInfo(CInputStream&)` = `fn_8029269C` (0x8029269C, 0x2A0), which is
// reachable from `CInputStream`'s `Get<CCharacterInfo>` helper `fn_80293E18` (0x80293E18) through
// the one-line thunk `fn_80293E84`. The destructor `fn_8002C638` (0x8002C638, 0xB0) walks the
// members with non-trivial destructors in descending order and independently bounds them.
//
// | member | slot | the instruction that fixes it |
// | --- | --- | --- |
// | `x0_tableCount` `ushort` | 0x000 | ctor `sth r0,0(r30)` @0x8002DE64; stream `lhz`/`sth` @0x802926CC |
// | `x4_name` `rstl::string` | 0x004 | `addi r3,r30,4` + `__ct__string` @0x8002DE60/0x8002DE68 |
// | `x14_cmdl` `CAssetId` | 0x014 | `stw r0,20(r30)` @0x8002DE7C |
// | `x18_cksr` `CAssetId` | 0x018 | `stw r5,24(r30)` @0x8002DE84 |
// | `x1c_cinf` `CAssetId` | 0x01C | `stw r0,28(r30)` @0x8002DE88 |
// | `x20_animInfo` `vector<pair<int,string>>` | 0x020 | one dtor call `addi r3,r30,32` @0x8002C6A0; element 0x14 from `mulli r0,r0,20` @0x8002E8EC and the word/string split @0x8002E910/0x8002E920; stream reader stride `mulli r0,r3,20` @0x80293394 |
// | `x30_pasDatabase` `CPASDatabase` | 0x030 | one dtor call `addi r3,r30,48` @0x8002C694; copy `addi r3,r30,48 / bl fn_8002E388` @0x8002DE90 |
// | `x44_partRes` `CParticleResData` | 0x044 | one dtor call `addi r3,r30,68 / bl fn_8002CAC0` @0x8002C688; six sub-vectors @0x8002CAE0..0x8002CB24 and @0x8002E32C..0x8002E368 |
// | `xa4_unk` `CAssetId` | 0x0A4 | **bare** `stw r0,164(r30)` @0x8002DEB4 - not a call, so it is its own member; stream read @0x8029277C |
// | `xa8_aabbs` `vector<pair<string,CAABox>>` | 0x0A8 | dtor `addi r3,r30,168 / bl fn_8002CB54` @0x8002C67C; element 0x28 from `mulli r3,r0,40` @0x8002E238 and the six `stfs` at +0x10..+0x24 @0x8002E2B0..0x8002E2D8 |
// | `xb8_effects` `vector<pair<string,0x10>>` | 0x0B8 | dtor `addi r3,r30,184 / bl fn_80027400` @0x8002C670; element 0x20 from `slwi r3,r0,5` @0x80027688, element walk `addi r31,r31,32` @0x800274E4, +0x10 sub-dtor @0x80027570 |
// | `xc8_cmdlOverlay` `CAssetId` | 0x0C8 | `stw r5,200(r30)` @0x8002DED8; stream default -1 @0x80292798, read when ANCS > 3 @0x80292844 |
// | `xcc_cksrOverlay` `CAssetId` | 0x0CC | `stw r0,204(r30)` @0x8002DEDC; stream default -1 @0x8029279C, read when ANCS > 3 @0x80292858 |
// | `xd0_animIdxs` `vector<CAssetId>` | 0x0D0 | dtor `addi r3,r30,208 / bl fn_8000917C` @0x8002C664; the copy ctor is `fn_8002E0EC`, the `vector<Ui>` one (`slwi r3,r0,2` + `allocate` @0x8002E138) - **not** `fn_8002E1EC`; stream reader `fn_80056098` @0x80292884 |
// | `xe0_unk` `CAssetId` | 0x0E0 | `stw r5,224(r30)` @0x8002DEF4; stream default -1 @0x802927AC, read when ANCS > 6 @0x802928BC |
// | `xe4_unk` `bool` | 0x0E4 | `stb r0,228(r30)` @0x8002DEF8; stream read as one byte when ANCS > 7 @0x802928E8 |
// | `xe8_unk` `vector<CAnimIdEntry>` | 0x0E8 | dtor `addi r3,r30,232 / bl fn_8002CC38` @0x8002C658; element 0x1C from `mulli r3,r0,28` @0x8002DF68 and the word-copy tail `addi r6,r6,28` @0x8002E0BC; stream reader `mulli r3,r3,28` @0x80292AA0 |
// | **end** | 0x0F8 | see the `CAnimData` ladder in `CAnimData.hpp`: one ctor call `addi r3,r25,12` @0x8002D1E8 and one dtor call `addi r3,r30,12` @0x8002C5EC cover 0x00C..0x104 of `CAnimData`, i.e. exactly 0xF8 |
//
// ## Two things that look like evidence and are not
//
// * **`fn_8002E0EC`'s copy loop advances 32 bytes per iteration.** That is an 8-word unroll of a
//   `memcpy`, not a 0x20 element: it runs `count/8` times and then a `count%8` one-word tail. The
//   `allocate(capacity << 2)` at 0x8002E138 and the `fn_80056098` stream reader settle it - 0xD0's
//   element is **4** bytes. Reading the stride would have put a 0x20 element in 0xD0 and made the
//   class 0xF0 instead of 0xF8.
// * **`CHECK_SIZEOF` proves nothing about a size.** `check_sizeof<T, n>` passes for any `n`, so
//   `CHECK_SIZEOF(CCharacterInfo, 0xc0)` sat here happily for however long the real class was
//   0xF8. A `Matching` unit is no better evidence: `CModelDataModelSlots.cpp` is `Matching` at
//   100.00% and reads `CAnimData` through a local duplicate shape, so it is layout-immune and
//   survived a 0x78-byte error in the header. The size is established from retail's own stores
//   and from the span retail's constructor and destructor cover, or it is not established.
//
// ## Where the 0x38 went, and why nothing else moved
//
// The class was 0x38 short for two independent reasons, and both are fixed above:
//
//   * `CParticleResData` is **six** `vector<CAssetId>` (0x60), not four (0x40) - **+0x20**. Prime
//     1's `.ANCS` has four such arrays; Echoes' has six.
//   * Echoes added three members above Prime 1's last one - `CAssetId` @0xE0 (ANCS > 6), `bool`
//     @0xE4 (ANCS > 7) and `vector<CAnimIdEntry>` @0xE8 (ANCS > 9) - worth 0x10, and the
//     `CAnimData` member at 0xA4 that Prime 1 had at 0x84 is a member in its own right rather
//     than padding - **+0x18**.
//
// `sizeof(CAnimData)` is **unchanged at 0x5B8**: `xcc_unk[0x38]` in `CAnimData` was standing in
// for exactly this, so removing it and growing `CCharacterInfo` by 0x38 cancel. Measured with
// mwcceppc by `tools/cchar_probe.cpp`, which asserts all 19 member offsets, all 7
// `CParticleResData` offsets and 10 member sizes against a duplicated retail table:
//
//   tools/probe_cc.sh tools/cchar_probe.cpp /tmp/cchar_probe.o
//   build/binutils/powerpc-eabi-objdump -s -j .data /tmp/cchar_probe.o
//
// Both includers of this header - `src/MetroidPrime/CActor.cpp` and
// `src/MetroidPrime/Player/CPlayerGun.cpp` - compile to **byte-identical objects** before and
// after, and so does the `Matching` unit that reads `CAnimData`,
// `src/MetroidPrime/CModelDataModelSlots.cpp`. Nothing in either of them reads a
// `CCharacterInfo` member, which is why a 0x38-byte error could sit here unnoticed.
#endif // _CCHARACTERINFO
