// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:4729-4730`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_8010EE5C_text.s:8-50` (the same range is now
// `build/G2ME01/asm/MetroidPrime/Carve8010EE5C.s`), and the bodies below are the C those bytes are
// the compilation of.
//
// .text 0x8010EE5C..0x8010EEEC, 0x90 = 144 bytes, 2 functions:
//
//   fn_8010EE5C    0x8010EE5C  0x70 = 112 bytes  28 instructions   a CActor's GetTouchBounds
//   fn_8010EECC    0x8010EECC  0x20 =  32 bytes   8 instructions   that class's AcceptScriptMsg
//
// **Both bodies are their twin's own, measured in this tree rather than guessed.**  `fn_8010EE5C`
// is `GetTouchBounds__11CGameCameraCFv` (0x801B062C, 0x70,
// `src/MetroidPrime/Cameras/CGameCamera.cpp:372-374`, `Matching`) instruction for instruction apart
// from the `bl`; that unit's `rstl::optional_object< CAABox > CGameCamera::GetTouchBounds() const
// { return CAABox(GetTranslation(), GetTranslation()); }` is what produced them, and re-running that
// same C under the flags in `build.ninja` gives these bytes.  `fn_8010EECC` is `fn_80004438`
// (0x80004438, 0x20, `src/MetroidPrime/Carve80004438.c`, `Matching`), the same 8 instructions with
// one `bl` target changed.
//
// **What the two are is measured from the vtable, not guessed.**  Both are entries of the *same*
// object, `lbl_803B4BE0` (`build/G2ME01/asm/auto_07_803B4BB0_data.s:31-64`), which is the vtable of
// an unnamed `CActor` subclass: its destructor entry is `fn_8010EEEC`, and `fn_8010EEEC` itself
// stores `lbl_803B4BE0` into `*self` at 0x8010EF18 and calls `__dt__6CActorFv`, so the class is
// retail's own and unnamed.  Counting `.4byte` entries from the top of that table:
//
//   * entry 18 (`.4byte` line 50) is `fn_8010EE5C`, and entry 18 is where `GetTouchBounds` sits in
//     every camera vtable - measured in three of them at once,
//     `build/G2ME01/asm/auto_07_803B75A8_data.s:136`, `auto_07_803B7640_data.s:94` and
//     `auto_07_803B8578_data.s:48`, each immediately after
//     `GetDamageVulnerability__6CActorCFRC9CVector3fRC9CVector3fRC11CDamageInfo` exactly as here.
//   * entry 6 (`.4byte` line 38) is `fn_8010EECC`, the `AcceptScriptMsg` slot every `CActor`
//     subclass overrides, and its body is a frame and one `bl` with the receiver still in r3 and
//     r4/r5 untouched - so it forwards to `CActor::AcceptScriptMsg`
//     (0x8004B71C, `symbols.txt:1446`) unchanged.
//
// **`addi r4,r4,0x54` and the same pointer passed twice is `mPosition`**, measured from both ends:
// `include/MetroidPrime/CActor.hpp:290` declares `mutable CVector3f mPosition;  // x54` and
// `GetTranslation()` (`:143`) returns it, and the callee
// `__ct__6CAABoxFRC9CVector3fRC9CVector3f` (0x802F8CC4, `symbols.txt:13712`) takes
// `CVector3f const&` twice.  Nothing here decides *which* point the class uses; retail's
// `GetTouchBounds` is a degenerate box at the object origin in the three cameras that share the
// twin and there is no measurement here that says otherwise for this one.
//
// **The returned value is `rstl::optional_object< CAABox >`, sized from the stores.**  `stb r0,1`
// lands at +0x18 of the destination and the six word copies fill +0x00..+0x17, and
// `include/rstl/optional_object.hpp:79-80` puts `uchar m_data[sizeof(T)]` first and the
// `ATTRIBUTE_ALIGN(4)` `bool m_valid` behind it, so the whole is 0x1C bytes over a
// `CHECK_SIZEOF(CAABox, 0x18)` box (`include/Kyoto/Math/CAABox.hpp:122`).  The value arrives in r3
// with `this` in r4, which is the MWCC hidden-return-pointer shape for a member function.
//
// **`fn_8010EE5C` takes its return slot as an explicit first argument, and that is deliberate.**
// Written the way the C++ twin reads - a named `OptBox` local and `return` - MWCC 2.7 in `-lang=c`
// materialises the local in the frame and copies it to `*sret` on the way out: 44 instructions in a
// 0x50 frame against retail's 28 in 0x30, with the six stores landing at 0x8010EE60..0x8010EE74
// offsets shifted (measured in this run on a scratch object).  Writing through the slot keeps
// retail's register use exactly: r3 held across the call in r31, the flag stored before the copy.
// The copy itself has to be **two 12-byte member assignments, not one 24-byte one**: a single
// `ret->box = box` compiles to two loads then two stores three times over (measured 8 differing
// instructions, retail interleaves load/store one word at a time), while
// `ret->box.min = box.min; ret->box.max = box.max;` is retail's sequence word for word.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  `python3 tools/check_decl_order.py --unit
// main/MetroidPrime/Carve8010EE5C` is the cheap check.
//
// Retail names neither of these.  `symbols.txt` carries the `fn_<addr>` placeholders and this file
// reproduces those symbols verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`, and why the box and the optional are spelled as local structs here instead of
// `#include`ing `Kyoto/Math/CAABox.hpp` - a C++ header in a `.c` unit drags the C++ ABI in and the
// two locals have to match retail's layout by hand.  The local structs are the retail layout and
// nothing more; they are not `CAABox` or `rstl::optional_object< CAABox >`, whose real definitions
// live in the C++ headers and are not claimed to match.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk `dol
// split` fails with "Cyclic dependency ... link order").  Below this claim is
// `MetroidPrime/Carve8010EE54.c` (0x8010EE54..0x8010EE5C) and above it `MetroidPrime/
// CMapWorldInfo.cpp` (0x8010F084..0x80110B18); the 0x120 bytes between them, 0x8010EEEC..0x8010F084,
// are still unclaimed and stay that way.
//
// The directory is retail's own, taken from the nearest claimed ranges: both neighbours above and
// below are `MetroidPrime`.  For an anonymous function that is the only evidence there is, and it
// beats a lane picking the directory it happened to own.
//
// **Both callees are declared and never defined here, and each needs one answer for two builds.**
// This unit is in `files.cmake`, so the PC build compiles it with the *host* compiler while the
// matching build uses mwcceppc, and `__MWERKS__` is the discriminator the tree already uses for
// that split (`src/MetroidPrime/Carve80004438.c:78-81`, `src/MetroidPrime/ScriptObjects/
// Carve801FEAE0.cpp:250`).
//   * `CAABox::CAABox(CVector3f const&, CVector3f const&)` **is** in the port - the port's own
//     `src/Kyoto/Math/CAABox.cpp:16` - but the host compiler mangles it the Itanium way, so under
//     `#else` the call below names `_ZN6CAABoxC1ERK9CVector3fS2_` (measured with
//     `powerpc-eabi-nm` on `build-port-link/CMakeFiles/mp_game.dir/src/Kyoto/Math/CAABox.cpp.o`)
//     and the port gets a real box.  Only the matching build needs retail's linker name, and dtk's
//     own object provides that definition there.
//   * `CActor::AcceptScriptMsg` has **no** definition anywhere in `src/`, in either build, so for
//     the port link it has an announced empty-body stand-in at the end of
//     `src/MetroidPrime/PortLinkStubs.cpp` (`stub_carve8010ee5c_0`).  That stand-in drops every
//     script message for this class; it is unreachable in the port today anyway, because the only
//     thing that reaches these two bodies is the vtable `lbl_803B4BE0`, a dtk-only data object
//     with no claimed unit, so there is no instance of the class for the port to dispatch on.
//     Nothing here claims either callee is decompiled.

typedef struct {
  float x;
  float y;
  float z;
} Vec3f;

typedef struct {
  Vec3f min;
  Vec3f max;
} Box24;

typedef struct {
  Box24 box;
  unsigned char has;
} OptBox;

/** 0x8010EECC, `symbols.txt:4730`, 0x20 = 32 bytes: the class's `AcceptScriptMsg`, a frame and
 *  one call to `CActor::AcceptScriptMsg` and nothing else.  `CScriptMsg` is spelled `const void*`
 *  because this file never dereferences it - the byte for byte twin `fn_80004438`
 *  (`src/MetroidPrime/Carve80004438.c`) forwards its single pointer the same way. */
extern void AcceptScriptMsg__6CActorFR13CStateManagerRC10CScriptMsg(void* self, void* mgr,
                                                                   const void* msg);

void fn_8010EECC(void* self, void* mgr, const void* msg);

void fn_8010EECC(void* self, void* mgr, const void* msg) {
  AcceptScriptMsg__6CActorFR13CStateManagerRC10CScriptMsg(self, mgr, msg);
}

/** 0x8010EE5C, `symbols.txt:4729`, 0x70 = 112 bytes: the class's `GetTouchBounds() const`, a
 *  degenerate `rstl::optional_object< CAABox >` at `mPosition`.  `ret` is retail's hidden return
 *  pointer, which is the register r3 the body parks in r31 across the call.
 *
 *  The callee is retail's own linker name in the matching build (0x802F8CC4, `symbols.txt:13712`)
 *  and the port's own mangled constructor outside it.  Both are the same six float stores: the
 *  port's `CAABox::CAABox` is `min(min), max(max)` and nothing else. */
#ifdef __MWERKS__
extern void __ct__6CAABoxFRC9CVector3fRC9CVector3f(Box24* self, const Vec3f* min, const Vec3f* max);
#define ct_caaabox __ct__6CAABoxFRC9CVector3fRC9CVector3f
#else
extern void _ZN6CAABoxC1ERK9CVector3fS2_(Box24* self, const Vec3f* min, const Vec3f* max);
#define ct_caaabox _ZN6CAABoxC1ERK9CVector3fS2_
#endif

void fn_8010EE5C(OptBox* ret, void* self);

void fn_8010EE5C(OptBox* ret, void* self) {
  Box24 box;
  const Vec3f* pos = (const Vec3f*)((const char*)self + 0x54);
  ct_caaabox(&box, pos, pos);
  ret->has = 1;
  ret->box.min = box.min;
  ret->box.max = box.max;
}
