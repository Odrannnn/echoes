# Port layer

Carried over from the Metroid Prime port (`../MetroidPrimePort/platform`) as the
adaptation baseline for Echoes. Nothing here is referenced by the build yet: the
scaffold compiles the decompiled game sources only (`MP_SDK_HEADERS_ONLY=ON`).

What is already in use:

- `compat.h` — force-included ahead of every C++ game translation unit. Restores
  the SDK macros Aurora omits (`AUTO*`, `nofralloc`, `__abs`) and maps the game's
  `triggerL/R` onto Aurora's `TARGET_PC` `PADStatus` members. It is C++-only.
- `include/dolphin/` — SDK headers Aurora omits: `arq.h`, `gba.h`, `PPCArch.h`,
  `thp/`, and `gx/GXShims.h` (GX entry points Aurora does not provide).

What is carried over but still Metroid-Prime-specific, and must be reworked
against Echoes' bootstrap and globals before it will compile:

- `main.cpp` — the port entry point (renames the decompilation's `main` and drives
  Aurora). Echoes' entry path has the same shape but its own symbols.
- `disc.cpp` — disc mounting and file access; expects an MP1 game ID.
- `shims.cpp`, `sdk_stubs.cpp`, `glibc_compat.c`, `ai_dma.cpp` — GX/SI/PAD token
  shims, missing SDK entry points, the glibc floor, and deferred ARQ callbacks.
  Mostly generic, but written against MP1's include set.
- `rel.cpp`, `include/port_rel.h` — the REL module runtime (arena, loader,
  linker, registry). Not MP1-specific and already tested; see `../PORT_NOTES.md`.
- `debug_ui.cpp`, `port_textures.cpp`, `port_prompts.cpp`, `port_randomizer.cpp`,
  `smoke.cpp` — the F1 overlay, HD textures, button prompts, randomizer and the
  opt-in smoke driver. All MP1-specific; see the Prime port's `docs/NATIVE_PORT.md`.

The REL/module runtime Echoes needs does not exist here or in Aurora — see
`../PORT_NOTES.md`.
