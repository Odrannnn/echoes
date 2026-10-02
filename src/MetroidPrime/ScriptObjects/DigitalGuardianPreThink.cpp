// DigitalGuardianPreThink.cpp - a carve of DigitalGuardian's (module 15) .text
// 0x0000A688..0x0000A6A8: `fn_14_A688`, the module's own `PreThink` override.
//
// The range is retail's: `config/G2ME01/rels/DigitalGuardian/symbols.txt:192` carries
// `fn_14_A688 = .text:0x0000A688; // type:function size:0x20`, and `fn_14_A6A8`
// (0xA6A8, 0x4F0) begins exactly where this claim ends.  Both neighbours of the claim are
// unclaimed, so dtk fills them from retail and the module's sha1 still holds.
//
// **What it is.**  Eight instructions, all of them prologue, one call and epilogue:
//
//   0xA688 stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1)
//   0xA694 bl PreThink__10CPatternedFfR13CStateManager
//   0xA698 lwz r0,0x14(r1) / mtlr r0 / addi r1,r1,0x10 / blr
//
// so it is `CPatterned::PreThink(time, stateManager);` - a forward, with r3 (`this`),
// f1 (`time`) and r4 (`stateManager`) passed straight through.  That the float argument is the
// one in f1 and not a stack slot is measured, not assumed: the DOL's
// `PreThink__10CPatternedFfR13CStateManager` at 0x80074018 (`symbols.txt:2162`, 0x24 bytes)
// opens with `stfs f1,1848(r3)` and then calls `PreThink__7CEntityFfR13CStateManager`, so the
// parameter is a bare `float` in f1.  `PreThink` is vtable slot 2, which the module's class
// takes over from `CEntity::PreThink`; the module's override adds nothing to the `CPatterned`
// implementation.
//
// The class is unnamed in retail, so the function below is a free function carrying the two
// pointers the ABI puts in r3 and r4 plus the float in f1, exactly as in
// `DigitalGuardianContact.cpp`.  Nothing here asserts what either object is.
//
// The name is retail's own `fn_14_A688`, so `symbols.txt` needs no rename.  The one relocation
// is the call to `PreThink__10CPatternedFfR13CStateManager`, a DOL symbol the port does not
// define, so the body is behind the `#ifdef __MWERKS__` guard that
// `DigitalGuardianDestroy.cpp` uses and the host object defines nothing.
//
// Definitions are in descending retail text order (mwcceppc emits definitions in reverse
// source order and mwldeppc keeps the object's `.text` order verbatim); there is one function
// here, so `python3 tools/check_decl_order.py --unit
// MetroidPrime/ScriptObjects/DigitalGuardianPreThink.cpp` has nothing to reorder.

extern "C" {

#ifdef __MWERKS__

// .text 0x74018 (module 0), 0x24 bytes: the DOL's
// `CPatterned::PreThink(float, CStateManager&)`.  Declared, never defined here - the DOL
// supplies it, so the object carries a relocation to a symbol the port does not define.
void PreThink__10CPatternedFfR13CStateManager(void* self, float time, void* stateManager);

// .text 0xA688, 0x20 bytes.  Every argument forwarded; `self` is not read here.
void fn_14_A688(void* self, float time, void* stateManager) {
  PreThink__10CPatternedFfR13CStateManager(self, time, stateManager);
}

#endif

} // extern "C"