// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:11253`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_80280340_text.s:893-904` before the claim
// existed (the same range is now `build/G2ME01/asm/MetroidPrime/Carve80280F38.s`), and the body
// below is the C++ those bytes are the compilation of.
//
// .text 0x80280F38..0x80280F5C, 0x24 = 36 bytes, 1 function:
//
//   fn_80280F38    0x80280F38  0x24    9 instructions   `rstl::destroy_impl< rstl::vector< void* > >`
//
// **It is a byte-shape twin of the matched `fn_80004458`, and the twin settles the whole body.**
// `fn_80004458` (0x80004458, 0x24 = 36 bytes, 9 instructions,
// `src/MetroidPrime/Carve80004438.c:89`, `Matching`) is
// `rstl::destroy_impl< CWorldState >` written out as `fn_8000447C(self, -1)`, and its nine words
// are the nine below word for word apart from the `bl`: same `stwu r1,-0x10(r1)`, same `mflr r0`,
// same `li r4,-1` in the same third slot, same `stw r0,0x14(r1)`, same epilogue
// (`lwz r0,0x14(r1) / mtlr r0 / addi r1,r1,0x10 / blr`).  That is the whole of
// `include/rstl/construct.hpp:84-90`, whose `destroy_impl` body is `in->~T()` and nothing else
// (`destroy` at :92-95 is that body wrapped once more, which is retail's other nine words at
// 0x80280F18 - `fn_80280F18`, 0x20, `symbols.txt:11252`, and this file's own `bl` target).
// So the `-1` is not written here: it is MWCC's "destroy, do not free me" flag, and **mwcceppc
// materialises it itself** for the destructor call, which is why the twin needs it spelled and
// this one does not (measured on this unit's own object - see `docs/goal-notes/carve-80280f38.md`).
// The callee confirms it from the other side: `__dt__Q24rstl37vector<Pv,Q24rstl17rmemory_allocator>Fv`
// reads the flag as `mr r31,r4 / ... / extsh. r0,r31 / ble` (0x8005C43C, 0x8005C454, 0x8005C458),
// so a flag of `-1` skips its own trailing `bl Free__7CMemoryFPCv` - which is what makes this
// `destroy_impl` and not a `delete`.
//
// **The unit is a `.cpp` rather than a `.c` because the call cannot be written in C at all.**
// The callee's MWCC symbol is `__dt__Q24rstl37vector<Pv,Q24rstl17rmemory_allocator>Fv`
// (0x8005C42C, 0x54, `symbols.txt:1813`), and `<`, `>` and `,` are not identifier characters:
// `extern void f(void*) asm("<mangled>");` is rejected by this compiler in **both** C and C++ mode
// with "type cannot be made into a global register variable" (measured, and recorded at
// `src/MetroidPrime/Carve801C2D74.cpp:52-59`, `src/MetroidPrime/CIngBoostBallGuardian3790.cpp:32-42`
// and `src/MetroidPrime/Carve80281310.c:59-63`).  The way through is `Carve8000447C.cpp`'s:
// declare the class template **locally, with its destructor declared and never defined**, so the
// one destructor call mangles to retail's own MWCC symbol and nothing at all is emitted for the
// declaration.  Measured on this unit's object: `powerpc-eabi-nm` reports
// `U __dt__Q24rstl37vector<Pv,Q24rstl17rmemory_allocator>Fv` and `T fn_80280F38`, one text symbol
// and no local definition, so `tools/unit_fit.sh` has nothing extra to report.
//
// **`include/rstl/vector.hpp` is deliberately not included.** Its `~vector()` is declared at :60
// and *defined inline* at :138-142, so a translation unit that can see the body would either
// inline the deallocation into `fn_80280F38` (wrong bytes - nine words, not 0x54) or outline a
// local weak copy of retail's own 0x8005C42C into this object, which retail's 36-byte range does
// not define; that is the same reason `Carve801C2D74.cpp:62-65` keeps
// `rstl/construct_impl` and `rstl/pointer_iterator` out of its unit.  Only the template's **name**
// is needed here, and the name is needed because the callee's mangled symbol spells it
// `Q24rstl37vector<Pv,Q24rstl17rmemory_allocator>` - the layout of `rstl::vector` is never read
// by these bytes.  Retail's own `rstl::vector< void* >::~vector()` is 0x8005C42C..0x8005C480:
// `mr. r30,r3 / beq`, `lwz r3,0xc(r30) / bl Free__7CMemoryFPCv` (the `mItems` release, member at
// +0xC per `include/rstl/vector.hpp:18-21`), then the flag test and the self-free.
//
// **The callee resolves against a unit our own tree compiles, so no stub is involved.**  Measured
// with `powerpc-eabi-nm build/G2ME01/obj/MetroidPrime/CGameArea.o`:
// `00008da4 T __dt__Q24rstl37vector<Pv,Q24rstl17rmemory_allocator>Fv`.  `CGameArea.cpp` claims
// `.text` 0x80053688..0x800609B4 (`config/G2ME01/splits.txt:259-260`, `configure.py:478`), so
// 0x8005C42C is inside it.  It is the same shape of call the already-matched
// `Kyoto/Graphics/DolphinCModel.cpp` makes at 0x80311B54, whose object likewise carries the
// symbol undefined (`U`) and never defines it.
//
// `extern "C"` is what keeps `fn_80280F38` unmangled: retail names it `fn_80280F38`
// (`symbols.txt:11253`), and a C++ definition without it would mangle to `_Z12fn_80280F38Pv` and
// objdiff would pair nothing.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  This unit has one function, so the rule is satisfied by
// construction; `python3 tools/check_decl_order.py --unit MetroidPrime/Carve80280F38.cpp` is the
// cheap check.
//
// Retail does not point at `fn_80280F38` from data: `grep -rn 'fn_80280F38' build/G2ME01/asm/`
// finds only the one `bl` above, in `fn_80280F18` (0x80280F24).  Nothing here claims to know why.
//
// Its own unit because a claim may not span an unclaimed gap.  This run sits inside the
// 0x80280340..0x80281310 hole that dtk covers with `auto_03_80280340_text`, so carving splits
// that object into 0x80280340..0x80280F38 and 0x80280F5C..0x80281310.  Below the claim
// `fn_80280EC8` (0x80280EC8, 0x50) and `fn_80280E90` end the run beneath and stay retail's -
// `fn_80280EC8` is `rstl::destroy(begin, end)` over 0x10-byte elements, each destroyed by
// `fn_80280F18` (`mr r3,r31 / bl fn_80280F18 / addi r31,r31,0x10` at 0x80280EE8..0x80280EF0); above
// the claim `fn_80280F5C` (0x80280F5C, 0x348 = 840 bytes, `symbols.txt:11254`) begins and is far
// too big for this item; the next claimed range is `MetroidPrime/Carve80281310.c`
// (0x80281310..0x8028139C).  **The claim touches no unit boundary** - the claim below ends
// 0x80280340, 0xBF8 = 3064 bytes under this one, and the claim above starts 0x3B4 = 948 bytes
// over it - so there is no link-order cycle for `dtk dol split` to report, the trap the carve vein
// records at `docs/RUNNING_THE_DECOMP.md:2073-2079`.
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `MetroidPrime/Carve80280338.c` (0x80280338..0x80280340), above is
// `MetroidPrime/Carve80281310.c` (0x80281310..0x8028139C).  For an anonymous function that is
// the only evidence there is, and it beats a lane picking the directory it happened to own.
//
// The body is inside `#ifdef __MWERKS__` for the reason `Carve801C2D74.cpp:87-92` and
// `Carve8000447C.cpp`'s header give: `MetroidPrime/CGameArea.cpp` compiles the mangled
// `__dt__Q24rstl37vector<Pv,Q24rstl17rmemory_allocator>Fv` for the **DOL**, and a host
// compilation of this file would emit a *different* mangling of the same call, adding one
// undefined symbol to the port's flat link for a function no host source calls -
// `fn_80280F18`, its only caller, is itself inside the unclaimed dtk range.  The host branch is
// empty by design; the DOL branch is the whole file.  `tools/check_files_cmake.py` carries this
// carve path for the same reason.

#ifdef __MWERKS__

namespace rstl {

/** Named only so `vector` below reads as the template it stands in for; nothing here uses it,
 *  and `include/rstl/rmemory_allocator.hpp` is not included because its layout is not read. */
class rmemory_allocator;

/** `rstl::vector< T, Alloc >` - `include/rstl/vector.hpp:15-125` is the real one, reduced to the
 *  one thing these 36 bytes need: a **complete** class with a destructor **declared and never
 *  defined**.  Declared, because the destructor must be a real member for `self->~vector()` to
 *  mangle to `__dt__Q24rstl37vector<Pv,Q24rstl17rmemory_allocator>Fv` (`symbols.txt:1813`,
 *  0x8005C42C); never defined, because the real header's `~vector()` is `inline`
 *  (`vector.hpp:138-142`) and a definition in this translation unit would either be inlined into
 *  `fn_80280F38` or outlined as a local weak copy of retail's own function, and retail's 36-byte
 *  range defines neither.  The four members are omitted because no byte below reads any of them -
 *  what reproduces here is the class's **name**, and the name is all a mangled symbol encodes. */
template < class T, class Alloc > class vector {
public:
  ~vector();
};

} // namespace rstl

/** `fn_80280F38` - retail `.text:0x80280F38`, 0x24 = 36 bytes: `rstl::destroy_impl` for a
 *  `rstl::vector< void* >`, i.e. `in->~T()` from `include/rstl/construct.hpp:84-90` and nothing
 *  else.  Its measured twin is `fn_80004458` (0x80004458, `MetroidPrime/Carve80004438.c:89`,
 *  `Matching`), these nine instructions word for word apart from the `bl` target. */
extern "C" void fn_80280F38(rstl::vector< void*, rstl::rmemory_allocator >* self);

extern "C" void fn_80280F38(rstl::vector< void*, rstl::rmemory_allocator >* self) {
  self->~vector();
}

#endif // __MWERKS__