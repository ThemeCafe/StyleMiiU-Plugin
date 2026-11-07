#include <algorithm>
#include <array>
#include <filesystem>
#include <flat_map>
#include <flat_set>
#include <functional>
#include <optional>
#include <random>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include <coreinit/launch.h>
#include <coreinit/memory.h>
#include <coreinit/thread.h>
#include <coreinit/title.h>
#include <sysapp/launch.h>
#include <sysapp/title.h>

#include <notifications/notifications.h>
#include <content_redirection/redirection.h>

#include <wups.h>
#include <wups/config.h>
#include <wups/config_api.h>
#include <wups/config/WUPSConfigItemStub.h>
#include <wups/storage.h>

#include "wupsext/ConfigToggleItem.h"
#include "wupsext/ConfigBooleanVarItem.h"

#include "globals.h"
#include "themeSelector.h"
#include "utils/File.h"
#include "utils/IgnoreCase.h"
#include "utils/logger.h"

WUPS_PLUGIN_NAME("StyleMiiU");
WUPS_PLUGIN_DESCRIPTION("A way to easily load custom themes");
WUPS_PLUGIN_VERSION(VERSION);
WUPS_PLUGIN_AUTHOR("Juanen100");
WUPS_PLUGIN_LICENSE("GPLv3");

WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("style-mii-u");

using namespace std::literals;

namespace {

    /*-------*/
    /* Types */
    /*-------*/

    using SortedPathSet = std::flat_set<std::filesystem::path, utils::IgnoreCaseLess>;

    // NOTE: Duplicated font names are ignored.
    using FontNamePathMap = std::flat_map<std::filesystem::path,
                                          std::filesystem::path,
                                          utils::IgnoreCaseLess>;

    using FontBlob = std::vector<char>;

    struct ThemesAndFonts {
        SortedPathSet   themes; // only filename of paths
        FontNamePathMap fonts;  // font name -> font path
    };


    enum ScanFlags : unsigned {
        ScanFlagsThemes         = 0b01u,
        ScanFlagsFonts          = 0b10u,
        ScanFlagsThemesAndFonts = 0b11u,
    };

    enum SearchType : unsigned {
        SearchMen         = 0b0001u,
        SearchMen2        = 0b0010u,
        SearchCafeBarista = 0b0100u,
        SearchAll         = SearchMen | SearchMen2 | SearchCafeBarista,
    };

    struct SearchResult {
        std::optional<std::filesystem::path> menPack;
        std::optional<std::filesystem::path> men2Pack;
        std::optional<std::filesystem::path> cafeBarista;
        std::optional<SortedPathSet> bfstmSet;
    };

    /*-----------*/
    /* Variables */
    /*-----------*/

    bool gNeedToRestart = false;
    std::minstd_rand gPRNGEngine;
    std::optional<wupsext::ConfigToggleItemGroup> gThemesGroup;
    std::optional<wupsext::ConfigToggleItemGroup> gFontsGroup;
    std::optional<FontBlob> gFontStdBlob;
    std::optional<FontNamePathMap> gKnownFonts;

    /*-----------------------*/
    /* Function declarations */
    /*-----------------------*/

    bool
    CalledFromWiiUMenu()
        noexcept;

    bool
    CalledFromWUPSMenu()
        noexcept;

    bool
    CalledFromWiiUMenuSWKBD()
        noexcept;

    template <std::ranges::forward_range R>
    std::ranges::range_value_t<R>
    GetRandomItem(R&& input);

    void
    InitializePRNG();

    bool
    IsTTF(const std::filesystem::path& path);

    template <std::ranges::forward_range R>
    std::string
    JoinPathSet(R&& input,
                bool shuffle);

    FontBlob
    LoadFont(const std::filesystem::path& fontName)
        noexcept;

    template <typename T>
    bool
    LoadVar(const char* key,
            T& var);

    template <typename T>
    bool
    LoadVar(const char* key,
            T& var,
            const T default_value);

    void
    OnFontItemChanged(wupsext::ConfigToggleItemImpl &item);

    void
    OnShuffleFontsChanged(wupsext::ConfigBooleanVarItemImpl &item);

    void
    OnShuffleThemesChanged(wupsext::ConfigBooleanVarItemImpl &item);

    void
    OnThemeItemChanged(wupsext::ConfigToggleItemImpl &item);

    void
    PickAndApplyTheme();

    std::optional<std::filesystem::path>
    PickFontName()
        noexcept;

    void
    ReloadConfig();

    void
    RestartWhenChanged(wupsext::ConfigBooleanVarItemImpl &item);

    ThemesAndFonts
    ScanThemesAndFonts(unsigned flags = ScanFlagsThemesAndFonts);

    SearchResult
    SearchThemeFiles(const std::filesystem::path &themePath,
                     unsigned flags);

    std::unordered_set<std::filesystem::path>
    SplitPathSet(const std::string& input);

    template <typename T>
    void
    StoreVar(const char* key,
             const T& var);

    std::string_view
    StripEOL(std::string_view input);

    /*----------------------*/
    /* Function definitions */
    /*----------------------*/

    bool
    CalledFromWiiUMenu()
        noexcept
    {
        // NOTE: _SYSGetSystemApplicationTitleId() crashes from inside applets, so let's just
        // hardcode the list of known Wii U Menu title IDs.
        switch (OSGetTitleID()) {
            case 0x00050010'10040000: // JPN
            case 0x00050010'10040100: // USA
            case 0x00050010'10040200: // EUR
                return true;
            default:
                return false;
        }
    }

    bool
    CalledFromWUPSMenu()
        noexcept
    {
        WUPSConfigAPIMenuStatus menu_status = WUPSCONFIG_API_MENU_STATUS_CLOSED;
        WUPSConfigAPI_Menu_GetStatus(&menu_status);
        return menu_status == WUPSCONFIG_API_MENU_STATUS_OPENED;
    }

    bool
    CalledFromWiiUMenuSWKBD()
        noexcept
    {
        OSThread* th_id = OSGetCurrentThread();
        const char* th_name = OSGetThreadName(th_id);
        if (!th_name)
            return false;
        return "MenSwkbdCalculator_Create"sv == th_name;
    }

    template <std::ranges::forward_range R>
    std::ranges::range_value_t<R>
    GetRandomItem(R&& input)
    {
        using T = std::ranges::range_value_t<R>;

        if (input.empty()) [[unlikely]]
            throw std::logic_error{"Cannot select item from empty set!"};

        T result;
        std::ranges::sample(std::forward<R>(input),
                            &result,
                            1,
                            gPRNGEngine);
        return result;
    }

    void
    InitializePRNG()
    {
        auto t = static_cast<std::uint64_t>(OSGetTime());
        std::seed_seq seeder{
            static_cast<std::uint32_t>(t),
            static_cast<std::uint32_t>(t >> 32)
        };
        gPRNGEngine.seed(seeder);
    }

    bool
    IsTTF(const std::filesystem::path& path)
    {
        return utils::IgnoreCaseEqual(path.extension(), ".ttf");
    }

    template <std::ranges::forward_range R>
    std::string
    JoinPathSet(R&& input,
                bool shuffle)
    {
        if (!shuffle) {
            if (input.empty())
                return "";
            else
                return *input.begin();
        } else {
            SortedPathSet sorted{input.begin(), input.end()};
            std::string result;
            for (const auto& item : sorted)
                result += item.string() + "|";
            return result;
        }
    }

    FontBlob
    LoadFont(const std::filesystem::path& fontName)
        noexcept
    {
        using std::runtime_error;

        if (fontName.empty())
            return {};

        // Special case: when a theme intends to use the original Cafe font.
        if (utils::IgnoreCaseEqual(fontName, "CafeStd")) {
            DEBUG_FUNCTION_LINE_INFO("Requested CafeStd font.");
            gLoadedFont = fontName;
            return {};
        }

        std::filesystem::path fontPath;
        if (gKnownFonts) {
            auto it = gKnownFonts->find(fontName);
            if (it != gKnownFonts->end())
                fontPath = it->second;
        }

        try {
            if (fontPath.empty())
                throw runtime_error{"Font path is empty!"};
            if (!exists(fontPath))
                throw runtime_error{"Font file does not exist!"};
            if (!is_regular_file(fontPath))
                throw runtime_error{"Font file is not a regular file!"};

            auto size = file_size(fontPath);
            if (size < 8)
                throw runtime_error{"Font file is too small!"};

            utils::File f{fontPath, "rb"};

            constexpr std::array<char, 4> ttf_magic = {0x00, 0x01, 0x00, 0x00};
            std::array<char, 4> file_magic;
            auto res = f.read(file_magic);
            if (res != 4)
                throw runtime_error{"Cannot read TTF magic!"};
            if (ttf_magic != file_magic)
                throw runtime_error{"No TTF magic in font file!"};

            f.rewind();

            FontBlob blob(size);
            res = f.read(blob);
            if (std::cmp_not_equal(res, size))
                throw runtime_error{"Could not load entire font file!"};

            DEBUG_FUNCTION_LINE_INFO("Loaded font: %s",
                                     fontPath.c_str());
            gLoadedFont = fontName;

            return blob;
        }
        catch (std::exception& e) {
            DEBUG_FUNCTION_LINE_ERR("error loading font: %s", e.what());
        }
        catch (...) {
            DEBUG_FUNCTION_LINE_ERR("unknown error loading font");
        }

        return {};
    }

    template <typename T>
    bool
    LoadVar(const char* key,
            T& var)
    {
        if (auto err = WUPSStorageAPI::Get(key, var)) {
            DEBUG_FUNCTION_LINE_ERR("Failed to get %s: %s (%d)",
                                    key,
                                    WUPSStorageAPI_GetStatusStr(err),
                                    err);
            return false;
        }
        return true;
    }

    template <typename T>
    bool
    LoadVar(const char* key,
            T& var,
            const T default_value)
    {
        if (auto err = WUPSStorageAPI::GetOrStoreDefault(key, var, default_value)) {
            DEBUG_FUNCTION_LINE_ERR("Failed to get or create %s: %s (%d)",
                                    key,
                                    WUPSStorageAPI_GetStatusStr(err),
                                    err);
            return false;
        }
        return true;
    }

    void
    OnFontItemChanged(wupsext::ConfigToggleItemImpl &item)
    {
        DEBUG_FUNCTION_LINE_VERBOSE("Font %s changed: %s",
                                    item.identifier.data(),
                                    item.value ? "true" : "false");
        if (!gShuffleFonts) {
            DEBUG_FUNCTION_LINE_VERBOSE("Restart needed.");
            gNeedToRestart = true;
        }
    }

    void
    OnShuffleFontsChanged(wupsext::ConfigBooleanVarItemImpl &item)
    {
        DEBUG_FUNCTION_LINE_VERBOSE("Setting %s changed: %s, restart needed.",
                                    item.identifier.data(),
                                    item.variable ? "true" : "false");
        using enum wupsext::ConfigToggleItemGroup::Mode;
        gFontsGroup->setMode(item.variable ? Multi : Single);
        gNeedToRestart = true;
    }

    void
    OnShuffleThemesChanged(wupsext::ConfigBooleanVarItemImpl &item)
    {
        DEBUG_FUNCTION_LINE_VERBOSE("Setting %s changed: %s",
                                    item.identifier.data(),
                                    item.variable ? "true" : "false");
        using enum wupsext::ConfigToggleItemGroup::Mode;
        gThemesGroup->setMode(item.variable ? Multi : Single);
        gNeedToRestart = true;
    }

    void
    OnThemeItemChanged(wupsext::ConfigToggleItemImpl &item)
    {
        DEBUG_FUNCTION_LINE_VERBOSE("Theme %s changed: %s",
                                    item.identifier.data(),
                                    item.value ? "true" : "false");
        if (!gShuffleThemes) {
            DEBUG_FUNCTION_LINE_VERBOSE("Restart needed.");
            gNeedToRestart = true;
        }
    }

    void
    PickAndApplyTheme()
    {
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
        SortedPathSet bfstmSet;

        if (gMashupThemes && gShuffleThemes) {

            int attempts = 0;
            const int maxAttempts = gEnabledThemes.size() * 2;

            // Find a Men.pack
            while (menPackPath.empty() && attempts < maxAttempts) {
                auto randomTheme = GetRandomItem(gEnabledThemes);
                auto themePath = themes_dir / randomTheme;

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
                auto randomTheme = GetRandomItem(gEnabledThemes);
                auto themePath = themes_dir / randomTheme;

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
                auto randomTheme = GetRandomItem(gEnabledThemes);
                auto themePath = themes_dir / randomTheme;

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

            contentPath = themes_dir / (gLoadedMenTheme + "/content");
            if (!exists(contentPath))
                contentPath.clear();
        }
        else {
            if (gShuffleThemes)
                gLoadedTheme = GetRandomItem(gEnabledThemes);
            else
                gLoadedTheme = *gEnabledThemes.begin();

            auto currentThemePath = themes_dir / gLoadedTheme;

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
            !contentPath.empty()     ||
            !bfstmSet.empty()) {
            success = ReplaceContent(menPackPath,
                                     men2PackPath,
                                     cafeBaristaPath,
                                     contentPath,
                                     std::move(bfstmSet).extract());
        }

        if (success && gShowNotification){
            if (NotificationModule_SetDefaultValue(
                    NOTIFICATION_MODULE_NOTIFICATION_TYPE_INFO,
                    NOTIFICATION_MODULE_DEFAULT_OPTION_DURATION_BEFORE_FADE_OUT,
                    12.0f))
                return;

            if (NotificationModule_SetDefaultValue(
                    NOTIFICATION_MODULE_NOTIFICATION_TYPE_INFO,
                    NOTIFICATION_MODULE_DEFAULT_OPTION_KEEP_UNTIL_SHOWN,
                    true))
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

    std::optional<std::filesystem::path>
    PickFontName()
        noexcept
    {
        try {
            SortedPathSet candidates;

            // If a theme is loaded, check for *.ttf and fonts.txt
            if (!gLoadedTheme.empty()) {
                auto themePath = themes_dir / gLoadedTheme;
                if (exists(themePath)) {
                    for (const auto& entry : std::filesystem::directory_iterator{themePath}) {
                        if (!entry.is_regular_file())
                            continue;

                        auto& path = entry.path();

                        // If a *.ttf file
                        if (IsTTF(path))
                            candidates.insert(path.stem());

                        // If fonts.txt
                        if (utils::IgnoreCaseEqual(path.filename(), "fonts.txt")) {
                            try {
                                utils::File f{path, "rt"};
                                static char line[256];
                                while (f.gets(line)) {
                                    auto name = StripEOL(line);
                                    if (gKnownFonts && gKnownFonts->contains(name))
                                        candidates.insert(name);
                                    if (utils::IgnoreCaseEqual(name, "CafeStd"))
                                        candidates.insert(name);
                                }
                            }
                            catch (std::exception& e) {
                                DEBUG_FUNCTION_LINE_ERR("Error reading fonts.txt: %s", e.what());
                            }
                        }
                    }
                }
                if (!candidates.empty())
                    return GetRandomItem(candidates);
            }

            // At this point, either the theme has no built-in font, or has no desired font.

            if (gEnabledFonts.empty())
                return {};

            // When not shuffling fonts, return the first entry of *gEnabledFonts
            if (!gShuffleFonts)
                return *gEnabledFonts.begin();
            else
                return GetRandomItem(gEnabledFonts);
        }
        catch (std::exception& e) {
            DEBUG_FUNCTION_LINE_ERR("Error selecting font: %s", e.what());
        }
        catch (...) {
            DEBUG_FUNCTION_LINE_ERR("Unknown selecting loading font.");
        }
        return {};
    }

    void
    ReloadConfig()
    {
        LoadVar(KEY_THEME_MANAGER_ENABLED,
                gThemeManagerEnabled,
                DEFAULT_THEME_MANAGER_ENABLED);

        // NOTE: let's migrate from "suffleThemes" to "shuffleThemes"
        bool suffleThemes;
        if (LoadVar(KEY_SHUFFLE_THEMES_OLD, suffleThemes)) {
            // If we found "suffleThemes", transfer it to gShuffleThemes and delete the old key.
            gShuffleThemes = suffleThemes;
            StoreVar(KEY_SHUFFLE_THEMES, gShuffleThemes);
            WUPSStorageAPI::DeleteItem(KEY_SHUFFLE_THEMES_OLD);
        } else {
            // "suffleThemes" not found, already migrated
            LoadVar(KEY_SHUFFLE_THEMES,
                    gShuffleThemes,
                    DEFAULT_SHUFFLE_THEMES);
        }

        LoadVar(KEY_MASHUP_THEMES,
                gMashupThemes,
                DEFAULT_MASHUP_THEMES);

        LoadVar(KEY_SHOW_NOTIFICATION,
                gShowNotification,
                DEFAULT_SHOW_NOTIFICATION);

        gEnabledThemes.clear();
        std::string rawEnabledThemes;
        if (LoadVar(KEY_ENABLED_THEMES,
                    rawEnabledThemes,
                    ""s)) {
            gEnabledThemes = SplitPathSet(rawEnabledThemes);
            rawEnabledThemes.clear();
            rawEnabledThemes.shrink_to_fit();
        }

        LoadVar(KEY_FONTS_ENABLED,
                gFontsEnabled,
                DEFAULT_FONTS_ENABLED);

        LoadVar(KEY_SHUFFLE_FONTS,
                gShuffleFonts,
                DEFAULT_SHUFFLE_FONTS);

        LoadVar(KEY_GLOBAL_FONT,
                gGlobalFont,
                DEFAULT_GLOBAL_FONT);

        gEnabledFonts.clear();
        std::string rawEnabledFonts;
        if (LoadVar(KEY_ENABLED_FONTS,
                    rawEnabledFonts,
                    ""s)) {
            gEnabledFonts = SplitPathSet(rawEnabledFonts);
            rawEnabledFonts.clear();
            rawEnabledFonts.shrink_to_fit();
        }

        DEBUG_FUNCTION_LINE_VERBOSE("Finished reloading config.");
    }

    void
    RestartWhenChanged(wupsext::ConfigBooleanVarItemImpl &item)
    {
        DEBUG_FUNCTION_LINE_VERBOSE("Setting %s changed: %s, need to restart.",
                                    item.identifier.data(),
                                    item.variable ? "true" : "false");
        gNeedToRestart = true;
    }

    // NOTE: scan both theme directories and fonts in one pass.
    ThemesAndFonts
    ScanThemesAndFonts(unsigned flags)
    {
        using std::filesystem::directory_iterator;
        using std::filesystem::recursive_directory_iterator;

        DEBUG_FUNCTION_LINE_VERBOSE("scanning themes and fonts: %x", flags);

        std::vector<std::filesystem::path> themes;
        FontNamePathMap fonts;

        // Find all theme folders in SD:/wiiu/themes/
        try {
            if (exists(themes_dir)) {
                for (auto &theme_entry : directory_iterator{themes_dir}) {
                    if (!theme_entry.is_directory())
                        continue;
                    if (flags & ScanFlagsThemes)
                        themes.push_back(theme_entry.path().filename());

                    if (flags & ScanFlagsFonts) {
                        // Scan .ttf fonts on the root of the theme folder.
                        for (const auto &entry : directory_iterator{theme_entry.path()}) {
                            if (!entry.is_regular_file())
                                continue;
                            auto& path = entry.path();
                            if (!IsTTF(path))
                                continue;
                            auto name = path.stem();
                            fonts.emplace(name, path);
                        }
                    }
                }
            }
        }
        catch (std::exception &e) {
            DEBUG_FUNCTION_LINE_WARN("Failed themes scan: %s", e.what());
        }

        if (flags & ScanFlagsFonts) {
            try {
                // Find all fonts in SD:/wiiu/fonts/, including sub-directories.
                if (exists(fonts_dir)) {
                    for (const auto &entry : recursive_directory_iterator{fonts_dir}) {
                        if (!entry.is_regular_file())
                            continue;
                        auto& path = entry.path();
                        if (!IsTTF(path))
                            continue;
                        auto name = path.stem();
                        fonts.emplace(name, path);
                    }
                }
            }
            catch (std::exception &e) {
                DEBUG_FUNCTION_LINE_WARN("Failed fonts scan: %s", e.what());
            }
        }

        DEBUG_FUNCTION_LINE_INFO("Found %u themes, %u fonts.",
                                 static_cast<unsigned>(themes.size()),
                                 static_cast<unsigned>(fonts.size()));

        return {
            SortedPathSet{std::move(themes)},
            std::move(fonts)
        };
    }

    SearchResult
    SearchThemeFiles(const std::filesystem::path &themePath,
                     unsigned flags)
    {
        using utils::IgnoreCaseEqual;

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

    std::unordered_set<std::filesystem::path>
    SplitPathSet(const std::string& input)
    {
        std::unordered_set<std::filesystem::path> result;
        for (const auto token : std::views::split(input, "|"s))
            if (!token.empty())
                result.insert(std::filesystem::path{token.begin(), token.end()});
        return result;
    }

    template <typename T>
    void
    StoreVar(const char* key,
             const T& var)
    {
        if (auto err = WUPSStorageAPI::Store(key, var)) {
            DEBUG_FUNCTION_LINE_ERR("Failed to store %s: %s (%d)",
                                    key,
                                    WUPSStorageAPI_GetStatusStr(err),
                                    err);
        }
    }

    std::string_view
    StripEOL(std::string_view input)
    {
        // NOTE: this specific order catches "\r\n", "\r" and "\n" terminators.
        if (input.ends_with('\n'))
            input.remove_suffix(1);
        if (input.ends_with('\r'))
            input.remove_suffix(1);
        return input;
    }

} // namepace

WUPSConfigAPICallbackStatus
ConfigMenuOpenedCallback(WUPSConfigCategoryHandle rootHandle)
    noexcept
{
    try {
        WUPSStorageAPI::ForceReloadStorage();
        ReloadConfig();

        WUPSConfigCategory root = WUPSConfigCategory(rootHandle);

        using BoolItem = wupsext::ConfigBooleanVarItem;

        root.add(BoolItem{"Enable themes",
                          KEY_THEME_MANAGER_ENABLED,
                          gThemeManagerEnabled,
                          RestartWhenChanged});

        root.add(BoolItem{"Show theme notification",
                          KEY_SHOW_NOTIFICATION,
                          gShowNotification});

        root.add(BoolItem{"Shuffle themes",
                          KEY_SHUFFLE_THEMES,
                          gShuffleThemes,
                          OnShuffleThemesChanged});

        root.add(BoolItem{"Mashup themes",
                          KEY_MASHUP_THEMES,
                          gMashupThemes,
                          RestartWhenChanged});

        root.add(BoolItem{"Enable fonts",
                          KEY_FONTS_ENABLED,
                          gFontsEnabled,
                          RestartWhenChanged});

        root.add(BoolItem{"Shuffle fonts",
                          KEY_SHUFFLE_FONTS,
                          gShuffleFonts,
                          OnShuffleFontsChanged});

        root.add(BoolItem{"Use custom font outside Wii U Menu",
                          KEY_GLOBAL_FONT,
                          gGlobalFont});

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

        if (!gLoadedFont.empty())
            root.add(WUPSConfigItemStub::Create("Loaded font: "
                                                + gLoadedFont));

        // Now build categories for themes and fonts
        using enum wupsext::ConfigToggleItemGroup::Mode;
        using std::filesystem::directory_iterator;

        auto [themes, fonts] = ScanThemesAndFonts();

        gThemesGroup.emplace();
        auto themes_cat = WUPSConfigCategory::Create("Available Themes");
        for (const auto &theme : themes) {
            bool isEnabled = gEnabledThemes.contains(theme);
            themes_cat.add(
                wupsext::ConfigToggleItem{
                    theme.string(),
                    theme.string(),
                    isEnabled,
                    OnThemeItemChanged,
                    &*gThemesGroup
                }
            );
        }
        gThemesGroup->setMode(gShuffleThemes ? Multi : Single);
        root.add(std::move(themes_cat));

        gFontsGroup.emplace();
        auto fonts_cat = WUPSConfigCategory::Create("Available fonts");
        for (const auto& [name, path] : fonts) {
            bool isEnabled = gEnabledFonts.contains(name);
            fonts_cat.add(
                wupsext::ConfigToggleItem{
                    name.string(),
                    path.string(),
                    isEnabled,
                    OnFontItemChanged,
                    &*gFontsGroup
                }
            );
        }
        gFontsGroup->setMode(gShuffleFonts ? Multi : Single);
        root.add(std::move(fonts_cat));

        gKnownFonts = std::move(fonts);

        return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
    }
    catch (std::exception &e) {
        DEBUG_FUNCTION_LINE_ERR("Exception: %s\n", e.what());
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }
}

void
ConfigMenuClosedCallback()
{
    // Update gEnabledThemes based on the enabled theme items.
    gEnabledThemes.clear();
    gThemesGroup->forEach(
        [](wupsext::ConfigToggleItemImpl &item)
        {
            if (item.value) {
                DEBUG_FUNCTION_LINE_VERBOSE("Enabled theme: %s", item.identifier.data());
                gEnabledThemes.insert(item.identifier);
            }
        }
    );

    // Updated gEnabledFonts based on the enabled font items.
    gEnabledFonts.clear();
    gFontsGroup->forEach(
        [](wupsext::ConfigToggleItemImpl &item)
        {
            if (item.value) {
                DEBUG_FUNCTION_LINE_VERBOSE("Enabled font: %s", item.displayName.data());
                gEnabledFonts.insert(item.displayName);
            }
        }
    );

    gThemesGroup.reset();
    gFontsGroup.reset();

    if (!gShuffleThemes && gEnabledThemes.size() > 1)
        DEBUG_FUNCTION_LINE_WARN("Inconsistent state: "
                                 "gEnabledThemes.size() > 1 but gShuffleThemes is false!");

    if (!gShuffleFonts && gEnabledFonts.size() > 1)
        DEBUG_FUNCTION_LINE_WARN("Inconsistent state: "
                                 "gEnabledFonts.size() > 1 but gShuffleFonts is false!");

    StoreVar(KEY_THEME_MANAGER_ENABLED, gThemeManagerEnabled);
    StoreVar(KEY_SHUFFLE_THEMES,        gShuffleThemes);
    StoreVar(KEY_MASHUP_THEMES,         gMashupThemes);
    StoreVar(KEY_SHOW_NOTIFICATION,     gShowNotification);
    StoreVar(KEY_FONTS_ENABLED,         gFontsEnabled);
    StoreVar(KEY_SHUFFLE_FONTS,         gShuffleFonts);
    StoreVar(KEY_GLOBAL_FONT,           gGlobalFont);

    StoreVar(KEY_ENABLED_THEMES, JoinPathSet(gEnabledThemes, gShuffleThemes));
    StoreVar(KEY_ENABLED_FONTS,  JoinPathSet(gEnabledFonts, gShuffleFonts));

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
            gNeedToRestart = true;

        // Handle mashups too: if any component of a mashup is disabled, gotta restart.
        if (!gLoadedMenTheme.empty() &&
            !gEnabledThemes.contains(gLoadedMenTheme))
            gNeedToRestart = true;

        if (!gLoadedMen2Theme.empty() &&
            !gEnabledThemes.contains(gLoadedMen2Theme))
            gNeedToRestart = true;

        if (!gLoadedCafeBaristaTheme.empty() &&
            !gEnabledThemes.contains(gLoadedCafeBaristaTheme))
            gNeedToRestart = true;

        // Case 2: no theme was loaded, now at least one theme is enabled
        if (!gEnabledThemes.empty() &&
            gLoadedTheme.empty() &&
            gLoadedMenTheme.empty() &&
            gLoadedMen2Theme.empty() &&
            gLoadedCafeBaristaTheme.empty())
            gNeedToRestart = true;
    }

    if (gNeedToRestart && CalledFromWiiUMenu()) {
        SYSLaunchMenu();
        gNeedToRestart = false;
    }
}

INITIALIZE_PLUGIN()
{
    InitializePRNG();

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

DEINITIALIZE_PLUGIN()
{
    NotificationModule_DeInitLibrary();
}

ON_APPLICATION_START()
{
    initLogging();

    if (CalledFromWiiUMenu()) {
        WUPSStorageAPI::ForceReloadStorage();
        ReloadConfig();
        PickAndApplyTheme();
    }
}

ON_APPLICATION_ENDS()
{
    gFontStdBlob.reset();
    gLoadedFont.clear();

    if (gContentLayerHandle != 0) {
        ContentRedirection_RemoveFSLayer(gContentLayerHandle);
        gContentLayerHandle = 0;
    }

    deinitLogging();
}

DECL_FUNCTION(BOOL,
              OSGetSharedData,
              OSSharedDataType type,
              uint32_t arg,
              void** out_data,
              uint32_t* out_size)
{
    try {
        if (arg == 0xefface) {
            /*
             * Homebrew apps can pass 0xefface as the (otherwise ignored) argument, to indicate
             * they really desire the original CafeStd font, and not a custom font.
             *
             * efface
             *
             * transitive verb: To cause to disappear (as anything impresses or inscribed upon a
             * surface) by rubbing out, striking out, etc.; to erase; to render illegible or
             * indiscernible.
             */
            arg = 0;

            goto real_function;
        }

        if (!gFontsEnabled) {
            DEBUG_FUNCTION_LINE_VERBOSE("gFontsEnabled is false");
            goto real_function;
        }

        // Never replace the font in the WUPS config menu.
        if (CalledFromWUPSMenu())
            goto real_function;

        // If the user does not want the font outside the Wii U Menu
        if (!gGlobalFont) {

            // Avoid when not inside the Wii U Menu.
            if (!CalledFromWiiUMenu()) {
                DEBUG_FUNCTION_LINE_VERBOSE("Not called from Wii U Menu");
                goto real_function;
            }

            // Avoid when using the on-screen keyboard inside the Wii U Menu.
            if (CalledFromWiiUMenuSWKBD()) {
                DEBUG_FUNCTION_LINE_VERBOSE("Called from Wii U Menu swkbd");
                goto real_function;
            }

        }

        // Actually load the font.
        switch (type) {

            case OS_SHAREDDATATYPE_FONT_STANDARD:

                if (!gKnownFonts) {
                    auto [themes, fonts] = ScanThemesAndFonts(ScanFlagsFonts);
                    gKnownFonts = std::move(fonts);
                }

                // If no fonts were found, give up.
                if (gKnownFonts->empty()) {
                    DEBUG_FUNCTION_LINE_VERBOSE("gKnownFonts is empty");
                    goto real_function;
                }

                if (!gFontStdBlob) {
                    auto fontName = PickFontName();
                    // If no font enabled, and no font provided by theme.
                    if (!fontName) {
                        DEBUG_FUNCTION_LINE_VERBOSE("Could not pick a font name.");
                        goto real_function;
                    }
                    gFontStdBlob = LoadFont(*fontName);
                    if (gShowNotification) {
                        std::string text = "Font: " + gLoadedFont;
                        NotificationModule_AddInfoNotification(text.c_str());
                    }
                }

                // When the blob exists but is empty, we already tried to load, and failed.
                if (gFontStdBlob->empty()) {
                    DEBUG_FUNCTION_LINE_VERBOSE("gFontStdBlob is empty");
                    goto real_function;
                }

                if (out_data)
                    *out_data = gFontStdBlob->data();
                if (out_size)
                    *out_size = gFontStdBlob->size();

                return true;

            default:
                goto real_function;

        } // switch (type)
    }
    catch (std::exception& e) {
        DEBUG_FUNCTION_LINE_ERR("Exception in OSGetSharedData(): %s", e.what());
    }
    catch (...) {
        DEBUG_FUNCTION_LINE_ERR("Unknown exception in OSGetSharedData()");
    }

 real_function:
    return real_OSGetSharedData(type, arg, out_data, out_size);
}

WUPS_MUST_REPLACE_FOR_PROCESS(OSGetSharedData,
                              WUPS_LOADER_LIBRARY_COREINIT,
                              OSGetSharedData,
                              WUPS_FP_TARGET_PROCESS_ALL);
