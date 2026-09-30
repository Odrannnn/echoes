# progress-prime1-ceulerangles

Re-measured baseline with `./tools/decomp_build.sh MetroidPrime/CEulerAngles`: `main/MetroidPrime/CEulerAngles` was **1/6**; the three queued targets were each **0.00%**. Overall `matched_functions` was **9940/28465**, linked **4896**.

| Function | Before → after | Prime 1 donor result |
| --- | ---: | --- |
| `__sinit_CEulerAngles_cpp` | 0% → 100% | `CEulerAngles::sIdentity(0.f, 0.f, 0.f)` matched unchanged. |
| `FromTransform__12CEulerAnglesFRC12CTransform4f` | 0% → 100% | Prime 1's direct Euler extraction did not match Echoes (0%); adapting to `FromMatrix(CMatrix3f::FromTransform(xf))` reproduced Echoes exactly. |
| `FromQuaternion__12CEulerAnglesFRC11CQuaternion` | 0% → 92.68% | Prime 1's direct Euler extraction did not match Echoes (0%); routing its quaternion-to-matrix result through Echoes' `FromMatrix` raised it to 92.44%, then using this tree's `const CVector3f&` getter raised it to 92.68%. It remains non-exact; the remaining diff is FPU register assignment/instruction scheduling and stack-store order. Copying the vector by value scored 80.65%; explicit scalar locals scored 72.81%; both were reverted. |

Final `main/MetroidPrime/CEulerAngles`: **3/6**; overall matched **9942/28465**, linked unchanged at **4896**. `MP_TOOLCHAIN_DIR=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort ./tools/goal_check.sh build/goal/item.json` passed: gate, report diff, symbol names, target rose `1 -> 3 / 6`, no asm. Did not run `flip_test.sh` (progress item; unit remains `NonMatching`). The remaining 92.68% quaternion function is the only incomplete part; no separate `NEW:` item, since the existing item already targets it.
