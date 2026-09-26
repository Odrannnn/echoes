/**
 * Host-side storage for the five guest globals that `src/Kyoto/Graphics/Carve*.cpp` reach.
 *
 * **This file is port-only**: `configure.py` does not declare it, so mwcceppc never sees it and
 * the DOL objects are byte-identical with or without it. That is the arrangement
 * `src/Kyoto/CSimplePoolPort.cpp` and `src/MetroidPrime/PortGlobals.cpp` already use, and it is
 * here because the obvious alternative is wrong.
 *
 * **Why not `#ifdef TARGET_PC` in the carve files**, which is how they arrived? Because
 * `TARGET_PC` reaches `mp_game` *only* inside the `if(MP_SDK_HEADERS_ONLY)` branch of
 * `CMakeLists.txt` (line 162), and deliberately so: the `#ifndef TARGET_PC` regions in
 * `src/MetroidPrime/main.cpp` hold bodies the port still needs, so giving `mp_game` `TARGET_PC`
 * unconditionally would delete `CMain::RsMain` from the port build. The two guards cannot both be
 * satisfied by one macro.
 *
 * The cost of getting that wrong was concrete and silent in the sense that matters: the ordinary
 * port link passed, because the headers-only configuration *does* define `TARGET_PC`, so the
 * definitions were compiled in. Only `tools/boot_probe.sh` failed, with
 *
 *     Carve802BEC1C.cpp:27: undefined reference to `lbl_80418AFF'
 *
 * and `build/gate.sh` could not see it, because the gate does not run the probe. So the file
 * looked correct to every instrument the project owns and was wrong in one configuration.
 *
 * **The lesson is the shape of the trap, not the missing symbol:** a host-only definition guarded
 * by a macro that only *some* host configurations define is correct in every configuration that
 * runs the gate and missing in the one that runs the port. A port-only file has no
 * configuration to get wrong.
 *
 * All five are `.bss` or uninitialised `.sdata` in retail, so zero is retail's own value and the
 * bytes are not a claim about anything. If one of them turns out to be non-zero in retail, that is
 * a real finding and belongs in the carve's own header, not here.
 */
#include "Kyoto/Graphics/CGraphics.hpp"

extern "C" {

/**
 * **These have initialisers on purpose.** Inside `extern "C" { }` a bare `extern uchar lbl;` is a
 * *declaration* in C++, not a tentative definition - C gives a tentative definition a common symbol,
 * C++ does not, and the reference stays undefined at link time. That is not hypothetical: it is
 * the same bug that had `extern FScriptLoader REL_loader_CannonBall;` bound as a `FUNC` in
 * `.text` earlier in this project, and the general check is `readelf -sW` - a data symbol showing
 * as `FUNC` in `.text` is it.
 */

/** `CGraphics::GetUseVideoFilter` / `SetUseVideoFilter`'s flag. `.sdata`, 1 byte. */
uchar lbl_80418AFF = 0;

/** `CGX::SetModelMatrix`'s "already set the identity once" flag. `.sdata`, 1 byte. */
uchar lbl_80418AFD = 0;

/** `CGraphics::GetProjectionState`'s return object. `.bss`. */
CGraphics::CProjectionState lbl_80416F28 = CGraphics::CProjectionState(false, 0.f, 0.f, 0.f, 0.f,
                                                                    0.f, 0.f);

/** `CGX::SetModelMatrix`'s destination. `.bss`. */
CTransform4f lbl_80416F74 =
    CTransform4f(CVector3f(0.f, 0.f, 0.f), CVector3f(0.f, 0.f, 0.f), CVector3f(0.f, 0.f, 0.f),
                 CVector3f(0.f, 0.f, 0.f));

/** `CGraphics::SetScreenPosition`'s `GXRenderModeObj` source. `.bss`. */
GXRenderModeObj mRenderModeObj__9CGraphics = { (VITVMode)0 };

/**
 * `CGraphics::SetModelMatrix` compares against this, so it needs an address, which is why it is
 * here rather than a temporary. Retail has the object in `.bss`; see the header note above.
 */
CTransform4f sIdentity__12CTransform4f =
    CTransform4f(CVector3f(0.f, 0.f, 0.f), CVector3f(0.f, 0.f, 0.f), CVector3f(0.f, 0.f, 0.f),
                 CVector3f(0.f, 0.f, 0.f));

} // extern "C"
