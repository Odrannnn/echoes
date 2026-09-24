#ifndef METROID_PRIME_PORT_PORT_RANDOMIZER_H
#define METROID_PRIME_PORT_PORT_RANDOMIZER_H
#include <cstdint>

namespace PortRandomizer {

// Loads the configured seed once. Call on the game thread; never throws.
void EnsureLoaded();
// True when a seed contains at least one placement.
bool Enabled();
// True when MP_RANDO_DUMP is set to a non-empty value other than "0".
bool DumpEnabled();
// Name from the loaded seed, or an empty string when none is available.
const char* SeedName();
// Number of pickup checks recorded during this process.
int CheckCount();
// Short status line suitable for the F1 overlay.
const char* StatusText();
// Logs a location in dump mode, or applies its placement when enabled.
bool ApplyPickup(uint32_t worldAssetId, uint32_t areaAssetId, uint32_t entityId,
                 int& itemType, int& capacity, int& amount);
// Records a collected pickup check when randomizer or dump mode is active.
void RecordCheck(uint32_t worldAssetId, uint32_t areaAssetId, uint32_t entityId, int itemType);
// Returns the retail item name, or "Unknown" for an out-of-range item type.
const char* ItemName(int itemType);
// Returns an item type for a case-insensitive exact name, or -1 when unknown.
int ItemFromName(const char* name);
// Formats a location as three uppercase, zero-padded eight-digit hexadecimal IDs.
void FormatLocationKey(uint32_t worldAssetId, uint32_t areaAssetId, uint32_t entityId,
                       char* out, int outSize);

} // namespace PortRandomizer

#endif
