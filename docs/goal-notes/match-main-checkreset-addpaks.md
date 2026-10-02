# match-main-checkreset-addpaks

`kind: match`, `target: MetroidPrime/main`. Verdict from `tools/goal_check.sh`: **PARTIAL** - target
rose 97 -> 98 / 99, `All:` matched 12610 -> 12611, gate clean, no asm. Flip fails (see below).

## Premise was half stale

`CheckReset__5CMainFv` was already 100.00% (1180 B) on the clean tree. Only
`AddPaksAndFactories__18CGameGlobalObjectsFR10COsContext` (83.53%, 1936 B) was open; it is now
**100%**. The unit's last open function is `RsMain__5CMainFiPCPCc` (72.81%, 2148 B), untouched.

## What changed (`src/MetroidPrime/main.cpp` only)

83.53 -> 100 in four steps, each measured by `decomp_build.sh MetroidPrime/main`:

1. **No named `CResLoader& loader` / `CFactoryMgr& factories` locals** (83.53 -> 86.57, and
   -> 98.83 with 3): retail keeps `factory` in r31 and recomputes `addi r3,r31,4` / `addi r3,r31,116`
   at every use. A hoisted reference makes mwcceppc put the sum in a register (one extra saved GPR,
   frame 304 vs 288). Spell `factory.GetResLoader().X` / `factory.GetFactoryMgr().AddFactory` each time.
2. **Tweak buffer is not an `rstl::auto_ptr<uchar>`**: retail has a 4-byte pointer at r1+16, no
   owner flag, and frees it with an unconditional `CMemory::Free` *between* the `single_ptr` request
   dtor and `~CDvdFile`. A local struct `TweakData { uchar* data; ctor(ptr); ~TweakData(){Free(data);} }`
   declared between `tweakFile` and `request` reproduces that order. Uses must go through
   `tweakBuf.data`, not an alias local (the alias got promoted to r30).
3. **Struct needs a constructor.** Aggregate init `= {Alloc(...)}` emitted a stray `stw r3,0(0)` to an
   `@1250` static; assigning `.data =` after default construction put `stw r3,16(r1)` before
   `mr r4,r3` (99.58%). The ctor-initialised form gives the exact retail order (100%).
4. **`inline CErrorOutputWindow::~CErrorOutputWindow() {}`** (98.83 -> 99.58): retail inlines the
   body at the call site (vtable store `stw 200(r1)`, `li r4,0`, `bl __dt__6CIOWin`) while still
   emitting the weak out-of-line copy at 0x800078F8. Adding `inline` to the existing out-of-line
   definition in main.cpp (declared non-inline in the header, so the header is unchanged) gets both.
   The out-of-line copy is still emitted and still matches (unit count 98, nothing fell).

## Verified

```
tools/goal_check.sh build/goal/item.json   PARTIAL: gate ok, matched 12610 -> 12611, linked 5971 -> 5971,
                                           check_symbol_names ok, target 97 -> 98 / 99, no asm
```

## What still stops the flip

`flip_test MetroidPrime/main.cpp` fails at link: `undefined: 'lbl_8041A3F0'` (unchanged class of
blocker from earlier notes: the unit's `.data/.bss` are not claimed in splits, and `RsMain` is
72.81%). Not investigated this run. Next work on the unit is `RsMain` (2148 B).
