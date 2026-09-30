// CFogOverlayRelStubs.cpp - FogOverlay's (module 23) two empty virtual overrides, .text
// 0x624..0x62C. Same arrangement as `MetroidPrime/ScriptObjects/CFogOverlayRel.cpp`: one
// contiguous range, one file, only what the object reproduces. The ranges come from
// `config/G2ME01/rels/FogOverlay/symbols.txt`:
//
//   0x624  fn_23_624  0x4  blr
//   0x628  fn_23_628  0x4  blr
//
// **Both bodies really are empty.** These are not stand-ins for unwritten work: each is a single
// `blr` in retail, which is what an override that does nothing compiles to, and each is a
// **vtable entry** of `lbl_23_data_0` (`.data:0x0`, the module's own 0x7C-byte CActor table) -
// `fn_23_628` at vtable offset 0x24 and `fn_23_624` at 0x2C. `build/G2ME01/FogOverlay/asm/
// auto_04_00000000_data.s` stores both, so both are referenced by the module's own data and
// neither is a dead-stripping hazard; dtk's generated `ldscript.lcf` also lists both in its
// FORCEACTIVE set, so no `force_active:` entry is needed in `config/G2ME01/config.yml`.
//
// Which CActor virtual each one overrides is not recoverable from this tree and is not claimed:
// the slots either side are `ClearFluidList__6CActorFR13CStateManager` and
// `PreRenderAllViewports__6CActorFR13CStateManager`, and no header here models a CActor virtual.
// Writing them over the same thirteen-virtual stand-in `CFogOverlayRel.cpp` uses would emit a
// `__vt__` of our own into `.data` and move the module's sha1, so they are declared here as
// the plain `extern "C"` functions retail's symbol table names - which is also what objdiff
// pairs, by name.
//
// Everything else in the module is left unclaimed, so dtk fills it from retail and the module's
// sha1 against `config/G2ME01/config.yml` still holds. `fn_23_62C` (0x62C, 0x484), the class's
// `Draw`, and the fifteen methods after it are the CActor/CPatterned class code this tree does
// not model and stay retail.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100%.

extern "C" {
// .text 0x628, 0x4 bytes. An override that does nothing; see the note above.
void fn_23_628() {}

// .text 0x624, 0x4 bytes. An override that does nothing; see the note above.
void fn_23_624() {}
}
