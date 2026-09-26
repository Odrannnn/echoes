// `CGraphics::GetProjectionState` - retail `GetProjectionState__9CGraphicsFv`,
// .text 0x802BF59C..0x802BF5A8, 12 bytes:
//
//     802bf59c:  3c 60 80 41   lis   r3,-32703      ; 0x8041 << 16
//     802bf5a0:  38 63 6f 28   addi  r3,r3,28456     ; r3 = 0x80416F28
//     802bf5a4:  4e 80 00 20   blr
//
// The address it hands back is `CGraphics::mProj`, retail's `mProj__9CGraphics`,
// and **the `lis`+`addi` pair is the point of the note above it**: `mProj` is a
// `CProjectionState`, 0x1C bytes, so it is not small-data eligible and mwcceppc
// materialises its address in two instructions. The 12-byte `GetUseVideoFilter`
// carve next door reads a one-byte `.sdata` word and gets a single SDA21 instead -
// same class, same file, two different addressing forms chosen by the global's
// declared size. That is the rule that bites the lookup tables in `Kyoto/Graphics`
// and `Kyoto/Text`, and here it is a whole 12-byte function rather than a diff.
//
// `mProj` is `.bss`, and the next symbol in `.bss` is `mViewMatrix__9CGraphics` at
// 0x80416F44, so `mProj` is exactly 0x80416F28..0x80416F44 = 0x1C bytes, which is
// `sizeof(CProjectionState)`: `bool x0_persp` then six `float`s. That is the header's
// declaration order, and `fn_802BF5A8` - the projection setup that follows it, and
// which reads `lbz`+six `lfs` out of the same object at +0, +4, +8, +0xC, +0x10,
// +0x14, +0x18 - agrees with it.
//
// As in `Carve802BEC1C.cpp`, the global is reached by its dtk label. `mProj` has no
// definition anywhere in the tree, `CGraphics` has no `.cpp`, and a `Matching`
// object is linked into the DOL where only the retail name exists.
#include "Kyoto/Graphics/CGraphics.hpp"

extern "C" {
extern CGraphics::CProjectionState lbl_80416F28;
}

#ifdef TARGET_PC
// Host-side storage for the guest global this unit (and `Carve802BF9C8.cpp`) returns a
// reference to, so the port build links. mwcceppc does not define `TARGET_PC`, so it
// never sees this and the DOL object is unchanged - the same arrangement as
// `src/Kyoto/Basics/CStopwatchCSWData.cpp` uses for the CPU-frequency register.
//
// All zeros, and that is retail's own value: `mProj` is `.bss:0x80416F28`, and the next
// symbol in `.bss` is `mViewMatrix__9CGraphics` at 0x80416F44, so retail's own binary
// carries 0x1C zero bytes there until `CGraphics::SetProjectionState` runs. A host with
// no VI has a perspective flag of false, which is the same 0.
extern "C" CGraphics::CProjectionState lbl_80416F28 =
    CGraphics::CProjectionState(false, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
#endif

const CGraphics::CProjectionState& CGraphics::GetProjectionState() { return lbl_80416F28; }
