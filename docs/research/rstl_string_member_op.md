# `rstl::basic_string` needs a **const member** `operator+(const char*)`, and adding it is a
# global change — measured, with the blast radius

## The gap

Retail has a member `operator+` on `rstl::basic_string` that this tree does not:

```
config/G2ME01/symbols.txt:604
__pl__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>FPCc
  = .text:0x80021634; // type:function size:0x60
```

**Correction, measured by lane g3 after I first wrote this file.** I read the `C` in
`...>FPCc` as a const-member marker. It is not: it is the `C` of the **`PCc` parameter**, and
**`F` immediately after the return type is the member-function marker.** So the spelling does not
distinguish a const member from a non-const one at all, which is why the shape has to be pinned by
compiling rather than read off the name.

g3's own limit, recorded so nobody inherits it as a conclusion: **MWCC rejects the out-of-line
template definition syntax**, so it could not produce a compile that separates the const and
non-const member cases, and does not claim that question is settled. What *is* established is the
narrower and sufficient fact: **the free function cannot produce retail's name**, and
`build/G2ME01/src/Kyoto/CResLoaderAddPakFileAsync.o` carries
`U __pl__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>FPCc` against
retail's definition at 0x80021634.

This tree has, at `include/rstl/string.hpp:344` onward, a set of **free** functions:

```cpp
string operator+(const string& a, const string& b);          // declared, defined in rstl_strings.cpp
inline wstring operator+(const wstring& a, const wstring& b);
inline string  operator+(const string& a, char c);
inline string  operator+(const string& a, const char* c);   // <-- the one that collides
```

A free `rstl::operator+(const string&, const char*)` mangles to
`__pl__4rstlFRCQ24rstl66basic_string<...>PCc` — a different name from retail's. A probe compile
therefore emits it as a **weak local copy**, and a call to it never reaches retail's address at
0x80021634. That is not a theoretical difference: it is why
`src/Kyoto/CResLoaderAddPakFileAsync.cpp` could not be made to match until the workaround below.

## What the unit in question did, and why it is a workaround

`src/Kyoto/CResLoaderAddPakFileAsync.cpp` gives `rstl/string.hpp` a `public:` it does not otherwise
have — chosen because it is the only token in that header occurring exactly once — and uses it to
declare the member for itself, with the `__MWERKS__` branch left untouched. **Its own comment says
the real fix is one declaration in `include/rstl/string.hpp`.** That is right, and this file is
where the rest of the story belongs.

## The measurement that decides how to do it

`0x80021634` is **unclaimed** — no entry in `config/G2ME01/splits.txt` covers it, so a new unit can
take it. That part is easy.

The part that is not easy is the **blast radius**, and it is the reason this has not simply been
done. Declaring

```cpp
string operator+(const char* c) const;
```

as a member of `rstl::basic_string` changes overload resolution at **every** `a + "literal"` in the
tree, not only in the one function that needs it. Today those bind to the inline free function at
`string.hpp:357`; afterwards they bind to the member, which is a different symbol and a different
emission. So this is a global change of exactly the kind lane f1 measured when `rc_ptr` changed
size: scores across many units will move, and the direction is not knowable in advance.

`rstl::basic_string` is included almost everywhere, so the affected set is close to the whole tree.

## The options, and the recommendation

1. **Add the member, declared in the header, defined in a new unit claiming 0x80021634..0x80021694.**
   Correct, and the only option that serves both builds — an `extern "C"`-style name match means one
   definition serves the decompilation and the port. **Costs a full-tree measurement**, and the
   per-function diff is the only thing that will see it. `docs/RUNNING_THE_DECOMP.md` records the
   precedent and the shape of the report that made f1's change reviewable.
2. **Leave the header alone and keep the per-unit workaround.** Zero blast radius, and it has
   already produced one 100% `Matching` unit. **The cost is that every future unit that wants the
   member has to repeat the `public:` hack**, and each repetition is a place the workaround can
   differ from the real declaration.
3. **Add the member but keep the free function too.** Rejected: two candidates for one call is
   exactly the ambiguity that makes a later measurement untrustworthy, and overload resolution
   between them is not something to leave to chance.

**Recommendation: option 1, as its own piece of work, with the unit-count report f1 produced for
`rc_ptr`.** It is the last known systematic mismatch in `rstl`, it has a measured unclaimed range
waiting for it, and one unit is already held back by the workaround.

## Probes that must be run before writing the declaration

Because the const marker is not visible in MWCC's spelling, the shape has to be pinned by compiling,
not by reading:

- `S operator+(const char*);` — non-const, by value. This is the spelling that mangles to retail's
  name, per the unit's own measurement.
- `static operator+` is **illegal in mwcceppc** — it is a compile error, not a codegen difference.
- Adding a `const` to the member changes the mangled name; if the probe's symbol no longer matches
  `__pl__Q24rstl66basic_string<...>FPCc`, the declaration is wrong regardless of how it compiles.
- Read the emitted symbol with `build/binutils/powerpc-eabi-nm` on **our own object**. Do not retype
  it from the retail name — that is how a wrong symbol ships.

## What this is worth

One function, 0x60 bytes, and the difference between a header that matches retail's interface and
one that does not. It is on the pak-loading path (`AddPakFileAsync` needs it) and it is a permanent
tax on every unit that does, until it is done properly.
