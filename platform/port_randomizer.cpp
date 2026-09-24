#include "port_randomizer.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <string>
#include <utility>

namespace PortRandomizer {
namespace {

struct ItemEntry {
  const char* name;
  int value;
};

// Frozen retail data. The enum source is include/MetroidPrime/Player/CPlayerState.hpp;
// tests/port_randomizer.cpp drift-checks these values against the real enum.
constexpr ItemEntry kItems[] = {
    {"PowerBeam", 0},      {"IceBeam", 1},       {"WaveBeam", 2},
    {"PlasmaBeam", 3},     {"Missiles", 4},      {"ScanVisor", 5},
    {"MorphBallBombs", 6}, {"PowerBombs", 7},    {"Flamethrower", 8},
    {"ThermalVisor", 9},   {"ChargeBeam", 10},   {"SuperMissile", 11},
    {"GrappleBeam", 12},   {"XRayVisor", 13},    {"IceSpreader", 14},
    {"SpaceJumpBoots", 15},{"MorphBall", 16},    {"CombatVisor", 17},
    {"BoostBall", 18},     {"SpiderBall", 19},   {"PowerSuit", 20},
    {"GravitySuit", 21},   {"VariaSuit", 22},    {"PhazonSuit", 23},
    {"EnergyTanks", 24},   {"UnknownItem1", 25}, {"HealthRefill", 26},
    {"UnknownItem2", 27},  {"Wavebuster", 28},   {"Truth", 29},
    {"Strength", 30},      {"Elder", 31},        {"Wild", 32},
    {"Lifegiver", 33},     {"Warrior", 34},      {"Chozo", 35},
    {"Nature", 36},        {"Sun", 37},          {"World", 38},
    {"Spirit", 39},        {"Newborn", 40},
};

struct Placement {
  int itemType = -1;
  int amount = 0;
  int capacity = 0;
  bool hasAmount = false;
  bool hasCapacity = false;
};

struct State {
  std::map<std::string, Placement> placements;
  std::string seedName;
  int checkCount = 0;
  bool enabled = false;
  bool dump = false;
};

State& GetState() {
  static State state;
  return state;
}

std::string UserDirectory() {
  std::string dir;
  if (const char* env = std::getenv("MP_USER_PATH")) {
    if (env[0] != '\0')
      dir = env;
  }
  if (dir.empty()) {
    if (char* pref = SDL_GetPrefPath(nullptr, "Metroid Prime")) {
      dir = pref;
      SDL_free(pref);
    } else {
      dir = ".";
    }
  }
  if (!dir.empty() && dir.back() != '/' && dir.back() != '\\')
    dir += '/';
  return dir;
}

std::string SeedPath() {
  if (const char* env = std::getenv("MP_RANDO_SEED")) {
    if (env[0] != '\0')
      return env;
  }
  return UserDirectory() + "randomizer_seed.json";
}

std::string LogPath(const char* name) { return UserDirectory() + name; }

bool EnvEnabled(const char* name) {
  const char* value = std::getenv(name);
  return value != nullptr && value[0] != '\0' && std::strcmp(value, "0") != 0;
}

struct ParseError {
  size_t offset;
  const char* reason;
};

// Seed grammar: a top-level object may contain "seed": string and
// "locations": object; each location maps to an object with required "item":
// string and optional integer "amount"/"capacity". Unknown keys at either
// level are ignored after validating their values. Values may be strings,
// integers, booleans, or objects; arrays, null, comments, and trailing commas
// are not accepted. Whitespace is insignificant.
class Parser {
  const std::string& mText;
  size_t mPos = 0;

  [[noreturn]] void Fail(const char* reason) const { throw ParseError{mPos, reason}; }

  void SkipWhitespace() {
    while (mPos < mText.size() && (mText[mPos] == ' ' || mText[mPos] == '\t' ||
                                   mText[mPos] == '\r' || mText[mPos] == '\n'))
      ++mPos;
  }

  bool Consume(char c) {
    SkipWhitespace();
    if (mPos < mText.size() && mText[mPos] == c) {
      ++mPos;
      return true;
    }
    return false;
  }

  void Expect(char c) {
    SkipWhitespace();
    if (mPos >= mText.size() || mText[mPos] != c)
      Fail("unexpected token");
    ++mPos;
  }

  static void AppendUtf8(std::string& out, unsigned int codepoint) {
    if (codepoint <= 0x7f) {
      out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
      out.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
      out.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
      out.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
      out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
      out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
  }

  unsigned int ParseHex4() {
    if (mText.size() - mPos < 4)
      Fail("incomplete unicode escape");
    unsigned int value = 0;
    for (int i = 0; i < 4; ++i) {
      const char c = mText[mPos++];
      value <<= 4;
      if (c >= '0' && c <= '9')
        value |= static_cast<unsigned int>(c - '0');
      else if (c >= 'a' && c <= 'f')
        value |= static_cast<unsigned int>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F')
        value |= static_cast<unsigned int>(c - 'A' + 10);
      else
        Fail("invalid unicode escape");
    }
    return value;
  }

  std::string ParseString() {
    SkipWhitespace();
    Expect('"');
    std::string result;
    while (mPos < mText.size()) {
      const unsigned char c = static_cast<unsigned char>(mText[mPos++]);
      if (c == '"')
        return result;
      if (c < 0x20)
        Fail("unescaped control character in string");
      if (c != '\\') {
        result.push_back(static_cast<char>(c));
        continue;
      }
      if (mPos >= mText.size())
        Fail("incomplete string escape");
      const char escape = mText[mPos++];
      switch (escape) {
      case '"': result.push_back('"'); break;
      case '\\': result.push_back('\\'); break;
      case '/': result.push_back('/'); break;
      case 'b': result.push_back('\b'); break;
      case 'f': result.push_back('\f'); break;
      case 'n': result.push_back('\n'); break;
      case 'r': result.push_back('\r'); break;
      case 't': result.push_back('\t'); break;
      case 'u': {
        unsigned int cp = ParseHex4();
        if (cp >= 0xd800 && cp <= 0xdbff) {
          if (mText.size() - mPos < 6 || mText[mPos] != '\\' || mText[mPos + 1] != 'u')
            Fail("missing low surrogate");
          mPos += 2;
          const unsigned int low = ParseHex4();
          if (low < 0xdc00 || low > 0xdfff)
            Fail("invalid low surrogate");
          cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
        } else if (cp >= 0xdc00 && cp <= 0xdfff) {
          Fail("unexpected low surrogate");
        }
        AppendUtf8(result, cp);
        break;
      }
      default: Fail("invalid string escape");
      }
    }
    Fail("unterminated string");
  }

  int ParseInteger() {
    SkipWhitespace();
    const size_t start = mPos;
    if (mPos < mText.size() && mText[mPos] == '-')
      ++mPos;
    if (mPos >= mText.size() || mText[mPos] < '0' || mText[mPos] > '9')
      Fail("expected integer");
    if (mText[mPos] == '0') {
      ++mPos;
      if (mPos < mText.size() && mText[mPos] >= '0' && mText[mPos] <= '9')
        Fail("leading zero in integer");
    } else {
      while (mPos < mText.size() && mText[mPos] >= '0' && mText[mPos] <= '9')
        ++mPos;
    }
    long long value = 0;
    try {
      value = std::stoll(mText.substr(start, mPos - start));
    } catch (...) {
      Fail("integer out of range");
    }
    if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
      Fail("integer out of range");
    return static_cast<int>(value);
  }

  bool ConsumeLiteral(const char* literal) {
    SkipWhitespace();
    const size_t length = std::strlen(literal);
    if (mText.compare(mPos, length, literal) != 0)
      return false;
    mPos += length;
    return true;
  }

  void SkipValue(int objectDepth = 0) {
    SkipWhitespace();
    if (mPos >= mText.size())
      Fail("expected value");
    if (mText[mPos] == '"') {
      (void)ParseString();
    } else if (mText[mPos] == '{') {
      SkipObject(objectDepth + 1);
    } else if (mText[mPos] == '-' || (mText[mPos] >= '0' && mText[mPos] <= '9')) {
      (void)ParseInteger();
    } else if (!ConsumeLiteral("true") && !ConsumeLiteral("false")) {
      Fail("unsupported value (expected string, integer, boolean, or object)");
    }
  }

  void SkipObject(int depth) {
    if (depth > 128)
      Fail("object nesting limit exceeded");
    Expect('{');
    if (Consume('}'))
      return;
    for (;;) {
      (void)ParseString();
      Expect(':');
      SkipValue(depth);
      if (Consume('}'))
        return;
      Expect(',');
    }
  }

  Placement ParsePlacement() {
    Placement placement;
    bool hasItem = false;
    Expect('{');
    if (!Consume('}')) {
      for (;;) {
        const std::string key = ParseString();
        Expect(':');
        if (key == "item") {
          const std::string name = ParseString();
          placement.itemType = PortRandomizer::ItemFromName(name.c_str());
          if (placement.itemType < 0)
            Fail("unknown item name");
          hasItem = true;
        } else if (key == "amount") {
          placement.amount = ParseInteger();
          placement.hasAmount = true;
        } else if (key == "capacity") {
          placement.capacity = ParseInteger();
          placement.hasCapacity = true;
        } else {
          SkipValue();
        }
        if (Consume('}'))
          break;
        Expect(',');
      }
    }
    if (!hasItem)
      Fail("placement is missing required item");
    return placement;
  }

  void ParseLocations(std::map<std::string, Placement>& placements) {
    Expect('{');
    if (Consume('}'))
      return;
    for (;;) {
      std::string key = ParseString();
      Expect(':');
      Placement placement = ParsePlacement();
      placements[std::move(key)] = placement;
      if (Consume('}'))
        return;
      Expect(',');
    }
  }

public:
  explicit Parser(const std::string& text) : mText(text) {}

  void Parse(std::string& seedName, std::map<std::string, Placement>& placements) {
    Expect('{');
    if (!Consume('}')) {
      for (;;) {
        const std::string key = ParseString();
        Expect(':');
        if (key == "seed") {
          seedName = ParseString();
        } else if (key == "locations") {
          ParseLocations(placements);
        } else {
          SkipValue();
        }
        if (Consume('}'))
          break;
        Expect(',');
      }
    }
    SkipWhitespace();
    if (mPos != mText.size())
      Fail("trailing data");
  }
};

bool LoadSeed(State& state) {
  const std::string path = SeedPath();
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return false;
  const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  if (input.bad()) {
    std::fprintf(stderr, "randomizer: seed read error at byte offset 0: could not read file\n");
    return false;
  }

  std::string seedName;
  std::map<std::string, Placement> placements;
  try {
    Parser(text).Parse(seedName, placements);
  } catch (const ParseError& error) {
    std::fprintf(stderr, "randomizer: seed parse error at byte offset %zu: %s\n", error.offset,
                 error.reason);
    return false;
  }

  state.seedName = std::move(seedName);
  state.placements = std::move(placements);
  state.enabled = !state.placements.empty();
  return state.enabled;
}

void AppendLog(const char* fileName, const char* line) noexcept {
  try {
    const std::string path = LogPath(fileName);
    std::ofstream output(path, std::ios::app);
    if (output)
      output << line;
  } catch (...) {
  }
}

} // namespace

void EnsureLoaded() {
  try {
    static const bool loaded = []() noexcept {
      try {
        State& state = GetState();
        state.dump = EnvEnabled("MP_RANDO_DUMP");
        (void)LoadSeed(state);
      } catch (const ParseError& error) {
        State& state = GetState();
        state.enabled = false;
        state.placements.clear();
        state.seedName.clear();
        std::fprintf(stderr, "randomizer: seed parse error at byte offset %zu: %s\n", error.offset,
                     error.reason);
      } catch (...) {
        try {
          State& state = GetState();
          state.enabled = false;
          state.placements.clear();
          state.seedName.clear();
        } catch (...) {
        }
      }
      return true;
    }();
    (void)loaded;
    // Announce the configuration once so a run tells the user whether a seed
    // loaded. Silent when the randomizer is inactive, so normal runs are clean.
    static const bool announced = [] {
      const State& state = GetState();
      if (state.dump) {
        std::fprintf(stderr, "randomizer: dump mode (MP_RANDO_DUMP)\n");
      } else if (state.enabled) {
        std::fprintf(stderr, "randomizer: seed '%s', %zu placements\n", state.seedName.c_str(),
                     state.placements.size());
      }
      return true;
    }();
    (void)announced;
  } catch (...) {
    // Includes function-local static initialization failures: the game hook must not unwind.
  }
}

bool Enabled() {
  EnsureLoaded();
  try {
    return GetState().enabled;
  } catch (...) {
    return false;
  }
}

bool DumpEnabled() {
  EnsureLoaded();
  try {
    return GetState().dump;
  } catch (...) {
    return false;
  }
}

const char* SeedName() {
  EnsureLoaded();
  try {
    return GetState().seedName.c_str();
  } catch (...) {
    return "";
  }
}

int CheckCount() {
  EnsureLoaded();
  try {
    return GetState().checkCount;
  } catch (...) {
    return 0;
  }
}

const char* StatusText() {
  EnsureLoaded();
  static char status[48];
  try {
    const State& state = GetState();
    if (state.dump) {
      std::snprintf(status, sizeof(status), "rando: dump mode");
    } else if (state.enabled) {
      std::snprintf(status, sizeof(status), "rando: %.20s, %d checks", state.seedName.c_str(),
                    state.checkCount);
    } else {
      std::snprintf(status, sizeof(status), "rando: off");
    }
  } catch (...) {
    std::snprintf(status, sizeof(status), "rando: off");
  }
  return status;
}

bool ApplyPickup(uint32_t worldAssetId, uint32_t areaAssetId, uint32_t entityId, int& itemType,
                 int& capacity, int& amount) {
  EnsureLoaded();
  try {
    State& state = GetState();
    char key[32];
    FormatLocationKey(worldAssetId, areaAssetId, entityId, key, sizeof(key));
    if (state.dump) {
      char line[128];
      std::snprintf(line, sizeof(line), "LOC %s %s amount=%d capacity=%d\n", key,
                    ItemName(itemType), amount, capacity);
      AppendLog("randomizer_locations.log", line);
      return false;
    }
    if (!state.enabled)
      return false;
    const auto it = state.placements.find(key);
    if (it == state.placements.end())
      return false;
    itemType = it->second.itemType;
    if (it->second.hasAmount)
      amount = it->second.amount;
    if (it->second.hasCapacity)
      capacity = it->second.capacity;
    return true;
  } catch (...) {
    return false;
  }
}

void RecordCheck(uint32_t worldAssetId, uint32_t areaAssetId, uint32_t entityId, int itemType) {
  EnsureLoaded();
  try {
    State& state = GetState();
    if (!state.enabled && !state.dump)
      return;
    if (state.checkCount < std::numeric_limits<int>::max())
      ++state.checkCount;
    char key[32];
    FormatLocationKey(worldAssetId, areaAssetId, entityId, key, sizeof(key));
    char line[128];
    std::snprintf(line, sizeof(line), "CHECK %s %s\n", key, ItemName(itemType));
    AppendLog("randomizer_checks.log", line);
  } catch (...) {
  }
}

const char* ItemName(int itemType) {
  for (const ItemEntry& item : kItems) {
    if (item.value == itemType)
      return item.name;
  }
  return "Unknown";
}

int ItemFromName(const char* name) {
  if (name == nullptr)
    return -1;
  for (const ItemEntry& item : kItems) {
    const unsigned char* left = reinterpret_cast<const unsigned char*>(name);
    const unsigned char* right = reinterpret_cast<const unsigned char*>(item.name);
    while (*left != '\0' && *right != '\0' &&
           std::tolower(*left) == std::tolower(*right)) {
      ++left;
      ++right;
    }
    if (*left == '\0' && *right == '\0')
      return item.value;
  }
  return -1;
}

void FormatLocationKey(uint32_t worldAssetId, uint32_t areaAssetId, uint32_t entityId, char* out,
                       int outSize) {
  if (out == nullptr || outSize <= 0)
    return;
  std::snprintf(out, static_cast<size_t>(outSize), "%08X:%08X:%08X",
                static_cast<unsigned int>(worldAssetId), static_cast<unsigned int>(areaAssetId),
                static_cast<unsigned int>(entityId));
}

} // namespace PortRandomizer
