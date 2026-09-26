#ifndef _CHINTOPTIONS
#define _CHINTOPTIONS

// Retail 0x18 bytes, measured from the three functions that touch it and not from a guess.
// `CGameState::CGameState(CInputStream&, int)` (retail fn_80144140, 0x80144140) constructs this
// member at `this+0xC4` and `CPersistentOptions` at `this+0xDC`, so the gap between the two
// start offsets is 0x18 - and 0x16 would put the next member at 0xDA, where retail has no call.
// The class's own three functions agree on the member set:
//   fn_80180738 (0x80180738, 0x24 = 36 bytes, the default constructor) writes only
//     +0x04 = 0, +0x08 = 0, +0x0C = 0, +0x10 = -1, +0x14 = 0, +0x15 = 0
//     and NOT +0x00, so the first word is something the constructor leaves alone.
//   fn_801805EC (0x801805EC, 0x14C) is the same six stores, then
//     `this->reserve(*(gpGameState+0x08)+4)` and a loop `for (i = 0; i < this[+8]; i++)`
//     appending 12-byte {int, float, float} elements at `*(this+0xC) + i*12`, which makes
//     +0x04 the element count, +0x08 the capacity and +0x0C the data pointer.
//   fn_801447C4 (0x801447C4, 0x54) is the copy assignment, and copies exactly +0x10, +0x14
//     and +0x15 - the same three the constructor sets to -1, 0 and 0.
// So the class does start at +0x00 and the first word is a member the constructor leaves alone -
// it is initialised by whatever allocates the object, or by a derived constructor. The class
// therefore runs 0xC4..0xDC. See docs/research/boot_globals.md for the +0x3C member of
// CGameState itself, and tools/size_probe_gs.cpp for the probe that measured these offsets.
//
// `x0c_data` is deliberately a raw pointer, not an `rstl::vector`: the elements are 12 bytes
// {int, float, float} and retail's own three functions index the data block as
// `data + count * 12`, with no capacity field anywhere but +0x08.
//
// The members are public because retail's constructor is unnamed in the symbol table, so the
// unit that reproduces it is an `extern "C" void fn_80180738(CHintOptions*)` rather than a C++
// constructor - a C++ one would mangle to `__ct__9CHintOptionsFv` and objdiff would have nothing
// to pair against. See src/MetroidPrime/Player/CHintOptionsCtor.cpp.
class CHintOptions {
public:
  int x00_unk;    //!< +0x00, left alone by the constructor
  int x04_count;  //!< +0x04, the element count
  int x08_cap;    //!< +0x08, the capacity the stream constructor loops to
  void* x0c_data; //!< +0x0C, 12-byte {int, float, float} elements
  int x10_unk;    //!< +0x10, -1 until a particular element type is seen
  bool x14;       //!< +0x14
  bool x15;       //!< +0x15
};
CHECK_SIZEOF(CHintOptions, 0x18)

#endif // _CHINTOPTIONS
