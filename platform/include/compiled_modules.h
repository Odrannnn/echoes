// The port's registry of REL modules that are compiled into the game library
// rather than loaded from the disc. See compiled_modules.cpp for why it exists.

#pragma once

#include <cstddef>

namespace port::modules {

// How many modules are compiled in, and the one-time init of all of them, in
// retail module order. Called from platform/main.cpp just before InvokeCMain.
size_t Count();
void InitAll();

// Reverse-order teardown, so a module goes down before whatever it initialised.
void ShutdownAll();

} // namespace port::modules

// C entry points, used by src/REL/REL_Setup.cpp's _prolog/_epilog.
extern "C" void port_modules_init_all(void);
extern "C" void port_modules_shutdown_all(void);
