#pragma once

// Device-aware HD texture replacements.
//
// Replacements live in the folder given by MP_TEXTURES (or `textures` next to
// the executable), in Aurora's naming convention. Inside it an optional
// per-device subfolder is selected from the connected controller:
// xbox, playstation, switch, gamecube, standard, or keyboard when no pad is
// connected. The subfolder is used when it exists, otherwise the root is used,
// so a single device-agnostic pack keeps working. MP_TEXTURE_DEVICE overrides
// the detected name. The set is reloaded when the active device changes.
//
// Texture dumping (for authoring replacements) is enabled with
// MP_DUMP_TEXTURES; dumps land in <cachePath>/texture_dumps.
namespace PortTextures {
// Resolves the device folder, loads the replacement set and remembers the base
// folder. `root` may be null or empty to leave replacements disabled.
void Initialize(const char* root);

// Reloads the set when the active device changed. Call once per frame.
void Poll();

// Name of the current device folder (never null).
const char* DeviceName();
} // namespace PortTextures
