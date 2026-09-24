// Entry-point helpers: the disc the port accepts and how it finds the image.
// These are the first things that run, so they get checked directly.

#include "port_entry.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {
using port::entry::IsDiscImageName;
using port::entry::IsSupportedDisc;
using port::entry::ResolveDiscPath;

int g_failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    g_failures++;
  }
}

void CheckEq(const std::string& actual, const std::string& expected, const std::string& what) {
  if (actual != expected) {
    std::fprintf(stderr, "FAIL: %s: got \"%s\" expected \"%s\"\n", what.c_str(), actual.c_str(),
                 expected.c_str());
    g_failures++;
  }
}

DVDDiskID MakeDiscId(const char* gameName, const char* company, uint8_t diskNumber,
                     uint8_t gameVersion) {
  DVDDiskID id{};
  std::memcpy(id.gameName, gameName, 4);
  std::memcpy(id.company, company, 2);
  id.diskNumber = diskNumber;
  id.gameVersion = gameVersion;
  return id;
}

void TestSupportedDisc() {
  Check(IsSupportedDisc(MakeDiscId("G2ME", "01", 0, 0)), "G2ME01 disc 0 rev 0 is supported");
  Check(!IsSupportedDisc(MakeDiscId("GM8E", "01", 0, 0)), "Prime 1's disc is rejected");
  Check(!IsSupportedDisc(MakeDiscId("G2ME", "01", 0, 1)), "a later revision is rejected");
  Check(!IsSupportedDisc(MakeDiscId("G2ME", "01", 1, 0)), "disc 2 is rejected");
  Check(!IsSupportedDisc(MakeDiscId("G2MJ", "01", 0, 0)), "the Japanese disc is rejected");
  Check(!IsSupportedDisc(MakeDiscId("G2MP", "01", 0, 0)), "the PAL disc is rejected");
  Check(!IsSupportedDisc(MakeDiscId("G2ME", "08", 0, 0)), "a third-party publisher is rejected");
  Check(!IsSupportedDisc(MakeDiscId("R3ME", "01", 0, 0)), "the Trilogy disc is rejected");
}

void TestDiscImageNames() {
  Check(IsDiscImageName("Metroid Prime 2 (USA) (v1.00).iso"), "an .iso name is accepted");
  Check(IsDiscImageName("GAME.ISO"), "the extension check is case-insensitive");
  Check(IsDiscImageName("disc.gcm"), "a .gcm name is accepted");
  Check(!IsDiscImageName("disc.rvz"), "a compressed image is not accepted");
  Check(!IsDiscImageName("iso"), "a name without an extension is not accepted");
  Check(!IsDiscImageName(".iso"), "a bare extension is not accepted");
}

void TestResolveDiscPath() {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "metroid_prime2_port_entry_test";
  std::error_code error;
  std::filesystem::remove_all(dir, error);
  std::filesystem::create_directories(dir, error);
  Check(!error, "temp directory is created");
  const std::filesystem::path secondImage = dir / "b.iso";
  const std::filesystem::path firstImage = dir / "a.iso";
  {
    std::FILE* file = std::fopen(firstImage.string().c_str(), "wb");
    if (file != nullptr) {
      std::fwrite("x", 1, 1, file);
      std::fclose(file);
    }
    file = std::fopen(secondImage.string().c_str(), "wb");
    if (file != nullptr) {
      std::fwrite("x", 1, 1, file);
      std::fclose(file);
    }
    file = std::fopen((dir / "notes.txt").string().c_str(), "wb");
    if (file != nullptr) {
      std::fclose(file);
    }
  }

  {
    std::vector<char*> argv{const_cast<char*>("port"), const_cast<char*>("/explicit/disc.iso")};
    CheckEq(ResolveDiscPath(static_cast<int>(argv.size()), argv.data(), "/from/env.iso", dir.string()),
            "/explicit/disc.iso", "an explicit argument wins");
  }
  {
    std::vector<char*> argv{const_cast<char*>("port"), const_cast<char*>("--verbose")};
    CheckEq(ResolveDiscPath(static_cast<int>(argv.size()), argv.data(), "/from/env.iso", dir.string()),
            "/from/env.iso", "flags are skipped and the environment is used");
  }
  {
    std::vector<char*> argv{const_cast<char*>("port")};
    CheckEq(ResolveDiscPath(static_cast<int>(argv.size()), argv.data(), nullptr, dir.string()),
            firstImage.string(), "the first image beside the executable is used");
  }
  {
    std::vector<char*> argv{const_cast<char*>("port"), const_cast<char*>("notes.txt")};
    CheckEq(ResolveDiscPath(static_cast<int>(argv.size()), argv.data(), nullptr, dir.string()),
            firstImage.string(), "a non-image argument falls through to the directory");
  }
  {
    std::vector<char*> argv{const_cast<char*>("port")};
    CheckEq(ResolveDiscPath(static_cast<int>(argv.size()), argv.data(), nullptr, "/nonexistent"),
            std::string(), "nothing found gives an empty path");
    CheckEq(ResolveDiscPath(1, argv.data(), "", ""), std::string(), "an empty environment value is ignored");
  }

  std::filesystem::remove_all(dir, error);
}

} // namespace

int main() {
  TestSupportedDisc();
  TestDiscImageNames();
  TestResolveDiscPath();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d checks failed\n", g_failures);
    return 1;
  }
  std::printf("port_entry_tests: all checks passed\n");
  return 0;
}
