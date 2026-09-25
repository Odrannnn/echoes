#ifndef _CMODEL
#define _CMODEL

class CModelFlags;

class CModel {
  static uint sTotalMemory;

public:
  void Touch(int) const;
  void Draw(const CModelFlags&) const;
  bool IsLoaded(int matIdx) const;

  static void AddToTotal(uint amt) { sTotalMemory += amt; }
  static void RemoveFromTotal(uint amt) { sTotalMemory -= amt; }

  // The bound `Touch` walks: `fn_80027AE8` (0x80027AE8) is
  //     for (int i = 0; i < model->x1c_numParts; i++) model->Touch(i);
  // and `fn_80027B44` (0x80027B44) is the same loop for one index. Nothing else in the tree
  // names a member here and nothing includes this header yet, so the offset is the only claim
  // being made; `Touch`'s parameter is the part index.
  char x0_pad[0x1c];
  int x1c_numParts;
};

#endif // _CMODEL
