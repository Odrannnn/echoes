/**
 * `MetroidPrime/CEulerAngles.cpp` - `configure.py` has listed this unit since the upstream merge
 * and `config/G2ME01/splits.txt` gives it `.text 0x8001D430..0x8001D7B0` plus `.ctors`
 * 0x803A54B4..0x803A54B8, but **the file was absent from the tree**, so the unit compiled to
 * nothing and all six of its functions read as unmatched. This creates it with the one function
 * the merge left lost - `sqrt__Ff` - and no more.
 *
 * `config/G2ME01/symbols.txt` calls the address `sqrt__Ff`, which is the MSL C library's `sqrt`
 * for a `float` argument, and the bytes agree: a standard sixteen-byte-frame wrapper whose only
 * content is `bl msl_sqrtf__Ff` (0x8001D678, the 12-byte `lfs f0,const ; fsqrts ; blr` that
 * immediately follows it in the same unit). So the name is MSL's and not a placeholder, and this
 * is the MSL spelling rather than the four-instruction `sqrtf` the host build wants - the host's
 * `sqrtf` is `src/Kyoto/Math/CMathSqrtF.cpp`, which is compiled by `files.cmake` and not by
 * `configure.py`.
 *
 * The other five functions of the range - `CEulerAngles::FromQuaternion`,
 * `CEulerAngles::FromMatrix`, `msl_sqrtf__Ff`, `CEulerAngles::FromTransform` and
 * `__sinit_CEulerAngles_cpp` - are still missing, and the `.ctors` and `.bss` the split claims are
 * still unclaimed.
 */
extern "C" float msl_sqrtf__Ff(float x);

extern "C" float sqrt__Ff(float x) { return msl_sqrtf__Ff(x); }
