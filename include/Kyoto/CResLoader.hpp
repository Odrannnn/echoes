#ifndef _CRESLOADER
#define _CRESLOADER

#include "types.h"

#include "rstl/list.hpp"
#include "rstl/string.hpp"

#include "Kyoto/IObjectStore.hpp"

// `CPakFile::SResInfo` is the type of `CResLoader`'s current-resource cursor at +0x68, and a
// forward declaration cannot name a nested class, so this header needs the class itself.
// `Kyoto/CPakFile.hpp` does not reach back to this one - its own includes are `CDvdFile.hpp`,
// `IObjectStore.hpp` and the `rstl` trio - so there is no cycle.
//
// **The namespace-scope `struct SResInfo` this header used to declare is gone.** It was a
// different type from `CPakFile::SResInfo` with the same name - `CAssetId` plus a hand-rolled
// bitfield expansion of the 7 data bytes - and it had **no user anywhere in the tree**
// (`grep -rn 'x4_compressed\|x4_typeIdx\|x5_offsetDiv32\|x7_sizeDiv32' src/ include/` returns
// nothing outside this header). It was a hazard the moment this include went in, because an
// unqualified `SResInfo` in any translation unit that includes this header would silently pick
// the wrong one.
#include "Kyoto/CPakFile.hpp"

class CARAMDvdRequest;

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
 * The flag is the `AddPakFileAsync` handshake: the caller writes it, the copy below clears it
 * (`stb r0,0(r30)` with `r0 = 0`, 0x802fc3d0), and the caller reads it back to decide whether
 * to drop its own `CPakFile` (`src/Kyoto/CResLoaderAddPakFileAsync.cpp`).
 *
 * **The copy constructor is load-bearing, and it is retail's own** - it is what the `stb
 * r0,0(r30)` at 0x802fc3d0 *is*. `fn_802FC378` is `rstl::list< SPakLoadEntry >::
 * do_insert_before(node*, const SPakLoadEntry&)` and nothing else, and its 0xA8 bytes are
 * **identical, instruction for instruction and register for register, to retail's
 * `do_insert_before__Q24rstl70list<Q24rstl28auto_ptr<16CFilePreloadData>,Q24rstl17rmemory_
 * allocator>FPQ34rstl70list<...>4nodeRCQ24rstl28auto_ptr<16CFilePreloadData>>` at 0x803445DC**
 * (`do_insert_before<list<auto_ptr<CFilePreloadData>>>`, a `Matching` unit, 100.00%). The three
 * stores inside the copy are `lbz`/`stb` the byte at +0, `lwz`/`stw` the word at +4, and
 * `li r0,0` / `stb` **the source's byte at +0** - which is `rstl::auto_ptr`'s auto-relinquishing
 * copy constructor, and it is the same class retail's item is: `fn_802FD174` (`do_erase`) runs
 * `lbz` on that byte and then `bl __dt__CPakFileFv` on `*(item+4)`, i.e. `auto_ptr`'s destructor.
 * So the item is `rstl::auto_ptr< CPakFile >` and this struct is it spelled out, which is what
 * lets `fn_802FC378` be written as the list's own member instead of a transcription.
 * **`mutable` on the flag is required**: the source is `const SPakLoadEntry&`.
 */
struct SPakLoadEntry {
  mutable bool x0_inList;
  CPakFile* x4_pak;
  SPakLoadEntry() : x0_inList(false), x4_pak(nullptr) {}
  SPakLoadEntry(const SPakLoadEntry& other)
  : x0_inList(other.x0_inList)
  , x4_pak(other.x4_pak) {
    other.x0_inList = false;
  }
};

// The port's own copies of the two retail helpers `src/Kyoto/CResLoaderPakPump.cpp` defines
// under `TARGET_PC`, because both are unnamed in `config/G2ME01/symbols.txt` and a `Matching`
// unit may not own their bytes. They are declared here, at namespace scope and with C linkage,
// because mwcceppc rejects `friend extern "C"` (it reads the `extern` as a storage class) and
// GCC rejects a friend declaration that does not match the linkage of the definition.
extern "C" void* fn_802FCFF4(void* resLoader, void* entry);
extern "C" void* fn_802FD174(void* list, void* node);

// The current-resource search, `fn_802FCDE8` (`.text:0x802FCDE8`, unnamed in retail), and the
// five accessors that read what it leaves in `x64_curId` / `x68_curRes`
// (`src/Kyoto/CResLoaderResAccessors.cpp`, a `Matching` unit claiming 0x802FCAE8..0x802FCC44).
// They are free functions with C linkage rather than `CResLoader` members because retail's
// symbols for them are unnamed and **21 dtk objects call them by those names**; the two
// members declared inside the class below (`GetResourceTypeById`, `ResourceSize`) are retail's
// real signatures and are left undefined, which is the state this tree was already in.
extern "C" void* fn_802FCDE8(void* resLoader, CAssetId id);
extern "C" void* fn_802FCEEC(void* resLoader, const SObjectTag& tag);
extern "C" void* fn_802FC81C(void* resLoader, const SObjectTag& tag, int, int, void* buf);
extern "C" void* fn_802FCA68(void* resLoader, const SObjectTag& tag, void* buf);
extern "C" void fn_802FC420(void* resLoader, const SObjectTag& tag, void** outBuf, uint* outSize);
extern "C" void* fn_802FC4D8(void* resLoader, const SObjectTag& tag, void* buf);
extern "C" void* fn_802FC63C(void* resLoader, const SObjectTag& tag, void* extBuf);
extern "C" void* fn_802FC898(void* resLoader, const SObjectTag& tag, void** out, int, int);
extern "C" const SObjectTag* fn_802FCC44(void* resLoader, const char* name);
extern "C" int fn_802FCAE8(void* resLoader, const SObjectTag& tag);
extern "C" uint fn_802FCB40(void* resLoader, const SObjectTag& tag);
extern "C" uint fn_802FCB88(void* resLoader, const SObjectTag& tag);
extern "C" bool fn_802FCBD0(void* resLoader, const SObjectTag& tag);
extern "C" uint fn_802FCC00(void* resLoader, CAssetId id);

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
  //
  // **Two of the four are not unnamed any more, and the evidence is one function.** `fn_802FCF98`
  // (0x802FCF98, 0x54) is the per-pak probe `fn_802FCDE8` calls in its walk, and after
  // `CPakFile::GetResInfo(id)` returns non-null it writes *both*:
  //
  //     802fcfd0:  stw r31,100(r30)   ; this->x64_ = the id it looked up (r31 = arg2)
  //     802fcfd4:  stw r3,104(r30)    ; this->x68_ = the CPakFile::SResInfo* it found
  //     802fcfd8:  li  r3,1
  //
  // and the five accessors at 0x802FCAE8..0x802FCC44 each read **only** `x68_`, after
  // `fn_802FCDE8` has positioned it: `lwz r3,104(r31)` and then `GetSize` / `GetOffset` /
  // `GetType` / `IsCompressed` on it, all four of which are `CPakFile::SResInfo` members taking
  // no argument. `SResInfo` is 11 bytes, so `x68_` is a pointer to one and not the struct - that
  // is what fixes the type. `x60_` and `x6c_` are still unnamed; nothing measured reaches them.
  uint x60_;
  CAssetId x64_curId;                  // +0x64, the id `fn_802FCF98` last looked up
  CPakFile::SResInfo* x68_curRes;      // +0x68, the resource it found, or null
  uint x6c_;

  // The two port-side helpers declared just above the class reach the lists directly, so they
  // are friends rather than the members being made public. Plain friend declarations, with
  // the C linkage already fixed by the namespace-scope declarations.
  friend void* fn_802FCFF4(void* resLoader, void* entry);
  friend void* fn_802FD174(void* list, void* node);

  // Same reason, for `x68_curRes`: the five accessors of
  // `src/Kyoto/CResLoaderResAccessors.cpp` are free functions because retail's symbols for them
  // are unnamed, and 21 dtk objects call them by those names. `fn_802FCDE8` is the search they
  // all call, and it is retail's own bytes in the matching build - the port has its own
  // definition, in the same file, under `TARGET_PC`.
  friend int fn_802FCAE8(void* resLoader, const SObjectTag& tag);
  friend uint fn_802FCB40(void* resLoader, const SObjectTag& tag);
  friend uint fn_802FCB88(void* resLoader, const SObjectTag& tag);
  friend bool fn_802FCBD0(void* resLoader, const SObjectTag& tag);
  friend uint fn_802FCC00(void* resLoader, CAssetId id);
  friend void* fn_802FCDE8(void* resLoader, CAssetId id);
  friend void* fn_802FCEEC(void* resLoader, const SObjectTag& tag);
  // The three loaders below read `x68_curRes` for the same reason the five accessors do, and
  // like them they are free functions with C linkage because retail's symbols for them are
  // unnamed and other dtk objects call them by those names (`fn_802FC81C` from
  // `auto_03_80052880`, `fn_802FCA68` from five objects, `fn_802FC63C`/`fn_802FC420`/
  // `fn_802FC898` from `auto_03_802F8EB0` and `auto_03_80161D04`). `fn_802FCC44` reads no
  // private member, only the two finished lists.
  friend void* fn_802FC81C(void* resLoader, const SObjectTag& tag, int, int, void* buf);
  friend void* fn_802FCA68(void* resLoader, const SObjectTag& tag, void* buf);
  friend void* fn_802FC63C(void* resLoader, const SObjectTag& tag, void* extBuf);
  friend void fn_802FC420(void* resLoader, const SObjectTag& tag, void**, uint*);
  friend void* fn_802FC4D8(void* resLoader, const SObjectTag& tag, void* buf);
  friend void* fn_802FC898(void* resLoader, const SObjectTag& tag, void** out, int, int);
  friend const SObjectTag* fn_802FCC44(void* resLoader, const char* name);
};
CHECK_SIZEOF(CResLoader, 0x70)

#endif // _CRESLOADER
