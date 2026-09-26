// Retail 0x802FA960-0x802FAA20: `CResFactory::Build`, 0xC0 = 192 bytes. One `Matching` unit,
// 100.00%, `tools/flip_test.sh` PASS.
//
// **This is the class's key function in the Itanium sense and the gate for the third frame-0
// vtable, and neither of those turned out to be where the briefing put them.** Two corrections,
// both measured, both worth more than the 192 bytes:
//
//  * **MWCC 2.7 emits a class's vtable into the unit that defines its *first* virtual, not into
//    the unit that defines its key function.** `~CResFactory` is declared out of line in
//    `Kyoto/CResFactory.hpp`, so it is the first virtual, and with it merely *declared* this
//    object's `.data` holds no `__vt__11CResFactory` at all - only the two
//    `__vt__4IObj`/`__vt__31CObjOwnerDerivedFromIObjUntyped` that `CFactoryFnReturn` drags in.
//    So **this unit claims `.text` only**, and the vtable's 0x20 bytes at 0x803BAF08 stay with
//    dtk's fill (`config/G2ME01/symbols.txt` calls it `lbl_803BAF08`; a rename to
//    `__vt__11CResFactory` is what lets retail's own constructor and destructor reach the symbol
//    once it *is* emitted, and is in the tree for that reason).
//  * **`~CResFactory` is written, byte-exact, 0x802FB038-0x802FB0E0, 0xA8 = 168 bytes - and is
//    still not a `Matching` unit**, because the same object then carries `__vt__8IFactory` (0x20
//    bytes of `.data` that retail has at 0x803B19B8 with a **zero** destructor slot) and a weak
//    `__dt__8IFactoryFv` (0x48 bytes of `.text` at 0x802FB0E0, which is retail's `fn_802FB0E0`).
//    `docs/research/paks.md`, "The vtable and the destructor that cannot own it", has both
//    measurements and the dead end.
//
// The vtable is **not** written out in the source and must not be: mwcceppc derives it from the
// class. Its contents, for the record, and all six names are measured rather than guessed -
// compiling the class declaration with `tools/probe_cc.sh` and reading the resulting object's
// `.data` relocations gives them, in this order:
//
//   803baf08  00000000  00000000  __dt__11CResFactoryFv
//   803baf18  BuildAsync__11CResFactoryFRC10SObjectTagRC15CVParamTransferPP4IObj
//            CancelBuild__11CResFactoryFRC10SObjectTag
//            CanBuild__11CResFactoryFRC10SObjectTag
//   803baf1c  GetResourceIdByName__11CResFactoryCFPCc
//
// which linked as 0, 0, 0x802FB038, 0x802FA960, 0x802FA658, 0x802FA490, 0x8008F3C8, 0x80006B80.
// Two zero words (offset-to-top and the typeinfo a `-RTTI off` build zeroes), then one slot per
// virtual in declaration order - **six slots and no NULL**, because `CResFactory` overrides all
// five of `IFactory`'s pure virtuals. MWCC spells a const member `C` immediately before the
// parameter list, which is why the last one is `CFPCc` and not `FPCc`. `config/G2ME01/symbols.txt`
// carries all five names, and two of them - `CanBuild` at 0x8008F3C8 and
// `GetResourceIdByName` at 0x80006B80 - are 36-byte forwarders retail placed in the middle of
// unrelated CGame code; 0x80006B80 is inside `MetroidPrime/main.cpp`'s claimed range, so promoting
// either of them means re-splitting that unit. They are the cheapest thing left in this class.
//
// Retail's bytes:
//
//   802fa960  stwu  r1,-32(r1) / mflr / stw / stw r31 / stw r30 / stw r29 / stw r28
//   802fa988  mr    r28,r3              ; the hidden return slot
//   802fa98c  addi  r3,r1,8
//   802fa990  bl    fn_802FAAE4         ; look the tag up in the map at +0xB4
//   802fa994  lwz   r3,8(r1)            ;   ... into the first word of an 8-byte local
//   802fa998  stw   r3,12(r1)           ; ... and the second word is set to the same value
//   802fa99c  lwz   r0,164(r29)         ; x9c_loading.x8_end, i.e. this+0xA4
//   802fa9a0  cmplw r3,r0
//   802fa9a4  beq   0x802fa9ec          ; not found: straight to the build
//   802fa9a8  lwz   r31,24(r3)          ; found: the entry's +0x18, a `void**`
//   802fa9ac  b     0x802fa9c8
//   802fa9b0  mr    r3,r29 / addi r4,r1,12 / li r5,0 / bl fn_802FA1BC ; the pump
//   802fa9c0  clrlwi. r0,r3,24 / beq 0x802fa9b0   ; a zero goes back to the *pump*
//   802fa9c8  lwz   r3,0(r31) / cmplwi r3,0 / beq 0x802fa9b0
//   802fa9d4  neg r0,r3 / or r0,r0,r3 / srwi r0,r0,31 ; (r3 != 0) as a byte
//   802fa9e0  stb   r0,0(r28)
//   802fa9e4  stw   r3,4(r28)            ; CFactoryFnReturn = one rstl::auto_ptr, 8 bytes
//   802fa9ec  mr r3,r28 / mr r4,r29 / mr r5,r30 / mr r6,r31 / bl fn_802FA7D4
//
// **The three callees are named by address, not written.** `fn_802FA7D4` is 0x18C = 396 bytes and
// is the real build - it calls `CFactoryMgr`'s find, `CResLoader::LoadResourceSync`,
// `CResLoader::LoadNewResourceSync` and both dispatch sites, and it ends in two
// `delete`-through-the-vtable teardowns of a `CInputStream`. `fn_802FA1BC` is 0x12C = 300 bytes and
// is the same pump `AsyncIdle` calls with a non-zero third argument. Neither is written here and
// neither is named in `symbols.txt`; they are `extern "C"` calls on retail's own `fn_` names, the
// arrangement `Kyoto/CResLoaderAddPakFileAsync.cpp` already uses for `fn_802FC350`. In the *port*
// they are three named holes the linker now asks for, which is the price of a real
// `_ZTV11CResFactory` in place of `PortReachStubs.cpp`'s 64 zero bytes - see
// `src/Kyoto/CResFactoryPortVirtuals.cpp`.
//
// **`x9c_loading.x8_end` is the comparison, not a "found" flag.** `fn_802FAAE4` (0x802FAAE4,
// 0x88 bytes) returns `this+0xA4` itself when the map at +0xB4 is empty - `lwz r0,164(r31)` /
// `stw r0,0(r30)` at 0x802FAB40 - and the found node's `+0x18` otherwise
// (`lwz r0,24(r4)` / `stw r0,0(r30)` at 0x802FAB4C). So "not found" is literally
// `entry == &x9c_loading.x8_end`, and that is how it is spelled here.
//
// Order below is retail's .text order reversed, which is what mwcceppc wants: it emits function
// definitions in reverse source order and mwldeppc keeps the object's order verbatim - see the
// lane briefing. This unit has one function, so there is nothing to order.
#include "Kyoto/CResFactory.hpp"

#include "rstl/auto_ptr.hpp"

// `fn_802FAAE4` - .text:0x802FAAE4, size:0x88. Unnamed in the retail map, so it is called by
// address, as `Kyoto/CResLoaderAddPakFileAsync.cpp` calls `fn_802FC350`. It takes its output
// pointer in r3 and `this` in r4, and the tag and the transfer in r5 and r6, which it forwards to
// `fn_802FAB6C` on the member at `CResFactory`+0xB4. **It is declared `void` on purpose**, and
// that is a measured difference rather than a style one: retail's next two instructions are
// `lwz r3,8(r1)` and `stw r3,12(r1)`, and r3 goes straight from that load into the comparison at
// 0x802FA9A0. With a `void*` return mwcceppc treats the returned pointer as a second object and
// stores it on top of what the callee has already written - measured, 31 differing instructions
// against retail, against 0 for `void`.
extern "C" void fn_802FAAE4(void* out, const void* self, const void* tag, const void* xfer);
// `fn_802FA1BC` - .text:0x802FA1BC, size:0x12C. `AsyncIdle` calls it with `(this, &xA0, elapsed)`
// and `Build` with `(this, &entry, 0)`; it returns non-zero once the resource is ready.
extern "C" bool fn_802FA1BC(const void* self, void* entry, unsigned int frames);
// `fn_802FA7D4` - .text:0x802FA7D4, size:0x18C. The synchronous build. It returns a
// `CFactoryFnReturn` by memory, like every function that does, which is why `Build`'s other exit
// hands it its own return slot (`mr r3,r28` at 0x802FA9EC) and has nothing to do afterwards.
extern "C" CFactoryFnReturn fn_802FA7D4(const void* self, const void* tag, const void* xfer);

// The 8-byte frame slot, named. Retail writes word 0 through a pointer (`addi r3,r1,8` at
// 0x802FA98C, `stw r0,0(r30)` inside `fn_802FAAE4`) and word 1 with a load and a store
// (`lwz r3,8(r1)` / `stw r3,12(r1)` at 0x802FA994-0x802FA998), and hands the address of word 1
// to the pump (`addi r4,r1,12` at 0x802FA9B4). The two words are the lookup's result and a copy
// of it; the element's own type is not identified anywhere in the DOL, so the body below spells
// them as two `void*` locals.
//
// The node `fn_802FAAE4` returns, as far as this function reads it: **`x18` is a `void**`, not the
// object.** `lwz r31,24(r3)` at 0x802FA9A8 loads the *pointer to the slot* and `lwz r3,0(r31)` at
// 0x802FA9C8 loads the object out of it, so there are two dereferences and reading `x18` as the
// object itself is one too few - and that one-off was worth 4 differing instructions on its own.
// Everything below +0x18 is unidentified and nothing here reads it, so it is padding.
struct SBuildEntry {
  uchar x0_unknown[0x18];
  void** x18_objectSlot;
};

CFactoryFnReturn CResFactory::Build(const SObjectTag& tag, const CVParamTransfer& xfer) {
  // The two words are two locals. `void* entry[2]` gives the same frame - `r1+8` and `r1+12`,
  // which is what retail's `addi r3,r1,8` at 0x802FA98C and `addi r4,r1,12` at 0x802FA9B4
  // address - and also 100%, but as two named locals they say what they are.
  void* pumpArg;
  void* found;
  fn_802FAAE4(&found, this, &tag, &xfer);
  pumpArg = found;
  if (found != x9c_loading.x8_end) {
    void** objectSlot = ((SBuildEntry*)found)->x18_objectSlot;
    // **The pump's return value is the loop's condition and the payload test is a preheader the
    // loop falls back into.** `clrlwi. r0,r3,24` / `beq 0x802FA9B0` at 0x802FA9C0-0x802FA9C4
    // sends a zero back to the *pump*, and the `lwz r3,0(r31)` / `cmplwi r3,0` /
    // `beq 0x802FA9B0` block at 0x802FA9C8 is entered once from 0x802FA9AC and then only from
    // the pump. Two nested loops are what that is; one
    // `while (*objectSlot == 0 && !pump())` is not, and mwcceppc is the proof - it rotates the
    // other way round and makes the payload test the loop head, which is 35 differing
    // instructions instead of none.
    while (*objectSlot == 0) {
      while (!fn_802FA1BC(this, &pumpArg, 0)) {
      }
    }
    // **A temporary, not a named local.** A named `CFactoryFnReturn` in this frame is
    // initialised, filled, copied into the caller's return slot and then destroyed - mwcceppc
    // emits `__dt__16CFactoryFnReturnFv` for it, 0x50 bytes retail does not have here, and the
    // object it destroys is the frame copy rather than the return slot.
    return CFactoryFnReturn((CObjOwnerDerivedFromIObjUntyped*)*objectSlot);
  }
  return fn_802FA7D4(this, &tag, &xfer);
}
