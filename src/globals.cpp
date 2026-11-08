#include "globals.h"

const std::filesystem::path themes_dir = "fs:/vol/external01/wiiu/themes";
const std::filesystem::path fonts_dir = "fs:/vol/external01/wiiu/fonts";

bool gThemeManagerEnabled = DEFAULT_THEME_MANAGER_ENABLED;
bool gShuffleThemes       = DEFAULT_SHUFFLE_THEMES;
bool gMashupThemes        = DEFAULT_MASHUP_THEMES;
bool gShowNotification    = DEFAULT_SHOW_NOTIFICATION;
bool gFontsEnabled        = DEFAULT_FONTS_ENABLED;
bool gShuffleFonts        = DEFAULT_SHUFFLE_FONTS;
bool gGlobalFont          = DEFAULT_GLOBAL_FONT;

std::unordered_set<std::filesystem::path> gEnabledThemes;
std::unordered_set<std::filesystem::path> gEnabledFonts;

std::string gLoadedTheme;
std::string gLoadedMenTheme;
std::string gLoadedMen2Theme;
std::string gLoadedCafeBaristaTheme;
std::string gLoadedFont;

CRLayerHandle gContentLayerHandle = 0;
