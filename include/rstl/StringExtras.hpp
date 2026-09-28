#ifndef _RSTL_STRINGEXTRAS
#define _RSTL_STRINGEXTRAS

#include "rstl/string.hpp"
#include "rstl/vector.hpp"

class CStringExtras {
public:
  static int IndexOfSubstring(const rstl::string&, const rstl::string&);
  static int CompareCaseInsensitive(const rstl::string&, const rstl::string&);
  static char ConvertToUpperCase(char c);
  static int ConvertToLowerCase(int c);
  static rstl::string ConvertToLowerCase(const rstl::string& str);
  static rstl::string CreatePrefix(const rstl::string& str, int count);
  static rstl::string CreateFromInteger(int v);
  // CreateFromReal and IsSeparator are dead-stripped from the retail DOL, but their literals
  // remain in this unit's string pool and CreateFromInteger's `addi` offset depends on them.
  static rstl::string CreateFromReal(float v, int precision);
  static bool IsSeparator(char c);
  static rstl::string ConvertToANSI(const rstl::wstring& str);
  static rstl::wstring ConvertToUNICODE(const rstl::string& str);
  static rstl::string ReadString(CInputStream& in);
  static rstl::vector<rstl::string> TokenizeString(const rstl::string&, const char*, int);
};

#endif // _RSTL_STRINGEXTRAS
