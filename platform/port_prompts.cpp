// Binding-aware button prompt icons; see port_prompts.h.

#include "port_prompts.h"

#include "port_textures.h"

#include <dolphin/gx.h>
#include <dolphin/pad.h>
#include <aurora/texture.hpp>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_scancode.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
// Stands in for the C-stick, which is an axis rather than a button, so the
// table can name the prompts that show it.
constexpr PADButton PAD_AXIS_CSTICK = 0;

// The prompt textures the port can re-icon. Each names the game action it
// stands for, and the hash identifies the game's own texture as it appears in
// a dump. One action usually has several, since each screen draws its own art.
struct PromptKey {
  PADButton button;
  uint32_t width;
  uint32_t height;
  uint64_t hash;
  const char* format;
};
constexpr PromptKey kKeys[] = {
    // Front end.
    {PAD_BUTTON_A, 32, 32, 0xbb21e8755f36b2f0ull, "5"},
    {PAD_BUTTON_B, 32, 32, 0xe6dbfd18d4666ee7ull, "5"},
    // Pause / inventory. The B prompt reuses the front end's texture; the two
    // shoulder prompts have their own, one per side.
    {PAD_BUTTON_A, 32, 32, 0x281ae5aa517797edull, "5"},
    {PAD_TRIGGER_L, 32, 32, 0x3f419d4a7ba3cff3ull, "5"},
    {PAD_TRIGGER_R, 32, 32, 0x178b7311fda3f949ull, "5"},
    // Map screen.
    {PAD_TRIGGER_L, 32, 32, 0x06ad76760dcad506ull, "5"},
    {PAD_TRIGGER_R, 32, 32, 0x45ccec4d3cda3f1bull, "5"},
    {PAD_TRIGGER_Z, 64, 32, 0x0f4cb495c960bcfaull, "14"},
    // HUD hint memos.
    {PAD_TRIGGER_R, 32, 32, 0xc39b2f9c2eac777bull, "5"},
    // Stick prompts. Not a button, so there is no binding to follow; the icon
    // is the device's own stick (or the direction keys for a keyboard).
    {PAD_AXIS_CSTICK, 32, 32, 0x1ff9d2b310c0b706ull, "14"},
    {PAD_AXIS_CSTICK, 64, 32, 0xe14dc493b5513d14ull, "5"},
};
constexpr size_t kKeyCount = sizeof(kKeys) / sizeof(kKeys[0]);

// The actions to resolve bindings for, resolved once per action rather than
// once per texture.
struct PromptAction {
  PADButton button;
  const char* label;
};
constexpr PromptAction kActions[] = {
    {PAD_BUTTON_A, "A"},
    {PAD_BUTTON_B, "B"},
    {PAD_TRIGGER_L, "L"},
    {PAD_TRIGGER_R, "R"},
    {PAD_TRIGGER_Z, "Z"},
    {PAD_AXIS_CSTICK, "stick"},
};
constexpr size_t kActionCount = sizeof(kActions) / sizeof(kActions[0]);

// Scancodes mapped to icon stems. Aurora reports mouse buttons as negative
// codes; the rest are SDL scancodes. Stems match the files generated into
// <textures>/bindings/ by tools/make_prompt_glyphs.py.
struct KeyIcon {
  int scancode;
  const char* stem;
};
constexpr KeyIcon kKeyIcons[] = {
    {SDL_SCANCODE_A, "keyboard_a"}, {SDL_SCANCODE_B, "keyboard_b"},
    {SDL_SCANCODE_C, "keyboard_c"}, {SDL_SCANCODE_D, "keyboard_d"},
    {SDL_SCANCODE_E, "keyboard_e"}, {SDL_SCANCODE_F, "keyboard_f"},
    {SDL_SCANCODE_G, "keyboard_g"}, {SDL_SCANCODE_H, "keyboard_h"},
    {SDL_SCANCODE_I, "keyboard_i"}, {SDL_SCANCODE_J, "keyboard_j"},
    {SDL_SCANCODE_K, "keyboard_k"}, {SDL_SCANCODE_L, "keyboard_l"},
    {SDL_SCANCODE_M, "keyboard_m"}, {SDL_SCANCODE_N, "keyboard_n"},
    {SDL_SCANCODE_O, "keyboard_o"}, {SDL_SCANCODE_P, "keyboard_p"},
    {SDL_SCANCODE_Q, "keyboard_q"}, {SDL_SCANCODE_R, "keyboard_r"},
    {SDL_SCANCODE_S, "keyboard_s"}, {SDL_SCANCODE_T, "keyboard_t"},
    {SDL_SCANCODE_U, "keyboard_u"}, {SDL_SCANCODE_V, "keyboard_v"},
    {SDL_SCANCODE_W, "keyboard_w"}, {SDL_SCANCODE_X, "keyboard_x"},
    {SDL_SCANCODE_Y, "keyboard_y"}, {SDL_SCANCODE_Z, "keyboard_z"},
    {SDL_SCANCODE_0, "keyboard_0"}, {SDL_SCANCODE_1, "keyboard_1"},
    {SDL_SCANCODE_2, "keyboard_2"}, {SDL_SCANCODE_3, "keyboard_3"},
    {SDL_SCANCODE_4, "keyboard_4"}, {SDL_SCANCODE_5, "keyboard_5"},
    {SDL_SCANCODE_6, "keyboard_6"}, {SDL_SCANCODE_7, "keyboard_7"},
    {SDL_SCANCODE_8, "keyboard_8"}, {SDL_SCANCODE_9, "keyboard_9"},
    {SDL_SCANCODE_UP, "keyboard_arrow_up"}, {SDL_SCANCODE_DOWN, "keyboard_arrow_down"},
    {SDL_SCANCODE_LEFT, "keyboard_arrow_left"}, {SDL_SCANCODE_RIGHT, "keyboard_arrow_right"},
    {SDL_SCANCODE_F1, "keyboard_f1"}, {SDL_SCANCODE_F2, "keyboard_f2"},
    {SDL_SCANCODE_F3, "keyboard_f3"}, {SDL_SCANCODE_F4, "keyboard_f4"},
    {SDL_SCANCODE_F5, "keyboard_f5"}, {SDL_SCANCODE_F6, "keyboard_f6"},
    {SDL_SCANCODE_F7, "keyboard_f7"}, {SDL_SCANCODE_F8, "keyboard_f8"},
    {SDL_SCANCODE_F9, "keyboard_f9"}, {SDL_SCANCODE_F10, "keyboard_f10"},
    {SDL_SCANCODE_F11, "keyboard_f11"}, {SDL_SCANCODE_F12, "keyboard_f12"},
    {SDL_SCANCODE_SPACE, "keyboard_space"}, {SDL_SCANCODE_RETURN, "keyboard_enter"},
    {SDL_SCANCODE_ESCAPE, "keyboard_escape"}, {SDL_SCANCODE_TAB, "keyboard_tab"},
    {SDL_SCANCODE_BACKSPACE, "keyboard_backspace"}, {SDL_SCANCODE_DELETE, "keyboard_delete"},
    {SDL_SCANCODE_INSERT, "keyboard_insert"}, {SDL_SCANCODE_HOME, "keyboard_home"},
    {SDL_SCANCODE_END, "keyboard_end"}, {SDL_SCANCODE_PAGEUP, "keyboard_page_up"},
    {SDL_SCANCODE_PAGEDOWN, "keyboard_page_down"},
    {SDL_SCANCODE_LSHIFT, "keyboard_shift"}, {SDL_SCANCODE_RSHIFT, "keyboard_shift"},
    {SDL_SCANCODE_LCTRL, "keyboard_ctrl"}, {SDL_SCANCODE_RCTRL, "keyboard_ctrl"},
    {SDL_SCANCODE_LALT, "keyboard_alt"}, {SDL_SCANCODE_RALT, "keyboard_alt"},
    {PAD_KEY_MOUSE_LEFT, "mouse_left"}, {PAD_KEY_MOUSE_RIGHT, "mouse_right"},
};

// The SDL button a mapping points at, named the way the generated pad icons
// are (tools/make_prompt_glyphs.py writes "<device>_<suffix>").
const char* SuffixForSdlButton(int button) {
  switch (button) {
  case SDL_GAMEPAD_BUTTON_SOUTH: return "south";
  case SDL_GAMEPAD_BUTTON_EAST: return "east";
  case SDL_GAMEPAD_BUTTON_WEST: return "west";
  case SDL_GAMEPAD_BUTTON_NORTH: return "north";
  case SDL_GAMEPAD_BUTTON_START: return "start";
  case SDL_GAMEPAD_BUTTON_BACK: return "back";
  case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return "leftshoulder";
  case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return "rightshoulder";
  case SDL_GAMEPAD_BUTTON_LEFT_STICK: return "leftstick";
  case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return "rightstick";
  default: return nullptr;
  }
}

struct Registration {
  std::string iconPath;  // stable storage for the read callback
  std::string activeStem;
  aurora::texture::ReplacementRegistration handle{};
  bool registered = false;
};
Registration sRegistrations[kKeyCount];
std::string sBindingsDir;
bool sEnabled = false;

// Which input the player last used. The prompts follow that rather than whatever
// happens to be plugged in, so switching between a keyboard, a pad and the touch
// overlay swaps the icons over. Pad is the starting state: with no pad connected
// the texture device resolves to "keyboard" anyway, which is today's behaviour.
enum class ActiveInput { Pad, Keyboard, TouchXbox, TouchGameCube };
std::atomic< ActiveInput > sActiveInput{ActiveInput::Pad};

bool SDLCALL active_input_watch(void*, SDL_Event* event) {
  switch (event->type) {
  case SDL_EVENT_KEY_DOWN:
  case SDL_EVENT_KEY_UP:
  case SDL_EVENT_TEXT_INPUT:
  case SDL_EVENT_MOUSE_MOTION:
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_UP:
  case SDL_EVENT_MOUSE_WHEEL:
    sActiveInput.store(ActiveInput::Keyboard, std::memory_order_relaxed);
    break;
  case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
  case SDL_EVENT_GAMEPAD_BUTTON_UP:
  case SDL_EVENT_GAMEPAD_AXIS_MOTION:
  case SDL_EVENT_GAMEPAD_ADDED:
    sActiveInput.store(ActiveInput::Pad, std::memory_order_relaxed);
    break;
  default:
    break;
  }
  return true;
}

// The icon set for the input in use. "gamecube" has no generated icons, which is
// deliberate: the game's own prompt art already is the GameCube set, so those
// prompts are left alone.
const char* ActiveDevice() {
  switch (sActiveInput.load(std::memory_order_relaxed)) {
  case ActiveInput::Keyboard:
    return "keyboard";
  case ActiveInput::TouchXbox:
    return "xbox";
  case ActiveInput::TouchGameCube:
    return "gamecube";
  case ActiveInput::Pad:
  default:
    return PortTextures::DeviceName();
  }
}

// Serves one generated icon as if it were a replacement file. Called from
// Aurora worker threads, so it only touches the filesystem.
bool ReadIconBytes(void* userData, const char* path, std::vector<uint8_t>& out) {
  // The prompts are 32x32 with a single mip, so any mip sidecar Aurora probes
  // for is a miss rather than the same image again.
  if (path != nullptr && std::strstr(path, "_mip") != nullptr) {
    return false;
  }
  const auto* filePath = static_cast<const std::string*>(userData);
  std::ifstream in(*filePath, std::ios::binary);
  if (!in) {
    return false;
  }
  in.seekg(0, std::ios::end);
  const std::streamoff size = in.tellg();
  if (size <= 0) {
    return false;
  }
  in.seekg(0, std::ios::beg);
  out.resize(static_cast<size_t>(size));
  in.read(reinterpret_cast<char*>(out.data()), size);
  return !in.fail();
}

// The icon stem for whatever is bound to `button` on `device`, or empty when
// the port has no icon for it (in which case the static set stays in place).
// Keyboard and mouse come from the key bindings; a pad follows its own button
// mapping, so a remapped button shows the button it was remapped to.
std::string IconStemForButton(PADButton button, const char* device) {
  const bool keyboard = std::strcmp(device, "keyboard") == 0;
  if (button == PAD_AXIS_CSTICK) {
    // A stick is bound to several keys at once, so the icon says "direction
    // keys" rather than naming one; a pad shows its own stick.
    return keyboard ? std::string("keyboard_arrows") : std::string(device) + "_stick";
  }

  // A pad's L and R are analog triggers, so they have no SDL button for the
  // mapping to name; the generated set carries them under one name per action.
  if (!keyboard && (button == PAD_TRIGGER_L || button == PAD_TRIGGER_R)) {
    return std::string(device) + (button == PAD_TRIGGER_L ? "_lt" : "_rt");
  }

  u32 count = 0;
  if (keyboard) {
    PADKeyButtonBinding* bindings = PADGetKeyButtonBindings(PAD_CHAN0, &count);
    if (bindings == nullptr) {
      return {};
    }
    for (u32 i = 0; i < count; ++i) {
      if (bindings[i].padButton != button) {
        continue;
      }
      for (const KeyIcon& icon : kKeyIcons) {
        if (icon.scancode == bindings[i].scancode) {
          return icon.stem;
        }
      }
      return {};
    }
    return {};
  }

  PADButtonMapping* mappings = PADGetButtonMappings(PAD_CHAN0, &count);
  if (mappings == nullptr) {
    return {};
  }
  for (u32 i = 0; i < count; ++i) {
    if (mappings[i].padButton != button) {
      continue;
    }
    const char* suffix = SuffixForSdlButton(static_cast<int>(mappings[i].nativeButton));
    return suffix != nullptr ? std::string(device) + "_" + suffix : std::string();
  }
  return {};
}

void Apply(size_t index, const std::string& stem) {
  Registration& reg = sRegistrations[index];
  if (reg.registered) {
    aurora::texture::unregister_replacement(reg.handle);
    reg.handle = {};
    reg.registered = false;
  }
  reg.activeStem = stem;
  if (stem.empty()) {
    return;
  }

  const PromptKey& key = kKeys[index];
  char keyName[80];
  std::snprintf(keyName, sizeof(keyName), "tex1_%ux%u_%016llx_%s.dds", key.width, key.height,
                static_cast<unsigned long long>(key.hash), key.format);
  reg.iconPath = (std::filesystem::path(sBindingsDir) / (stem + ".dds")).string();
  // A device with no generated icon for this action keeps the game's own art
  // rather than registering a source that would fail to load and blank it.
  std::error_code existsError;
  if (!std::filesystem::is_regular_file(reg.iconPath, existsError)) {
    reg.activeStem.clear();
    return;
  }
  reg.handle = aurora::texture::register_virtual_replacement(
      keyName, aurora::texture::VirtualFileSource{&ReadIconBytes, &reg.iconPath});
  reg.registered = reg.handle.id != 0;
}

const char* LabelForButton(PADButton button) {
  for (const PromptAction& action : kActions) {
    if (action.button == button) {
      return action.label;
    }
  }
  return "?";
}
} // namespace

namespace PortPrompts {
void Initialize(const char* textureRoot) {
  if (textureRoot == nullptr || textureRoot[0] == '\0') {
    return;
  }
  const std::filesystem::path bindingsDir = std::filesystem::path(textureRoot) / "bindings";
  std::error_code ec;
  if (!std::filesystem::is_directory(bindingsDir, ec)) {
    return;
  }
  sBindingsDir = bindingsDir.string();
  sEnabled = true;
  SDL_AddEventWatch(active_input_watch, nullptr);
  Poll();
}

void Poll() {
  if (!sEnabled) {
    return;
  }
  const char* device = ActiveDevice();
  for (const PromptAction& action : kActions) {
    const std::string stem = IconStemForButton(action.button, device);
    size_t applied = 0;
    for (size_t i = 0; i < kKeyCount; ++i) {
      if (kKeys[i].button != action.button) {
        continue;
      }
      const std::string& current = sRegistrations[i].activeStem;
      if (stem.empty() && current.empty()) {
        continue;
      }
      if (!stem.empty() && stem == current) {
        continue;
      }
      Apply(i, stem);
      ++applied;
    }
    if (applied != 0) {
      std::fprintf(stderr, "metroid_prime_port: prompt %s %s\n", LabelForButton(action.button),
                   stem.empty() ? "(back to the static icon)" : stem.c_str());
    }
  }
}
void NoteTouchInput(bool xboxLayout) {
  sActiveInput.store(xboxLayout ? ActiveInput::TouchXbox : ActiveInput::TouchGameCube,
                     std::memory_order_relaxed);
}
} // namespace PortPrompts
