#include "globals.h"
#include "utils/logger.h"
#include "themeSelector.h"
#include <utils/StringTools.h>

#include <notifications/notifications.h>
#include <content_redirection/redirection.h>

#include <coreinit/title.h>
#include <coreinit/launch.h>
#include <sysapp/title.h>
#include <sysapp/launch.h>

#include <wups.h>
#include <wups/config.h>
#include <wups/config_api.h>
#include <wups/config/WUPSConfigItemStub.h>
#include <wups/storage.h>

#include "wupsext/ConfigItemTheme.h"
#include "wupsext/ConfigItemBooleanVar.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <flat_map>
#include <functional>
#include <random>
#include <ranges>
#include <vector>

WUPS_PLUGIN_NAME("StyleMiiU");
WUPS_PLUGIN_DESCRIPTION("A way to easily load custom themes");
WUPS_PLUGIN_VERSION(VERSION);
WUPS_PLUGIN_AUTHOR("Juanen100");
WUPS_PLUGIN_LICENSE("GPLv3");

WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("style-mii-u");

using namespace std::literals;

static bool need_to_restart = false;
static bool is_wiiu_menu = false;
static std::minstd_rand prng_engine;

void initialize_prng()
{
    auto t = static_cast<std::uint64_t>(OSGetTime());
    std::seed_seq seeder{static_cast<std::uint32_t>(t),
                         static_cast<std::uint32_t>(t >> 32)};
    prng_engine.seed(seeder);
}

template<typename T>
T
get_random_item(const std::unordered_set<T>& input) {
    std::array<T, 1> result;
    auto it = std::ranges::sample(input, result.begin(), result.size(), prng_engine);
    if (it == result.end())
        return std::move(result.front());
    return {};
}

std::unordered_set<std::filesystem::path> ParseEnabledThemes(const std::string& themes) {
    std::unordered_set<std::filesystem::path> result;
    if (!gShuffleThemes) {
        if (!themes.empty())
            result = { themes }; // when not shuffling, only a single theme is allowed
    } else {
        auto split_themes = StringTools::stringSplit(themes, "|");
        for (auto& theme : split_themes)
            if (!theme.empty())
                result.insert(std::move(theme));
    }
    return result;
}

bool
IgnoreCaseLess(const std::filesystem::path& a,
               const std::filesystem::path& b)
    noexcept
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

std::string SerializeEnabledThemes(const std::unordered_set<std::filesystem::path>& themes) {
    if (!gShuffleThemes) {
        if (themes.empty())
            return "";
        else
            return *themes.begin();
    } else {
        std::vector<std::filesystem::path> sorted_themes{themes.begin(), themes.end()};
        std::ranges::sort(sorted_themes, IgnoreCaseLess);
        std::string result;
        for (const auto& theme : sorted_themes)
            result += theme.string() + "|";
        return result;
    }
}

void ReloadConfig() {
    using WUPSStorageAPI::GetOrStoreDefault;

    if (auto err = GetOrStoreDefault(KEY_THEME_MANAGER_ENABLED,
                                     gThemeManagerEnabled,
                                     DEFAULT_THEME_MANAGER_ENABLED)) {
        DEBUG_FUNCTION_LINE_ERR("Failed to get or create %s: %s (%d)",
                                KEY_THEME_MANAGER_ENABLED,
                                WUPSStorageAPI_GetStatusStr(err),
                                err);
    }

    if (auto err = GetOrStoreDefault(KEY_SHUFFLE_THEMES,
                                     gShuffleThemes,
                                     DEFAULT_SHUFFLE_THEMES)) {
        DEBUG_FUNCTION_LINE_ERR("Failed to get or create %s: %s (%d)",
                                KEY_SHUFFLE_THEMES,
                                WUPSStorageAPI_GetStatusStr(err),
                                err);
    }

    if (auto err = GetOrStoreDefault(KEY_MASHUP_THEMES,
                                     gMashupThemes,
                                     DEFAULT_MASHUP_THEMES)) {
        DEBUG_FUNCTION_LINE_ERR("Failed to get or create %s: %s (%d)",
                                KEY_MASHUP_THEMES,
                                WUPSStorageAPI_GetStatusStr(err),
                                err);
    }

    if (auto err = GetOrStoreDefault(KEY_SHOW_NOTIFICATION,
                                     gShowNotification,
                                     DEFAULT_SHOW_NOTIFICATION)) {
        DEBUG_FUNCTION_LINE_ERR("Failed to get or create %s: %s (%d)",
                                KEY_MASHUP_THEMES,
                                WUPSStorageAPI_GetStatusStr(err),
                                err);
    }

    gEnabledThemes.clear();
    std::string rawEnabledThemes;
    if (auto err = GetOrStoreDefault(KEY_ENABLED_THEMES,
                                     rawEnabledThemes,
                                     ""s)) {
        DEBUG_FUNCTION_LINE_ERR("Failed to get or create %s: %s (%d)",
                                KEY_ENABLED_THEMES,
                                WUPSStorageAPI_GetStatusStr(err),
                                err);
    } else {
        gEnabledThemes = ParseEnabledThemes(rawEnabledThemes);
    }
}

void theme_manager_enabled_changed_callback(wupsext::ConfigItemBooleanVarContext &item) {
    DEBUG_FUNCTION_LINE_VERBOSE("New value in %s changed: %s",
                                item.identifier.data(),
                                item.variable ? "true" : "false");
    need_to_restart = true;
}

void shuffle_themes_changed_callback(wupsext::ConfigItemBooleanVarContext &item) {
    DEBUG_FUNCTION_LINE_VERBOSE("New value in %s changed: %s",
                                item.identifier.data(),
                                item.variable ? "true" : "false");
    // When toggling shuffle, it's not clear what the current theme should be, so let's
    // disable them all.
    wupsext::ConfigItemTheme_ResetAll();
    need_to_restart = true;
}

void mashup_themes_changed_callback(wupsext::ConfigItemBooleanVarContext &item) {
    DEBUG_FUNCTION_LINE_VERBOSE("New value in %s changed: %s",
                                item.identifier.data(),
                                item.variable ? "true" : "false");
    need_to_restart = true;
}

void theme_changed_callback(wupsext::ConfigItemThemeContext &item) {
    DEBUG_FUNCTION_LINE_VERBOSE("New value in %s changed: %s",
                                item.identifier.data(),
                                item.value ? "true" : "false");
    if (!gShuffleThemes)
        need_to_restart = true;
}

WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle rootHandle)
    noexcept
try {
    WUPSStorageAPI::ForceReloadStorage();
    ReloadConfig();

    WUPSConfigCategory root = WUPSConfigCategory(rootHandle);

    using BoolItem = wupsext::ConfigItemBooleanVar;

    root.add(BoolItem::Create(KEY_THEME_MANAGER_ENABLED,
                              "Enable StyleMiiU",
                              gThemeManagerEnabled,
                              theme_manager_enabled_changed_callback));

    root.add(BoolItem::Create(KEY_SHOW_NOTIFICATION,
                              "Show theme notification",
                              gShowNotification));

    root.add(BoolItem::Create(KEY_SHUFFLE_THEMES,
                              "Shuffle themes",
                              gShuffleThemes,
                              shuffle_themes_changed_callback));

    root.add(BoolItem::Create(KEY_MASHUP_THEMES,
                              "Mashup themes",
                              gMashupThemes,
                              mashup_themes_changed_callback));

    // Show last loaded theme
    if (!gLoadedTheme.empty())
        root.add(WUPSConfigItemStub::Create("Loaded theme: "
                                            + gLoadedTheme));

    if (!gLoadedMenTheme.empty()) {
        root.add(WUPSConfigItemStub::Create("Loaded Content: "
                                            + gLoadedMenTheme));
        root.add(WUPSConfigItemStub::Create("Loaded Men: "
                                            + gLoadedMenTheme));
    }

    if (!gLoadedMen2Theme.empty())
        root.add(WUPSConfigItemStub::Create("Loaded Men2: "
                                            + gLoadedMen2Theme));

    if (!gLoadedCafeBaristaTheme.empty())
        root.add(WUPSConfigItemStub::Create("Loaded cafe_barista_men: "
                                            + gLoadedCafeBaristaTheme));

    auto themes = WUPSConfigCategory::Create("Available Themes");

    std::vector<std::filesystem::path> themeDirs;
    try {
        for (auto &entry : std::filesystem::directory_iterator{theme_directory_path})
            if (entry.is_directory())
                themeDirs.push_back(entry.path().filename());
        std::ranges::sort(themeDirs, IgnoreCaseLess);
        for (auto& curTheme : themeDirs) {
            bool themeEnabled = gEnabledThemes.contains(curTheme);
            themes.add(wupsext::ConfigItemTheme::Create(curTheme,
                                                        curTheme,
                                                        themeEnabled,
                                                        theme_changed_callback));
        }
    }
    catch (std::exception &e) {
        DEBUG_FUNCTION_LINE_WARN("Failed to list themes: %s", e.what());
    }

    root.add(std::move(themes));
    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
 }
catch (std::exception &e) {
    DEBUG_FUNCTION_LINE_ERR("Exception: %s\n", e.what());
    return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
}

void ConfigMenuClosedCallback() {
    // Update gEnabledThemes based on the enabled items.
    gEnabledThemes.clear();
    wupsext::ConfigItemTheme_ForEach(
        [](wupsext::ConfigItemThemeContext &item)
        {
            if (item.value) {
                DEBUG_FUNCTION_LINE_VERBOSE("Found enabled: %s", item.identifier.data());
                gEnabledThemes.insert(item.identifier);
            }
        }
    );
    wupsext::ConfigItemTheme_Clear();

    using WUPSStorageAPI::Store;

    if (!gShuffleThemes && gEnabledThemes.size() > 1)
        DEBUG_FUNCTION_LINE_WARN("Inconsistent state: gEnableThemes.size() > 1 but gShuffleThemes is false!");

    if (auto err = Store(KEY_ENABLED_THEMES, SerializeEnabledThemes(gEnabledThemes))) {
            DEBUG_FUNCTION_LINE_ERR("Failed to store %s: %s (%d)",
                                    KEY_ENABLED_THEMES,
                                    WUPSStorageAPI_GetStatusStr(err),
                                    err);
    }

    if (auto err = Store(KEY_THEME_MANAGER_ENABLED, gThemeManagerEnabled)) {
        DEBUG_FUNCTION_LINE_ERR("Failed to store %s: %s (%d)",
                                KEY_THEME_MANAGER_ENABLED,
                                WUPSStorageAPI_GetStatusStr(err),
                                err);
    }

    if (auto err = Store(KEY_SHUFFLE_THEMES, gShuffleThemes)) {
        DEBUG_FUNCTION_LINE_ERR("Failed to store %s: %s (%d)",
                                KEY_SHUFFLE_THEMES,
                                WUPSStorageAPI_GetStatusStr(err),
                                err);
    }

    if (auto err = Store(KEY_MASHUP_THEMES, gMashupThemes)) {
        DEBUG_FUNCTION_LINE_ERR("Failed to store %s: %s (%d)",
                                KEY_MASHUP_THEMES,
                                WUPSStorageAPI_GetStatusStr(err),
                                err);
    }

    if (auto err = Store(KEY_SHOW_NOTIFICATION, gShowNotification)) {
        DEBUG_FUNCTION_LINE_ERR("Failed to store %s: %s (%d)",
                                KEY_SHOW_NOTIFICATION,
                                WUPSStorageAPI_GetStatusStr(err),
                                err);
    }

    if (auto err = WUPSStorageAPI::SaveStorage()) {
        DEBUG_FUNCTION_LINE_ERR("Failed to save storage: %s (%d)",
                                WUPSStorageAPI_GetStatusStr(err),
                                err);
    }

    /*
     * Special restart conditions, when shuffle is enabled:
     * 1. If the current theme was disabled.
     * 2. If no theme is loaded, and at least one theme is enabled.
     */
    if (gShuffleThemes) {

        // Case 1: if current theme was disabled.
        if (!gLoadedTheme.empty() &&
            !gEnabledThemes.contains(gLoadedTheme))
            need_to_restart = true;

        // Handle mashups too: if any component of a mashup is disabled, gotta restart.
        if (!gLoadedMenTheme.empty() &&
            !gEnabledThemes.contains(gLoadedMenTheme))
            need_to_restart = true;

        if (!gLoadedMen2Theme.empty() &&
            !gEnabledThemes.contains(gLoadedMen2Theme))
            need_to_restart = true;

        if (!gLoadedCafeBaristaTheme.empty() &&
            !gEnabledThemes.contains(gLoadedCafeBaristaTheme))
            need_to_restart = true;

        // Case 2: no theme was loaded, now at least one theme is enabled
        if (!gEnabledThemes.empty() &&
            gLoadedTheme.empty() &&
            gLoadedMenTheme.empty() &&
            gLoadedMen2Theme.empty() &&
            gLoadedCafeBaristaTheme.empty())
            need_to_restart = true;
    }

    if(need_to_restart && is_wiiu_menu) {
        SYSLaunchMenu();
        need_to_restart = false;
    }
}

INITIALIZE_PLUGIN() {
    initialize_prng();

    if (auto err = ContentRedirection_InitLibrary()) {
        DEBUG_FUNCTION_LINE_ERR("Failed to init ContentRedirection. Error %s %d",
                                ContentRedirection_GetStatusStr(err),
                                err);
        OSFatal("Failed to init ContentRedirection.");
    }

    if (NotificationModule_InitLibrary()) {
        DEBUG_FUNCTION_LINE("NotificationModule_InitLibrary failed");
        gShowNotification = false;
    }

    WUPSConfigAPIOptionsV1 configOptions = {.name = "StyleMiiU"};
    if (auto err = WUPSConfigAPI_Init(configOptions,
                                      ConfigMenuOpenedCallback,
                                      ConfigMenuClosedCallback)) {
        DEBUG_FUNCTION_LINE_ERR("Failed to init config api: %s (%d)",
                                WUPSConfigAPI_GetStatusStr(err),
                                err);
    }

    ReloadConfig();

    if (auto err = WUPSStorageAPI::SaveStorage()) {
        DEBUG_FUNCTION_LINE_ERR("Failed to save storage: %s (%d)",
                                WUPSStorageAPI_GetStatusStr(err),
                                err);
    }

    gContentLayerHandle = 0;
}

DEINITIALIZE_PLUGIN() {
    NotificationModule_DeInitLibrary();
}

enum SearchType : unsigned {
    SearchMen         = 1u << 0u,
    SearchMen2        = 1u << 1u,
    SearchCafeBarista = 1u << 2u,

    SearchAll = SearchMen | SearchMen2 | SearchCafeBarista
};

struct SearchResult {
    std::optional<std::filesystem::path> menPack;
    std::optional<std::filesystem::path> men2Pack;
    std::optional<std::filesystem::path> cafeBarista;
    std::optional<std::flat_set<std::filesystem::path>> bfstmSet;
};

SearchResult
SearchThemeFiles(const std::filesystem::path &themePath,
                 unsigned flags)
{
    SearchResult result;
    try {
        for (auto &entry : std::filesystem::recursive_directory_iterator{themePath}) {
            if (!entry.is_regular_file())
                continue;
            auto path = entry.path();
            auto filename = path.filename();

            if ((flags & SearchMen) &&
                !result.menPack &&
                IgnoreCaseEqual(filename, "Men.pack"))
                result.menPack = path;

            if ((flags & SearchMen2) &&
                !result.men2Pack &&
                IgnoreCaseEqual(filename, "Men2.pack"))
                result.men2Pack = path;

            if ((flags & SearchCafeBarista) &&
                !result.cafeBarista &&
                IgnoreCaseEqual(filename, "cafe_barista_men.bfsar"))
                result.cafeBarista = path;

            if ((flags & SearchCafeBarista) &&
                IgnoreCaseEqual(filename.extension(), ".bfstm")) {
                if (!result.bfstmSet)
                    result.bfstmSet.emplace();
                result.bfstmSet->insert(path);
            }
        }
    }
    catch (...) {}
    return result;
}

void HandleThemes() {
    gLoadedTheme.clear();
    gLoadedMenTheme.clear();
    gLoadedMen2Theme.clear();
    gLoadedCafeBaristaTheme.clear();

    if (!gThemeManagerEnabled)
        return;

    if (gEnabledThemes.empty()) {
        DEBUG_FUNCTION_LINE_WARN("no themes enabled");
        return;
    }

    bool success = false;

    std::filesystem::path menPackPath, men2PackPath, cafeBaristaPath, contentPath;
    std::flat_set<std::filesystem::path> bfstmSet;

    if (gMashupThemes && gShuffleThemes) {

        int attempts = 0;
        const int maxAttempts = gEnabledThemes.size() * 2;

        // Find a Men.pack
        while (menPackPath.empty() && attempts < maxAttempts) {
            auto randomTheme = get_random_item(gEnabledThemes);
            auto themePath = theme_directory_path / randomTheme;

            auto res = SearchThemeFiles(themePath, SearchMen);
            if (res.menPack) {
                menPackPath = std::move(*res.menPack);
                gLoadedMenTheme = randomTheme;
                break;
            }
            attempts++;
        }

        // Find a Men2.pack
        attempts = 0;
        while (men2PackPath.empty() && attempts < maxAttempts) {
            auto randomTheme = get_random_item(gEnabledThemes);
            auto themePath = theme_directory_path / randomTheme;

            auto res = SearchThemeFiles(themePath, SearchMen2);
            if (res.men2Pack) {
                men2PackPath = std::move(*res.men2Pack);
                gLoadedMen2Theme = randomTheme;
                break;
            }
            attempts++;
        }

        // Find a cafe_barista_men.bfsar, and .bfstm files.
        attempts = 0;
        while (cafeBaristaPath.empty() && attempts < maxAttempts) {
            auto randomTheme = get_random_item(gEnabledThemes);
            auto themePath = theme_directory_path / randomTheme;

            auto res = SearchThemeFiles(themePath, SearchCafeBarista);
            if (res.cafeBarista || res.bfstmSet) {
                if (res.bfstmSet)
                    bfstmSet = std::move(*res.bfstmSet);
                if (res.cafeBarista) {
                    cafeBaristaPath = std::move(*res.cafeBarista);
                    gLoadedCafeBaristaTheme = randomTheme;
                }
                break;
            }
            attempts++;
        }

        contentPath = theme_directory_path / (gLoadedMenTheme + "/content");
        if (!exists(contentPath))
            contentPath.clear();
    }
    else {
        if (gShuffleThemes)
            gLoadedTheme = get_random_item(gEnabledThemes);
        else
            gLoadedTheme = *gEnabledThemes.begin();

        auto currentThemePath = theme_directory_path / gLoadedTheme;

        auto [newMen, newMen2, newCafeBarista, newBfstmSet] =
            SearchThemeFiles(currentThemePath, SearchAll);
        if (newMen)
            menPackPath = std::move(*newMen);
        if (newMen2)
            men2PackPath = std::move(*newMen2);
        if (newCafeBarista)
            cafeBaristaPath = std::move(*newCafeBarista);
        if (newBfstmSet)
            bfstmSet = std::move(*newBfstmSet);

        contentPath = currentThemePath / "content";
        if (!exists(contentPath))
            contentPath.clear();
    }

    if (!menPackPath.empty()     ||
        !men2PackPath.empty()    ||
        !cafeBaristaPath.empty() ||
        !bfstmSet.empty()        ||
        !contentPath.empty()) {
        success = ReplaceContent(menPackPath,
                                 men2PackPath,
                                 cafeBaristaPath,
                                 contentPath,
                                 bfstmSet);
    }

    if (success && gShowNotification){
        if (NotificationModule_SetDefaultValue(
                NOTIFICATION_MODULE_NOTIFICATION_TYPE_INFO,
                NOTIFICATION_MODULE_DEFAULT_OPTION_DURATION_BEFORE_FADE_OUT,
                12.0f))
            return;

        if (gMashupThemes) {
            if (!contentPath.empty()){
                std::string text = "Content: " + gLoadedMenTheme;
                NotificationModule_AddInfoNotification(text.c_str());
            }

            if (!cafeBaristaPath.empty()){
                std::string text = "cafe_barista_men.bfsar: " + gLoadedCafeBaristaTheme;
                NotificationModule_AddInfoNotification(text.c_str());
            }

            if (!men2PackPath.empty()){
                std::string text = "Men2.pack: " + gLoadedMen2Theme;
                NotificationModule_AddInfoNotification(text.c_str());
            }

            if (!menPackPath.empty()){
                std::string text = "Men.pack: " + gLoadedMenTheme;
                NotificationModule_AddInfoNotification(text.c_str());
            }
        } else {
            std::string text = "Theme: " + gLoadedTheme;
            NotificationModule_AddInfoNotification(text.c_str());
        }
    }
}

ON_APPLICATION_START() {
    initLogging();

    uint64_t current_title_id = OSGetTitleID();
    uint64_t wiiu_menu_tid = _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_WII_U_MENU);

    is_wiiu_menu = (current_title_id == wiiu_menu_tid);

    if (!is_wiiu_menu)
        return;

    WUPSStorageAPI::ForceReloadStorage();

    ReloadConfig();

    HandleThemes();
}

ON_APPLICATION_ENDS() {
    if (gContentLayerHandle != 0) {
        ContentRedirection_RemoveFSLayer(gContentLayerHandle);
        gContentLayerHandle = 0;
    }
    deinitLogging();
}
