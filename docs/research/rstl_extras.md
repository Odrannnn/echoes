# `rstl/RstlExtras.cpp` — 64.87% was a *claim* problem, and the unit still cannot promote

Measured 2026-09-26, lane `rmap`. Unit claims `.text 0x802FF4BC..0x802FFF04` (2632 B) and
`.rodata 0x803AFAB8..0x803AFAD8` (32 B).

## The report said "8 unwritten functions and 2652 bytes of COMDAT extras". Both halves were wrong

`tools/fnmap.py rstl/RstlExtras` pairs functions by **instruction bytes**, not by name, and it
says **7 of the 8 were already byte-identical in our object**. They were only `fn_802FF...` on the
retail side, so objdiff had no pair to score. The eight were:

| retail | size | what it is | how it was established |
|---|---|---|---|
| `0x802FF4BC` | 0x68 | `FSTLCFactory__FRC10SObjectTagR12CInputStreamRC15CVParamTransfer` | `fnmap` IDENTICAL; already in our source |
| `0x802FF524` | 0xA4 | `__ct<rstl::vector<rstl::string>>__16CFactoryFnReturnFPQ…` | read out of our object with `nm`; pairs but scores 0% (see below) |
| `0x802FF5C8` | 0x90 | `__dt__146TObjOwnerDerivedFromIObj<vector<string>>Fv` | `fnmap` IDENTICAL |
| `0x802FF658` | 0x2C | `GetIObjObjectFor__128TToken<vector<string>>FRCauto_ptr<…>` | `fnmap` IDENTICAL |
| `0x802FF684` | 0x9C | `GetNewDerivedObject__146TObjOwnerDerivedFromIObj<…>FRC…` | `fnmap` IDENTICAL |
| `0x802FF720` | 0x64 | `__dt__Q24rstl130auto_ptr<vector<string>>Fv` | `fnmap` IDENTICAL |
| `0x802FFC8C` | 0x38 | `CStringExtras::CreatePrefix` | `fnmap` IDENTICAL; already in our source |
| `0x802FFCC4` | 0x94 | `CStringExtras::ConvertToLowerCase(const rstl::string&)` | same size, **not** identical — needed a body fix |

Seven `symbols.txt` renames (names read out of `build/G2ME01/src/rstl/RstlExtras.o` with `nm`,
never invented) took the unit from **7/17 to 13/17 functions at 100%** and `matched` from 3255 to
**3261**, with the DOL sha1 unchanged. Each line was given `scope:weak` and the alignment comment
it already had, because every one of these is a COMDAT weak in our object.

**`fn_802FF524` is `CFactoryFnReturn::CFactoryFnReturn<T*>`** — the template constructor in
`include/Kyoto/CFactoryMgr.hpp`, instantiated at `T = rstl::vector<rstl::string>`. It is worth
recording *how*: the body builds an `rstl::auto_ptr<T>` temporary at `sp+16` (`stb (p != 0)` /
`stw p`) and calls `TToken<T>::GetIObjObjectFor` on it, which is what the tree's
`TToken<T>::GetIObjObjectFor(const rstl::auto_ptr<T>&)` + implicit conversion already spells.
Retail's bytes are 0xA4; ours are 0xB0, and the whole difference is 6 instructions: mwceppc
**inlines** `rstl::auto_ptr<T>::~auto_ptr()` (`lbz / cmplwi / beq / lwz / li r4,1 / bl fn_8000971C`)
where retail emits `addi r3,r1,16; li r4,-1; bl __dt__Q24rstl130auto_ptr<…>Fv`. Making that
not-inline means moving the destructor out of the class in `include/rstl/auto_ptr.hpp`, which every
`rstl` user sees. Not attempted.

## `CStringExtras::ConvertToLowerCase(const rstl::string&)`: 98.92% -> 100.00%

Retail (0x802FFD08 in the object) loads `ret.data()`, calls `internal_prepare_to_write` with
`li r5,1` and **no r4 setup at all**, then reloads `ret.data()`. The source that produces that is
`ret.reserve(ret.length())`: `reserve` is `internal_prepare_to_write(len, true)`, and mwceppc
reuses the loop latch's `r4` (which already holds `ret.length()`) instead of reloading it.

**Do not "fix" this into `internal_prepare_to_write(1)`.** Measured: `reserve(ret.length())` is
**98.92%**, `reserve(1)` and `internal_prepare_to_write(1)` are both lower, and making
`internal_prepare_to_write` public in `rstl/string.hpp` to spell it does not get there either.
Retail's `r4` is genuinely undefined at the call on the first iteration, which means the function is
dead code with a latent bug; the byte pattern is the evidence, not the semantics.

The remaining 1.08% is one register — retail's loop counter is `r29`, ours `r31`. Fixed by
declaring the two pointers *before* the loop, `char* after;` first and `const unsigned char* before;`
second (the reverse of the order they are assigned in the body). See `docs/RUNNING_THE_DECOMP.md`,
"MWCC hands out callee-saved registers in declaration order" and its two refinements.

## Why the unit still cannot be `Matching`, and how that was established

`tools/unit_fit.sh rstl/RstlExtras.cpp` after the renames:

```
   .text      claimed   2632   ours   4368   retail   2632   over by 1736
   .rodata    claimed     32   ours     27   retail     32   SHORT by 5
   .data      claimed      -   ours     36   <- NOT CLAIMED BY splits.txt
   16 function(s) present in ours but not in the retail unit object, 1900 bytes total
```

The brief's warning applies and was checked: **is a COMDAT extra discardable, or owned?** Owned.
An `nm` sweep over all **481** objects in `build/G2ME01/src` finds **zero other definers** of any of
those 16 — `__ct__vector<string>(CInputStream)`, `__ct__vector<string>(const vector<string>&)`,
`reserve<vector<string>>`, `__dt__vector<string>`, `__dt__auto_ptr<TObjOwnerDerivedFromIObj<…>>`,
`__dt__auto_ptr<CObjOwnerDerivedFromIObjUntyped>`, `__dt__IObj`, `__dt__CObjOwnerDerivedFromIObjUntyped`,
`__vt__4IObj`, `__vt__31CObjOwnerDerivedFromIObjUntyped`, `__vt__146TObjOwnerDerivedFromIObj<…>`,
`cinput_stream_helper<basic_string<char,…>>`, `uninitialized_copy<pointer_iterator<…>>`, and the
two dead reconstructions `CStringExtras::CreateFromReal` / `IsSeparator`. mwldeppc therefore keeps
every one of them, the object is 1736 bytes over its claimed range, and no percentage fixes that.
This is the `CStaticAudioPlayer` situation and the same two instruments separate the two cases:
`unit_fit.sh` for the list, the `nm` sweep for the ownership.

Separately, `.rodata` is **5 bytes short**: ours is 27 bytes — `"??(??"`, `" \t\n\r\""`, `"%f"`,
`"%%.%df"`, `"0"`, `"-"` — and retail's is 32, with five trailing NUL bytes that none of those six
literals accounts for. Five more empty literals, or a `char[5]`, or padding; not determined.

## The trap in the rename itself

`symbols.txt` is *read* by `dtk dol split` but is **not one of its declared ninja inputs** — the
rule depends on `config/G2ME01/config.yml`. `tools/link_gap.py --rebuild` does not re-run the
split. A rename is invisible in `build/G2ME01/obj/**` until

    touch build-port/build.ninja && ninja build/G2ME01/config.json

and **deleting** `build/G2ME01/config.json` is what works; `touch`ing it leaves ninja with an
output newer than its input. The symptom is `try_batch.py` reporting `no function matching … in the
retail object` for a name `nm` can see. The DOL sha1 is unaffected either way.
