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
disc as areas load and links at runtime with `OSLink` against the DOL's exports.

Aurora declares `OSLink`/`OSLinkFixed` in `extern/aurora/include/dolphin/os/OSModule.h`
but **implements neither**, and has no REL loader at all. Any native Echoes port
needs one: allocate the module's sections, apply its relocations, resolve its
imports, and keep a module table the game can query. Metroid Prime's port has no
equivalent — MP1 is a single DOL. Two viable shapes:

- a real REL loader that reads the module from the disc and relocates it in the
  host process (preserves the game's streaming behaviour), or
- a build-time approach that links each decompiled REL into a host shared library
  and fakes the module registry with `dlopen`.

Both are bounded and well-specified by the SDK's `OSModuleHeader`, and both are
needed by the static-recompilation route as well.

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

The same sweep is available without CMake as a parallel `-fsyntax-only` probe,
which is how the queue was first measured; keep the two in sync (`compat.h` is
C++-only; the bundled LZO `.c` files are compiled as C).

## Next steps, in dependency order

1. **Aurora `OSModule`/`OSLink` and a REL loader.** Needed by every route to a
   native Echoes, and independent of decompilation progress (the modules are
   well-specified by the SDK header and can be tested against real REL files
   extracted from an owned disc).
2. **Adapt the platform layer**: entry point, disc, CARD/saves, input, debug
   overlay — against Echoes' bootstrap, which is the same shape as Prime 1's.
3. **Track upstream**: re-run the compile and re-add `CARAMManager.cpp`, the
   size-taking `CGX::SetArray`, and the five unfinished functions as the
   decompilation fills them in. Tighten `-Werror=return-type` when it is complete.
4. **Only once the decompilation links**: entry point wiring, `mp_game` link,
   packaging (AppImage/Flatpak/APK), widescreen, mouse aim.

A static recompilation of the DOL (the approach used for the earlier Metroid
Prime recompilation experiment) remains a faster route to a *playable* binary,
but it inherits the same REL problem and gives up the readable source that makes
widescreen, mouse aim and HD textures practicable. The Metroid Prime port's
history — rewritten from a recompilation experiment into a decompilation port for
exactly that reason — is the argument for this route.
