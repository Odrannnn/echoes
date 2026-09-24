// Real REL integration check.
//
// Validates the port's module runtime against REL modules extracted from a disc
// the user owns. It relies on an independent reader for the relocation format -
// one that walks the lists and decodes the patched instructions itself - so it
// disagrees with the loader when the loader is wrong.
//
//   port_rel_real_tests <module.rel> [dol-symbols.txt]
//   port_rel_real_tests --dir <rel-directory> [dol-symbols.txt]
//
// `--dir` links every module in the directory in dependency order, so modules
// that import each other resolve, and checks all of them. With no arguments the
// test reports itself skipped (exit 77), so a checkout without a disc passes.

#include "port_rel.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {
using namespace port::rel;

int g_failures = 0;
int g_modules = 0;
size_t g_checks = 0;
size_t g_dolImports = 0;
size_t g_unresolved = 0;
size_t g_unexercised = 0;

void Fail(const std::string& what) {
  std::fprintf(stderr, "FAIL: %s\n", what.c_str());
  g_failures++;
}

void Check(bool condition, const std::string& what) {
  if (!condition) {
    Fail(what);
  }
}

void CheckEq(uint64_t actual, uint64_t expected, const std::string& what) {
  if (actual != expected) {
    std::fprintf(stderr, "FAIL: %s: got 0x%llx expected 0x%llx\n", what.c_str(),
                 static_cast<unsigned long long>(actual),
                 static_cast<unsigned long long>(expected));
    g_failures++;
  }
}

bool ReadFile(const std::string& path, std::vector<uint8_t>& out) {
  std::FILE* file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) {
    return false;
  }
  std::fseek(file, 0, SEEK_END);
  const long size = std::ftell(file);
  std::fseek(file, 0, SEEK_SET);
  if (size <= 0) {
    std::fclose(file);
    return false;
  }
  out.resize(static_cast<size_t>(size));
  const size_t read = std::fread(out.data(), 1, out.size(), file);
  std::fclose(file);
  return read == out.size();
}

// Section payloads stay in guest byte order, so the independent reader reads and
// decodes them big-endian.
uint32_t BE32(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

uint16_t BE16(const uint8_t* p) {
  return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

uint32_t Guest32(const void* base, size_t offset) {
  return BE32(static_cast<const uint8_t*>(base) + offset);
}

uint16_t Guest16(const void* base, size_t offset) {
  return BE16(static_cast<const uint8_t*>(base) + offset);
}

int32_t SignExtend(uint32_t value, int bits) {
  if ((value & (1u << (bits - 1))) != 0) {
    return static_cast<int32_t>(value | ~((1u << bits) - 1));
  }
  return static_cast<int32_t>(value);
}

uint32_t HiAdj(uint32_t value) {
  return ((value >> 16) + ((value & 0x8000) ? 1 : 0)) & 0xffff;
}

std::vector<uint32_t> SectionOffsets(const std::vector<uint8_t>& file, const ImageInfo& info) {
  std::vector<uint32_t> offsets(info.numSections);
  const uint32_t raw = BE32(&file[0x10]);
  for (uint32_t i = 0; i < info.numSections; i++) {
    offsets[i] = BE32(&file[raw + i * 8]);
  }
  return offsets;
}

uint32_t LinkedSectionBase(const RelModuleHeader* header, uint32_t section) {
  return kSectionInfoOffset(SectionTable(const_cast<RelModuleHeader*>(header))[section].offset);
}

// --- independent reader -----------------------------------------------------

struct Reloc {
  size_t imageOffset; // where the relocation writes, relative to the image start
  uint8_t type;
  uint8_t section; // for non-DOL imports: an index into the *target* module's table
  uint32_t addend;
};

struct ImportList {
  uint32_t id = 0;
  std::vector<Reloc> relocations;
  std::vector<uint8_t> types; // includes the control records, for counting
};

std::vector<ImportList> ReadImports(const std::vector<uint8_t>& file,
                                    const std::vector<uint32_t>& sectionOffsets, size_t impOffset,
                                    size_t impSize) {
  std::vector<ImportList> result;
  for (size_t entry = 0; entry + 8 <= impSize; entry += 8) {
    ImportList list;
    list.id = BE32(&file[impOffset + entry]);
    size_t cursor = BE32(&file[impOffset + entry + 4]);
    size_t position = 0;
    bool positioned = false;
    while (cursor + 8 <= file.size()) {
      const uint16_t delta = BE16(&file[cursor]);
      const uint8_t type = file[cursor + 2];
      const uint8_t section = file[cursor + 3];
      const uint32_t addend = BE32(&file[cursor + 4]);
      cursor += 8;
      list.types.push_back(type);
      if (positioned) {
        position += delta;
      }
      if (type == kRelocDolphinSection) {
        positioned = true;
        position = sectionOffsets[section] & ~1u;
      } else if (type == kRelocDolphinEnd) {
        break;
      } else if (type != kRelocDolphinNop) {
        list.relocations.push_back({position, type, section, addend});
      }
    }
    result.push_back(std::move(list));
  }
  return result;
}

// --- DOL symbols ------------------------------------------------------------

std::set<uint32_t> ReadSymbols(const std::string& path) {
  std::set<uint32_t> symbols;
  std::FILE* file = std::fopen(path.c_str(), "r");
  if (file == nullptr) {
    return symbols;
  }
  char line[512];
  while (std::fgets(line, sizeof(line), file) != nullptr) {
    const char* equals = std::strchr(line, '=');
    const char* colon = equals != nullptr ? std::strchr(equals, ':') : nullptr;
    const char* end = colon != nullptr ? std::strchr(colon, ';') : nullptr;
    if (end == nullptr) {
      continue;
    }
    uint32_t value = 0;
    bool valid = false;
    for (const char* c = colon + 1; c < end; ++c) {
      const char digit = *c;
      if (digit == 'x' || digit == 'X') {
        continue;
      }
      int nibble = -1;
      if (digit >= '0' && digit <= '9') {
        nibble = digit - '0';
      } else if (digit >= 'a' && digit <= 'f') {
        nibble = digit - 'a' + 10;
      } else if (digit >= 'A' && digit <= 'F') {
        nibble = digit - 'A' + 10;
      }
      if (nibble < 0) {
        valid = false;
        break;
      }
      value = (value << 4) | static_cast<uint32_t>(nibble);
      valid = true;
    }
    if (valid) {
      symbols.insert(value);
    }
  }
  std::fclose(file);
  return symbols;
}

// --- module extents ---------------------------------------------------------

struct Extent {
  uint32_t id = 0;
  uint32_t base = 0;
  uint32_t imageSize = 0;
  uint32_t bss = 0;
  uint32_t bssSize = 0;
  const RelModuleHeader* header = nullptr;

  bool Contains(uint32_t value) const {
    return (value >= base && value < base + imageSize) ||
           (bssSize != 0 && value >= bss && value < bss + bssSize);
  }
};

const char* TypeName(uint8_t type) {
  switch (type) {
  case kRelocNone: return "R_PPC_NONE";
  case kRelocAddr32: return "R_PPC_ADDR32";
  case kRelocAddr24: return "R_PPC_ADDR24";
  case kRelocAddr16: return "R_PPC_ADDR16";
  case kRelocAddr16Lo: return "R_PPC_ADDR16_LO";
  case kRelocAddr16Hi: return "R_PPC_ADDR16_HI";
  case kRelocAddr16Ha: return "R_PPC_ADDR16_HA";
  case kRelocAddr14: return "R_PPC_ADDR14";
  case kRelocAddr14Brtaken: return "R_PPC_ADDR14_BRTAKEN";
  case kRelocAddr14Brntaken: return "R_PPC_ADDR14_BRNTAKEN";
  case kRelocRel24: return "R_PPC_REL24";
  case kRelocRel14: return "R_PPC_REL14";
  case kRelocRel14Brtaken: return "R_PPC_REL14_BRTAKEN";
  case kRelocRel14Brntaken: return "R_PPC_REL14_BRNTAKEN";
  case kRelocDolphinNop: return "R_DOLPHIN_NOP";
  case kRelocDolphinSection: return "R_DOLPHIN_SECTION";
  case kRelocDolphinEnd: return "R_DOLPHIN_END";
  default: return "unknown";
  }
}

bool Supported(uint8_t type) {
  return type == kRelocNone || type == kRelocAddr32 || type == kRelocAddr24 ||
         type == kRelocAddr16 || type == kRelocAddr16Lo || type == kRelocAddr16Hi ||
         type == kRelocAddr16Ha || type == kRelocAddr14 || type == kRelocAddr14Brtaken ||
         type == kRelocAddr14Brntaken || type == kRelocRel24 || type == kRelocRel14 ||
         type == kRelocRel14Brtaken || type == kRelocRel14Brntaken || type == kRelocDolphinNop ||
         type == kRelocDolphinSection || type == kRelocDolphinEnd;
}

// Header, section placement, bss and entry points of a freshly linked module.
void ValidateHeader(const std::vector<uint8_t>& file, const ImageInfo& info, const Module& module,
                    const std::string& label) {
  const uint32_t base = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(module.Image()));
  Check(info.version >= 1 && info.version <= 3, label + ": version in range");
  CheckEq(info.imageSize, file.size(), label + ": the whole file is mapped");

  const uint32_t rawSectionInfo = BE32(&file[0x10]);
  const uint32_t rawImpOffset = BE32(&file[0x28]);
  const uint32_t rawImpSize = BE32(&file[0x2C]);
  Check(rawSectionInfo + info.numSections * 8 <= file.size(), label + ": section table fits");
  Check(rawImpOffset + rawImpSize <= file.size(), label + ": import table fits");
  if (info.version >= 3) {
    Check(info.fixedSize != 0 && info.fixedSize <= file.size(), label + ": fixedSize fits the file");
    Check(rawImpOffset + rawImpSize <= info.fixedSize, label + ": import table is in the fixed part");
    CheckEq(module.Header()->fixSize, base + info.fixedSize, label + ": fixSize relocated");
  }

  CheckEq(module.Header()->sectionInfoOffset, base + rawSectionInfo, label + ": section table relocated");
  CheckEq(module.Header()->impOffset, base + rawImpOffset, label + ": import table relocated");

  uint32_t bssSection = 0;
  uint32_t bssSize = 0;
  for (uint32_t i = 1; i < info.numSections; i++) {
    const uint32_t rawOffset = BE32(&file[rawSectionInfo + i * 8]);
    const uint32_t rawSize = BE32(&file[rawSectionInfo + i * 8 + 4]);
    if (rawOffset != 0) {
      CheckEq(SectionTable(module.Header())[i].offset, base + rawOffset,
              label + ": section " + std::to_string(i) + " base");
    } else if (rawSize != 0) {
      if (bssSection == 0) {
        bssSection = i;
        bssSize = rawSize;
      }
      CheckEq(LinkedSectionBase(module.Header(), i),
              static_cast<uint32_t>(reinterpret_cast<uintptr_t>(module.Bss())),
              label + ": bss section base");
    }
  }
  CheckEq(module.Header()->bssSection, bssSection, label + ": bssSection index");
  CheckEq(info.bssSize, bssSize, label + ": declared bssSize matches the bss section");
  CheckEq(module.Header()->bssSize, bssSize, label + ": linked bssSize");

  const auto entry = [&](uint8_t section, uint32_t offset) {
    return section == 0 ? 0u : LinkedSectionBase(module.Header(), section) + offset;
  };
  CheckEq(module.Header()->prolog, entry(file[0x30], BE32(&file[0x34])), label + ": prolog entry");
  CheckEq(module.Header()->epilog, entry(file[0x31], BE32(&file[0x38])), label + ": epilog entry");
  CheckEq(module.Header()->unresolved, entry(file[0x32], BE32(&file[0x3C])),
          label + ": unresolved entry");
}

// Every relocation the module wrote, decoded back out of the patched image.
void ValidateRelocations(const std::vector<uint8_t>& file, const ImageInfo& info,
                         const Module& module, const std::vector<ImportList>& imports,
                         const std::map<uint32_t, Extent>& loaded, const std::set<uint32_t>* symbols,
                         const std::string& label) {
  const uint32_t base = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(module.Image()));
  Extent self;
  self.id = info.moduleId;
  self.base = base;
  self.imageSize = static_cast<uint32_t>(info.imageSize);
  self.bss = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(module.Bss()));
  self.bssSize = info.bssSize;
  self.header = module.Header();

  for (const ImportList& list : imports) {
    for (uint8_t type : list.types) {
      Check(Supported(type),
            label + ": relocation type " + TypeName(type) + " is implemented");
    }

    if (list.id == 0) {
      for (const Reloc& reloc : list.relocations) {
        g_dolImports++;
        const size_t site = reloc.imageOffset;
        switch (reloc.type) {
        case kRelocRel24: {
          const uint32_t word = Guest32(module.Image(), site);
          CheckEq(word >> 26, 18, label + ": DOL REL24 patches a branch");
          const int32_t displacement = SignExtend(word & 0x03fffffcu, 26);
          CheckEq(static_cast<uint32_t>(base + site + displacement), reloc.addend,
                  label + ": DOL REL24 reaches its address");
          break;
        }
        case kRelocAddr32:
          CheckEq(Guest32(module.Image(), site), reloc.addend, label + ": DOL ADDR32");
          break;
        case kRelocAddr16Ha:
          CheckEq(Guest16(module.Image(), site), HiAdj(reloc.addend), label + ": DOL ADDR16_HA");
          break;
        case kRelocAddr16Lo:
          CheckEq(Guest16(module.Image(), site), reloc.addend & 0xffff, label + ": DOL ADDR16_LO");
          break;
        case kRelocAddr16Hi:
          CheckEq(Guest16(module.Image(), site), (reloc.addend >> 16) & 0xffff,
                  label + ": DOL ADDR16_HI");
          break;
        default:
          Fail(label + ": unexpected DOL relocation type " + TypeName(reloc.type));
          break;
        }
        if (symbols != nullptr && symbols->find(reloc.addend) == symbols->end()) {
          Fail(label + ": DOL address " + std::to_string(reloc.addend) + " is not a known symbol");
        }
        g_checks++;
      }
      continue;
    }

    const Extent* target = nullptr;
    if (list.id == info.moduleId) {
      target = &self;
    } else if (const auto found = loaded.find(list.id); found != loaded.end()) {
      target = &found->second;
    }
    if (target == nullptr) {
      g_unresolved += list.relocations.size();
      continue;
    }

    std::vector<bool> paired(list.relocations.size(), false);
    for (size_t index = 0; index < list.relocations.size(); index++) {
      const Reloc& reloc = list.relocations[index];
      const size_t site = reloc.imageOffset;
      switch (reloc.type) {
      case kRelocAddr32: {
        const uint32_t value = Guest32(module.Image(), site);
        CheckEq(value, LinkedSectionBase(target->header, reloc.section) + reloc.addend,
                label + ": ADDR32 to module " + std::to_string(list.id) + " resolves");
        Check(target->Contains(value),
              label + ": ADDR32 to module " + std::to_string(list.id) + " lands inside it");
        break;
      }
      case kRelocAddr16Ha:
      case kRelocAddr16Hi: {
        const Reloc* low = nullptr;
        for (size_t other = 0; other < list.relocations.size(); other++) {
          if (paired[other] || other == index) {
            continue;
          }
          const Reloc& candidate = list.relocations[other];
          if (candidate.type == kRelocAddr16Lo && candidate.section == reloc.section &&
              candidate.addend == reloc.addend) {
            low = &candidate;
            paired[other] = true;
            break;
          }
        }
        if (low == nullptr) {
          Fail(label + ": high half to module " + std::to_string(list.id) + " has no low half");
          break;
        }
        const uint16_t high = Guest16(module.Image(), site);
        const uint16_t lowValue = Guest16(module.Image(), low->imageOffset);
        const uint32_t decoded =
            reloc.type == kRelocAddr16Ha
                ? static_cast<uint32_t>((static_cast<int32_t>(SignExtend(high, 16)) << 16) +
                                        SignExtend(lowValue, 16))
                : static_cast<uint32_t>((static_cast<uint32_t>(high) << 16) | lowValue);
        CheckEq(decoded, LinkedSectionBase(target->header, reloc.section) + reloc.addend,
                label + ": addressed pair to module " + std::to_string(list.id) + " resolves");
        Check(target->Contains(decoded),
              label + ": addressed pair to module " + std::to_string(list.id) + " lands inside it");
        break;
      }
      case kRelocAddr16Lo:
        break; // checked through its high half
      case kRelocRel24: {
        const uint32_t word = Guest32(module.Image(), site);
        CheckEq(word >> 26, 18, label + ": REL24 patches a branch");
        const int32_t displacement = SignExtend(word & 0x03fffffcu, 26);
        const auto destination = static_cast<uint32_t>(base + site + displacement);
        CheckEq(destination, LinkedSectionBase(target->header, reloc.section) + reloc.addend,
                label + ": REL24 to module " + std::to_string(list.id) + " resolves");
        Check(target->Contains(destination),
              label + ": REL24 to module " + std::to_string(list.id) + " lands inside it");
        break;
      }
      case kRelocRel14:
      case kRelocRel14Brtaken:
      case kRelocRel14Brntaken: {
        const uint32_t word = Guest32(module.Image(), site);
        CheckEq(word >> 26, 16, label + ": REL14 patches a conditional branch");
        const int32_t displacement = SignExtend(word & 0x0000fffcu, 16);
        const auto destination = static_cast<uint32_t>(base + site + displacement);
        CheckEq(destination, LinkedSectionBase(target->header, reloc.section) + reloc.addend,
                label + ": REL14 to module " + std::to_string(list.id) + " resolves");
        Check(target->Contains(destination),
              label + ": REL14 to module " + std::to_string(list.id) + " lands inside it");
        break;
      }
      default:
        // Implemented by the linker, but no module on hand uses it: count it
        // rather than pretending to check it.
        g_unexercised++;
        break;
      }
      g_checks++;
    }
  }
}

// Loads one module, checks it, and reports its extent for the modules that
// import it.
bool LoadAndValidate(const std::string& path, GuestArena& arena,
                     const std::map<uint32_t, Extent>& loaded, const std::set<uint32_t>* symbols,
                     bool quiet, Extent* out = nullptr) {
  const std::string label = std::filesystem::path(path).filename().string();
  std::vector<uint8_t> file;
  if (!ReadFile(path, file)) {
    Fail("cannot read " + path);
    return false;
  }
  ImageInfo info;
  if (!Probe(file.data(), file.size(), info)) {
    Fail(label + ": does not probe as a module");
    return false;
  }

  const auto module = Load(file.data(), file.size(), arena);
  if (!module.Linked()) {
    Fail(label + ": does not link");
    return false;
  }
  g_modules++;

  ValidateHeader(file, info, module, label);
  ValidateRelocations(file, info, module,
                      ReadImports(file, SectionOffsets(file, info), BE32(&file[0x28]),
                                  BE32(&file[0x2C])),
                      loaded, symbols, label);

  if (out != nullptr) {
    out->id = info.moduleId;
    out->base = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(module.Image()));
    out->imageSize = static_cast<uint32_t>(info.imageSize);
    out->bss = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(module.Bss()));
    out->bssSize = info.bssSize;
    out->header = module.Header();
  }
  if (!quiet) {
    std::printf("%-28s id=%2u sections=%2u bss=%#-6x imports=%zu\n", label.c_str(), info.moduleId,
                info.numSections, info.bssSize, ReadImports(file, SectionOffsets(file, info),
                                                           BE32(&file[0x28]), BE32(&file[0x2C]))
                                                .size());
  }
  return true;
}

// --- directory mode ---------------------------------------------------------

struct Pending {
  std::string path;
  uint32_t id = 0;
  std::vector<uint32_t> dependencies;
};

int RunDirectory(const std::string& directory, const std::set<uint32_t>* symbols) {
  std::vector<std::string> paths;
  std::error_code error;
  for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
    if (entry.is_regular_file(error) && entry.path().extension() == ".rel") {
      paths.push_back(entry.path().string());
    }
  }
  std::sort(paths.begin(), paths.end());
  if (paths.empty()) {
    std::printf("SKIP: no .rel files in %s\n", directory.c_str());
    return 77;
  }

  // Read every header first so modules can be linked dependency-first.
  std::vector<Pending> pending;
  for (const std::string& path : paths) {
    std::vector<uint8_t> file;
    if (!ReadFile(path, file)) {
      Fail("cannot read " + path);
      continue;
    }
    ImageInfo info;
    if (!Probe(file.data(), file.size(), info)) {
      Fail(std::filesystem::path(path).filename().string() + ": does not probe as a module");
      continue;
    }
    Pending entry;
    entry.path = path;
    entry.id = info.moduleId;
    for (const ImportList& list :
         ReadImports(file, SectionOffsets(file, info), BE32(&file[0x28]), BE32(&file[0x2C]))) {
      if (list.id != info.moduleId && list.id != 0) {
        entry.dependencies.push_back(list.id);
      }
    }
    for (const Pending& other : pending) {
      Check(other.id != entry.id, std::string("duplicate module id ") + std::to_string(entry.id));
    }
    pending.push_back(std::move(entry));
  }

  GuestArena arena(32u << 20, 0x81000000);
  if (!arena.Valid()) {
    Fail("arena is 32-bit addressable");
    return 1;
  }

  std::map<uint32_t, Extent> loaded;
  while (!pending.empty()) {
    bool progress = false;
    for (auto it = pending.begin(); it != pending.end();) {
      const bool ready = std::all_of(it->dependencies.begin(), it->dependencies.end(),
                                     [&](uint32_t id) { return loaded.count(id) != 0; });
      if (!ready) {
        ++it;
        continue;
      }
      Extent extent;
      if (LoadAndValidate(it->path, arena, loaded, symbols, false, &extent)) {
        loaded.emplace(extent.id, extent);
      }
      it = pending.erase(it);
      progress = true;
    }
    if (!progress) {
      std::printf("note: %zu modules have dependencies outside the set\n", pending.size());
      for (const Pending& entry : pending) {
        std::printf("  %s needs", std::filesystem::path(entry.path).filename().c_str());
        for (uint32_t id : entry.dependencies) {
          std::printf(" %u%s", id, loaded.count(id) == 0 ? "(absent)" : "");
        }
        std::printf("\n");
      }
      break;
    }
  }

  // Unloading has to walk every remaining module's import list to undo the
  // references it owned, so this exercises it over the whole set.
  size_t unlinked = 0;
  while (ModuleCount() != 0) {
    if (!UnlinkModule(ModuleAt(ModuleCount() - 1))) {
      Fail("unlink of module " + std::to_string(ModuleAt(ModuleCount() - 1)->id) + " failed");
      break;
    }
    unlinked++;
  }
  CheckEq(unlinked, static_cast<size_t>(g_modules), "every loaded module unlinks");
  CheckEq(ModuleCount(), 0, "the registry empties");

  std::printf("loaded %d modules, %zu relocation checks, %zu DOL imports, %zu unresolved, "
              "%zu unexercised, %zu unlinked\n",
              g_modules, g_checks, g_dolImports, g_unresolved, g_unexercised, unlinked);
  return g_failures == 0 ? 0 : 1;
}
} // namespace

int main(int argc, char** argv) {
  std::set<uint32_t> symbolSet;
  const std::set<uint32_t>* symbols = nullptr;
  const auto useSymbols = [&](const char* path) {
    symbolSet = ReadSymbols(path);
    if (symbolSet.empty()) {
      std::fprintf(stderr, "note: no DOL symbols read from %s\n", path);
      return;
    }
    symbols = &symbolSet;
  };

  if (argc >= 2 && std::strcmp(argv[1], "--dir") == 0) {
    if (argc < 3) {
      std::printf("SKIP: --dir needs a directory\n");
      return 77;
    }
    if (argc >= 4 && argv[3][0] != '\0') {
      useSymbols(argv[3]);
    }
    return RunDirectory(argv[2], symbols);
  }

  if (argc < 2 || argv[1][0] == '\0') {
    std::printf("SKIP: no REL module given\n");
    return 77;
  }
  if (argc >= 3 && argv[2][0] != '\0') {
    useSymbols(argv[2]);
  }

  GuestArena arena(16u << 20, 0x81000000);
  if (!arena.Valid()) {
    Fail("arena is 32-bit addressable");
    return 1;
  }
  std::map<uint32_t, Extent> loaded;
  if (!LoadAndValidate(argv[1], arena, loaded, symbols, false)) {
    return 1;
  }
  std::printf("%d module, %zu relocation checks, %zu DOL imports, %zu unresolved (not loaded)\n",
              g_modules, g_checks, g_dolImports, g_unresolved);
  return g_failures == 0 ? 0 : 1;
}
