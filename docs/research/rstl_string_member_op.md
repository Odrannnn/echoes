# `rstl::basic_string::operator+(const char*)` — the member, and how the shape was pinned

**Status: done.** `src/rstl/rstl_string_member_op.cpp` is a `Matching` DOL unit at
**100.00%, 1/1 functions, 96 bytes of code**, claiming `.text 0x80021634-0x80021694` — a range
no unit had claimed, so the DOL hash could not move. `flip_test.sh` says `PASS -> kept as
Matching`; `main.dol` is `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` and 87/87 objects OK.

This file was written twice. The first version was a proposal that called the change
"tree-wide" and recommended deferring it. **That was wrong by two orders of magnitude**, and
the second version records what the measurement actually showed.

## The gap, and what it cost

Retail defines

```
__pl__Q24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>FPCc
  = .text:0x80021634; // type:function size:0x60
```

This tree had only free `rstl::operator+` overloads. A free
`operator+(const string&, const char*)` mangles to `__pl__4rstlFRCQ24rstl66basic_string<...>PCc`
— a different name — so the compiler emitted it as a **weak local copy** in each object and the
call never reached retail. `src/Kyoto/CResLoaderAddPakFileAsync.cpp` worked around this by
giving `rstl/string.hpp` a `public:` it does not otherwise have and injecting the member
declaration for its own translation unit only. Its own comment said the real fix was one
declaration in the header. That is what this is.

## The shape: `C` is the const marker, and it sits after the template-id's `>`

This is the part that took the measurements, and the way to get them is the transferable bit.

A previous note in this file — mine, and then corrected once already — read the `C` in
`...>FPCc` as the `C` of the `PCc` **parameter**. It is not. Measured, by declaring the member
one way and the other and reading the **undefined** symbol out of the object with `nm`:

| declaration | emitted symbol |
| --- | --- |
| `basic_string operator+(const char* c) const;` | `...rmemory_allocator`C`FPCc` |
| `basic_string operator+(const char* c);` | `...rmemory_allocator`FPCc` — **retail's, exactly** |

So **the `C` is the const-member marker, placed between the template-id's closing `>` and the
`F`**, and retail's is the **non-const** member.

**The way to measure a mangling is a declaration plus `nm` on an undefined symbol.** It is *not*
the out-of-line definition — mwcceppc 2.7 rejects that syntax outright ("declaration syntax
error"), which is what made this look unreachable, and what lane g3 recorded as its limit when it
could not settle the const question. An undefined reference needs no definition at all, so the
obstruction never applies. Two lanes had tried to write the definition and concluded the shape
was unknowable; one `nm` call settles it.

**An explicit specialization does compile**, which is what finally makes the unit writable:

```cpp
template <>
rstl::basic_string< char > rstl::basic_string< char >::operator+(const char* b) { ... }
```

That is different syntax from the rejected out-of-line template definition, and mwcceppc takes it.
Retail's 0x60 bytes are `copy-ctor temp at r1+8` → `append(b, -1)` → copy into the sret slot →
`internal_dereference` on the temp, which is the body written literally.

## The blast radius was 2 call sites, not the tree

The first version of this file claimed that declaring the member "changes overload resolution at
**every** `a + "literal"` in the tree", that `rstl/basic_string` is included almost everywhere,
and that the change should therefore be deferred as its own project with a full unit-movement
report. **Measured: there are exactly two such call sites in the whole tree.**

```
src/MetroidPrime/ScriptObjects/CScriptStreamedMusic.cpp:158   rstl::string(...) + "R.dsp"
src/Kyoto/Graphics/CCubeMoviePlayer.cpp:33                    name + "_pal"
```

The reason the claim looked true is worth keeping: declaring the member *without* removing the
free function makes every one of those sites an **`ambiguous access to overloaded function`**, and
the build stops. That is a loud failure at two sites, not a silent drift across the tree. The fix
is to **delete** the free `operator+(const string&, const char*)` so the member is the only
candidate — and then there is no ambiguity and nothing else to audit.

So the change is: one declaration in the header, one deleted free function, one new `Matching`
unit, and the port-side `#define public` hack in `CResLoaderAddPakFileAsync.cpp` **deleted**, with
the definition moved into the new unit so exactly one copy exists for both builds. Verified
0 duplicate definitions in the port's own link.

## What it did and did not move

| | before | after |
| --- | --- | --- |
| matched functions | 3120 | **3121** |
| DOL sha1 | `6ef9b491…` | `6ef9b491…` (unchanged; the range was unclaimed) |
| port link gap | 490 | 490 |
| port link undefined | 525 | 525 |

**Net on the port link: 0, and that was predicted.** The port's reference is host-mangled, so the
`Matching` unit does not close a port symbol; and `FRuleSetFactory`-style savings of the kind
lane g3 measured do not apply here because nothing referenced the member before. The gain is in
the matching build, and the cost of the old workaround — a `public:` hack that each future unit
would have had to repeat, at a place where it could drift from the real declaration — is gone.

## The lesson

**"This header is included everywhere, so changing it is a global change" is a guess wearing a
measurement's clothes.** The measurement here was one `grep` for `+ "`, and it returned two hits.
The expensive part of the original claim — a full unit-movement report like f1's `rc_ptr` change
— would have been spent proving that a two-site change was safe.
