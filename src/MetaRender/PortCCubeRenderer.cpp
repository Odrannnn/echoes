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
  * ## THIS SECTION NO LONGER COMPILES, AND IT CANNOT BE RENAMED ONTO UPSTREAM
  *
  * Every definition below is a member of `CCubeRenderer` that **upstream's `CCubeRenderer` does
  * not declare**, and they are left as they are rather than mapped onto it. The reason is that
  * the merge replaced the interface outright rather than renaming it:
  *
  *   * the pre-merge `include/MetaRender/IRenderer.hpp` has **78 virtual slots** and its comments
  *     name each one (`UnkA` is slot 4, `UnkB(int,int,int)` is 11, `Unk53`..`Unk76` are 53-76);
  *   * upstream's `IRenderer.hpp` has **62**, and the shapes differ in the same places -
  *     `EnablePVS(const CPVSVisSet&, int)` became `EnablePVS(int, const rstl::vector<pair<int,int>>&)`,
  *     `DisablePVS()` became `DisablePVS(int)`, `DrawUnsortedGeometry(int,int,int)` became
  *     `DrawUnsortedGeometry(int)`, `AddStaticGeometry` went from 3 parameters to 6, and
  *     `GetStaticWorldDataSize` from `void` to `int`.
  *
  * So the slot numbers do not line up and the signatures do not match: of the 34 definitions here
  * that no longer compile, only 6 share a name with anything upstream, and **all 6 differ in
  * arity or type**. There is no member offset or layout to check a rename against - the test the
  * merge lane's brief sets, and the one that cannot be passed here. Guessing which of upstream's
  * 62 virtuals each of these 78 was meant to be would be inventing an interface, and it would be
  * wrong in a way that does *not* fail to compile, which is worse.
  *
  * The vtable-filling job is also **moot**: `Carve80270848.cpp` emits `_ZTV13CCubeRenderer` from
  * *upstream's* class, so it has 62 slots to fill, not 82, and it wants upstream's names. These 61
  * hand-written stand-ins were sized for the old table. Whoever finishes this should regenerate the
  * list from the new vtable rather than translate the old one - the same "delete, do not translate"
  * recipe the rest of the port uses for a stale carve.
  *
  * The three definitions above this block (`fn_802C46E0`, `fn_802C4A5C`, `fn_80271238`) are
  * unaffected by all of that and still compile; it is only the member section that does not.
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

// --- stand-ins for every upstream `CCubeRenderer` override no listed TU defines ------------------
//
// Regenerated in the 2026-09-28 upstream merge from the override list in
// `include/MetaRender/CCubeRenderer.hpp`: upstream renamed and re-signed most of the slots the
// earlier `UnkN` stand-ins covered, so the list below is exactly the header's overrides minus the
// ones with real bodies elsewhere in `files.cmake` (the Carve* units: dtor, `BeginScene`,
// `EndScene`, `GetFPS`, the `SetBlendMode_*` family, `PrimNormal`, `PrimColor`, `SetAmbientColor`,
// `SetDepthReadWrite`, `SetDrawableCallback`, `SetModelMatrix`). Every body logs its own name, so a
// call into an unwritten slot is visible rather than quiet. **None of this is retail behaviour.**

void CCubeRenderer::AddStaticGeometry(const rstl::vector< CMetroidModelInstance >* geometry, const CAreaRenderOctTree* octTree, const rstl::vector< SAreaSurface >* surfaces, const rstl::vector< uint >* ambientLightIds, const rstl::vector< signed char >* ambientLightIndices, int areaId) {
  (void)geometry;
  (void)octTree;
  (void)surfaces;
  (void)ambientLightIds;
  (void)ambientLightIndices;
  (void)areaId;
  mpUnwrittenSlot("AddStaticGeometry");
}
void CCubeRenderer::EnablePVS(int areaId, const rstl::vector< rstl::pair< int, int > >& visible) {
  (void)areaId;
  (void)visible;
  mpUnwrittenSlot("EnablePVS");
}
void CCubeRenderer::DisablePVS(int areaId) {
  (void)areaId;
  mpUnwrittenSlot("DisablePVS");
}
void CCubeRenderer::PrepareWorldRendering( const rstl::pair< int, const CPVSVisSet* >* pvsSets, int pvsCount, const CFrustumPlanes& frustum, const rstl::reserved_vector< rstl::pair< int, CFrustumPlanes >, 10 >* areaFrusta, const rstl::vector< CLight >& lights, const rstl::pair< int, float >* ambientLights, int ambientLightCount) {
  (void)pvsSets;
  (void)pvsCount;
  (void)frustum;
  (void)areaFrusta;
  (void)lights;
  (void)ambientLights;
  (void)ambientLightCount;
  mpUnwrittenSlot("PrepareWorldRendering");
}
void CCubeRenderer::RemoveStaticGeometry(const rstl::vector< CMetroidModelInstance >* geometry) {
  (void)geometry;
  mpUnwrittenSlot("RemoveStaticGeometry");
}
void CCubeRenderer::DrawUnsortedGeometry(int areaId) {
  (void)areaId;
  mpUnwrittenSlot("DrawUnsortedGeometry");
}
void CCubeRenderer::DrawSortedGeometry(int mode, int areaId) {
  (void)mode;
  (void)areaId;
  mpUnwrittenSlot("DrawSortedGeometry");
}
void CCubeRenderer::DrawSpecialGeometry(int areaId, int, int) {
  (void)areaId;
  mpUnwrittenSlot("DrawSpecialGeometry");
}
void CCubeRenderer::DrawScanRing(float radius, float thickness, float alpha, float fade, float scanTime, int areaId) {
  (void)radius;
  (void)thickness;
  (void)alpha;
  (void)fade;
  (void)scanTime;
  (void)areaId;
  mpUnwrittenSlot("DrawScanRing");
}
void CCubeRenderer::DrawUnsortedGeometryAlpha(int areaId) {
  (void)areaId;
  mpUnwrittenSlot("DrawUnsortedGeometryAlpha");
}
void CCubeRenderer::DrawSpecialGeometryAlpha(int areaId, int, int) {
  (void)areaId;
  mpUnwrittenSlot("DrawSpecialGeometryAlpha");
}
void CCubeRenderer::DrawAreaModel(int areaId, int modelId, const CModelFlags& flags) {
  (void)areaId;
  (void)modelId;
  (void)flags;
  mpUnwrittenSlot("DrawAreaModel");
}
void CCubeRenderer::PostRenderFogs() {
  mpUnwrittenSlot("PostRenderFogs");
}
void CCubeRenderer::AddParticleGen(const CParticleGen& gen) {
  (void)gen;
  mpUnwrittenSlot("AddParticleGen/1");
}
void CCubeRenderer::AddParticleGen(const CParticleGen& gen, const CVector3f& pos, const CAABox& bounds) {
  (void)gen;
  (void)pos;
  (void)bounds;
  mpUnwrittenSlot("AddParticleGen/3");
}
void CCubeRenderer::AddPlaneObject(const void* obj, const CAABox& bounds, const CPlane& plane, int type) {
  (void)obj;
  (void)bounds;
  (void)plane;
  (void)type;
  mpUnwrittenSlot("AddPlaneObject");
}
void CCubeRenderer::AddDrawable(const void* obj, const CVector3f& pos, const CAABox& bounds, int mode, EDrawableSorting sorting) {
  (void)obj;
  (void)pos;
  (void)bounds;
  (void)mode;
  (void)sorting;
  mpUnwrittenSlot("AddDrawable");
}
void CCubeRenderer::SetWorldViewpoint(const CTransform4f& xf) {
  (void)xf;
  mpUnwrittenSlot("SetWorldViewpoint");
}
void CCubeRenderer::SetPerspective(float fovy, float width, float height, float znear, float zfar) {
  (void)fovy;
  (void)width;
  (void)height;
  (void)znear;
  (void)zfar;
  mpUnwrittenSlot("SetPerspective/5");
}
void CCubeRenderer::SetPerspective(float fovy, float aspect, float znear, float zfar) {
  (void)fovy;
  (void)aspect;
  (void)znear;
  (void)zfar;
  mpUnwrittenSlot("SetPerspective/4");
}
rstl::pair< CVector2f, CVector2f > CCubeRenderer::SetViewportOrtho(bool centered, float znear, float zfar) {
  (void)centered;
  (void)znear;
  (void)zfar;
  mpUnwrittenSlot("SetViewportOrtho");
  // CVector2f has no default constructor, so the pair is built explicitly; a degenerate
  // (0,0)-(0,0) viewport is an obviously wrong answer rather than a plausible one.
  return rstl::pair< CVector2f, CVector2f >(CVector2f(0.f, 0.f), CVector2f(0.f, 0.f));
}
void CCubeRenderer::SetViewport(int left, int top, int width, int height) {
  (void)left;
  (void)top;
  (void)width;
  (void)height;
  mpUnwrittenSlot("SetViewport");
}
void CCubeRenderer::SetDebugOption(EDebugOption option, int value) {
  (void)option;
  (void)value;
  mpUnwrittenSlot("SetDebugOption");
}
void CCubeRenderer::BeginPrimitive(EPrimitiveType primitive, int count) {
  (void)primitive;
  (void)count;
  mpUnwrittenSlot("BeginPrimitive");
}
void CCubeRenderer::BeginLines(int count) {
  (void)count;
  mpUnwrittenSlot("BeginLines");
}
void CCubeRenderer::BeginLineStrip(int count) {
  (void)count;
  mpUnwrittenSlot("BeginLineStrip");
}
void CCubeRenderer::BeginTriangles(int count) {
  (void)count;
  mpUnwrittenSlot("BeginTriangles");
}
void CCubeRenderer::BeginTriangleStrip(int count) {
  (void)count;
  mpUnwrittenSlot("BeginTriangleStrip");
}
void CCubeRenderer::BeginTriangleFan(int count) {
  (void)count;
  mpUnwrittenSlot("BeginTriangleFan");
}
void CCubeRenderer::PrimVertex(const CVector3f& vertex) {
  (void)vertex;
  mpUnwrittenSlot("PrimVertex");
}
void CCubeRenderer::EndPrimitive() {
  mpUnwrittenSlot("EndPrimitive");
}
void CCubeRenderer::DrawString(const char* text, int x, int y) {
  (void)text;
  (void)x;
  (void)y;
  mpUnwrittenSlot("DrawString");
}
void CCubeRenderer::CacheReflection(void (*callback)(void*, const CVector3f&), void* context, bool clear) {
  (void)callback;
  (void)context;
  (void)clear;
  mpUnwrittenSlot("CacheReflection");
}
void CCubeRenderer::DrawSpaceWarp(const CVector3f& point, float strength) {
  (void)point;
  (void)strength;
  mpUnwrittenSlot("DrawSpaceWarp");
}
void CCubeRenderer::DrawModelDisintegrate(const CModel& model, const CTexture& texture, const CColor& color, float amount) {
  (void)model;
  (void)texture;
  (void)color;
  (void)amount;
  mpUnwrittenSlot("DrawModelDisintegrate");
}
void CCubeRenderer::DrawModelFlat(const CModel& model, const CModelFlags& flags, bool unsortedOnly) {
  (void)model;
  (void)flags;
  (void)unsortedOnly;
  mpUnwrittenSlot("DrawModelFlat");
}
void CCubeRenderer::DrawModelProjectedShadow(const CModel& model, const CTexture& texture, const CVector3f& direction, const CColor& color, float scale) {
  (void)model;
  (void)texture;
  (void)direction;
  (void)color;
  (void)scale;
  mpUnwrittenSlot("DrawModelProjectedShadow");
}
void CCubeRenderer::DrawModelNoise(const CModel& model, const CColor& color, bool additive) {
  (void)model;
  (void)color;
  (void)additive;
  mpUnwrittenSlot("DrawModelNoise");
}
bool CCubeRenderer::EnableSilhouetteRender() {
  mpUnwrittenSlot("EnableSilhouetteRender");
  return false;
}
void CCubeRenderer::fn_802679DC(const void* unused, const CModel& model, const CModelFlags& flags) {
  (void)unused;
  (void)model;
  (void)flags;
  mpUnwrittenSlot("fn_802679DC");
}
void CCubeRenderer::DrawSilhouetteNoise(const SSilhouetteNoise& noise) {
  (void)noise;
  mpUnwrittenSlot("DrawSilhouetteNoise");
}
void CCubeRenderer::SetWireframeFlags(int flags) {
  (void)flags;
  mpUnwrittenSlot("SetWireframeFlags");
}
void CCubeRenderer::SetWorldFog(ERglFogMode mode, float start, float end, const CColor& color) {
  (void)mode;
  (void)start;
  (void)end;
  (void)color;
  mpUnwrittenSlot("SetWorldFog");
}
void CCubeRenderer::RenderFogVolume(const CColor& color, const CAABox& bounds, const TLockedToken< CModel >* model, const CSkinnedModel* skinnedModel) {
  (void)color;
  (void)bounds;
  (void)model;
  (void)skinnedModel;
  mpUnwrittenSlot("RenderFogVolume");
}
void CCubeRenderer::SetRequestedMaterialMode(int mode) {
  (void)mode;
  mpUnwrittenSlot("SetRequestedMaterialMode");
}
void CCubeRenderer::DrawDarkWorldVolume(const CVector3f& pos, const CVector3f& scale, uchar mix, uchar alpha, bool inside, float lod, const CVector2f& scroll1, const CVector2f& scroll2, const CVector2f& texScale1, const CVector2f& texScale2, const CTexture& environment, const CTexture& cloud1, const CTexture& cloud2, CColor color, CColor additiveColor, bool cylinder, bool additive) {
  (void)pos;
  (void)scale;
  (void)mix;
  (void)alpha;
  (void)inside;
  (void)lod;
  (void)scroll1;
  (void)scroll2;
  (void)texScale1;
  (void)texScale2;
  (void)environment;
  (void)cloud1;
  (void)cloud2;
  (void)color;
  (void)additiveColor;
  (void)cylinder;
  (void)additive;
  mpUnwrittenSlot("DrawDarkWorldVolume");
}
void CCubeRenderer::DrawDarkWorldFilter(float amount) {
  (void)amount;
  mpUnwrittenSlot("DrawDarkWorldFilter");
}
void CCubeRenderer::DrawScanVisor(float scanTime, float width, float height, const CColor& color, const CColor& scanColor, const CColor& maskColor, const CColor* palette, int paletteSize, const CVector3f& scanRange) {
  (void)scanTime;
  (void)width;
  (void)height;
  (void)color;
  (void)scanColor;
  (void)maskColor;
  (void)palette;
  (void)paletteSize;
  (void)scanRange;
  mpUnwrittenSlot("DrawScanVisor");
}
void CCubeRenderer::DrawScreenFilter(const CColor& color0, const CColor& color1, const CColor& color2) {
  (void)color0;
  (void)color1;
  (void)color2;
  mpUnwrittenSlot("DrawScreenFilter");
}
int CCubeRenderer::GetStaticWorldDataSize() {
  mpUnwrittenSlot("GetStaticWorldDataSize");
  return 0;
}
void CCubeRenderer::SetGXRegister1Color(const CColor& color) {
  (void)color;
  mpUnwrittenSlot("SetGXRegister1Color");
}
void CCubeRenderer::SetWorldLightFadeLevel(float level) {
  (void)level;
  mpUnwrittenSlot("SetWorldLightFadeLevel");
}
CAABox CCubeRenderer::GetAreaModelBounds(int areaId, int modelId) {
  (void)areaId;
  (void)modelId;
  mpUnwrittenSlot("GetAreaModelBounds");
  return CAABox(0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
}
void CCubeRenderer::SetDestinationAlpha(int alpha) {
  (void)alpha;
  mpUnwrittenSlot("SetDestinationAlpha");
}
void CCubeRenderer::DisableDestinationAlpha() {
  mpUnwrittenSlot("DisableDestinationAlpha");
}
void CCubeRenderer::DrawDarkWorldTransition(const CColor& color0, const CColor& color1, const CColor& color2, const CColor& color3, const CVector2i& offset, const CVector2i& sourceSize, const CVector2i& targetSize) {
  (void)color0;
  (void)color1;
  (void)color2;
  (void)color3;
  (void)offset;
  (void)sourceSize;
  (void)targetSize;
  mpUnwrittenSlot("DrawDarkWorldTransition");
}
void CCubeRenderer::CopyTextureRegion(void* dest, int format, int left, int top, int width, int height) {
  (void)dest;
  (void)format;
  (void)left;
  (void)top;
  (void)width;
  (void)height;
  mpUnwrittenSlot("CopyTextureRegion");
}
void CCubeRenderer::DrawDarkWorldCloud(float time, const CVector3f& scale, const CColor& color) {
  (void)time;
  (void)scale;
  (void)color;
  mpUnwrittenSlot("DrawDarkWorldCloud");
}
