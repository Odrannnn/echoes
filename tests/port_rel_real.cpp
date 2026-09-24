// Real REL integration check.
//
// Loads a REL module extracted from a disc the user owns and verifies the
// loader against it: the header and section layout, a full walk of both
// relocation lists, and - the point of the exercise - that every relocation
// against the main executable decodes back to the address it claims.
//
// Usage: port_rel_real_tests <module.rel> [dol-symbols.txt]
// Exits 77 (skipped) when no module is given, so a checkout without a disc
// still passes.

#include "port_rel.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>


namespace {
using namespace port::rel;
int g_failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    g_failures++;
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

bool ReadFile(const char* path, std::vector<uint8_t>& out) {
  std::FILE* file = std::fopen(path, "rb");
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

uint32_t BE32(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

uint16_t BE16(const uint8_t* p) {
  return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

// Section payloads stay in guest byte order, so read them big-endian.
uint32_t Host32(const void* base, size_t offset) {
  const auto* p = static_cast<const uint8_t*>(base) + offset;
  return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

uint16_t Host16(const void* base, size_t offset) {
  const auto* p = static_cast<const uint8_t*>(base) + offset;
  return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

struct Reloc {
  size_t imageOffset;
  uint8_t type;
  uint8_t section;
  uint32_t addend;
};

// Independent reader for an on-disc relocation list. Deliberately does not share
// code with the loader: its job is to disagree if the loader is wrong.
std::vector<Reloc> ReadRelocations(const std::vector<uint8_t>& file,
                                   const std::vector<uint32_t>& sectionOffsets,
                                   uint32_t relocStart, std::vector<uint8_t>& types) {
  std::vector<Reloc> result;
  size_t p = 0;
  bool haveSection = false;
  size_t cursor = relocStart;
  while (cursor + 8 <= file.size()) {
    const uint16_t delta = BE16(&file[cursor]);
    const uint8_t type = file[cursor + 2];
    const uint8_t section = file[cursor + 3];
    const uint32_t addend = BE32(&file[cursor + 4]);
    cursor += 8;
    types.push_back(type);

    if (haveSection) {
      p += delta;
    }
    if (type == kRelocDolphinSection) {
      haveSection = true;
      p = sectionOffsets[section] & ~1u;
    } else if (type == kRelocDolphinEnd) {
      break;
    } else if (type == kRelocDolphinNop) {
      // Control record: it only advances the cursor.
    } else {
      result.push_back({p, type, section, addend});
    }
  }
  return result;
}

// Width of the field each relocation type patches.
int32_t SignExtend(uint32_t value, int bits) {
  const uint32_t signBit = 1u << (bits - 1);
  if ((value & signBit) != 0) {
    return static_cast<int32_t>(value | ~((1u << bits) - 1));
  }
  return static_cast<int32_t>(value);
}

std::set<uint32_t> ReadSymbols(const char* path) {
  std::set<uint32_t> symbols;
  std::FILE* file = std::fopen(path, "r");
  if (file == nullptr) {
    return symbols;
  }
  char line[512];
  while (std::fgets(line, sizeof(line), file) != nullptr) {
    const char* equals = std::strchr(line, '=');
    if (equals == nullptr) {
      continue;
    }
    const char* colon = std::strchr(equals, ':');
    if (colon == nullptr) {
      continue;
    }
    const char* end = std::strchr(colon, ';');
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
} // namespace

int main(int argc, char** argv) {
  if (argc < 2 || argv[1][0] == '\0') {
    std::printf("SKIP: no REL module given\n");
    return 77;
  }

  std::vector<uint8_t> file;
  if (!ReadFile(argv[1], file)) {
    std::fprintf(stderr, "FAIL: cannot read %s\n", argv[1]);
    return 1;
  }

  // The reference values below were established with an independent decoder
  // (tools/rel_decode.py) run over the same module.
  port::rel::ImageInfo info;
  Check(port::rel::Probe(file.data(), file.size(), info), "module probes");
  CheckEq(info.version, 2, "module version");
  CheckEq(info.numSections, 15, "section count");
  CheckEq(info.moduleId, 1, "module id");
  CheckEq(info.bssSize, 0x9e1c, "bssSize");
  CheckEq(info.align, 32, "align");
  CheckEq(info.bssAlign, 32, "bssAlign");
  CheckEq(info.imageSize, file.size(), "image size is the file size");

  const uint32_t rawSectionInfoOffset = BE32(&file[0x10]);
  const uint32_t rawImpOffset = BE32(&file[0x28]);
  const uint32_t rawImpSize = BE32(&file[0x2C]);
  std::vector<uint32_t> sectionOffsets(info.numSections);
  for (uint32_t i = 0; i < info.numSections; i++) {
    sectionOffsets[i] = BE32(&file[rawSectionInfoOffset + i * 8]);
  }

  GuestArena arena(4u << 20, 0x81000000);
  Check(arena.Valid(), "arena is 32-bit addressable");
  auto module = port::rel::Load(file.data(), file.size(), arena);
  Check(module.Linked(), "module links");
  if (!module.Linked()) {
    std::printf("%d checks failed\n", g_failures);
    return 1;
  }

  const uintptr_t base = reinterpret_cast<uintptr_t>(module.Header());
  const uintptr_t bss = reinterpret_cast<uintptr_t>(module.Bss());

  // Section layout after linking: file offsets plus the image base.
  CheckEq(module.Header()->sectionInfoOffset, base + rawSectionInfoOffset,
          "section table relocated");
  CheckEq(module.Header()->impOffset, base + rawImpOffset, "import table relocated");
  CheckEq(SectionTable(module.Header())[1].offset, base + sectionOffsets[1],
          "code section base keeps its code flag");
  CheckEq(SectionTable(module.Header())[5].offset, base + sectionOffsets[5], "data section base");
  CheckEq(SectionTable(module.Header())[6].offset, bss, "bss section base");
  CheckEq(module.Header()->bssSection, 6, "bssSection index");
  CheckEq(module.Header()->prolog, base + (sectionOffsets[1] & ~1u) + 0x00, "prolog entry");
  CheckEq(module.Header()->epilog, base + (sectionOffsets[1] & ~1u) + 0x50, "epilog entry");
  CheckEq(module.Header()->unresolved, base + (sectionOffsets[1] & ~1u) + 0x9c, "unresolved entry");

  // Walk both import lists with the independent reader.
  const uint32_t importCount = rawImpSize / sizeof(RelImportInfo);
  CheckEq(importCount, 2, "import entry count");
  std::vector<Reloc> dolRelocs;
  std::vector<Reloc> selfRelocs;
  std::vector<uint8_t> dolTypes;
  std::vector<uint8_t> selfTypes;
  for (uint32_t i = 0; i < importCount; i++) {
    const uint32_t id = BE32(&file[rawImpOffset + i * 8]);
    const uint32_t relocStart = BE32(&file[rawImpOffset + i * 8 + 4]);
    if (id == 0) {
      dolRelocs = ReadRelocations(file, sectionOffsets, relocStart, dolTypes);
    } else if (id == info.moduleId) {
      selfRelocs = ReadRelocations(file, sectionOffsets, relocStart, selfTypes);
    } else {
      Check(false, "unexpected import module id");
    }
  }

  // Counts by type, taken from the independent decoder.
  auto countOf = [](const std::vector<uint8_t>& types, uint8_t type) {
    size_t n = 0;
    for (uint8_t t : types) {
      if (t == type) {
        n++;
      }
    }
    return n;
  };
  CheckEq(countOf(dolTypes, kRelocRel24), 409, "main-module REL24 count");
  CheckEq(countOf(dolTypes, kRelocAddr16Ha), 1, "main-module ADDR16_HA count");
  CheckEq(countOf(dolTypes, kRelocAddr16Lo), 1, "main-module ADDR16_LO count");
  CheckEq(countOf(dolTypes, kRelocDolphinEnd), 1, "main-module END count");
  CheckEq(dolTypes.size(), 413, "main-module record count");
  CheckEq(countOf(selfTypes, kRelocAddr32), 495, "self-relocation ADDR32 count");
  CheckEq(countOf(selfTypes, kRelocAddr16Lo), 282, "self-relocation ADDR16_LO count");
  CheckEq(countOf(selfTypes, kRelocAddr16Ha), 278, "self-relocation ADDR16_HA count");
  CheckEq(countOf(selfTypes, kRelocAddr16Hi), 4, "self-relocation ADDR16_HI count");
  CheckEq(countOf(selfTypes, kRelocRel14), 4, "self-relocation REL14 count");
  CheckEq(countOf(selfTypes, kRelocDolphinSection), 2, "self-relocation SECTION count");
  CheckEq(countOf(selfTypes, kRelocDolphinNop), 1, "self-relocation NOP count");
  CheckEq(selfTypes.size(), 1067, "self-relocation record count");

  // Every relocation type the module uses must be one the linker implements.
  auto supported = [](uint8_t type) {
    switch (type) {
    case kRelocNone:
    case kRelocAddr32:
    case kRelocAddr24:
    case kRelocAddr16:
    case kRelocAddr16Lo:
    case kRelocAddr16Hi:
    case kRelocAddr16Ha:
    case kRelocAddr14:
    case kRelocAddr14Brtaken:
    case kRelocAddr14Brntaken:
    case kRelocRel24:
    case kRelocRel14:
    case kRelocRel14Brtaken:
    case kRelocRel14Brntaken:
    case kRelocDolphinNop:
    case kRelocDolphinSection:
    case kRelocDolphinEnd:
      return true;
    default:
      return false;
    }
  };
  for (uint8_t type : dolTypes) {
    Check(supported(type), "main-module relocation type is supported");
  }
  for (uint8_t type : selfTypes) {
    Check(supported(type), "self-relocation type is supported");
  }

  // Against the main executable: decode every patched instruction/value and
  // check that it points at the address the relocation asked for.
  size_t dolChecked = 0;
  for (const Reloc& reloc : dolRelocs) {
    const size_t site = reloc.imageOffset;
    switch (reloc.type) {
    case kRelocRel24: {
      const uint32_t word = Host32(module.Header(), site);
      CheckEq(word >> 26, 18, "REL24 patches a branch opcode");
      const int32_t displacement = SignExtend(word & 0x03fffffcu, 26);
      CheckEq(static_cast<uint32_t>(base + site + displacement), reloc.addend,
              "REL24 branch reaches the relocated address");
      dolChecked++;
      break;
    }
    case kRelocAddr32:
      CheckEq(Host32(module.Header(), site), reloc.addend, "ADDR32 against the main module");
      dolChecked++;
      break;
    case kRelocAddr16Ha: {
      const uint32_t expected = ((reloc.addend >> 16) + ((reloc.addend & 0x8000) ? 1 : 0)) & 0xffff;
      CheckEq(Host16(module.Header(), site), expected, "ADDR16_HA against the main module");
      CheckEq(Host32(module.Header(), site & ~3u) >> 26, 15, "ADDR16_HA patches an addis");
      dolChecked++;
      break;
    }
    case kRelocAddr16Lo: {
      CheckEq(Host16(module.Header(), site), reloc.addend & 0xffff, "ADDR16_LO against the main module");
      const uint32_t opcode = Host32(module.Header(), site & ~3u) >> 26;
      Check(opcode == 14 || opcode == 24, "ADDR16_LO patches an addi or ori");
      dolChecked++;
      break;
    }
    default:
      Check(false, "unexpected main-module relocation type");
      break;
    }
  }
  CheckEq(dolChecked, 411, "main-module relocations checked");

  // Against the module itself: ADDR32 values and REL14 branches must land inside
  // the module, and the two halves of an addressed pair (addis/addi or
  // addis/ori) must decode back to the section-relative address the relocation
  // asked for. The second check is a different path to the same number - it
  // reads the instruction immediates back - so it disagrees if a half was
  // written at the wrong place or with the wrong carry.
  size_t selfChecked = 0;
  std::vector<bool> paired(selfRelocs.size(), false);
  for (size_t i = 0; i < selfRelocs.size(); i++) {
    const Reloc& reloc = selfRelocs[i];
    const size_t site = reloc.imageOffset;
    switch (reloc.type) {
    case kRelocAddr32: {
      const uint32_t value = Host32(module.Header(), site);
      const bool inImage = value >= base && value < base + module.ImageSize();
      const bool inBss = value >= bss && value < bss + info.bssSize;
      Check(inImage || inBss, "self ADDR32 stays inside the module");
      selfChecked++;
      break;
    }
    case kRelocRel14: {
      const uint32_t word = Host32(module.Header(), site);
      CheckEq(word >> 26, 16, "REL14 patches a conditional branch");
      const int32_t displacement = SignExtend(word & 0x0000fffcu, 16);
      const auto target = static_cast<uint32_t>(base + site + displacement);
      const bool inImage = target >= base && target < base + module.ImageSize();
      const bool inBss = target >= bss && target < bss + info.bssSize;
      Check(inImage || inBss, "self REL14 reaches inside the module");
      selfChecked++;
      break;
    }
    case kRelocAddr16Ha:
    case kRelocAddr16Hi: {
      CheckEq(Host32(module.Header(), site & ~3u) >> 26, 15, "high half patches an addis");
      // The low half of the same address follows as its own record.
      size_t lowIndex = selfRelocs.size();
      for (size_t j = i + 1; j < selfRelocs.size(); j++) {
        if (paired[j]) {
          continue;
        }
        if (selfRelocs[j].type == kRelocAddr16Lo && selfRelocs[j].addend == reloc.addend &&
            selfRelocs[j].section == reloc.section) {
          lowIndex = j;
          break;
        }
      }
      if (lowIndex == selfRelocs.size()) {
        Check(false, "high half has a matching low half");
        break;
      }
      paired[lowIndex] = true;
      const uint16_t writtenHigh = Host16(module.Header(), site);
      const uint16_t writtenLow = Host16(module.Header(), selfRelocs[lowIndex].imageOffset);
      const uint32_t sectionBase =
          reloc.section == module.Header()->bssSection
              ? static_cast<uint32_t>(bss)
              : static_cast<uint32_t>(base + (sectionOffsets[reloc.section] & ~1u));
      const uint32_t expected = sectionBase + reloc.addend;
      const uint32_t decoded =
          reloc.type == kRelocAddr16Ha
              ? static_cast<uint32_t>((static_cast<int32_t>(SignExtend(writtenHigh, 16)) << 16) +
                                      SignExtend(writtenLow, 16))
              : static_cast<uint32_t>((static_cast<uint32_t>(writtenHigh) << 16) | writtenLow);
      CheckEq(decoded, expected, "addressed pair decodes to the module address");
      Check(decoded >= base && decoded < base + module.ImageSize() + info.bssSize,
            "decoded address is inside the module");
      selfChecked += 2;
      break;
    }
    case kRelocAddr16Lo:
      // Verified through its high half.
      Check(paired[i], "low half belongs to a pair");
      break;
    default:
      std::fprintf(stderr, "  unexpected self-relocation type %u at %#zx\n", reloc.type, site);
      Check(false, "unexpected self-relocation type");
      break;
    }
  }
  CheckEq(selfChecked, 1063, "self-relocations checked");

  // Every address the module asks the main executable for must be a real symbol
  // in the DOL it was linked against.
  if (argc >= 3 && argv[2][0] != '\0') {
    const std::set<uint32_t> symbols = ReadSymbols(argv[2]);
    CheckEq(symbols.size() > 10000, true, "symbol list loaded");
    size_t missing = 0;
    for (const Reloc& reloc : dolRelocs) {
      if (symbols.find(reloc.addend) == symbols.end()) {
        missing++;
        if (missing <= 5) {
          std::fprintf(stderr, "  unresolved main-module address 0x%08x\n", reloc.addend);
        }
      }
    }
    CheckEq(missing, 0, "every main-module relocation names a known symbol");
  } else {
    std::printf("note: no symbol list given; skipped the symbol cross-check\n");
  }

  Check(module.Unlink(), "module unlinks");
  if (g_failures != 0) {
    std::printf("%d checks failed\n", g_failures);
    return 1;
  }
  std::printf("port_rel_real_tests: all checks passed\n");
  return 0;
}
