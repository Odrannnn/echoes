// `LoadTypedefSLdrEditorProperties` and its one callee, from retail's own instructions. Both
// addresses are in G2ME01 `config/G2ME01/symbols.txt` and were read with
// `tools/dis.sh 0x8023EF3C 0x140` and `tools/dis.sh 0x8023F8CC 0x9C`; the one place this file
// does not spell retail's call out is the vector read, which the last section justifies.
//
// (Retail's own name, `LoadTypedefSLdrEditorProperties__FR20SLdrEditorPropertiesR12CInputStream` in
// symbols.txt; the 2026-09-28 upstream merge took it over the port's earlier `LoadTypedefEditorProperties`.)
//
// ## Why this file is port-only (in `files.cmake`, absent from `configure.py`)
//
// `0x8023EF3C` is in an unclaimed `.text` range: the nearest split blocks are
// `MetroidPrime/ScriptLoader/RubiksPuzzle.cpp` ending at `0x802399F4` and
// `MetroidPrime/ScriptLoader.cpp` starting at `0x80242894`, so there is no unit that owns
// these bytes and therefore nothing for `configure.py` to mark `Matching` and nothing for
// `tools/flip_test.sh` to flip. Carving the range would be a different job - four files
// (`configure.py`, `config/G2ME01/splits.txt`, `files.cmake`, the source's own claim) in one
// change - and is not what this file is for. It is listed in `files.cmake` only, so the
// port's `mp_game` compiles it and `LoadTypedefSLdrEditorProperties` leaves the port's
// undefined-symbol list. `src/MetroidPrime/ScriptLoader/SLdrStructMembers.cpp` is the
// precedent: a file of `SLdr*` struct bodies that is absent from `configure.py` and listed
// in `files.cmake` for exactly that reason.
//
// The port asked for it: `build/goal/judge/undef.base.txt` recorded it undefined at the
// branch head with ten referring objects (`CScanTreeInventory.cpp.o` ... `CUnknown90.cpp.o`),
// and `docs/research/boot_path_undefined.txt` has it on the boot path.
//
// ## The retail body, property by property
//
// Retail reads `propertyCount` as a `u16`, then per property `propertyId` as a word and
// `propertySize` as a `u16`, and switches on the id - the same shape every generated
// `LoadTypedef*` has (see `SLdrTweakTargeting_Scan.cpp`). The four ids are the header's own
// comments, and each store below is the one the disassembly makes:
//
//   0x494E414D  name                constructs a temporary `rstl::string` from the stream,
//                                   `assign`s it over `name`, destroys the temporary - which is
//                                   exactly what `sldrThis.name = rstl::string(input);` emits
//   0x5846524D  transform           calls `fn_8023F8CC` with `&transform` and the stream
//   0x41435456  active              one byte read, normalised to 0/1 (`neg`/`or`/`srwi 31`),
//                                   stored at +0x34 - `input.ReadBool()` is `ReadUint8() != 0`
//   0x5D298A43  unknown_0x5d298a43  one word read, stored at +0x38
//   default                         `ReadBytes(nullptr, propertySize)`, retail `0x8023F050`
//
// Retail's struct is 60 bytes: `name` 0x00, `transform` 0x10 (three CVector3f), `active`
// 0x34, `unknown` 0x38. That is the header's declaration order, so no member here is
// invented.
//
// ## `LoadTypedefSLdrTransform` is retail's `fn_8023F8CC`
//
// `0x8023F8CC`, size `0x9C`, reads three `CVector3f` from the stream into offsets 0x00, 0x0C
// and 0x18 of its first argument - position, rotation, scale, in declaration order. It is
// unnamed in `symbols.txt` because it has exactly one caller in the whole DOL (measured by
// scanning `.text` for `bl 0x8023F8CC`: one hit, at `0x8023F00C`, inside the function above),
// and `include/MetroidPrime/ScriptLoader/Structs/SLdrEditorProperties.hpp:18` already declares
// it as `LoadTypedefSLdrTransform`. Writing it here rather than calling an undefined symbol is
// what keeps the port's undefined count from rising: the target's `transform` case would
// otherwise open a new gap while closing one, and the boot probe would fail to link, because
// this helper is not in `docs/research/boot_path_reachable.tsv` and no reach stub provides it.
//
// ## Why the three vectors are read as three floats
//
// Retail's helper calls `__ct__9CVector3fFR12CInputStream` three times (`0x8023F8EC`,
// `0x8023F910`, `0x8023F934`), and on a big-endian PowerPC that constructor is
// `in.Get(this, sizeof(CVector3f))` - a plain twelve-byte copy. The port's own copy of it is
// still exactly that (`src/Kyoto/Math/CVector3f.cpp:24`, a `Matching` unit whose body may not
// change), and `CInputStream::Get(void*, unsigned long)` is a plain `memcpy`
// (`src/Kyoto/Streams/CInputStream.cpp:51`) with no `TARGET_PC` conversion - so on this
// little-endian host it hands back the stream's big-endian float bytes in stream order, which
// is not retail's value. `CInputStream::ReadFloat()` is: under `TARGET_PC` it goes through
// `ReadInt32` and `cinput_stream_read_be32`, which is why `CColor::CColor(CInputStream&)`
// (`src/Kyoto/Graphics/DolphinCColor.cpp:6`) spells its four floats out that way. Hence the
// local helper, and hence not three `ReadFloat()`s in one argument list - argument evaluation
// order is unspecified, and the vector reads would come out reversed.
//
// `CScriptPickup.cpp:289/292/361` reads its three vector properties with `CVector3f(input)`
// and inherits the same conversion gap. That is pre-existing shared code and is deliberately
// not touched here: this item is one function.

#include "MetroidPrime/ScriptLoader/Structs/SLdrEditorProperties.hpp"

namespace {

CVector3f ReadVector3f(CInputStream& input) {
  const float x = input.ReadFloat();
  const float y = input.ReadFloat();
  const float z = input.ReadFloat();
  return CVector3f(x, y, z);
}

} // namespace

void LoadTypedefSLdrTransform(SLdrTransform& data, CInputStream& input) {
  data.position = ReadVector3f(input);
  data.rotation = ReadVector3f(input);
  data.scale = ReadVector3f(input);
}

void LoadTypedefSLdrEditorProperties(SLdrEditorProperties& sldrThis, CInputStream& input) {
  const int propertyCount = input.ReadUint16();
  for (int i = 0; i < propertyCount; ++i) {
    const uint propertyId = input.ReadInt32();
    const u16 propertySize = input.ReadUint16();
    switch (propertyId) {
    case 0x5846524d: {
      LoadTypedefSLdrTransform(sldrThis.transform, input);
      break;
    }
    case 0x494e414d: {
      sldrThis.name = rstl::string(input);
      break;
    }
    case 0x41435456: {
      sldrThis.active = input.ReadBool();
      break;
    }
    case 0x5d298a43: {
      sldrThis.unknown_0x5d298a43 = input.ReadInt32();
      break;
    }
    default:
      input.ReadBytes(nullptr, propertySize);
      break;
    }
  }
}
