// DigitalGuardianPreRender.cpp - a carve of DigitalGuardian's (module 15) .text
// 0x0000A384..0x0000A3A4: `fn_14_A384`, the module's own `PreRender` override.
//
// The range is retail's: `config/G2ME01/rels/DigitalGuardian/symbols.txt:187` carries
// `fn_14_A384 = .text:0x0000A384; // type:function size:0x20`, and `fn_14_A3A4`
// (0xA3A4, 0x30) begins exactly where this claim ends.  The function in front of it,
// `fn_14_A348` (0xA348, 0x3C), ends exactly where this claim starts.  Both neighbours are
// unclaimed, so dtk fills them from retail and the module's sha1 still holds.
//
// **What it is.**  Eight instructions, all of them prologue, one call and epilogue:
//
//   0xA384 stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1)
//   0xA390 bl PreRender__10CPatternedFR13CStateManager
//   0xA394 lwz r0,0x14(r1) / mtlr r0 / addi r1,r1,0x10 / blr
//
// so it is `CPatterned::PreRender(stateManager);` - a forward, with r3 (`this`) and r4
// (`stateManager`) passed straight through and nothing else done.  `PreRender` is the
// vtable slot 7 this module's class takes over from `CActor::PreRender` (0x8004CD24,
// `symbols.txt:1487`); the module's own override drops straight to the `CPatterned`
// implementation rather than doing anything of its own, and the eight bytes say exactly that
// and nothing more.  The module's other `PreRender` forwarder, `fn_14_17A44`, is these eight
// instructions with a different name - see `DigitalGuardianPreRender2.cpp`.
//
// The class is unnamed in retail, so the function below is a free function carrying the two
// pointers the ABI puts in r3 and r4, exactly as in `DigitalGuardianContact.cpp`.  Nothing
// here asserts what either object is.
//
// The name is retail's own `fn_14_A384`, so `symbols.txt` needs no rename.  The one relocation
// is the call to `PreRender__10CPatternedFR13CStateManager`, a DOL symbol the port does not
// define, so the body is behind the `#ifdef __MWERKS__` guard that
// `DigitalGuardianDestroy.cpp` uses and the host object defines nothing.
//
// Definitions are in descending retail text order (mwcceppc emits definitions in reverse
// source order and mwldeppc keeps the object's `.text` order verbatim); there is one function
// here, so `python3 tools/check_decl_order.py --unit
// MetroidPrime/ScriptObjects/DigitalGuardianPreRender.cpp` has nothing to reorder.

extern "C" {

#ifdef __MWERKS__

// .text 0x753AC (module 0), 0x564 bytes: the DOL's `CPatterned::PreRender(CStateManager&)`.
// Declared, never defined here - the DOL supplies it, so the object carries a relocation to a
// symbol the port does not define.
void PreRender__10CPatternedFR13CStateManager(void* self, void* stateManager);

// .text 0xA384, 0x20 bytes.  Both arguments forwarded; `self` is not read here.
void fn_14_A384(void* self, void* stateManager) {
  PreRender__10CPatternedFR13CStateManager(self, stateManager);
}

#endif

} // extern "C"