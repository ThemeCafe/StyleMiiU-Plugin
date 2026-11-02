#include <cerrno>
#include <stdexcept>
#include <string>
#include <system_error>

#include "File.h"

using namespace std::literals;

namespace utils {

    namespace {

        [[noreturn]]
        void
        throw_from_error(int e,
                         const std::string& msg)
        {
            throw std::system_error{e, std::generic_category(), msg};
        }

    } // namespace

    File::File(const std::filesystem::path& filename,
               const char* mode) :
        fp{std::fopen(filename.c_str(), mode)}
    {
        if (!fp)
            throw_from_error(errno, "failed to open \""s + filename.string() + "\""s);
    }

    File::~File()
        noexcept
    {
        close();
    }

    File::operator bool()
        const noexcept
    {
        return fp;
    }

    void
    File::open(const std::filesystem::path& filename,
               const char* mode)
    {
        close();
        fp = std::fopen(filename.c_str(), mode);
        if (!fp)
            throw_from_error(errno, "failed to open \""s + filename.string() + "\""s);
    }

    void
    File::close()
        noexcept
    {
        if (fp) {
            std::fclose(fp);
            fp = nullptr;
        }
    }

    bool
    File::gets(std::span<char> line)
        noexcept
    {
        return std::fgets(line.data(), line.size(), fp);
    }


    std::size_t
    File::read(std::span<char> dst)
    {
        auto result = std::fread(dst.data(), 1, dst.size(), fp);
        if (std::ferror(fp))
            throw std::runtime_error{"fread() failed"};
        return result;
    }


    void
    File::rewind()
        noexcept
    {
        if (fp)
            std::rewind(fp);
    }

} // namespace utils
