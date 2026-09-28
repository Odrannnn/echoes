// `.text 0x802B2568..0x802B2570`, 0x8 = 8 bytes, one function:
//
//   fn_802B2568    0x802B2568  0x8    li        r3, 0x0
//
// **The claim was narrowed from 0x802B2568..0x802B2580 to 0x802B2568..0x802B2570.** Upstream's
// `Kyoto/Animation/IAnimReader.cpp` owns 0x802B2570 onwards, and its first three functions are
// `VGetSegData__11IAnimReaderCFRC15CCharLayoutInfoR24CJointData_LinearStorage` (0x802B2570),
// `...RC13CCharAnimTime` (0x802B2574) and the pair that followed. This file used to define plain
// `fn_802B2570`/`fn_802B2574`/`fn_802B2578` stubs for those same three addresses, which are
// symbols retail does not have, so they are deleted: the only function here now is the one
// `config/G2ME01/symbols.txt` still leaves unnamed.
//
// Retail names none of the four, so `fn_802B2568` is the placeholder from `symbols.txt` and the
// definition has to stay C: a C++ one would mangle to `_Z12fn_802B2568v` and objdiff would pair
// nothing. That is why this unit is a `.c` rather than a `.cpp`.
//
// It is `li r3,0` / `blr` - a `bool`-returning function that answers false - and no `bl` in the
// DOL calls it, so it is the out-of-line copy MWCC emits for an empty inline at file scope, the
// same shape as `fn_80025E08` in `MetroidPrime/CAnimData.cpp` and `fn_8001935C` in
// `MetroidPrime/Player/CPlayer.cpp`.
//
// The gap it fills is upstream's: `Kyoto/Animation/Carve802B2088.c` ends at 0x802B2090 and
// `Kyoto/Animation/IAnimReader.cpp` starts at 0x802B2570.
int fn_802B2568(void) { return 0; }
