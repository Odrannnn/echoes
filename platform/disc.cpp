#include "port_disc.h"

#include <dolphin/dvd.h>
#include <stdexcept>

namespace {
uint32_t ReadBig32(const uint8_t* data) {
  return (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) |
         (uint32_t(data[2]) << 8) | uint32_t(data[3]);
}

}

std::vector<uint8_t> PortReadDolResource(uint32_t address, uint32_t length) {
  s32 dolSize = 0;
  const uint8_t* dol = DVDGetDOLLocation(&dolSize);
  if (dol == nullptr || dolSize < 0x100) {
    throw std::runtime_error("Could not read the mounted disc's DOL");
  }
  for (uint32_t i = 0; i < 18; ++i) {
    const uint32_t sectionOffset = ReadBig32(dol + i * 4);
    const uint32_t sectionAddress = ReadBig32(dol + 0x48 + i * 4);
    const uint32_t sectionSize = ReadBig32(dol + 0x90 + i * 4);
    if (address < sectionAddress || address - sectionAddress > sectionSize ||
        length > sectionSize - (address - sectionAddress))
      continue;
    const uint64_t start = uint64_t(sectionOffset) + address - sectionAddress;
    if (start > static_cast<uint32_t>(dolSize) || length > dolSize - start)
      break;
    return {dol + start, dol + start + length};
  }
  throw std::runtime_error("Expected embedded resource is absent from the disc's DOL");
}
