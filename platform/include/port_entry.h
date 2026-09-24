#pragma once

// Port entry-point helpers: what the process entry needs before it can hand
// control to the game. Kept free of game types so they can be tested without
// linking the game (see tests/port_entry_tests.cpp).

#include <string>

#include <dolphin/dvd.h>

namespace port {
namespace entry {

// The disc this port is built for: Metroid Prime 2: Echoes, USA v1.00
// (`G2ME01`), disc 0, revision 0 - the version the decompilation targets.
bool IsSupportedDisc(const DVDDiskID& id);

// Disc-image file names the port will pick up beside the executable.
bool IsDiscImageName(const std::string& name);

// Disc image path, in order: an explicit non-flag argument, `envDiscPath`, then
// the first disc image in `executableDir`. `envDiscPath` may be null. Returns an
// empty string when nothing is found. The caller supplies the environment and
// the directory so this stays testable.
std::string ResolveDiscPath(int argc, char** argv, const char* envDiscPath,
                            const std::string& executableDir);

} // namespace entry
} // namespace port
