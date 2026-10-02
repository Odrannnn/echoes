// CIngBlobSwarmFree.cpp - IngBlobSwarm's (module 31) own copy of `__sys_free`, .text
// 0x1A60..0x1A80: 32 bytes, one call to the imported `CMemory::Free`. A third unit in the module,
// the same arrangement as `CIngBlobSwarmVecTail.cpp`: the head claims `.text 0x0..0xD8` and that
// one claims 0x2350..0x23D0, this one claims 0x1A60..0x1A80, and everything else stays unclaimed
// so `dtk` fills it from retail and the module's sha1 against `config/G2ME01/config.yml` holds.
//
//   0x1A60 fn_31_1A60  0x20  stwu / mflr / stw / bl fn_80_81C8 / lwz / mtlr / addi / blr
//
// Every REL module compiles its own `__sys_free`, because the DOL's `CMemory::Free` is in its
// import table. The identical 32 bytes appear at `fn_2_59C`, `fn_1_10D4`, `fn_1_2870` and a dozen
// other modules' auto units, and `src/MetroidPrime/main.cpp`'s
// `extern "C" void __sys_free(const void* ptr) { CMemory::Free(ptr); }` is retail's spelling of
// this shape. `src/MetroidPrime/ScriptObjects/CAtomicAlphaRel.cpp` already landed it in a module
// under the name `RELMain`; there it had to be a module entry point, here nothing calls it.
//
// **The call has to name this call site's own import, and that is `fn_80_81C8`, not
// `Free__7CMemoryFPCv` - measured, and it costs the module's hash to get it wrong.** The module
// uses both spellings for `CMemory::Free`: `build/G2ME01/IngBlobSwarm/asm/*.s` shows `bl
// Free__7CMemoryFPCv` at 0x2558, 0x25AC, 0x2610, 0x2654, 0x2664, 0x26BC and 0x2870, and `bl
// fn_80_81C8` at 0x1A6C, this function's only call. Writing the friendlier
// `Free__7CMemoryFPCv` compiles, links and produces byte-identical `.text` - and the module comes
// out 8 bytes long with 662 differing bytes, all in the import table: dtk records a fixup per
// *import name*, so a call under the other name is a second record for one call site. Retail's
// table holds 42 `CMemory::Free` call records, ours 43, with retail's `fn_80_81C8` entry missing
// and a `Free__7CMemoryFPCv` entry added. The plf carries both strings; `strings
// build/G2ME01/IngBlobSwarm/IngBlobSwarm.plf` is where to read which is which, and the module's
// own disassembly names each call site.
//
// **No `force_active:` entry is needed for this range**, unlike `AIMannedTurret`'s `fn_1_48C0`:
// `fn_31_1A60` is already in the generated `build/G2ME01/IngBlobSwarm/ldscript.lcf` FORCEACTIVE
// block, because something in the module's data stores its address, so mwldeppc keeps it.
//
// This file is listed in `files.cmake`, and its host branch defines nothing at all (the body is
// inside `#ifdef __MWERKS__`), for the reason `tools/check_files_cmake.py` enforces; see
// `CLumiteRelTail.cpp`.
//
// Definitions are in descending retail text order (mwcceppc emits definitions in reverse source
// order); `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CIngBlobSwarmFree.cpp`.

extern "C" {

// This call site's import name. `Free__7CMemoryFPCv` is the same DOL function under the name the
// module's *other* call sites use; see the note above for what naming it wrongly costs.
void fn_80_81C8(const void* ptr);

#ifdef __MWERKS__

// .text 0x1A60, 0x20 bytes. `ptr` in r3; the whole body is the call and retail's own epilogue,
// so it is not a tail call.
void fn_31_1A60(const void* ptr) { fn_80_81C8(ptr); }

#endif
}