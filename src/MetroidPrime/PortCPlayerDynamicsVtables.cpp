// Host definitions of the three `.data` vtable objects
// `src/MetroidPrime/Player/CPlayerDynamics.cpp` references from the deleting destructors it now
// writes out: `fn_80185814` (retail 0x80185814), `fn_80185870` (0x80185870) and `fn_801894C4`
// (0x801894C4).
//
// This is the `src/MetroidPrime/PortCMorphBallVtables.cpp` arrangement, for the same reason: all
// three are *unclaimed* `.data` gaps - `config/G2ME01/splits.txt` ends the nearest units before
// 0x803B5B20 - so in the DOL build `dtk` fills them with retail's own bytes and they resolve with
// no declaration anywhere. The host build has no `dtk` step and nothing else defines them, so
// without this file the three destructors add `lbl_803B5B30` / `lbl_803B5B3C` / `lbl_803B5B48` to
// `tools/link_gap.py`'s MISSING set and `tools/gate.sh` fails on `link-gap`. Listed in
// `files.cmake`, the port-only list, and not in `configure.py`.
//
// The four objects, from `config/G2ME01/symbols.txt:17970` and `:18244-18246`, measured with
// `python3 tools/dol_read.py 0x803B5B20 0x40` and `python3 tools/dol_read.py 0x803B1750 0x10`:
//
//   0x803B5B30  size 0xC  `0, 0, 0x801894C4`  the derived vtable `fn_801894C4` stores first
//   0x803B5B3C  size 0xC  `0, 0, 0x80185814`  the derived vtable `fn_80185814` stores first
//   0x803B5B48  size 0x10 `0, 0, 0x80185870`  the derived vtable `fn_80185870` stores first
//   0x803B1750  size 0x10 `0, 0, 0x8000DF48`  the shared base vtable all three store next, already
//                                          hosted by PortCMorphBallVtables.cpp
//
// Each is the MWCC layout `{0, 0, slot0, 0}` - the two leading words are retail's offset-to-top
// and type-info slots - with slot 0 the class's own destructor, which is why each derived object
// holds its own `fn_` address in the third word and the base one holds `fn_8000DF48`.
//
// **They are left zero-filled rather than transcribed**, for the reason
// `PortCMorphBallVtables.cpp` gives: their contents are DOL code addresses, so a real copy would
// build a vtable whose slot 0 points at unmapped host memory. Nothing in the port calls through
// them - the only references are the `stw`s in the destructors themselves.
//
// The zero initialiser is load-bearing (measured in PortCMorphBallVtables.cpp): g++ 15 emits
// nothing at all for an unreferenced `extern "C" char name[N];` with no initialiser, so the file
// would compile to an object with no `lbl_*` symbol and `link-gap` would still fail. `= {0}`
// makes all three appear. The initialiser is inside an `extern "C" { }` block rather than written
// on the declaration, which keeps g++ from warning "initialized and declared 'extern'".
extern "C" {
char lbl_803B5B30[0xC] = {0};
char lbl_803B5B3C[0xC] = {0};
char lbl_803B5B48[0x10] = {0};
}
