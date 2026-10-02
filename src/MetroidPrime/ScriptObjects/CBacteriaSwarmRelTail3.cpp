// CBacteriaSwarmRelTail3.cpp - BacteriaSwarm's (module 6) third out-of-line template run, .text
// 0x5C00..0x5C48: the same two short shapes as `CBacteriaSwarmRelTail2.cpp`, over a third element
// type:
//
//   0x5C00 fn_6_5C00  0x20  `rstl::construct` for one element: a frame and one call, nothing else
//   0x5C20 fn_6_5C20  0x28  `rstl::construct_impl` for one element: the destination null test and
//                             the element's own constructor
//
// The element's constructor is `fn_6_5C48` (0x5C48, 0x90), which is **not** one of the item's twins -
// it builds a `CTransform4f` by calling `__ct__12CTransform4fFRC12CTransform4f` and then copies
// three more blocks, so it is class code this tree does not model. It stays retail's and the claim
// stops at 0x5C48 rather than spanning it. `fn_6_5C00` is called from the module's own retail bytes
// (0x5BEC, 0x7210) and `fn_6_5C20` only from `fn_6_5C00` above, so their names have to be the ones
// those bytes reference; both are also in the module's `force_active` list in
// `config/G2ME01/config.yml`.
//
// **This is a third unit rather than a second range in `CBacteriaSwarmRelTail2.cpp` because one unit
// cannot claim two discontiguous ranges** - `dtk dol split` fails with a link-order cycle when it
// tries, which is why `ScriptCoin` has six files and `CSandwormRelTail.cpp` has a `-2`.
//
// **The bodies are inside `#ifdef __MWERKS__` and the host branch is empty, so listing this file in
// `files.cmake` adds no undefined reference** - the arrangement `CSandBossRelTail.cpp` uses. The one
// callee outside this file, `fn_6_5C48`, is module-local, and it is declared, never defined here.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim. Bodies and names are read off
// `build/G2ME01/BacteriaSwarm/asm/auto_00_000042E8_text.s`.

#ifdef __MWERKS__

/** 0x5C48, 0x90: the element's own constructor, the callee of `fn_6_5C20` below. It builds a
 *  `CTransform4f` and copies three further blocks, so it is class code this tree does not model and
 *  it is left unclaimed - retail's bytes, in `auto_00_00005C48_text`. Declared under the module's own
 *  name, never defined here. */
extern "C" void fn_6_5C48(void* self, const void* src);

/** 0x5C20, 0x28: `rstl::construct_impl` for one element, as `fn_6_4040` in
 *  `CBacteriaSwarmRelTail2.cpp`: the null test is on the **destination** and the element's
 *  constructor is then called on it. */
extern "C" void fn_6_5C20(void* dest, const void* src) {
  if (dest) {
    fn_6_5C48(dest, src);
  }
}

/** 0x5C00, 0x20: `rstl::construct` for one element - a frame and one unconditional call. */
extern "C" void fn_6_5C00(void* dest, const void* src) { fn_6_5C20(dest, src); }

#endif