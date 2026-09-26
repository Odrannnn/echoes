// Retail `CanBeShot__3CAiFRC13CStateManageri` = `_ZN3CAi9CanBeShotERK13CStateManageri`,
// .text 0x800358D8..0x800358E0, 0x8 = 8 bytes:
//
//     800358d8  li   r3,1
//     800358dc  blr
//
// The base class says every creature can be shot; the overrides that do not are further down
// the vtable. Both parameters are unused, and `bool` return is `li r3,1` - the same encoding
// `CPatterned::VSlot68` (0x80073C90) uses.
//
// Its own unit: 0x800358E0, `CPatterned::VSlot61`, is the very next function, and
// `CAi::fn_80035650` sits 0x188 bytes above, so a unit covering both ends would be two
// discontiguous `.text` ranges - which `dtk dol split` rejects with "Cyclic dependency
// encountered while resolving link order" before anything compiles.
#include "MetroidPrime/Enemies/CAi.hpp"

bool CAi::CanBeShot(const CStateManager&, int) { return true; }
