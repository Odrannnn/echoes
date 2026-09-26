/**
 * `AllocateRenderer`, retail 0x8026EF54, 0x9C = 156 bytes, one function.
 *
 * **This is the function that makes `gpRender` non-null.** `CGameGlobalObjects::PostInitialize`
 * (src/MetroidPrime/main.cpp:239) does
 *
 *     renderer = AllocateRenderer(simplePool, osContext, memorySys, resFactory);
 *     gpRender = reinterpret_cast< CCubeRenderer* >(renderer.get());
 *
 * and `gpRender` is `.sbss:0x804192F8`, so until this function exists the pointer the frame loop
 * dereferences at `docs/research/boot_path.md` step 21c is null. `PostInitialize` is already
 * `Matching` at 100.00%, so nothing else has to change for the call to resolve.
 *
 * Retail's own name is already the C++ one - `_Z16AllocateRendererR12IObjectStoreR10COsContextR10CMemorySysR8IFactory`,
 * `config/G2ME01/symbols.txt` line 10822 - so this is a carve of a *named* function and needs no
 * rename. `.text` 0x8026EF54 starts at **exactly** the end of `MetaRender/Carve8026EF24.cpp`
 * (0x8026EF54), so the two units are adjacent and neither has to grow.
 *
 * ## `NonMatching`, and the reason is structural, not a missing spelling
 *
 * 39 of the 156 bytes are exactly right; **8 are not, and no spelling moves them**, because the
 * second argument is a pointer into `.rodata` that **this tree cannot own**:
 *
 * ```
 * 8026ef5c:  lis     r7,0x803b          ; 0x803B0000
 * 8026ef88:  addi    r3,r7,-7236        ; 0x803AE3BC      <- relocated pair
 * 8026ef8c:  addi    r4,r3,86           ; 0x803AE412      <- plain constant add
 * ```
 *
 * Two instructions, and only the `+86` shape produces the second one: a string *literal* in that
 * position compiles to `lis ; addi ; mr` (three instructions, 160-byte function, measured) and a
 * folded constant compiles to `lis ; addi` (154 bytes, measured). `&lbl_803AE3BC[86]` is right and
 * costs nothing - it was measured at **156 bytes, 39 instructions, and 100.00% fuzzy from
 * objdiff** - and it cannot be linked:
 *
 * 1. `lbl_803AE3BC` has to be *defined*, and `.rodata` is the only section covering 0x803AE3BC.
 * 2. `symbols.txt` has one object there, `lbl_803AE3BC = .rodata:0x803AE3BC; size:0xFC`, and
 *    **`dtk dol split` refuses a claim that ends inside a symbol** ("ends within symbol
 *    'lbl_803AE3BC' (0x803AE3BC..0x803AE4B8)"), so the whole 0xFC = 252 bytes is this unit's or
 *    none of it. The 252 bytes are thirteen tail-merged retail strings plus seven zero bytes and
 *    they reproduce exactly as one string literal (measured: 252/252 byte-identical).
 * 3. **0x803AE3BC is 4 (mod 8) and every MWCC data input section is 8-aligned**, so mwldeppc
 *    places this object at 0x803AE3C0 and leaves four zero bytes at 0x803AE3BC. Measured: the
 *    built `main.dol` then differs from retail in 856 `.text` bytes, 6,651 `.rodata` bytes, 10
 *    `.data` and 3 `.sdata`, and `dtk shasum -c` fails. Every `.rodata` symbol boundary in this
 *    neighbourhood is 4 (mod 8) - the nearest 8-aligned one is `lbl_803AE130`, which would mean
 *    claiming **904 bytes** of unrelated tables to save a 156-byte function - so there is no
 *    cheap way round it.
 * 4. Claiming 4 bytes lower, at 0x803AE3B8 so the object is 8-aligned, is refused too: dtk then
 *    reports the *preceding* auto range ending inside `lbl_803AE394`. A claim may not start inside
 *    a symbol either.
 *
 * **So `Matching` here would mean shipping a broken DOL.** The unit is `NonMatching` on purpose.
 * What it does buy is real: the function exists with retail's body, so `PostInitialize` resolves,
 * `renderer` is a real `rstl::unique_ptr` and `gpRender` is no longer null.
 *
 * The string the allocator is handed is `0x803AE412` = `"??(??)"`, retail's own unused debug name
 * (it is `fn_802729C0`'s caller that never reads it - see below). It is written as a literal
 * because owning it is the thing that cannot be done, and the literal's seven bytes of `.rodata`
 * cost nothing in a `NonMatching` unit: dtk links *its* retail object for the DOL
 * (`build/G2ME01/obj/...`), not this one, so the section here never reaches `main.dol`.
 *
 * ## What the body is
 *
 * ```
 * stwu r1,-32(r1) ; mflr r0 ; lis r7,0x803b ; stw r0,36(r1)
 * ... r31=r6 r30=r5 r29=r4 r28=r3 ...          the four arguments, spilled in that order
 * li r5,0
 * addi r3,r7,-7236 ; addi r4,r3,86 ; li r3,1376
 * bl  fn_80272958
 * mr. r4,r3 ; beq
 * mr r4,r28 ; mr r5,r29 ; mr r6,r30 ; mr r7,r31 ; bl fn_80271238 ; mr r4,r3
 * cmplwi r4,0 ; mr r0,r4 ; beq ; addi r0,r4,4
 * stw r0,-29672(r13)        ; 0x80418998
 * mr r3,r4 ... blr
 * ```
 *
 * Three things in it are claims about retail rather than guesses, and each was measured:
 *
 * 1. **`fn_80272958` ignores all three arguments.** Its whole body is `bl fn_802729C0`,
 *    `lwz r4,0(r3) ; addi r0,r4,1 ; stw r0,0(r3) ; bl fn_802729B4` (0x30 bytes), and
 *    `fn_802729C0` (0x802729C0, 0x24 bytes) is a lazy singleton initialiser that touches none of
 *    r3/r4/r5: it tests a `.sbss` byte, stores a pointer into a `.sbss` word and returns
 *    `&that word`. So the second argument is a **name the allocator never reads**, and getting it
 *    byte-right is a linkage requirement, not a behavioural one. **This is also the single most
 *    important fact for the port: `fn_80272958` is a memory-pool allocator, so its `1376` is the
 *    size of the object that comes back.** Both callees are unclaimed in `splits.txt`
 *    (`fn_80272958` 0x80272958 0x30, `fn_80271238` 0x80271238 0x59C), so neither is written yet.
 *
 * 2. **The `.sdata` word at 0x80418998 is `p` or `p + 4`, and the order is load-then-test.**
 *    Retail emits `cmplwi r4,0 ; mr r0,r4 ; beq ; addi r0,r4,4 ; stw r0`, i.e. it writes the
 *    destination register *before* the branch. A `?:` gives `beq ; addi ; b ; mr` (measured, 160
 *    bytes) and assigning the global twice gives a memory round trip through `lbl_80418998`
 *    (measured, 156 bytes but two `stw`s where retail has one). **A local** holding the
 *    conditional add is what produces `mr r0,r4` before the `beq`, and it is byte-exact. The `+4`
 *    is four bytes on a byte-typed pointer; written as `uint` arithmetic it is the same
 *    instruction, and retail's own object is the only evidence the pointer is byte-addressed.
 *
 * 3. **The claim is eight bytes of `.sdata`, not four**, for the same reason as the `.rodata`:
 *    `symbols.txt` line 20055 gives `lbl_80418998` `size:0x8`, and dtk refuses a four-byte claim
 *    on it with the same "ends within symbol" message. The object therefore carries both words -
 *    `0x80419738`, retail's initial value, a pointer into `.sbss` that `symbols.txt` line 20760
 *    names `lbl_80419738` and that no unit owns - and a zero. Nothing in the DOL reads the word;
 *    this unit only writes it, which is why it is spelled as the two words it is.
 *
 * ## What still stands between the port and a frame
 *
 * **`gpRender` stops being null, and that is all this buys.** `fn_80271238` (0x80271238, 0x59C =
 * 1436 bytes) is the constructor that would put a `CCubeRenderer` vtable in the 1376 bytes
 * `fn_80272958` returns, and it has no body. Until it does, the pointer at 0x804192F8 is a real
 * address with no vtable in it and step 21c's `lwz r12,0(r3)` reads whatever the pool contained.
 * After that comes `CCubeRenderer::BeginScene` (0x8026FBFC, 0x180 = 384 bytes, vtable slot 35 =
 * offset 0x94) and the host framebuffer path - `COsContext::GetFramebuf1/2` legitimately answer
 * null until `OpenWindow` runs. **No frame has been rendered by this change and none is claimed.**
 */

#include "MetaRender/IRenderer.hpp"

#include "Kyoto/Alloc/CMemorySys.hpp"
#include "Kyoto/Basics/COsContext.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/IObjectStore.hpp"

// `.sdata:0x80418998`, 8 bytes. See (3) in the header. Typed as `void*` rather than `uint` so the
// host build can store an address into it without a 64-bit-to-32-bit cast diagnostic: MWCC's
// `void*` is 4 bytes, so the object is the same 8 bytes and the same `.sdata` relocation.
extern "C" void* lbl_80418998[2];
extern "C" void* lbl_80418998[2] = {reinterpret_cast< void* >(0x80419738), 0};

// 0x803AE3BC, retail's `.rodata`. **Declared, never defined, in the matching build** - see (2) in
// the header: the only claim that could hold it is refused by dtk and then misplaced by mwldeppc,
// and this unit is `NonMatching`, so dtk links *its* retail object for the DOL and never resolves
// the reference. Under `TARGET_PC` it is defined here, because a host link has no retail object
// and the boot path passes the address straight through to `fn_80272958`.
extern "C" const char lbl_803AE3BC[];
#ifdef TARGET_PC
extern "C" const char lbl_803AE3BC[] =
  "DrawGeometryScanTranslast\0DrawTranslastGeometry\0DrawGeomet"
  "ryScan\0DrawUnsortedGeometry\0??(??)\0TXTR_BigRing\0TXTR_Dar"
  "kWorldCloud\0TXTR_ScanSweepBar\0CMDL_FlatSphere\0CMDL_FlatSp"
  "hereLow\0CMDL_FlatCylinder\0CMDL_FlatCylinderLow\0TXTR_DarkL"
  "ightworldPalette\0\0\0\0\0\0\0";
#endif

// Unclaimed in `config/G2ME01/splits.txt`, so neither has a body in this tree yet.
extern "C" void* fn_80272958(int size, const char* name, void* mem);
extern "C" void* fn_80271238(void* self, IObjectStore&, COsContext&, CMemorySys&, IFactory&);

IRenderer* AllocateRenderer(IObjectStore& store, COsContext& osContext, CMemorySys& memorySys,
                            IFactory& resFactory) {
  // 1376 = 0x560, the pool block size. `&lbl_803AE3BC[86]` is 0x803AE412, retail's own unused
  // debug name; spelled as `lbl_803AE3BC + 86` because the `+86` is a separate `addi` in retail.
  void* p = fn_80272958(1376, lbl_803AE3BC + 86, 0);
  if (p) {
    p = fn_80271238(p, store, osContext, memorySys, resFactory);
  }
  void* q = p;
  if (p) {
    q = static_cast< char* >(p) + 4;
  }
  lbl_80418998[0] = q;
  return static_cast< IRenderer* >(p);
}
