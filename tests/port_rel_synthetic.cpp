// Synthetic REL modules: every relocation type the linker implements, the
// section/bss layout, the module list and unlink, and the loader's handling of
// the on-disc big-endian layout. Built by hand so the expected words are known
// exactly; the real-disc check lives in port_rel_real.cpp.

#include "port_rel.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>


namespace {
using namespace port::rel;
int g_failures = 0;

void Check(bool condition, const char* what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    g_failures++;
  }
}

void CheckEq(uint32_t actual, uint32_t expected, const char* what) {
  if (actual != expected) {
    std::fprintf(stderr, "FAIL: %s: got 0x%08x expected 0x%08x\n", what, actual, expected);
    g_failures++;
  }
}

void PutBE32(std::vector<uint8_t>& image, size_t offset, uint32_t value) {
  image[offset + 0] = static_cast<uint8_t>(value >> 24);
  image[offset + 1] = static_cast<uint8_t>(value >> 16);
  image[offset + 2] = static_cast<uint8_t>(value >> 8);
  image[offset + 3] = static_cast<uint8_t>(value);
}

void PutBE16(std::vector<uint8_t>& image, size_t offset, uint16_t value) {
  image[offset + 0] = static_cast<uint8_t>(value >> 8);
  image[offset + 1] = static_cast<uint8_t>(value);
}

// Section payloads stay in guest byte order, so read them big-endian.
uint32_t Get32(const void* base, size_t offset) {
  const auto* p = static_cast<const uint8_t*>(base) + offset;
  return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

uint16_t Get16(const void* base, size_t offset) {
  const auto* p = static_cast<const uint8_t*>(base) + offset;
  return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

uint32_t HiAdj(uint32_t value) {
  return ((value >> 16) + ((value & 0x8000) ? 1 : 0)) & 0xffff;
}

// A relocation record in the on-disc (big-endian) layout.
void PutReloc(std::vector<uint8_t>& image, size_t offset, uint16_t delta, uint8_t type,
              uint8_t section, uint32_t addend) {
  PutBE16(image, offset, delta);
  image[offset + 2] = type;
  image[offset + 3] = section;
  PutBE32(image, offset + 4, addend);
}

constexpr uint8_t kSection = 202; // kRelocDolphinSection
constexpr uint8_t kEnd = 203;     // kRelocDolphinEnd
constexpr uint8_t kNop = 201;     // kRelocDolphinNop
constexpr uint8_t kAddr32 = 1;
constexpr uint8_t kAddr24 = 2;
constexpr uint8_t kAddr16Hi = 5;
constexpr uint8_t kAddr16Lo = 4;
constexpr uint8_t kAddr16Ha = 6;
constexpr uint8_t kAddr14 = 7;
constexpr uint8_t kRel24 = 10;
constexpr uint8_t kRel14 = 11;

// Module 7: 4 sections (header, code, data, bss) and both import lists.
std::vector<uint8_t> BuildMainModule() {
  std::vector<uint8_t> image(0x1D0, 0);

  PutBE32(image, 0x00, 7);          // info.id
  PutBE32(image, 0x0C, 4);          // info.numSections
  PutBE32(image, 0x10, 0x48);       // info.sectionInfoOffset
  PutBE32(image, 0x1C, 2);          // info.version
  PutBE32(image, 0x20, 0x20);       // bssSize
  PutBE32(image, 0x24, 0x00);       // relOffset (unused by the linker)
  PutBE32(image, 0x28, 0x1C0);      // impOffset
  PutBE32(image, 0x2C, 16);         // impSize
  image[0x30] = 1;                  // prologSection
  image[0x31] = 1;                  // epilogSection
  image[0x32] = 1;                  // unresolvedSection
  image[0x33] = 0;                  // bssSection (set at link time)
  PutBE32(image, 0x34, 0x10);       // prolog
  PutBE32(image, 0x38, 0x20);       // epilog
  PutBE32(image, 0x3C, 0x30);       // unresolved
  PutBE32(image, 0x40, 32);         // align
  PutBE32(image, 0x44, 32);         // bssAlign

  PutBE32(image, 0x48 + 0 * 8, 0);      // section 0: the header itself
  PutBE32(image, 0x48 + 0 * 8 + 4, 0);
  PutBE32(image, 0x48 + 1 * 8, 0xC1);   // section 1: code, exec bit set
  PutBE32(image, 0x48 + 1 * 8 + 4, 0x40);
  PutBE32(image, 0x48 + 2 * 8, 0x100);  // section 2: data
  PutBE32(image, 0x48 + 2 * 8 + 4, 0x20);
  PutBE32(image, 0x48 + 3 * 8, 0);      // section 3: bss
  PutBE32(image, 0x48 + 3 * 8 + 4, 0x20);

  // Branch placeholders so the low bits of a patched word are observable.
  PutBE32(image, 0xC4, 0x48000001);
  PutBE32(image, 0xD0, 0x48000001);
  PutBE32(image, 0xD4, 0x48000001);

  // Import list for the main executable (id 0): absolute guest addresses.
  PutReloc(image, 0x140, 0, kSection, 1, 0);
  PutReloc(image, 0x148, 0, kAddr32, 0, 0x80003140);
  PutReloc(image, 0x150, 4, kRel24, 0, 0x8037FA2C);
  PutReloc(image, 0x158, 0, kEnd, 0, 0);

  // Self relocations (id == the module's own id), section-relative.
  PutReloc(image, 0x160, 0, kSection, 1, 0);
  PutReloc(image, 0x168, 8, kAddr32, 2, 4);
  PutReloc(image, 0x170, 4, kAddr16Ha, 2, 0x1234);
  PutReloc(image, 0x178, 2, kAddr16Lo, 2, 0x1234);
  PutReloc(image, 0x180, 2, kAddr24, 1, 0x30);
  PutReloc(image, 0x188, 4, kRel14, 2, 0x10);
  PutReloc(image, 0x190, 4, kAddr32, 3, 8); // target the bss section
  PutReloc(image, 0x198, 0, kNop, 0, 0);
  PutReloc(image, 0x1A0, 0, kEnd, 0, 0);

  PutBE32(image, 0x1C0, 0);      // import: id 0
  PutBE32(image, 0x1C4, 0x140);
  PutBE32(image, 0x1C8, 7);      // import: id 7 (itself)
  PutBE32(image, 0x1CC, 0x160);

  return image;
}

// Module 9: 2 sections, one import from module 7.
std::vector<uint8_t> BuildSecondModule() {
  std::vector<uint8_t> image(0x108, 0);

  PutBE32(image, 0x00, 9);
  PutBE32(image, 0x0C, 2);
  PutBE32(image, 0x10, 0x48);
  PutBE32(image, 0x1C, 2);
  PutBE32(image, 0x20, 0x00);   // no bss
  PutBE32(image, 0x24, 0x00);
  PutBE32(image, 0x28, 0x100);  // impOffset
  PutBE32(image, 0x2C, 8);
  image[0x30] = 1;
  image[0x31] = 1;
  image[0x32] = 1;
  PutBE32(image, 0x34, 0x04);
  PutBE32(image, 0x38, 0x08);
  PutBE32(image, 0x3C, 0x0C);
  PutBE32(image, 0x40, 32);
  PutBE32(image, 0x44, 32);

  PutBE32(image, 0x48 + 0 * 8, 0);
  PutBE32(image, 0x48 + 0 * 8 + 4, 0);
  PutBE32(image, 0x48 + 1 * 8, 0xC1);
  PutBE32(image, 0x48 + 1 * 8 + 4, 0x20);

  // A reference into section 2 of module 7 (its data section).
  PutReloc(image, 0xE0, 0, kSection, 1, 0);
  PutReloc(image, 0xE8, 0, kAddr32, 2, 8);
  PutReloc(image, 0xF0, 0, kEnd, 0, 0);

  PutBE32(image, 0x100, 7);     // import: id 7
  PutBE32(image, 0x104, 0xE0);

  return image;
}

void TestProbeRejectsMalformed() {
  port::rel::ImageInfo info;
  const auto valid = BuildMainModule();
  Check(port::rel::Probe(valid.data(), valid.size(), info), "valid module probes");
  CheckEq(info.version, 2, "probe version");
  CheckEq(info.numSections, 4, "probe sections");
  CheckEq(info.bssSize, 0x20, "probe bssSize");
  CheckEq(info.align, 32, "probe align");
  CheckEq(info.imageSize, valid.size(), "probe image size");

  Check(!port::rel::Probe(nullptr, 0, info), "null image rejected");
  Check(!port::rel::Probe(valid.data(), 0x20, info), "truncated header rejected");

  auto badVersion = valid;
  PutBE32(badVersion, 0x1C, 99);
  Check(!port::rel::Probe(badVersion.data(), badVersion.size(), info), "bad version rejected");

  auto noSections = valid;
  PutBE32(noSections, 0x0C, 0);
  Check(!port::rel::Probe(noSections.data(), noSections.size(), info), "zero sections rejected");

  auto badAlign = valid;
  PutBE32(badAlign, 0x40, 24);
  Check(!port::rel::Probe(badAlign.data(), badAlign.size(), info), "non-power-of-two align rejected");

  auto badImports = valid;
  PutBE32(badImports, 0x28, 0x100000);
  Check(!port::rel::Probe(badImports.data(), badImports.size(), info), "out-of-range imports rejected");

  auto badSections = valid;
  PutBE32(badSections, 0x0C, 64);
  Check(!port::rel::Probe(badSections.data(), badSections.size(), info),
        "section table outside the image rejected");
}

// A relocation list that never reaches R_DOLPHIN_END must be rejected rather
// than walked off the end of the arena.
void TestRejectsUnterminatedRelocations() {
  ResetModules();
  auto image = BuildMainModule();
  PutReloc(image, 0x158, 0, kNop, 0, 0); // main-executable list loses its END
  PutReloc(image, 0x1A0, 0, kNop, 0, 0); // self list loses its END

  GuestArena arena(1 << 16, 0x81000000);
  const auto module = port::rel::Load(image.data(), image.size(), arena);
  Check(!module.Linked(), "a module whose relocation list never ends is rejected");
  CheckEq(ModuleCount(), 0, "a rejected module is not registered");
}

void TestLinking() {
  ResetModules();
  CheckEq(ModuleCount(), 0, "registry starts empty");
  GuestArena arena(1 << 16, 0x81000000);
  Check(arena.Valid(), "arena is 32-bit addressable");
  Check(reinterpret_cast<uintptr_t>(arena.Base()) + arena.Size() <= 0x100000000ull,
        "arena lives below 4 GiB");

  // Fill the arena so bss zeroing is observable.
  std::memset(arena.Base(), 0xAA, arena.Size());

  const auto mainImage = BuildMainModule();
  auto module = port::rel::Load(mainImage.data(), mainImage.size(), arena);
  Check(module.Linked(), "module 7 links");
  if (!module.Linked()) {
    return;
  }

  CheckEq(ModuleCount(), 1, "module 7 is registered");
  Check(ModuleAt(0) == module.Header(), "module 7 is the first registry entry");

  const uintptr_t base = reinterpret_cast<uintptr_t>(module.Header());
  const uintptr_t code = base + 0xC0;
  const uintptr_t data = base + 0x100;
  const uintptr_t bss = reinterpret_cast<uintptr_t>(module.Bss());
  Check(base + module.ImageSize() <= reinterpret_cast<uintptr_t>(arena.Base()) + arena.Size(),
        "image fits the arena");

  // The loader's own view of the header matches the on-disc values.
  CheckEq(module.Header()->id, 7, "module id");
  CheckEq(module.Header()->version, 2, "header version");
  CheckEq(module.Header()->bssSize, 0x20, "header bssSize");

  // Linking turns file offsets into guest addresses.
  CheckEq(module.Header()->sectionInfoOffset, static_cast<uint32_t>(base + 0x48),
          "sectionInfoOffset relocated");
  CheckEq(module.Header()->impOffset, static_cast<uint32_t>(base + 0x1C0), "impOffset relocated");
  CheckEq(module.Header()->relOffset, static_cast<uint32_t>(base), "relOffset relocated");
  CheckEq(SectionTable(module.Header())[1].offset, static_cast<uint32_t>(code | 1),
          "code section keeps its exec bit");
  CheckEq(SectionTable(module.Header())[2].offset, static_cast<uint32_t>(data),
          "data section base");
  CheckEq(SectionTable(module.Header())[3].offset, static_cast<uint32_t>(bss), "bss section base");
  CheckEq(module.Header()->bssSection, 3, "bssSection points at the bss section");
  CheckEq(module.Header()->prolog, static_cast<uint32_t>(code + 0x10), "prolog entry");
  CheckEq(module.Header()->epilog, static_cast<uint32_t>(code + 0x20), "epilog entry");
  CheckEq(module.Header()->unresolved, static_cast<uint32_t>(code + 0x30), "unresolved entry");
  CheckEq(module.Prolog(), static_cast<uint32_t>(code + 0x10), "Module::Prolog");
  CheckEq(module.Epilog(), static_cast<uint32_t>(code + 0x20), "Module::Epilog");
  CheckEq(module.Unresolved(), static_cast<uint32_t>(code + 0x30), "Module::Unresolved");

  // Relocations against the main executable are absolute guest addresses.
  CheckEq(Get32(module.Header(), 0xC0), 0x80003140, "ADDR32 against the main module");
  const uint32_t rel24 = (0x48000001u & ~0x03fffffcu) |
                         ((0x8037FA2Cu - static_cast<uint32_t>(code + 4)) & 0x03fffffcu);
  CheckEq(Get32(module.Header(), 0xC4), rel24, "REL24 against the main module");

  // Self relocations are section-relative.
  CheckEq(Get32(module.Header(), 0xC8), static_cast<uint32_t>(data) + 4, "ADDR32 into data");
  const uint32_t haTarget = static_cast<uint32_t>(data) + 0x1234;
  CheckEq(Get16(module.Header(), 0xCC), HiAdj(haTarget), "ADDR16_HA into data");
  CheckEq(Get16(module.Header(), 0xCE), haTarget & 0xffff, "ADDR16_LO into data");
  const uint32_t addr24 = (0x48000001u & ~0x03fffffcu) |
                          ((static_cast<uint32_t>(code) + 0x30) & 0x03fffffcu);
  CheckEq(Get32(module.Header(), 0xD0), addr24, "ADDR24 within the code section");
  const uint32_t rel14 = (0x48000001u & ~0x0000fffcu) |
                         (((static_cast<uint32_t>(data) + 0x10) - static_cast<uint32_t>(code + 0x14)) &
                          0x0000fffcu);
  CheckEq(Get32(module.Header(), 0xD4), rel14, "REL14 into data");
  CheckEq(Get32(module.Header(), 0xD8), static_cast<uint32_t>(bss) + 8, "ADDR32 targeting bss");

  // OSLink zeroes the bss after relocating.
  bool bssZeroed = true;
  for (size_t i = 0; i < 0x20; i++) {
    bssZeroed &= *reinterpret_cast<const uint8_t*>(bss + i) == 0;
  }
  Check(bssZeroed, "bss is zeroed");

  // Pointer search, the way the game's debugger finds an owning module.
  uint32_t section = 99;
  uint32_t offset = 99;
  RelModuleHeader* found = SearchModule(reinterpret_cast<void*>(code + 0x10), &section, &offset);
  Check(found == module.Header(), "OSSearchModule finds the module");
  CheckEq(section, 1, "OSSearchModule section");
  CheckEq(offset, 0x10, "OSSearchModule offset");
  Check(SearchModule(reinterpret_cast<void*>(bss + 0x4), &section, &offset) != nullptr,
        "OSSearchModule finds the bss section");
  Check(SearchModule(&arena, nullptr, nullptr) == nullptr, "OSSearchModule ignores foreign pointers");

  // A second module that imports from the first: its relocations resolve
  // against module 7's section table.
  const auto secondImage = BuildSecondModule();
  auto second = port::rel::Load(secondImage.data(), secondImage.size(), arena);
  Check(second.Linked(), "module 9 links");
  if (second.Linked()) {
    CheckEq(ModuleCount(), 2, "module 9 is registered after module 7");
    Check(ModuleAt(1) == second.Header(), "module 9 is the second registry entry");
    CheckEq(Get32(second.Header(), 0xC0), static_cast<uint32_t>(data) + 8,
            "import from module 7 resolves through its section table");

    // Unlinking module 7 undoes the relocation it owned in module 9.
    Check(module.Unlink(), "module 7 unlinks");
    CheckEq(ModuleCount(), 1, "module 7 leaves the registry");
    Check(ModuleAt(0) == second.Header(), "module 9 remains registered");
    CheckEq(Get32(second.Header(), 0xC0), 0, "unlink undoes the other module's relocation");
    Check(second.Header()->sectionInfoOffset != 0, "module 9 survives module 7's unlink");
    Check(second.Unlink(), "module 9 unlinks");
    CheckEq(ModuleCount(), 0, "registry is empty again");
  }

  // Unlink restores the module's own relative layout.
  CheckEq(module.Header()->sectionInfoOffset, 0x48, "sectionInfoOffset restored");
  CheckEq(module.Header()->impOffset, 0x1C0, "impOffset restored");
  CheckEq(module.Header()->relOffset, 0x00, "relOffset restored");
  // Unlinking makes the offsets image-relative again, so read the table the way
  // the loader does rather than through SectionTable (which expects a linked
  // module).
  const auto* restored = reinterpret_cast<const RelSectionInfo*>(
      static_cast<const uint8_t*>(module.Image()) + module.Header()->sectionInfoOffset);
  CheckEq(restored[1].offset, 0xC1, "code section restored");
  CheckEq(restored[3].offset, 0, "bss section cleared");
  CheckEq(module.Header()->bssSection, 0, "bssSection cleared");
  CheckEq(module.Header()->prolog, 0x10, "prolog restored");
  Check(SearchModule(reinterpret_cast<void*>(code + 0x10), nullptr, nullptr) == nullptr,
        "unlinked module is out of the list");
}
} // namespace

int main() {
  TestProbeRejectsMalformed();
  TestRejectsUnterminatedRelocations();
  TestLinking();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d checks failed\n", g_failures);
    return 1;
  }
  std::printf("port_rel_tests: all checks passed\n");
  return 0;
}
