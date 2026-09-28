// Retail's REL module manager, the closure under `fn_801F05D0` - port-only, host build only.
//
// `CMain::RsMain`'s frame loop hands `fn_801F05D0` the object at `CGameGlobalObjects`+0x150
// (`lbl_80418EC8`) twice a frame (0x8000607C, 0x80006244), and `CMain::ShutdownSubsystems` pumps
// it until `Tweaks.rel` is gone. That object is an `rstl::map<rstl::string, SModuleRecord*>`, and
// each record is a small state machine that reads one `.rel` off the disc, links it, runs its
// prolog, and later runs its epilog and unlinks it. Retail's symbol table names none of it (nor
// does R3ME01's), so every function keeps its dtk label. The bodies are transcribed from
// `tools/dis.sh` of each address, noted per function.
//
// **Why this is a `files.cmake` entry and not a `configure.py` unit.** Two of the twelve
// functions cannot be the retail code on the host. `fn_802136A0` ends in `OSLinkFixed` and a
// `bctrl` through the image's prolog pointer, and `fn_802137C0` in a `bctrl` through the epilog
// and `OSUnlink`: all four operate on the PowerPC image read off the disc, whose code the host
// cannot execute. The port instead runs the module's host init and shutdown out of
// `platform/compiled_modules.cpp` (`port_modules_prolog`/`port_modules_epilog`), and a module
// that is not compiled in is a declared stop there. `OSLinkFixed` and `OSUnlink` therefore stay
// undefined on purpose (`platform/sdk_stubs.cpp`). Everything else is retail's logic, quirks
// included.
//
// **When a decomp unit writes one of these for the DOL, delete it here**: this file only fills
// the host link, and a second definition would be a duplicate symbol.
//
// What still does not exist: `fn_801F03C4`, which creates a record and inserts it (0x801F03C4,
// 0x70), and the rest of the 0x801F0xxx handle family `CMain::ShutdownSubsystems` uses. So on
// the host the map is empty, and `fn_801F05D0` is a walk over no records until those land.

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/CDvdFile.hpp"
#include "Kyoto/CDvdRequest.hpp"
#include "MetroidPrime/CGameGlobalObjects.hpp"
#include "rstl/map.hpp"
#include "rstl/string.hpp"
#include "types.h"

#include "dolphin/os.h"

#include <string.h>

// platform/compiled_modules.cpp: the host init and shutdown of a compiled-in module, by the path
// the record holds. Declared here the way src/REL/REL_Setup.cpp declares its two.
extern "C" void port_modules_prolog(const char* path);
extern "C" void port_modules_epilog(const char* path);

namespace {

// The debugger's module list: 0x14 bytes at record +0x28, a doubly-linked list headed by
// `lbl_80419CA8` (.sbss, `lwz r0,-24792(r13)`). `fn_8033EDF4` links, `fn_8033EDA8` unlinks.
struct SModuleRegistryNode {
  const char* x0_name;
  uchar* x4_image;
  uint x8_size;
  SModuleRegistryNode* xc_next;
  SModuleRegistryNode* x10_prev;
};

// `x24_state`. Retail's transitions (`fn_80213838`) are the only definition of these names.
enum EModuleState {
  kMS_Reading = 0,    // a read is in flight; complete -> link -> kMS_Linked
  kMS_Linked = 1,     // linked; no load requests left -> unlink -> kMS_Unloaded
  kMS_Cancelling = 2, // a read was cancelled; complete -> free -> kMS_Unloaded
  kMS_Unloaded = 3,   // a load request -> read -> kMS_Reading
};

// 0x3C bytes on the cube (the offsets below). Nothing on the host constructs one yet.
struct SModuleRecord {
  rstl::string x0_name;          // the file, e.g. "Tweaks.rel"
  CDvdRequest* x10_request;
  uchar* x14_buffer;             // the whole allocation: image, then the module's .bss
  uint x18_capacity;
  uchar* x1c_module;             // the image's OSModuleHeader, == x14_buffer once read
  short x20_holders;             // fn_80213AEC / fn_80213AD8; 0 + unloaded = destroy
  short x22_loadRequests;        // fn_80213828 / fn_80213818
  int x24_state;                 // EModuleState
  SModuleRegistryNode x28_registry;
};

typedef rstl::map< rstl::string, SModuleRecord* > TModuleMap;

// `CGameGlobalObjectsTail` is this map; its size had to change for the host for exactly that
// reason (`include/MetroidPrime/CGameGlobalObjects.hpp`).
static_assert(sizeof(TModuleMap) == sizeof(CGameGlobalObjectsTail),
              "CGameGlobalObjects+0x150 must be the module map");

// The image is the disc's bytes, so its header is big-endian on every host.
uint ReadBE32(const uchar* p) {
  return (uint(p[0]) << 24) | (uint(p[1]) << 16) | (uint(p[2]) << 8) | uint(p[3]);
}

uint Round32(uint value) { return (value + 31) & ~31u; }

} // namespace

// .sbss 0x80419CA8, 8 bytes in retail; only the first word is ever read or written.
extern "C" SModuleRegistryNode* lbl_80419CA8 = nullptr;

extern "C" {
void fn_80213838(SModuleRecord* rec);
}

// 0x8033EDA8, 0x4C: unlink a registry node.
extern "C" void fn_8033EDA8(SModuleRegistryNode* node) {
  if (lbl_80419CA8 == node) {
    lbl_80419CA8 = node->xc_next;
  }
  if (node->xc_next != nullptr) {
    node->xc_next->x10_prev = node->x10_prev;
  }
  if (node->x10_prev != nullptr) {
    node->x10_prev->xc_next = node->xc_next;
  }
  node->xc_next = nullptr;
  node->x10_prev = nullptr;
}

// 0x8033EDF4, 0x38: link a registry node at the head. Retail tests only `next`, so the node at the
// tail of the list (next == null) would be linked a second time; kept as retail has it.
extern "C" void fn_8033EDF4(SModuleRegistryNode* node, const char* name, uchar* image, uint size) {
  if (node->xc_next != nullptr) {
    return;
  }
  node->x0_name = name;
  node->x4_image = image;
  node->x8_size = size;
  node->xc_next = lbl_80419CA8;
  if (node->xc_next != nullptr) {
    node->xc_next->x10_prev = node;
  }
  lbl_80419CA8 = node;
}

// 0x8033EE2C, 0x50: the registry node's destructor, `flags > 0` deleting.
extern "C" SModuleRegistryNode* fn_8033EE2C(SModuleRegistryNode* node, short flags) {
  if (node != nullptr) {
    fn_8033EDA8(node);
    if (flags > 0) {
      CMemory::Free(node);
    }
  }
  return node;
}

// 0x80213650, 0x28: may the record be destroyed.
extern "C" bool fn_80213650(const SModuleRecord* rec) {
  return rec->x24_state == kMS_Unloaded && rec->x20_holders == 0;
}

// 0x80213A64, 0x98: drop the image. Retail gives the fake statics back first, by the capacity
// *now* - which after a grow in `fn_802136A0` is more than `fn_80213960` added. Kept.
extern "C" void fn_80213A64(SModuleRecord* rec) {
  CMemory::OffsetFakeStatics(-static_cast< int >(rec->x18_capacity));
  if (rec->x10_request != nullptr) {
    delete rec->x10_request;
  }
  rec->x10_request = nullptr;
  CMemory::Free(rec->x14_buffer);
  rec->x14_buffer = nullptr;
  rec->x18_capacity = 0;
  rec->x1c_module = nullptr;
}

// 0x80213960, 0x104: start reading the file into a fresh 32-byte-rounded buffer. False, with
// nothing changed, if the file is not on the disc.
extern "C" bool fn_80213960(SModuleRecord* rec) {
  if (!CDvdFile::FileExists(rec->x0_name.data())) {
    return false;
  }
  CDvdFile file(rec->x0_name.data());
  rec->x18_capacity = Round32(file.Length());
  uchar* buffer = static_cast< uchar* >(
      CMemory::Alloc(rec->x18_capacity, IAllocator::kHI_RoundUpLen, IAllocator::kSC_Unk1,
                     IAllocator::kTP_Heap));
  CMemory::Free(rec->x14_buffer);
  rec->x14_buffer = buffer;
  rec->x1c_module = rec->x14_buffer;
  CMemory::OffsetFakeStatics(rec->x18_capacity);
  CDvdRequest* request = file.SyncRead(rec->x1c_module, rec->x18_capacity);
  if (rec->x10_request != nullptr) {
    delete rec->x10_request;
  }
  rec->x10_request = request;
  return true;
}

// 0x802136A0, 0x120: link the image and run its prolog. The buffer is grown first when the
// module's .bss does not fit behind its fixed part (header +0x48 `fixSize`, +0x20 `bssSize`).
extern "C" void fn_802136A0(SModuleRecord* rec) {
  OSGetTime();
  const uint fixed = Round32(ReadBE32(rec->x1c_module + 0x48));
  const uint bss = ReadBE32(rec->x1c_module + 0x20);
  if (rec->x18_capacity - fixed < bss) {
    const uint capacity = Round32(bss) + fixed;
    uchar* grown = static_cast< uchar* >(CMemory::Alloc(
        capacity, IAllocator::kHI_RoundUpLen, IAllocator::kSC_Unk1, IAllocator::kTP_Heap));
    memcpy(grown, rec->x14_buffer, rec->x18_capacity); // fn_8028BE1C
    rec->x18_capacity = capacity;
    CMemory::Free(rec->x14_buffer);
    rec->x14_buffer = grown;
    rec->x1c_module = rec->x14_buffer;
  }
  // Retail: `OSLinkFixed(module, module + fixed)`, result ignored - see the file header.
  fn_8033EDF4(&rec->x28_registry, rec->x0_name.data(), rec->x14_buffer, rec->x18_capacity);
  // Retail: `bctrl` through the header's prolog (+0x34).
  port_modules_prolog(rec->x0_name.data());
  OSGetTime();
}

// 0x802137C0, 0x58: run the epilog, unlink, drop the image.
extern "C" void fn_802137C0(SModuleRecord* rec) {
  fn_8033EDA8(&rec->x28_registry);
  if (rec->x1c_module != nullptr) {
    // Retail: `bctrl` through the header's epilog (+0x38), then `OSUnlink(module)`.
    port_modules_epilog(rec->x0_name.data());
  }
  fn_80213A64(rec);
}

// 0x80213838, 0x128: one step of the record's state machine.
extern "C" void fn_80213838(SModuleRecord* rec) {
  switch (rec->x24_state) {
  case kMS_Reading:
    if (rec->x10_request->IsComplete()) {
      rec->x24_state = kMS_Linked;
      fn_802136A0(rec);
    } else if (rec->x22_loadRequests == 0) {
      rec->x10_request->PostCancelRequest();
      rec->x24_state = kMS_Cancelling;
    }
    break;
  case kMS_Linked:
    if (rec->x22_loadRequests == 0) {
      fn_802137C0(rec);
      rec->x24_state = kMS_Unloaded;
    }
    break;
  case kMS_Cancelling:
    if (!rec->x10_request->IsComplete()) {
      break;
    }
    fn_80213A64(rec);
    rec->x24_state = kMS_Unloaded;
    // Falls into kMS_Unloaded in the same step, as retail's branch to 0x802138A4 does.
  case kMS_Unloaded:
    if (rec->x22_loadRequests >= 1) {
      // A file that is not on the disc lands in kMS_Linked with no image, and the next step
      // with no requests unlinks nothing and comes back here. Retail's own logic.
      rec->x24_state = fn_80213960(rec) ? kMS_Reading : kMS_Linked;
    }
    break;
  }
}

// 0x80213AFC, 0xB0: the record's destructor, `flags > 0` deleting. A record that is not unloaded
// gets one more step first.
extern "C" SModuleRecord* fn_80213AFC(SModuleRecord* rec, short flags) {
  if (rec != nullptr) {
    if (rec->x24_state != kMS_Unloaded) {
      fn_80213838(rec);
    }
    fn_8033EE2C(&rec->x28_registry, -1);
    CMemory::Free(rec->x14_buffer);
    if (rec->x10_request != nullptr) {
      delete rec->x10_request;
    }
    rec->x0_name.~basic_string();
    if (flags > 0) {
      CMemory::Free(rec);
    }
  }
  return rec;
}

// 0x801F05D0, 0xF8: step every record, and destroy and erase the first one that is done, then
// start over - retail restarts the walk after every erase. `fn_801F06C8` is the map's erase.
extern "C" void fn_801F05D0(void* owner) {
  TModuleMap& modules = *static_cast< TModuleMap* >(owner);
  bool restart = true;
  while (restart) {
    restart = false;
    for (TModuleMap::iterator it = modules.begin(); it != modules.end(); ++it) {
      SModuleRecord* rec = it->second;
      fn_80213838(rec);
      if (fn_80213650(rec)) {
        fn_80213AFC(rec, 1);
        modules.erase(it);
        restart = true;
        break;
      }
    }
  }
}
