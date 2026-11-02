// -*- mode: c++ -*-

#pragma once

#include <filesystem>

namespace utils {

    struct IgnoreCaseLess{
        bool
        operator ()(const std::filesystem::path& a,
                    const std::filesystem::path& b)
            const noexcept;
    };

    bool
    IgnoreCaseEqual(const std::filesystem::path& a,
                    const std::filesystem::path& b)
        noexcept;

} // namespace utils
