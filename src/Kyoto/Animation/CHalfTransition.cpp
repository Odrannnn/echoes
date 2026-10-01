/**
 * `CHalfTransition::CHalfTransition(CInputStream&)`, retail `.text:0x8032250C`, 0x70 = 112 bytes.
 *
 * ## Why it is its own unit
 *
 * 0x8032250C..0x8032257C was an **unclaimed gap** in `config/G2ME01/splits.txt`: the unit before
 * it, `Kyoto/Audio/CStreamAudioManager.cpp`, ends at 0x8032250C and the one after it,
 * `Kyoto/Particles/CElectricDescription.cpp`, starts at 0x8032257C, and nothing in between was
 * claimed. dtk filled it with retail's own bytes as `main/auto_03_8032250C_text`, so the DOL
 * linked and hashed to retail with the function *reachable only under its address name*.
 *
 * ## The `symbols.txt` rename is what makes the carve possible
 *
 * `Kyoto/Animation/CAnimationSet.cpp` is `NonMatching`, so `tools/project.py`'s `add_unit` links
 * **dtk's extracted object** for it, not ours - and retail's object calls this constructor by the
 * name the map gives it. Measured before the rename, the build failed with
 *
 *     ### mwldeppc.exe Linker Error:
 *     #   undefined: 'fn_8032250C'
 *     #   Referenced from 'fn_8028D604' in CAnimationSet.o
 *
 * Claiming the range deletes `main/auto_03_8032250C_text`, and the mangled name our unit defines
 * is `__ct__15CHalfTransitionFR12CInputStream`, so the reference had to be renamed to match. That
 * is a one-line change to `config/G2ME01/symbols.txt` and nothing else refers to the old name:
 * `grep -rn 8032250C config/G2ME01/rels/` is empty, so no REL looks this symbol up by name, and
 * `python3 tools/check_symbol_names.py CHalfTransition` reports 0 missing names afterwards.
 * (`scope:global` is stated explicitly, as it is on `__ct__11CTransitionFR12CInputStream` and
 * `__ct__18CMetaTransMetaAnimFR12CInputStream`, the other two stream constructors of this family.)
 *
 * This is also what unblocks `CAnimationSet`: its `fn_8028D524` sits at 94.64% only because the
 * obvious spelling instantiates `CInputStream::Get< CHalfTransition >`, whose COMDAT referenced a
 * `CHalfTransition` stream constructor **no object in the tree defined**. That object is this one.
 *
 * ## The body, and why it is a member-init list
 *
 * ```
 *   8032250c  stwu r1,-32(r1)                frame; the temporary rc_ptr lives at r1+8
 *   80322510  mflr r0 / stw r0,36(r1)
 *   80322518  stw r31,28(r1)
 *   8032251c  mr   r31,r3                    r31 = this
 *   80322520  addi r3,r1,8                   &temporary
 *   80322524  lwz  r5,8(r4)                  in.mPtr
 *   80322528  addi r0,r5,4 / stw r0,8(r4)    in.mPtr += 4
 *   80322530  lwz  r0,0(r5)                  *in.mPtr
 *   80322534  stw  r0,0(r31)                 this->mId
 *   80322538  bl   80295fd8 <CMetaTransFactory::CreateMetaTrans>
 *   8032253c  lwz  r0,8(r1) / stw r0,4(r31)  this->mTrans.mRefCount
 *   80322548  lwz  r0,12(r1) / stw r0,8(r31) this->mTrans.mPtr
 *   80322550  lwz  r5,8(r31) / lwz r4,0(r5)  ++*mTrans.mPtr     <- the copy constructor's AddRef
 *   80322558  addi r0,r4,1 / stw r0,0(r5)
 *   80322560  bl   80031dfc <rstl::rc_ptr<IMetaTrans>::ReleaseData>   <- the temporary's destructor
 *   80322564  lwz  r0,36(r1) / mr r3,r31 / lwz r31,28(r1) / mtlr / addi r1,32 / blr
 * ```
 *
 * The read is `CInputStream::ReadInt32()` inlined - `Get< uint >` in this tree is a call to it -
 * and `rstl::rc_ptr< IMetaTrans >` is `{ const IMetaTrans* mPtr; int* mRefCount; }`, so the
 * temporary is at r1+8 and the member at r31+4.
 *
 * The last four instructions before the epilogue are what pin the spelling: `rc_ptr`'s **copy**
 * constructor (`include/rstl/rc_ptr.hpp`) copies the two words and then `++*mRefCount`, and the
 * temporary's destructor then releases it. Written as an assignment in the body instead,
 * `operator=` would emit a `ReleaseData` on the *uninitialised* member first. So it is a member
 * init list, which is also how `CMetaTransMetaAnim` and `CTransition` are written in this tree.
 *
 * ## The two weak COMDATs, and why they are not a problem
 *
 * `tools/unit_fit.sh` measures 268 bytes against a 112-byte claim and names both:
 *
 *     extra:    +   80  __dt__Q24rstl20rc_ptr<10IMetaTrans>Fv
 *     extra:    +   76  ReleaseData__Q24rstl20rc_ptr<10IMetaTrans>Fv
 *
 * `include/rstl/rc_ptr.hpp` defines `rc_ptr<T>::ReleaseData()` out of line in the header and C++98
 * has no `extern template`, so instantiating `rc_ptr<IMetaTrans>` emits them into this translation
 * unit. **They land after the claim, in the object, and are called, so `-strip_partial` cannot drop
 * them.** This is not new and it is not fatal: `Kyoto/Animation/CTransition.cpp` - `Matching` in
 * `configure.py`, 152-byte claim, `CreateMetaTrans` in the same member-init position - carries the
 * same 156 bytes (`__dt__` at +0x98, `ReleaseData` at +0xE8) past its own claim and is reported
 * `100.00% fuzzy, 100.00% matched (1 / 1 functions)`. Only `tools/flip_test.sh` decides, and it
 * reports `PASS -> kept as Matching` here.
 *
 * Retail's own `ReleaseData__Q24rstl20rc_ptr<10IMetaTrans>Fv` is at 0x80031DFC, inside
 * `MetroidPrime/Factories/CCharacterFactory.cpp`'s claim, so retail emitted its instantiation in
 * another TU. Ours cannot be moved out of this one - see `src/MetroidPrime/Carve80049244.cpp`'s
 * header, which is the full account of that and of why it is a bigger job than a carve.
 *
 * ## Claimed exactly
 *
 * `.text 0x8032250C..0x8032257C`, the whole gap and nothing more. One function, so the
 * descending-declaration-order rule (`python3 tools/check_decl_order.py`) is satisfied trivially.
 */

#include "Kyoto/Animation/CHalfTransition.hpp"

#include "Kyoto/Animation/CMetaTransFactory.hpp"
#include "Kyoto/Streams/CInputStream.hpp"

CHalfTransition::CHalfTransition(CInputStream& in)
: mId(in.Get< uint >())
, mTrans(CMetaTransFactory::CreateMetaTrans(in)) {}
