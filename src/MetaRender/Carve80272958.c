/**
 * `fn_80272958` - the allocator `AllocateRenderer` takes `li r3,1376` from. Retail .text
 * 0x80272958..0x80272988, 0x30 = 48 bytes, one function, unnamed in the matching build's own
 * `splits.txt` and named `fn_80272958` in `config/G2ME01/symbols.txt` (line 10891, `size:0x30`).
 *
 * ## Why it matters to the port, and it is not the reason the file header used to claim
 *
 * `src/MetaRender/Carve8026EF54.cpp` is `AllocateRenderer`, and it is a `Matching`-shaped body
 * that the port links. Its whole job is
 *
 *     void* p = fn_80272958(1376, "??(??)", 0);
 *     if (p) { p = fn_80271238(p, store, osContext, memorySys, resFactory); }
 *
 * so until `fn_80272958` has a body the host calls
 * `src/MetroidPrime/PortReachStubs.cpp`'s **`extern "C" void fn_80272958(void)`**, which returns
 * nothing. `r3` on the way out of that stub is whatever the stub last had, and `AllocateRenderer`
 * stores it in `lbl_80418998[0]` and returns it as `IRenderer*` - which is
 * `docs/research/boot_path.md` step 21c's `lwz r12,148(r12)`.
 *
 * **A stub that returns `void` where the caller reads a pointer is worse than a null pointer, and
 * that is the measurement here**: `gpRender` was "non-null" in the 2026-09-27 run, and the value it
 * held was an artefact of a `printf`. A carve of these 48 bytes replaces it with a real
 * allocation-shaped call.
 *
 * ## The body, and the two facts in it that are worth more than the carve
 *
 * ```
 * 80272958: stwu r1,-16(r1)
 * 8027295c: mflr r0
 * 80272960: stw  r0,20(r1)
 * 80272964: bl   0x802729c0        fn_802729C0
 * 80272968: lwz  r4,0(r3)
 * 8027296c: addi r0,r4,1
 * 80272970: stw  r0,0(r3)
 * 80272974: bl   0x802729b4        fn_802729B4
 * 80272978: lwz  r0,20(r1)
 * 8027297c: mtlr r0
 * 80272980: addi r1,r1,16
 * 80272984: blr
 * ```
 *
 * 1. **It ignores all three arguments.** `r3`, `r4` and `r5` are never read. So the `1376` in
 *    `AllocateRenderer` is not read here either, and the file header of `Carve8026EF54.cpp` -
 *    which says "`fn_80272958` is a memory-pool allocator, so its `1376` is the size of the object
 *    that comes back" - is **wrong about the mechanism and right about the number**. The number
 *    is retail's, and it is the only evidence of `sizeof(CCubeRenderer)` outside the constructor;
 *    the allocator never sees it.
 * 2. **It returns a single fixed address, not a fresh block.** `fn_802729B4` (0x802729B4, 0xC) is
 *
 *    ```
 *    lis  r3,0x803e
 *    addi r3,r3,-4312      ; 0x803DEF28
 *    blr
 *    ```
 *
 *    and **0x803DEF28 is in the DOL's unbacked gap**, between `.data` (ends 0x803C5A10) and
 *    `.sdata` (0x80417D80) - so it is a `.bss` object, not a heap block. `fn_802729C0`
 *    (0x802729C0, 0x24) is a lazy initialiser for a `.sbss` byte at 0x804197CC and a `.sbss` word
 *    at 0x804197C8, and it returns `&that word`. So retail has **one** renderer-shaped object,
 *    refcounted: `fn_80272988` (0x80272988, 0x2C) is the matching decrement, and it is what
 *    `CCubeRenderer`'s deleting destructor calls.
 *
 *    The consequence for the port is worth stating plainly: **writing this function does not give
 *    the port a heap.** `gpRender` will point at one 1376-byte arena, shared, and constructed over.
 *    `src/MetaRender/PortCCubeRenderer.cpp` is the host's translation of `fn_802729B4` and it
 *    returns a static 1376-byte arena, because a fixed arena is what retail has and a
 *    `CMemory::Alloc` would hide the one-block-at-a-time behaviour rather than mirror it. Two
 *    live renderers is not a state retail can reach and neither is this.
 *
 * Neither callee is written, so both are declared here and left undefined. In the matching build
 * that is correct: dtk resolves `R_PPC_REL24` against unclaimed `.text`, and a carve is allowed to
 * call outside its own claim. On the host they are defined in `PortCCubeRenderer.cpp`.
 *
 * ## A `.c`, and why
 *
 * A C++ `fn_80272958` mangles to `_Z12fn_80272958iPKvPv` and objdiff pairs nothing with retail's
 * unmangled `fn_80272958`, so the unit would silently score 0/0 - the trap `docs/` records twice.
 * The body needs nothing but a pointer increment, so C is the honest spelling anyway.
 *
 * ## Measured 2026-09-27 (lane `render2`): byte-identical modulo two `bl` relocations
 *
 * Compiled with the unit's own MWCC flags and compared against the **retail DOL's** bytes at
 * 0x80272958 - not against `main.elf`, which does not contain this claim:
 *
 * ```
 * $ nm -S Carve80272958.o
 * 00000000 00000030 T fn_80272958                     <- 0x30 = 48, retail's size exactly
 * retail 48 bytes @ 0x80272958, ours 48 bytes
 * DIFFERS
 *   +0x0f retail 5d ours 01
 *   +0x1f retail 41 ours 01
 * ```
 *
 * Two bytes, both the top byte of a four-byte `bl` displacement the object leaves as 1 for the
 * linker to fill, and both carrying the right target: `R_PPC_REL24 fn_802729C0` and
 * `R_PPC_REL24 fn_802729B4`. Nothing outside a relocation differs and the length is retail's
 * length, so **this is `Matching` the moment `configure.py` claims
 * `.text:0x80272958..0x80272988`**. That claim is not in the tree; adding it is the
 * orchestrator's merge, not this lane's.
 */

/* 0x802729C0: lazy-initialises a `.sbss` flag and returns the address of a `.sbss` counter word. */
extern void* fn_802729C0(void);
/* 0x802729B4: `return (void*)0x803DEF28;` - one fixed `.bss` arena. */
extern void* fn_802729B4(void);

void* fn_80272958(int size, const char* name, void* mem) {
  int* counter = (int*)fn_802729C0();
  *counter = *counter + 1;
  return fn_802729B4();
}
