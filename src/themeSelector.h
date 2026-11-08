// -*- mode: c++ -*-
#pragma once

#include <filesystem>
#include <vector>

bool
ReplaceContent(const std::filesystem::path& men,
               const std::filesystem::path& men2,
               const std::filesystem::path& cafeBarista,
               const std::filesystem::path& contentExtraPath,
               const std::vector<std::filesystem::path>& bfstmSet);
