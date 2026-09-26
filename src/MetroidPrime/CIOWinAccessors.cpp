// Retail 0x80049E10-0x80049E20: `CIOWin::PreDraw`, `CIOWin::Draw`, `CIOWin::GetIsContinueDraw`,
// 16 bytes of `.text` in three functions.
//
// These three are the *body* of `CIOWin` that the vtable at 0x803B1BA0 points into, and they are
// what makes that vtable resolvable: a vtable is only emitted by the translation unit defining
// the class's key function, and once `~CIOWin` exists the vtable's three slots relocate against
// these symbols. `docs/research/boot_probe.md` measured the trap - defining the destructor alone
// turns one missing vtable into three missing methods, a net +2 - so the accessors and the
// destructor have to land together.
//
// Retail's bytes, read out of build/G2ME01/main.elf at the addresses the vtable holds:
//
//   80049e10  4e 80 00 20   blr                     PreDraw
//   80049e14  4e 80 00 20   blr                     Draw
//   80049e18  38 60 00 01   li r3,1                 GetIsContinueDraw, first half
//   80049e1c  4e 80 00 20   blr                     ... and its blr
//
// The claim ends at 0x80049E20, not at 0x80049E1C: `dtk dol split` refuses a range that ends
// *inside* a symbol, and `GetIsContinueDraw__6CIOWinCFv` is 0x80049E18..0x80049E20. 0x80049E20 is
// `fn_80049E20`, which is not CIOWin's - it is in no vtable - and is left to dtk's fill.
//
// Which slot is which is not a guess. `vtable for CIOWin` is 0x20 bytes at 0x803B1BA0 and its
// contents are 0, 0, 0x80049E30, 0, 0x80049E18, 0x80049E14, 0x80049E10: two zero words of header
// (offset-to-top, and typeinfo which is zero because the build is -RTTI off), then one slot per
// virtual in declaration order. `~CIOWin` is first, `OnMessage` next and *null* because the
// header declares it pure, then `GetIsContinueDraw`, `Draw`, `PreDraw`. `GetIsContinueDraw`
// returning **true** and `CMainFlow`'s returning **false** is therefore retail's, not a guess:
// the two vtables disagree, and the two bodies are `li r3,1` and `li r3,0`.
//
// Order below is retail's .text order reversed, which is what mwcceppc wants: it emits function
// definitions in reverse source order and mwldeppc keeps the object's order verbatim. Ascending
// here permutes the module's bytes while objdiff still reads 100% - see the lane briefing.
#include "MetroidPrime/CIOWin.hpp"

bool CIOWin::GetIsContinueDraw() const { return true; }
void CIOWin::Draw() const {}
void CIOWin::PreDraw() const {}
