#include <string.h>

#include "IgnoreCase.h"

namespace utils {

    bool
    IgnoreCaseLess::operator ()(const std::filesystem::path& a,
                                const std::filesystem::path& b)
        const noexcept
    {
        int r = strcasecmp(a.c_str(), b.c_str());
        return r < 0;
    }


    bool
    IgnoreCaseEqual(const std::filesystem::path& a,
                    const std::filesystem::path& b)
        noexcept
    {
        int r = strcasecmp(a.c_str(), b.c_str());
        return r == 0;
    }

} // namespace utils
