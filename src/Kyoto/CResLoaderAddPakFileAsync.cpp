/**
 * `CResLoader::AddPakFileAsync` - retail `.text:0x802FC268`, `size:0xE8` = 232 bytes.
 *
 * This is the one function every pak load in the boot path goes through: nine of
 * `CGameGlobalObjects::AddPaksAndFactories` and every `CMain::AddWorldPaks` call land
 * here (`docs/research/paks.md`), it is on the port link-gap ratchet, and nothing on
 * the path can have a pak without it. Retail's body is
 *
 *   0x802FC268  rstl::string s = <pak name> + ".pak";     // member operator+, r3 = sret
 *   0x802FC2A0  rstl::string name(s);                     // the copy that outlives it
 *   0x802FC2AC  ~s;
 *   0x802FC2B4  if (CDvdFile::FileExists(name.c_str())) {
 *   0x802FC2C4    CPakFile* pak = new CPakFile(name, buildDepList, worldPak);
 *   0x802FC2F4    bool inList = pak != nullptr;
 *   0x802FC310    fn_802FC350(&x48_curPak, &inList);
 *   0x802FC314    if (inList) { delete pak; }              // the loader kept the other one
 *   0x802FC32C  }
 *   0x802FC330  ~name;
 *
 * so the flag is written by the caller, overwritten in place by the loader, and read
 * back: the three `lbz`/`stb` on `r1+8` are the whole of the handshake.
 *
 * ---------------------------------------------------------------------------
 * `rstl` concatenation in retail is `rstl::basic_string`'s own **member**
 * `operator+(const char*)`, not a free function. The mangling is the proof:
 *
 *   __pl__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>FPCc
 *
 * while this tree's former free `rstl::operator+(const string&, const char*)` mangled to
 * `__pl__4rstlFRCQ24rstl66basic_string<...>PCc` - a different name, so the compiler emitted
 * it as a weak local copy in the object and the call never reached retail at 0x80021634.
 *
 * The shape is now measured exactly, and only needed a declaration plus `nm` on an
 * *undefined* symbol - no out-of-line template definition, which is what made this look
 * unreachable before (mwcceppc rejects that syntax). Declared **const**, the member mangles
 * to `...rmemory_allocator>CFPCc`; declared non-const, to `...rmemory_allocator>FPCc`,
 * which is retail's name exactly. **So the `C` is the const-member marker and it sits
 * between the template-id's closing `>` and the `F`**, and retail's is the non-const member.
 * (A `static` one is impossible: mwcceppc rejects `static` `operator+` outright.)
 *
 * So the declaration is in `include/rstl/string.hpp` where it belongs, and the free
 * `operator+(const string&, const char*)` is **deleted**: with both present every
 * `s + "literal"` is an ambiguous access, and there are exactly two such sites
 * (`CScriptStreamedMusic.cpp:158`, `CCubeMoviePlayer.cpp:33`) so the change is contained,
 * not tree-wide as `docs/research/rstl_string_member_op.md` first claimed.
 *
 * A `Matching` unit may not own a `.rodata` byte, so `lbl_803AFAA0` is only declared
 * here; its definition is in `src/MetroidPrime/PortGlobals.cpp`, exactly as for the
 * other retail read-only data.
 *
 * Two further details are codegen, not logic, and both are commented where they are
 * used below: the read through a `volatile` lvalue in the `delete`, which is what
 * gives `pakFile` the stack slot retail's `stw r4,12(r1)` / `lwz r3,12(r1)` pair is,
 * and the `if (inList)` polarity, which is retail's own - the loader *keeps* the pak
 * when the flag comes back clear and the caller's copy is dropped when it does not.
 */

// See the long comment on the `operator new` block below: this translation unit supplies its
// own throwing `operator new`, and `Kyoto/Alloc/CMemory.hpp`'s is kept out by its guard.
#define _CMEMORY

#include "types.h"

#include "rstl/rmemory_allocator.hpp"
#include "rstl/linear_iterator.hpp"
#include "rstl/pair.hpp"
#include "rstl/string.hpp"

#include "Kyoto/CDvdFile.hpp"
#include "Kyoto/CPakFile.hpp"
#include "Kyoto/CResLoader.hpp"

// `lbl_803AFAA0` - `.rodata:0x803AFAA0`, `size:0x10`, owned by no unit:
//
//   803afaa0  3f 3f 28 3f 3f 29 00 2e 70 61 6b 00 00 00 00 00   "??(??)..pak"
//
// Two strings the linker merged: bytes 0..6 are the `??(??)?` that stands in for
// `__FILE__` in this throwaway `operator new`, and byte 7 starts the `".pak"` this
// function appends. Retail materialises the address twice - once into r7, which stays
// live across the prologue so the suffix is `addi r5,r7,7`, and once into r4 for the
// `new`'s file argument - and the retail object's relocations confirm it: `R_PPC_ADDR16_HA/LO`
// against `lbl_803AFAA0` at both .text+0x0A and .text+0x5E.
extern "C" const char lbl_803AFAA0[];

// The throwing `operator new`, so that the `new CPakFile` below passes retail's
// `lbl_803AFAA0` as its file argument instead of a literal of its own.
//
// This is `Kyoto/Alloc/CMemory.hpp:28-37` with the one word that matters changed, and the
// header is *not* included here so its version does not collide. It matters because retail's
// object for this function references `lbl_803AFAA0` at both of its rodata uses - the
// relocations are `R_PPC_ADDR16_HA/LO lbl_803AFAA0` at .text+0x0A (the `.pak` suffix) *and*
// at .text+0x5E (the `new`'s file argument) - while the header's `"??(?)?"` is an ordinary
// literal, so mwcceppc emits it as a local `@stringBase0` and the linker lands that on the
// existing pool at 0x803AFAB0. Every byte is then identical except one `addi`:
// `addi r4,r4,-1360` against retail's `addi r4,r4,-1376` - and that one `addi` is the whole
// of the main.dol sha1 difference.
//
// Naming retail's object also means this unit owns no `.rodata`: with the header's literal
// the object carries a 7-byte `.rodata` section, and a `Matching` unit may not.
//
// The `_CMEMORY` define is what keeps the header's own version out, and it has to be set
// before *any* include: `rstl/rmemory_allocator.hpp:6` pulls `Kyoto/Alloc/CMemory.hpp` in,
// and `rstl/string.hpp:6` pulls `rmemory_allocator.hpp` in. Nothing else in this unit needs
// `CMemory`, and if that stops being true the build fails on a missing declaration rather
// than quietly changing which literal the `new` passes. The host build is unaffected - it
// never sees the block below, and with `CMemory` skipped the `new CPakFile` below reaches
// the platform's own global `operator new`, which is what `CMemory.hpp:38-44` does for
// every other port source anyway.
#if defined(__MWERKS__) || defined(CLANGD)
void* operator new(size_t sz, const char*, const char*);
void* operator new[](size_t sz, const char*, const char*);
inline void* operator new(size_t sz) { return operator new(sz, lbl_803AFAA0, nullptr); }
inline void* operator new[](size_t sz) { return operator new[](sz, lbl_803AFAA0, nullptr); }
inline void* operator new(size_t n, void* ptr) { return ptr; }
#endif

// `fn_802FC350` - .text:0x802FC350; size:0x28. Unnamed in the retail map, so it is
// called by address: 0x28 bytes that forward to `fn_802FC378` with `*(this+8)` and the
// caller's flag and pass the result back, and they do not use the return value - the
// caller reloads the byte it stored before the call, which is the proof that the flag
// is written through the pointer.
extern "C" void* fn_802FC350(void* pakLoadingList, void* flagAndPak);

// The member's *definition* is not here: it is `rstl/rstl_string_member_op.cpp`, a
// `Matching` unit claiming retail's 0x80021634, and that one file serves both builds. A
// second copy under TARGET_PC would be a duplicate definition in the port's link.
#ifdef TARGET_PC
// The 8 bytes retail's frame holds at r1+8: a flag byte at +0 and the `CPakFile*` at +4.
// The retail side of this file keeps them as two separate locals, because that is what its
// `stb r0,8(r1)` and `stw r4,12(r1)` are and the register allocator puts them in adjacent
// 4-byte slots; a 64-bit host does not lay two scalars out that way, so the port's build
// hands `fn_802FC350` one object instead of relying on adjacency. Retail's own layout and
// this one are both in `src/MetroidPrime/PortGlobals.cpp`, transcribed from 0x802FC350 and
// 0x802FC378.
struct SPakLoadEntry {
  bool x0_inList;
  CPakFile* x4_pak;
};
#endif // TARGET_PC

void CResLoader::AddPakFileAsync(const rstl::string& pakName, bool buildDepList, bool worldPak) {
  // `pl` takes a hidden return pointer in r3, so the concatenation is a copy-
  // initialisation rather than an assignment: retail constructs straight into r1+16
  // and never stores a pointer to it.
  rstl::string fullName = const_cast< rstl::string& >(pakName).operator+(lbl_803AFAA0 + 7);

  if (CDvdFile::FileExists(fullName.c_str())) {
#ifdef TARGET_PC
    SPakLoadEntry entry;
    entry.x4_pak = new CPakFile(fullName, buildDepList, worldPak);
    entry.x0_inList = entry.x4_pak != nullptr;
    fn_802FC350(&x48_curPak, &entry);
    if (entry.x0_inList) {
      delete entry.x4_pak;
    }
#else
    CPakFile* pakFile = new CPakFile(fullName, buildDepList, worldPak);

    bool inList = pakFile != nullptr;
    fn_802FC350(&x48_curPak, &inList);
    if (inList) {
      // Read through a volatile lvalue: that is what gives `pakFile` a stack slot, and
      // the slot is retail's `stw r4,12(r1)` / `lwz r3,12(r1)`. Without it the pointer
      // is registerised into r28 and the frame grows by 16 bytes (0xEC against 0xE8).
      delete static_cast< CPakFile* volatile& >(pakFile);
    }
#endif
  }
}
