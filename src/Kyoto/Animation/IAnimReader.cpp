#include "Kyoto/Animation/IAnimReader.hpp"

IAnimReader::~IAnimReader() {}

rstl::optional_object< rstl::ownership_transfer< IAnimReader > > IAnimReader::VSimplified() {
  return rstl::optional_object_null();
}

// Retail's unnamed 0x802B27A0 is the "remainder only" result builder - Prime 1 spells the
// same code CAdvancementResults::RemainderOnly. dtk has no name for it, so it is spelled with
// the name dtk generates, the same convention as
// src/MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp: objdiff pairs a function with its
// retail counterpart by symbol name only.
extern "C" SAdvancementResults fn_802B27A0(const CCharAnimTime& time) {
  return SAdvancementResults(time);
}

SAdvancementResults IAnimReader::VGetAdvancementResults(const CCharAnimTime& time,
                                                        const CCharAnimTime&) const {
  return fn_802B27A0(time);
}

uint IAnimReader::GetBoolPOIList(const CCharAnimTime& time, CBoolPOINode* listOut, uint capacity,
                                 uint iterator, int additive) const {
  return time.GreaterThanZero() ? VGetBoolPOIList(time, listOut, capacity, iterator, additive) : 0;
}

uint IAnimReader::GetInt32POIList(const CCharAnimTime& time, CInt32POINode* listOut, uint capacity,
                                  uint iterator, int additive) const {
  return time.GreaterThanZero() ? VGetInt32POIList(time, listOut, capacity, iterator, additive) : 0;
}

uint IAnimReader::GetParticlePOIList(const CCharAnimTime& time, CParticlePOINode* listOut,
                                     uint capacity, uint iterator, int additive) const {
  return time.GreaterThanZero() ? VGetParticlePOIList(time, listOut, capacity, iterator, additive)
                                : 0;
}

uint IAnimReader::GetSoundPOIList(const CCharAnimTime& time, CSoundPOINode* listOut, uint capacity,
                                  uint iterator, int additive) const {
  return time.GreaterThanZero() ? VGetSoundPOIList(time, listOut, capacity, iterator, additive) : 0;
}

bool IAnimReader::IsCAnimTreeNode() const { return false; }

void IAnimReader::VGetSegData(const CCharLayoutInfo&, CJointData_LinearStorage&,
                              const CCharAnimTime&) const {}

void IAnimReader::VGetSegData(const CCharLayoutInfo&, CJointData_LinearStorage&) const {}
