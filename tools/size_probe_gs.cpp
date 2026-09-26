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

unsigned int g_probe[] = {
    S(CGameState),
    O(CGameState, x3c_worldState),
    O(CGameState, x40_refCount),
    O(CGameState, x50_unk),
    O(CGameState, gameOptions),
    O(CGameState, hintOptions),
    O(CGameState, persistentOptions),
    O(CGameState, cardSerial),
    S(CGameOptions),
    S(CHintOptions),
    S(CPersistentOptions),
};
unsigned int g_n = sizeof(g_probe) / sizeof(g_probe[0]);
