/**
 * `fn_802FC63C` - retail `.text:0x802FC63C`, `size:0x1E0` = 480 bytes, 48-byte frame, `stmw r27`.
 * `CResLoader::LoadNewResourceSync`: the whole of `fn_802FC4D8` plus the *optional
 * caller-supplied buffer*, and it is the function the boot path's `CMain` and every script's
 * `LoadNewResourceSync` call.
 *
 * ```
 * 802fc63c:  stwu r1,-48(r1) ; mflr r0 ; stw r0,52(r1) ; stmw r27,28(r1)
 * 802fc64c:  mr   r27,r3                      <- this
 * 802fc650:  mr   r30,r5                      <- arg3, the caller's buffer (may be null)
 * 802fc654:  bl   fn_802FCEEC
 * 802fc658:  lwz  r31,104(r27)                <- this->x68_curRes
 * 802fc65c:  mr   r28,r3                      <- the pak
 * 802fc660:  bl   GetSize
 * 802fc66c:  addi r0,r3,31 ; clrrwi r27,r0,5   <- the 32-byte-rounded length
 * 802fc674:  cmplwi r30,0 ; beq <allocate> ; mr r29,r30 ; b <the read>
 * 802fc688:  <allocate: the CCallStack at r1+8, then CMemory::Alloc(padded, 2, 1, 0, callstack)>
 * 802fc6c4:  bl   GetOffset
 * 802fc6d4:  bl   SyncSeekRead                 <- pak, buf, padded, kSO_Set, offset
 * 802fc6dc:  <new(20) for the CMemoryInStream, then its THREE-argument constructor>
 * 802fc704:  stw  r28,20(r1) ; stb r0,16(r1)   <- the auto_ptr, at r1+16
 * 802fc738:  bl   IsCompressed ; clrlwi. ; beq <the not-compressed arm>
 * 802fc744:  <the same compressed tail as fn_802FC4D8, with addi r4,r1,16>
 * ```
 *
 * The differences from `fn_802FC4D8` (`Kyoto/CResLoaderLoadResourceSyncCompressed.cpp`) are three,
 * and each is one instruction pair:
 *
 *  1. **The buffer is the caller's when the caller gave one.** `cmplwi r30,0` / `beq` picks
 *     between `mr r29,r30` and a `CMemory::Alloc` that is *skipped entirely*, so the `Alloc` call
 *     and its `CCallStack` are on the cold side of the branch. This is why the frame is 48 and not
 *     32: the `CCallStack` at `r1+8` and the `rstl::auto_ptr< CInputStream >` at `r1+16` coexist,
 *     where `fn_802FC4D8` has only the `auto_ptr` and puts it at `r1+8`.
 *  2. **The `CMemoryInStream` constructor takes its `EOwnerShip` argument** -
 *     `__ct__15CMemoryInStreamFPCvUlQ215CMemoryInStream10EOwnerShip` against
 *     `__ct__15CMemoryInStreamFPCvUl` in the other two - and the value is
 *     `neg r0,r30 ; or r0,r0,r30 ; srwi r30,r0,31`, i.e. **`callerBuf != nullptr`**. That is
 *     `CMemoryInStream::kOS_NotOwned` (= 1), and it is the only correct answer: a buffer the
 *     caller owns must not be freed by the stream, and one this function allocated must be. So
 *     the ownership is *derived from which arm of the branch produced the pointer*, and the
 *     three-argument constructor is what expresses it.
 *  3. **The search's return value is kept** (`mr r28,r3`) and used for the `SyncSeekRead` -
 *     `fn_802FC4D8` discards it. Both are correct readings of retail; the difference is that this
 *     one reads the resource again *after* the read has been set up.
 *
 * Everything from the `IsCompressed` test onwards is **the same twenty instructions as
 * `fn_802FC4D8`**, at `r1+10` instead of `r1+8` for the `auto_ptr` and with r31/r29/r28 in place of
 * r31/r30/r29 for the two temporaries. Reading the two side by side is what identifies the
 * four-byte decompressed-size prefix, the `GetSize() - (x8 - x4)` compressed length, and the
 * `~CInputStream(1)` teardown through vtable slot +8 - see that file's comment for all three.
 *
 * Unnamed in `config/G2ME01/symbols.txt` and referenced by `auto_03_802F8EB0_text.o` and
 * `auto_03_80161D04_text.o`, so it keeps its dtk name with C linkage. **The header declares a
 * four-argument `LoadNewResourceSync(const SObjectTag&, int, int, char*)` and retail's function
 * reads only r3/r4/r5**, so this is the three-argument overload and the header's declaration is
 * for a different one that is also unnamed.
 */
#include "types.h"

#include "rstl/auto_ptr.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/CPakFile.hpp"
#include "Kyoto/CDvdFile.hpp"
#include "Kyoto/CResLoader.hpp"
#include "Kyoto/Streams/CLZOInputStream.hpp"
#include "Kyoto/Streams/CMemoryInStream.hpp"

// `lbl_803AFAA0` - `.rodata:0x803AFAA0`, `size:0x10`, owned by no unit. See
// `src/Kyoto/CResLoaderAddPakFileAsync.cpp`; it is `__FILE__` for the `new`s here and the
// `CCallStack`'s line-and-file text there. The `new` spelling is behind a macro because the host
// build has no three-argument `operator new` and a unit has to compile in both.
extern "C" const char lbl_803AFAA0[];

#if defined(__MWERKS__) || defined(CLANGD)
void* operator new(size_t sz, const char*, const char*);
#define RESLOADER_NEW new (lbl_803AFAA0, nullptr)
#else
#define RESLOADER_NEW new
#endif

extern "C" void* fn_802FC63C(void* resLoader, const SObjectTag& tag, void* extBuf) {
  CResLoader* const self = static_cast< CResLoader* >(resLoader);
  CPakFile* const pak = static_cast< CPakFile* >(fn_802FCEEC(resLoader, tag));
  CPakFile::SResInfo* const res = self->x68_curRes;
  const uint paddedSize = (res->GetSize() + 31) & ~static_cast< uint >(31);
  // Two arguments, not three: `CCallStack`'s third parameter defaults to the class's own
  // `kUnknownType`, and that default *is* `kUnknownType__10CCallStack` (0x803AEAB8) - the symbol
  // retail's relocation names. The temporary is passed straight into `Alloc` rather than named,
  // because retail's `addi r3,r1,8` / `bl <ctor>` / `mr r7,r3` reuses the constructor's own r3.
  void* const buf =
      (extBuf != nullptr) ? extBuf
                          : CMemory::Alloc(paddedSize, IAllocator::kHI_RoundUpLen,
                                           IAllocator::kSC_Unk1, IAllocator::kTP_Heap,
                                           CCallStack(-1, lbl_803AFAA0));
  pak->DvdFile().SyncSeekRead(buf, paddedSize, kSO_Set, res->GetOffset());
  rstl::auto_ptr< CInputStream > stream(RESLOADER_NEW CMemoryInStream(
      buf, res->GetSize(), extBuf != nullptr ? CMemoryInStream::kOS_NotOwned
                                            : CMemoryInStream::kOS_Owned));
  if (res->IsCompressed()) {
    // The pak's four-byte decompressed-size prefix, read by hand off `x8_ptr` for the same reason
    // `fn_802FC4D8` does it: it is the one read off this cursor `ReadInt32` does not perform, and
    // on a host it is pak bytes and needs `cinput_stream_read_be32` from `CInputStream.hpp`.
#ifdef TARGET_PC
    const uint decompressedSize = cinput_stream_read_be32(stream->x8_ptr);
#else
    const uint decompressedSize = *reinterpret_cast< const uint* >(stream->x8_ptr);
#endif
    stream->x8_ptr += sizeof( uint );
    return RESLOADER_NEW CLZOInputStream(stream, res->GetSize() - stream->GetReadPosition(),
                                        decompressedSize);
  }
  return stream.release();
}
