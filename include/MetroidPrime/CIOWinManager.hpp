#ifndef _CIOWINMANAGER
#define _CIOWINMANAGER

#include "types.h"

#include "MetroidPrime/CArchitectureQueue.hpp"

#include "rstl/list.hpp"
#include "rstl/rc_ptr.hpp"

class CIOWin;

class CIOWinManager {
public:
  struct IOWinPQNode {
    rstl::rc_ptr<CIOWin> x0_iowin;
    int x8_prio;
    IOWinPQNode* xc_next;
    
    IOWinPQNode(rstl::ncrc_ptr<CIOWin> iowin, int prio, IOWinPQNode* next);
    /// The node owns its `x0_iowin`, so the class is not trivially destructible: retail's
    /// `RemoveIOWin` (0x80049A98) releases the node's own copy and *then* frees the node, with
    /// the three dead `cmplwi`/`beq` tests mwcceppc emits in front of an implicit member
    /// destructor. Defined in `src/MetroidPrime/CIOWinManagerRemoveIOWin.cpp`.
    ~IOWinPQNode();
    
    rstl::ncrc_ptr<CIOWin> GetIOWin() const;
  };
  CIOWinManager();
  ~CIOWinManager();

  void Draw() const;
  void AddIOWin(rstl::ncrc_ptr< CIOWin >, int, int);
  // By **const reference**, not by value: retail's `RemoveAllIOWins` builds a stack `rc_ptr` and
  // passes its address (`mr r3,this ; addi r4,r1,16 ; bl fn_80049A98`), and `RemoveIOWin` reads
  // `0(r4)` against the local's `0(r1+16)`. A by-value parameter would be passed as a pointer to a
  // *caller* temporary and the bytes would not match. 0x80049A98, 0x144 bytes, still unnamed.
  void RemoveIOWin(const rstl::rc_ptr<CIOWin>& chIow);
  void RemoveAllIOWins();
  void ChangeIOWinPriority(rstl::ncrc_ptr<CIOWin> toChange, int pumpPrio, int drawPrio);
  rstl::ncrc_ptr<CIOWin> FindIOWin(const char* name);
  
  void PumpMessages(CArchitectureQueue& queue);
  bool DistributeOneMessage(const CArchitectureMessage& msg, CArchitectureQueue& queue);
  bool OnIOWinMessage(const CArchitectureMessage& msg);

  inline bool IsEmpty() const { return x4_pumpRoot == nullptr && x0_drawRoot == nullptr; }

private:
  // `x0_drawRoot` and `x4_pumpRoot` are plain `IOWinPQNode*`, four bytes each - retail's
  // `RemoveAllIOWins` reads `0(this)` and `4(this)` and dereferences them, and `__dt__CIOWinManager`
  // puts the member `CArchitectureQueue` at +8 with a total size of 0x20.
  //
  // The node is 16 bytes, not 12: `AddIOWin` does `li r3,16` before `operator new`, and the
  // constructor at 0x80049D58 stores the priority at +8 and the successor at +0xc. The `x4_`/`x8_`
  // names the header used to carry were read off a 4-byte `rc_ptr` and are wrong; see
  // docs/research/rc_ptr.md.
  IOWinPQNode* x0_drawRoot;
  IOWinPQNode* x4_pumpRoot;
  CArchitectureQueue x8_localGatherQueue;
};
CHECK_SIZEOF(CIOWinManager, 0x20)
// `CHECK_SIZEOF` pastes `cls##_check`, so it cannot be spelled for a nested class - `NESTED_` has
// the same problem. Spelled out.
extern int IOWinPQNode_check[check_sizeof< CIOWinManager::IOWinPQNode, 0x10 >::value];

#endif // _CIOWINMANAGER
