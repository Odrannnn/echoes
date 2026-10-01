#ifndef _CPASANIMPARM
#define _CPASANIMPARM

#include "rstl/construct.hpp"
#include "types.h"

class CPASAnimParm {
public:
  enum EParmType {
    kPT_None = -1,
    kPT_Int32 = 0,
    kPT_UInt32 = 1,
    kPT_Float = 2,
    kPT_Bool = 3,
    kPT_Enum = 4,

  };
  union UParmValue {
    int m_int;
    uint m_uint;
    float m_float;
    bool m_bool;
  };

  CPASAnimParm(UParmValue value, EParmType type);

  static CPASAnimParm NoParameter();
  static CPASAnimParm FromInt32(int value);
  static CPASAnimParm FromUint32(uint value);
  static CPASAnimParm FromReal32(float value);
  static CPASAnimParm FromBool(bool val);
  static CPASAnimParm FromEnum(int value);

  int GetInt32Value() const;
  uint GetUint32Value() const;
  float GetReal32Value() const;
  bool GetBoolValue() const;
  int GetEnumValue() const;

  const UParmValue& GetParameterValue() const { return mValue; }
  EParmType GetParameterType() const { return mType; }

private:
  UParmValue mValue;
  EParmType mType;
};

namespace rstl {
// Trivially *constructible* as well as trivially destructible: `CPASAnimParm` is two 4-byte
// members, so `rstl::construct` lowers to the plain assignment `construct_impl` below defines.
// Retail's lowering says so - measured on `CPASAnimParmData`'s copy constructor at 0x801DC820
// (232 bytes, this unit's `fn_801DC820`), whose element loop is eight `lwz`/`stw` pairs with no
// null test on the destination cursor. Through the placement-`new` form that test survives
// inlining (`new (dest) T(src)` becomes "call `operator new`, test it against null, then
// construct"), which leaves a 1x loop behind a `cmplwi`/`beq` and scores 0.00%.
RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE(CPASAnimParm)
} // namespace rstl

CHECK_SIZEOF(CPASAnimParm, 0x8)

#endif // _CPASANIMPARM
