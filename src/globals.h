#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>

#include <content_redirection/redirection.h>

#define VERSION "v0.5.3-dev"

extern const std::filesystem::path themes_dir;
extern const std::filesystem::path fonts_dir;

#define KEY_THEME_MANAGER_ENABLED "themeManagerEnabled"
#define KEY_SHUFFLE_THEMES        "shuffleThemes"
#define KEY_SHUFFLE_THEMES_OLD    "suffleThemes"
#define KEY_MASHUP_THEMES         "mashupThemes"
#define KEY_SHOW_NOTIFICATION     "showNotification"
#define KEY_FONTS_ENABLED         "fontsEnabled"
#define KEY_SHUFFLE_FONTS         "shuffleFonts"
#define KEY_GLOBAL_FONT           "globalFont"
#define KEY_ENABLED_THEMES        "enabledThemes"
#define KEY_ENABLED_FONTS         "enabledFonts"

#define DEFAULT_THEME_MANAGER_ENABLED true
#define DEFAULT_SHUFFLE_THEMES        false
#define DEFAULT_MASHUP_THEMES         false
#define DEFAULT_SHOW_NOTIFICATION     true
#define DEFAULT_FONTS_ENABLED         true
#define DEFAULT_SHUFFLE_FONTS         false
#define DEFAULT_GLOBAL_FONT           false

extern bool gThemeManagerEnabled;
extern bool gShuffleThemes;
extern bool gMashupThemes;
extern bool gShowNotification;
extern bool gFontsEnabled;
extern bool gShuffleFonts;
extern bool gGlobalFont;

extern std::unordered_set<std::filesystem::path> gEnabledThemes;
extern std::unordered_set<std::filesystem::path> gEnabledFonts;

extern std::string gLoadedTheme;
extern std::string gLoadedMenTheme;
extern std::string gLoadedMen2Theme;
extern std::string gLoadedCafeBaristaTheme;
extern std::string gLoadedFont;

extern CRLayerHandle gContentLayerHandle;
