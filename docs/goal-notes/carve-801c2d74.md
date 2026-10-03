# carve-801c2d74 — `MetroidPrime/Carve801C2D74` is `Matching`, 2/2

**Result: the flip passes and the judge passes.** `tools/flip_test.sh MetroidPrime/Carve801C2D74.cpp`
prints `PASS -> kept as Matching`, and `./tools/goal_check.sh build/goal/item.json` prints
`goal_check: PASS carve-801c2d74` with every check `ok`. `matched 13493 -> 13495`,
`linked 6541 -> 6543`, `total_functions` still 28465, DOL sha1 still
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

## What I did

A four-file carve of `.text 0x801C2D74..0x801C2DFC` (0x88 = 136 bytes) out of dtk's
`auto_03_801C13F8_text`, which previously ran 0x801C13F8..0x801C2DFC and held 12 functions:

- `src/MetroidPrime/Carve801C2D74.cpp` (new, 183 lines, all of it inside `#ifdef __MWERKS__`)
- `config/G2ME01/splits.txt:1220-1221` — the claim, in address order between
  `MetroidPrime/Carve801C13F4.c` and `MetroidPrime/Player/CGrappleArm.cpp`
- `configure.py:760` — `Object(Matching, "MetroidPrime/Carve801C2D74.cpp"),` on one line, in
  address order
- `files.cmake:757-767` — the source, with the reason its host branch is empty

| function | address | size | what it is |
| --- | --- | --- | --- |
| `fn_801C2D74` | 0x801C2D74 | 0x68 (104 B, 26 insns) | `rstl::uninitialized_copy< pointer_iterator<CRagDoll::CRagDollParticle,...>, CRagDoll::CRagDollParticle* >` |
| `fn_801C2DDC` | 0x801C2DDC | 0x20 (32 B, 8 insns) | `rstl::construct< CRagDoll::CRagDollParticle >` |

## Measured, not recalled

Everything below came off the tree; the file names its own sources.

**The twins are exact, and I checked it word for word** rather than trusting the seed. I extracted
both `.fn` blocks from the `.s` files and compared the opcode words (ignoring the address column):

```
fn_801C2D74 26 vs fn_8028E754 26 -> differing idx: [12]      ('48 00 00 39' vs '4B FF E5 25', both bl)
fn_801C2DDC  8 vs fn_80004438  8 -> differing idx: [3]       ('4B F9 BD 99' vs '48 00 00 15', both bl)
```

One differing word each, and it is the `bl`. So `fn_801C2D74` is `fn_8028E754`
(`src/Kyoto/Animation/CAnimationSet.cpp:182`, `Matching`) with a different callee, and
`fn_801C2DDC` is `fn_80004438` (`src/MetroidPrime/Carve80004438.c:95-97`, `Matching`) with a
different callee.

**The element is `CRagDoll::CRagDollParticle` and the step is its size.** The loop's stride is the
literal `addi ..,0x44`, and `include/MetroidPrime/CRagDoll.hpp:197` asserts
`NESTED_CHECK_SIZEOF(CRagDoll, CRagDollParticle, 0x44)` — the same relation the twin carries between
its `addi ..,0x44` and `CHECK_SIZEOF(CAnimPOIData, 0x44)`. Two independent confirmations:

- `fn_801C2DDC`'s `bl` at 0x801C2DE8 targets
  `construct_impl<Q28CRagDoll16CRagDollParticle>__4rstlFPvRCQ28CRagDoll16CRagDollParticle`
  (`symbols.txt:5843`, 0x8015EB80, 0x28, weak) — **which our own tree already defines**, in
  `MetroidPrime/CRagDoll.cpp` (`.text` 0x8015DDE8..0x80160C20, `splits.txt:980`). So the DOL call
  resolves with no stub and no duplicate.
- `fn_801C2CBC` (0x801C2CBC, 0xB8, `symbols.txt:7321`), retail's only caller of `fn_801C2D74`
  (`grep -rn 'bl fn_801C2D74' build/G2ME01/asm/` returns exactly one hit), does
  `mulli r0, r0, 0x44` on its own count before calling.

**The iterators are parameters by value, and that is load-bearing** — carried over from the twin's
header, which records it as 92.69% -> 100.00% on `fn_8028E754` alone. Seen from the caller side,
`fn_801C2CBC` builds both iterators on its own stack at 0x801C2D24
(`addi r3,r1,0x14 / addi r4,r1,0xc / mr r5,r31`) before the call, and inside `fn_801C2D74` the test
is `lwz r0,0x0(r29) / cmplw r31,r0 / bne` — `end` re-read from the caller's frame *after* the `bl`,
which is what a by-value class parameter forces and what a `const T* const*` would let the compiler
hoist.

## Three decisions worth recording

**1. `.cpp`, not `.c`, and `extern "C"`.** The seed said `.c`. A `.c` cannot express the one call
this range makes: the callee's name is
`construct_impl<Q28CRagDoll16CRagDollParticle>__4rstlFPvRCQ28CRagDoll16CRagDollParticle`, and the
`<`, `>`, `,` in it are not identifier characters — `CIngBoostBallGuardian3790.cpp:32-42` records
that mwcceppc has no `__asm__` symbol renaming either. So the unit is a `.cpp` with `extern "C"` on
both definitions, which keeps the `fn_<addr>` symbols verbatim (a C++ definition without it would
mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing). The unit name and the target string
in `item.json` are extensionless, so the judge resolves it to the `.cpp` unchanged.

**2. `rstl::construct_impl` is declared as a template and never defined, and no `rstl` header is
included.** This is the `Carve8000447C.cpp:50-59` arrangement, and it is load-bearing here for a
measurable reason: `include/rstl/construct.hpp:50-56` gives `construct_impl` an *inline
definition*, and with it visible mwceppc may either inline the placement new into `fn_801C2DDC`
(wrong bytes — retail's eight instructions end in a `bl`, not in the copy) or outline a local weak
copy of `construct_impl<Q28CRagDoll16CRagDollParticle>...` into this object, which retail's range
does not define. Declared bare, the one call mangles to retail's own MWCC symbol and resolves
against `MetroidPrime/CRagDoll.cpp`. `rstl::pointer_iterator` is declared locally for the same
reason: its header includes `rstl/construct.hpp`. Only `rmemory_allocator` is named as an
incomplete class, because the template parameter list mentions it and nothing uses it.

**3. The element type is a local stand-in, and only two of its properties are reproduced: the
name and the size.** The name, because the callee's mangled symbol spells
`Q28CRagDoll16CRagDollParticle`; the size, because the loop steps `0x44`. Nothing here reads a
member — the loop only steps a pointer, and the callee is retail's own `construct_impl` in another
unit — so the eleven members of the real `CRagDoll::CRagDollParticle`
(`include/MetroidPrime/CRagDoll.hpp:44-77`) are deliberately not spelled out. The source says so,
so the next reader does not mistake the stand-in for a claim about the layout.

## Verification, all measured

```
python3 tools/check_decl_order.py --unit MetroidPrime/Carve801C2D74
  ok: 1 unit(s) checked, none emits its functions out of retail order

python3 tools/carve_diff.sh 801C2D74 88 build/G2ME01/obj/MetroidPrime/Carve801C2D74.o
  retail: 34 instructions, 136 bytes
  ours  : 34 instructions, 136 bytes
  differing instructions: 2      <- both are the unresolved `bl` relocations in the unlinked .o

./tools/unit_fit.sh MetroidPrime/Carve801C2D74.cpp
   .text  claimed 136  ours 136  retail 136  fits
   no extra functions: our object defines only what the retail unit object does

./tools/flip_test.sh MetroidPrime/Carve801C2D74.cpp
  PASS  -> kept as Matching     kept: 1/1  failed: 0  skipped: 0

./tools/goal_check.sh build/goal/item.json
  goal_check: PASS carve-801c2d74
    ok  gate.sh   ok  counts: matched 13493 -> 13495   linked 6541 -> 6543
    ok  check_symbol_names.py   ok  flip_test PASS, Object(Matching) in configure.py

sha1sum build/G2ME01/main.dol -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
total_functions 28465 (unchanged)
```

`carve_diff.sh`'s two reported differences are the `bl` immediates, which are still 0 in an
unlinked object; the instruction count and byte count agree on both sides and the flip — which links
the object — is what settles it. Nothing was left half-edited: `git status` shows only the four
carve files plus the two doc files the judge's own `check_docs_claims.py --write` rewrote (derived
counts only), which the driver discards.

## No wall, no blocker, no new item

Both functions reached 100.00% on the first spelling the twins implied, so there is no `WALL:` line
and nothing to file as `NEW:`. The one spelling question I had to answer before writing was
`.c` vs `.cpp` (decision 1 above), and it is a property of the name retail spells, not a wall a
later run would want re-attempted — it is written down in the source's header comment and here.