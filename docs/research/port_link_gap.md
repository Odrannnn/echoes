# What the port still needs in order to link

Measured 2026-09-25 with `python3 tools/link_gap.py`. Regenerate the numbers with
`--list`; the list at the bottom is the ratchet the tool checks, so a symbol appearing that
is not listed here fails, and a listed symbol that is no longer missing also fails.

## Why this had to be measured

The port builds its game sources as an **OBJECT library**, so there is no link step and
therefore nothing that ever reports what is missing. `MP_SDK_HEADERS_ONLY` is on by default
and `PORT_NOTES.md` records it as the only verified configuration. "The game does not link"
was true and unquantified.

`tools/link_gap.py` compiles `mp_game` for the host, subtracts what the objects define from
what they reference, and classifies the remainder. Of **1440 undefined symbols**, 722 are
the C++ runtime, 23 are libc, 106 appear in Aurora's own sources, 1 (`AIStartDMA`) only in an
Aurora header, and **44 are genuinely unaccounted for**. Those 44 are the work.

> The first measurement said 63. The 19 that closed are the retail globals below, and closing them
> turned up two miscounts worth recording: the split of 63 was **29** unwritten functions, **19**
> retail globals, 8 game globals, 6 REL symbols and `BuildTime` - the text said 30 and 20, which
> sums to 64.

## What this does not measure, and it hid a stale claim for a whole release

`link_gap.py` only globs objects under a path containing `mp_game`. The other two port targets
are invisible to it: `mp_platform` (the SDK shims and stubs) and `mp_port_entry` (`platform/main.cpp`,
the process entry point). The entry point is the one that *references* the game's classes, so
**nothing the port layer needs from the game can ever appear in this list.**

That is how `PORT_NOTES.md` came to say for a whole release that `COsContext`/`CMemorySys` were
"still missing upstream", and it was wrong about one of the two: `CMemorySys`'s three methods
have been in `src/Kyoto/Alloc/CMemory.cpp` since the port's first build. A header with no `.cpp`
of its own is not a class with no definition, and the tool could not have caught the difference
because no `mp_game` object references either class.

The entry point's own gap is measurable without an executable target - `nm -u` on
`mp_port_entry`'s object, minus everything `mp_game` and `mp_platform` define - and it is now
**zero**: `COsContext` and `CMemorySys` are both fully defined (`src/Kyoto/Basics/COsContext.cpp`
is new; `CMemory.cpp` already had `CMemorySys`). What remains unresolved there is Aurora's own
entry points, libc and the C++ runtime, which `MP_SDK_HEADERS_ONLY` deliberately does not link.
Extending `link_gap.py` to cover all three targets would fold that into the ratchet; it is not
done, because it changes what MISSING means and every entry in the list below would have to be
re-derived.

## The correction that produced this number

**The first version of this measurement said 63, and it was wrong by more than 20x.** It
classified any symbol starting `_Z` as "c++ runtime", which is true of `std::` instantiations
and false of every game function MWCC mangles - they all start `_Z` too. That hid **700 real
game symbols** in a bucket nobody reads. The tool now demangles with `c++filt` and classifies on
the result.

Two smaller versions of the same mistake are recorded because they recur. A bare word match is
not evidence that Aurora provides a symbol - "Allocate" and "Renderer" occur all over its trees,
and whole-word matching filed the game's own `AllocateRenderer` and
`CInputGenerator::CInputGenerator` as provided - so a symbol must be *used*, followed by `(` or
preceded by `::`, `.`, `->`, `&`, `*`, before Aurora's tree may claim it. And a measurement of
stale objects is worse than none, so a source newer than the newest object exits 3 rather than
being believed.

**724 is the honest number, and it is far more useful than 63** because most of it is bulk work
rather than hand-decompilation:

| group | count | what closes it |
| --- | --- | --- |
| other game methods | 300 | decompilation, one function at a time. This is the honest remainder |
| REL module loaders | 234 | **one generator.** Every module has the same `Load*(CStateManager&, CInputStream&, const CEntityInfo&)` shape, one per module, and the Tweaks and ForgottenObject modules already show the pattern |
| `SLdr*` script-loader struct constructors | 136 | **one generator.** The `SLdrTweak*`/`SLdr*` structs' default constructors and destructors; retail's are all trivial |
| unmangled: `fn_*`, `lbl_*`, globals | 31 | the class this document was written about: 19 retail globals, 9 game globals and sentinels, 3 unwritten functions. **All 31 closed** |
| static data members | 12 | definitions for `CSfxManager::kMedPriority`, `CActorLights::kDefaultPositionUpdateThreshold`, `CAudioSys::kVolumeTable` and friends |
| `TypesMatch` overrides | 8 | eight classes declare `TypesMatch` and never define it - a one-line body each |
| `rstl` templates | 6 | `rstl::string_l(const char*)` and the `basic_string` null sentinels |

So the shape of the remaining work is **about 370 symbols a generator can produce, 300 that are
decompilation proper, and 31 already done.** That is a different project from "close 63
symbols", and worth knowing before a lane is pointed at the wrong thing.

## What is left, and what the port still needs

`COsContext` is written (`src/Kyoto/Basics/COsContext.cpp`), and the `CMemorySys` claim in
`PORT_NOTES.md` was simply wrong - its three methods and the allocator it returns have been in
`src/Kyoto/Alloc/CMemory.cpp` since the port's first build. **A header with no `.cpp` of its own
is not a class with no definition.**

The two things that block a first frame are not in this list at all, because they are not symbol
problems: `CMain::RsMain` is an empty body and `CMain::OpenWindow` is unimplemented, so nothing
calls `COsContext::OpenWindow` yet. Both are in `src/MetroidPrime/main.cpp`, whose unit is 20 of
99 functions with 58 never written.

## What this does not tell you

The 106 symbols attributed to Aurora's sources are attributed because the identifier appears
in a file under `extern/aurora/lib`. That is strong evidence, not proof - a name in a source
file is not the same as a definition in an object. **The authoritative answer is an actual
link**, and the only symbol where the distinction is already known to bite is `AIStartDMA`,
which appears in an Aurora *header* and in none of its sources. Do not treat the 106 as
resolved until a link has succeeded.

