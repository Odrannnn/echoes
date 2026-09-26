#ifndef _CGAMEAREA
#define _CGAMEAREA

#include "Kyoto/SObjectTag.hpp"

class CScriptAreaProperties;
class CStateManager;
class CGameArea {
public:
  enum EOcclusionState { kOS_Occluded, kOS_Visible };

  struct CPostConstructed {
    char pad1[0x138];
    // Named because `CGameArea::SetAreaAttributes` writes it; see
    // src/MetroidPrime/CGameAreaSetAreaAttributes.cpp. The size of the struct is unchanged
    // (0x140) and `m_occlusionState` is still at 0x13c.
    CScriptAreaProperties* x138_areaProperties; // 0x138
    EOcclusionState m_occlusionState;           // 0x13c

    CPostConstructed();
    ~CPostConstructed();
  };

  int GetPhase() const { return m_phase; }
  CAssetId GetAreaAssetId() const { return m_areaAssetId; }

  void SetAreaAttributes(CScriptAreaProperties*);
  bool IsLoaded() const { return m_phase == 0x10; }

  bool TryTakingOutOfARAM();
  EOcclusionState GetOcclusionState() const { return m_postConstructed->m_occlusionState; }
  CGameArea* GetNext() const { return m_next; }

  bool fn_80057550() const;
  void fn_800575BC(CStateManager& mgr);

  class CChainIterator {
  protected:
    CGameArea* m_area;

  public:
    CChainIterator() : m_area(nullptr) {}
    explicit CChainIterator(CGameArea* area) : m_area(area) {}
    CGameArea& operator*() const { return *m_area; }
    CGameArea* operator->() const { return m_area; }
    CChainIterator& operator++() {
      m_area = m_area->GetNext();
      return *this;
    }
    bool operator!=(const CChainIterator& other) const { return other.m_area != m_area; }
    bool operator==(const CChainIterator& other) const { return m_area == other.m_area; }
  };

  class CConstChainIterator : protected CChainIterator {
  public:
    CConstChainIterator() {}
    explicit CConstChainIterator(const CGameArea* area)
    : CChainIterator(const_cast< CGameArea* >(area)) {}
    const CGameArea& operator*() const { return CChainIterator::operator*(); }
    const CGameArea* operator->() const { return CChainIterator::operator->(); }
    CConstChainIterator& operator++() {
      CChainIterator::operator++();
      return *this;
    }
    bool operator!=(const CConstChainIterator& other) const {
      return !CChainIterator::operator==(other);
    }
    bool operator==(const CConstChainIterator& other) const {
      return CChainIterator::operator==(other);
    }
  };

private:
  char pad1[0x54];
  CAssetId m_areaAssetId;             // 0x54
  char pad2[0x9c];
  int m_phase;                         // 0xf4;
  CGameArea* m_next;                   // 0xf8
  CGameArea* m_prev;                   // 0xfc
  int m_curChain;                      // 0x100  (shouled be EChain)
  CPostConstructed* m_postConstructed; // 0x104
  int m_flagsActive;                   // 0x108
};

#endif // _CGAMEAREA
