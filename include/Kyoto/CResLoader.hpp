#ifndef _CRESLOADER
#define _CRESLOADER

#include "types.h"

#include "rstl/list.hpp"
#include "rstl/string.hpp"

#include "Kyoto/IObjectStore.hpp"

class CPakFile;
class CARAMDvdRequest;

struct SResInfo {
  CAssetId x0_id;
  bool x4_compressed : 1;
  int x4_typeIdx; // CFactoryMgr::ETypeTable
  uint x5_offsetDiv32 : 27;
  uint x7_sizeDiv32 : 27;
};

/**
 * One entry of one of `CResLoader`'s four pak lists. **8 bytes, and the layout is not a
 * guess** - three independent retail instructions fix it:
 *
 *  * `fn_802FC378` (0x802FC378) allocates a **16**-byte node (`li r3,16` /
 *    `allocate__Q24rstl17rmemory_allocatorFi` at 0x802fc3a0/0x802fc3a8) and then copies
 *    exactly the two words at `+8` and `+0xC` of it from the caller's 8 bytes
 *    (`stb` 0x802fc3c4, `stw` 0x802fc3cc). 16 - 8 = 8, so the item is 8 bytes.
 *  * `fn_802FCFF4` (0x802fcff4) reads the **byte** at `+0` of the item to choose which list
 *    to move the entry into (`lbz r0,40(r5)` where `r5` is `*(item+4)`, at 0x802fd000), so
 *    `+0` is a one-byte flag and not a pointer.
 *  * `CResLoader::GetPakFile` (0x802fba68) walks `idx` nodes with `lwz rX,4(rX)` and returns
 *    `lwz r3,12(r5)` (0x802fbae4), so the value it hands back is the item's **second** word.
 *
 * The flag is the `AddPakFileAsync` handshake: the caller writes it, the insert clears it
 * (`stb r0,0(r30)` with `r0 = 0`, 0x802fc3d0), and the caller reads it back to decide whether
 * to drop its own `CPakFile` (`src/Kyoto/CResLoaderAddPakFileAsync.cpp`).
 */
struct SPakLoadEntry {
  bool x0_inList;
  CPakFile* x4_pak;
};

// The port's own copies of the two retail helpers `src/Kyoto/CResLoaderPakPump.cpp` defines
// under `TARGET_PC`, because both are unnamed in `config/G2ME01/symbols.txt` and a `Matching`
// unit may not own their bytes. They are declared here, at namespace scope and with C linkage,
// because mwcceppc rejects `friend extern "C"` (it reads the `extern` as a storage class) and
// GCC rejects a friend declaration that does not match the linkage of the definition.
extern "C" void* fn_802FCFF4(void* resLoader, void* entry);
extern "C" void* fn_802FD174(void* list, void* node);

class CResLoader {
public:
  int GetPakCount() const;
  CPakFile& GetPakFile(int idx) const;
  void AddPakFileAsync(const rstl::string&, bool, bool);
  void AsyncIdlePakLoading();
  bool AreAllPaksLoaded() const;
  CInputStream* LoadNewResourceSync(const SObjectTag& tag, char* extBuf);
  CInputStream* LoadNewResourceSync(const SObjectTag& tag, int, int, char* extBuf);
  CARAMDvdRequest* LoadResourcePartAsync(const SObjectTag& tag, int, int, char*);

  FourCC GetResourceTypeById(CAssetId) const;
  uint ResourceSize(const SObjectTag& tag) const;

private:
  /**
   * Four `rstl::list< SPakLoadEntry >`, **0x18 bytes each, at +0x00, +0x18, +0x30 and +0x48**,
   * so the lists are 0x60 of the loader's 0x70. Measured, not inferred:
   *
   *  * `rstl::list` puts its count at `+0x14`, and three retail functions read a count at
   *    exactly those three places: `GetPakCount` is `lwz r4,44(r3)` + `lwz r0,68(r3)`
   *    (0x802fbc60/0x802fbc64) = `x18.x14_count + x30.x14_count`; and `fn_802FCCE4` -
   *    `AreAllPaksLoaded` - is `lwz r0,92(r3)` (0x802fcce4) = `x48.x14_count`.
   *  * `fn_802FC378` reads `x4_start` at `+4`, `x8_end` at `+8` and `x14_count` at `+0x14`
   *    of the list `AddPakFileAsync` passes as `&x48_pakLoadingList` (0x802fc3d4, 0x802fc360,
   *    0x802fc3f4), and `fn_802FD174` decrements the same `+0x14` after unlinking a node
   *    (0x802fd1e8/0x802fd1f4) - the identical word `AreAllPaksLoaded` reads. That is what
   *    makes `x48` a list and not the four scalars this header used to declare.
   *  * `fn_802FCFF4` (0x802fcff4) picks between `this+0x18` and `this+0x30` on the entry's
   *    **ARAM-file** bit - `rlwinm. r0,r0,26,31,31` at 0x802fd008, which is bit field 25 of
   *    `CPakFile`'s flag byte, i.e. `x28_aramFile` and *not* `x28_worldPak` (field 26, read
   *    with `rlwinm ...,27,31,31`, which is what `EnsureWorldPakReady` at 0x8032309c uses).
   *    So `+0x18` is the ARAM-file paks and `+0x30` the ordinary ones.
   *  * `GetPakFile` reads `this+0x1C` when `idx < *(this+0x2C)` and `this+0x34` otherwise
   *    (0x802fba78/0x802fbaf0), i.e. the `x4_start` of the `+0x18` and the `+0x30` list.
   *
   * `x0_aramList` is the fourth; nothing in the pak chain reaches it, and it is here because
   * the other three are at 0x18 strides from it. Four unnamed words follow the four lists, which
   * is what makes the loader 0x70 - see them below.
   */
  rstl::list< SPakLoadEntry > x0_aramList;         // +0x00, count at +0x14
  rstl::list< SPakLoadEntry > x18_aramFileList;    // +0x18, count at +0x2C
  rstl::list< SPakLoadEntry > x30_pakList;         // +0x30, count at +0x44
  rstl::list< SPakLoadEntry > x48_pakLoadingList;  // +0x48, count at +0x5C

  // Four unnamed words, so `CResLoader` is **0x70** bytes. This is measured twice over and the
  // two measurements agree, and **neither of them is the constructor's next call** - that
  // inference is wrong and is recorded as wrong in `docs/research/paks.md`. What fixes the size is
  // that `CResFactory::CResFactory` (retail `fn_802FB154`, 0x802FB154) builds this member at its
  // own `+0x04` (`addi r3,r31,4` / `bl 802fd0f4` at 0x802FB17C/0x802FB184) and then builds
  // `CFactoryMgr` at its own `+0x74` (`addi r3,r31,116` / `bl 802f98d0` at 0x802FB188/0x802FB18C)
  // with nothing in between, and that `AddPaksAndFactories` reaches the manager as
  // `gpResourceFactory`+0x74. The four lists are 0x60 and these four words are the other 0x10.
  // See the note on `x74_factoryMgr` in `Kyoto/CResFactory.hpp`, and the adjudication and third
  // correction in `docs/research/paks.md`.
  uint x60_;
  uint x64_;
  uint x68_;
  uint x6c_;

  // The two port-side helpers declared just above the class reach the lists directly, so they
  // are friends rather than the members being made public. Plain friend declarations, with
  // the C linkage already fixed by the namespace-scope declarations.
  friend void* fn_802FCFF4(void* resLoader, void* entry);
  friend void* fn_802FD174(void* list, void* node);
};
CHECK_SIZEOF(CResLoader, 0x70)

#endif // _CRESLOADER
