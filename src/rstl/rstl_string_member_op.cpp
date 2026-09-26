/**
 * rstl::basic_string::operator+(const char*) - the member, not a free function.
 *
 * 0x80021634, 0x60 bytes. Retail's symbol is
 *
 *   __pl__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>FPCc
 *
 * and this tree had only the free `rstl::operator+(const string&, const char*)`, which mangles
 * to `__pl__4rstlFRCQ24rstl66basic_string<...>PCc` - a different name. The compiler therefore
 * emitted it as a weak local copy in every object that used it, and the call never reached
 * retail. The free function is deleted; the declaration is in `include/rstl/string.hpp`.
 *
 * The shape was measured, and the way to measure it is the non-obvious part. Declared `const`
 * the member mangles to `...rmemory_allocator>CFPCc`; declared non-const, to
 * `...rmemory_allocator>FPCc`, which is retail's name exactly. So **the `C` is the const-member
 * marker and it sits between the template-id's closing `>` and the `F`**, and retail's is the
 * non-const member. That needed only a declaration plus `nm` on an *undefined* symbol - not the
 * out-of-line template definition, which mwcceppc rejects outright and which is what previously
 * made this look unreachable. An explicit specialization uses different syntax and does compile,
 * which is what this file is.
 */

#include "rstl/string.hpp"

template <>
rstl::basic_string< char > rstl::basic_string< char >::operator+(const char* b) {
  rstl::basic_string< char > result(*this);
  result.append(b, -1);
  return result;
}
