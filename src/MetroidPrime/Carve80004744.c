// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:81-82`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80004744_text.s:9-32`, and the body below is the C those
// bytes are the compilation of.
//
// .text 0x80004744..0x80004798, 0x54 = 84 bytes, 1 function:
//
//   fn_80004744    0x80004744  0x54    21 instructions
//
// **It is one step of the deleting-destructor chain this neighbourhood is full of**, and the
// smallest one: `if (self) { Free__7CMemoryFPCv(*(void**)(self + 0xC)); if (flag > 0)
// Free__7CMemoryFPCv(self); } return self;`.  Read off the 21 instructions:
//
//   - `mr. r30,r3` / `beq` is MWCC's receiver guard, and `mr r31,r4` before it is the incoming
//     16-bit flag (`extsh. r0,r31` / `ble` is the `flag > 0` test that decides whether the
//     receiver itself is freed - `li r4,-1` at the call sites means "do not free me afterwards",
//     so a member teardown never reaches the second `Free`).
//   - `lwz r3,0xc(r30)` is the single thing destroyed: the pointer at +0xC, freed through the same
//     `Free__7CMemoryFPCv` that frees the object.  There is **no count load, no stride and no
//     element loop at all**, so the member is a plain pointer, not an `rstl::vector`'s item array.
//   - `mr r3,r30` in the epilogue is the return of the receiver, as in every member of the family.
//
// Its place in the chain is fixed by the three call sites, measured with
// `grep -rn "bl fn_80004744" build/G2ME01/asm/`:
//
//   0x8000429C  in `__dt__10CGameStateFv` (0x8000419C, 0x190, `auto_03_80003BE8_text.s:463`),
//               called on `r30 + 0xF4` with `li r4,-1`, right after the two `0x110`/`0x144` blocks
//               and before `fn_80004678` on `r30 + 0xDC` and `fn_800045A0` on `r30 + 0xC4` - the
//               same 4-function chain `src/MetroidPrime/Carve800045A0.c` documents, one step up.
//   0x800050CC  in `__dt__PersistentOptions_800050A4` (0x800050A4, 0x64,
//               `auto_03_80004D84_text.s:241`), on `r30 + 0x18` with `li r4,-1`.
//   0x801EF754  in `fn_801EF730` (0x801EF730, 0x54, `auto_03_801EF598_text.s:129`), the same shape
//               one level up: it forwards `li r4,-1` to this function and then applies the same
//               `extsh. r0,r31` / `ble` / `Free__7CMemoryFPCv(self)` tail to its own receiver.
//
// The boundary above is the real-named `__dt__9CGameModeFv` (`symbols.txt:82`, 0x80004798, 0x48):
// it stores `__vt__9CGameMode` at +0 and frees `this`, and never touches +0xC, so it is a different
// object rather than another step of this chain.  The boundary below is the end of the
// `Carve800045A0.c` claim (0x800045A0..0x80004744), whose `fn_800046D0` already names this
// address as a neighbour it is not related to.
//
// The callee is declared, never defined here: `Free__7CMemoryFPCv` (0x802CE388, `symbols.txt:12992`,
// size 0x64) is claimed by `Kyoto/Alloc/CMemory.cpp` in the DOL, and
// `src/Kyoto/Alloc/PortMwccNew.cpp:34` defines it for the host, so both of this object's `bl`s
// resolve inside our own tree.
//
// Retail names none of this.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  One definition, so the descending-source-order rule of
// `docs/RUNNING_THE_DECOMP.md` holds trivially; only `tools/flip_test.sh` decides.
//
// Its own unit because a claim may not span an unclaimed gap: `fn_80004744` ends at 0x80004798 and
// `__dt__9CGameModeFv` begins there.
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `MetroidPrime/Carve800045A0.c` (0x800045A0..0x80004744) and above is
// `MetroidPrime/Player/CGameStateBlockDtor.cpp` (0x80004A4C..0x80004AA0), so this address sits in
// the `MetroidPrime/` neighbourhood.

/** 0x802CE388, `symbols.txt:12992`, size 0x64: `CMemory::Free(void const*)`.  Claimed by
 *  `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C), so our own tree supplies it.
 *  Declared, never defined here.  `src/Kyoto/Alloc/PortMwccNew.cpp:34` defines it for the host. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** The object `fn_80004744` tears down.  Only the one field the bytes read is modelled: the
 *  `void*` at +0xC that `lwz r3,0xc(r30)` loads and hands to `Free__7CMemoryFPCv`.  The three words
 *  before it are padding here - nothing in this function reads them - and exist only so the field
 *  lands on retail's displacement.  What they hold is not this function's business: the callers
 *  hand it a member subobject at a fixed offset inside a much larger class. */
struct SBufferHolder {
  int m0;
  int m4;
  int m8;
  void* mC;
};

void* fn_80004744(struct SBufferHolder* self, short flag);

void* fn_80004744(struct SBufferHolder* self, short flag) {
  if (self) {
    Free__7CMemoryFPCv(self->mC);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
