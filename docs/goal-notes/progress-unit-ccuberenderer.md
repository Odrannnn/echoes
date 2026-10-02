# progress-unit-ccuberenderer

Result: MetaRender/CCubeRenderer 57 -> 58 / 217 matched (build/report.json). `goal_check.sh` PASS.
Changes in `src/MetaRender/CCubeRenderer.cpp` only (plus `#include "Kyoto/Basics/CBasics.hpp"`).

## DisablePVS__13CCubeRendererFi  67.4% -> 100%
Retail's loop is `CBasics::ZeroMemory(area->mPVSAlpha.data(), area->mPVSAlpha.size())`
(bl 0x8028bd3c), not a byte loop. Matched.

## GetStaticWorldDataSize__13CCubeRendererFv  70.3% -> 97.67% (not 100)
Retail null-checks `area->mTextures` before reading its size; added that. Remaining diff is
only the register numbers of the running sum vs the pointer temp (retail sum=r5, temp=r6; ours
the reverse), and the end iterator ends in r3 in both.
Spellings tried:
- null check inside the original for-loop: 97.67%
- iterator declared before `int size`: 97.67%
- local `const vector* textures = area->mTextures.get()`: 97.00% (worse)

Not attempted: the other listed candidates (ctor, GetScreenMipInfo, SetWorldViewpoint, ...), and
SetWorldLightFadeLevel (retail has an `lfs` const + stack float->int conversion, `fctiwz`; the
disassembly of the retail range shows odd `xsmaddasp` from objdump, which is a rendering artefact
of the data word, so read the raw bytes before spelling it).
