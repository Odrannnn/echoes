// The port's registry of REL modules that are compiled into the game library
// rather than loaded from the disc.
//
// The other modules are read out of the ISO and relocated by platform/rel.cpp,
// which calls each module's prolog and epilog by guest address inside the loaded
// image. The ones listed below are not: we have our own sources for them in src/,
// and those are compiled straight into mp_game. That is why they need this file.
//
// On the cube each of them is a separate module with its own RELMain and
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
#include <cstdlib>
#include <cstring>

extern "C" {
// One per compiled-in module, matching kCompiledModules below. Regenerate with
// tools/rename_module_entries.py rather than by hand.
// CSwarmBasicsREL.cpp is not in files.cmake yet, so its two names are still the
// ones the boot-probe stubs define.
void mp_cswarmbasics();
void mp_cswarmbasics_exit();
void mp_relexit_cannonball();
void mp_relexit_coin();
void mp_relexit_metaree();
void mp_relexit_playerproxy();
void mp_relexit_puffer();
void mp_relexit_riftportal();
void mp_relexit_rsfaudio();
void mp_relexit_safezone();
void mp_relexit_scriptguisetup();
void mp_relexit_skyripple();
void mp_relexit_swarm();
void mp_relexit_tweaks();
void mp_relexit_wallcrawler();
void mp_relmain_cannonball();
void mp_relmain_coin();
void mp_relmain_metaree();
void mp_relmain_playeractormain();
void mp_relmain_playerproxy();
void mp_relmain_puffer();
void mp_relmain_riftportal();
void mp_relmain_rsfaudio();
void mp_relmain_safezone();
void mp_relmain_scriptguisetup();
void mp_relmain_skyripple();
void mp_relmain_swarm();
void mp_relmain_tweaks();
void mp_relmain_wallcrawler();
}

namespace port::modules {

namespace {

struct CompiledModule {
  const char* name;
  // The module's file on the disc, without the directory and the ".rel": the name
  // `configure.py`'s `Rel(...)` block gives it, and the one retail's module manager
  // holds in its record (`RelProd/Tweaks.rel` and so on). Prolog/Epilog match on it.
  const char* file;
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
    {"CSwarmBasicsREL", "SwarmBasics", &mp_cswarmbasics, &mp_cswarmbasics_exit},
    {"CFlyerSwarmRel", "FlyerSwarm", &mp_relmain_swarm, &mp_relexit_swarm},
    {"CScriptCannonBall", "ScriptCannonBall", &mp_relmain_cannonball, &mp_relexit_cannonball},
    {"CScriptCoinRel", "ScriptCoin", &mp_relmain_coin, &mp_relexit_coin},
    {"CScriptMetaree", "Metaree", &mp_relmain_metaree, &mp_relexit_metaree},
    // Retail's module has a prolog and no epilog, so there is no exit point.
    {"CScriptPlayerActorMain", "ScriptPlayerActor", &mp_relmain_playeractormain, nullptr},
    {"CScriptPlayerProxy", "ScriptPlayerProxy", &mp_relmain_playerproxy, &mp_relexit_playerproxy},
    {"CScriptPufferRel", "Puffer", &mp_relmain_puffer, &mp_relexit_puffer},
    {"CScriptRiftPortal", "ScriptRiftPortal", &mp_relmain_riftportal, &mp_relexit_riftportal},
    {"CScriptRsfAudio", "ScriptRsfAudio", &mp_relmain_rsfaudio, &mp_relexit_rsfaudio},
    {"CScriptSafeZone", "ScriptSafeZone", &mp_relmain_safezone, &mp_relexit_safezone},
    {"CScriptSkyRipple", "SkyRipple", &mp_relmain_skyripple, &mp_relexit_skyripple},
    {"CScriptWallCrawler", "WallCrawler", &mp_relmain_wallcrawler, &mp_relexit_wallcrawler},
    {"ScriptGuiSetup", "ScriptGui", &mp_relmain_scriptguisetup, &mp_relexit_scriptguisetup},
    {"Tweaks", "Tweaks", &mp_relmain_tweaks, &mp_relexit_tweaks},
};

// Whether each module's init has run and its shutdown has not, indexed like
// kCompiledModules. InitAll runs every init at port entry, and retail's module
// manager then asks for a module's prolog when it links the image and its epilog
// when it unlinks it (src/MetroidPrime/CRelFile.cpp). The flag is what keeps
// both callers honest: a prolog of a module InitAll already started is a no-op, and
// an epilog runs the shutdown once and lets a later prolog run the init again.
bool gInitialised[sizeof(kCompiledModules) / sizeof(kCompiledModules[0])];

// `path` is what retail's record holds: an optional directory, the file, ".rel".
// Returns the table index, or -1 for a module that is not compiled in.
int Find(const char* path) {
  const char* file = std::strrchr(path, '/');
  file = file != nullptr ? file + 1 : path;
  size_t length = std::strlen(file);
  if (length > 4 && std::strcmp(file + length - 4, ".rel") == 0) {
    length -= 4;
  }
  for (size_t i = 0; i < sizeof(kCompiledModules) / sizeof(kCompiledModules[0]); ++i) {
    const char* candidate = kCompiledModules[i].file;
    if (std::strlen(candidate) == length && std::strncmp(candidate, file, length) == 0) {
      return static_cast< int >(i);
    }
  }
  return -1;
}

// Every module retail can ask for that this table lacks is PowerPC code only: the port
// has no host body to run, and relocating the disc image on the host would give it
// PowerPC instructions it cannot execute. So it is a declared stop, not a no-op that
// would leave the module's loaders unregistered with nothing reporting it.
int FindOrAbort(const char* path, const char* what) {
  const int index = Find(path);
  if (index < 0) {
    std::printf("metroid_prime2_port: module %s: %s is not compiled into the port - "
                "no host code to run\n", what, path);
    std::fflush(nullptr);
    std::abort();
  }
  return index;
}

} // namespace

size_t Count() { return sizeof(kCompiledModules) / sizeof(kCompiledModules[0]); }

void InitAll() {
  for (size_t i = 0; i < Count(); ++i) {
    std::printf("metroid_prime2_port: init compiled module %s\n", kCompiledModules[i].name);
    kCompiledModules[i].init();
    gInitialised[i] = true;
  }
}

void ShutdownAll() {
  // Reverse order, so a module is torn down before whatever it initialised.
  for (size_t i = Count(); i-- > 0;) {
    // A module need not define an exit point - CScriptPlayerActorMain.cpp has a
    // RELMain and no RELExit - so the table can hold a null shutdown.
    if (gInitialised[i] && kCompiledModules[i].shutdown != nullptr) {
      kCompiledModules[i].shutdown();
    }
    gInitialised[i] = false;
  }
}

void Prolog(const char* path) {
  const int i = FindOrAbort(path, "prolog");
  if (!gInitialised[i]) {
    kCompiledModules[i].init();
    gInitialised[i] = true;
  }
}

void Epilog(const char* path) {
  const int i = FindOrAbort(path, "epilog");
  if (gInitialised[i] && kCompiledModules[i].shutdown != nullptr) {
    kCompiledModules[i].shutdown();
  }
  gInitialised[i] = false;
}

} // namespace port::modules

// C entry points, so src/REL/REL_Setup.cpp's _prolog/_epilog can forward to the
// registry without depending on a C++ name. On the cube those two call the
// module's own RELMain and RELExit; on the host the registry stands in for both.
extern "C" void port_modules_init_all(void) { port::modules::InitAll(); }

extern "C" void port_modules_shutdown_all(void) { port::modules::ShutdownAll(); }

extern "C" void port_modules_prolog(const char* path) { port::modules::Prolog(path); }

extern "C" void port_modules_epilog(const char* path) { port::modules::Epilog(path); }
