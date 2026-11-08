#pragma once

#include <coreinit/debug.h>
#include <string.h>
#include <whb/log.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_APP_TYPE "P"
#define LOG_APP_NAME "StyleMiiU"

#define __FILENAME__ ({                                \
    const char *__filename = __FILE__;                 \
    const char *__pos      = strrchr(__filename, '/'); \
    if (!__pos) __pos = strrchr(__filename, '\\');     \
    __pos ? __pos + 1 : __filename;                    \
})

#define LOG(LOG_FUNC, FMT, ...) \
    LOG_EX_DEFAULT(LOG_FUNC, "", "", FMT, __VA_ARGS__)

#define LOG_EX_DEFAULT(LOG_FUNC, LOG_LEVEL, LINE_END, FMT, ...) \
    LOG_EX(__FILENAME__, __FUNCTION__, __LINE__, LOG_FUNC, LOG_LEVEL, LINE_END, \
           FMT, __VA_ARGS__)

#define LOG_EX(FILENAME, FUNCTION, LINE, LOG_FUNC, LOG_LEVEL, LINE_END, FMT, ...)       \
    do {                                                                                \
        LOG_FUNC("[(%s)%18s][%23s]%30s@L%04d: " LOG_LEVEL "" FMT "" LINE_END,           \
                 LOG_APP_TYPE, LOG_APP_NAME, FILENAME, FUNCTION, LINE __VA_OPT__(,)     \
                 __VA_ARGS__);                                                          \
    } while (0)

#ifdef DEBUG

#ifdef VERBOSE_DEBUG

#define DEBUG_FUNCTION_LINE_VERBOSE(...)        \
    LOG(WHBLogPrintf, __VA_ARGS__)

#define DEBUG_FUNCTION_LINE_VERBOSE_EX(FILENAME, FUNCTION, LINE, ...)   \
    LOG_EX(FILENAME, FUNCTION, LINE, WHBLogPrintf, "", "", __VA_ARGS__)

#else // !VERBOSE_DEBUG

#define DEBUG_FUNCTION_LINE_VERBOSE(...)        \
    do {                                        \
        (void)sizeof((__VA_ARGS__));            \
    } while (0)
#define DEBUG_FUNCTION_LINE_VERBOSE_EX(...)     \
    do {                                        \
        (void)sizeof((__VA_ARGS__));            \
    } while (0)

#endif // !VERBOSE_DEBUG

#define DEBUG_FUNCTION_LINE(...)                \
    LOG(WHBLogPrintf, __VA_ARGS__)

#define DEBUG_FUNCTION_LINE_WRITE(...)          \
    LOG(WHBLogWritef, __VA_ARGS__)

#define DEBUG_FUNCTION_LINE_ERR(...)                            \
    LOG_EX_DEFAULT(WHBLogPrintf, "##ERROR## ", "", __VA_ARGS__)
#define DEBUG_FUNCTION_LINE_WARN(...)                           \
    LOG_EX_DEFAULT(WHBLogPrintf, "##WARN ## ", "", __VA_ARGS__)
#define DEBUG_FUNCTION_LINE_INFO(...)                           \
    LOG_EX_DEFAULT(WHBLogPrintf, "##INFO ## ", "", __VA_ARGS__)

#define DEBUG_FUNCTION_LINE_ERR_LAMBDA(FILENAME, FUNCTION, LINE, ...)                   \
    LOG_EX(FILENAME, FUNCTION, LINE, WHBLogPrintf, "##ERROR## ", "", __VA_ARGS__);

#else // !DEBUG

#define DEBUG_FUNCTION_LINE_VERBOSE_EX(...)     \
    do {                                        \
        (void)sizeof((__VA_ARGS__));            \
    } while (0)

#define DEBUG_FUNCTION_LINE_VERBOSE(...)        \
    do {                                        \
        (void)sizeof((__VA_ARGS__));            \
    } while (0)

#define DEBUG_FUNCTION_LINE(...)                \
    do {                                        \
        (void)sizeof((__VA_ARGS__));            \
    } while (0)

#define DEBUG_FUNCTION_LINE_WRITE(...)          \
    do {                                        \
        (void)sizeof((__VA_ARGS__));            \
    } while (0)

#define DEBUG_FUNCTION_LINE_ERR(...)                            \
    LOG_EX_DEFAULT(OSReport, "##ERROR## ", "\n", __VA_ARGS__)

#define DEBUG_FUNCTION_LINE_WARN(...)                           \
    LOG_EX_DEFAULT(OSReport, "##WARN ## ", "\n", __VA_ARGS__)

#define DEBUG_FUNCTION_LINE_INFO(...)                           \
    LOG_EX_DEFAULT(OSReport, "##INFO ## ", "\n", __VA_ARGS__)

#define DEBUG_FUNCTION_LINE_ERR_LAMBDA(FILENAME, FUNCTION, LINE, ...) \
    LOG_EX(FILENAME, FUNCTION, LINE, OSReport, "##ERROR## ", "\n", __VA_ARGS__);

#endif

void initLogging();

void deinitLogging();

#ifdef __cplusplus
}
#endif
