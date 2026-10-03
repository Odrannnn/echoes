// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:7831-7832`, the instructions are retail's own at
// `.text 0x801E5458..0x801E555C` (read out of the disc with
// `python3 tools/dol_read.py 0x801E5458 0x104 orig/G2ME01/sys/main.dol` - **not** out of
// `build/G2ME01/main.elf`, which holds our bytes once this unit is in the link), and the bodies
// below are the source those 260 bytes are the compilation of.
//
// .text 0x801E5458..0x801E555C, 0x104 = 260 bytes, 2 functions:
//
//   fn_801E5510  0x801E5510  0x4C = 76 bytes   19 instructions
//   fn_801E5458  0x801E5458  0xB8 = 184 bytes  46 instructions
//
// **Both are byte-shape twins of matched code in this tree, and both are byte-exact.**  The twins
// are the two template instantiations `rstl::vector<CGameHintInfo::SHintLocation,
// rstl::rmemory_allocator>::reserve(int)` and
// `rstl::uninitialized_copy<rstl::pointer_iterator<CGameHintInfo::SHintLocation, ...>,
// CGameHintInfo::SHintLocation*>`, both at 100.00% in `src/MetroidPrime/CGameHintInfo.cpp`
// (47/47 functions, `build/report.json`); in the built object they are
// `reserve__Q24rstl67vector<Q213CGameHintInfo13SHintLocation,Q24rstl17rmemory_allocator>Fi` at
// 0x16EC and the `t`-local `uninitialized_copy<...PQ213CGameHintInfo13SHintLocation>__4rstl...` at
// 0x17A4 of `build/G2ME01/src/MetroidPrime/CGameHintInfo.o`.  Same instructions, same registers,
// same frame, apart from the three `bl` targets.  Retail's two copies here are the same two
// templates instantiated for a different 16-byte element type - see the `SPortalState` paragraph
// below for which one, and why nothing in the bytes needs to know.
//
// The whole 260 bytes were checked word by word against the disc: **253 of 260 bytes identical**,
// and the 7 that differ are the low three bytes of the three `bl` displacements
// (`48 11 86 31` -> `allocate__Q24rstl17rmemory_allocatorFi`, `48 00 00 51` -> `fn_801E5510`,
// `48 0E 8E A1` -> `Free__7CMemoryFPCv`), which in the object are `R_PPC_REL24` relocations the
// linker fills.  `tools/flip_test.sh` is the acceptance test; `tools/carve_diff.sh` is only
// evidence while the unit is out of the link, for the reason `src/MetroidPrime/ScriptLoader/
// Carve80233A90.cpp` records.
//
// **What these two functions are.**  `rstl::vector<T, rstl::rmemory_allocator>::reserve(int)` and
// the `rstl::uninitialized_copy` it calls, for `T = CPortalArea::SPortalState`.  The only caller
// is retail's `CPortalArea::CPortalArea(const TLockedToken<CPortalAreaData>&)` at 0x801E5058
// (`build/G2ME01/asm/auto_03_801E3E38_text.s:1297-1340`): at 0x801E50D4 it passes `r3 =
// this + 0x401C` and `r4 = *(token + 4)`, and `CPortalArea::mPortals` is the member at 0x401C
// (`include/MetroidPrime/CPortalArea.hpp:45`).  The loop that follows in the same constructor
// writes one 16-byte element per iteration - `{-1, this, 0, token->mItems + i * 0x2C}` - which is
// `SPortalState { int x0_; SActorList { SActorPool* mPool; SActorNode* mHead; } mActors;
// const void* mPortalData; }` (`CPortalArea.hpp:30-39`) with `x0_ = -1`, `mPool = this`,
// `mHead = nullptr`: 4 + 8 + 4 = 0x10, the element size the two functions step by (`addi r6,r6,0x10`
// in the copy, `addi r4,r4,0x10` in the destroy, `slwi r3,r30,4` in the allocation).  So the
// element's *contents* are known from the tree; only the container's name is not, and it does not
// reach the code.
//
// **The two element accesses are word copies, not calls.**  `is_trivially_destructible<
// SPortalState>::value` is false - the primary template in `include/rstl/construct.hpp:26-29` says
// so, and `SPortalState` is a nested type this port never names for the compiler - so
// `construct`/`destroy` go through the placement-new path and `destroy_impl`'s **loop survives**
// (`construct.hpp:101-109`).  That is the whole of `fn_801E5510`'s 4 `lwz`/`stw` pairs and the
// whole of the empty loop in `fn_801E5458`; `SPortalState`'s own destructor is trivial, so both
// bodies come out empty and the loop is `addi r4,r4,0x10 / cmplw r4,r0 / bne` with nothing in it.
// The empty `for` in `fn_801E5458` is therefore **not** dropped work - it is the loop retail
// compiles, and removing it deletes 3 of the 46 instructions.  The `cmplwi r5,0 / beq` guard in
// the copy is `construct_impl`'s placement new (`construct.hpp:53-56`).
//
// **The iterator arguments are spelled as compound literals, and that is load-bearing.**  Retail
// writes each of the two `rstl::pointer_iterator` values into **two** stack words and passes the
// address of the second - `end` at 0x8 and 0xC with `r4 = r1+0xC`, `begin` at 0x10 and 0x14 with
// `r3 = r1+0x14` - and it loads `mItems` **twice** (0x801E5494 into `r4` for `end()`'s
// `data() + mCount`, and again at 0x801E54B0 into `r0` for `begin()`), which is 13 of the 46
// instructions.  Measured here, with everything else identical:
//
//   * two named `Iter` locals assigned before the call  ->  45 instructions, one `lwz r0,12(r29)`
//     (the two loads are CSE'd) and the argument slots at 0x8/0xC instead of 0xC/0x14;
//   * two **compound literals** `(Iter){...}` as the arguments  ->  46 instructions and the
//     0x8/0xC/0x10/0x14 layout retail has, word for word.
//
// So the two `(Iter){...}` below are not decoration: they are what keeps the argument objects as
// temporaries instead of named locals, which is the difference between the two numbers above.
//
// **`fn_801E5510`'s registers are set by leaving `end` where the parameter put it.**  Retail has
// `lwz r6,0(r3)` (the copy cursor in `r6`), `lwz r0,0(r4)` (the end cursor in `r0`) and `r3` as
// the per-word load scratch.  Hoisting `end.current` into a named local instead - the spelling any
// C programmer reaches for - is still 19 instructions but gives `r3`/`r4` for the cursors and `r0`
// as the scratch (measured).  So `end.current` is read from the parameter inside the loop test
// below, and only `begin` is copied out.
//
// **The layout of the container follows `rstl/vector.hpp:16-22`** (`mAllocator` at +0, `mCount` at
// +4, `mCapacity` at +8, `mItems` at +0xC): retail's first load is `lwz r0,0x8(r3)` for the capacity
// test and its last two stores are `stw r31,0xc(r29)` / `stw r30,0x8(r29)`.  `x0_allocator` below
// is the 4 bytes of `rstl::rmemory_allocator`, which is empty, and it is written out only to put
// `mCount` at +4; it is never read.  `allocate(int)` is the out-of-line
// `rstl::rmemory_allocator::allocate` (`rmemory_allocator.hpp:12`) and `deallocate` is
// `CMemory::Free` (`rmemory_allocator.hpp:34-43`), which is why the two external callees are
// retail's own symbols rather than anything local.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  `powerpc-eabi-nm -n` on the built object must read
// `fn_801E5458 @ 0x0` and `fn_801E5510 @ 0xb8`.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>...` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp` (29 of the tree's carve units are `.c`; `Carve80233A90.cpp` is the measured
// exception and its header says why).  The file includes nothing and is compiled for the port's
// host build as C++ by `tools/probe_sources.sh`, hence the explicit casts and the `unsigned int`
// typedef rather than the DOL build's `uint`.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges in one section (dtk `dol split` fails with "Cyclic dependency ... link
// order").  The claim starts at `fn_801E5458` and stops at 0x801E555C, where `symbols.txt:7833`
// names the next function, so nothing outside the item's two functions is taken.  The nearest
// claimed range below is `MetroidPrime/ScriptObjects/Carve801E5230.c` (0x801E5230..0x801E52D0),
// which is also where the containing dtk unit `auto_03_801E52D0_text` starts; that unit now runs
// 0x801E52D0..0x801E5458 and 0x801E555C..0x801E7038 as two objects, and 0x188 + 0x104 + 0x1ADC
// is the 0x1D68 it was before.  This claim does not begin where a `Matching` unit ends, which is
// the case `RUNNING_THE_DECOMP.md` records as a link-order cycle.
//
// The directory is retail's own, taken from the nearest claimed range below
// (`MetroidPrime/ScriptObjects/Carve801E5230.c`).  For an anonymous function that is the only
// evidence there is, and it beats a lane picking the directory it happened to own.
//
// The one external callee that is not in this range, `allocate__Q24rstl17rmemory_allocatorFi`
// (0x802FDAB8, `symbols.txt:13824`) and `Free__7CMemoryFPCv` (0x802CE388, `symbols.txt:12992`), are
// both retail's own functions, so the unit adds two undefined symbols to the DOL link that the
// port's link already resolves and needs no `PortLinkStubs.cpp` entry.

/** The element `fn_801E5510` copies: four words, which is `CPortalArea::SPortalState`
 *  (`include/MetroidPrime/CPortalArea.hpp:35-39`) - `int x0_`, `SActorList { SActorPool* mPool;
 *  SActorNode* mHead; }` and `const void* mPortalData`.  Named locally because there is no
 *  declaration of it outside `CPortalArea.hpp`'s private section; only the four words matter here,
 *  and the constructor that fills them is retail's. */
typedef struct {
    unsigned int x0;
    unsigned int x4;
    unsigned int x8;
    unsigned int xc;
} SPortalState801E5458;

/** `rstl::vector`'s four members as `fn_801E5458` sees them (`rstl/vector.hpp:16-22`), with
 *  `rstl::rmemory_allocator`'s empty four bytes spelled out so `mCount` lands at +4, where retail
 *  reads it.  Never touched at +0. */
typedef struct {
    unsigned int x0_allocator;
    int x4_count;
    int x8_capacity;
    SPortalState801E5458* xc_items;
} SPortalVec801E5458;

/** `rstl::pointer_iterator`, which `rstl::uninitialized_copy` takes **by value**
 *  (`rstl/vector.hpp:174`), and which therefore arrives in the callee's frame rather than in a
 *  register - that is what `lwz r6,0(r3)` / `lwz r0,0(r4)` at the top of `fn_801E5510` reads.
 *  One member, one load. */
typedef struct {
    const SPortalState801E5458* current;
} SPortalIter801E5458;

/** `rstl::rmemory_allocator::allocate(int)` - retail `.text:0x802FDAB8`, 0x3C bytes
 *  (`symbols.txt:13824`).  Out of line in retail and here: `fn_801E5458` computes the byte count
 *  itself (`slwi r3,r30,4`) and calls it. */
extern void* allocate__Q24rstl17rmemory_allocatorFi(int size);

/** `CMemory::Free(void const*)` - retail `.text:0x802CE388`, 0x64 bytes (`symbols.txt:12992`), the
 *  body of `rstl::rmemory_allocator::deallocate` under `TARGET_PC`
 *  (`rstl/rmemory_allocator.hpp:34-43`). */
extern void Free__7CMemoryFPCv(const void* ptr);

/** `fn_801E5510` - retail `.text:0x801E5510`, 0x4C = 76 bytes: `rstl::uninitialized_copy` over the
 *  range `[begin, end)`, one 16-byte element at a time.  Called only by `fn_801E5458` below.
 *  `dst` is null-checked per element because the copy goes through placement new. */
void* fn_801E5510(SPortalIter801E5458 begin, SPortalIter801E5458 end, void* out);
void* fn_801E5510(SPortalIter801E5458 begin, SPortalIter801E5458 end, void* out) {
    const SPortalState801E5458* src = begin.current;
    SPortalState801E5458* dst = (SPortalState801E5458*)out;
    for (; src != end.current; ++src, ++dst) {
        if (dst) {
            dst->x0 = src->x0;
            dst->x4 = src->x4;
            dst->x8 = src->x8;
            dst->xc = src->xc;
        }
    }
    return dst;
}

/** `fn_801E5458` - retail `.text:0x801E5458`, 0xB8 = 184 bytes:
 *  `rstl::vector<SPortalState>::reserve(int)` (`rstl/vector.hpp:166-179`) - grow to `newSize`
 *  elements, copying the `mCount` that are already there, drop the old block, and only then write
 *  `mItems` and `mCapacity`.  A `newSize` that already fits is the 8-instruction tail at 0x801E54F4.
 *  Called by retail's `CPortalArea` constructor with `this + 0x401C`. */
void fn_801E5458(SPortalVec801E5458* self, int newSize);
void fn_801E5458(SPortalVec801E5458* self, int newSize) {
    SPortalState801E5458* newData;
    const SPortalState801E5458* cur;
    if (newSize <= self->x8_capacity) {
        return;
    }
    newData = (SPortalState801E5458*)allocate__Q24rstl17rmemory_allocatorFi(
        (int)((unsigned int)newSize << 4));
    fn_801E5510((SPortalIter801E5458){self->xc_items},
                (SPortalIter801E5458){self->xc_items + self->x4_count}, newData);
    /* `rstl::destroy(mItems, mItems + mCount)` - see the header comment: the loop is real, the
     * body is empty because `SPortalState`'s destructor is trivial. */
    for (cur = self->xc_items; cur != self->xc_items + self->x4_count; ++cur) {
    }
    Free__7CMemoryFPCv(self->xc_items);
    self->xc_items = newData;
    self->x8_capacity = newSize;
}