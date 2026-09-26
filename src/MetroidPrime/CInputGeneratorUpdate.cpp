/**
 * `CInputGenerator::Update` - retail `fn_8001D888`, 0x8001D888, 0x1FC = 508 bytes, the largest
 * single symbol in the frame loop's list.
 *
 * **`NonMatching`, blocked on one symbol this lane may not touch.** The body is retail's; what
 * stops it being `Matching` is that its `queue.Push(msg)` is
 * `Push__18CArchitectureQueueFRC20CArchitectureMessage` at 0x80007A80, and
 * `tools/range_owner.py` says that range belongs to **`MetroidPrime/main.cpp`** - a `NonMatching`
 * unit, so nothing in the DOL link defines it and a `Matching` unit calling it would not link.
 * `src/MetroidPrime/main.cpp` is another lane's file. The unit claims its retail range so objdiff
 * measures it, which is safe: a `NonMatching` object is not in the link.
 *
 * ## What the three virtual calls are, and how it is known
 *
 * `__vt__11IController` is `.data:0x803BB030` and `__vt__18CDolphinController` is
 * `.data:0x803BB068`, both `size:0x20`, and the second reads
 *
 *   803bb068  00000000 00000000 8030bdf4 8030bd1c      <- [0]=0 [1]=0 [2]=dt [3]=0x8030bd1c
 *   803bb078  8030b5e0 8030b5cc 8030b5bc 8030b58c      <-    [4] [5]     [6]     [7]
 *
 * so the vptr points at `[0]` and `lwz rX,12/16/20(r12)` are `[3]`, `[4]`, `[5]` - the first, second
 * and third pure virtual of the header's own declaration order, which is `Poll`,
 * `GetDeviceCount`, `GetGamepadData`. Nothing had to be renamed or guessed.
 *
 * The order of the first two is retail's and is not the order one would write:
 * `GetDeviceCount()` is called **first** (0x8001D8D4, its result is the loop bound) and `Poll()`
 * second with the result discarded (0x8001D8EC).
 *
 * ## The shape
 *
 * - `lwz r3,0(r3) ; bl fn_8028C058 ; clrlwi. r0,r3,24 ; bne` - `fn_8028C058` is `li r3,1 ; blr`
 *   (0x8028C058, 8 bytes), so this is a "is input enabled" stub and the test is the **low byte** of
 *   its result. Zero returns `false` from the top of the function.
 * - `x4_controller` null returns `true` (`li r3,1` at 0x8001DA64, shared with the fall-through).
 * - Four callee-saved registers hold loop-invariant addresses of the four
 *   `CArchitectureMessage` temporaries' `rc_ptr`s: r30 = r1+0x24, r29 = r1+0x44, r28 = r1+0x14,
 *   r27 = r1+0x34. The temporaries themselves are at r1+0x1c, r1+0x3c, r1+0x0c and r1+0x2c, in
 *   pairs: mwcceppc 2.7 does not elide the copy out of a return value, so each
 *   `CArchitectureMessage msg = factory();` is a return slot plus a copy-initialised local, and the
 *   slot's destructor runs at the end of the full expression - between the copy and the `Push`.
 *   This is the same mechanism as `CIOWinManager::PumpMessages`, and it is why there are four
 *   messages and not two.
 * - Four **dead** `beq`s, each testing one of those hoisted addresses against zero. Retail has them
 *   and they are reproduced here rather than "fixed".
 * - `x8_connectedControllers[i]` is read with `lbzx r0,r23,r26` where `r26 = i + 8`, i.e. it is a
 *   `bool[4]` at `this+8`, and it is compared against `data->x0_present` - **not** against
 *   `x1_justDisconnected`, which only takes part in the "should I build a user-input message"
 *   test at the top of the body.
 * - The parm `fn_80048CF4` allocates is **48 bytes**: `li r3,48 ; bl __nw__FUlPCcPCc` at
 *   0x80048D28. The buffer `fn_80306BB0` fills is at `r1+0x4c`, and this frame's saved registers
 *   start at `r1+0x7c`, so 0x4c + 0x30 = 0x7c exactly. Retail's class is not named in the map, so
 *   it is spelled as an opaque 0x30 buffer here; the size is what is load-bearing.
 * - The device index is passed as a **`short`** (`sth r25,10(r1)` at 0x8001D9E0), and the
 *   connected flag as a one-byte `bool` whose address is taken (`stb r3,8(r1)` at 0x8001D9D0 and
 *   a reload `lbz r0,8(r1)` at 0x8001DA40 after the call).
 */

#include "Kyoto/Input/CControllerGamepadData.hpp"

#include "MetroidPrime/CArchitectureQueue.hpp"

#include "MetroidPrime/CInputGenerator.hpp"

// All four are unnamed in config/G2ME01/symbols.txt and are called through `extern "C"`, so the
// port-side spelling stays the map's rather than a guessed C++ name.
//
//   fn_8028C058  8 bytes, `li r3,1 ; blr` - "is input enabled" for a COsContext.
//   fn_80306BB0  fills a 48-byte parm from (index, gamepad data, dt, leftDiv, rightDiv).
//   fn_80048CF4  returns a CArchitectureMessage by value: its return slot is r3 and the two
//                arguments are r4 and r5, which is the Itanium indirect-return convention for a
//                non-trivial class, so declaring the return type is what produces `addi r3,r1,28`.
//   fn_80048C08  the same with three arguments, r4/r5/r6.
extern "C" int fn_8028C058(const void* context);
extern "C" void fn_80306BB0(void* out, int index, const CControllerGamepadData* data, float dt,
                            float leftDiv, float rightDiv);
extern "C" CArchitectureMessage fn_80048CF4(int type, const void* parm);
extern "C" CArchitectureMessage fn_80048C08(int type, const short* device, const bool* connected);

namespace {
// 48 bytes, which is what `fn_80048CF4` allocates. Not named in the map.
struct SInputParm {
  uchar x00[0x30];
};
} // namespace

bool CInputGenerator::Update(float dt, CArchitectureQueue& queue) {
  if ((fn_8028C058(x0_context) & 0xFF) == 0) {
    return false;
  }
  if (x4_controller.get() == nullptr) {
    return true;
  }
  int const count = static_cast< int >(x4_controller->GetDeviceCount());
  x4_controller->Poll();

  for (int i = 0; i < count; ++i) {
    CControllerGamepadData& data = x4_controller->GetGamepadData(i);
    if (data.DeviceIsPresent() || data.DeviceJustDisconnected()) {
      SInputParm parm;
      fn_80306BB0(&parm, i, &data, dt, xc_leftDiv, x10_rightDiv);
      CArchitectureMessage msg = fn_80048CF4(1, &parm);
      queue.Push(msg);
    }
    // `connected` is assigned *before* the test, not inside the body: retail stores it with the
    // byte it has just loaded (`stb r3,8(r1)` at 0x8001D9D0, before the `cmplw` at 0x8001D9D8) and
    // reloads it after the call (`lbz r0,8(r1)` at 0x8001DA40). Declaring it inside the `if` puts
    // the store after the `addi`s, which is four instructions out of place - and it also costs a
    // register, because `data` then has to stay live across the test instead of dying at it, which
    // pushes the frame from 176 bytes to 192 and `this` from r23 to r22.
    bool const connected = data.DeviceIsPresent();
    if (x8_connectedControllers[i] != connected) {
      short const device = static_cast< short >(i);
      CArchitectureMessage msg = fn_80048C08(1, &device, &connected);
      queue.Push(msg);
      x8_connectedControllers[i] = connected;
    }
  }
  return true;
}
