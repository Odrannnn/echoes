// Retail `GetGameMode__10CGameStateFv` = `_ZN10CGameState11GetGameModeEv`,
// .text 0x80142464..0x8014246C, 8 bytes:
//
//     80142464  lwz  r3,412(r3)      ; 0x19C
//     80142468  blr
//
// 0x19C is `x19c_ptr`, the 12-byte object `CGameState`'s constructor `new(12)`s at 0x80144278
// and then `if (p) p->ctor()` - the header documents exactly that at `x19c_ptr`. Returning
// `CGameMode&` is a no-op in the generated code: the reference *is* the pointer, so the whole
// body is the one load of the member. That is also why the method is non-const in the header
// while the emitted code cannot tell.
//
// Nothing dereferences the pointer here, so the declared return type does not have to be right
// to get the bytes - but `CGameMode&` is what the header says, and it is what the port's call
// sites expect, so that is what this uses.
//
// **`mGameMode` is this member.** `rstl::auto_ptr<CGameMode>` is `{ mutable bool mHas; CGameMode*
// mItem; }` - 8 bytes, `mItem` at +0x04 - and upstream puts it at `CGameState+0x198`, so `mItem`
// is at `+0x19C`, which is `x19c_ptr`'s offset; `mHas` at `+0x198` is retail's `x198_ptrSet`, the
// `(ptr != nullptr)` byte at 0x801442A8. `operator*` is `*mItem` and is inlined, so `*mGameMode`
// is the one `lwz r3,412(r3)` retail has and nothing else.
#include "MetroidPrime/Player/CGameState.hpp"

CGameMode& CGameState::GetGameMode() { return *mGameMode; }
