/**
 * Host-only stand-ins for the two functions `src/MetaRender/Carve80272958.c` calls, plus the one
 * entry point `src/MetaRender/Carve8026EF54.cpp` needs to reach the real constructor.
 *
 * Nothing here is retail code and nothing here claims to be. The three bodies exist because the
 * retail functions they stand in for are `.text` at 0x802729B4 and 0x802729C0 and are reached only
 * through `fn_80272958`, and because `fn_80271238` - the name `Carve8026EF54.cpp` calls - is **the
 * name dtk gives the claim**, not the name the tree defines: the definition in
 * `src/MetaRender/Carve80271238.cpp` is the C++ member `CCubeRenderer::CCubeRenderer`. Those are
 * two different symbols, so with the constructor compiled for the host and nothing else,
 * `AllocateRenderer` would still call the reach stub. **This file defines the name the caller
 * already uses**, so the fix costs nothing in the `Matching` unit; see the body below.
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
 * `fn_80271238` placement-news a `CCubeRenderer`, so it needs
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
#include <stdio.h>

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

/**
 * `xb8_blackTex`'s bitmap, 4x4 RGB565 = 4 * 4 * 2 = **32 bytes** - and retail's own `memset` at
 * 0x80271780 clears 32, so the two agree independently of each other. It is static, not
 * per-texture, for the same reason the renderer arena above is: there is exactly one black
 * texture and retail's constructor is called at most once per run.
 *
 * **This is not fabricated image data and nothing draws from it.** The constructor's own
 * `memset(fn_802C46E0(&xb8_blackTex, 0), 0, 32)` is what establishes that it is zeroed, so
 * returning a zeroed block means the `memset` is a no-op on already-zero memory - which is
 * exactly the state retail leaves it in. `CTexture::Load` (a GX texture upload) is never
 * reached on a PC, so this is never a framebuffer.
 */
alignas(8) uchar s_blackTexBitmap[32];
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
 * `fn_802C46E0` (retail 0x802C46E0) - `CTexture::GetBitMapData(int)`, the texture's own bitmap
 * pointer. **Retail's method is not written in this tree** (`src/Kyoto/Graphics/CTexture.cpp` is
 * in no manifest, and retail's `0x802C46E0` is unclaimed), so the constructor's call lands on
 * `tools/boot_probe.sh`'s `extern "C" void fn_802C46E0(void)` auto-stub, **which returns
 * whatever `printf` left in `%rax` - and the very next instruction is retail's
 * `memset(fn_802C46E0(...), 0, 32)`.** That is the measured crash: `movups %xmm0,(%rax)` at the
 * constructor's host offset `+0x52a`, in the run recorded in `docs/HANDOFF.md`.
 *
 * So the fix is upstream of the stub and it is small: hand back the texture's own 32 bytes. The
 * `memset` then does what retail's does. **This is a stand-in and not a claim that the method is
 * written** - it is defined here under a C name that `Carve80271238.cpp` declares, so a lane
 * that writes retail's `CTexture::GetBitMapData` does **not** collide with it; a lane that
 * carves `fn_802C46E0` itself would, and that is the four-times-seen stale-alias shape, so the
 * recipe is the one `tools/boot_probe.sh` prints: delete this definition, not the carve.
 */
void* fn_802C46E0(CTexture* tex, int mip) {
  (void)tex;
  (void)mip;
  return s_blackTexBitmap;
}

/**
 * `fn_802C4A5C` (retail 0x802C4A5C) - `CTexture::UnLock()`. Also unwritten, also auto-stubbed, and
 * a no-op is the whole of its contract on a PC: there is no EFB copy to release, because
 * `CTexture::Load` is never reached. Same note as `fn_802C46E0` about a future carve colliding.
 */
void fn_802C4A5C(CTexture* tex) {
  (void)tex;
}

/**
 * `fn_80271238` - `CCubeRenderer`'s constructor, as the port reaches it.
 *
 * **`src/MetaRender/Carve8026EF54.cpp` calls this name**, not `mp_CCubeRenderer_ctor`. That unit
 * is `Matching`, so changing the name it calls changes the relocation it emits and costs the
 * unit; defining the name it *already* calls costs nothing. The earlier version of this file
 * defined a differently-named bridge and left the caller to be edited, which is the more
 * expensive of two ways to fix the same mismatch.
 *
 * ## The `boot: step 21c` marker, and why it is here
 *
 * This is the only place on a host where "the constructor returned" is knowable: the ladder in
 * `src/MetroidPrime/PortBoot.cpp` is one level up, inside `PostInitialize`, and the constructor
 * is called from `AllocateRenderer`, so nothing above it can tell a completed constructor from a
 * stubbed one. **`fflush(nullptr)` after the marker**, and not `fflush(stdout)`: the markers'
 * whole job is to survive a fault, and the step-17 lane measured that `stdout` is the one new
 * *data* symbol `fflush` puts in the port's link gap (`--allow-shlib-undefined` satisfies a
 * function from libc's dynamic table but not a data symbol, which wants a copy relocation).
 *
 * ## What the marker does not claim
 *
 * **A returned constructor is not a rendered frame, and nothing here prints a pixel.** The eight
 * `TLockedToken`s the constructor loads now resolve to *stand-ins* - a `CTexture` with no bitmap
 * and a `CModel` with no parts, registered in `src/MetroidPrime/PortPoolStandIns.cpp` and named
 * there as stand-ins - so the object is constructed and the tokens hold real pointers, and a
 * draw through them would have no geometry and no texture to draw. `BeginScene` (vtable slot 35,
 * the frame loop's first call) is a GX state setup rather than a draw, and the five tail methods
 * this constructor calls are unwritten retail `.text`. See `docs/HANDOFF.md`, "No frame, and the
 * next wall is named and is not decompilation".
 *
 * Retail's constructor writes through `this` and returns its own `this`, so this returns `self`
 * unchanged and the caller's `p ? p + 4 : p` is unaffected.
 */
void* fn_80271238(void* self, IObjectStore& store, COsContext& osContext, CMemorySys& memorySys,
                  IFactory& resFactory) {
  CCubeRenderer* renderer = static_cast< CCubeRenderer* >(self);
  ::new (renderer) CCubeRenderer(store, osContext, memorySys, resFactory);
  printf("boot: step 21c returned - CCubeRenderer's constructor completed, 8 pool tokens\n");
  fflush(nullptr);
  return self;
}
} // extern "C"

/**
  * The 61 vtable slots retail's key function emits and this tree has no body for.
 *
 * `src/MetaRender/Carve80270848.cpp` is `~CCubeRenderer`, the class's key function, and **a vtable
 * is emitted only by the translation unit that defines it.** Retail's vtable has 82 entries; the
 * carve emits it as `.data.rel.ro` with a relocation per slot, so **every slot must resolve or the
 * port does not link** - which is exactly what happened: 23
 * undefined symbols, all of them `CCubeRenderer::`. `tools/probe_sources.sh` does not see this,
 * because it *compiles* the sources and never links them. The compile-only gate passing while the
 * link was broken is the whole reason this section had to be written by hand.
 *
 * **Measured on the linked port, 2026-09-27:** `readelf -rW` on `Carve80270848.cpp.o` gives **84
 * relocation entries** in `.rela.data.rel.ro._ZTV13CCubeRenderer` (83 unique symbols - the typeinfo
 * pointer, both destructor entries, and 3 non-virtual thunks to the `IWeaponRenderer` half). Of
 * those 83, **61 are defined in this file** and 22 elsewhere (`~CCubeRenderer`, `BeginScene`,
 * `SetModelMatrix`, `SetAmbientColor`, `SetDrawableCallback`, `PrimNormal`, the two `PrimColor`s,
 * `GetFPS`, `SetDepthReadWrite` and the ten `SetBlendMode_*`), and **0 are unresolved**. That last
 * number is the one that matters, and it is only meaningful because the linker agrees: the port's
 * own log went from 14 `CCubeRenderer::` undefined to 0. Note it is `readelf` against the object
 * and the *linker's* log, not the count of definitions in this file - a body the linker never asks
 * for is dead weight, and the 47 this file defined before the third wave were 47 slots, not 45.
 *
 * **These are not retail behaviour and they do not claim to be.** Each one logs its own name and
 * returns, so a call into an unwritten slot is visible in the log rather than silently returning a
 * plausible value. That is the whole discipline: a stub that announces itself cannot fake a frame,
 * and a stub that returns 0 quietly could.
 *
 * **Nothing on the path to a first frame calls these.** The boot ladder constructs the object and
 * `BeginScene` (slot 35, the frame loop's first call, and the one slot with a real body) sets GX
 * state. A returned constructor plus GX state is still not a drawn frame, and nothing here prints a
 * pixel. When a lane writes any of these, it deletes the corresponding definition here - the same
 * recipe `tools/boot_probe.sh` prints, and the reason the reason is written down.
 */
namespace {
/** Announce an unwritten vtable slot. Never returns a value, so no slot can invent one. */
void mpUnwrittenSlot(const char* name) {
  printf("[CCubeRenderer] slot %s has no decompiled body - stand-in, retail behaviour NOT "
         "reproduced\n",
         name);
  fflush(nullptr);
}
} // namespace

void CCubeRenderer::GetStaticWorldDataSize() { mpUnwrittenSlot("GetStaticWorldDataSize"); }
void CCubeRenderer::SetWireframeFlags(int flags) {
  (void)flags;
  mpUnwrittenSlot("SetWireframeFlags");
}
void CCubeRenderer::SetWorldFog(ERglFogMode mode, float startz, float endz, const CColor& color) {
  (void)mode;
  (void)startz;
  (void)endz;
  (void)color;
  mpUnwrittenSlot("SetWorldFog");
}
void CCubeRenderer::Unk56() { mpUnwrittenSlot("Unk56"); }
void CCubeRenderer::Unk57() { mpUnwrittenSlot("Unk57"); }
void CCubeRenderer::Unk58() { mpUnwrittenSlot("Unk58"); }
void CCubeRenderer::Unk59() { mpUnwrittenSlot("Unk59"); }
void CCubeRenderer::Unk62() { mpUnwrittenSlot("Unk62"); }
void CCubeRenderer::Unk63() { mpUnwrittenSlot("Unk63"); }
void CCubeRenderer::Unk64() { mpUnwrittenSlot("Unk64"); }
void CCubeRenderer::Unk65() { mpUnwrittenSlot("Unk65"); }
void CCubeRenderer::Unk66() { mpUnwrittenSlot("Unk66"); }
void CCubeRenderer::Unk67() { mpUnwrittenSlot("Unk67"); }
void CCubeRenderer::Unk69() { mpUnwrittenSlot("Unk69"); }
void CCubeRenderer::Unk70() { mpUnwrittenSlot("Unk70"); }
void CCubeRenderer::Unk71() { mpUnwrittenSlot("Unk71"); }
void CCubeRenderer::Unk74() { mpUnwrittenSlot("Unk74"); }
void CCubeRenderer::Unk75() { mpUnwrittenSlot("Unk75"); }
void CCubeRenderer::Unk76() { mpUnwrittenSlot("Unk76"); }
void CCubeRenderer::UnkH(int arg) {
  (void)arg;
  mpUnwrittenSlot("UnkH");
}
void CCubeRenderer::UnkI() { mpUnwrittenSlot("UnkI"); }
void CCubeRenderer::UnkL(const CVector3f& pos, const CColor& color) {
  (void)pos;
  (void)color;
  mpUnwrittenSlot("UnkL");
}
void CCubeRenderer::AddParticleGen(const CParticleGen& gen) {
  (void)gen;
  mpUnwrittenSlot("AddParticleGen");
}

// --- second wave: 23 more, which the first wave is what revealed -------------------------
//
// **The set was 45 when this wave was written, not 23, and the first 23 was the part that was
// visible.** Defining the 22 members above made this file emit the class's vtable, and *that* is
// what made the linker start asking for
// the other 23 - the drawing half, `BeginPrimitive` through `SetWorldViewpoint`. So the count grew
// as a consequence of fixing it, which is the least convenient order a problem can arrive in and
// the reason to expect a second wave rather than to trust the first number. These are slots 18-52
// of retail's 82; the first wave was 60-77. A third wave (slots 1-13 and 16, below) took the file's
// vtable-slot count to 61 - **45 was a checkpoint, and it was two short**, and the handoff's
// "remaining 19" was one short of the 20 the link log actually names.
//
// `BeginScene` (slot 35, 0x8026FBFC) is **not** here: it is `src/MetaRender/Carve8026FBFC.cpp` and
// has a real body. `EndScene` (slot 36, 0x8026FB80) is only 124 bytes earlier and is unwritten, so
// **slot 36 returning and slot 35 being real is the current shape of the frame path** - `BeginScene`
// sets GX state and `EndScene`, which would close it, does not exist yet. Nothing here prints a
// pixel, and a stub that logs cannot pretend otherwise.

void CCubeRenderer::AddDrawable(const void* obj, const CVector3f& pos, const CAABox& bounds,
                                int mode, IRenderer::EDrawableSorting sorting) {
  (void)obj;
  (void)pos;
  (void)bounds;
  (void)mode;
  (void)sorting;
  mpUnwrittenSlot("AddDrawable");
}
void CCubeRenderer::AddPlaneObject(const void* obj, const CAABox& aabb, const CPlane& plane,
                                   int type) {
  (void)obj;
  (void)aabb;
  (void)plane;
  (void)type;
  mpUnwrittenSlot("AddPlaneObject");
}
void CCubeRenderer::SetWorldViewpoint(const CTransform4f& xf) {
  (void)xf;
  mpUnwrittenSlot("SetWorldViewpoint");
}
void CCubeRenderer::SetPerspective(float a, float b, float c, float d, float e) {
  (void)a;
  (void)b;
  (void)c;
  (void)d;
  (void)e;
  mpUnwrittenSlot("SetPerspective/5");
}
void CCubeRenderer::SetPerspective(float a, float b, float c, float d) {
  (void)a;
  (void)b;
  (void)c;
  (void)d;
  mpUnwrittenSlot("SetPerspective/4");
}
rstl::pair< CVector2f, CVector2f > CCubeRenderer::SetViewportOrtho(bool centered, float znear,
                                                                   float zfar) {
  (void)centered;
  (void)znear;
  (void)zfar;
  mpUnwrittenSlot("SetViewportOrtho");
  // The one slot in this file that must return a value rather than void. `rstl::pair`'s default
  // constructor is unusable here - `CVector2f` has none, only `CVector2f(float, float)` - so the
  // pair is built explicitly.
  //
  // It returns **(0,0)-(0,0)**, a degenerate viewport, and logs before doing so. It is on the
  // projection path, so a caller that trusted it would get a viewport with no area: an obviously
  // wrong answer that announces itself, chosen over a plausible-looking one that does not.
  return rstl::pair< CVector2f, CVector2f >(CVector2f(0.f, 0.f), CVector2f(0.f, 0.f));
}
void CCubeRenderer::SetViewport(int left, int right, int width, int height) {
  (void)left;
  (void)right;
  (void)width;
  (void)height;
  mpUnwrittenSlot("SetViewport");
}
void CCubeRenderer::SetDebugOption(EDebugOption option, int value) {
  (void)option;
  (void)value;
  mpUnwrittenSlot("SetDebugOption");
}
void CCubeRenderer::EndScene() { mpUnwrittenSlot("EndScene"); }
void CCubeRenderer::BeginPrimitive(IRenderer::EPrimitiveType prim, int count) {
  (void)prim;
  (void)count;
  mpUnwrittenSlot("BeginPrimitive");
}
void CCubeRenderer::BeginLines(int nverts) {
  (void)nverts;
  mpUnwrittenSlot("BeginLines");
}
void CCubeRenderer::BeginLineStrip(int nverts) {
  (void)nverts;
  mpUnwrittenSlot("BeginLineStrip");
}
void CCubeRenderer::BeginTriangles(int nverts) {
  (void)nverts;
  mpUnwrittenSlot("BeginTriangles");
}
void CCubeRenderer::BeginTriangleStrip(int nverts) {
  (void)nverts;
  mpUnwrittenSlot("BeginTriangleStrip");
}
void CCubeRenderer::BeginTriangleFan(int nverts) {
  (void)nverts;
  mpUnwrittenSlot("BeginTriangleFan");
}
void CCubeRenderer::PrimVertex(const CVector3f& vtx) {
  (void)vtx;
  mpUnwrittenSlot("PrimVertex");
}
void CCubeRenderer::EndPrimitive() { mpUnwrittenSlot("EndPrimitive"); }
void CCubeRenderer::DrawString(const char* str, int x, int y) {
  (void)str;
  (void)x;
  (void)y;
  mpUnwrittenSlot("DrawString");
}
void CCubeRenderer::CacheReflection(TReflectionCallback cb, void* ctx, bool clearAfter) {
  (void)cb;
  (void)ctx;
  (void)clearAfter;
  mpUnwrittenSlot("CacheReflection");
}
void CCubeRenderer::DrawSpaceWarp(const CVector3f& pt, float strength) {
  (void)pt;
  (void)strength;
  mpUnwrittenSlot("DrawSpaceWarp");
}
void CCubeRenderer::Unk53() { mpUnwrittenSlot("Unk53"); }
void CCubeRenderer::Unk54() { mpUnwrittenSlot("Unk54"); }
void CCubeRenderer::Unk55() { mpUnwrittenSlot("Unk55"); }

// --- third wave: retail's vtable slots 1-13 and 16, the drawing-and-scene half ---------------
//
// `docs/HANDOFF.md` records the tail of the port's link gap as "the remaining 19" and lists 14
// `CCubeRenderer::` members plus "5 that are NOT `CCubeRenderer` members". **The second list has
// six names in it and the arithmetic does not close: 14 + 6 = 20, not 19.** Measured, not
// recalled: `grep "undefined reference to" build-port-link/build.log` names all six, and
// `readelf -rW` on `Carve80270848.cpp.o` shows that only 14 of the 82 vtable slots are
// unresolved - `SAreaListItem::~SAreaListItem` and `SFogVolumeListItem::~SFogVolumeListItem`
// appear in a *different* relocation section (the destructor's inline `rstl::list` teardown at
// `Carve80270848.cpp`+0x1bd and +0x11d), not in `_ZTV13CCubeRenderer`, and
// `CGraphics::SetViewport` / `CGraphics::mViewport` are reached by direct `bl` from
// `Carve8026FBFC.cpp`+0x3f / +0x0f rather than through the vtable at all. So the honest starting
// number for this file's work is **14 vtable slots**, and the six belong to other classes.
//
// The 14 below are in **vtable-slot order**, not alphabetical order, so the slot each one fills is
// readable off the list and can be checked against the header's own numbering. The mangled names
// the linker wants were read out of `readelf -rW ... _ZTV13CCubeRenderer` rather than retyped from
// the header, and every signature is copied from `include/MetaRender/CCubeRenderer.hpp` verbatim -
// `AddParticleGen` is an **overload**, so the 1-argument definition above is untouched and only
// slot 16's is added.
//
// **None of these is on the path to a first frame.** Slots 1-13 are the static-geometry, PVS and
// fog half of a scene, all of which the boot ladder never reaches: it constructs the object and
// calls `BeginScene` (slot 35) and stops. Nothing here writes a pixel, and every one of them logs
// its own name through `mpUnwrittenSlot`, so a call into an unwritten slot is visible in the log
// rather than returning quietly.
//
// **`rstl::vector< CMetroidModelInstance >*` is taken by pointer, not by reference**, exactly as
// the header declares it at slot 1 and slot 5, and it is a pointer *to* an incomplete-by-
// intention type rather than to `rstl::vector`'s internals - the body never dereferences it.

// 1
void CCubeRenderer::AddStaticGeometry(const rstl::vector< CMetroidModelInstance >* geometry,
                                      const CAreaOctTree* octTree, int areaIdx) {
  (void)geometry;
  (void)octTree;
  (void)areaIdx;
  mpUnwrittenSlot("AddStaticGeometry");
}
// 2
void CCubeRenderer::EnablePVS(const CPVSVisSet& set, int areaIdx) {
  (void)set;
  (void)areaIdx;
  mpUnwrittenSlot("EnablePVS");
}
// 3
void CCubeRenderer::DisablePVS() { mpUnwrittenSlot("DisablePVS"); }
// 4
void CCubeRenderer::UnkA() { mpUnwrittenSlot("UnkA"); }
// 5
void CCubeRenderer::RemoveStaticGeometry(const rstl::vector< CMetroidModelInstance >* geometry) {
  (void)geometry;
  mpUnwrittenSlot("RemoveStaticGeometry");
}
// 6
void CCubeRenderer::DrawUnsortedGeometry(int areaIdx, int mask, int targetMask) {
  (void)areaIdx;
  (void)mask;
  (void)targetMask;
  mpUnwrittenSlot("DrawUnsortedGeometry");
}
// 7
void CCubeRenderer::DrawSortedGeometry(int areaIdx, int mask, int targetMask) {
  (void)areaIdx;
  (void)mask;
  (void)targetMask;
  mpUnwrittenSlot("DrawSortedGeometry");
}
// 8
void CCubeRenderer::DrawStaticGeometry(int areaIdx, int mask, int targetMask) {
  (void)areaIdx;
  (void)mask;
  (void)targetMask;
  mpUnwrittenSlot("DrawStaticGeometry");
}
// 9
void CCubeRenderer::DrawAreaGeometry(int areaIdx, int mask, int targetMask) {
  (void)areaIdx;
  (void)mask;
  (void)targetMask;
  mpUnwrittenSlot("DrawAreaGeometry");
}
// 10
void CCubeRenderer::PostRenderFogs() { mpUnwrittenSlot("PostRenderFogs"); }
// 11
void CCubeRenderer::UnkB(int areaIdx, int mask, int targetMask) {
  (void)areaIdx;
  (void)mask;
  (void)targetMask;
  mpUnwrittenSlot("UnkB");
}
// 12
void CCubeRenderer::UnkC() { mpUnwrittenSlot("UnkC"); }
// 13
void CCubeRenderer::UnkD() { mpUnwrittenSlot("UnkD"); }
// 16 - the overload. The 1-argument one at slot 15 is above and is deliberately not touched:
// `IWeaponRenderer` reaches it through retail's `@4@AddParticleGen__13CCubeRendererFRC12CParticleGen`
// thunk (0x80273F9C), so both symbols are live and renaming either would be a silent behaviour
// change rather than a tidy-up.
void CCubeRenderer::AddParticleGen(const CParticleGen& gen, const CVector3f& pos,
                                   const CAABox& bounds) {
  (void)gen;
  (void)pos;
  (void)bounds;
  mpUnwrittenSlot("AddParticleGen/3");
}

