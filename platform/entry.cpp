#include "port_entry.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <vector>

namespace port {
namespace entry {

bool IsSupportedDisc(const DVDDiskID& id) {
  return std::memcmp(id.gameName, "G2ME", 4) == 0 && std::memcmp(id.company, "01", 2) == 0 &&
         id.diskNumber == 0 && id.gameVersion == 0;
}

bool IsDiscImageName(const std::string& name) {
  std::string lower = name;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return lower.size() > 4 && (lower.ends_with(".iso") || lower.ends_with(".gcm"));
}

std::string ResolveDiscPath(int argc, char** argv, const char* envDiscPath,
                            const std::string& executableDir) {
  for (int i = 1; i < argc; i++) {
    if (argv[i] != nullptr && argv[i][0] != '-' && IsDiscImageName(argv[i])) {
      return argv[i];
    }
  }

  if (envDiscPath != nullptr && envDiscPath[0] != '\0') {
    return envDiscPath;
  }

  if (!executableDir.empty()) {
    std::error_code error;
    std::filesystem::directory_iterator entries(executableDir, error);
    if (!error) {
      std::vector<std::string> candidates;
      for (const auto& entry : entries) {
        if (!entry.is_regular_file(error)) {
          continue;
        }
        const std::string name = entry.path().filename().string();
        if (IsDiscImageName(name)) {
          candidates.push_back(entry.path().string());
        }
      }
      // Deterministic when several images sit side by side.
      std::sort(candidates.begin(), candidates.end());
      if (!candidates.empty()) {
        return candidates.front();
      }
    }
  }

  return {};
}

} // namespace entry
} // namespace port
