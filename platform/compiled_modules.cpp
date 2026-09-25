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
void mp_relmain_tweaks();
void mp_relexit_tweaks();
void mp_relmain_cannonball();
void mp_relexit_cannonball();
void mp_relmain_forgottenobject();
void mp_relexit_forgottenobject();
}

namespace port::modules {

namespace {

struct CompiledModule {
  const char* name;
  void (*init)();
  void (*shutdown)();
};

constexpr CompiledModule kCompiledModules[] = {
    {"Tweaks", &mp_relmain_tweaks, &mp_relexit_tweaks},
    {"CannonBall", &mp_relmain_cannonball, &mp_relexit_cannonball},
    {"ForgottenObject", &mp_relmain_forgottenobject, &mp_relexit_forgottenobject},
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
    kCompiledModules[i].shutdown();
  }
}

} // namespace port::modules

// C entry points, so src/REL/REL_Setup.cpp's _prolog/_epilog can forward to the
// registry without depending on a C++ name. On the cube those two call the
// module's own RELMain and RELExit; on the host the registry stands in for both.
extern "C" void port_modules_init_all(void) { port::modules::InitAll(); }

extern "C" void port_modules_shutdown_all(void) { port::modules::ShutdownAll(); }
