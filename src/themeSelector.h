#pragma once

#include <filesystem>
#include <flat_set>

bool
ReplaceContent(const std::filesystem::path& men,
               const std::filesystem::path& men2,
               const std::filesystem::path& cafeBarista,
               const std::filesystem::path& contentExtraPath,
               const std::flat_set<std::filesystem::path>& bfstmSet);
