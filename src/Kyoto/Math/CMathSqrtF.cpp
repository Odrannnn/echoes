// Retail 0x8001D658, 32 bytes: a stack frame whose whole body is a call to 0x8001D678 and no
// register shuffling at all, which is how MWCC compiles a forwarding wrapper around a function
// that already takes its argument in f1 and returns its result in f1.
//
// 0x8001D678 is the Gekko software square root - `frsqrte` plus three Newton steps - and it is
// the thing `MSL_NO_INLINE_SQRT` (set at the top of `Kyoto/Math/RMathUtils.cpp`) exists to keep
// this call from being inlined. Its 12 callers include `CMath::SqrtF` and `CMath::InvSqrtF` and
// `CActor::GetYaw`, and the port reaches it through all three.
//
// Retail's symbol table has no name for it; the decompilation and the port both call it
// `fn_8001D658`, which is the name the link needs.
extern "C" float fn_8001D678(float x);

#ifdef TARGET_PC
// On the host there is no retail object to resolve 0x8001D678 against, and it is 228 bytes of
// `frsqrte` plus three Newton steps and the libm edge-case classifier, which is not written.
// `libm` already has the function, so the port gets that. This is the `#ifdef TARGET_PC` shape
// `Kyoto/Math/RMathUtils.cpp` already uses for the same call.
extern "C"
float fn_8001D678(float x) {
  return sqrtf(x);
}
#endif

extern "C"
float fn_8001D658(float x) {
  return fn_8001D678(x);
}
