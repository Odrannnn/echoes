# Metroid Prime 2: Echoes native port — bring-up notes

Working notes for a native-PC port of **Metroid Prime 2: Echoes** (`G2ME01`, USA
v1.00), built the same way as the Metroid Prime port next door
(`../MetroidPrimePort`): the [PrimeDecomp/echoes](https://github.com/PrimeDecomp/echoes)
matching decompilation is compiled against [Aurora](https://github.com/encounter/aurora),
an MIT GameCube SDK/GX compatibility layer, and runs on the GPU directly — no
emulator and no static recompilation.

No game content is included. The port reads a user-supplied disc image.

## Status (2026-09-24)

**The scaffold compiles.** All 109 decompiled game and engine translation units
build against Aurora's SDK headers with **0 errors** and produce 109 objects.
They cannot be linked or run yet, and the reason is not the port layer.

The blocker is decompilation coverage. From [decomp.dev](https://decomp.dev/PrimeDecomp/echoes):

| | Echoes `G2ME01` | Metroid Prime `GM8E01_00` (for reference) |
| --- | --- | --- |
| Functions matched | 2,024 / 28,465 (**7.1%**) | 16,317 / 16,685 (**97.8%**) |
| Code size | 6.54 MB (DOL 3.81 + 2.73 in RELs) | 4.01 MB (single DOL) |
| DOL | 1,834 / 16,726 fns (**11.0%**) | 16,317 / 16,685 fns |
| REL modules | 190 / 11,739 fns (**1.6%**), 86 modules | none |
| Fully linked | 4.5% | 49.2% |

Metroid Prime's port consumed a decompilation that was 97.8% complete. Echoes is
at 7.1%: the same exercise repeated today would need roughly 26,000 more
functions written before the port can do anything with them.

## Blocker ranking

1. **Decompilation coverage** — years of upstream work; nothing here shortcuts it.
2. **REL modules and `OSLink`** — new subsystem, see below. Needed by *every*
   route to a native Echoes, including a static recompilation.
3. **Port layer adaptation** — the part this scaffold starts. Bounded, and mostly
   transferable from the Metroid Prime port.

### REL modules are the structural difference from Prime 1

Echoes splits gameplay across `main.dol` plus 86 REL modules (one per enemy,
boss and script family: `DarkSamus`, `EmperorIngStage1..3`, `SpacePirate`,
`Blogg`, `SandBoss`, `Tweaks`, `ScriptGui`, …), which the game loads from the
disc as areas load and links at runtime against the DOL's exports.

Prime 1 is not quite a single DOL either — its disc carries `NESemu.rel`,
`NESemuD.rel`, `NESemuP.rel` and `Tweaks.Pak` — but its port sidesteps modules
entirely by compiling those sources into the executable and excluding the NES
emulator (see `files.cmake` in this tree). Echoes cannot do that: 86 modules
carry most of its gameplay.

The port therefore implements the runtime itself (see **REL runtime** below).
Aurora declares `OSLink`/`OSLinkFixed` in
`extern/aurora/include/dolphin/os/OSModule.h` but implements neither, and it
cannot host the linker as written — see the first finding below.

## REL runtime (2026-09-24)

`platform/include/port_rel.h` and `platform/rel.cpp` implement the module
runtime: a 32-bit guest arena, the REL probe/loader, the linker (a port of the
console's `OSLink` — section and bss placement, import lists, every relocation
type the console handles, the `R_DOLPHIN_*` control records, and unlink with
undo) and the module registry (`SearchModule` answers the pointer-to-module
queries the OS error paths use). It deliberately does not live in Aurora: see
the first finding.

### Three things that shape it

1. **Aurora's `OSModuleHeader` cannot overlay a REL image on a 64-bit host.** On
   the console the struct is built with 32-bit pointers, so the loaded image can
   be overlaid with it and patched in place. On the host `OSModuleLink` holds two
   8-byte pointers, which shifts every field after it, while the relocation
   encoding is fixed at 32 bits. The port therefore works on the file's own
   layout (`RelModuleHeader`, `RelSectionInfo`, `RelImportInfo`, `RelReloc`) and
   keeps its registry outside the image. An earlier attempt that used the SDK
   structs silently read `version` from the `nameSize` bytes; the real-module
   test now catches exactly that.
2. **The image is big-endian and stays that way.** Only the header, section
   table, import table and relocation records are converted to host order, for
   the linker's benefit. Section payloads keep their guest bytes and relocation
   results are written in guest byte order, so the linked image is the same image
   the console would produce — which is what a recompilation or interpreter
   runtime needs. Consumers of pointer-bearing data convert per field, as the
   port does for other disc resources.
3. **The arena has to sit in the console's module region.** `R_PPC_REL24` encodes
   ±32 MiB, and a module's addends are absolute addresses in the DOL (0x80003100
   upward). Anywhere below 4 GiB is not enough: linking the real module at
   0x40000000 produced branches that decode back to 0x40389a84 instead of
   0x80389a84. `GuestArena(size, preferredBase)` takes a base; the tests use
   0x81000000.

### Verification

- `port_rel_tests` — a hand-built module exercising every relocation type the
  linker implements, the code flag on section offsets, bss placement and zeroing,
  prolog/epilog/unresolved resolution, a second module importing from the first,
  unlink-and-undo, and `SearchModule`.
- `port_rel_real_tests` — `NESemuP.rel` (181,020 bytes: version 2, 15 sections,
  0x9e1c of bss) extracted from an owned Metroid Prime disc with
  `tools/extract_disc_file.py`. It checks the header and section layout, walks
  both relocation lists with an independent reader (413 main-module records and
  1067 self records, counts asserted per type), decodes all 411 main-module
  relocations back out of the patched instructions (every `bl` reaches the
  address it claims, the halfword pair reconstructs its address), and requires
  each of those addresses to be a symbol of the DOL the module was linked
  against. All 1063 self-relocations are checked to stay inside the module.
- Configure with `-DMP_REL_FIXTURE=<module.rel> -DMP_DOL_SYMBOLS=<symbols.txt>`
  to run the real-module test; without them ctest reports it skipped
  (`SKIP_RETURN_CODE 77`).
- The 109 game translation units still compile, and both tests pass.

### Still missing

- `OSLinkFixed`'s fixed-address path (the version-3 `impSize` truncation). No
  module that needs it is known for either game, so it is left unimplemented
  rather than guessed at.
- The game-side module manager — which module an area loads, and the module
  entry points the game calls — is part of the undecompiled 93%.
- Nothing loads modules yet: the runtime is exercised by tests only.

## Platform layer and the SDK link gap (2026-09-24)

### What the compiled game still needs from the SDK

Measured, not guessed: `nm -u` over the built objects, classified against what
Aurora actually defines (not just declares).

- **99** SDK entry points are referenced by the 109 compiled translation units.
- Aurora provides **79** of them.
- **20** are missing. The port's carried-over platform layer already implements
  17 (13 stubs, the 4 AI DMA entries in `ai_dma.cpp`); the 3 genuinely new ones —
  `OSClearContext`, `OSSetCurrentContext`, `OSGetStackPointer`, referenced by
  `RAssertDolphin` and `REL_Setup` — are now stubbed there.
- The carried-over `OSLink`/`OSUnlink` **no-op stubs were removed**: they returned
  `TRUE` without linking anything, which would have made module loading silently
  appear to work. The port's linker is `port::rel::LinkModule`; a call to the SDK
  entry point is now a link error on purpose.

`platform/shims.cpp`, `platform/sdk_stubs.cpp`, `platform/glibc_compat.c` and
`platform/entry.cpp` are built as the `mp_platform` target, so the SDK-facing
layer stays compiled while the game cannot link; `platform/main.cpp` is compiled
by `mp_port_entry`. `tools/probe_sources.sh` runs the same sweep without a
configure (114 files: the 109 game units plus those five).

`ai_dma.cpp` needs SDL3 and `disc.cpp` needs the game's resource model, so both
stay out of the build for now. `debug_ui.cpp`, `port_textures.cpp`,
`port_prompts.cpp`, `port_randomizer.cpp` and `smoke.cpp` are still
Metroid-Prime-shaped; `platform/README.md` lists what each waits for.

### The entry point, and why nothing runs yet

The decompilation exports the seam the port needs:
`InvokeCMain(argc, argv, COsContext*, void*, CMemorySys*, void*)` in
`src/MetroidPrime/main.cpp:79`, which constructs `CMain` and calls `RsMain` —
cleaner than Metroid Prime, whose port had to rename the decompilation's `main`
to `metroid_main` because that is what its bootstrap exported.

`platform/main.cpp` is that entry point: the proven sequence
(`aurora_initialize` → disc resolution → `aurora_dvd_open` → check the disc is
`G2ME01` revision 0 → `aurora_update` → hand control to `InvokeCMain` →
`aurora_dvd_close` → `aurora_shutdown`). The parts that do not need the game are
split into `platform/entry.cpp` and covered by `port_entry_tests`: which disc the
port accepts, and how it finds the image (argument, then `MP2_DISC`, then the
first image beside the executable).

`InvokeCMain` takes the OS context and memory system from *its* caller, and that
caller is not decompiled, so the entry point builds them the way that caller will
need to:

| still missing upstream | where |
| --- | --- |
| `COsContext::COsContext(bool, bool)` / `~COsContext()` | `include/Kyoto/Basics/COsContext.hpp` |
| `COsContext::OpenWindow` — the window/VI bring-up | same |
| `CMemorySys::CMemorySys(COsContext&, IAllocator&)`, `~CMemorySys()`, `GetGameAllocator()` | `include/Kyoto/Alloc/CMemorySys.hpp` |

None of these has a source file in the tree, so the entry point compiles but
cannot link yet. `COsContext::OpenWindow` is the interesting one: Aurora has
already created the window by then, so it becomes an adapter over Aurora's VI
rather than a real window setup — the same shape the Prime 1 port ended up with.

**`CMain::RsMain` is an empty body** (`src/MetroidPrime/main.cpp:219`), so there
is nothing to run even once it links. The same file stubs the pieces that make
the rest of the bootstrap meaningful:

| function | line | state |
| --- | --- | --- |
| `CMain::RsMain` | 219 | empty — the game loop |
| `CMain::InitializeSubsystems` | 88 | `ARInit` plus a TODO |
| `CGameGlobalObjects::AddPaksAndFactories` | 203 | empty — registers asset factories |
| `CMain::FillInAssetIDs` | 215 | partial, TODOs |
| `CMain::AsyncIdle` | 221 | partial |
| `CMain::MemoryCardInitializePump` | 201 | empty |
| `CMain::DrawDebugMetrics` | 205 | empty |
| `CMain::CheckReset` | 213 | empty |
| `CMain::ShutdownSubsystems` | 93 | empty |
| `CGameArchitectureSupport::Update` | 199 | empty |
| `CMain::StreamNewGameState` | 275 | TODO |

`CGameGlobalObjects::PostInitialize` (98) and `LoadStringTable` (106) *are*
implemented, and `CGameArchitectureSupport`'s constructor is substantial, so the
bootstrap is in progress rather than absent. Until `RsMain` and the paks/factories
land there is nothing for the port's entry point to drive; that is upstream's
work, and no port-side change can substitute for it.

## The shim queue

The first sweep produced 187 errors across all 110 sources. Applying the queue
below left 0 errors, and a clean build produces 109 objects with 25 warnings
(4 upstream categories, listed under open issues below). This is the port's
whole SDK-compatibility cost so far.

| Errors | Root cause | Fix |
| --- | --- | --- |
| 106 | `include/Kyoto/Alloc/CMemory.hpp` redeclared the standard placement `new` and defined weak global `operator delete`, conflicting with `<new>` | Guard both behind `__MWERKS__ || CLANGD`; the host runtime owns global new/delete |
| 46 | `src/Kyoto/CARAMManager.cpp` — `CARAMManager.hpp` is a stub missing the members the `.cpp` defines, and the TU is not in upstream `configure.py` | Excluded from the file list; re-enable when upstream finishes it |
| 12 | `src/rstl/rstl_strings.cpp` specialized members after implicit instantiation (MWCC allowed use-before-specialization) | Declared every specialization in `rstl/string.hpp` and again at the top of the `.cpp`, as the Metroid Prime port does |
| 11 | `src/Kyoto/Basics/RAssertDolphin.cpp` dumped `OSContext::gpr/lr/cr/srr0`; Aurora's `TARGET_PC` `OSContext` is opaque | Dump guarded with `#ifndef TARGET_PC`, with a one-line substitute |
| 2 | LZO compiled for 32-bit `size_t` | `SIZEOF_SIZE_T` from `CMAKE_SIZEOF_VOID_P` in `files.cmake`, plus a `TARGET_PC` fallback in `lzo_conf.h` for compiles that bypass CMake |
| 1 | `IRenderer::AddParticleGen` — derived code overrides a bounds-taking overload the base never declared | `AddParticleGen2` was a placeholder name for that overload; it now carries the real signature (same vtable slot, so the layout is unchanged) |
| 1 | `CActor::AddToRenderer` was missing the frustum-taking overload derived classes override | Added alongside the existing 1-arg form |
| 1 | `CGX::SetArray` — Aurora's `TARGET_PC` `GXSetArray` needs the array size and endianness | Forwarded with size 0 (see the open issue below) |
| 3 | `CDvdFile::ARAMARAMXferCallback` and two address casts assumed 32-bit pointers | Signature and casts moved to `uintptr_t`, matching Aurora's `ARQCallback` |
| 1 | `RstlExtras.cpp` declared `char* strchr(const char*, int)`, clashing with modern glibc's const-correct one | Declaration dropped; the host libc's is used |

## Open issues carried in the port

These are known, deliberate and documented in the code. None is fixed by the
port; each needs either upstream decompilation work or Aurora work.

- **`CGX::SetArray` has no size parameter.** Aurora uploads exactly `size` bytes
  of a vertex array to the GPU and renders nothing for an array of size 0. Echoes'
  console signature `(attr, data, stride)` cannot supply one, and no caller is
  decompiled yet. The forwarder passes 0. When the model code lands, Echoes must
  grow the size-taking overload the way Metroid Prime's
  `include/Kyoto/Graphics/CGX_Impl.hpp` does.
- **Five functions fall off the end of a non-void function** — they are
  unfinished decompilations that MWCC accepted as undefined behaviour:
  `MetroidPrime/main.cpp` (×2), `ScriptObjects/CScriptForgottenObject.cpp`,
  `ScriptObjects/CScriptPickup.cpp`, `ScriptObjects/CScriptSpawnPoint.cpp`.
  The port build keeps `-Wno-error=return-type` so they warn rather than fail;
  the Metroid Prime port uses `-Werror=return-type`, and this should return to
  that once upstream completes them. The port deliberately does not invent
  return values in not-yet-decompiled code.
- **`single_ptr`/`rc_ptr` delete incomplete types** (`-Wdelete-incomplete`
  warnings). Console-side lifetime idiom; harmless as warnings, and unchanged
  from the decompilation.
- **`REL_Setup.cpp::_unresolved` walks the PowerPC stack.** The REL module's
  handler for unlinked imports reads the 32-bit back-chain via
  `OSGetStackPointer`, which neither the host stack layout nor 64-bit pointers
  match (`-Wint-to-pointer-cast`). It only matters once a REL is actually loaded,
  so it belongs with the REL runtime work in the first next step.
- **`CGameOptions::ResetControllerAssets` reads past a fixed-size array**
  (`-Waggressive-loop-optimizations`, iteration 1 of an inlined loop is undefined
  behaviour). Upstream's; it compiles but needs fixing before it runs.

## What transfers from the Metroid Prime port

Aurora is game-agnostic, and Echoes is the same engine generation (Kyoto, `rstl`,
`CMain`/`CGameGlobalObjects`/`InitializeSubsystems` bring-up). Expect to reuse:

- `platform/compat.h` — already in use here; it restores the SDK macros Aurora
  omits and maps `triggerL/R` to Aurora's `TARGET_PC` `PADStatus`.
- `platform/include/dolphin/{arq.h, gba.h, PPCArch.h, thp/, gx/GXShims.h}` — the
  SDK headers Aurora omits.
- `platform/{disc.cpp, sdk_stubs.cpp, shims.cpp, glibc_compat.c}` — disc mounting,
  GX/SI/PAD token shims, the glibc floor.
- The `extern/aurora` and `extern/musyx` snapshots (both MIT, vendored, not
  submodules). **Caveat:** the vendored MusyX is Prime 1's snapshot; Echoes ships
  a later MusyX (streamed music, different managers) and will need its own audio
  bring-up, as the Prime port's MusyX work shows.
- The Prime port's playbook in `../MetroidPrimePort/PORT_NOTES.md` and
  `docs/NATIVE_PORT.md`, including the widescreen/HUD, mouse-aim and save work
  that has to be redone against Echoes' layouts.

`platform/` here is a carried-over copy of the Prime port's platform sources. It
is **not referenced by the build**: several files are MP1-specific (textures,
prompts, randomizer, debug UI) and all of them assume MP1's entry point and
globals. They are the adaptation baseline, not working code.

## Building

```sh
cmake -S . -B build/probe -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/probe -j "$(nproc)"
```

`MP_SDK_HEADERS_ONLY=ON` (the default) compiles the game sources against Aurora's
headers without linking Aurora; that is the only verified configuration, and it
is what the shim queue above is tracked against. `-DMP_SDK_HEADERS_ONLY=OFF`
(the eventual port shape: `add_subdirectory(extern/aurora)`, link the Aurora
targets, build the executable) stops with a `FATAL_ERROR` until the port layer is
adapted — see the comment block at the end of `CMakeLists.txt`.

The same sweep is available without a configure:

```sh
tools/probe_sources.sh          # syntax-check every source; -v prints errors
```

It compiles the 109 game units plus the `mp_platform` and `mp_port_entry`
sources (114 files), mirroring the build's flags: `compat.h` is C++-only, and the
bundled LZO `.c` files are compiled as C.

## Next steps, in dependency order

1. **Done: the REL runtime** (`platform/rel.cpp`) — loader, linker and registry,
   verified against a real module from an owned disc. What remains here is
   `OSLinkFixed`'s fixed-address path, and feeding it real disc reads.
2. **Adapt the platform layer** (started): the engine-independent sources build,
   and the entry point is written against the decompilation's own seam. What is
   left is upstream's: `COsContext`/`CMemorySys` before it can link, and `RsMain`
   and the asset factories before it can do anything. Disc resources, CARD/saves,
   input and the debug overlay come after those — see "Platform layer and the SDK
   link gap" above.
3. **Track upstream**: re-run the compile and re-add `CARAMManager.cpp`, the
   size-taking `CGX::SetArray`, and the five unfinished functions as the
   decompilation fills them in. Tighten `-Werror=return-type` when it is complete.
4. **Only once the decompilation links**: entry point wiring, the game-side
   module manager and module entry points, `mp_game` link, packaging
   (AppImage/Flatpak/APK), widescreen, mouse aim.

A static recompilation of the DOL (the approach used for the earlier Metroid
Prime recompilation experiment) remains a faster route to a *playable* binary,
but it inherits the same REL problem and gives up the readable source that makes
widescreen, mouse aim and HD textures practicable. The Metroid Prime port's
history — rewritten from a recompilation experiment into a decompilation port for
exactly that reason — is the argument for this route.
