/**
 * `fn_802FC4D8` - retail `.text:0x802FC4D8`, `size:0x164` = 356 bytes, 32-byte frame. The
 * **compressed** twin of `fn_802FC420` (`Kyoto/CResLoaderLoadResourceSync.cpp`): same search,
 * same `x68_curRes`, but the resource is LZO-wrapped and the buffer is supplied by the caller
 * rather than allocated.
 *
 * The whole function is one shape, and every load-half of this loader is the same shape:
 *
 * ```
 * 802fc4d8:  bl   fn_802FCEEC
 * 802fc4e0:  lwz  r31,104(r30)              <- this->x68_curRes
 * 802fc4ec:  li   r3,20 ; lis r4,lbl_803AFAA0 ; li r5,0
 * 802fc4f0:  bl   __nw__FUlPCcPCc           <- operator new(20, lbl_803AFAA0, 0)
 * 802fc4f4:  mr.  r30,r3 ; beq <skip the ctor>
 * 802fc500:  bl   GetSize
 * 802fc510:  bl   __ct__15CMemoryInStreamFPCvUl
 * 802fc518:  neg r0,r30 ; stw r30,12(r1) ; or ; srwi ; stb r0,8(r1)   <- rstl::auto_ptr's ctor
 * 802fc530:  bl   IsCompressed
 * 802fc538:  clrlwi. r0,r3,24 ; beq <the not-compressed arm>
 * ```
 *
 * The `rstl::auto_ptr< CInputStream >` **is the frame's `r1+8`/`r1+12` pair** - `x0_has` at +8 and
 * `x4_item` at +12, which is `auto_ptr`'s two words - and its construction is the three
 * instructions `auto_ptr(T*)` emits, not a store pair written by hand. That matters because the
 * same two words are re-read as the `const rstl::auto_ptr< CInputStream >&` the `CLZOInputStream`
 * constructor takes (`addi r4,r1,8` at 0x802fc590), and that constructor
 * (`__ct__15CLZOInputStreamFRCQ24rstl24auto_ptr<12CInputStream>UlUl`, 0x8034412C, a `Matching` unit)
 * *consumes* the source with `rstl::auto_ptr< CInputStream > stream(in);` - so the loader's own
 * `auto_ptr` comes back empty and the teardown at 0x802fc59c is the **null** path. It is still
 * emitted, which is what tells you the local is an `auto_ptr` and not a raw pointer.
 *
 * The compressed arm, in order:
 *
 *  1. **Read the 4-byte decompressed size off the front of the stream** -
 *     `lwz r6,8(r7)` (`x8_ptr`), `addi r0,r6,4` / `stw r0,8(r7)` (`x8_ptr += 4`), `lwz r30,0(r6)`.
 *     This is `CInputStream::Get(4)` inlined, and the stream's cursor moves, so the
 *     `CMemoryInStream` the caller passed is left positioned past the prefix. `Get` lives in
 *     `Kyoto/Streams/CInputStream.cpp`, a different translation unit, so it cannot be inlined from
 *     here - these two functions are friends of `CInputStream` (see the header) and read
 *     `x8_ptr` directly. **`fn_802FC63C` is the identical sequence at 0x802fc734**, which is the
 *     confirmation that it is the read and not something else.
 *  2. `new (20) for the `CLZOInputStream`, with the same `lbl_803AFAA0` / `0` file-and-line pair.
 *  3. **The compressed length is `GetSize() - (x8 - x4)`**, computed as
 *     `subf r31,r4,r0` off `lwz r4,4(r5)` / `lwz r0,8(r5)` of the **`SResInfo`** and then
 *     `subf r5,r31,r3` off `GetSize()`. So the source is
 *     `GetSize() - ( *(uint*)(res+8) - *(uint*)(res+4) )` on the *uncompressed* size, and the two
 *     `lwz`s are `SResInfo`'s own packed words, read directly for the same cross-TU reason.
 *     `CPakFile::SResInfo` is `#pragma pack(1)` at 0xB bytes, so `+4` and `+8` are
 *     `x4_data[0..3]` and `x4_data[4..7]`.
 *
 * The teardown is the interesting part. It is **`rstl::auto_ptr< CInputStream >::~auto_ptr`**
 * inlined, and because `CInputStream` is polymorphic it goes through the vtable:
 *
 * ```
 * 802fc59c:  lbz  r0,8(r1)          ; x0_has
 * 802fc5a0:  cmplwi r0,0 ; beq <skip>
 * 802fc5a8:  lwz  r3,12(r1)         ; x4_item
 * 802fc5ac:  cmplwi r3,0 ; beq <skip>
 * 802fc5b4:  lwz  r12,0(r3)        ; the vptr
 * 802fc5b8:  li   r4,1             ; <-- the DELETING-destructor flag
 * 802fc5bc:  lwz  r12,8(r12)       ; vtable slot +8
 * 802fc5c0:  mtctr r12 ; bctrl
 * ```
 *
 * **The `li r4,1` is the whole identification.** It is `~CInputStream(1)` - the *deleting*
 * destructor - reached as vtable slot **+8**, not +0, and retail's own
 * `__dt__15CMemoryInStreamFv` (0x800055CC, a weak symbol) is the match: it saves `r4` into `r31`,
 * stores the base vptr, `bl`s `__dt__12CInputStreamFv`, and then `extsh. r0,r31 ; ble` - i.e. it
 * calls `CMemory::Free` **only when the flag is non-zero**. `__vt__15CMemoryInStream` is 0xC bytes
 * at 0x803B0D5C: `[0, 0, __dt__15CMemoryInStreamFv]`, and the vptr the constructor stores is
 * `0x803B0D5C` itself, so slot +8 is the third word. **`CInputStream`'s one virtual is its
 * destructor** (`virtual ~CInputStream()` in the header) and the `auto_ptr`'s `delete x4_item` is
 * the only thing in this function that can call it, which is why the tail of the compressed arm
 * and the whole of the not-compressed arm are the same eleven instructions.
 *
 * The not-compressed arm (0x802fc5d0) is that teardown with **`li r0,0` / `stb r0,8(r1)` before
 * it** - `auto_ptr::release()`-shaped: the flag is cleared, so the destructor is *not* taken, and
 * `r29` is the returned pointer. That is the source's `return stream.release();` in the
 * `!IsCompressed()` arm, and the `li r0,0` sits in the *fallthrough* position because MWCC laid the
 * compressed arm out as the `beq`'s not-taken side. The two `beq`s that skip the `bctrl`
 * (`cmplwi r0,0` on the flag and `cmplwi r3,0` on the pointer) are `auto_ptr`'s own two guards.
 *
 * The `__nw__FUlPCcPCc` calls pass **`lbl_803AFAA0` and a literal `0`**, not `CMemory`'s
 * `rs_new`; this is the same "new with a file string" the recipe in
 * `docs/research/rc_ptr.md` describes, and `CMemory.hpp`'s `operator new(size_t)` here forwards
 * to `operator new(sz, "??(??)", nullptr)`, which is *this* symbol. A unit that includes
 * `Kyoto/Alloc/CMemory.hpp` normally gets that inline, so nothing extra is declared here - the
 * `_CMEMORY` dance from `CResLoaderAddPakFileAsync.cpp` is **not** needed because this function
 * never calls `CMemory::Alloc`, only `operator new`.
 *
 * Unnamed in `config/G2ME01/symbols.txt` and referenced by `auto_03_802C3E88_text.o`, so it keeps
 * its dtk name with C linkage. Its range is 0x802FC4D8..0x802FC63C and `fn_802FC63C` is next, so
 * the two are separate units: they differ in where the buffer comes from.
 *
 * ---------------------------------------------------------------------------
 * **This unit is `NonMatching` at 99.10%, and the reason is four register numbers.**
 *
 * Every instruction is right - 89 emitted against retail's 89, with the same order, the same
 * branches and the same relocations. What differs is which callee-saved registers mwcceppc hands
 * the compressed arm's four temporaries:
 *
 * ```
 *                         retail          this build
 *   the stream           r7              r6
 *   x8_ptr               r6              r7
 *   decompressedSize     r30             r29
 *   the new'd stream     r29             r30
 * ```
 *
 * Two pairs, each swapped, 20 differing instructions. `fn_802FC63C`
 * (`Kyoto/CResLoaderLoadNewResourceSync.cpp`) has the same wall in the other direction - r28/r29/r30
 * against r27/r28/r29, so all three are one lower, 16 instructions. **About forty body shapes were
 * tried and none moved it**: `prefix` vs `stream->x8_ptr`, `= prefix + 4` vs `+= 4`, `const` /
 * `uchar*` / `void*` / `uint*` cursor types, `uint` / `s32` / `long` / `unsigned long` for the size,
 * naming the `CLZOInputStream` result or not, naming the cursor, `Get(4)`, `stream.get()`, `*stream`,
 * a `CMemoryInStream*` cast, `operator->`, a `CDvdFile&` local, declarations hoisted or sunk, a
 * named `owns` bool, an `if` instead of `?:`, a named `resSize`. This is a register-allocator
 * preference and not a source-shape problem, so the unit stays `NonMatching` **with its range
 * claimed** - retail's bytes stay in the link, and the body is here for the next lane to try.
 */
#include "types.h"

#include "rstl/auto_ptr.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/CPakFile.hpp"
#include "Kyoto/CResLoader.hpp"
#include "Kyoto/Streams/CLZOInputStream.hpp"
#include "Kyoto/Streams/CMemoryInStream.hpp"

// `lbl_803AFAA0` - `.rodata:0x803AFAA0`, `size:0x10`, owned by no unit. See the long comment in
// `CResLoaderAddPakFileAsync.cpp`; the same object is `__FILE__` for both this unit's `new`s and
// for that one's.
//
// `Kyoto/Alloc/CMemory.hpp`'s own `operator new(size_t)` forwards to
// `operator new(sz, "??(??)", nullptr)`, and **that is retail's symbol** - but only because the
// literal it names and `lbl_803AFAA0` are the same six bytes at the same address. mwcceppc emits a
// literal as a local `@stringBase0` and the linker places *that* somewhere else, so the `addi`
// differs and only the hash sees it. Passing the named object explicitly is what fixes it, and it
// is the same correction `docs/research/rc_ptr.md` records.
extern "C" const char lbl_803AFAA0[];

// The two `new`s below pass retail's own `__FILE__` string. The MWCC build has the three-argument
// `operator new` (`Kyoto/Alloc/CMemory.hpp` declares it under `__MWERKS__`); the host build does not,
// and a `Matching` unit has to compile in both. `src/Kyoto/CResLoaderAddPakFileAsync.cpp` solves the
// same problem by defining its own; here the two `new`s are in one function, so a macro is enough.
#if defined(__MWERKS__) || defined(CLANGD)
void* operator new(size_t sz, const char*, const char*);
#define RESLOADER_NEW new (lbl_803AFAA0, nullptr)
#else
#define RESLOADER_NEW new
#endif

extern "C" void* fn_802FC4D8(void* resLoader, const SObjectTag& tag, void* buf) {
  CResLoader* const self = static_cast< CResLoader* >(resLoader);
  // The search's *return value is discarded*: retail calls `fn_802FCEEC` and never uses r3
  // afterwards, and the resource is re-read from `x68_curRes` instead. That is the difference
  // between this and `fn_802FC81C`/`fn_802FCA68`, which keep the `CPakFile*`.
  fn_802FCEEC(resLoader, tag);
  CPakFile::SResInfo* const res = self->x68_curRes;
  rstl::auto_ptr< CInputStream > stream(
      RESLOADER_NEW CMemoryInStream(buf, res->GetSize()));
  if (res->IsCompressed()) {
    // The four-byte decompressed-size prefix the pak format puts in front of the LZO payload, read
    // off the front of the memory stream and stepped over. `x8_ptr` is `CInputStream`'s private
    // cursor and `Get(4)` is defined in another translation unit, so this function is a friend (see
    // the header) and reaches the member directly. The read itself is the header's
    // `cinput_stream_read_be32` on a host, because the prefix is pak bytes and a host's own load
    // would return it reversed - the one read off this cursor that `ReadInt32` does not do.
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
