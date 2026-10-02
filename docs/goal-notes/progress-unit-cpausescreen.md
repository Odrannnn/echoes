# progress-unit-cpausescreen

`kind: progress`, target `MetroidPrime/CPauseScreen` (stays NonMatching; flip_test not run).

## Result

`matched_functions` 9 -> **10** (of 84); project 13079 -> 13080. `./tools/goal_check.sh build/goal/item.json`
-> `goal_check: PASS progress-unit-cpausescreen`. No asm added.

## Change

`src/MetroidPrime/CPauseScreen.cpp` only: added `#include "MetroidPrime/CAnimData.hpp"` and filled in
`DrawModels` (was a `// TODO` stub). `docs/HANDOFF.md` shows modified from the judge's derived-count rewrite, not by me.

## DrawModels__12CPauseScreenCFf: 1.43% -> 100.00%

Body read off the retail object (`build/G2ME01/obj/MetroidPrime/CPauseScreen.o`):
return if `mModels` is empty or `!mModelsReady`; for each non-null, non-`IsNull()` model: return if
`!IsLoaded(0)`, `Touch(kWM_Normal, 0)`, and `PreRender()` the anim data if present; then
`SetFog(false)`, `DrawModelView(mModelTransform, alpha)`, `SetFog(true)`.

Spellings (cumulative):

| spelling | score |
|---|---|
| local `CModelData* data = it->get()`, `if (size != 0 && ready) {...}`, `AnimationData() != nullptr` | 88.57 |
| repeat `it->get()->` per call (retail re-loads `4(r30)` each time; no local) | 96.86 |
| `HasAnimation()` instead of `AnimationData() != nullptr` (matches the `lwz r0; cmplwi r0` test shape) | 98.43 |
| early return `if (size == 0 \|\| !mModelsReady) return;` instead of a wrapping `&&` if | **100.00** |

Tried and no better than 98.43: nested `if`s, `mModelsReady != false`. Retail's `bne; b end` pair comes from the early-return form.

## Not attempted

- `AdvancePage` (344 B) calls unnamed `fn_80210988/fn_80210938/fn_80212BE4/fn_80210ADC/fn_8020D278` (rc_ptr/scan-tree helpers with no GC name), so it can't be written against declared symbols.
- `RestoreTextures`, `TouchVisibleNodes`, `__dt__`, `__ct__`: blockers as recorded in `docs/goal-notes/progress-prime1-cpausescreen.md` (unnamed `fn_8020AE24`/`fn_8020D820`, string-pool addressing). Not re-measured this run.
- Prime 1's `CPauseScreen.cpp` is a different class (see that note); not used.

Helper used: a diff of `objdump -dr` of the retail and our object with branch targets stripped (`/tmp/cmp.sh`, not in the diff).
