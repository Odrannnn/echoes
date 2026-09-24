#pragma once

// REL (module) loading and linking for the native port.
//
// Metroid Prime 2 runs its gameplay from `main.dol` plus per-area REL modules
// that the game loads from the disc and links at runtime; Metroid Prime (the
// sibling port) is a single DOL and has no equivalent. This is the port's
// replacement for the console's OSLink: the module registry, the module loader,
// and the 32-bit address arena the images live in.
//
// Why the port does not use the SDK's OSModuleHeader: on the GameCube it is
// built with 32-bit pointers, so the module image can be overlaid with it and
// the linker works in place. On a 64-bit host the SDK struct grows (`OSModuleLink`
// holds two pointers) and no longer matches the on-disc layout at all, while the
// relocation encoding is fixed at 32 bits. The port therefore works on the
// module file's own layout, defined below, and keeps its registry outside the
// image. `Relocate`/`LinkModule`/`UnlinkModule` follow the console implementation
// (decompiled as PrimeDecomp/{prime,echoes} src/Dolphin/os/OSLink.c) step for
// step.

#include <cstddef>
#include <cstdint>

namespace port {
namespace rel {

// --- On-disc layout -------------------------------------------------------
// Exactly the bytes the REL file contains, in host order once converted by
// Probe/Load. Module images are big-endian on disc; see ConvertMetadata.

enum : uint8_t {
  kRelocNone = 0,
  kRelocAddr32 = 1,
  kRelocAddr24 = 2,
  kRelocAddr16 = 3,
  kRelocAddr16Lo = 4,
  kRelocAddr16Hi = 5,
  kRelocAddr16Ha = 6,
  kRelocAddr14 = 7,
  kRelocAddr14Brtaken = 8,
  kRelocAddr14Brntaken = 9,
  kRelocRel24 = 10,
  kRelocRel14 = 11,
  kRelocRel14Brtaken = 12,
  kRelocRel14Brntaken = 13,

  kRelocDolphinNop = 201,
  kRelocDolphinSection = 202,
  kRelocDolphinEnd = 203,
  kRelocDolphinMrkref = 204,
};

// Version-1 header; version 2 appends align/bssAlign and version 3 fixSize.
struct RelModuleHeader {
  uint32_t id;
  uint32_t linkNext; // console list links; the port keeps its own registry
  uint32_t linkPrev;
  uint32_t numSections;
  uint32_t sectionInfoOffset;
  uint32_t nameOffset;
  uint32_t nameSize;
  uint32_t version;

  uint32_t bssSize;
  uint32_t relOffset;
  uint32_t impOffset;
  uint32_t impSize;
  uint8_t prologSection;
  uint8_t epilogSection;
  uint8_t unresolvedSection;
  uint8_t bssSection; // set at link time
  uint32_t prolog;
  uint32_t epilog;
  uint32_t unresolved;

  // version >= 2
  uint32_t align;
  uint32_t bssAlign;

  // version >= 3
  uint32_t fixSize;
};

struct RelSectionInfo {
  uint32_t offset; // bit 0 is the "contains code" flag
  uint32_t size;
};

struct RelImportInfo {
  uint32_t id;     // module the following relocations import from
  uint32_t offset; // byte offset of the relocation list
};

struct RelReloc {
  uint16_t offset; // delta from the previous entry
  uint8_t type;
  uint8_t section;
  uint32_t addend;
};

constexpr uint32_t kSectionInfoExecFlag = 0x1;
constexpr uint32_t kSectionInfoOffset(uint32_t offset) {
  return offset & ~kSectionInfoExecFlag;
}

// Header sizes by version, used to validate an image.
constexpr size_t kHeaderSizeV1 = 0x40;
constexpr size_t kHeaderSizeV2 = 0x48;
constexpr size_t kHeaderSizeV3 = 0x4C;

// Section index values the header may carry.
constexpr uint8_t kSectionUndef = 0;

// --- Arena ----------------------------------------------------------------

// REL relocations carry 32-bit addresses, so module images have to live below
// 4 GiB. Allocation is a bump allocator: modules are loaded once and are not
// meant to be freed individually (unlinking does not release the image).
// Guest addresses have to stay within branch range of the addresses the module
// imports from: a REL24 relocation can only encode +/-32 MiB, so a port that
// links real RELs has to place its arena in the console's module region (the
// 0x80000000 area), not anywhere below 4 GiB. `preferredBase` asks for a
// specific address; when the mapping fails the arena falls back to any
// 32-bit-addressable address, which is fine for modules that do not branch.
class GuestArena {
public:
  explicit GuestArena(size_t size, uint32_t preferredBase = 0);
  ~GuestArena();

  GuestArena(const GuestArena&) = delete;
  GuestArena& operator=(const GuestArena&) = delete;

  // Returns null when the arena is exhausted or the alignment is not a power of
  // two.
  void* Allocate(size_t size, size_t alignment);

  bool Valid() const { return m_base != nullptr; }
  void* Base() const { return m_base; }
  size_t Size() const { return m_size; }
  size_t Used() const { return m_used; }
  bool Contains(const void* ptr) const;

private:
  uint8_t* m_base = nullptr;
  size_t m_size = 0;
  size_t m_used = 0;
};

// --- Module ---------------------------------------------------------------

// Header fields that decide how a module image has to be placed.
struct ImageInfo {
  uint32_t version = 0;
  uint32_t numSections = 0;
  uint32_t moduleId = 0;
  uint32_t bssSize = 0;
  uint32_t align = 0;
  uint32_t bssAlign = 0;
  size_t imageSize = 0;
};

// Validates a REL image as it is stored on disc and reports the fields a loader
// needs. Returns false for a truncated or implausible header; never reads past
// `size` bytes.
bool Probe(const void* data, size_t size, ImageInfo& info);

class Module {
public:
  Module() = default;

  // Compares equal to false when the image could not be loaded or linking was
  // refused (bad version or alignment).
  explicit operator bool() const { return m_linked; }
  bool Linked() const { return m_linked; }

  RelModuleHeader* Header() const { return m_header; }

  // The loaded image and its bss block, for tests and diagnostics.
  void* Image() const { return m_header; }
  void* Bss() const { return m_bss; }
  size_t ImageSize() const { return m_imageSize; }

  // Guest entry points, valid once linked. Zero when the module declares none.
  uint32_t Prolog() const;
  uint32_t Epilog() const;
  uint32_t Unresolved() const;

  // Removes the module from the registry and undoes the relocations it owned in
  // the modules that are still loaded.
  bool Unlink();

private:
  friend Module Load(const void* data, size_t size, GuestArena& arena);
  friend bool LinkModule(RelModuleHeader* header, void* bss);
  friend bool UnlinkModule(RelModuleHeader* header);

  RelModuleHeader* m_header = nullptr;
  void* m_bss = nullptr;
  size_t m_imageSize = 0;
  bool m_linked = false;
};

// Copies `data` into `arena`, allocates the module's bss and links it. The image
// is mutated in place, exactly as it is on the console.
Module Load(const void* data, size_t size, GuestArena& arena);

// --- Linking --------------------------------------------------------------

// Links an image that already sits in a 32-bit arena. `header` must point at the
// start of a validated, host-order image and `bss` at a block of at least
// `header->bssSize` bytes with the module's alignment. Registers the module.
bool LinkModule(RelModuleHeader* header, void* bss);

// Reverses LinkModule. `header` must be a linked module.
bool UnlinkModule(RelModuleHeader* header);

// The module that owns a guest address, the way the console's OSSearchModule
// answers for the OS's error paths. Returns null when no loaded module owns it.
// `section` and `offset` may be null.
RelModuleHeader* SearchModule(const void* address, uint32_t* section, uint32_t* offset);

// Number of currently linked modules, in load order.
size_t ModuleCount();

// The i-th linked module, or null.
RelModuleHeader* ModuleAt(size_t index);

// Section table of a *linked* module as a host pointer; entries are
// guest-addressed. Unlinking makes the header's offsets image-relative again, so
// this must not be called on an unlinked module.
RelSectionInfo* SectionTable(RelModuleHeader* header);

// Empties the registry. For tests.
void ResetModules();

} // namespace rel
} // namespace port
