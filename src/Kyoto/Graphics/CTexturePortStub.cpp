/**
 * The port's `CTexture` constructor and destructor. **This file is port-only**:
 * `configure.py` does not declare it, so mwcceppc never sees it and it is not a decompilation
 * unit - the same arrangement as `src/Kyoto/CSimplePoolPort.cpp` and
 * `src/Kyoto/CResFactoryPortVirtuals.cpp`.
 *
 * ## Why it exists
 *
 * `src/MetaRender/Carve80271238.cpp` is `CCubeRenderer`'s constructor, and it builds six
 * `CTexture`s. `CTexture.cpp` is in no manifest, so listing that carve asks the host link for
 * `CTexture::CTexture(ETexelFormat, short, short, int)` and `CTexture::~CTexture()`. Those are
 * retail's `__ct__8CTextureF12ETexelFormatssi` at 0x802C6000 and `__dt__8CTextureF12Ev` beside
 * it, and both are still unclaimed.
 *
 * **It also blocks `tools/boot_probe.sh` outright, which is why this file is not optional
 * polish.** The probe's self-heal declares a missing symbol as `extern "C" void <name>(void)`
 * and skips anything that is not a valid C identifier, printing what it skipped. A demangled
 * C++ signature is not one, so the probe reports
 *
 * ```
 * boot_probe: not declarable as C identifiers (left for a human):
 *   CTexture::CTexture(ETexelFormat,
 *   short,
 *   short,
 *   int)
 * ```
 *
 * and then the relink fails: without a binary the probe cannot print a single boot marker, so a
 * carve that *added* two unstubbable symbols would have cost the whole diagnostic. Defining
 * them here is what keeps the probe runnable.
 *
 * ## The bodies are the class's contract, not retail's instructions
 *
 * Retail's constructor and destructor are not written. What is implemented is the state the
 * header declares and that anything reading a `CTexture` afterwards depends on: the texel
 * format, the two dimensions, one mip level, and zero everywhere else. **No claim is made that
 * this is what 0x802C6000 does** - it is what a `CTexture` has to be for
 * `CCubeRenderer`'s constructor to have run to completion. `GetBitMapData(0)` therefore returns
 * null and `Load` is a no-op, and the bitfields `Lock`/`UnLock` maintain are still their own
 * code (`include/Kyoto/Graphics/CTexture.hpp`, `Lock` is inline).
 *
 * A lane that decompiles `CTexture.cpp` should delete this file and list that one; the two
 * definitions are deliberately the *same* symbols, so there is no duplicate if only one of the
 * two files is ever listed.
 */

#include "types.h"

#include "Kyoto/Graphics/CTexture.hpp"

CTexture::CTexture(ETexelFormat fmt, short w, short h, int mips) {
  mTexelFormat = fmt;
  mWidth = w;
  mHeight = h;
  mNumMips = static_cast< uchar >(mips);
  mBitsPerPixel = 0;
  mLocked = false;
  mCanLoadPalette = false;
  mIsPowerOfTwo = false;
  mNoSwap = false;
  mCounted = false;
  mCanLoadObj = false;
  mMemoryAllocated = 0;
  mNativeFormat = 0;
  mNativeCIFormat = 0;
  mClampMode = kCM_Clamp;
  mFrameAllocated = 0;
}

CTexture::~CTexture() {}
