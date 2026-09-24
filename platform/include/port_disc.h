#pragma once
#include <cstdint>
#include <vector>

// Read an embedded resource from a mapped DOL section on the already-mounted
// GM8E01_00 disc. The native executable contains no generated game asset arrays.
std::vector<uint8_t> PortReadDolResource(uint32_t address, uint32_t length);
