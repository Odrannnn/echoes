/**
 * Host-only stand-ins for the two functions `src/MetaRender/Carve80272958.c` calls, plus the one
 * entry point `src/MetaRender/Carve8026EF54.cpp` needs to reach the real constructor.
 *
 * Nothing here is retail code and nothing here claims to be. The three bodies exist because the
 * retail functions they stand in for are `.text` at 0x802729B4 and 0x802729C0 and are reached only
 * through `fn_80272958`, and because `fn_80271238` is **the name dtk gives the claim**, not the
 * name the tree defines: the definition in `src/MetaRender/Carve80271238.cpp` is the C++ member
 * `CCubeRenderer::CCubeRenderer`, and `Carve8026EF54.cpp` calls it through
 * `extern "C" void* fn_80271238(void*, ...)`. Those are two different symbols, so with the
 * constructor compiled for the host and nothing else, `AllocateRenderer` would still call the
 * reach stub. That mismatch is a bug in `Carve8026EF54.cpp` and this file is the fix; the bridge
 * is `mp_CCubeRenderer_ctor`.
 *
 * ## `fn_802729B4` is a fixed arena, not a heap, and the host keeps it that way
 *
 * Retail's `fn_802729B4` is three instructions that return the constant **0x803DEF28**, which is in
 * the DOL's unbacked gap between `.data` and `.sdata` - a `.bss` object. `fn_80272958` therefore
 * hands out **one** 1376-byte block, ever, and `fn_80272988` is a refcount decrement rather than a
 * free. The host translation below returns one 1376-byte static arena for the same reason and
 * deliberately does **not** call `CMemory::Alloc`: matching retail means "one block, and a second
 * `AllocateRenderer` overwrites the first", and an arena says that where a malloc would hide it.
 * The consequence is written down because it is real - two live renderers is not a state retail
 * can reach and this is not a general allocator.
 *
 * ## The key function is the other half of this
 *
 * `mp_CCubeRenderer_ctor` placement-news a `CCubeRenderer`, so it needs
 * **`vtable for CCubeRenderer`**. A vtable is emitted only by the translation unit that defines the
 * class's key function, which for this class is `~CCubeRenderer` -
 * `src/MetaRender/Carve80270848.cpp`. Measured on the host object of that file: it emits
 * `vtable for CCubeRenderer`, `typeinfo for CCubeRenderer` and the `IWeaponRenderer`/`IRenderer`
 * tables, and `_ZTV13CCubeRenderer` is 0x2b0 bytes with `CCubeRenderer::BeginScene` at entry
 * offset 0x130 (slot 35 of retail's 82, which is offset 0x94 in retail's own 4-byte-entry table).
 * **Without `Carve80270848.cpp` in `files.cmake` this file does not link**, and that is the one
 * manifest line this whole path depends on.
 */

#include "MetaRender/CCubeRenderer.hpp"

#include "Kyoto/Alloc/CMemorySys.hpp"
#include "Kyoto/Basics/COsContext.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/IObjectStore.hpp"

#include <new>

#if defined(__MWERKS__) || defined(CLANGD)
#error "this file is host-only: it defines TARGET_PC stand-ins for retail .text functions"
#endif

namespace {
/**
 * Retail's `.bss` arena at 0x803DEF28. 1376 = 0x560 bytes, which is
 * `sizeof(CCubeRenderer)` as mwcceppc measures the header - see
 * `include/MetaRender/CCubeRenderer.hpp`.
 */
alignas(8) uchar s_rendererArena[1376];

/** Retail's `.sbss` flag at 0x804197CC, which `fn_802729C0` only ever stores 1 into. */
bool s_poolInitialised = false;

/** Retail's `.sbss` word at 0x804197C8, the refcount `fn_80272958` increments. */
int s_refCount = 0;
} // namespace

extern "C" {
/** 0x802729C0. Returns `&s_refCount`, after the retail "already initialised?" test. */
void* fn_802729C0() {
  if (!s_poolInitialised) {
    s_poolInitialised = true;
  }
  return &s_refCount;
}

/** 0x802729B4. Retail returns the constant 0x803DEF28; the host returns the arena above. */
void* fn_802729B4() {
  return s_rendererArena;
}

/**
 * The bridge `src/MetaRender/Carve8026EF54.cpp` calls instead of `extern "C" fn_80271238`, which is
 * a claim name and not a definition. Retail's constructor writes through `this` and returns its
 * own `this`, so this returns `self` unchanged and the caller's `p ? p + 4 : p` is unaffected.
 */
void* mp_CCubeRenderer_ctor(void* self, IObjectStore& store, COsContext& osContext,
                            CMemorySys& memorySys, IFactory& resFactory) {
  CCubeRenderer* renderer = static_cast< CCubeRenderer* >(self);
  ::new (renderer) CCubeRenderer(store, osContext, memorySys, resFactory);
  return self;
}
} // extern "C"
