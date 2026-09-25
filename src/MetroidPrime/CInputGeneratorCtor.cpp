// Retail 0x8001DA84-0x8001DAF4: `CInputGenerator::CInputGenerator`, 112 bytes.
//
// It is one of the twelve because `CGameArchitectureSupport`'s constructor reaches it
// (`main.cpp:225`), not because anything calls it by name from the frame loop.
//
// Declared with the single function it holds, so the reverse-source-order rule is trivially met.
#include "MetroidPrime/CInputGenerator.hpp"

#include "Kyoto/Input/IController.hpp"

// 0x8001DA84, 0x70 bytes. Six stores and one call, in this order:
//   `stw r4,0(r31)`                 - x0_context. The reference argument is never taken by
//                                    address: `mr r3,r4` at 0x8001DAAC dereferences the pointer
//                                    the *caller* passed, so retail's is
//                                    `IController::Create(*x0_context)`, and `Create` takes a
//                                    `const COsContext&`.
//   `bl 0x8030B4A8` / `stw r3,4`    - the single_ptr is one word wide on retail, so the
//                                    `single_ptr<IController>` member initialiser compiles to
//                                    the bare store, with no owner pointer beside it.
//   four `stb r0` with r0 = 0       - x8_connectedControllers[0..3], all false. C++ does not allow
//                                    an array in a mem-init list, so this is a `for` in the body and
//                                    mwcceppc unrolls it to the four `stb` retail has, in retail's
//                                    order - after the float stores, which is where retail puts
//                                    them. Nothing else in the body changes.
//   `stfs f30,12` / `stfs f31,16`   - xc_leftDiv and x10_rightDiv, from f1 and f2. f2/f1 are
//                                    callee-saved here (`stfd f31` / `stfd f30`) because the
//                                    `IController::Create` call sits between the argument
//                                    shuffles and the stores.
CInputGenerator::CInputGenerator(COsContext* context, float leftDiv, float rightDiv)
: x0_context(context)
, x4_controller(IController::Create(*context))
, xc_leftDiv(leftDiv)
, x10_rightDiv(rightDiv) {
  for (int i = 0; i < 4; ++i) {
    x8_connectedControllers[i] = false;
  }
}
