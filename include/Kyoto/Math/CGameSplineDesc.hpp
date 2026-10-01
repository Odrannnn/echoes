#ifndef _CGAMESPLINEDESC
#define _CGAMESPLINEDESC

#include "Kyoto/Math/CMayaSpline.hpp"
#include "Kyoto/Math/CMotionSpline.hpp"

// Class name corroborated by the Echoes Wii CScriptEffect constructor export.
class CGameSplineDesc {
public:
  CGameSplineDesc(const SLdrSpline& spline, CMotionSpline::ESplineType type, float duration,
                  bool closedLoop);
  // Declared, not defined: retail carries the deleting destructor out of line in more than
  // one object (`CScriptEffect` at 0x80080A80, `CScriptCannonBall` in its own REL), so each
  // translation unit that has one writes its own copy. An inline body here would be inlined
  // at every call site and neither object would ever contain the symbol.
  ~CGameSplineDesc();

  const SLdrSpline& GetSpline() const { return mSpline; }
  CMotionSpline::ESplineType GetType() const { return mType; }
  float GetDuration() const { return mDuration; }
  bool IsClosedLoop() const { return mClosedLoop; }

private:
  SLdrSpline mSpline;
  CMotionSpline::ESplineType mType;
  float mDuration;
  bool mClosedLoop : 1;
};
CHECK_SIZEOF(CGameSplineDesc, 0x50)

#endif // _CGAMESPLINEDESC
