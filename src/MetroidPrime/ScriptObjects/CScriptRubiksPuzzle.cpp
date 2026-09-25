// Functions for the RubiksPuzzle REL.

#include "MetroidPrime/ScriptLoader/SLdrRubiksPuzzle.hpp"

extern "C" const float lbl_4_rodata_0;

SLdrRubiksPuzzleData::SLdrRubiksPuzzleData() {
  stateMachine = 0xFFFFFFFFu;
  rotationSpeed = lbl_4_rodata_0;
}

// REL/REL_Setup.cpp functions.
// 0x00002618  _unresolved  size 0xC4
// 0x000026DC  _epilog  size 0x24
// 0x00002700  _prolog  size 0x24
// 0x00002724  fn_4_2724  size 0x4C
// 0x00002770  fn_4_2770  size 0x4C
