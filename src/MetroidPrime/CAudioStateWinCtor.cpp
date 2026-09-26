/**
 * `CAudioStateWin::CAudioStateWin()` - retail `fn_800E25C4`, `.text:0x800E25C4`, `size:0x5C` = 92
 * bytes, 23 instructions (`config/G2ME01/symbols.txt:3930` carries the size).
 *
 * It is the **fourth** of the four IOWins `CGameArchitectureSupport`'s constructor registers at
 * boot step 18 (`src/MetroidPrime/main.cpp:289`), and the only one of the four with no source in
 * the tree before this file: `CMainFlow` is `Matching` (`CMainFlowCtor.cpp`), `CConsoleOutputWindow`
 * has a body at 98.17% (`CConsoleOutputWindowCtor.cpp`, which is in **neither** `configure.py` nor
 * `files.cmake`, so nothing builds it) and `CErrorOutputWindow` is at 78.56%
 * (`CErrorOutputWindowCtor.cpp`, a proven compiler wall - see that file's header). `docs/research/
 * boot_path.md`'s step-18 row still says all four are missing; that row is stale, and this file is
 * the one that was true.
 *
 * ## The size is the whole shape
 *
 * The caller allocates **20 bytes** (`li r3,20` at 0x800080C8) and `CIOWin` is a vtable pointer
 * plus a 16-byte `rstl::string`, so `CAudioStateWin` adds nothing to its base. The body confirms
 * it: base-constructor call, temporary's destructor, one store, and the prologue. There is no
 * member to initialise, which is why this is 92 bytes where `CErrorOutputWindow`'s 180-byte
 * constructor has six stores.
 *
 * ## Why this file declares its own class rather than including the header
 *
 * The same reason `CConsoleOutputWindowCtor.cpp` and `CErrorOutputWindowCtor.cpp` do, and for the
 * same reason: **`CAudioStateWin` has no key function anywhere in the tree** - its destructor
 * (retail `fn_800E24D0`, 0x800E24D0, 0x5C) and its `OnMessage` (retail `fn_800E2530`, 0x800E2530,
 * 0x94) are both unwritten - so mwcceppc emits no vtable for it, and `__vt__14CAudioStateWin` would
 * be an undefined symbol in the DOL link. Retail's own store is
 * `R_PPC_ADDR16_HA/LO lbl_803B3950` - retail's vtable object, an unclaimed `.data` gap `dtk`
 * fills - so the base is made a *member* and the store is written against the gap object.
 * Composition is layout-identical here: `CIOWin` is 0x14 bytes, the member sits at +0, and
 * `stw r0,0(r31)` overwrites the base's own vptr with the derived one, which is what
 * `stw r0,0(r31)` at 0x800E2608 is.
 *
 * The vtable is `lbl_803B3950` (`config/G2ME01/symbols.txt:18044`, `size:0x20`) and its contents
 * are `0, 0, 0x800E24D0, 0x800E2530, 0x80049E18, 0x80049E14, 0x80049E10` - two header words, then
 * one slot per virtual in declaration order: `~CAudioStateWin`, `OnMessage`, and then
 * `GetIsContinueDraw` / `Draw` / `PreDraw` **not overridden**, so the last three are `CIOWin`'s own
 * at 0x80049E18 / 0x80049E14 / 0x80049E10. That is the same check `CMainFlowAccessors.cpp` uses,
 * and it is what fixes the slot order rather than a guess.
 *
 * ## The name string
 *
 * `addi r4,r4,-30024` off `lis r4,0x803b` is 0x803A8AB8, the 14-character `CAudioStateWin` in
 * `lbl_803A8AB8` (`symbols.txt:17094`). Note the neighbouring `lbl_803A8AB0` is a *different*
 * 7-byte string - the merged-literal tail `??(??)\0`, exactly the object `CMainFlowCtor.cpp` has
 * to step seven bytes into. This constructor points at the string itself, so it needs no offset.
 * The string is a **reference**: the unit claims `.text` only, and the bytes stay retail's.
 */
#include "types.h"

#include "rstl/string.hpp"

extern "C" {
/** `.rodata 0x803A8AB8`, "CAudioStateWin" (14 characters plus a terminator). */
extern const char lbl_803A8AB8[];
/** `.data 0x803B3950`, the vtable object `dtk` fills. */
extern const char lbl_803B3950[];
} // extern "C"

/** `CIOWin` without its virtuals, its name being 16 bytes of storage this unit never reads. */
class CIOWin {
public:
  void* x00_vtable;
  char x04_name[16];
  CIOWin(const rstl::string& inName);
};

class CAudioStateWin {
  CIOWin x00_base;

public:
  CAudioStateWin();
};

CAudioStateWin::CAudioStateWin() : x00_base(rstl::string_l(lbl_803A8AB8)) {
  // 0x800E25FC-0x800E2608: the derived vtable store; see the header for why it is written here.
  *reinterpret_cast< void** >(this) = const_cast< char* >(lbl_803B3950);
}
