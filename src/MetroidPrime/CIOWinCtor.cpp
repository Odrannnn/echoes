// Retail 0x80049E98-0x80049ED8: `CIOWin::CIOWin(const rstl::string&)`, 64 bytes.
//
// This is not one of the frame loop's twelve, but `CMainFlow::CMainFlow` cannot be written
// without it: the base-constructor call is in the middle of that body, and retail's is *unnamed*
// (`fn_80049E98` in `config/G2ME01/symbols.txt`). Writing the derived constructor therefore meant
// giving the base one its real name, which is the one config change this unit needs - see
// docs/research/frame_loop.md.
#include "MetroidPrime/CIOWin.hpp"

#include "rstl/string.hpp"

// 0x80049E98, 0x40 bytes: two instructions of work.
//   `lis r5,0x803B; addi r0,r5,0x1BA0` -> 0x803B1BA0, CIOWin's vtable, stored at +0.
//   `addi r3,r31,4` then `bl __ct__Q24rstl66basic_string<c,...>FRCQ24rstl66basic_string<c,...>`
//   - the copy *constructor* on the `rstl::string mName` member, not an assignment: there is no
//   `basic_string()` call before it, so `name` is initialised from the parameter.
//
// CIOWin's own vtable entry (the first two words of it) is 0x803B1BA0, measured by reading
// .data at that address in build/G2ME01/main.elf; mwcceppc derives the same value from the class
// itself, so nothing in the source names it.
CIOWin::CIOWin(const rstl::string& inName) : mName(inName) {}
