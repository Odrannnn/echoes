// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:7827-7829`, and the instructions are retail's own at
// `.text 0x801E5230..0x801E52D0`, read out of the disc (`orig/G2ME01/sys/main.dol`, DOL header
// parsed: text section 1 at file offset 0x640 / address 0x80003840 / size 0x3a1c60) and **not** out
// of `build/G2ME01/main.elf`, which holds our bytes once this unit is in the link.
//
// .text 0x801E5230..0x801E52D0, 0xA0 = 160 bytes, 3 functions:
//
//   fn_801E5230    0x801E5230  0x4C   19 instructions   push-front onto the chain at +4 of the
//                                                        receiver: allocate a node through
//                                                        `fn_801E528C`, store the key at +0, link
//                                                        the old head at +4, store the node back
//   fn_801E527C    0x801E527C  0x10    4 instructions   move-to-front on the list at +0 of the
//                                                        first argument: node[1] = *list, *list = node
//   fn_801E528C    0x801E528C  0x44   17 instructions   pop the head of that list and clear it:
//                                                        node = *self, *self = node[1],
//                                                        memset(node, 0, 8), return node
//
// All three are byte-exact in the object: compiled with this unit's own flags (mwcc `GC/2.7`,
// `-O4,p -inline deferred,noauto -lang=c`), `objcopy -O binary --only-section=.text` and compared
// against the disc, each function's `.text` is its retail length with **one** differing word, its
// unresolved call relocation - `fn_801E5230` 76/76 (the `bl fn_801E528C` at 0x20 is `48000001` in
// the object), `fn_801E527C` 16/16 (no calls at all), `fn_801E528C` 68/68 apart from the `bl memset`
// at 0x28, which is `R_PPC_REL24 memset` in the object and retail `4BE1DE4D` -> 0x80003100, the
// address `symbols.txt:1` gives `memset`).  That last one is why this claim can stop at 0x801E52D0
// without a PortLinkStubs entry: the only external symbol in the range is `memset`, which the port's
// link already has.
//
// The chain is the one `fn_801E51A4` (0x801E51A4, 0x8C, still retail's - see below) searches:
// `fn_801E4F9C` (0x801E4F9C, 0xBC) calls `fn_801E5230` directly at 0x801E5028 with
// `addi r3,r28,0x402c` / `mr r4,r29` and drops its result, and the neighbour unit
// `src/MetroidPrime/ScriptObjects/Carve801E515C.c`'s `fn_801E5180` calls it at 0x801E5190 with
// `(char*)self + 4` and the same key argument, returning the node.  `fn_801E527C` and `fn_801E528C`
// have no caller outside this range (checked over `build/G2ME01/asm/`: the two `bl fn_801E527C` at
// 0x801E51E8/0x801E5200 and the one `bl fn_801E528C` at 0x801E5250 are these).
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  `powerpc-eabi-nm --defined-only -n` on the built object must
// read `fn_801E5230 @ 0x0`, `fn_801E527C @ 0x4c`, `fn_801E528C @ 0x5c`.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The file is compiled as C for the port's host build and, like every other source
// in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh` - hence the explicit
// `void*` casts, which are compile-time only and leave the object byte-identical.
//
// In front of the claim, `fn_801E51A4` (0x801E51A4, 0x8C) stays retail's: it is 140/140 bytes with
// retail's exact instruction sequence but a register-allocation diff (24 spellings in the run that
// measured it, best still 12 differing words), so it cannot be in a `Matching` unit and the claim
// starts where the run that does match begins.
//
// The directory is retail's own, taken from the claimed range in front: the neighbour
// `MetroidPrime/ScriptObjects/Carve801E515C.c` (0x801E515C..0x801E51A4) is this file's own callee.

/** `memset` - retail `.init:0x80003100` (`config/G2ME01/symbols.txt:1`, 0x30 = 48 bytes), the one
 *  symbol this range calls outside itself.  Declared rather than included: this file includes
 *  nothing, and the port's own headers are not part of the DOL build. */
extern void* memset(void* dst, int val, unsigned long n);

/** `fn_801E528C` - retail `.text:0x801E528C`, 0x44 = 68 bytes: pop the head of the free list at
 *  +0 of the receiver, link its +4 as the new head, clear the popped node's two words and return
 *  it.  Called only by `fn_801E5230` below. */
void* fn_801E528C(void* self);

void* fn_801E528C(void* self) {
    char* node = *(char**)self;
    *(void**)self = *(void**)(node + 4);
    memset(node, 0, 8);
    return node;
}

/** `fn_801E527C` - retail `.text:0x801E527C`, 0x10 = 16 bytes: move `node` to the front of the
 *  list at +0 of the first argument.  Called only by `fn_801E51A4` (retail's). */
void fn_801E527C(void* list, void* node);

void fn_801E527C(void* list, void* node) {
    *(void**)((char*)node + 4) = *(void**)list;
    *(void**)list = node;
}

/** `fn_801E5230` - retail `.text:0x801E5230`, 0x4C = 76 bytes: the push-front the neighbour unit's
 *  `fn_801E5180` forwards to.  Its node comes from `fn_801E528C`, its key is the caller's own
 *  second argument, and the node is its return value. */
void* fn_801E5230(void* self, void* key);

void* fn_801E5230(void* self, void* key) {
    char* node = (char*)fn_801E528C(*(void**)self);
    *(void**)node = key;
    *(void**)(node + 4) = *(void**)((char*)self + 4);
    *(void**)((char*)self + 4) = node;
    return node;
}
