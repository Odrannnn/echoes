// Temporary layout probe for CGameState, run with mwcceppc's flags.
// `private` is opened up so offsetof() can be applied to the members directly; the host
// compiler must not be used (64-bit host, rstl::string 24 bytes against retail's 0x10).
#define private public
#define protected public
#include "MetroidPrime/Player/CGameState.hpp"
#undef private
#undef protected

#include "MetroidPrime/Player/CGameOptions.hpp"
#include "MetroidPrime/Player/CHintOptions.hpp"
#include "MetroidPrime/Player/CPersistentOptions.hpp"
#include <stddef.h>

#define S(T) ((int)sizeof(T))
#define O(T, m) ((int)offsetof(T, m))

// Every row is one of the 27 `this` offsets `fn_80144140` (retail 0x80144140) writes, reads or
// passes to a callee, plus the sizes of the member types. The expected values are the ones
// docs/research/cgamestate_layout.md and this header's comments state, measured out of
// build/G2ME01/main.elf; `tools/probe_gs_offsets.py` diffs this against them.
unsigned int g_probe[] = {
    S(CGameState),                                       //  0x2F0
    O(CGameState, x00_unk),                              //  0x000
    O(CGameState, x04_unk),                              //  0x004
    O(CGameState, x08_reserve),                          //  0x008
    O(CGameState, x18_playerStates),                     //  0x018
    O(CGameState, x01c_players),                         //  0x01C
    O(CGameState, x3c_worldState),                       //  0x03C
    O(CGameState, x40_refCount),                         //  0x040
    O(CGameState, x44_unk),                              //  0x044
    O(CGameState, x48_time),                             //  0x048
    O(CGameState, x50_unk),                              //  0x050
    O(CGameState, x54),                                  //  0x054
    O(CGameState, gameOptions),                          //  0x080
    O(CGameState, hintOptions),                          //  0x0C4
    O(CGameState, persistentOptions),                    //  0x0DC
    O(CGameState, cardSerial),                           //  0x108
    O(CGameState, x110),                                 //  0x110
    O(CGameState, x144),                                 //  0x144
    O(CGameState, x178),                                 //  0x178
    O(CGameState, x188),                                 //  0x188
    O(CGameState, x198_ptrSet),                          //  0x198
    O(CGameState, x19c_ptr),                             //  0x19C
    O(CGameState, x1a0),                                 //  0x1A0
    O(CGameState, x1f4),                                 //  0x1F4
    O(CGameState, x204),                                 //  0x204
    O(CGameState, x2ec_flags),                           //  0x2EC
    S(CGameOptions),                                     //  0x044
    S(CHintOptions),                                     //  0x018
    S(CPersistentOptions),                               //  0x02C
    S(SGameStateBlock),                                  //  0x010
    S(SGameStateSlots),                                  //  0x034
    S(SGameStateCardOpts),                   //  0x02C
    S(SGameStateWorlds),                     //  0x54
    S(SGameStateMemcard),                    //  0x0E8
    O(SGameStateSlots, x04_blk),                         //  0x004
    O(SGameStateWorlds, x10_count),          //  0x010
    O(SGameStateWorlds, x14_rec),            //  0x014
    O(SGameStateMemcard, x00_size),          //  0x000
    O(SGameStateMemcard, x04_buf),           //  0x004
    O(SGameStateMemcard, x50_size),          //  0x050
    O(SGameStateMemcard, x54_buf),           //  0x054
    O(SGameStateMemcard, xa0_unk),           //  0x0A0
    O(SGameStateMemcard, xa4_unk),           //  0x0A4
    O(SGameStateMemcard, xe4_flag),          //  0x0E4
};
unsigned int g_n = sizeof(g_probe) / sizeof(g_probe[0]);
