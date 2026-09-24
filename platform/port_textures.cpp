// Device-aware HD texture replacement loading; see port_textures.h.

#include "port_textures.h"

#include <dolphin/gx.h>
#include <dolphin/pad.h>
#include <aurora/texture.hpp>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

namespace {
aurora::texture::ReplacementGroup sGroup;
std::string sRoot;
std::string sDevice = "keyboard";
bool sLoaded = false;

// Every name DeviceDirForType can return; used to tell a device-pack root from
// a single device-agnostic pack.
constexpr const char* kDeviceDirs[] = {"xbox", "playstation", "switch", "gamecube", "standard", "keyboard"};

// SDL's gamepad types back PADControllerType, so this mirrors the pad in use.
const char* DeviceDirForType(PADControllerType type) {
  switch (type) {
  case PAD_TYPE_XBOX360:
  case PAD_TYPE_XBOXONE:
    return "xbox";
  case PAD_TYPE_PS3:
  case PAD_TYPE_PS4:
  case PAD_TYPE_PS5:
    return "playstation";
  case PAD_TYPE_SWITCH_PROCON:
  case PAD_TYPE_JOYCON_LEFT:
  case PAD_TYPE_JOYCON_RIGHT:
  case PAD_TYPE_JOYCON_PAIR:
    return "switch";
  case PAD_TYPE_GAMECUBE:
  case PAD_TYPE_NSO_GAMECUBE:
    return "gamecube";
  case PAD_TYPE_STANDARD:
    return "standard";
  default:
    return "keyboard";
  }
}

// The override, else the name for the connected pad. Never allocates.
const char* ResolveDeviceName() {
  const char* env = std::getenv("MP_TEXTURE_DEVICE");
  if (env != nullptr && env[0] != '\0') {
    return env;
  }
  return DeviceDirForType(PADGetControllerType(PAD_CHAN0));
}

// Prefer the device folder, falling back to the root when it is absent. When
// the root holds device folders but not the one we need, load nothing: the
// directory scan is recursive, so falling back would merge the other devices'
// packs into this one.
std::optional<std::filesystem::path> ResolveDirectory() {
  const std::filesystem::path base(sRoot);
  std::error_code ec;
  const std::filesystem::path device = base / sDevice;
  if (std::filesystem::is_directory(device, ec)) {
    return device;
  }
  for (const char* name : kDeviceDirs) {
    if (std::filesystem::is_directory(base / name, ec)) {
      return std::nullopt;
    }
  }
  return base;
}

void Load() {
  if (sRoot.empty()) {
    return;
  }
  const std::optional<std::filesystem::path> dir = ResolveDirectory();
  if (!dir.has_value()) {
    if (sLoaded) {
      aurora::texture::unregister_replacements(sGroup);
      sGroup = {};
    }
    std::fprintf(stderr, "metroid_prime_port: no texture replacements for device '%s' in %s\n",
                 sDevice.c_str(), sRoot.c_str());
    return;
  }
  if (sLoaded) {
    aurora::texture::reload_replacement_directory(*dir, sGroup);
  } else {
    sGroup = aurora::texture::load_replacement_directory(*dir);
    sLoaded = true;
  }
  std::fprintf(stderr, "metroid_prime_port: loaded %zu texture replacements from %s (device: %s)\n",
               sGroup.registrations.size(), dir->string().c_str(), sDevice.c_str());
}
} // namespace

namespace PortTextures {
void Initialize(const char* root) {
  if (root == nullptr || root[0] == '\0') {
    return;
  }
  sRoot = root;
  sDevice = ResolveDeviceName();
  Load();
}

void Poll() {
  if (sRoot.empty()) {
    return;
  }
  const char* device = ResolveDeviceName();
  if (sDevice == device) {
    return;
  }
  sDevice = device;
  Load();
}

const char* DeviceName() { return sDevice.c_str(); }
} // namespace PortTextures
