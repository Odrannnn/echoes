# carve-8025dba4 — `Weapons/Carve8025DBA4`, flip PASS

`kind: match`, target `Weapons/Carve8025DBA4`. **PASS**, judged by `tools/goal_check.sh
build/goal/item.json` after the change: gate clean, `matched 13514 -> 13516`,
`linked 6562 -> 6564`, `check_symbol_names.py` clean, `All: 37.59% fuzzy, 31.03% matched,
13.89% linked (13516 / 28465 functions)`, and `flip_test Weapons/Carve8025DBA4.cpp: PASS,
Object(Matching) in configure.py`.

## What the claim is

`.text 0x8025DBA4..0x8025DC80`, 0xDC = 220 bytes, 2 functions, carved out of dtk's
`auto_03_8025CA80_text` (0x8025CA80..0x8025DBA4):

| function | addr | size | retail name | twin |
|---|---|---|---|---|
| `fn_8025DBA4` | 0x8025DBA4 | 0xB0 = 176 B, 44 insns | unnamed | `__ct<13CSkinnedModel>__16CFactoryFnReturnFP13CSkinnedModel` (0x80031950, 0xB0) |
| `fn_8025DC54` | 0x8025DC54 | 0x2C = 44 B, 11 insns | unnamed | `GetIObjObjectFor__23TToken<13CSkinnedModel>FRCQ24rstl25auto_ptr<13CSki` (0x80031A00, 0x2C) |

Addresses and sizes from `config/G2ME01/symbols.txt:10614-10615`. Instructions from
`build/G2ME01/asm/auto_03_8025CA80_text.s:1251-1314` (the range is now
`build/G2ME01/asm/Weapons/Carve8025DBA4.s`).

Byte evidence is the **pristine disc**, not our own build — `python3 tools/dol_read.py
0x8025DBA4 0xDC orig/G2ME01/sys/main.dol`. I also wrote a small checker for this run,
`.tmp/opencode/doldiff.py`, which reads the disc's `u32:` line and masks each branch's
24-bit displacement (each object resolves those differently: ours into its own `.text`,
retail's into the DOL). Result on the committed object:

```
python3 .tmp/opencode/doldiff.py 8025DBA4 DC build/G2ME01/src/Weapons/Carve8025DBA4.o
retail 55 words / ours 55 words
differing words (branch displacements masked): 0 -> BYTE-EXACT
```

`tools/carve_diff.sh` cannot decide this one — it compares branch *displacements* too, and
those are relocations, so it always reports a difference at each `bl`. That is a limitation
of the tool for any unit with calls, not a finding about this change.

`tools/unit_fit.sh Weapons/Carve8025DBA4.cpp`: `.text claimed 220 ours 220 retail 220
fits`, and **no extra functions: our object defines only what the retail unit object does**.

## What the two functions are, and the evidence for it

Read from retail's own bytes, not guessed.

**`fn_8025DBA4` is `CFactoryFnReturn<T>::CFactoryFnReturn(T*)`.** Its one caller,
`FProjectileWeaponDataFactory__FRC10SObjectTagR12CInputStreamRC15CVParamTransfer`
(0x8025DB38, 0x6C, `symbols.txt:10613`, `auto_03_8025CA80_text.s:1218-1249`), does
`mr r3,r31 / mr r4,r0` then `bl fn_8025DBA4` at 0x8025DB84 — so **r3 is a class returned by
value and r4 is one pointer**. The callee parks r3 in r31 (`mr r31,r3`, 0x8025DBC0) and
returns it (`mr r3,r31`, 0x8025DC40), which is the MWCC hidden-return-pointer shape for a
member function. The member it initialises is the two-word `rstl::auto_ptr< IObj >` at
`include/Kyoto/CFactoryMgr.hpp:24`, and retail's own source for this constructor is in this
tree at `include/Kyoto/CFactoryMgr.hpp:19`.

**`fn_8025DC54` is `TToken<T>::GetIObjObjectFor`.** Retail's whole source for it is
`include/Kyoto/TToken.hpp:24-27`: one `return TObjOwnerDerivedFromIObj< T >::GetNewDerivedObject(obj);`.
The caller passes `addi r3,r1,0x8 / addi r4,r1,0x10` before its `bl` at 0x8025DBD4 — an
8-byte class return in r3 and one 8-byte **by-const-reference** argument in r4, the argument
being the `rstl::auto_ptr<T>` temporary built from the incoming pointer at
`stb r0,0x10(r1)` / `stw r4,0x14(r1)` (0x8025DBD0, 0x8025DBC8). Both that and the incoming
`T*` are therefore the two-word `{ bool mHas; T* mItem; }` of `include/rstl/auto_ptr.hpp:15-16`,
which is what the `neg r0,r4 / or / srwi` at the top of `fn_8025DBA4` (0x8025DBB0..0x8025DBBC)
is: `ptr != nullptr` being stored into `mHas`.

**The declared-but-unclaimed callee `fn_8025DC80` is `GetNewDerivedObject`.** 0x8025DC80,
0x9C, `symbols.txt:10616` (now `build/G2ME01/asm/auto_03_8025DC80_text.s`): `li r3,0x8 /
bl __nw__FUlPCcPCc`, then the three `stw r0,0x0(r3)` of `__vt__4IObj` /
`__vt__31CObjOwnerDerivedFromIObjUntyped` / `lbl_803B8B40`, then `stb r5,0x0(r31)` releasing
the source's +0 flag and `stw r4,0x4(r3)` copying its +4 pointer. Byte for byte
`GetNewDerivedObject__50TObjOwnerDerivedFromIObj<22CCollisionResponseData>F...` at 0x8025DF24
(`symbols.txt:10621`, also 0x9C) and the twin at 0x80031A2C — hence the same return type, the
class `rstl::auto_ptr` by value. It is **not claimed here**: this claim stops at 0x8025DC80
and it is the next unsourced function of the same run, so the DOL link takes it from dtk's own
object of the surrounding run. This unit claims `.text` and nothing else.

**The two teardown blocks, distinguished by `bl` vs `bctrl`.** 0x8025DC10 is `lwz r12,0x0(r3)`
(vtable), `li r4,0x1` (deleting flag), `lwz r12,0x8(r12)`, `mtctr`, `bctrl` — the returned
`rstl::auto_ptr`'s `if (mHas) { delete mItem; }` from `include/rstl/auto_ptr.hpp:21-25`, and a
**virtual** delete because the pointee derives from the polymorphic `IObj`
(`include/Kyoto/IObj.hpp:13-14`, `:36-39`). The other is the **static** `bl fn_8026258C` at
0x8025DC38: the same `delete` on the argument's `T*`, whose destructor is not virtual.
`fn_8026258C` (0x8026258C, `symbols.txt:10699`, size 0x500) is unclaimed and declared, never
defined.

## What the spellings were, and why each is forced (all measured this run)

This is the part worth keeping. The twin bodies are `rstl::auto_ptr` shapes, and the obvious
spelling — `#include "rstl/auto_ptr.hpp"` and let the compiler write the two destructor
blocks — compiles **byte-exact and wrong**: `powerpc-eabi-nm` on the object then shows

```
000000b0 W __dt__9SOwnedArgFv
00000114 W __dt__Q24rstl32auto_ptr<20SOwnedProjectileData>Fv
```

i.e. 0xD4 bytes of out-of-line weak destructor copies that retail does not have in this
range. That is exactly what `tools/unit_fit.sh` exists to catch, and `unit_fit.sh` does fail
it. So the destructor bodies are written out as the two `if`s at the end of `fn_8025DBA4`, in
the order the frame destroys them, and the classes carry **no destructor**.

Three further spellings are forced by bytes, not preference. Each was found by compiling a
candidate and reading `carve_diff.sh` / `nm`; the numbers below are from those runs.

1. **`SOwnedRes::release()` is `const` and `mHas` is `mutable`**, exactly as
   `include/rstl/auto_ptr.hpp:15` and `:46-49` are. Retail **stores** the flag through the
   object it is about to release (`stb r0,0x8(r1)`, 0x8025DBE0) and then **reloads** it
   (`lbz r0,0x8(r1)`, 0x8025DBF8) instead of folding the load to the constant it just wrote.
   A non-`const` `release()` with a tracked local emits the **41-instruction** body, dropping
   both instructions (0xAC bytes, 3 instructions short). The `const`+`mutable` pair is what
   makes the reload survive.
2. **`fn_8025DC54`'s result is bound to a `const&`**, not copied into a named local. That is
   retail's `return ...;` eliding the temporary's own copy, and it keeps the sret slot at
   `r1+0x8`. A named `SOwnedRes res = fn_8025DC54(src);` puts the sret at
   `addi r3,r1,0x10` (0x8025DBC4 becomes `addi r3,r1,16`) and drops the `lwz r3,0xc(r1)`.
3. **`SOwnedRes` declares a copy constructor.** `rstl::auto_ptr` has one
   (`include/rstl/auto_ptr.hpp:27-29`) and it is what makes the class non-POD, which is what
   decides the sret convention: a plain 8-byte C struct comes back in r3:r4 with no r31 at
   all — measured, 8 instructions instead of retail's 11. Same finding as
   `src/MetroidPrime/Carve801EF730.cpp:99-108` records for `fn_801EF784`.
4. **`fn_8025DBA4` tests `if (out->mHas)` and then a bare `delete`, with no `&& mItem != 0`.**
   Retail does have a null check, but as the `cmplwi r3,0 / beq` the compiler writes for
   `delete` itself (0x8025DC08, 0x8025DC0C). Adding the test by hand emits a **second** `beq`
   — measured, 56 instructions against retail's 55.

Dead ends, so the next run does not repeat them: a plain `struct` (no class, no copy ctor)
for the result gives the 8-instruction register-return shape; a class **with** a destructor
gives the `__dt__` copies; `volatile bool mHas` makes it 52 differing instructions; a
`SOwnedRes*` non-`const` out-parameter is rejected by mwcceppc outright
(`illegal implicit conversion from 'SOwnedRes'`). `auto_ptr`'s own `release()` and the
hand-written `release()` are the same 2 instructions in isolation — the difference is only
whether the compiler can track the object.

## The four carve files, all present and in address order

| file | change |
|---|---|
| `src/Weapons/Carve8025DBA4.cpp` | new, 264 lines, `.cpp` with `extern "C"` (see below) |
| `config/G2ME01/splits.txt:2023-2024` | `Weapons/Carve8025DBA4.cpp: .text start:0x8025DBA4 end:0x8025DC80`, between `Weapons/CProjectileWeapon.cpp` and `Weapons/CCollisionResponseData.cpp` |
| `configure.py:632` | `Object(Matching, "Weapons/Carve8025DBA4.cpp")`, same position |
| `files.cmake:871` | `src/Weapons/Carve8025DBA4.cpp`, after the `src/WorldFormat/Carve*` block |

`tools/check_decl_order.py`: `ok: 1208 unit(s) checked, 37 permuted, all 37 accounted for
in decl_order.md`. Definitions are descending by address — `fn_8025DBA4` (0x8025DBA4) then
`fn_8025DC54` (0x8025DC54) — which is what mwcceppc needs to emit them in retail order.

**It is a `.cpp` with `extern "C"`, not a `.c`.** The seed said plain C "so the fn_ names do
not mangle" and the concern is right, but point 3 above is the whole of the second function's
shape: sret is a class convention. The `extern "C"` wrapper is what keeps the names verbatim,
and it is the arrangement `src/MetroidPrime/Carve801EF730.cpp:99-108` and
`src/MetroidPrime/Player/CGameStateBlockCopyCtor.cpp:34` already use for Matching carves of
this shape.

**The unit is `.cpp` in `configure.py` and `.cpp` in `files.cmake`.** No `PortLinkStubs.cpp`
duplicate: the port-only stand-ins for the two unclaimed callees are at the bottom of the
carve itself under `#ifndef __MWERKS__` (the guard `src/MetroidPrime/Carve801EF730.cpp:197-215`
uses — the matching build must take both symbols from dtk's objects, and a second definition
there would be the duplicate `tools/gate.sh`'s `port link dups` step exists to catch). I did
**not** put them in `PortLinkStubs.cpp`: that file is being rewritten by other lanes in this
same goal run, and a concurrent edit there is a merge conflict over an announced stand-in
nobody has reviewed. Both stand-ins are announced as empty bodies in the carve's own header,
and both are unreachable in the port today — the only thing that reaches these bodies is
`FProjectileWeaponDataFactory`, itself inside the unclaimed `auto_03_8025CA80_text` with no
claimed unit and no vtable entry.

## The link-order shape, checked rather than assumed

`docs/RUNNING_THE_DECOMP.md` "The carve vein" records that a carve can create a `dtk dol
split` link-order cycle **when the run it is carved out of starts exactly where the previous
unit's `.text` ends** — that is this claim's shape: `auto_03_8025CA80_text` starts at
0x8025CA80 and `Weapons/CProjectileWeapon.cpp` ends at 0x8025CA80. It does not bite here:
`decomp_build.sh -r` reconfigured, re-split and re-linked with no cycle, and
`sha1sum build/G2ME01/main.dol` is the pinned `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`
(gate step `ninja + build.sha1`: ok, and `87 files OK` for all 86 RELs).

So proximity to an existing unit boundary is a risk to measure, not a rule that forbids the
carve. Recorded because a future lane reading only the warning would reasonably skip a claim
this one shows is fine.

## For the next reader

- `Weapons/Carve8025DBA4.cpp` is a `.cpp` even though every symbol is `fn_`-named. That is
  forced by sret, not a leftover.
- The claim ends at 0x8025DC80, not at 0x8025DD1C. `fn_8025DC80` (0x9C bytes) is the obvious
  next carve — it is `GetNewDerivedObject`, its twin at 0x8025DF24 is retail-named and
  matched, and `Weapons/CCollisionResponseData.cpp` starts at 0x8025DD1C. It would close the
  gap between this claim and the next unit entirely.
- The two classes are written out locally rather than `#include`d. That is because the
  argument's type parameter is a class retail does not name here, and because writing them
  out is what keeps this range free of `__dt__` copies (see above).
