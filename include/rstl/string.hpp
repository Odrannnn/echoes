#ifndef _RSTL_STRING
#define _RSTL_STRING

#include "types.h"

#include "rstl/rmemory_allocator.hpp"
#include "rstl/linear_iterator.hpp"
#include "rstl/pair.hpp"

class CInputStream;
class COutputStream;

namespace rstl {
template < typename _CharTp >
struct char_traits {
  static void copy(_CharTp* out, const _CharTp* in, int count) {
    for (int i = 0; i < count; ++i) {
      out[i] = in[i];
    }
  }

  static void assign(_CharTp& out, const _CharTp& value) { out = value; }

  static void assign(_CharTp* out, int count, const _CharTp& value) {
    for (int i = 0; i < count; ++i) {
      out[i] = value;
    }
  }

  static bool eq(const _CharTp& lhs, const _CharTp& rhs) { return lhs == rhs; }
  static _CharTp eos() { return 0; }
  static int compare(const _CharTp& lhs, const _CharTp& rhs) {
    return static_cast< int >(lhs) - static_cast< int >(rhs);
  }
};

template <>
struct char_traits< char > {
  static void copy(char* out, const char* in, int count) {
    for (int i = 0; i < count; ++i) {
      out[i] = in[i];
    }
  }

  static void assign(char& out, const char& value) { out = value; }

  static void assign(char* out, int count, const char& value) {
    for (int i = 0; i < count; ++i) {
      out[i] = value;
    }
  }

  static bool eq(const char& lhs, const char& rhs) { return lhs == rhs; }
  static char eos() { return 0; }
  static int compare(const char& lhs, const char& rhs) {
    return static_cast< int >(static_cast< signed char >(lhs)) -
           static_cast< int >(static_cast< signed char >(rhs));
  }
};

/**
 * The traits argument of retail's case-insensitive `char` string - `rstl::istring`. Only the
 * comparison is case-insensitive: `copy` and `assign` are the same code as `char_traits<char>`'s,
 * because retail folds the case in `compare` rather than when the characters are stored (the
 * `istring` in `CTextParser::GetImage` still compares equal to its own `mPtr` bytes).
 *
 * The lower-casing is `fn_8016BED0`, and it tests two ranges on the **sign-extended** byte:
 * `['a','z']` and `[0xE0,0xFE]`. The second is unreachable there - `extsb` leaves the value in
 * `[-128,127]` - so retail's effective fold is `['a','z']` only, and that is what this writes.
 */
template < typename _CharTp >
struct case_insensitive_char_traits : public char_traits< _CharTp > {};

template <>
struct case_insensitive_char_traits< char > {
  static void copy(char* out, const char* in, int count) {
    char_traits< char >::copy(out, in, count);
  }

  static void assign(char& out, const char& value) { char_traits< char >::assign(out, value); }

  static void assign(char* out, int count, const char& value) {
    char_traits< char >::assign(out, count, value);
  }

  static int lower(char c) {
    int value = static_cast< int >(static_cast< signed char >(c));
    if (value >= 'a' && value <= 'z') {
      value -= 32;
    }
    return value;
  }

  static bool eq(const char& lhs, const char& rhs) { return lower(lhs) == lower(rhs); }
  static char eos() { return 0; }
  static int compare(const char& lhs, const char& rhs) { return lower(lhs) - lower(rhs); }
};

template < typename _CharTp, typename Traits = char_traits< _CharTp >,
           typename Alloc = rmemory_allocator >
class basic_string {
  struct control {
    int mCapacity;
    int mRefCount;
  };

  const _CharTp* mPtr;
  control* mCow;
  uint mSize;
  Alloc mAllocator;

  void internal_prepare_to_write(int len, bool);
  void internal_allocate(int size);

  void internal_dereference();
  void internal_reference() {
    if (mCow) {
      ++mCow->mRefCount;
    }
  }

  template < typename It >
  static pair< It, int > compute_length(It data, int count) {
    It end = data;
    int len = 0;
    while ((count == -1 || len < count) && !Traits::eq(*end, Traits::eos())) {
      ++end;
      ++len;
    }
    return pair< It, int >(end, len);
  }

  static _CharTp mNull;

public:
  typedef const_linear_iterator< _CharTp, basic_string, Alloc > const_iterator;

  struct literal_t {};

  basic_string() : mPtr(&mNull), mCow(nullptr), mSize(0) {}

  //!< Puts the object in the state `basic_string()`'s own constructor produces, without
  //!< constructing: `mPtr = &mNull`, `mCow = nullptr`, `mSize = 0`.
  //!
  //!< Public, and it exists for exactly one caller: `src/MetroidPrime/CWorldStateCtor.cpp`, which
  //!< has to write those three words at a fixed offset in a `CWorldState` and **cannot** spell them
  //!< as the member's construction, because **mwcceppc deletes the construction of a class member
  //!< this translation unit never reads**. Measured, all of these dropping the three stores: the
  //!< member as `rstl::string`; a three-word struct with a user-provided constructor, with and
  //!< without a non-trivial destructor; the same from a mem-init list; and `x = rstl::string()`.
  //!< What survives is writing the three words from the constructor's body, which needs the three
  //!< private members - so a member function is the only route. A `friend` is not available:
  //!< **mwcceppc rejects every friend function declaration inside this class template** ("illegal
  //!< function definition", for `void f(T*)`, `void f(T*, int)`, `void f(void*)`, `void f()` and a
  //!< typedef'd return type alike), and a scoped `#define private public` does not help either
  //!< because `basic_string` has no explicit `private:` - it relies on the class default. Note also
  //!< that writing `&rstl::string::mNull` from outside, with `mNull` made public for it, emits **no
  //!< relocation at all** and stores 0.
  //!
  //!< Additive: it adds no member, moves nothing and is called from nowhere else, so the matching
  //!< GameCube build is unaffected.
  void SetEmpty() { mPtr = &mNull; mCow = nullptr; mSize = 0; }

  basic_string(literal_t, const _CharTp* data) {
    mPtr = data;
    mCow = nullptr;

    const _CharTp* it = data;
    while (*it)
      ++it;

    mSize = static_cast< uint >(it - data);
  }

  basic_string(const basic_string& str);

  basic_string(CInputStream& in, const Alloc& = rmemory_allocator());

  template < typename It >
  basic_string(It first, It last, const Alloc& = rmemory_allocator()) {
    const int len = rstl::distance(first, last);
    internal_allocate(len + 1);
    int i = 0;
    for (It it = first; it != last; it = it + 1, ++i) {
      const_cast< _CharTp& >(mPtr[i]) = *it;
    }
    const_cast< _CharTp& >(mPtr[i]) = Traits::eos();
    mSize = len;
  }

  basic_string(const _CharTp* data, int size = -1, const Alloc& = rmemory_allocator());

  ~basic_string() { internal_dereference(); }

  size_t size() const { return mSize; }
  int length() const { return mSize; }
  int refcount() { return mCow != nullptr ? mCow->mRefCount : -1; }
  void reserve(int len) { internal_prepare_to_write(len, true); }

  basic_string& assign(const basic_string&);
  basic_string& assign(const _CharTp*, int);
  basic_string& operator=(const basic_string& other) {
    assign(other);
    return *this;
  }
  basic_string& append(const basic_string& other);
  basic_string& append(int, _CharTp);
  basic_string& append(const _CharTp*, int);

  int compare(const _CharTp* rhs, int count = -1) const;
  const _CharTp& operator[](int idx) const { return mPtr[idx]; }
  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, size()); }

  template < typename It >
  static int internal_compare(const_iterator first, const_iterator last, It otherFirst,
                              It otherLast);
  template < typename It, typename OtherIt >
  static int internal_search(It first, It last, OtherIt otherFirst, OtherIt otherLast);
  template < typename It, typename OtherIt >
  static int internal_search_of(It first, It last, OtherIt otherFirst, OtherIt otherLast);
  int compare(const basic_string& other) const;
  bool operator==(const basic_string& other) const;
  bool operator!=(const basic_string& other) const;

  int find(const basic_string& other, int pos = 0) const;
  int find(_CharTp ch, int pos = 0) const;
  int find_first_of(const basic_string& other, int pos = 0) const;
  const_iterator position_iterator(int pos) const;
  pair< const_iterator, const_iterator > range_iterator(int pos, int count) const;
  basic_string substr(int pos = 0, int count = -1) const;
  int get_real_pos_for_begin(int pos) const {
    if (pos == -1 || pos >= static_cast< int >(size())) {
      return size();
    }
    return pos;
  }
  const _CharTp* c_str() const { return mPtr; }
  const _CharTp* data() const { return mPtr; }
  void PutTo(COutputStream& out) const;
  const _CharTp at(int idx) const { return data()[idx]; }
};

#ifdef TARGET_PC
// Port: declare explicit member specializations before use; clang otherwise
// instantiates them before their definitions in rstl_strings.cpp.
template <> basic_string< char >::basic_string(const basic_string< char >& other);
template <> basic_string< char >::basic_string(const char* data, int count, const rmemory_allocator& alloc);
template <> basic_string< char >& basic_string< char >::append(const basic_string< char >& other);
template <> basic_string< char >& basic_string< char >::append(const char* data, int count);
template <> basic_string< char >& basic_string< char >::append(int count, char value);
template <> basic_string< char >& basic_string< char >::assign(const basic_string< char >& other);
template <> void basic_string< char >::internal_allocate(int size);
template <> void basic_string< char >::internal_dereference();
template <> void basic_string< char >::internal_prepare_to_write(int len, bool preserve);

template <> basic_string< wchar_t >::basic_string(const basic_string< wchar_t >& other);
template <>
basic_string< wchar_t >& basic_string< wchar_t >::append(const basic_string< wchar_t >& other);
template <> basic_string< wchar_t >& basic_string< wchar_t >::append(const wchar_t* data, int count);
template <> basic_string< wchar_t >& basic_string< wchar_t >::append(int count, wchar_t value);
template <> basic_string< wchar_t >& basic_string< wchar_t >::assign(const basic_string< wchar_t >& other);
template <> basic_string< wchar_t >& basic_string< wchar_t >::assign(const wchar_t* data, int count);
template <> void basic_string< wchar_t >::internal_allocate(int size);
template <> void basic_string< wchar_t >::internal_dereference();
template <> void basic_string< wchar_t >::internal_prepare_to_write(int len, bool preserve);
#endif

template < typename _CharTp, typename Traits, typename Alloc >
template < typename It, typename OtherIt >
inline int basic_string< _CharTp, Traits, Alloc >::internal_search_of(It first, It last,
                                                                      OtherIt otherFirst,
                                                                      OtherIt otherLast) {
  int index = 0;
  for (It it = first; it != last; ++it, ++index) {
    for (OtherIt other = otherFirst; other != otherLast; ++other) {
      if (Traits::eq(*it, *other)) {
        return index;
      }
    }
  }
  return -1;
}

template < typename _CharTp, typename Traits, typename Alloc >
inline int basic_string< _CharTp, Traits, Alloc >::find_first_of(const basic_string& other,
                                                                 int pos) const {
  pos = get_real_pos_for_begin(pos);
  const int found = internal_search_of(begin() + pos, end(), other.begin(), other.end());
  int result = found + pos;
  if (found == -1) {
    result = found;
  }
  return result;
}

template < typename _CharTp, typename Traits, typename Alloc >
template < typename It, typename OtherIt >
inline int basic_string< _CharTp, Traits, Alloc >::internal_search(It first, It last,
                                                                   OtherIt otherFirst,
                                                                   const OtherIt otherLast) {
  if (otherFirst == otherLast) {
    return 0;
  }
  It it = first;
  int matched = 0;
  OtherIt search = otherFirst;
  for (; it != last; ++it) {
    if (Traits::eq(*it, *search)) {
      ++search;
      ++matched;
      if (search == otherLast) {
        return (it - first) - matched + 1;
      }
    } else {
      search = otherFirst;
      matched = 0;
    }
  }
  return -1;
}

template < typename _CharTp, typename Traits, typename Alloc >
int basic_string< _CharTp, Traits, Alloc >::find(const basic_string& other, int pos) const {
  pos = get_real_pos_for_begin(pos);
  const int found = internal_search(begin() + pos, end(), other.begin(), other.end());
  int result = found + pos;
  if (found == -1) {
    result = found;
  }
  return result;
}

template < typename _CharTp, typename Traits, typename Alloc >
int basic_string< _CharTp, Traits, Alloc >::find(_CharTp ch, int pos) const {
  pos = get_real_pos_for_begin(pos);
  const int found = internal_search(begin() + pos, end(), static_cast< const _CharTp* >(&ch),
                                    static_cast< const _CharTp* >(&ch) + 1);
  int result = found + pos;
  if (found == -1) {
    result = found;
  }
  return result;
}

template < typename _CharTp, typename Traits, typename Alloc >
int basic_string< _CharTp, Traits, Alloc >::compare(const _CharTp* rhs, int count) const {
  int rhsCharCount = 0;
  const _CharTp* rhsStart = rhs;
  while ((count == -1 || rhsCharCount < count) && *rhs != '\0') {
    ++rhs;
    ++rhsCharCount;
  }
  return internal_compare(begin(), end(), rhsStart, rhs);
}

template < typename _CharTp, typename Traits, typename Alloc >
template < typename It >
inline int basic_string< _CharTp, Traits, Alloc >::internal_compare(const_iterator first,
                                                                    const_iterator last,
                                                                    It otherFirst, It otherLast) {
  const_iterator it = first;
  It other = otherFirst;
  for (; it != last && other != otherLast; ++it, ++other) {
    int cmp = Traits::compare(*it, *other);
    if (cmp != 0) {
      return cmp;
    }
  }
  if (it == last && other != otherLast) {
    return -1;
  }
  if (it == last) {
    return 0;
  }
  return 1;
}

template < typename _CharTp, typename Traits, typename Alloc >
inline int basic_string< _CharTp, Traits, Alloc >::compare(const basic_string& other) const {
  return internal_compare(begin(), end(), other.begin(), other.end());
}

template < typename _CharTp, typename Traits, typename Alloc >
inline bool basic_string< _CharTp, Traits, Alloc >::operator==(const basic_string& other) const {
  return compare(other) == 0;
}

template < typename _CharTp, typename Traits, typename Alloc >
inline bool basic_string< _CharTp, Traits, Alloc >::operator!=(const basic_string& other) const {
  return compare(other) != 0;
}

typedef basic_string< wchar_t > wstring;
typedef basic_string< char > string;

/**
 * `basic_string<char, case_insensitive_char_traits<char>, rmemory_allocator>` - retail's third
 * `rstl` string. `CTextParser::GetImage` is its only caller and its one use is
 * `fn_802FF3AC("0") == <the tokenised tag>`, to tell a static image from an animated one.
 *
 * Note that `operator==` and `compare` are **members** here (as on every `basic_string`), not the
 * free `rstl::operator==(const string&, const char*)` above: retail emits both out of line for
 * this instantiation - `fn_8016BEA8` is `operator==` and it calls `fn_8016BED0`, which is
 * `compare`. Neither is claimed by any unit yet; they live in an unclaimed `.text` gap
 * (`0x8016BDEC..0x8016C230`) far from `rstl/rstl_string_l.cpp`, so they need a unit of their own.
 */
typedef basic_string< char, case_insensitive_char_traits< char > > istring;

inline bool operator<(const string& lhs, const string& rhs) { return lhs.compare(rhs) < 0; }

inline bool operator==(const string& lhs, const char* rhs) { return lhs.compare(rhs) == 0; }

bool operator==(const char* lhs, const string& rhs);
bool operator!=(const string& lhs, const char* rhs);

wstring wstring_l(const wchar_t* data);

string string_l(const char* data);

// `istring_l` is the third `basic_string`'s `literal_t` constructor, `fn_802FF3AC`. Retail gives
// the instantiation **no symbol of its own** - `symbols.txt` carries the `fn_802FF3AC` placeholder -
// and `CTextParser`'s retail object references that placeholder, so the definition has to reproduce
// the name verbatim or the link loses the symbol when the address is claimed. `extern "C"` is what
// reproduces it from C++; a C++ definition would mangle to `Z<len>fn_802FF3AC...` and objdiff would
// pair nothing. Defined in `src/rstl/rstl_string_l.cpp`.
extern "C" istring fn_802FF3AC(const char* data);

// The readable spelling of the same function, so `GetImage` reads the way retail's source does.
// Retail's own call site is the `fn_802FF3AC` name above; this only forwards to it.
inline istring istring_l(const char* data) { return fn_802FF3AC(data); }

string operator+(const string& a, const string& b);
inline wstring operator+(const wstring& a, const wstring& b) {
  wstring result(a);
  result.append(b);
  return result;
}

inline string operator+(const string& a, char c) {
  string result(a);
  result.append(1, c);
  return result;
}

inline string operator+(const char* a, const string& b) {
  string result(a);
  result.append(b);
  return result;
}

inline string operator+(const string& a, const char* c) {
  string result(a);
  result.append(c, -1);
  return result;
}

static inline wstring operator+(const wstring& a, const wchar_t* c) {
  wstring result(a);
  result.append(c, -1);
  return result;
}

CHECK_SIZEOF(string, 0x10)
} // namespace rstl

#endif // _RSTL_STRING
