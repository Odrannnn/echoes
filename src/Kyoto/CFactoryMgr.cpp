#include "Kyoto/CFactoryMgr.hpp"

#include <ctype.h>

// The 46 resource types, in index order. Retail .rodata 0x803AF9D8, 0xB8 bytes, immediately
// followed by the "??(??).." string literal that begins at index 46. The loop bound of 46 was
// measured: retail's scan is unrolled two-at-a-time with `mtctr`/`bdnz` over `li r0,23`, and
// 23 * 2 = 46.
//
// `FourCCToTypeIdx` is why this list exists - it folds a FourCC to upper case and linearly scans
// it. Nothing else in retail depends on the order except `TypeIdxToFourCC`, which indexes the
// same array, and CPakFile, which stores the *index* in a pak's resource directory.
static const FourCC kFactoryMgrTypes[] = {
    'CLSN', 'CSPP', 'CMDL', 'CSKR', 'ANIM', 'CINF', 'TXTR', 'PLTT', 'FONT', 'ANCS', 'ANMS', 'MADF',
    'MLVL', 'MREA', 'MAPW', 'MAPA', 'SAVW', 'SAVA', 'PART', 'WPSC', 'SWHC', 'DPSC', 'ELSC', 'CRSC',
    'SPSC', 'SRSC', 'AFSM', 'DCLN', 'AGSC', 'ATBL', 'CSNG', 'STRG', 'SCAN', 'PATH', 'DGRP', 'HMAP',
    'PTLA', 'STLC', 'EGMC', 'RULE', 'FSM2', 'CTWK', 'FRME', 'HINT', 'MAPU', 'DUMB',
};

// Declared descending by retail offset: mwcceppc emits function definitions in reverse source
// order and mwldeppc keeps the object's .text order verbatim, so 0x802F8E9C (TypeIdxToFourCC,
// 0x14 bytes) comes first in the file and 0x802F8D90 (FourCCToTypeIdx, 0x10C) second.
uint CFactoryMgr::TypeIdxToFourCC(uint typeIdx) { return kFactoryMgrTypes[typeIdx]; }

// The case fold is `toupper` from <ctype.h>, whose `EOF` test and `__zero_fill` are exactly
// the `extsb`/`cmpwi r0,-1`/`clrlwi r0,r0,24` triple retail emits, and whose table
// `__upper_map` is at .data 0x803BC568 - **already owned by the Matching unit Runtime/ctype.c**,
// which is why this unit claims .text and .rodata only. Writing a private copy of the table
// here would have been a second definition of a symbol that already exists.
//
// The four folds are four statements, **not a `for (i = 0; i < 4; ++i)` loop**, and that is not
// a style choice: with the loop, mwcceppc fully unrolls it and then hoists the loop-invariant
// `lis`/`addi` of `__upper_map` out of the four branches, giving 0x108 bytes against retail's
// 0x10C. Written out, the address stays inside each branch and the object is byte-identical
// (measured with tools/try_batch.py: the loop form differs in 14 instructions, the four
// statements in 0).
uint CFactoryMgr::FourCCToTypeIdx(uint fourCC) {
  char* bytes = reinterpret_cast< char* >(&fourCC);
  bytes[0] = static_cast< char >(toupper(bytes[0]));
  bytes[1] = static_cast< char >(toupper(bytes[1]));
  bytes[2] = static_cast< char >(toupper(bytes[2]));
  bytes[3] = static_cast< char >(toupper(bytes[3]));

  for (uint i = 0; i < sizeof(kFactoryMgrTypes) / sizeof(kFactoryMgrTypes[0]); ++i) {
    if (kFactoryMgrTypes[i] == fourCC) {
      return i;
    }
  }
  return uint( -1 );
}

