# Port layer

Carried over from the Metroid Prime port (`../MetroidPrimePort/platform`) as the
adaptation baseline for Echoes. `compat.h`, `include/dolphin/` and the three
sources in the `mp_platform` target are in use; the rest is listed with what it
waits for. See `../PORT_NOTES.md` for the measured SDK link gap.

## In use

- `compat.h` — force-included ahead of every C++ game translation unit. Restores
  the SDK macros Aurora omits (`AUTO*`, `nofralloc`, `__abs`) and maps the game's
  `triggerL/R` onto Aurora's `TARGET_PC` `PADStatus` members. C++-only.
- `include/dolphin/` — SDK headers Aurora omits: `arq.h`, `gba.h`, `PPCArch.h`,
  `thp/`, and `gx/GXShims.h` (GX entry points Aurora does not provide).
- `shims.cpp`, `sdk_stubs.cpp`, `glibc_compat.c` — GX/SI/PAD/PPCArch shims, stubs
  for the SDK entry points Aurora lacks (17 of the 20 the compiled game needs),
  and the glibc floor. Built as `mp_platform`, so they stay compiled and verified
  against Aurora's headers even while the game cannot link.
- `entry.cpp`, `include/port_entry.h` — which disc the port accepts and how it
  finds the image. Game-free, so it is covered by `port_entry_tests`.
- `main.cpp` — the process entry point: Aurora up, disc mounted and checked,
  `InvokeCMain` called, platform down. Compiled by `mp_port_entry`; it cannot link
  until upstream writes `COsContext` and `CMemorySys` (see `../PORT_NOTES.md`).
- `rel.cpp`, `include/port_rel.h` — the REL module runtime (arena, loader, linker,
  registry), tested against a module extracted from an owned disc.

## Reworked for Echoes

- `sdk_stubs.cpp` — the carried-over `OSLink`/`OSUnlink` no-op stubs were removed
  (`port::rel::LinkModule`/`UnlinkModule` own that job now), and the three OS
  context entry points Echoes references were added.
- `main.cpp` — rewritten against Echoes' bootstrap: it drives the decompilation's
  `InvokeCMain` seam instead of Metroid Prime's `metroid_main`, and builds the OS
  context and memory system that seam takes.

## Waiting

- `disc.cpp`, `include/port_disc.h` — reads embedded DOL resources by console
  address (Metroid Prime's font fetch). Echoes' resources are LZO/zlib-compressed
  and split across `main.dol` and the REL modules, so this needs the game's
  resource model, not just the DOL section walk.
- `ai_dma.cpp` — the AI DMA bridge; needs SDL3, which arrives with Aurora's build.
- `debug_ui.cpp` — the F1 overlay. Reads `CStateManager`, `CWorld`, `CPlayerState`,
  `CGameState` and `CMemoryCard` and the MP1 debug hooks (`port_debug.h`).
- `port_textures.cpp`, `port_prompts.cpp`, `port_randomizer.cpp`, `smoke.cpp` —
  HD textures, button prompts, randomizer and the opt-in smoke driver. All
  Metroid-Prime-specific; see the Prime port's `docs/NATIVE_PORT.md` for what each
  does, and expect to rewrite them against Echoes' assets and UI.
