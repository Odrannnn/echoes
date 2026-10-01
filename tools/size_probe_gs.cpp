// Temporary layout probe for CGameState, run with mwcceppc's flags.
// `private` is opened up so offsetof() can be applied to the members directly; the host
// compiler must not be used (64-bit host, rstl::string 24 bytes against retail's 0x10).
// Since 2026-10-01 there is one layout - upstream's, on the host too - and these rows name its
// members; the offsets are the ones the port's former TARGET_PC layout was measured at.
#include "types.h"
#include "Kyoto/SObjectTag.hpp"
#include "Kyoto/TToken.hpp"
#include "MetroidPrime/CControlMapper.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "MetroidPrime/Player/CGameOptions.hpp"
#include "MetroidPrime/Player/CGameStateBlocks.hpp"
#include "MetroidPrime/Player/CGameStateEnvVarManager.hpp"
#include "MetroidPrime/Player/CHintOptions.hpp"
#include "MetroidPrime/Player/CWorldState.hpp"
#include "rstl/auto_ptr.hpp"
#include "rstl/pair.hpp"
#include "rstl/rc_ptr.hpp"
#include "rstl/reserved_vector.hpp"
#include "rstl/vector.hpp"
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
    O(CGameState, mWorldId),                              //  0x000
    O(CGameState, mDesiredWorldId),                              //  0x004
    O(CGameState, mWorldStates),                          //  0x008
    O(CGameState, mPlayerStates),                     //  0x018
    O(CGameState, mTransManager),                       //  0x03C
    O(CGameState, mTotalPlayTime),                       //  0x048
    O(CGameState, mEscapeTime),                          //  0x050
    O(CGameState, mSystemOptions),                       //  0x054
    O(CGameState, mGameOptions),                          //  0x080
    O(CGameState, mHintOptions),                          //  0x0C4
    O(CGameState, mPersistentOptions),                    //  0x0DC
    O(CGameState, mCardSerial),                          //  0x108
    O(CGameState, mCompressedGameStates),                                 //  0x110
    O(CGameState, mCompressedGameOptions),                                 //  0x144
    O(CGameState, mCompressedMultiplayerOptions),                                 //  0x178
    O(CGameState, mCheckpointGameState),                                 //  0x188
    O(CGameState, mGameMode),                            //  0x198, `mHas` = retail's x198_ptrSet
    S(rstl::auto_ptr< CGameMode >),                      //  0x008, so `mItem` (retail's x19c_ptr) ends at 0x1A0;
                                                         //  its members are private by `class` default, which `#define private` cannot open
    O(CGameState, mPreviousGameResults),                        //  0x1A0
    O(CGameState, mAudioGroups),                                 //  0x1F4
    O(CGameState, mControlMapper),                       //  0x204
    O(CGameState, x2ed_) - 1,                            //  0x2EC, the `mHardMode` byte (a bitfield has no offsetof)
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
