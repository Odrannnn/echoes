#ifndef _CERROROUTPUTWINDOW
#define _CERROROUTPUTWINDOW

#include "types.h"

#include "MetroidPrime/CIOWin.hpp"

class CErrorOutputWindow : public CIOWin {
public:
  enum EFlag {
    kF_Zero,
    kF_One,
  };

  CErrorOutputWindow(EFlag);
  /** **Out of line, where the inline `{}` was.** Retail's `__dt__18CErrorOutputWindowFv`
      (0x800078F8, 96 bytes) is an out-of-line definition in `MetroidPrime/main.cpp`'s object, and
      an inline body in a class the matching build only ever `new`s is never emitted. It is
      `main.cpp`'s, so the port gets its own copy in `src/MetroidPrime/PortGlobals.cpp` - which also
      gives the port the vtable this destructor is the key function for. */
  ~CErrorOutputWindow() override;

  EMessageReturn OnMessage(const CArchitectureMessage&, CArchitectureQueue&) override;
  bool GetIsContinueDraw() const override;
  void Draw() const override;

  void UpdateWindow();
  void Update();
  void ShowMessage() const;

private:
  enum EState {
    kS_Zero,
    kS_One,
  };

  EState mState;
  bool x18_25_ : 1;
  bool x18_26_ : 1;
  bool x18_27_ : 1;
  bool x18_28_ : 1;
  const wchar_t* mMsg;

  void SetState(EState);
  void DrawError() const;
};
CHECK_SIZEOF(CErrorOutputWindow, 0x20)

#endif // _CERROROUTPUTWINDOW
