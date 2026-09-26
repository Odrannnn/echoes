#ifndef _RSTL_RMEMORY_ALLOCATOR
#define _RSTL_RMEMORY_ALLOCATOR

#include "types.h"

#include "Kyoto/Alloc/CCallStack.hpp"
#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Alloc/IAllocator.hpp"

namespace rstl {
/**
 * Retail's `rstl::rmemory_allocator` has **two** bodies for the same function, and the DOL
 * proves it. This is the whole reason the template below is behind a macro.
 *
 * **The out-of-line body** - `.text:0x802FDAB8`, `size:0x3C`, the entire contents of
 * `rstl/rmemory_allocator`'s object, which is `src/rstl/rstl_misc.cpp` and nothing else:
 *
 *     802fdac0: cmpwi r3,0 ; bne ; li r3,0 ; b            <- size == 0 -> nullptr
 *     802fdad4: lis r4,-32709 ; li r5,0 ; addi r4,r4,-1360
 *     802fdae0: bl 802ce224 <__nwa__FUlPCcPCc>           <- operator new[](size,"??(??)",0)
 *
 * i.e. `size == 0 ? nullptr : rs_new uchar[size]`, and it takes `"??(??)"` out of
 * `lbl_803AFAB0` - the 8 bytes of `.rodata` that unit owns. `__nwa__FUlPCcPCc` in turn calls
 * `CMemory::Alloc(len, kHI_None=0, kSC_Unk1=1, kTP_Array=1, CCallStack(-1, file, type))`.
 *
 * **A second body**, inlined by mwcceppc rather than called, and used by exactly one function
 * in the whole DOL: `reserve<rstl::vector<CPakFile::SResInfo>>` in `Kyoto/CPakFile.o`
 * (`.text:0x80324A64`, `object+0x1A2C`):
 *
 *     1a50: mulli  r27,r31,11     ; size = count * sizeof(SResInfo) = count * 11
 *     1a54: cmpwi  r27,0 ; bne ; li r28,0 ; b            <- size == 0 -> nullptr, same test
 *     1a6c: addi   r5,r3,76       ; "??(??)" = lbl_803B0098 + 76 = 0x803B00E4
 *     1a6a: kUnknownType__10CCallStack                   ; the default `type` argument
 *     1a78: li     r4,-1          ; the line number
 *     1a7a: addi   r3,r1,24       ; the temporary, built in the CALLER's frame
 *     1a80: bl     CCallStack::CCallStack
 *     1a8c: li     r4,2           ; IAllocator::kHI_RoundUpLen  <- NOT kHI_None
 *     1a90: li     r5,1           ; IAllocator::kSC_Unk1
 *     1a94: li     r6,0           ; IAllocator::kTP_Heap       <- NOT kTP_Array
 *     1a98: bl     CMemory::Alloc
 *
 * So it is `size == 0 ? nullptr : CMemory::Alloc(size, kHI_RoundUpLen, kSC_Unk1, kTP_Heap,
 * CCallStack(-1, "??(??)"))`, spelled directly, and the two bodies differ in the *literal*
 * hint and type arguments, so no inlining or outlining of one can produce the other.
 *
 * **Measured, not assumed: which is which.** All sixteen `reserve` instantiations in the DOL
 * were disassembled. Fifteen call `0x802FDAB8` out of line - `vector<SObjectTag>` (0x80008DE8),
 * `vector<unsigned int>` (0x8004698C, and a second unnamed copy at 0x80052220), `vector<
 * TUniqueId>` (0x80046AC8), `vector<SConnection>` (0x800485E4), `vector<CMayaSplineKnot>`
 * (0x80082DDC), `vector<CPlayerState::SPersistentState::SScanState>` (0x800865CC),
 * `vector<SLdrConnection>` (0x801E21B4), `vector<CRuleValue>` (0x801F6E2C), `vector<
 * CRuleCondition>` (0x801F6F00), `vector<CRuleAction>` (0x801F7004), `vector<CRuleSetRule>`
 * (0x801F7184), `vector<CTHPTextureSet>` (0x8031A7E0), `vector<auto_ptr<unsigned char>>`
 * (0x8031A960), `vector<auto_ptr<CDvdRequest>>` (0x803270A4) - and one inlines. The
 * discriminator is **not** `sizeof(T)`: the out-of-line set has sizes 2, 4, 8, 12, 16, 20, 24,
 * 28, 32 and 40, and the inlined one is 11, which is neither special nor unique among them.
 * Nor is it the return type, the module, or whether the unit is `Matching` here. What it
 * tracks is the **object**: `Kyoto/CPakFile.o` is the one object in the DOL whose `reserve`
 * carries the `kHI_RoundUpLen` body, and that is exactly the granularity a per-translation-unit
 * macro can express. Retail's build plainly contains two revisions of this header, and
 * `Kyoto/CPakFile.cpp` was compiled against the newer one.
 *
 * **Measured cost of getting this wrong.** Making the template inline unconditionally - the
 * obvious reading of "fix the header" - costs **seven functions at 100% in four `Matching`
 * units** and drops the linked total **1831 -> 1824**, which is a gate failure.
 * (Re-measured 2026-09-26 by the counterfactual: `#if 1` in this header and in
 * `rstl/construct.hpp`, then reverted. These figures were correct when written, but the totals
 * move as `linked` rises, and a stale number in a comment whose only job is to warn is worse
 * than no number - the seven and the four had both been six and three. The *ratios* never
 * changed, so the conclusion never changed, and that is the only reason this is still worth
 * reading.)
 * `CFrameDelayedKiller::do_insert_before<list<void*>>` 100.00 -> 64.86,
 * `CResLoaderInsert::fn_802FC378` 100.00 -> 59.83,
 * `CFilePreload::do_insert_before<list<auto_ptr<CFilePreloadData>>>` 100.00 -> 59.83,
 * `rstl_strings::internal_allocate<char>` 100.00 -> 23.88,
 * `rstl_strings::internal_allocate<wchar_t>` 100.00 -> 29.52,
 * `rstl_strings::internal_prepare_to_write<char>` 100.00 -> 73.20 and
 * `rstl_strings::internal_prepare_to_write<wchar_t>` 100.00 -> 81.11, plus more percentage
 * drops in units that were not `Matching`. The macro keeps that movement at zero while taking
 * `Kyoto/CPakFile`'s `reserve<vector<CPakFile::SResInfo>>` from **33.84% to 66.26%**, and with
 * `rstl::uninitialized_copy` inlined in the same translation unit (see
 * `rstl/construct.hpp` - the same object is the only one retail inlined that into as well, so
 * the two are one decision) to **99.74%**.
 *
 * The macro is `RSTL_INLINE_RESERVE_HELPERS`, not something about allocation, because it
 * selects the revision of the *rstl headers* this object was compiled against and the same
 * object needs both.
 *
 * **The remaining `addi`, and how it is fixed.** Retail's `addi r5,r5,76` is
 * `lbl_803B0098 + 76`, i.e. retail's linker merged this `"??"(??)?"` with the tail of the
 * pak-version message that `CPakFile::InitialHeaderLoad` already references at
 * `.rodata:0x803B0098` (which is why the claim in `config/G2ME01/splits.txt` is
 * `0x803B0098..0x803B00F0` and not one string). `src/Kyoto/CPakFile.cpp` therefore names that
 * object and sets `RSTL_ALLOCATE_FILE_AND_LINE` to `lbl_803B0098 + 76`, the same treatment
 * `CResLoaderAddPakFileAsync.cpp` gives `lbl_803AFAA0`, and objdiff pairs the relocation.
 */
struct rmemory_allocator {
  rmemory_allocator() {}
  rmemory_allocator(const rmemory_allocator&) {}

  /** The 0x3C-byte out-of-line body, defined in `src/rstl/rstl_misc.cpp`. */
  static void* allocate(int size);

#ifndef RSTL_ALLOCATE_FILE_AND_LINE
#define RSTL_ALLOCATE_FILE_AND_LINE "\?\?(\?\?)"
#endif

#ifdef RSTL_INLINE_RESERVE_HELPERS
  /**
   * The `CMemory::Alloc`/`CCallStack` body, inlined. Only for the objects that carry it.
   *
   * `RSTL_ALLOCATE_FILE_AND_LINE` is the `CCallStack`'s file-and-line text. Retail's is a 7-byte
   * `"??"(??)?"` that its linker merged into a neighbouring read-only object, so where the
   * object is known it is named and the offset is arithmetic on it - `src/Kyoto/CPakFile.cpp`
   * sets it to `lbl_803B0098 + 76` and gets the relocation paired by objdiff. Left at the
   * default it is the same string as a plain literal, which mwcceppc emits as a local
   * `@stringBase0` and objdiff cannot pair.
   */
  template < typename T >
  static void allocate(T*& out, int count) {
    const int size = count * sizeof(T);
    out = size == 0 ? nullptr
                    : reinterpret_cast< T* >(CMemory::Alloc(
                          size, IAllocator::kHI_RoundUpLen, IAllocator::kSC_Unk1,
                          IAllocator::kTP_Heap, CCallStack(-1, RSTL_ALLOCATE_FILE_AND_LINE)));
  }
#else
  template < typename T >
  static void allocate(T*& out, int count) {
    int size = count * sizeof(T);
    out = reinterpret_cast< T* >(allocate(size));
  }
#endif
  // TODO: this fixes a regswap in vector::reserve
  template < typename T >
  static T* allocate2(int count) {
    int size = count * sizeof(T);
    if (size == 0) {
      return nullptr;
    } else {
#ifdef TARGET_PC
      // The same pair as `deallocate` below: `new uchar[]` is `__nwa__FUlPCcPCc` under mwcceppc,
      // i.e. exactly the out-of-line `allocate(int)`, so that is what the host calls too.
      return reinterpret_cast< T* >(allocate(size));
#else
      return reinterpret_cast< T* >(new uchar[size]);
#endif
    }
  }
  /**
   * **Both halves of this allocator are the game heap, on the host as on retail.** Under mwcceppc
   * `delete[]` is `CMemory.hpp`'s inline `operator delete[]`, which is `CMemory::Free`, so the pair
   * is `CMemory::Alloc` / `CMemory::Free` and this line is retail's. On the host that header takes
   * its `#else` and the same `delete[]` is **glibc's `free`** - while `allocate<T>` goes through
   * the out-of-line `allocate(int)`, which `src/rstl/rstl_misc.cpp` routes to `CMemory::Alloc`.
   * Every `rstl::vector`, `list`, `red_black_tree` and `hash_map` buffer was therefore allocated by
   * the game and freed by the host: defect 2's shape again, one level up.
   *
   * Measured in the port binary, not argued: a probe right after `CGameAllocator::Initialize` that
   * pushes five ints into an `rstl::vector<int>` (capacity 4 -> 8, so `reserve` frees the old
   * buffer) printed
   *
   *     [INSTR probe] vector growth 4 -> 8
   *     free(): invalid pointer                     <- SIGABRT, exit 134
   *
   * and with this branch it survives the growth and a `list` erase. The boot had not reached it
   * only because nothing on the path so far frees a container buffer; `CSimplePool`'s
   * `ObjectUnreferenced` erases a pool bucket's list node, so the first resource token to die
   * after real paks load would have been the first abort.
   *
   * `#ifdef TARGET_PC` because this template is instantiated by `Matching` units under mwcceppc:
   * the retail branch is the pre-existing `delete[]`, which is the same `CMemory::Free`.
   */
  template < typename T >
  static void deallocate(T* ptr) {
#ifdef TARGET_PC
    CMemory::Free(ptr);
#else
    delete[] reinterpret_cast< uchar* >(ptr);
#endif
  }
};
} // namespace rstl

#endif // _RSTL_RMEMORY_ALLOCATOR
