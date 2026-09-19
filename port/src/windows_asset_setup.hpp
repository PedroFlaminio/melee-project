#pragma once

#include <filesystem>

namespace melee::windows {

std::filesystem::path find_project_root();
bool has_local_assets(const std::filesystem::path& project_root);
/* Returns true when asset extraction completed successfully. */
bool show_missing_assets_window(const std::filesystem::path& project_root);

} // namespace melee::windows
