#ifndef _CGAMESTATEENVVARMANAGER
#define _CGAMESTATEENVVARMANAGER

#include "MetroidPrime/Player/CEnvironmentVariable.hpp"
#include "rstl/map.hpp"
#include "rstl/string.hpp"

// Class name corroborated by the MP2 Wii SEL; method names are inferred.
class CGameStateEnvVarManager {
public:
  // Guessed enum name; these select the system and per-game SAVW variable lists.
  enum EVariableScope { kVS_System, kVS_Game };

  explicit CGameStateEnvVarManager(EVariableScope scope);
  CGameStateEnvVarManager(EVariableScope scope, CBitStreamReader& in);
  CEnvironmentVariable* FindEnvironmentVariable(const char* name);
  void InitializeMemoryState();
  void PutTo(CBitStreamWriter& out) const;

  // Retail 0x80145C98, `fn_80145C98` until 2026-10-01. **Public, not private**: it is the
  // initialiser `CPersistentOptions`'s constructor calls (`CPersistentOptionsCtor.cpp`, which is
  // not a member of this class and so could not reach it privately), and it is the body both
  // constructors below call. With it private and only an unreachable `extern "C"` alias to
  // declare, the port's `_ZN23CGameStateEnvVarManager10LoadFieldsEv` had no definition at all and
  // every construction went to `PortReachStubs.cpp`'s print-a-line stand-in.
  void LoadFields();

private:
  void AddVariable(const rstl::string& name, const CEnvironmentVariable& variable);

  EVariableScope mScope;
  rstl::map< rstl::string, CEnvironmentVariable > mVariables;
};
CHECK_SIZEOF(CGameStateEnvVarManager, 0x18)

#endif // _CGAMESTATEENVVARMANAGER
