#ifndef _CIOWINMANAGER
#define _CIOWINMANAGER

#include "types.h"

#include "MetroidPrime/CArchitectureQueue.hpp"

#include "rstl/list.hpp"
#include "rstl/rc_ptr.hpp"
#include "rstl/string.hpp"

class CIOWin;

class CIOWinManager {
public:
  struct IOWinPQNode {
    rstl::ncrc_ptr< CIOWin > mIowin;
    int mPrio;
    IOWinPQNode* mNext;

    IOWinPQNode(rstl::ncrc_ptr< CIOWin > iowin, int prio, IOWinPQNode* next);

#ifdef TARGET_PC
    // Port: the node owns its `mIowin`, so the class is not trivially destructible and the retail
    // `RemoveIOWin` walk releases the node's own copy *before* freeing the node. Upstream leaves
    // the destructor implicit, which makes every `delete node` in a matching-build unit expand
    // it at the delete site; the port's split `RemoveIOWin` needs the out-of-line spelling.
    // TARGET_PC only, so `MetroidPrime/CIOWinManager.cpp` - a unit of the matching build - keeps
    // its implicit destructor and the DOL is untouched.
    ~IOWinPQNode();
#endif

    rstl::ncrc_ptr< CIOWin > GetIOWin() const;
    IOWinPQNode* GetNext() const { return mNext; }
    void SetNext(IOWinPQNode* next) { mNext = next; }
    int GetPriority() const { return mPrio; }
    void SetPriority(int prio) { mPrio = prio; }
  };

  CIOWinManager();
  ~CIOWinManager();

  void Draw() const;
  void AddIOWin(rstl::ncrc_ptr< CIOWin >, int, int);
  void RemoveIOWin(rstl::ncrc_ptr< CIOWin > chIow);
#ifdef TARGET_PC
  // Port: retail's `RemoveIOWin` (0x80049A98) receives the address of the caller's 8-byte
  // `rc_ptr` temporary (`mr r3,this ; addi r4,r1,16 ; bl`) and reads `0(r4)` on every iteration,
  // so the port's split file takes it **by const reference**. Upstream's by-value overload above
  // is a distinct symbol and is what `MetroidPrime/CIOWinManager.cpp` still calls, so nothing in
  // the matching build changes. TARGET_PC only for the same reason as `~IOWinPQNode` above.
  void RemoveIOWin(const rstl::rc_ptr< CIOWin >& chIow);
#endif
  void RemoveAllIOWins();
  void ChangeIOWinPriority(rstl::ncrc_ptr< CIOWin > toChange, int pumpPrio, int drawPrio);
  rstl::ncrc_ptr< CIOWin > FindIOWin(const rstl::string& name) const;

  void PumpMessages(CArchitectureQueue& queue);
  bool DistributeOneMessage(const CArchitectureMessage& msg, CArchitectureQueue& queue);
  bool OnIOWinMessage(const CArchitectureMessage& msg);

  inline bool IsEmpty() const { return mPumpRoot == nullptr && mDrawRoot == nullptr; }

private:
  IOWinPQNode* mDrawRoot;
  IOWinPQNode* mPumpRoot;
  CArchitectureQueue mLocalGatherQueue;
};
CHECK_SIZEOF(CIOWinManager, 0x20)

#endif // _CIOWINMANAGER
