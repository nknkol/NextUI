extern "C"
{
#include "msettings.h"

#include "defines.h"
#include "api.h"
#include "utils.h"
}

#include <fstream>
#include <sstream>
#include "wifimenu.hpp"
#include "keyboardprompt.hpp"

static int appQuit = false;
static bool appSuspend = false;

static void sigHandler(int sig)
{
    switch (sig)
    {
    case SIGINT:
    case SIGTERM:
        appQuit = true;
        break;
    case SIGSTOP:
        appSuspend = true;
        break;
    case SIGCONT:
        appSuspend = false;
        break;
    default:
        break;
    }
}

struct Context
{
    MenuList *menu;
    SDL_Surface *screen;
    int dirty;
    int show_setting;
};

// This is all the MinUiSettings stuff, for now just copied over from the old settings app

static const std::vector<std::any> colors = {
    0x000022U, 0x000044U, 0x000066U, 0x000088U, 0x0000AAU, 0x0000CCU, 0x1e2329U, 0x3366FFU, 0x4D7AFFU, 0x6699FFU, 0x80B3FFU, 0x99CCFFU, 0xB3D9FFU,
    0x002222U, 0x004444U, 0x006666U, 0x008888U, 0x00AAAAU, 0x00CCCCU, 0x33FFFFU, 0x4DFFFFU, 0x66FFFFU, 0x80FFFFU, 0x99FFFFU, 0xB3FFFFU,
    0x002200U, 0x004400U, 0x006600U, 0x008800U, 0x00AA00U, 0x00CC00U, 0x33FF33U, 0x4DFF4DU, 0x66FF66U, 0x80FF80U, 0x99FF99U, 0xB3FFB3U,
    0x220022U, 0x440044U, 0x660066U, 0x880088U, 0x9B2257U, 0xAA00AAU, 0xCC00CCU, 0xFF33FFU, 0xFF4DFFU, 0xFF66FFU, 0xFF80FFU, 0xFF99FFU, 0xFFB3FFU,
    0x110022U, 0x220044U, 0x330066U, 0x440088U, 0x5500AAU, 0x6600CCU, 0x8833FFU, 0x994DFFU, 0xAA66FFU, 0xBB80FFU, 0xCC99FFU, 0xDDB3FFU,
    0x220000U, 0x440000U, 0x660000U, 0x880000U, 0xAA0000U, 0xCC0000U, 0xFF3333U, 0xFF4D4DU, 0xFF6666U, 0xFF8080U, 0xFF9999U, 0xFFB3B3U,
    0x222200U, 0x444400U, 0x666600U, 0x888800U, 0xAAAA00U, 0xCCCC00U, 0xFFFF33U, 0xFFFF4DU, 0xFFFF66U, 0xFFFF80U, 0xFFFF99U, 0xFFFFB3U,
    0x221100U, 0x442200U, 0x663300U, 0x884400U, 0xAA5500U, 0xCC6600U, 0xFF8833U, 0xFF994DU, 0xFFAA66U, 0xFFBB80U, 0xFFCC99U, 0xFFDDB3U,
    0x000000U, 0x141414U, 0x282828U, 0x3C3C3CU, 0x505050U, 0x646464U, 0x8C8C8CU, 0xA0A0A0U, 0xB4B4B4U, 0xC8C8C8U, 0xDCDCDCU, 0xFFFFFFU};
// all colors above but as strings
static const std::vector<std::string> color_strings = {
    "0x000022", "0x000044", "0x000066", "0x000088", "0x0000AA", "0x0000CC", "0x1E2329", "0x3366FF", "0x4D7AFF", "0x6699FF", "0x80B3FF", "0x99CCFF", "0xB3D9FF",
    "0x002222", "0x004444", "0x006666", "0x008888", "0x00AAAA", "0x00CCCC", "0x33FFFF", "0x4DFFFF", "0x66FFFF", "0x80FFFF", "0x99FFFF", "0xB3FFFF",
    "0x002200", "0x004400", "0x006600", "0x008800", "0x00AA00", "0x00CC00", "0x33FF33", "0x4DFF4D", "0x66FF66", "0x80FF80", "0x99FF99", "0xB3FFB3",
    "0x220022", "0x440044", "0x660066", "0x880088", "0x9B2257", "0xAA00AA", "0xCC00CC", "0xFF33FF", "0xFF4DFF", "0xFF66FF", "0xFF80FF", "0xFF99FF", "0xFFB3FF",
    "0x110022", "0x220044", "0x330066", "0x440088", "0x5500AA", "0x6600CC", "0x8833FF", "0x994DFF", "0xAA66FF", "0xBB80FF", "0xCC99FF", "0xDDB3FF",
    "0x220000", "0x440000", "0x660000", "0x880000", "0xAA0000", "0xCC0000", "0xFF3333", "0xFF4D4D", "0xFF6666", "0xFF8080", "0xFF9999", "0xFFB3B3",
    "0x222200", "0x444400", "0x666600", "0x888800", "0xAAAA00", "0xCCCC00", "0xFFFF33", "0xFFFF4D", "0xFFFF66", "0xFFFF80", "0xFFFF99", "0xFFFFB3",
    "0x221100", "0x442200", "0x663300", "0x884400", "0xAA5500", "0xCC6600", "0xFF8833", "0xFF994D", "0xFFAA66", "0xFFBB80", "0xFFCC99", "0xFFDDB3",
    "0x000000", "0x141414", "0x282828", "0x3C3C3C", "0x505050", "0x646464", "0x8C8C8C", "0xA0A0A0", "0xB4B4B4", "0xC8C8C8", "0xDCDCDC", "0xFFFFFF"};

static const std::vector<std::string> font_names = {"OG", "Next", "NotoSans"};

static const std::vector<std::any> timeout_secs = {0U, 5U, 10U, 15U, 30U, 45U, 60U, 90U, 120U, 240U, 360U, 600U};
static const std::vector<std::string> timeout_labels = {"Never", "5s", "10s", "15s", "30s", "45s", "60s", "90s", "2m", "4m", "6m", "10m"};

static const std::vector<std::string> on_off = {"Off", "On"};

static const std::vector<std::string> scaling_strings = {"Fullscreen", "Fit", "Fill"};
static const std::vector<std::any> scaling = {(int)GFX_SCALE_FULLSCREEN, (int)GFX_SCALE_FIT, (int)GFX_SCALE_FILL};

int main(int argc, char *argv[])
{
    try
    {
        char* device = getenv("DEVICE");
        bool is_brick = exactMatch("brick", device);

        char version[128];
        PLAT_getOsVersionInfo(version, 128);
        LOG_info("This is TrimUI stock OS version %s\n", version);
        InitSettings();

        PWR_setCPUSpeed(CPU_SPEED_MENU);

        Context ctx = {0};
        ctx.dirty = 1;
        ctx.show_setting = 0;
        ctx.screen = GFX_init(MODE_MAIN);
        PAD_init();
        PWR_init();
        TIME_init();
        WIFI_init();

        signal(SIGINT, sigHandler);
        signal(SIGTERM, sigHandler);

        char timezones[MAX_TIMEZONES][MAX_TZ_LENGTH];
        int tz_count = 0;
        TIME_getTimezones(timezones, &tz_count);

        int was_online = PLAT_isOnline();
        
        std::vector<std::any> tz_values;
        std::vector<std::string> tz_labels;
        for (int i = 0; i < tz_count; ++i) {
            tz_values.push_back(std::string(timezones[i]));
            tz_labels.push_back(std::string(timezones[i]));
        }

        auto appearanceMenu = new MenuList(MenuItemType::Fixed, "Appearance",
            {   // 在 Font 的 MenuItem 中，将 {0, 1} 修改为 {0, 1, 2}
                new MenuItem{ListItemType::Generic, "Font", "The font to render all UI text.", {0, 1, 2}, font_names, 
                    []() -> std::any{ return CFG_getFontId(); },
                    [](const std::any &value){ CFG_setFontId(std::any_cast<int>(value)); },
                    []() { CFG_setFontId(CFG_DEFAULT_FONT_ID);}},
                new MenuItem{ListItemType::Color, "Main Color", "The color used to render main UI elements.", colors, color_strings, 
                []() -> std::any{ return CFG_getColor(1); }, 
                [](const std::any &value){ CFG_setColor(1, std::any_cast<uint32_t>(value)); },
                []() { CFG_setColor(1, CFG_DEFAULT_COLOR1);}},
                new MenuItem{ListItemType::Color, "Primary Accent Color", "The color used to highlight important things in the user interface.", colors, color_strings, 
                []() -> std::any{ return CFG_getColor(2); }, 
                [](const std::any &value){ CFG_setColor(2, std::any_cast<uint32_t>(value)); },
                []() { CFG_setColor(2, CFG_DEFAULT_COLOR2);}},
                new MenuItem{ListItemType::Color, "Secondary Accent Color", "A secondary highlight color.", colors, color_strings, 
                []() -> std::any{ return CFG_getColor(3); }, 
                [](const std::any &value){ CFG_setColor(3, std::any_cast<uint32_t>(value)); },
                []() { CFG_setColor(3, CFG_DEFAULT_COLOR3);}},
                new MenuItem{ListItemType::Color, "Hint info Color", "Color for button hints and info", colors, color_strings, 
                []() -> std::any{ return CFG_getColor(6); }, 
                [](const std::any &value){ CFG_setColor(6, std::any_cast<uint32_t>(value)); },
                []() { CFG_setColor(6, CFG_DEFAULT_COLOR6);}},
                new MenuItem{ListItemType::Color, "List Text", "List text color", colors, color_strings, 
                []() -> std::any{ return CFG_getColor(4); }, 
                [](const std::any &value){ CFG_setColor(4, std::any_cast<uint32_t>(value)); },
                []() { CFG_setColor(4, CFG_DEFAULT_COLOR4);}},
                new MenuItem{ListItemType::Color, "List Text Selected", "List selected text color", colors, color_strings, 
                []() -> std::any { return CFG_getColor(5); }, 
                [](const std::any &value) { CFG_setColor(5, std::any_cast<uint32_t>(value)); },
                []() { CFG_setColor(5, CFG_DEFAULT_COLOR5);}},
                new MenuItem{ListItemType::Generic, "Show battery percentage", "Show battery level as percent in the status pill", {false, true}, on_off, 
                []() -> std::any { return CFG_getShowBatteryPercent(); },
                [](const std::any &value) { CFG_setShowBatteryPercent(std::any_cast<bool>(value)); },
                []() { CFG_setShowBatteryPercent(CFG_DEFAULT_SHOWBATTERYPERCENT);}},
                new MenuItem{ListItemType::Generic, "Show menu animations", "Enable or disable menu animations", {false, true}, on_off, 
                []() -> std::any{ return CFG_getMenuAnimations(); },
                [](const std::any &value) { CFG_setMenuAnimations(std::any_cast<bool>(value)); },
                []() { CFG_setMenuAnimations(CFG_DEFAULT_SHOWMENUANIMATIONS);}},
                new MenuItem{ListItemType::Generic, "Show menu transitions", "Enable or disable animated transitions", {false, true}, on_off, 
                []() -> std::any{ return CFG_getMenuTransitions(); },
                [](const std::any &value) { CFG_setMenuTransitions(std::any_cast<bool>(value)); },
                []() { CFG_setMenuTransitions(CFG_DEFAULT_SHOWMENUTRANSITIONS);}},
                new MenuItem{ListItemType::Generic, "Game art corner radius", "Set the radius for the rounded corners of game art", 0, 24, "px",
                []() -> std::any{ return CFG_getThumbnailRadius(); }, 
                [](const std::any &value) { CFG_setThumbnailRadius(std::any_cast<int>(value)); },
                []() { CFG_setThumbnailRadius(CFG_DEFAULT_THUMBRADIUS);}},
                new MenuItem{ListItemType::Generic, "Game art width", "Set the percentage of screen width used for game art.\nUI elements might overrule this to avoid clipping.", 
                5, 100, "%",
                []() -> std::any{ return (int)(CFG_getGameArtWidth() * 100); }, 
                [](const std::any &value) { CFG_setGameArtWidth((double)std::any_cast<int>(value) / 100.0); },
                []() { CFG_setGameArtWidth(CFG_DEFAULT_GAMEARTWIDTH);}},
                new MenuItem{ListItemType::Generic, "Show recents", "Show \"Recently Played\" menu entry.\nThis also disables Game Switcher.", {false, true}, on_off, 
                []() -> std::any { return CFG_getShowRecents(); },
                [](const std::any &value) { CFG_setShowRecents(std::any_cast<bool>(value)); },
                []() { CFG_setShowRecents(CFG_DEFAULT_SHOWRECENTS);}},
                new MenuItem{ListItemType::Generic, "Show game art", "Show game artwork in the main menu", {false, true}, on_off, []() -> std::any
                { return CFG_getShowGameArt(); },
                [](const std::any &value)
                { CFG_setShowGameArt(std::any_cast<bool>(value)); },
                []() { CFG_setShowGameArt(CFG_DEFAULT_SHOWGAMEART);}},
                new MenuItem{ListItemType::Generic, "Use folder background for ROMs", "If enabled, used the emulator background image. Otherwise uses the default.", {false, true}, on_off, []() -> std::any
                { return CFG_getRomsUseFolderBackground(); },
                [](const std::any &value)
                { CFG_setRomsUseFolderBackground(std::any_cast<bool>(value)); },
                []() { CFG_setRomsUseFolderBackground(CFG_DEFAULT_ROMSUSEFOLDERBACKGROUND);}},
                new MenuItem{ListItemType::Generic, "Show Quickswitcher UI", "Show/hide Quickswitcher UI elements.\nWhen hidden, will only draw background images.", {false, true}, on_off, 
                []() -> std::any{ return CFG_getShowQuickswitcherUI(); },
                [](const std::any &value){ CFG_setShowQuickswitcherUI(std::any_cast<bool>(value)); },
                []() { CFG_setShowQuickswitcherUI(CFG_DEFAULT_SHOWQUICKWITCHERUI);}},
                new MenuItem{ListItemType::Button, "Reset to defaults", "Resets all options in this menu to their default values.", ResetCurrentMenu},
        });

        auto displayMenu = new MenuList(MenuItemType::Fixed, "Display",
        {
            new MenuItem{ListItemType::Generic, "Brightness", "Display brightness (0 to 10)", 0, 10, "",[]() -> std::any
            { return GetBrightness(); }, [](const std::any &value)
            { SetBrightness(std::any_cast<int>(value)); },
            []() { SetBrightness(SETTINGS_DEFAULT_BRIGHTNESS);}},
            new MenuItem{ListItemType::Generic, "Color temperature", "Color temperature (0 to 40)", 0, 40, "",[]() -> std::any
            { return GetColortemp(); }, [](const std::any &value)
            { SetColortemp(std::any_cast<int>(value)); },
            []() { SetColortemp(SETTINGS_DEFAULT_COLORTEMP);}},
            new MenuItem{ListItemType::Generic, "Contrast", "Contrast enhancement (-4 to 5)", -4, 5, "",[]() -> std::any
            { return GetContrast(); }, [](const std::any &value)
            { SetContrast(std::any_cast<int>(value)); },
            []() { SetContrast(SETTINGS_DEFAULT_CONTRAST);}},
            new MenuItem{ListItemType::Generic, "Saturation", "Saturation enhancement (-5 to 5)", -5, 5, "",[]() -> std::any
            { return GetSaturation(); }, [](const std::any &value)
            { SetSaturation(std::any_cast<int>(value)); },
            []() { SetSaturation(SETTINGS_DEFAULT_SATURATION);}},
            new MenuItem{ListItemType::Generic, "Exposure", "Exposure enhancement (-4 to 5)", -4, 5, "",[]() -> std::any
            { return GetExposure(); }, [](const std::any &value)
            { SetExposure(std::any_cast<int>(value)); },
            []() { SetExposure(SETTINGS_DEFAULT_EXPOSURE);}},
            new MenuItem{ListItemType::Button, "Reset to defaults", "Resets all options in this menu to their default values.", ResetCurrentMenu},
        });

        auto systemMenu = new MenuList(MenuItemType::Fixed, "System",
        {
            new MenuItem{ListItemType::Generic, "Volume", "Speaker volume", 
            {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20}, 
            {"Muted", "5%","10%","15%","20%","25%","30%","35%","40%","45%","50%","55%","60%","65%","70%","75%","80%","85%","90%","95%","100%"}, 
            []() -> std::any{ return GetVolume(); }, [](const std::any &value)
            { SetVolume(std::any_cast<int>(value)); },
            []() { SetVolume(SETTINGS_DEFAULT_VOLUME);}},
            new MenuItem{ListItemType::Generic, "Screen timeout", "Time before screen turns off (0-600s)", timeout_secs, timeout_labels, []() -> std::any
            { return CFG_getScreenTimeoutSecs(); }, [](const std::any &value)
            { CFG_setScreenTimeoutSecs(std::any_cast<uint32_t>(value)); },
            []() { CFG_setScreenTimeoutSecs(CFG_DEFAULT_SCREENTIMEOUTSECS);}},
            new MenuItem{ListItemType::Generic, "Suspend timeout", "Time before device goes to sleep (0-600s)", timeout_secs, timeout_labels, []() -> std::any
            { return CFG_getSuspendTimeoutSecs(); }, [](const std::any &value)
            { CFG_setSuspendTimeoutSecs(std::any_cast<uint32_t>(value)); },
            []() { CFG_setSuspendTimeoutSecs(CFG_DEFAULT_SUSPENDTIMEOUTSECS);}},
            new MenuItem{ListItemType::Generic, "Haptic feedback", "Enable or disable haptic feedback on certain actions in the OS", {false, true}, on_off, []() -> std::any
            { return CFG_getHaptics(); }, [](const std::any &value)
            { CFG_setHaptics(std::any_cast<bool>(value)); },
            []() { CFG_setHaptics(CFG_DEFAULT_HAPTICS);}},
            new MenuItem{ListItemType::Generic, "Default view", "The initial view to show on boot", 
            {(int)SCREEN_GAMELIST, (int)SCREEN_GAMESWITCHER, (int)SCREEN_QUICKMENU}, 
            {"Content List","Game Switcher","Quick Menu"}, 
            []() -> std::any { return CFG_getDefaultView(); }, 
            [](const std::any &value){ CFG_setDefaultView(std::any_cast<int>(value)); },
            []() { CFG_setDefaultView(CFG_DEFAULT_VIEW);}},
            new MenuItem{ListItemType::Generic, "Show 24h time format", "Show clock in the 24hrs time format", {false, true}, on_off, []() -> std::any
            { return CFG_getClock24H(); },
            [](const std::any &value)
            { CFG_setClock24H(std::any_cast<bool>(value)); },
            []() { CFG_setClock24H(CFG_DEFAULT_CLOCK24H);}},
            new MenuItem{ListItemType::Generic, "Show clock", "Show clock in the status pill", {false, true}, on_off, []() -> std::any
            { return CFG_getShowClock(); },
            [](const std::any &value)
            { CFG_setShowClock(std::any_cast<bool>(value)); },
            []() { CFG_setShowClock(CFG_DEFAULT_SHOWCLOCK);}},
            new MenuItem{ListItemType::Generic, "Set time and date automatically", "Automatically adjust system time\nwith NTP (requires internet access)", {false, true}, on_off, []() -> std::any
            { return TIME_getNetworkTimeSync(); }, [](const std::any &value)
            { TIME_setNetworkTimeSync(std::any_cast<bool>(value)); },
            []() { TIME_setNetworkTimeSync(false);}},
            new MenuItem{ListItemType::Generic, "Time zone", "Your time zone", tz_values, tz_labels, []() -> std::any
            { return std::string(TIME_getCurrentTimezone()); }, [](const std::any &value)
            { TIME_setCurrentTimezone(std::any_cast<std::string>(value).c_str()); },
            []() { TIME_setCurrentTimezone("Asia/Shanghai");}},
            new MenuItem{ListItemType::Generic, "Save format", "The save format to use.\nMinUI: Game.gba.sav, Retroarch: Game.srm, Generic: Game.sav", 
            {(int)SAVE_FORMAT_SAV, (int)SAVE_FORMAT_SRM, (int)SAVE_FORMAT_SRM_UNCOMPRESSED, (int)SAVE_FORMAT_GEN}, 
            {"MinUI (default)", "Retroarch (compressed)", "Retroarch (uncompressed)", "Generic"}, []() -> std::any
            { return CFG_getSaveFormat(); }, [](const std::any &value)
            { CFG_setSaveFormat(std::any_cast<int>(value)); },
            []() { CFG_setSaveFormat(CFG_DEFAULT_SAVEFORMAT);}},
            new MenuItem{ListItemType::Generic, "Save state format", "The save state format to use.\nMinUI: Game.st0, Retroarch: Game.state.0", 
            {(int)STATE_FORMAT_SAV, (int)STATE_FORMAT_SRM, (int)STATE_FORMAT_SRM_UNCOMRESSED}, 
            {"MinUI (default)", "Retroarch (compressed)", "Retroarch (uncompressed)"}, []() -> std::any
            { return CFG_getStateFormat(); }, [](const std::any &value)
            { CFG_setStateFormat(std::any_cast<int>(value)); },
            []() { CFG_setStateFormat(CFG_DEFAULT_STATEFORMAT);}},
            new MenuItem{ListItemType::Button, "Reset to defaults", "Resets all options in this menu to their default values.", ResetCurrentMenu},
        });

        std::vector<AbstractMenuItem*> muteItems = 
        {
            new MenuItem{ListItemType::Generic, "Volume when toggled", "Speaker volume (0-20)", 
            {(int)SETTINGS_DEFAULT_MUTE_NO_CHANGE, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20}, 
            {"Unchanged", "Muted", "5%","10%","15%","20%","25%","30%","35%","40%","45%","50%","55%","60%","65%","70%","75%","80%","85%","90%","95%","100%"}, 
            []() -> std::any { return GetMutedVolume(); },
            [](const std::any &value) { SetMutedVolume(std::any_cast<int>(value)); },
            []() { SetMutedVolume(0); }},
            new MenuItem{ListItemType::Generic, "FN switch disables LED", "Switch will also disable LEDs", {false, true}, on_off, 
            []() -> std::any { return CFG_getMuteLEDs(); },
            [](const std::any &value) { CFG_setMuteLEDs(std::any_cast<bool>(value)); },
            []() { CFG_setMuteLEDs(CFG_DEFAULT_MUTELEDS); }},
            new MenuItem{ListItemType::Generic, "Brightness when toggled", "Display brightness (0 to 10)", 
            {(int)SETTINGS_DEFAULT_MUTE_NO_CHANGE, 0,1,2,3,4,5,6,7,8,9,10}, 
            {"Unchanged","0","1","2","3","4","5","6","7","8","9","10"},
            []() -> std::any { return GetMutedBrightness(); }, [](const std::any &value)
            { SetMutedBrightness(std::any_cast<int>(value)); },
            []() { SetMutedBrightness(SETTINGS_DEFAULT_MUTE_NO_CHANGE);}},
            new MenuItem{ListItemType::Generic, "Color temperature when toggled", "Color temperature (0 to 40)", 
            {(int)SETTINGS_DEFAULT_MUTE_NO_CHANGE, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40}, 
            {"Unchanged","0","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20","21","22","23","24","25","26","27","28","29","30","31","32","33","34","35","36","37","38","39","40"},
            []() -> std::any{ return GetMutedColortemp(); }, [](const std::any &value)
            { SetMutedColortemp(std::any_cast<int>(value)); },
            []() { SetMutedColortemp(SETTINGS_DEFAULT_MUTE_NO_CHANGE);}},
            new MenuItem{ListItemType::Generic, "Contrast when toggled", "Contrast enhancement (-4 to 5)", 
            {(int)SETTINGS_DEFAULT_MUTE_NO_CHANGE, -4,-3,-2,-1,0,1,2,3,4,5}, 
            {"Unchanged","-4","-3","-2","-1","0","1","2","3","4","5"}, 
            []() -> std::any  { return GetMutedContrast(); }, [](const std::any &value)
            { SetMutedContrast(std::any_cast<int>(value)); },
            []() { SetMutedContrast(SETTINGS_DEFAULT_MUTE_NO_CHANGE);}},
            new MenuItem{ListItemType::Generic, "Saturation when toggled", "Saturation enhancement (-5 to 5)", 
            {(int)SETTINGS_DEFAULT_MUTE_NO_CHANGE, -5,-4,-3,-2,-1,0,1,2,3,4,5}, 
            {"Unchanged","-5","-4","-3","-2","-1","0","1","2","3","4","5"}, 
            []() -> std::any{ return GetMutedSaturation(); }, [](const std::any &value)
            { SetMutedSaturation(std::any_cast<int>(value)); },
            []() { SetMutedSaturation(SETTINGS_DEFAULT_MUTE_NO_CHANGE);}},
            new MenuItem{ListItemType::Generic, "Exposure when toggled", "Exposure enhancement (-4 to 5)", 
            {(int)SETTINGS_DEFAULT_MUTE_NO_CHANGE, -4,-3,-2,-1,0,1,2,3,4,5}, 
            {"Unchanged","-4","-3","-2","-1","0","1","2","3","4","5"}, 
            []() -> std::any  { return GetMutedExposure(); }, [](const std::any &value)
            { SetMutedExposure(std::any_cast<int>(value)); },
            []() { SetMutedExposure(SETTINGS_DEFAULT_MUTE_NO_CHANGE);}},
            new MenuItem{ListItemType::Generic, "Turbo fire A", "Enable turbo fire A", {0, 1}, on_off, []() -> std::any
            { return GetMuteTurboA(); },
            [](const std::any &value) { SetMuteTurboA(std::any_cast<int>(value));},
            []() { SetMuteTurboA(0);}},
            new MenuItem{ListItemType::Generic, "Turbo fire B", "Enable turbo fire B", {0, 1}, on_off, []() -> std::any
            { return GetMuteTurboB(); },
            [](const std::any &value) { SetMuteTurboB(std::any_cast<int>(value));},
            []() { SetMuteTurboB(0);}},
            new MenuItem{ListItemType::Generic, "Turbo fire X", "Enable turbo fire X", {0, 1}, on_off, []() -> std::any
            { return GetMuteTurboX(); },
            [](const std::any &value) { SetMuteTurboX(std::any_cast<int>(value));},
            []() { SetMuteTurboX(0);}},
            new MenuItem{ListItemType::Generic, "Turbo fire Y", "Enable turbo fire Y", {0, 1}, on_off, []() -> std::any
            { return GetMuteTurboY(); },
            [](const std::any &value) { SetMuteTurboY(std::any_cast<int>(value));},
            []() { SetMuteTurboY(0);}},
            new MenuItem{ListItemType::Generic, "Turbo fire L1", "Enable turbo fire L1", {0, 1}, on_off, []() -> std::any
            { return GetMuteTurboL1(); },
            [](const std::any &value) { SetMuteTurboL1(std::any_cast<int>(value));},
            []() { SetMuteTurboL1(0);}},
            new MenuItem{ListItemType::Generic, "Turbo fire L2", "Enable turbo fire L2", {0, 1}, on_off, []() -> std::any
            { return GetMuteTurboL2(); },
            [](const std::any &value) { SetMuteTurboL2(std::any_cast<int>(value));},
            []() { SetMuteTurboL2(0);}},
            new MenuItem{ListItemType::Generic, "Turbo fire R1", "Enable turbo fire R1", {0, 1}, on_off, []() -> std::any
            { return GetMuteTurboR1(); },
            [](const std::any &value) { SetMuteTurboR1(std::any_cast<int>(value));},
            []() { SetMuteTurboR1(0);}},
            new MenuItem{ListItemType::Generic, "Turbo fire R2", "Enable turbo fire R2", {0, 1}, on_off, []() -> std::any
            { return GetMuteTurboR2(); },
            [](const std::any &value) { SetMuteTurboR2(std::any_cast<int>(value));},
            []() { SetMuteTurboR2(0);}},
        };
        if(is_brick) {
            muteItems.push_back(
                new MenuItem{ListItemType::Generic, "Dpad mode when toggled", "Dpad: default. Joystick: Dpad exclusively acts as analog stick.\nBoth: Dpad and Joystick inputs at the same time.", {0, 1, 2}, {"Dpad", "Joystick", "Both"}, []() -> std::any
                {
                    if(!GetMuteDisablesDpad() && !GetMuteEmulatesJoystick()) return 0;
                    if(!GetMuteDisablesDpad() && GetMuteEmulatesJoystick()) return 1;
                    return 2; 
                },
                [](const std::any &value)
                { 
                    int v = std::any_cast<int>(value);
                    SetMuteDisablesDpad((v == 1)); 
                    SetMuteEmulatesJoystick((v > 0));
                },
                []()
                { 
                    SetMuteDisablesDpad(0); 
                    SetMuteEmulatesJoystick(0);
                }});
        }
        muteItems.push_back(new MenuItem{ListItemType::Button, "Reset to defaults", "Resets all options in this menu to their default values.", ResetCurrentMenu});

        auto muteMenu = new MenuList(MenuItemType::Fixed, "FN Switch", muteItems);

        auto networkMenu = new Wifi::Menu(appQuit);

        auto aboutMenu = new MenuList(MenuItemType::Fixed, "About",
        {
            new StaticMenuItem{ListItemType::Generic, "NextUI version", "", 
            []() -> std::any { 
                std::ifstream t(ROOT_SYSTEM_PATH "/version.txt");
                std::stringstream buffer;
                buffer << t.rdbuf();
                return buffer.str();
            }},
            new StaticMenuItem{ListItemType::Generic, "Platform", "", 
            []() -> std::any { 
                return std::string(PLAT_getModel()); }
            },
            new StaticMenuItem{ListItemType::Generic, "Stock OS version", "", 
            []() -> std::any { 
                char osver[128];
                PLAT_getOsVersionInfo(osver, 128);
                return std::string(osver); }
            },
        });

        ctx.menu = new MenuList(MenuItemType::Main, "Settings",
        {
            new MenuItem{ListItemType::Generic, "Appearance", "UI customization", {}, {}, nullptr, nullptr, nullptr, DeferToSubmenu, appearanceMenu},
            new MenuItem{ListItemType::Generic, "Display", "", {}, {}, nullptr, nullptr, nullptr, DeferToSubmenu, displayMenu},
            new MenuItem{ListItemType::Generic, "System", "", {}, {}, nullptr, nullptr, nullptr, DeferToSubmenu, systemMenu},
            new MenuItem{ListItemType::Generic, "FN switch", "FN switch settings", {}, {}, nullptr, nullptr, nullptr, DeferToSubmenu, muteMenu},
            new MenuItem{ListItemType::Generic, "Network", "", {}, {}, nullptr, nullptr, nullptr, DeferToSubmenu, networkMenu},
            new MenuItem{ListItemType::Generic, "About", "", {}, {}, nullptr, nullptr, nullptr, DeferToSubmenu, aboutMenu},
        });

        const bool showTitle = true;
        const bool showIndicator = true;
        const bool showHints = true;

        SDL_Surface* bgbmp = IMG_Load(SDCARD_PATH "/bg.png");
        SDL_Surface* convertedbg = SDL_ConvertSurfaceFormat(bgbmp, SDL_PIXELFORMAT_RGB565, 0);
        if (convertedbg) {
            SDL_FreeSurface(bgbmp); 
            SDL_Surface* scaled = SDL_CreateRGBSurfaceWithFormat(0, ctx.screen->w, ctx.screen->h, 32, SDL_PIXELFORMAT_RGB565);
            GFX_blitScaleToFill(convertedbg, scaled);
            bgbmp = scaled;
        }

        SDL_Rect listRect = {SCALE1(PADDING), SCALE1(PADDING), ctx.screen->w - SCALE1(PADDING * 2), ctx.screen->h - SCALE1(PADDING * 2)};
        if (showTitle || showIndicator)
            listRect = dy(listRect, SCALE1(PILL_SIZE));
        if (showHints)
            listRect.h -= SCALE1(PILL_SIZE);
        ctx.menu->performLayout(listRect);

        while (!appQuit)
        {
            GFX_startFrame();
            uint32_t now = SDL_GetTicks();
            PAD_poll();

            ctx.menu->handleInput(ctx.dirty, appQuit);

            PWR_update(&ctx.dirty, &ctx.show_setting, nullptr, nullptr);

            int is_online = PLAT_isOnline();
            if (was_online!=is_online) ctx.dirty = 1;
            was_online = is_online;

            if (ctx.dirty)
            {
                GFX_clear(ctx.screen);
                if(bgbmp) {
                    SDL_Rect image_rect = {0, 0, ctx.screen->w, ctx.screen->h};
                    SDL_BlitSurface(bgbmp, NULL, ctx.screen, &image_rect);
                }

                int ow = 0;

                if (showIndicator)
                {
                    ow = GFX_blitHardwareGroup(ctx.screen, ctx.show_setting);
                }
                int max_width = ctx.screen->w - SCALE1(PADDING * 2) - ow;

                if (showTitle)
                {
                    char display_name[256];
                    int text_width = GFX_truncateText(font.large, "Settings", display_name, max_width, SCALE1(BUTTON_PADDING * 2));
                    max_width = MIN(max_width, text_width);

                    SDL_Surface *text;
                    text = TTF_RenderUTF8_Blended(font.large, display_name, uintToColour(THEME_COLOR6_255));
                    SDL_Rect target = {SCALE1(PADDING), SCALE1(PADDING), max_width, SCALE1(PILL_SIZE)};
                    GFX_blitPillLight(ASSET_WHITE_PILL, ctx.screen, &target);
                    SDL_BlitSurfaceCPP(text, {0, 0, max_width - SCALE1(BUTTON_PADDING * 2), text->h}, ctx.screen, {SCALE1(PADDING + BUTTON_PADDING), SCALE1(PADDING + 4)});
                    SDL_FreeSurface(text);
                }

                if (showHints)
                {
                    if (ctx.show_setting && !GetHDMI()) {
                        GFX_blitHardwareHints(ctx.screen, ctx.show_setting);
                    }
                    else
                    {
                        char* left_hints[] = {(char*)(BTN_SLEEP==BTN_POWER?"POWER":"MENU"), (char*)"SLEEP", NULL};
                        GFX_blitButtonGroup(left_hints, 0, ctx.screen, 0);
                    }
                    char *right_hints[] = {(char *)("B"), (char *)("BACK"), (char *)("A"), (char *)("OKAY"), NULL};
                    GFX_blitButtonGroup(right_hints, 1, ctx.screen, 1);
                }

                ctx.menu->draw(ctx.screen, listRect);

                GFX_flip(ctx.screen);
                ctx.dirty = false;
            }
        }

        delete ctx.menu;
        delete appearanceMenu;
        delete systemMenu;
        ctx.menu = NULL;

        QuitSettings();
        PWR_quit();
        PAD_quit();
        GFX_quit();

        return EXIT_SUCCESS;
    }
    catch (const std::exception &e)
    {
        LOG_error("%s", e.what());
        QuitSettings();
        PWR_quit();
        PAD_quit();
        GFX_quit();

        return EXIT_FAILURE;
    }
}
