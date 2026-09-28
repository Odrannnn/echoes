/**
 * `fn_8015C34C` - retail `.text:0x8015C34C`, `size:0x114` = 276 bytes: `CWorldTransManagerView`'s default
 * constructor.
 *
 * This is the one piece of `CGameState` that boot-path **step 17** needs, and the only thing
 * between `CGameState+0x3C` and a frame. `CGameState`'s own constructor (retail `fn_80144140`)
 * does `li r3,1200` / `__nw__FUlPCcPCc` and `bl fn_8015C34C` on the result, stores it at
 * +0x3C, and separately `new(4)`s the refcount word set to 1 at +0x40 - retail's
 * `rstl::rc_ptr<CWorldTransManagerView>`. `CGameState::GetWorldState()` (retail 0x80142520, 8 bytes:
 * `addi r3,r3,60; blr`) hands out the address of that pair, and every caller dereferences it
 * once before calling a method. See `docs/research/cgamestate_layout.md` for the member map and
 * `include/MetroidPrime/CWorldTransManagerView.hpp` for where each offset comes from.
 *
 * It is an `extern "C"` free function rather than `CWorldTransManagerView::CWorldTransManagerView()` because retail's
 * symbol table has **no name** for it - `config/G2ME01/symbols.txt:5789` calls it `fn_8015C34C`,
 * which is what objdiff pairs on, and a constructor would mangle to `__ct__11CWorldStateFv`
 * with nothing to pair against. It is also why the two non-default-constructible members are
 * built by calling retail's constructors directly rather than by placement `new`; see below.
 *
 * ---------------------------------------------------------------------------
 * Four things here are the whole difference between matching and not.
 *
 * **The float is `lbl_8041C398`, not `1.0f`.** `lfs f0,-24616(r2)` with `_SDA2_BASE_` = 0x804223C0
 * resolves to 0x8041C398, and `.sdata2` there holds `3f800000`. Retail loads it **twice** - once
 * for `+0x00` and once for `+0xC4`/`+0xEC`/`+0xF0`/`+0xF4` - so a literal would materialise it a
 * second way in the middle of the block and change the bytes. It is also the trap
 * `docs/research/rc_ptr.md` records: a *literal* left every function at 100% and every section
 * size correct while `main.dol` grew 32 bytes. `extern "C" float` plus a definition in
 * `PortGlobals.cpp` (the only place a `Matching` unit may not define data) is the fix; the
 * object it names is in the link already, in `build/G2ME01/obj/auto_11_8041C148_sdata2.o`.
 *
 * **The `rstl::string` at +0xD8 has to be written word by word.** `addi r0,r13,-25240` is
 * `(0x8041FD80 - 25240) & 0xFFFF` = 0x9D68 = `mNull__Q24rstl66basic_string<c,...>`, i.e. the
 * address of that class's static `mNull`, and it is a `stw` of that pointer followed by two
 * zero `stw`s - exactly this tree's inline `rstl::basic_string()` constructor
 * (`include/rstl/string.hpp`: `x0_ptr(&mNull), x4_cow(nullptr), x8_size(0)`). **But mwcceppc
 * deletes that construction**, measured: a member with a user-provided constructor whose stores
 * nothing in this function reads is dropped whole, whether the member is an `rstl::string`, a
 * three-word struct with a default constructor, or one with a non-trivial destructor as well -
 * while a *scalar* member written from the function body is always kept, and a class member
 * written from the body is not. Constructing the same string in a mem-init list drops it too,
 * and so does `self->x = rstl::string()`. The three stores are the only spelling that survives,
 * which is why the member keeps its `rstl::string` type and the constructor writes its three
 * words itself.
 *
 * **Getting at `mNull` needed one new public method on `rstl::basic_string`.** There is no public
 * route to it - `c_str()` and `data()` both read `x0_ptr`, so they fold to whatever the constructor
 * left there - and friendship is not available: mwcceppc rejects every friend function declaration
 * inside that class template. `SetEmpty()` in `include/rstl/string.hpp` is the way out, and the
 * measurement behind both facts is recorded there.
 *
 * **+0x468 is copy-constructed from `CTransform4f::sIdentity`, and the address is hoisted.**
 * `lis r3,-32703` / `addi r4,r3,29652` builds 0x804173D4 = `sIdentity__12CTransform4f` and hands
 * it to `__ct__12CTransform4fFRC12CTransform4f` (0x802C9054, 52 bytes) as `r4`, while `r3` is
 * already `this+0x468` from `addi r3,r31,1128`. The two instructions sit near the top of the
 * block, long before the call, which is retail's scheduler.
 *
 * **The two non-default-constructible members are built by calling retail's constructors, not
 * with placement `new`.** `new (&self->xa0_random) CRandom16(99);` is correct C++ and is one
 * instruction too long: the language says placement `new` does nothing when the address is null,
 * so mwcceppc emits `addic. r3,r31,160` (which sets CR0) plus a `beq` over the call, where retail
 * has a bare `addi r3,r31,160` and no branch. The same applies at +0x468. Calling the retail-named
 * constructor directly is what the tree does for unnamed retail functions
 * (`CHintOptionsCtor.cpp` is an `extern "C"` free function for the same reason), and the symbol
 * each call needs is the one retail itself calls, so the relocation is the same.
 *
 * The other two callees are named retail functions, which is what made this the cheapest large
 * thing left on `CGameState`: `__ct__9CRandom16FUi` (0x802C8AC4, 8 bytes) is already a
 * `Matching` unit (`Kyoto/CRandom16.cpp`), and `__ct__12CTransform4fFRC12CTransform4f` is
 * written at 100% inside `Kyoto/Math/CTransform4f.cpp`. A `Matching` unit needs only
 * relocations to its callees, so neither being in the link is what matters here - but they both
 * are.
 *
 * ---------------------------------------------------------------------------
 * **What is left: 272 of 276 bytes, 89.13%, `NonMatching`.** One instruction, and it is dead in
 * retail too - an unused `mr r3,r31` between the `+0x4A4` and `+0x4A8` stores - plus its
 * consequence, which is that mwcceppc's register allocator then gives the bitfield block `r3`
 * (the 1) and `r4` (the 0) where retail uses `r4` and `r5`. The allocator picks the lowest free
 * volatile register, and `r3` is only free here because nothing gave it a value; a *Matching*
 * unit with one fewer instruction is not a Matching unit, so this is `NonMatching` and the range
 * is claimed only so objdiff measures it.
 *
 * What was tried for the `mr` and did not produce it, all measured on this function: an empty
 * inline member function and an empty `static` free function at that point (the `static` one is
 * not inlined and leaves a `bl`); a member function carrying the whole tail; a static member
 * function taking `CWorldTransManagerView*`; a member function with a dummy argument; the flags written
 * through a local pointer; `volatile` lvalues for the `+0x4A4`/`+0x4A8` stores; and all six
 * permutations of the `+0x4A4`, `+0x4A8`, flag-block statements. The register choice is
 * unaffected by all of them.
 *
 * Two smaller differences come out of the same place and are listed so the next lane does not
 * re-measure them: retail's first `lfs f0,-24616(r2)` is its **third** instruction, above the
 * frame setup, where mwcceppc always puts it seventh, immediately before the `stfs` that uses it -
 * and moving the read in the source does not move it; and retail's `addi r0,r13,-25240` (&mNull)
 * and `addi r3,r31,1128` (`this+0x468`) are emitted in the other order from this unit's.
 */

#include "types.h"

#include "rstl/string.hpp"

#include "Kyoto/CRandom16.hpp"
#include "Kyoto/Math/CTransform4f.hpp"

#include "MetroidPrime/CWorldTransManagerView.hpp"

// Placement `new`, for the port's build only - see the macro block below. Under `__MWERKS__` the
// two members are built by calling retail's constructors by name, because placement `new` costs an
// `addic.`/`beq` null check that retail does not have.
#if !defined(__MWERKS__)
#include <new>
#endif

// `lbl_8041C398` - `.sdata2:0x8041C398`, 4 bytes, `3f800000` = 1.0f, owned by no unit
// (`config/G2ME01/splits.txt` has no `.sdata2` block covering it, so dtk's
// `build/G2ME01/obj/auto_11_8041C148_sdata2.o` contributes it to the link). Retail's five
// `lfs` of it are `lfs f0,-24616(r2)`, and `lbl_8041C394` and `lbl_8041C390` either side of it
// are 32.0f and 4096.0f - a run of small float constants in `.sdata2`.
//
// A `Matching` unit may not own data, so this is only declared here. The definition is in
// `src/MetroidPrime/PortGlobals.cpp`, the same place as `lbl_803AFAA0` and the other retail
// read-only objects.
extern "C" float lbl_8041C398;

// Retail's own names for the two constructors this function calls, declared at namespace scope
// with C linkage so the call sites carry retail's relocations. `include/Kyoto/CRandom16.hpp` and
// `include/Kyoto/Math/CTransform4f.hpp` declare them as members, which mwcceppc mangles to
// exactly these two strings; declaring them again here costs a redeclaration and buys the
// placement-free call above. `rstl::construct` is the tree's other spelling for this and it has
// the same placement `new`, hence the same null check.
//
// **MWERKS only, and that is load-bearing for the port's link.** The host compiler mangles the
// *member* definitions to `_ZN9CRandom16C1Ej` and `_ZN12CTransform4fC2ERKS0_`, so a C-linkage
// declaration of the retail spellings would leave `__ct__9CRandom16FUi` and
// `__ct__12CTransform4fFRC12CTransform4f` undefined in the port executable - measured, +2 on
// `tools/link_check.sh`'s undefined count. Under `#else` the calls below are the ordinary C++
// ones, which reach the same two definitions.
#if defined(__MWERKS__)
extern "C" void __ct__9CRandom16FUi(CRandom16* self, uint seed);
extern "C" void __ct__12CTransform4fFRC12CTransform4f(CTransform4f* self, const CTransform4f* src);
#define CTOR_RANDOM16( obj, seed ) __ct__9CRandom16FUi(obj, seed)
#define CTOR_TRANSFORM4F_COPY( obj, src ) __ct__12CTransform4fFRC12CTransform4f(obj, src)
#else
#define CTOR_RANDOM16( obj, seed ) new (obj) CRandom16(seed)
#define CTOR_TRANSFORM4F_COPY( obj, src ) new (obj) CTransform4f(*(src))
#endif

void fn_8015C34C(CWorldTransManagerView* self) {
  // Declaration order, which is retail's store order: +0x00, +0x04, +0x08, +0x0C, +0x18,
  // +0x8C, CRandom16 at +0xA0, +0xAC, +0xB0, +0xB4, +0xB8, +0xB9, +0xBC, +0xC4, the string at
  // +0xD8, +0xEC, +0xF0, +0xF4, +0x2AC, +0x464, the transform at +0x468, +0x4A4, +0x4A8 and
  // then the six flag bits. The padding between them is never touched.
  self->x0_scale = lbl_8041C398;
  self->x4_modelData = nullptr;
  self->x8_unk = 0;
  self->xc_unk = 0;
  self->x18_flag = false;
  self->x8c_flag = false;
  CTOR_RANDOM16(&self->xa0_random, 99);
  self->xac_flag = false;
  self->xb0_seed = 9611;
  self->xb4_unk = 0;
  self->xb8_max = 127;
  self->xb9_min = 64;
  self->xbc_unk = 0;
  // Read once into a local, as retail does: mwcceppc re-reads a non-`const` global after every
  // store it cannot prove does not alias it, and a store to `this+K` is exactly that, so the
  // direct spelling emits five `lfs` where retail emits two. A `const` *local* is worse than
  // nothing - mwcceppc gives it an FPR *pair*, a `xscmpeqdp` and a 16-byte-larger frame - so the
  // local is non-`const` and read after the call, which is where retail's second `lfs` is.
  float one = lbl_8041C398;
  self->xc4_scale = one;
  // `rstl::basic_string()`'s three words, written out: see the note at the top.
  self->xd8_name.SetEmpty();
  self->xec_f0 = one;
  self->xf0_f0 = one;
  self->xf4_f0 = one;
  self->x2ac_flag = false;
  self->x464_flag = false;
  CTOR_TRANSFORM4F_COPY(&self->x468_transform, &CTransform4f::Identity());
  self->x4a4_flag = false;
  self->x4a8_token = nullptr;
  self->x4ac_flags.b0 = true;
  self->x4ac_flags.b1 = false;
  self->x4ac_flags.b2 = false;
  self->x4ac_flags.b3 = false;
  self->x4ac_flags.b4 = false;
  self->x4ac_flags.b8 = false;
}
