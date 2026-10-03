// Carved out of an unclaimed dtk `auto_*` range by lane `carve3`.  Every number here is
// measured: the addresses and sizes come from `config/G2ME01/symbols.txt:11048`, the
// instructions are the ones dtk itself emitted into `build/G2ME01/asm/auto_03_80278BF8_text.s:42-47`
// (the same range is now `build/G2ME01/asm/GuiSys/Carve80278C68.s`), and the body below is the C
// those bytes are the compilation of.
//
// .text 0x80278C68..0x80278C74, 0xC = 12 bytes, 1 function:
//
//   fn_80278C68    0x80278C68  0xC    lis r3, 0x534c ; addi r3, r3, 0x4750 ; blr
//
// **The twin, and what it proves about this copy.**  `build/G2ME01/asm/GuiSys/CAuiImagePane.s:52-57`
// is `GetWidgetTypeID__13CAuiImagePaneCFv` (0x8027EFA0, `symbols.txt:11219`, `size:0xC`,
// `scope:weak`), and its three instructions are the same three with only the immediate differing
// - `lis r3, 0x494d ; addi r3, r3, 0x4750 ; blr`.  Its source is one line,
// `include/GuiSys/CAuiImagePane.hpp:17`: `FourCC GetWidgetTypeID() const override { return
// 'IMGP'; }`, where `FourCC` is `typedef uint FourCC` (`include/Kyoto/SObjectTag.hpp:11`).  So the
// shape here is "materialise one 32-bit constant in r3 and return it", and the constant this copy
// materialises is `0x534C4750` - the same three instructions are what `return 0x534C4750;`
// compiles to, and both `CAuiImagePane.cpp` and this file are `NonMatching`/`Matching` proof that
// the spelling below reproduces them.
//
// **The constant is a FourCC, `SLGP`, and retail's own vtable says which function this is.**
// `0x534C4750` read big-endian is `'S' 'L' 'G' 'P'`.  `build/G2ME01/asm/auto_07_803B8F68_data.s:9-25`
// lists this function at index 3 of the table `lbl_803B8F68`, and that table matches slot for slot
// the layout of `__vt__13CAuiImagePane` (`build/G2ME01/asm/GuiSys/CAuiImagePane.s:1400-1416`) -
// index 2 is the destructor (`fn_80278BF8` here, `__dt__13CAuiImagePaneFv` there), index 3 is
// `GetWidgetTypeID` here, index 4 is `GetWidgetUsageFlags` here, index 6/7 are retail's shared
// `GetIsActive__10CGuiWidgetCFv` / `GetIsVisible__10CGuiWidgetCFv` in both, index 8/9 are
// `Update`/`Draw`, and index 14 is `Initialize__10CGuiWidgetCFv` in both.  The class is a
// `CGuiCompoundWidget`: indices 12 and 13 are `OnVisible__18CGuiCompoundWidgetFv` and
// `OnActivate__18CGuiCompoundWidgetFv` (`src/GuiSys/CGuiCompoundWidget.cpp`, `Matching`) where
// `CAuiImagePane` has the `CGuiWidget` ones, and retail gives it no `WriteData` override at all
// (index 15 is `0x00000000`).  So index 3 is `GetWidgetTypeID() const` for this class too, and
// index 4 - `fn_80278C74`, 8 bytes, `li r3, 0x6` - is `GetWidgetUsageFlags`, which is why the
// carve one slot above this one returns a small integer and this one returns a FourCC.  Retail
// names neither class, so the function keeps the `fn_` placeholder; the *slot* is identified, the
// C++ name is not claimed.
//
// **Nothing in `src/` or `include/` names this symbol**, so there is no `TARGET_PC` arm to write:
// it is reached only through the `.data` table above, which retail's own unclaimed `auto_07_*`
// object supplies and the host link does not carry.  The body below is therefore the DOL's.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  With one function the order cannot be wrong, and the rule is
// recorded because the next carve added to this file would break it.
//
// Retail names this function.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z11fn_80278C68v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.
//
// Its own unit because a claim may not span an unclaimed gap: this run sits inside the
// 0x80278BF8..0x80278C74 hole that dtk covers with `auto_03_80278BF8_text`, whose other half is
// `fn_80278BF8` (0x80278BF8, 0x70 = 112 bytes, `symbols.txt:11047`) - the destructor the table
// above lists at index 2 - which is not trivial.  The directory is retail's own, taken from the
// nearest claimed range: below is `GuiSys/CGuiPane.cpp` (0x8027855C..0x80278BF8) and the next
// claimed range above is `MetroidPrime/Carve80278C74.c` (0x80278C74..0x80278C7C), which is this
// very vtable's `GetWidgetUsageFlags`.  Adjacency to that carve is the `carve3` pattern the
// carve vein records as safe - proximity to another *carve* links, proximity to an existing
// pre-existing unit boundary does not.
unsigned int fn_80278C68(void) { return 0x534C4750u; }