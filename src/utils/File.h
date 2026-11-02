// -*- mode: c++ -*-
#pragma once

#include <cstdio>
#include <filesystem>
#include <span>

namespace utils {

    // RAII type for FILE pointers
    struct File {

        std::FILE* fp = nullptr;

        File() = default;

        File(const std::filesystem::path& filename,
             const char* mode);

        ~File()
            noexcept;

        operator bool()
            const noexcept;

        void
        open(const std::filesystem::path& filename,
             const char* mode);

        void
        close()
            noexcept;

        bool
        gets(std::span<char> line)
            noexcept;

        std::size_t
        read(std::span<char> dst);

        void
        rewind()
            noexcept;

    };

} // namespace utils
