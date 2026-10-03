// `fn_8022A0C0` - the loader that builds the object whose vtable is `lbl_803B86B8`, carved out
// of dtk's `auto_03_8022A0C0_text` (the run `auto_03_8022A058_text` was split into by
// `Carve8022A060.c`, this lane's previous item).
//
// .text 0x8022A0C0..0x8022A2CC, 0x20C = 524 bytes, one function, 131 instructions:
// `__ct__20SLdrEditorPropertiesFv`, the six-case property loop, `__nw__FUlPCcPCc`,
// `AllocateUniqueId__13CStateManagerFv`, `LdrToTransform4f__FRC20SLdrEditorProperties`,
// `LdrToEntityInfo__FR11CEntityInfoRC20SLdrEditorProperties`, `fn_8022A408` and
// `__dt__20SLdrEditorPropertiesFv`.
//
// ## What it builds, from retail's own measurements
//
// `lbl_803B86B8` (`.data:0x803B86B8`, `size:0x80`, `symbols.txt:18370`) is a 0x80 = 32-word
// table with one destructor entry (`fn_8022A060`, entry 0x8) and then virtuals; m2c names its
// members `TypesMatch__13CScriptAIHintCFi` (0xC), `PreThink__7CEntityFfR13CStateManager`
// (0x10), `GetValueParm__13CScriptAIHintCFv` (0x160), `SetInUse__13CScriptAIHintFb` (0x16C),
// `GetInUse__13CScriptAIHintCF9TUniqueId` (0x16E) and `GetValueParm2` (0x164), so the class is
// the `CScriptAIHint` branch of `CActor` and the four property cases below are its
// `radius` / `valueParm` / `valueParm2` / `valueParm3`.
//
// `fn_8022A408` (`.text:0x8022A408`, `size:0x13C`, `symbols.txt:9851`) is the constructor
// retail calls: it runs
// `__ct__6CActorF9TUniqueIdRCQ24rstl66basic_string...RC11CEntityInfoUiRC12CTransform4fRC10CModelDataRC13CMaterialListRC16CActorParameters9TUniqueId`
// and then stores, on the derived object, the vptr `lbl_803B86B8`, `+0x158` from argument r8,
// `+0x15C`..`+0x168` from f1..f4, a clear byte at `+0x16C`, `kInvalidUniqueId` at `+0x16E` and
// `lbl_8041DB08` at `+0x170`. **The class itself is unnamed in retail** (its constructor,
// destructor and vtable all carry `fn_`/`lbl_` placeholders, and no `__ct__13CScriptAIHint...`
// or `__dt__13CScriptAIHint...` exists anywhere in `symbols.txt`), so this file calls
// `fn_8022A408` as the free function it is instead of declaring a constructor whose mangled
// name nothing could resolve. The allocation size `0x178` is that constructor's own:
// `+0x170` is the last member and `0x178` is its 8-byte-rounded end.
//
// ## The property block is a local aggregate, not `SLdrAIHint` and not five locals
//
// `include/MetroidPrime/ScriptLoader/SLdrAIHint.hpp` has exactly this layout - `editorProperties`
// at +0x00, `hintType` at +0x3C, the four floats at +0x40..+0x4C - and its `.inc` has exactly
// these six case values (measured equal: `0x255a4580`, `0xb3127b71`, `0x78c507eb`, `0x19028099`,
// `0x2c93aaf5`, `0xe7cf7950`, and mwcceppc builds the same six-comparison tree from them in the
// same shape as retail's). It is not usable as it stands, for two reasons, both measured here:
//
//  - **Its destructor is declared and defined empty** (`SLdrAIHint.hpp:28`), so a `SLdrAIHint`
//    local is torn down with no call at all, where retail's tail is
//    `addi r3,r1,0x40 / li r4,-1 / bl __dt__20SLdrEditorPropertiesFv`.
//  - **Its constructor writes the four floats as `0.0f` literals**, which would put 4 bytes of
//    `.sdata2` in an object of ours that owns none and shift every section after it.
//
// Fixing the generated header would fix both, but it is shared with the port and a generated
// file; `SAIHintBlock` below is the same aggregate stated locally, so this item is one file.
// The layout is retail's either way: `stw r0,0x7c(r1)` and the four
// `stfs f0,0x80..0x8c(r1)` in retail's prologue put `SLdrEditorProperties` at `r1+0x40` (0x3C
// bytes) with `hintType` at `r1+0x7C` and the floats at `r1+0x80`..`r1+0x8C`, i.e.
// `0x40 + offsetof(SLdrAIHint, x)` for every member.
//
// **It has to be an aggregate, not five separate locals.** Written as five locals, mwcceppc
// register-allocates the four floats into the non-volatile f28..f31 across the property loop and
// emits eight `stfd`/`psq_st` pairs in the prologue and eight `psq_l`/`lfd` pairs in the
// epilogue - a 0xE0 frame against retail's 0xB0 and a 0x248-byte function against retail's
// 0x20C, measured. As members they are addressed off `r1` and retail's frame comes out.
//
// ## `lbl_8041DB08` is named, not written as `0.0f`
//
// `.sdata2:0x8041DB08`, `size:0x4`, `data:float`, `symbols.txt:24513`, and it reads `0.0` (read
// out of `main.elf`: `.sdata2` is at file offset `0x3c3c20`, so `0x8041DB08 - 0x8041a3c0` into
// it). A literal would put 4 bytes of `.sdata2` in an object of ours that owns none - the same
// reasoning as `src/MetroidPrime/ScriptObjects/CUnknown90.cpp:38-49` for `lbl_8041D648`.
// Referencing retail's own symbol emits the identical `lfs f0, lbl_8041DB08@sda21(r0)`, which is
// retail's `0x8022A0E4`, and `fn_8022A394` and `fn_8022A408` keep the other two users.
//
// ## The order of the three calls before the constructor, read off retail
//
// `0x8022A254 bl AllocateUniqueId`, `0x8022A268 bl LdrToTransform4f`,
// `0x8022A274 bl LdrToEntityInfo` - `AllocateUniqueId` first, and the transform *before*
// `LdrToEntityInfo` although it is the fifth argument and that is the fourth. So both are
// arguments rather than statements: mwcceppc evaluates this argument list outside-in, taking
// `mgr.AllocateUniqueId()` first and the transform before `LdrToEntityInfo`. Measured the other
// two ways, both wrong:
//
//  - `const TUniqueId uid = mgr.AllocateUniqueId();` as its own statement puts the calls in
//    retail's order but costs a **third** `TUniqueId` slot (`r1+0x08`, `+0x0C` *and* `+0x10`),
//    which pushes the transform to `r1+0x14` and the block to `r1+0x44` - 24 bytes over the
//    claim.
//  - `const CTransform4f& transform = LdrToTransform4f(...)` as its own statement calls
//    `LdrToTransform4f` *before* `AllocateUniqueId`, the reverse of retail's.
//
// Retail passes the 2-byte `TUniqueId` by value through a copy at `r1+0x0C`
// (`lhz r0,0x8(r1) ; sth r0,0xc(r1)`), which the inline spelling gives for free.
//
// **The `static_cast< u16 >` on the property count below is redundant and was measured to change
// nothing here** - removing it leaves the object's `.text` byte-identical (both 0x20C, `fn_8022A0C0`
// still in r27). `CUnknown90.cpp:65-73` needs the same node for the same reason, but this unit
// does not, so it is not claimed as load-bearing.
//
// ## Its own unit
//
// The claim is 0x8022A0C0..0x8022A2CC, the first 0x20C bytes of dtk's `auto_03_8022A0C0_text`
// (which holds `0x8022A0C0..0x8022A3F4`, eight functions). A claim inside an auto unit splits
// it: after this build the listing reads `# 0x8022A2CC..0x8022A3F4 | size: 0x128` (seven) with
// this unit's own first, and `total_functions` is still 28465. `Carve8022A060.c` below at
// 0x8022A060..0x8022A0C0 and `Carve8022A3F4.c` above at 0x8022A3F4 are this lane's other two
// ends of the same run. The directory is retail's own, from those two neighbours.
//
// Source order is descending by address and there is one function, so it cannot be violated
// (`python3 tools/check_decl_order.py --unit MetroidPrime/ScriptLoader/Carve8022A0C0.cpp`).

#include "Kyoto/Math/CTransform4f.hpp"
#include "MetroidPrime/CEntityInfo.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrEditorProperties.hpp"

/// `.sdata2:0x8041DB08`, `symbols.txt:24513`, `data:float`, value 0.0 - see the header.
/// `extern` and not a definition: the matching build takes the four bytes from dtk's object.
extern "C" const float lbl_8041DB08;

namespace {

/** The 0x50-byte block retail's loader builds, and the only reason it exists as a type here.
 *
 *  Layout and field names are `SLdrAIHint`'s, and the header at the top of this file says why
 *  the struct is restated locally rather than included; what is repeated here is the one fact
 *  that decides the shape:
 *
 *  **The five scalars have to live in memory.** Written as five separate locals, mwcceppc
 *  register-allocates the four floats into the non-volatile f28..f31 across the property loop
 *  and emits eight `stfd`/`psq_st` pairs in the prologue and eight `psq_l`/`lfd` pairs in the
 *  epilogue - a 0xE0 frame against retail's 0xB0 and a 0x248-byte function against retail's
 *  0x20C, measured. As members of an aggregate they are addressed off `r1` and retail's frame
 *  and its `stfs f0,0x80..0x8c(r1)` come out.
 */
struct SAIHintBlock {
  SLdrEditorProperties editorProperties;
  int hintType;
  float radius;
  float valueParm;
  float valueParm2;
  float valueParm3;

  SAIHintBlock()
  : editorProperties()
  , hintType(0)
  , radius(lbl_8041DB08)
  , valueParm(lbl_8041DB08)
  , valueParm2(lbl_8041DB08)
  , valueParm3(lbl_8041DB08) {}
  // No user-declared destructor, and that is load-bearing: the implicit one destroys
  // `editorProperties`, which is the single `__dt__20SLdrEditorPropertiesFv` call in retail's
  // tail.  Writing one out - even one that calls the member destructor explicitly - emits that
  // call twice, measured.
};

} // namespace

/// `.rodata:0x803AD338`, the `operator new` placement string this call passes. Claimed by no
/// unit of ours, so the matching build takes it from dtk's `.rodata` object. `const char[]` with
/// no size, not `const char*`, for the same reason as `lbl_803B86B8` at
/// `src/MetroidPrime/ScriptLoader/Carve8022A060.c:153`: an ordinary small-data extern makes
/// mwldeppc refuse the link, and the unknown-size array is what produces retail's
/// `lis r4,@ha` / `addi r4,@l` pair.
extern "C" const char lbl_803AD338[];

/// `__nw__FUlPCcPCc` - mwcceppc's mangling of `operator new(unsigned long, const char*, const
/// char*)`, and retail's own linker name for the allocation at 0x8022A240. Spelled as the
/// three-argument `::operator new` under mwcceppc it is the same call, but off-mwcceppc
/// `include/Kyoto/Alloc/CMemory.hpp:42` includes `<new>` and declares no such overload, so the
/// port build rejects it; the `extern "C"` name is the tree's answer for both, and
/// `src/Kyoto/Alloc/PortMwccNew.cpp:27` defines it for the host. Same spelling and same reason
/// as `src/MetroidPrime/PathFinding/CPathFindArea.cpp:40`.
extern "C" void* __nw__FUlPCcPCc(uint size, const char* file, const char* function);

/// `fn_8022A408` (`.text:0x8022A408`, `size:0x13C`) - the constructor of the object
/// `lbl_803B86B8`'s class, spelled from its own prologue and its call to the `CActor`
/// constructor: r3 `this`, r4 `TUniqueId` by value, r5 `const rstl::string&`, r6
/// `const CEntityInfo&`, r7 `const CTransform4f&`, r8 `uint`, f1..f4 four `float`. It returns
/// `this` (retail's `mr r27,r3` after the call, and `LdrToTransform4f`'s own `__ct__` epilogue
/// shape), so it is a free function here rather than a constructor - a C++ `new` would mangle
/// to `__ct__13CScriptAIHint...`, which is in no symbol table.
extern "C" void* fn_8022A408(void* self, TUniqueId uid, const rstl::string& name,
                             const CEntityInfo& info, const CTransform4f& transform, uint hintType,
                             float radius, float valueParm, float valueParm2, float valueParm3);

/// `.text:0x8022A0C0`, 0x20C bytes: read the `SLdrAIHint` block out of `input`, allocate
/// 0x178 bytes and construct the object over it, or return null. The three arguments are the
/// loader signature of `include/MetroidPrime/ScriptLoader.hpp:23` - retail holds them in r26
/// (`mgr`), r27 (`input`) and r28 (`info`) and keeps them there across the property loop.
/// The returned pointer is the new object, and null when the allocation returned null, which
/// is retail's `mr. r27,r3 / beq` pair.
extern "C" void* fn_8022A0C0(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  SAIHintBlock sldrThis;

  // The explicit `u16` conversion is redundant - `ReadUint16()` already returns `u16`. It is
  // kept because the sibling loaders carry it (`CUnknown90.cpp:74`, and every generated
  // `SLdr*.inc`), but it is measured to be neutral here; see the header.
  const u16 propertyCount = static_cast< u16 >(input.ReadUint16());
  for (int i = 0; i < propertyCount; ++i) {
    // `Get< uint >()`, not `(uint)ReadInt32()`.  They are the same function; the cast spelling
    // makes mwcceppc materialise the id in a sixth register and reload the stream pointer
    // (9 differing bytes), and this loader's reads are retail's.
    const uint propertyId = input.Get< uint >();
    const u16 propertySize = input.ReadUint16();

    switch (propertyId) {
    case 0x255a4580:
      LoadTypedefEditorProperties(sldrThis.editorProperties, input);
      break;
    case 0xb3127b71:
      sldrThis.hintType = input.ReadInt32();
      break;
    case 0x78c507eb:
      sldrThis.radius = input.ReadFloat();
      break;
    case 0x19028099:
      sldrThis.valueParm = input.ReadFloat();
      break;
    case 0x2c93aaf5:
      sldrThis.valueParm2 = input.ReadFloat();
      break;
    case 0xe7cf7950:
      sldrThis.valueParm3 = input.ReadFloat();
      break;
    default:
      input.ReadBytes(nullptr, propertySize);
      break;
    }
  }

  // `operator new`'s size and placement string are retail's (`li r3,0x178`, `li r5,0`), and
  // the null test is the one mwcceppc expands `new` into - see the header.
  void* self = __nw__FUlPCcPCc(0x178, lbl_803AD338, nullptr);
  if (self != nullptr) {
    // Retail's three calls come in this order - `AllocateUniqueId` (0x8022A254),
    // `LdrToTransform4f` (0x8022A268), `LdrToEntityInfo` (0x8022A274) - so the `TUniqueId` and the
    // transform are arguments rather than statements: mwcceppc evaluates the argument list
    // outside-in here, taking `mgr.AllocateUniqueId()` first and the transform (fifth) before
    // `LdrToEntityInfo` (fourth).  Promoting either to its own statement moves one of the calls
    // and, for a named `TUniqueId`, costs a third `r1+0x08`-`0x10` slot and shifts the whole
    // frame.  Measured; see the header.
    self = fn_8022A408(self, mgr.AllocateUniqueId(), sldrThis.editorProperties.name,
                       LdrToEntityInfo(info, sldrThis.editorProperties),
                       LdrToTransform4f(sldrThis.editorProperties), sldrThis.hintType,
                       sldrThis.radius, sldrThis.valueParm, sldrThis.valueParm2,
                       sldrThis.valueParm3);
  }
  return self;
}

#ifndef __MWERKS__
// Host-only definitions of the two data symbols this unit references and no unit of `src/`
// defines.  The matching build does not compile this block, so `main.dol` still takes both from
// dtk's own `.sdata2` / `.rodata` objects and the function keeps retail's bytes; without them the
// port's flat link grows its undefined-symbol count by two and the gate's `port link gap` step
// fails.  Same arrangement and same reason as `src/MetroidPrime/ScriptObjects/CUnknown90.cpp:44-49`
// (`lbl_8041D648`) and `src/MetroidPrime/ScriptLoader/Carve8022A060.c:174-183` (`lbl_803B86B8`).
//
// Both values are read out of `build/G2ME01/main.elf`, not written from memory:
// `lbl_8041DB08` is `.sdata2` at file offset `0x3c7368` and holds `00 00 00 00` = 0.0f;
// `lbl_803AD338` is `.rodata` at file offset `0x3a5038`, `symbols.txt:17570` gives it
// `size:0x7`, and the seven bytes are `3f 3f 28 3f 3f 29 00` = "??(??)" and its terminator.
// Sized at retail's 0x7 on purpose, so nothing in `src/` can read a seventh character that
// retail's table does not have.  `fn_8022A408` is deliberately NOT defined here: it is real
// retail code in dtk's `auto_03_8022A3FC_text` with no source in this tree, so it belongs in
// `docs/research/port_link_gap_list.md` next to `fn_80270A64` and `lbl_8041A3C0` rather than in a
// stub here.
extern "C" const float lbl_8041DB08 = 0.0f;
extern "C" const char lbl_803AD338[7] = "??(??)";
#endif