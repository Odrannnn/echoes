#include "port_rel.h"

#include <cstring>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace port {
namespace rel {
namespace {

constexpr size_t kDefaultAlignment = 32;
constexpr uint32_t kMaxSections = 64;
constexpr uint32_t kMaxBssSize = 64u << 20;

bool IsPowerOfTwo(uint32_t value) {
  return value != 0 && (value & (value - 1)) == 0;
}

size_t EffectiveAlignment(uint32_t value) {
  return value != 0 ? value : kDefaultAlignment;
}

uint32_t ReadBE32(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

uint16_t ReadBE16(const uint8_t* p) {
  return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

void SwapBE32(uint8_t* p) {
  const uint32_t value = ReadBE32(p);
  std::memcpy(p, &value, sizeof(value));
}

void SwapBE16(uint8_t* p) {
  const uint16_t value = ReadBE16(p);
  std::memcpy(p, &value, sizeof(value));
}

// Module images live in a 32-bit arena, so a guest address is a host address
// inside it. Everything the linker computes stays in guest addresses; this is
// the only place they become pointers.
inline uint32_t GuestAddress(const void* hostPointer) {
  return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hostPointer));
}

inline uint32_t* GuestPointer(uint32_t guestAddress) {
  return reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(guestAddress));
}

// Relocation results are written into a guest image, so they are stored in guest
// (big-endian) byte order: section payloads are left exactly as the console
// would leave them, and only the module's metadata is converted for the host.
inline uint32_t LoadGuest32(const uint32_t* p) {
  return ReadBE32(reinterpret_cast<const uint8_t*>(p));
}

inline void StoreGuest32(uint32_t* p, uint32_t value) {
  const uint8_t bytes[4] = {static_cast<uint8_t>(value >> 24), static_cast<uint8_t>(value >> 16),
                            static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
  std::memcpy(p, bytes, sizeof(bytes));
}

inline uint16_t LoadGuest16(const uint16_t* p) {
  return ReadBE16(reinterpret_cast<const uint8_t*>(p));
}

inline void StoreGuest16(uint16_t* p, uint16_t value) {
  const uint8_t bytes[2] = {static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
  std::memcpy(p, bytes, sizeof(bytes));
}

// Modules in load order. The console keeps an intrusive list inside the module
// image; the port keeps it here, which also keeps the order the console's
// Relocate sweeps depend on.
std::vector<RelModuleHeader*> s_modules;

size_t FindModuleIndex(const RelModuleHeader* header) {
  for (size_t i = 0; i < s_modules.size(); i++) {
    if (s_modules[i] == header) {
      return i;
    }
  }
  return s_modules.size();
}

// Applies the relocation list `module` holds for `newModule`, using
// `newModule`'s section table for the target section bases. A null `newModule`
// means the main executable (module id 0), whose relocations are absolute guest
// addresses and use no section base.
bool Relocate(RelModuleHeader* newModule, RelModuleHeader* module) {
  const uint32_t idNew = newModule != nullptr ? newModule->id : 0;
  const auto* imports = reinterpret_cast<const RelImportInfo*>(GuestPointer(module->impOffset));
  const auto importCount = module->impSize / sizeof(RelImportInfo);
  const RelImportInfo* imp = nullptr;
  for (uint32_t i = 0; i < importCount; i++) {
    if (imports[i].id == idNew) {
      imp = &imports[i];
      break;
    }
  }
  if (imp == nullptr) {
    return false;
  }

  // `p` advances by each record's delta; the first record of a list is always
  // R_DOLPHIN_SECTION, which replaces it with a section base. Until then there is
  // no target, so records are only positioned, never written: a malformed list
  // would otherwise write near address zero.
  uint32_t* p = nullptr;
  bool positioned = false;
  for (const auto* rel = reinterpret_cast<const RelReloc*>(GuestPointer(imp->offset));
       rel->type != kRelocDolphinEnd; rel++) {
    p = GuestPointer(GuestAddress(p) + rel->offset);
    if (!positioned && rel->type != kRelocDolphinSection) {
      continue;
    }
    uint32_t offset = 0;
    if (idNew != 0) {
      offset = kSectionInfoOffset(SectionTable(newModule)[rel->section].offset);
    }

    uint32_t x = 0;
    switch (rel->type) {
    case kRelocNone:
      break;
    case kRelocAddr32:
      StoreGuest32(p, offset + rel->addend);
      break;
    case kRelocAddr24:
      x = offset + rel->addend;
      StoreGuest32(p, (LoadGuest32(p) & ~0x03fffffcu) | (x & 0x03fffffcu));
      break;
    case kRelocAddr16:
    case kRelocAddr16Lo:
      x = offset + rel->addend;
      StoreGuest16(reinterpret_cast<uint16_t*>(p), static_cast<uint16_t>(x & 0xffff));
      break;
    case kRelocAddr16Hi:
      x = offset + rel->addend;
      StoreGuest16(reinterpret_cast<uint16_t*>(p), static_cast<uint16_t>((x >> 16) & 0xffff));
      break;
    case kRelocAddr16Ha:
      x = offset + rel->addend;
      StoreGuest16(reinterpret_cast<uint16_t*>(p),
                   static_cast<uint16_t>(((x >> 16) + ((x & 0x8000) ? 1 : 0)) & 0xffff));
      break;
    case kRelocAddr14:
    case kRelocAddr14Brtaken:
    case kRelocAddr14Brntaken:
      x = offset + rel->addend;
      StoreGuest32(p, (LoadGuest32(p) & ~0x0000fffcu) | (x & 0x0000fffcu));
      break;
    case kRelocRel24:
      x = offset + rel->addend - GuestAddress(p);
      StoreGuest32(p, (LoadGuest32(p) & ~0x03fffffcu) | (x & 0x03fffffcu));
      break;
    case kRelocRel14:
    case kRelocRel14Brtaken:
    case kRelocRel14Brntaken:
      x = offset + rel->addend - GuestAddress(p);
      StoreGuest32(p, (LoadGuest32(p) & ~0x0000fffcu) | (x & 0x0000fffcu));
      break;
    case kRelocDolphinNop:
      break;
    case kRelocDolphinSection:
      p = GuestPointer(kSectionInfoOffset(SectionTable(module)[rel->section].offset));
      positioned = true;
      break;
    default:
      // The console reports and skips anything else.
      break;
    }
  }

  return true;
}

// Reverses what Relocate() did, so an unlinked module leaves no dangling guest
// addresses behind in the modules that imported from it.
bool Undo(RelModuleHeader* newModule, RelModuleHeader* module) {
  const uint32_t idNew = newModule->id;
  const auto* imports = reinterpret_cast<const RelImportInfo*>(GuestPointer(module->impOffset));
  const auto importCount = module->impSize / sizeof(RelImportInfo);
  const RelImportInfo* imp = nullptr;
  for (uint32_t i = 0; i < importCount; i++) {
    if (imports[i].id == idNew) {
      imp = &imports[i];
      break;
    }
  }
  if (imp == nullptr) {
    return false;
  }

  uint32_t* p = nullptr;
  bool positioned = false;
  for (const auto* rel = reinterpret_cast<const RelReloc*>(GuestPointer(imp->offset));
       rel->type != kRelocDolphinEnd; rel++) {
    p = GuestPointer(GuestAddress(p) + rel->offset);
    if (!positioned && rel->type != kRelocDolphinSection) {
      continue;
    }

    uint32_t x = 0;
    switch (rel->type) {
    case kRelocNone:
      break;
    case kRelocAddr32:
      StoreGuest32(p, x);
      break;
    case kRelocAddr24:
      StoreGuest32(p, (LoadGuest32(p) & ~0x03fffffcu) | (x & 0x03fffffcu));
      break;
    case kRelocAddr16:
    case kRelocAddr16Lo:
      StoreGuest16(reinterpret_cast<uint16_t*>(p), static_cast<uint16_t>(x & 0xffff));
      break;
    case kRelocAddr16Hi:
      StoreGuest16(reinterpret_cast<uint16_t*>(p), static_cast<uint16_t>((x >> 16) & 0xffff));
      break;
    case kRelocAddr16Ha:
      StoreGuest16(reinterpret_cast<uint16_t*>(p),
                   static_cast<uint16_t>(((x >> 16) + ((x & 0x8000) ? 1 : 0)) & 0xffff));
      break;
    case kRelocAddr14:
    case kRelocAddr14Brtaken:
    case kRelocAddr14Brntaken:
      StoreGuest32(p, (LoadGuest32(p) & ~0x0000fffcu) | (x & 0x0000fffcu));
      break;
    case kRelocRel24:
      if (module->unresolvedSection != kSectionUndef) {
        x = module->unresolved - GuestAddress(p);
      }
      StoreGuest32(p, (LoadGuest32(p) & ~0x03fffffcu) | (x & 0x03fffffcu));
      break;
    case kRelocRel14:
    case kRelocRel14Brtaken:
    case kRelocRel14Brntaken:
      StoreGuest32(p, (LoadGuest32(p) & ~0x0000fffcu) | (x & 0x0000fffcu));
      break;
    case kRelocDolphinNop:
      break;
    case kRelocDolphinSection:
      p = GuestPointer(kSectionInfoOffset(SectionTable(module)[rel->section].offset));
      positioned = true;
      break;
    default:
      break;
    }
  }

  return true;
}

// Converts the header, section table, import table and relocation records of a
// module image from the on-disc big-endian layout to host order, in place.
// Returns false when a relocation list does not terminate inside the image, so a
// truncated module is rejected instead of walked off the end of the arena.
bool ConvertMetadata(void* image, size_t imageSize, const ImageInfo& info) {
  auto* bytes = static_cast<uint8_t*>(image);

  // The header is a run of u32 fields with one four-byte run of section numbers
  // in the middle (prolog/epilog/unresolved sections and bss section).
  for (size_t off = 0; off + 4 <= 0x30; off += 4) {
    SwapBE32(bytes + off);
  }
  const size_t headerEnd =
      info.version >= 3 ? kHeaderSizeV3 : (info.version >= 2 ? kHeaderSizeV2 : kHeaderSizeV1);
  for (size_t off = 0x34; off + 4 <= headerEnd; off += 4) {
    SwapBE32(bytes + off);
  }

  // Until linking runs, these three offsets are relative to the image start.
  const auto* header = reinterpret_cast<const RelModuleHeader*>(bytes);
  auto* sections = reinterpret_cast<RelSectionInfo*>(bytes + header->sectionInfoOffset);
  for (uint32_t i = 0; i < header->numSections; i++) {
    SwapBE32(reinterpret_cast<uint8_t*>(&sections[i].offset));
    SwapBE32(reinterpret_cast<uint8_t*>(&sections[i].size));
  }

  auto* imports = reinterpret_cast<RelImportInfo*>(bytes + header->impOffset);
  const uint32_t importCount = header->impSize / sizeof(RelImportInfo);
  for (uint32_t i = 0; i < importCount; i++) {
    SwapBE32(reinterpret_cast<uint8_t*>(&imports[i].id));
    SwapBE32(reinterpret_cast<uint8_t*>(&imports[i].offset));
  }

  for (uint32_t i = 0; i < importCount; i++) {
    auto* rel = bytes + imports[i].offset;
    bool terminated = false;
    while (rel + sizeof(RelReloc) <= bytes + imageSize) {
      SwapBE16(rel);     // offset (delta)
      SwapBE32(rel + 4); // addend; type and section are single bytes
      const bool end = rel[2] == kRelocDolphinEnd;
      rel += sizeof(RelReloc);
      if (end) {
        terminated = true;
        break;
      }
    }
    if (!terminated) {
      return false;
    }
  }
  return true;
}

} // namespace

GuestArena::GuestArena(size_t size, uint32_t preferredBase) {
  if (size == 0) {
    return;
  }

#if defined(_WIN32)
  void* base = preferredBase != 0
                   ? VirtualAlloc(reinterpret_cast<void*>(static_cast<uintptr_t>(preferredBase)), size,
                                  MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)
                   : VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#elif defined(MAP_FIXED_NOREPLACE)
  void* base = nullptr;
  if (preferredBase != 0) {
    void* placed = mmap(reinterpret_cast<void*>(static_cast<uintptr_t>(preferredBase)), size,
                        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                        -1, 0);
    base = placed == MAP_FAILED ? nullptr : placed;
  }
  if (base == nullptr) {
    base = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    if (base == MAP_FAILED) {
      base = nullptr;
    }
  }
#elif defined(MAP_32BIT)
  // Linux x86-64: ask for an address below 2 GiB, which satisfies the 32-bit
  // guest address requirement unconditionally.
  void* base =
      mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
  if (base == MAP_FAILED) {
    base = nullptr;
  }
#else
  // Elsewhere, hint below 4 GiB and verify; the 32-bit relocation encoding
  // cannot represent anything higher.
  void* hint = reinterpret_cast<void*>(static_cast<uintptr_t>(0x10000000));
  void* base = mmap(hint, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (base == MAP_FAILED) {
    base = nullptr;
  }
#endif

  if (base == nullptr || reinterpret_cast<uintptr_t>(base) + size > 0x100000000ull) {
#if defined(_WIN32)
    if (base != nullptr) {
      VirtualFree(base, 0, MEM_RELEASE);
    }
#else
    if (base != nullptr) {
      munmap(base, size);
    }
#endif
    return;
  }

  m_base = static_cast<uint8_t*>(base);
  m_size = size;
}

GuestArena::~GuestArena() {
  if (m_base == nullptr) {
    return;
  }
#if defined(_WIN32)
  VirtualFree(m_base, 0, MEM_RELEASE);
#else
  munmap(m_base, m_size);
#endif
}

void* GuestArena::Allocate(size_t size, size_t alignment) {
  if (m_base == nullptr || !IsPowerOfTwo(static_cast<uint32_t>(alignment))) {
    return nullptr;
  }
  const size_t aligned = (m_used + alignment - 1) & ~(alignment - 1);
  if (aligned + size > m_size) {
    return nullptr;
  }
  void* result = m_base + aligned;
  m_used = aligned + size;
  return result;
}

bool GuestArena::Contains(const void* ptr) const {
  const auto address = reinterpret_cast<uintptr_t>(ptr);
  const auto base = reinterpret_cast<uintptr_t>(m_base);
  return m_base != nullptr && address >= base && address < base + m_size;
}

RelSectionInfo* SectionTable(RelModuleHeader* header) {
  return reinterpret_cast<RelSectionInfo*>(static_cast<uintptr_t>(header->sectionInfoOffset));
}

bool Probe(const void* data, size_t size, ImageInfo& info) {
  if (data == nullptr || size < kHeaderSizeV1) {
    return false;
  }
  const auto* bytes = static_cast<const uint8_t*>(data);

  const uint32_t moduleId = ReadBE32(bytes + 0x00);
  const uint32_t numSections = ReadBE32(bytes + 0x0C);
  const uint32_t sectionInfoOffset = ReadBE32(bytes + 0x10);
  const uint32_t version = ReadBE32(bytes + 0x1C);

  if (version < 1 || version > 3) {
    return false;
  }
  if (numSections < 1 || numSections > kMaxSections) {
    return false;
  }

  uint32_t align = 0;
  uint32_t bssAlign = 0;
  uint32_t fixSize = 0;
  if (version >= 2) {
    if (size < kHeaderSizeV2) {
      return false;
    }
    align = ReadBE32(bytes + 0x40);
    bssAlign = ReadBE32(bytes + 0x44);
  }
  if (version >= 3) {
    if (size < kHeaderSizeV3) {
      return false;
    }
    fixSize = ReadBE32(bytes + kHeaderSizeV2);
  }

  if ((align != 0 && !IsPowerOfTwo(align)) || (bssAlign != 0 && !IsPowerOfTwo(bssAlign))) {
    return false;
  }
  if (sectionInfoOffset > size || numSections * sizeof(RelSectionInfo) > size - sectionInfoOffset) {
    return false;
  }

  const uint32_t bssSize = ReadBE32(bytes + 0x20);
  const uint32_t relOffset = ReadBE32(bytes + 0x24);
  const uint32_t impOffset = ReadBE32(bytes + 0x28);
  const uint32_t impSize = ReadBE32(bytes + 0x2C);
  if (bssSize > kMaxBssSize) {
    return false;
  }
  if (impOffset > size || impSize > size - impOffset) {
    return false;
  }
  if (relOffset > size) {
    return false;
  }

  // Everything the linker walks - section table, import table and the relocation
  // records - has to be addressable, and the records sit after `fixSize` in a
  // version-3 file, so the whole image is mapped. `fixSize` is validated below
  // and reported for reference.
  if (version >= 3 && fixSize != 0) {
    if (fixSize > size) {
      return false;
    }
    if (impOffset + impSize > fixSize) {
      // The import table belongs to the fixed part of the image.
      return false;
    }
  }

  info.version = version;
  info.numSections = numSections;
  info.moduleId = moduleId;
  info.bssSize = bssSize;
  info.align = align;
  info.bssAlign = bssAlign;
  info.fixedSize = version >= 3 ? fixSize : 0;
  info.imageSize = size;
  return true;
}

bool LinkModule(RelModuleHeader* header, void* bss) {
  header->bssSection = 0;

  if (header->version < 1 || header->version > 3) {
    return false;
  }
  if (header->version >= 2 &&
      ((header->align != 0 && reinterpret_cast<uintptr_t>(header) % header->align != 0) ||
       (header->bssAlign != 0 && reinterpret_cast<uintptr_t>(bss) % header->bssAlign != 0))) {
    return false;
  }
  // The console's OSLink is the non-fixed path; OSLinkFixed's impSize
  // truncation is deliberately not implemented, because no module that needs it
  // is known for these games and it cannot be tested without one.

  s_modules.push_back(header);

  // Section, relocation and import offsets arrive image-relative and become
  // guest addresses.
  const uint32_t base = GuestAddress(header);
  header->sectionInfoOffset += base;
  header->relOffset += base;
  header->impOffset += base;
  if (header->version >= 3) {
    header->fixSize += base;
  }

  for (uint32_t i = 1; i < header->numSections; i++) {
    RelSectionInfo* section = &SectionTable(header)[i];
    if (section->offset != 0) {
      section->offset += base;
    } else if (section->size != 0) {
      // A section with no file offset and a size is bss. The console linker maps
      // every bss section onto the single block the loader supplied.
      header->bssSection = static_cast<uint8_t>(i);
      section->offset = GuestAddress(bss);
    }
  }

  auto* imports = reinterpret_cast<RelImportInfo*>(static_cast<uintptr_t>(header->impOffset));
  const uint32_t importCount = header->impSize / sizeof(RelImportInfo);
  for (uint32_t i = 0; i < importCount; i++) {
    imports[i].offset += base;
  }

  if (header->prologSection != kSectionUndef) {
    header->prolog += kSectionInfoOffset(SectionTable(header)[header->prologSection].offset);
  }
  if (header->epilogSection != kSectionUndef) {
    header->epilog += kSectionInfoOffset(SectionTable(header)[header->epilogSection].offset);
  }
  if (header->unresolvedSection != kSectionUndef) {
    header->unresolved += kSectionInfoOffset(SectionTable(header)[header->unresolvedSection].offset);
  }
  // The console adds its string table to nameOffset here; the port has no such
  // table, so the field stays image-relative.

  Relocate(nullptr, header);

  // The console re-relocates every module against the new one and the new one
  // against every module. The order matters and is preserved.
  for (size_t i = 0; i < s_modules.size(); i++) {
    Relocate(header, s_modules[i]);
    if (s_modules[i] != header) {
      Relocate(s_modules[i], header);
    }
  }

  std::memset(bss, 0, header->bssSize);

  return true;
}

bool UnlinkModule(RelModuleHeader* header) {
  const size_t index = FindModuleIndex(header);
  if (index == s_modules.size()) {
    return false;
  }
  s_modules.erase(s_modules.begin() + static_cast<ptrdiff_t>(index));

  for (RelModuleHeader* module : s_modules) {
    Undo(header, module);
  }

  if (header->prologSection != kSectionUndef) {
    header->prolog -= kSectionInfoOffset(SectionTable(header)[header->prologSection].offset);
  }
  if (header->epilogSection != kSectionUndef) {
    header->epilog -= kSectionInfoOffset(SectionTable(header)[header->epilogSection].offset);
  }
  if (header->unresolvedSection != kSectionUndef) {
    header->unresolved -= kSectionInfoOffset(SectionTable(header)[header->unresolvedSection].offset);
  }

  const uint32_t base = GuestAddress(header);
  auto* imports = reinterpret_cast<RelImportInfo*>(static_cast<uintptr_t>(header->impOffset));
  const uint32_t importCount = header->impSize / sizeof(RelImportInfo);
  for (uint32_t i = 0; i < importCount; i++) {
    imports[i].offset -= base;
  }

  for (uint32_t i = 1; i < header->numSections; i++) {
    RelSectionInfo* section = &SectionTable(header)[i];
    if (i == header->bssSection) {
      header->bssSection = 0;
      section->offset = 0;
    } else if (section->offset != 0) {
      section->offset -= base;
    }
  }

  header->relOffset -= base;
  header->impOffset -= base;
  header->sectionInfoOffset -= base;
  return true;
}

RelModuleHeader* SearchModule(const void* address, uint32_t* section, uint32_t* offset) {
  if (address == nullptr) {
    return nullptr;
  }
  const auto value = reinterpret_cast<uintptr_t>(address);

  for (RelModuleHeader* header : s_modules) {
    RelSectionInfo* sections = SectionTable(header);
    for (uint32_t i = 0; i < header->numSections; i++) {
      if (sections[i].size != 0) {
        const uintptr_t base = kSectionInfoOffset(sections[i].offset);
        if (base <= value && value < base + sections[i].size) {
          if (section != nullptr) {
            *section = i;
          }
          if (offset != nullptr) {
            *offset = static_cast<uint32_t>(value - base);
          }
          return header;
        }
      }
    }
  }
  return nullptr;
}

size_t ModuleCount() {
  return s_modules.size();
}

RelModuleHeader* ModuleAt(size_t index) {
  return index < s_modules.size() ? s_modules[index] : nullptr;
}

void ResetModules() {
  s_modules.clear();
}

// Linking turns the prolog/epilog/unresolved offsets into guest entry points,
// exactly as the console does, so these are read straight back out.
uint32_t Module::Prolog() const {
  return m_linked && m_header->prologSection != kSectionUndef ? m_header->prolog : 0;
}

uint32_t Module::Epilog() const {
  return m_linked && m_header->epilogSection != kSectionUndef ? m_header->epilog : 0;
}

uint32_t Module::Unresolved() const {
  return m_linked && m_header->unresolvedSection != kSectionUndef ? m_header->unresolved : 0;
}

bool Module::Unlink() {
  if (!m_linked) {
    return false;
  }
  const bool result = UnlinkModule(m_header);
  m_linked = false;
  return result;
}

Module Load(const void* data, size_t size, GuestArena& arena) {
  Module module;

  ImageInfo info;
  if (!Probe(data, size, info) || !arena.Valid()) {
    return module;
  }

  void* image = arena.Allocate(info.imageSize, EffectiveAlignment(info.align));
  if (image == nullptr) {
    return module;
  }
  // The image is a mixture of file data and (for version 3) trailing padding.
  std::memset(image, 0, info.imageSize);
  std::memcpy(image, data, size < info.imageSize ? size : info.imageSize);
  if (!ConvertMetadata(image, info.imageSize, info)) {
    return module;
  }

  // Linking zeroes the bss; give it a real block even for a module without one
  // so the alignment check always has something valid to look at.
  void* bss =
      arena.Allocate(info.bssSize != 0 ? info.bssSize : 1, EffectiveAlignment(info.bssAlign));
  if (bss == nullptr) {
    return module;
  }

  auto* header = static_cast<RelModuleHeader*>(image);
  if (!LinkModule(header, bss)) {
    return module;
  }

  module.m_header = header;
  module.m_bss = bss;
  module.m_imageSize = info.imageSize;
  module.m_linked = true;
  return module;
}

} // namespace rel
} // namespace port
