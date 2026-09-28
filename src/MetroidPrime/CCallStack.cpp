// `CCallStack`'s three members, retail 0x8028BFD8..0x8028BFF4, 0x1C = 28 bytes:
//
//     8028bfd8 <fn_8028BFD8>:                       8 bytes
//         lwz  r3,4(r3)
//         blr
//     8028bfe0 <fn_8028BFE0>:                       8 bytes
//         lwz  r3,0(r3)
//         blr
//     8028bfe8 <__ct__10CCallStackFUiPCcPCc>:      12 bytes
//         stw  r5,0(r3)
//         stw  r6,4(r3)
//         blr
//
// **This is retail's whole `RAssert` call-stack scaffolding, and it formats nothing.** The
// class is eight bytes - two `char const*` and nothing else - the constructor ignores its
// first argument entirely, and the two accessors are plain member reads. There is no string
// building, no `vsnprintf`, no file/line join anywhere in the three bodies, and
// `include/Kyoto/Alloc/CCallStack.hpp` is right about both, and the members spell out what the
// disassembly forces: `mLine` and `mType` are the *second* and *third* constructor arguments,
// stored verbatim (they were `x0_line`/`x4_type` before the rename to upstream's `m` names).
//
// Which accessor is which is not guessed. `CGameAllocator::FixupAllocPtrs` is the only
// caller, and at 0x8030E42C it is unambiguous:
//
//     8030e42c:  mr   r3,r26                       ; &cs
//     8030e430:  bl   8028bfe0 <fn_8028BFE0>        ; -> +0
//     8030e434:  stw  r3,8(r28)                    ; SGameMemInfo::x8_fileAndLine
//     8030e438:  mr   r3,r26
//     8030e43c:  bl   8028bfd8 <fn_8028BFD8>        ; -> +4
//     8030e440:  stw  r3,12(r28)                   ; SGameMemInfo::xc_type
//
// so `GetFileAndLineText` is the `+0` read at 0x8028BFE0 and `GetTypeText` the `+4` read at
// 0x8028BFD8. `+8` and `+0xC` of `SGameMemInfo` are the port's own names for the same two
// slots, in `include/Kyoto/Alloc/SGameMemInfo.hpp`.
//
// **The "line" is a string the caller built, not a number.** The constructor's `uint` is
// discarded, so retail's assert machinery must be handing it a pre-formatted
// `RSTL_ALLOCATE_FILE_AND_LINE` - see the comment at
// `include/rstl/rmemory_allocator.hpp:112`, where the inlined `CMemory::Alloc` body passes a
// 7-byte literal. That is the whole reason the third constructor argument defaults to
// `kUnknownType` (`.rodata:0x803AEAB8`, `"UnknownType"`, defined for the port in
// `src/MetroidPrime/PortGlobals.cpp`): the class never manufactures a type name either.
//
// **This is on the port's boot path and it was costing three undefined symbols.** All three
// were in `docs/research/port_link_gap_list.md` and in the diagnostic reach-stub list
// (`src/MetroidPrime/PortReachStubs.cpp` stubs 28/29/30), because
// `CGameAllocator::FixupAllocPtrs` - a real compiled unit on the allocation path - calls the
// two accessors out of line and `CMemory::Alloc`'s inline body constructs the object.
//
// A named C++ member, so a `.cpp` is right here. One discontiguous `.text` range, three
// contiguous functions, so the descending-source-order rule is the only ordering constraint:
// ctor (0x8028BFE8) first, then `+0` (0x8028BFE0), then `+4` (0x8028BFD8).
#include "Kyoto/Alloc/CCallStack.hpp"

CCallStack::CCallStack(uint, const char* lineStr, const char* type) {
  mLine = lineStr;
  mType = type;
}

const char* CCallStack::GetFileAndLineText() const {
  return mLine;
}

const char* CCallStack::GetTypeText() const {
  return mType;
}
