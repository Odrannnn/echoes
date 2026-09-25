// The port's registry of REL modules that are compiled into the game library
// rather than loaded from the disc.
//
// The other 83 modules are read out of the ISO and relocated by
// platform/rel.cpp, which calls each module's prolog and epilog by guest
// address inside the loaded image. Three modules are not: Tweaks, CannonBall and
// ForgottenObject have our own sources in src/, and those are compiled straight
// into mp_game. That is why they need this file.
//
// On the cube each of the three is a separate module with its own RELMain and
// RELExit, which mwldeppc's linker script turns into the module's prolog and
// epilog. Two things follow, and both are handled here rather than papered over:
//
//   1. Three translation units defining one symbol cannot coexist in a flat
//      link. Each module therefore gives its entry points a distinct name on the
//      host only (`#ifdef __MWERKS__` still compiles the retail RELMain and
//      RELExit, so those Matching units are untouched).
//   2. Renaming them would otherwise leave them never called, because nothing on
//      the host knows they exist. This table is that knowledge: it names each
//      compiled module and holds its init and shutdown, and the port entry runs
//      the inits before handing control to the game.
//
// Order matters and is the retail module order, because a module's init
// publishes its function-pointer table and a consumer may be initialised first.

#include <cstddef>
#include <cstdio>

extern "C" {
// One per compiled-in module, matching kCompiledModules below. Regenerate with
// tools/rename_module_entries.py rather than by hand.
void mp_coin();
void mp_coin_exit();
void mp_cswarmbasics();
void mp_cswarmbasics_exit();
void mp_metaree();
void mp_metaree_exit();
void mp_playeractorexit();
void mp_playeractormain();
void mp_playerproxy();
void mp_playerproxy_exit();
void mp_puffer();
void mp_puffer_exit();
void mp_relexit_cannonball();
void mp_relexit_tweaks();
void mp_relmain_cannonball();
void mp_relmain_tweaks();
void mp_riftportal();
void mp_riftportal_exit();
void mp_rsfaudio();
void mp_rsfaudio_exit();
void mp_safezone();
void mp_safezone_exit();
void mp_scriptguisetup();
void mp_scriptguisetup_exit();
void mp_skyripple();
void mp_skyripple_exit();
void mp_swarm();
void mp_swarm_exit();
void mp_wallcrawler();
void mp_wallcrawler_exit();
}

namespace port::modules {

namespace {

struct CompiledModule {
  const char* name;
  void (*init)();
  void (*shutdown)();
};

// The names here are the `mp_*` the host build emits, collected from the sources by
// the naming rule in tools/rename_module_entries.py, so the registry cannot drift
// from what the modules actually define. Order is retail module order where known
// and alphabetical otherwise: a module's init publishes its function-pointer table,
// and a consumer initialised first would read a null one.
// Collected from the sources themselves, so the registry cannot drift from what the
// modules actually emit: every entry's `mp_*` names are the ones
// tools/rename_module_entries.py wrote (or, for the three renamed by hand before
// that tool existed, the ones in those files). Order is retail module order where
// known and alphabetical otherwise - a module's init publishes its
// function-pointer table, so a consumer initialised first reads a null one.
// Collected from the sources themselves, so the registry cannot drift from what the
// modules emit. Every `mp_*` here is defined by exactly one module and declared
// above; a name defined but not registered is a module that never initialises, and
// that is worse than a link error because nothing reports it. Order is retail
// module order where known and alphabetical otherwise: a module's init publishes
// its function-pointer table, so a consumer initialised first reads a null one.
constexpr CompiledModule kCompiledModules[] = {
    {"CSwarmBasicsREL", &mp_cswarmbasics, &mp_cswarmbasics_exit},
    {"CFlyerSwarmRel", &mp_swarm, &mp_swarm_exit},
    {"CScriptCannonBall", &mp_relmain_cannonball, &mp_relexit_cannonball},
    {"CScriptCoinRel", &mp_coin, &mp_coin_exit},
    {"CScriptMetaree", &mp_metaree, &mp_metaree_exit},
    {"CScriptPlayerActorMain", &mp_playeractormain, &mp_playeractorexit},
    {"CScriptPlayerProxy", &mp_playerproxy, &mp_playerproxy_exit},
    {"CScriptPufferRel", &mp_puffer, &mp_puffer_exit},
    {"CScriptRiftPortal", &mp_riftportal, &mp_riftportal_exit},
    {"CScriptRsfAudio", &mp_rsfaudio, &mp_rsfaudio_exit},
    {"CScriptSafeZone", &mp_safezone, &mp_safezone_exit},
    {"CScriptSkyRipple", &mp_skyripple, &mp_skyripple_exit},
    {"CScriptWallCrawler", &mp_wallcrawler, &mp_wallcrawler_exit},
    {"ScriptGuiSetup", &mp_scriptguisetup, &mp_scriptguisetup_exit},
    {"Tweaks", &mp_relmain_tweaks, &mp_relexit_tweaks},
};

} // namespace

size_t Count() { return sizeof(kCompiledModules) / sizeof(kCompiledModules[0]); }

void InitAll() {
  for (const CompiledModule& module : kCompiledModules) {
    std::printf("metroid_prime2_port: init compiled module %s\n", module.name);
    module.init();
  }
}

void ShutdownAll() {
  // Reverse order, so a module is torn down before whatever it initialised.
  for (size_t i = Count(); i-- > 0;) {
    // A module need not define an exit point - CScriptPlayerActorMain.cpp has a
    // RELMain and no RELExit - so the table can hold a null shutdown.
    if (kCompiledModules[i].shutdown != nullptr) {
      kCompiledModules[i].shutdown();
    }
  }
}

} // namespace port::modules

// C entry points, so src/REL/REL_Setup.cpp's _prolog/_epilog can forward to the
// registry without depending on a C++ name. On the cube those two call the
// module's own RELMain and RELExit; on the host the registry stands in for both.
extern "C" void port_modules_init_all(void) { port::modules::InitAll(); }

extern "C" void port_modules_shutdown_all(void) { port::modules::ShutdownAll(); }
