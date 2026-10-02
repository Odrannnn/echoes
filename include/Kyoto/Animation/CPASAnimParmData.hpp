#ifndef _CPASANIMPARMDATA
#define _CPASANIMPARMDATA

#include "Kyoto/Animation/CPASAnimState.hpp"

class CPASAnimParmData {
  pas::EAnimationState mStateId;
  rstl::reserved_vector< CPASAnimParm, 8 > mParms;

public:
  CPASAnimParmData(pas::EAnimationState stateId,
                   const CPASAnimParm& parm1 = CPASAnimParm::NoParameter(),
                   const CPASAnimParm& parm2 = CPASAnimParm::NoParameter(),
                   const CPASAnimParm& parm3 = CPASAnimParm::NoParameter(),
                   const CPASAnimParm& parm4 = CPASAnimParm::NoParameter(),
                   const CPASAnimParm& parm5 = CPASAnimParm::NoParameter(),
                   const CPASAnimParm& parm6 = CPASAnimParm::NoParameter(),
                   const CPASAnimParm& parm7 = CPASAnimParm::NoParameter(),
                   const CPASAnimParm& parm8 = CPASAnimParm::NoParameter())
  : mStateId(stateId) {
    mParms.push_back(parm1);
    mParms.push_back(parm2);
    mParms.push_back(parm3);
    mParms.push_back(parm4);
    mParms.push_back(parm5);
    mParms.push_back(parm6);
    mParms.push_back(parm7);
    mParms.push_back(parm8);
  }
  ~CPASAnimParmData() {}

  //!< Declared, not implicit, so this unit can define it and own retail's bytes for it.
  //!
  //!< Retail emits this copy constructor out of line, 232 bytes at 0x801DC820, unnamed in
  //!< `config/G2ME01/symbols.txt` (`fn_801DC820`) because Metaforce never sees an
  //!< implicitly-declared special member. `EnterStruck` is its only caller in retail, and
  //!< `MetroidPrime/Weapons/GunController/CGunController`'s split claims 0x801DC1B8..0x801DCC68,
  //!< so that is the unit that has to define it; `src/Kyoto/Animation/` has no
  //!< `CPASAnimParmData.cpp`. This header is the one place the declaration can go, exactly as
  //!< `rstl::reserved_vector`'s own `operator=` is declared here.
  CPASAnimParmData(const CPASAnimParmData& other);

  pas::EAnimationState GetStateId() const { return mStateId; }
  const rstl::reserved_vector< CPASAnimParm, 8 >& GetAnimParmData() const { return mParms; }

  static CPASAnimParmData NoParameters(pas::EAnimationState stateId) {
    return CPASAnimParmData(stateId);
  }
};

#endif // _CPASANIMPARMDATA
