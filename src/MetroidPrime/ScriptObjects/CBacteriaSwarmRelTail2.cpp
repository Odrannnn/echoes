// CBacteriaSwarmRelTail2.cpp - BacteriaSwarm's (module 6) second out-of-line template run, .text
// 0x4020..0x4068: the two short shapes of `CBacteriaSwarmRelTail.cpp`'s pair, over another
// element type:
//
//   0x4020 fn_6_4020  0x20  `rstl::construct` for one element: a frame and one call, nothing else
//   0x4040 fn_6_4040  0x28  `rstl::construct_impl` for one element: the destination null test and
//                              the element's own constructor
//
// The element's constructor is `fn_6_4068` (0x4068, 0x5C), which is **not** one of the item's
// twins - it copies two words and then the record's own `rstl::reserved_vector`-shaped member by
// calling `fn_6_4150`, so it is class code this tree does not model. It therefore stays retail's,
// and this claim stops at 0x4068 rather than spanning it; `fn_6_40C4` above, the
// `~reserved_vector()` instantiation, stays retail's for the reason
// `CBacteriaSwarmRelTail.cpp`'s header gives.
//
// **This is a second unit rather than a second range in the first because one unit cannot claim two
// discontiguous ranges** - `dtk dol split` fails with a link-order cycle when it tries, which is
// why `ScriptCoin` has six files and `CSandwormRelTail.cpp` has a `-2`.
//
// `fn_6_4020` is called from the module's own retail bytes (0x3FE8) and `fn_6_4040` only from
// `fn_6_4020` above, so their names have to be the ones those bytes reference; both are also in the
// module's `force_active` list in `config/G2ME01/config.yml`.
//
// **The bodies are inside `#ifdef __MWERKS__` and the host branch is empty, so listing this file in
// `files.cmake` adds no undefined reference** - the arrangement `CSandBossRelTail.cpp` uses. The one
// callee outside this file, `fn_6_4068`, is module-local, and it is declared, never defined here.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim. Bodies and names are read off
// `build/G2ME01/BacteriaSwarm/asm/auto_00_000000A0_text.s`.

#ifdef __MWERKS__

/** 0x4068, 0x5C: the element's own constructor, the callee of `fn_6_4040` below. It copies two
 *  words and then the record's member at +0x8 by calling `fn_6_4150`, so it is class code this tree
 *  does not model and it is left unclaimed - retail's bytes, in
 *  `auto_00_00004068_text`. Declared under the module's own name, never defined here. */
extern "C" void fn_6_4068(void* self, const void* src);

/** 0x4040, 0x28: `rstl::construct_impl` for one element - `cmplwi r3,0x0` is the null test on the
 *  **destination**, and the element's constructor is then called on it. The twin is
 *  `construct_impl<CPASAnimState>`, i.e. `rstl::construct_impl< CPASAnimState >(dest, src)` folded
 *  into `fn_8002E4B8` at `src/MetroidPrime/CAnimData.cpp:145`; `fn_6_421C` in
 *  `CBacteriaSwarmRelTail.cpp` is the same nine instructions over another element and is at
 *  100.00%, so the body is that one. */
extern "C" void fn_6_4040(void* dest, const void* src) {
  if (dest) {
    fn_6_4068(dest, src);
  }
}

/** 0x4020, 0x20: `rstl::construct` for one element - a frame and one unconditional call, no load
 *  and no test. Eight instructions, the shape of `__sys_free` (0x80008A28,
 *  `src/MetroidPrime/main.cpp`) and of `fn_6_41FC`, apart from the `bl` target. */
extern "C" void fn_6_4020(void* dest, const void* src) { fn_6_4040(dest, src); }

#endif