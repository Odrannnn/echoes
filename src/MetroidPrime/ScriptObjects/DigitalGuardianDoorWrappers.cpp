// DigitalGuardianDoorWrappers.cpp - a carve of DigitalGuardian's (module 15) .text
// 0x000179E0..0x00017A64: three of the module's own virtual forwarders, `fn_14_179E0`
// (`GetDamageVulnerability`), `fn_14_17A00` (`AddToRenderer`) and `fn_14_17A44`
// (`PreRender`).
//
// The range is retail's: `config/G2ME01/rels/DigitalGuardian/symbols.txt:375-377` carry
// `fn_14_179E0 = .text:0x000179E0; size:0x20`, `fn_14_17A00 = .text:0x00017A00; size:0x44`
// and `fn_14_17A44 = .text:0x00017A44; size:0x20`, each ending exactly where the next
// begins and the last ending exactly where `fn_14_17A64` (0x17A64, 0xC4) begins.  The
// function in front, `fn_14_17974` (0x17974, 0x6C), ends exactly where the claim starts.
// Both neighbours are unclaimed, so dtk fills them from retail and the module's sha1 still
// holds.  One unit claims one contiguous range, which is why the three sit in one file rather
// than three - and why `fn_14_9FE4`/`fn_14_A384`/`fn_14_A688`, the same shapes elsewhere in
// the module, each need a file of their own.
//
// **What they are.**  `fn_14_179E0` and `fn_14_17A44` are eight instructions each - prologue,
// one call, epilogue - and each is a pure forward, so they are `DigitalGuardianVulnerability.cpp`
// and `DigitalGuardianPreRender.cpp` with a different name and a different call target:
//
//   0x179E0 stwu / mflr / stw / bl PassThruVulnerability__20CDamageVulnerabilityFv / lwz /
//          mtlr / addi / blr
//   0x17A44 stwu / mflr / stw / bl PreRender__10CPatternedFR13CStateManager / lwz / mtlr /
//          addi / blr
//
// `fn_14_17A00` is 0x44 bytes and forwards twice, once each way round, which is why it saves
// both argument registers before the first call:
//
//   0x17A00 stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1)
//   0x17A0C stw r31,0xc(r1) / mr r31,r4          ; stateManager, live across the first call
//   0x17A14 stw r30,0x8(r1) / mr r30,r3          ; this, live across the first call
//   0x17A1C bl AddToRenderer__10CPatternedCFRC13CStateManager
//   0x17A20 mr r3,r30 / mr r4,r31                 ; reload the arguments for the second call
//   0x17A28 bl Render__10CPatternedCFRC13CStateManager
//   0x17A2C lwz r0,0x14(r1) / lwz r31,0xc(r1) / lwz r30,0x8(r1) / mtlr r0 /
//          addi r1,r1,0x10 / blr
//
// i.e. `AddToRenderer(stateManager); Render(stateManager);` in one body.  `AddToRenderer` and
// `Render` are vtable slots 8 and the render slot this module's class inherits from
// `CPatterned` (DOL 0x80073F64 and 0x80074F70, `symbols.txt:2161` and `:2185`), and the
// two explicit `mr`s are what fall out of re-reading the parameters after a call rather than
// anything asked for: MWCC keeps the arguments in r30/r31 across the first call because
// neither survives it in a caller-saved register.
//
// The class is unnamed in retail - nothing in these 0x84 bytes names it - so all three
// definitions below are free functions carrying the pointers the ABI puts in r3 and r4,
// exactly as in `DigitalGuardianContact.cpp`.  Nothing here asserts what either object is.
//
// The names are retail's own, so `symbols.txt` needs no rename.  The four relocations are all
// calls to DOL symbols the port does not define, so the bodies are behind the `#ifdef
// __MWERKS__` guard that `DigitalGuardianDestroy.cpp` uses and the host object defines
// nothing.
//
// Definitions are in descending retail text order (mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim), so
// `python3 tools/check_decl_order.py --unit
// MetroidPrime/ScriptObjects/DigitalGuardianDoorWrappers.cpp` has nothing to reorder.

extern "C" {

#ifdef __MWERKS__

// What the ABI carries in r3 on `fn_14_179E0`'s return: the address of the shared
// vulnerability record.  A one-word stand-in, unnamed in retail.
struct SDGDamageVulnerability {
  const void* record;
};

// .text 0xDBB50 (module 0), 0x10 bytes: the DOL's
// `CDamageVulnerability::PassThruVulnerability()`.  Declared, never defined here.
const SDGDamageVulnerability* PassThruVulnerability__20CDamageVulnerabilityFv();
// .text 0x753AC (module 0), 0x564 bytes: the DOL's `CPatterned::PreRender(CStateManager&)`.
void PreRender__10CPatternedFR13CStateManager(void* self, void* stateManager);
// .text 0x73F64 (module 0), 0xB4 bytes: the DOL's `CPatterned::AddToRenderer(...)`.
void AddToRenderer__10CPatternedCFRC13CStateManager(void* self, void* stateManager);
// .text 0x74F70 (module 0), 0x2B4 bytes: the DOL's `CPatterned::Render(...)`.
void Render__10CPatternedCFRC13CStateManager(void* self, void* stateManager);

// .text 0x17A44, 0x20 bytes.  Forward; `self` is not read here.
void fn_14_17A44(void* self, void* stateManager) {
  PreRender__10CPatternedFR13CStateManager(self, stateManager);
}

// .text 0x17A00, 0x44 bytes.  Both arguments are live across the first call, so the body is
// the two forwards in one function and nothing else.
void fn_14_17A00(void* self, void* stateManager) {
  AddToRenderer__10CPatternedCFRC13CStateManager(self, stateManager);
  Render__10CPatternedCFRC13CStateManager(self, stateManager);
}

// .text 0x179E0, 0x20 bytes.  The receiver is passed through untouched and never read.
const SDGDamageVulnerability* fn_14_179E0(void* self) {
  return PassThruVulnerability__20CDamageVulnerabilityFv();
}

#endif

} // extern "C"