#include "Kyoto/Animation/CAnimation.hpp"

#include "Kyoto/Animation/CAnimTreeNode.hpp"
#include "Kyoto/Animation/CAnimationDatabase.hpp"
#include "Kyoto/Animation/CAnimationManager.hpp"
#include "Kyoto/Animation/CMetaAnimFactory.hpp"
#include "Kyoto/Animation/IMetaAnim.hpp"
#include "Kyoto/Streams/CInputStream.hpp"

// The three functions below are retail's *unnamed* functions in this unit - the ones dtk names
// `fn_<address>` because retail's own symbol table has no name for them. Same technique, and the
// same reason, as the block at the top of `src/Kyoto/Animation/CAnimationSet.cpp`: mwceppc emits
// the out-of-line copy of a template under its *mangled* name, retail's own copy of the same code
// got the name of the address it sits at, and objdiff pairs functions by name - so each is written
// out by hand under an `extern "C"` name. Nothing here is transcribed disassembly: each body is
// the C++ the tree already spells for the same thing.
//
// Each body was written from the retail disassembly (`tools/dis.sh 0x8028CA3C 32` and the two
// after it) and then measured: all three sit at 100.00% in `build/report.json`, and the unit's
// `.text` is 448 of 448 bytes. `tools/compare_unit.sh` cannot be used for that - it needs retail's
// own `build/G2ME01/obj/.../CAnimation.cpp.o`, which this worktree does not have.
//
// **Definitions are in descending retail offset**, which is what mwcceppc needs in order to emit
// them in ascending order (see "Declare your functions in reverse" in
// `docs/RUNNING_THE_DECOMP.md`, and `tools/check_decl_order.py`):
//
//     fn_8028CAE4  .text:0x8028CAE4  0xA4 = 164 bytes
//     fn_8028CA5C  .text:0x8028CA5C  0x88 = 136 bytes
//     fn_8028CA3C  .text:0x8028CA3C  0x20 =  32 bytes
//     __ct__10CAnimationFR12CInputStream .text:0x8028C9C8
extern "C" rstl::ncrc_ptr< CAnimTreeNode >
fn_8028CAE4(const CAnimationManager* mgr, uint animId, const CMetaAnimTreeBuildOrders& orders);

extern "C" rstl::rc_ptr< IMetaAnim > fn_8028CA5C(const CAnimationManager* mgr, uint animId);

extern "C" TToken< CAnimationDatabase > fn_8028CA3C(const TToken< CAnimationDatabase >* src);

// `fn_8028CAE4` - retail `.text:0x8028CAE4`, 0xA4 = 164 bytes, unnamed. It builds the animation
// tree for one animation in one animation database: it resolves the database through the
// `CAnimationManager`'s own token, asks that database for the meta animation, and asks the meta
// animation for its tree.
//
//     cb10  addi  r3,r1,8 / bl   fn_8028CA3C        ; the token, copied out of the manager
//     cb18  addi  r3,r1,8 / bl   GetObj__6CTokenFv  ; ... then loaded: mObjRef->GetObject()
//     cb20  lwz   r3,4(r3)                          ; ... which is the object's wrapper, so +4 is
//                                                    ;     the CAnimationDatabase itself
//     cb24  mr    r4,r31 / vtable[0] / bctrl          ; GetMetaAnim(animId)
//     cb38  mr    r31,r3                             ; the returned `const rc_ptr<IMetaAnim>&`
//     cb3c  addi  r3,r1,8 / bl   __dt__6CTokenFv     ; the token dies at the end of the
//                                                    //  statement that read the meta animation
//     cb48  lwz   r4,0(r31)                          ; the meta animation
//     cb4c  mr    r3,r28 / mr r6,r30 / addi r5,r29,8 ; sret, orders, &mgr->mSysCtx
//     cb58  vtable[1] / bctrl                        ; GetAnimationTree(sysCtx, orders)
//
// `vtable[0]` is `CAnimationDatabase`'s first virtual (`GetMetaAnim`) and `vtable[1]` is
// `IMetaAnim`'s second (`GetAnimationTree`): an object's vtable pointer addresses the first
// *function*, not the two words of type info in front of it, which is why the load is at +0 and
// +4 rather than +8 and +12. Both come out of the header's own declarations - the vtable is
// another unit's data and is not changed here.
//
// `*token` is `TToken::operator*()` -> `CToken::GetObj()->GetContents()`, which is what produces
// the `bl GetObj` above, and the *temporary* is what puts the token's destructor where retail has
// it: the token dies at the end of the statement that reads the meta animation out of the
// database, i.e. after the `bctrl` at 0x8028CB34 and before the tree is built. The `rc_ptr` that
// the database hands back lives in the database, not in the token, so keeping its address is safe.
extern "C" rstl::ncrc_ptr< CAnimTreeNode >
fn_8028CAE4(const CAnimationManager* mgr, uint animId, const CMetaAnimTreeBuildOrders& orders) {
  const rstl::rc_ptr< IMetaAnim >* meta =
      &(*fn_8028CA3C(&mgr->GetAnimationDatabase()))->GetMetaAnim(animId);
  return (*meta)->GetAnimationTree(mgr->GetSysContext(), orders);
}

// `fn_8028CA5C` - retail `.text:0x8028CA5C`, 0x88 = 136 bytes, unnamed. The same lookup without
// the tree: it returns a counted pointer to the meta animation, which is why the two words of
// `rstl::rc_ptr`'s copy constructor and its `++*mRefCount` are inlined here
//
//     caa0  lwz   r0,0(r3)  / stw r0,0(r30)          ; sret.mPtr     = source.mPtr
//     caac  lwz   r0,4(r3)  / stw r0,4(r30)          ; sret.mRefCount = source.mRefCount
//     cab8  lwz   r6,4(r30) / lwz r5,0(r6) / addi 0 / stw ; ++*sret.mRefCount
//
// and why it is the *constructor* rather than `operator=` - `operator=` compares the two
// pointers first, and retail has no such compare here. That is also why the return type is
// `rc_ptr` by value and not `const rc_ptr&`: a reference would be returned in `r3` and copied by
// the caller, not here.
//
// The token is a temporary, so it dies at the end of the `return` statement - after the value has
// been built, which is where retail's `bl __dt__6CTokenFv` sits (0x8028CAC8). `fn_8028CAE4` above
// takes the address of the `rc_ptr` instead, so its token dies one statement earlier.
extern "C" rstl::rc_ptr< IMetaAnim > fn_8028CA5C(const CAnimationManager* mgr, uint animId) {
  return (*fn_8028CA3C(&mgr->GetAnimationDatabase()))->GetMetaAnim(animId);
}

// `fn_8028CA3C` - retail `.text:0x8028CA3C`, 0x20 = 32 bytes, unnamed. It is the out-of-line copy
// of `TToken< T >::NonConstCopy() const` (the same instantiation is matched under its mangled name
// at `NonConstCopy__29TToken<19CTransitionDatabase>CFv` in `src/Kyoto/Animation/CTreeUtils.cpp`,
// and `NonConstCopy__32TToken<22CAnimationDatabaseGame>CFv` in
// `src/MetroidPrime/Factories/CCharacterFactory.cpp`), so it is a frame and one `bl` to
// `CToken::CToken(const CToken&)` and nothing else.
//
// It is a frame and one call rather than placement new spelled inline: mwcceppc expands
// `new (dest) T(src)` into "call `operator new`, test the result against null, then construct",
// which is 40 bytes and not 32 - the same measurement `fn_8028E1D0` records in
// `src/Kyoto/Animation/CAnimationSet.cpp`. Returning the token by value gives retail's own
// convention instead: the caller passes the destination in `r3` and the source token in `r4`,
// which is exactly the `addi r3,r1,8` / `bl fn_8028CA3C` in the two callers above.
extern "C" TToken< CAnimationDatabase > fn_8028CA3C(const TToken< CAnimationDatabase >* src) {
  return *src;
}

CAnimation::CAnimation(CInputStream& in) : mName(in), mAnim(CMetaAnimFactory::CreateMetaAnim(in)) {}
