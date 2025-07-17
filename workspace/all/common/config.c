#include "config.h"
#include "defines.h"
#include "utils.h"
#include "lang.h" // 新增头文件引用

NextUISettings settings = {0};

// deprecated
uint32_t THEME_COLOR1_255;
uint32_t THEME_COLOR2_255;
uint32_t THEME_COLOR3_255;
uint32_t THEME_COLOR4_255;
uint32_t THEME_COLOR5_255;
uint32_t THEME_COLOR6_255;
uint32_t THEME_COLOR7_255;

static inline uint32_t HexToUint32_unmapped(const char *hexColor) {
    // Convert the hex string to an unsigned long
    uint32_t value = (uint32_t)strtoul(hexColor, NULL, 16);
    return value;
}

void CFG_defaults(NextUISettings *cfg)
{
    if (!cfg)
        return;

    NextUISettings defaults = {
        .font = CFG_DEFAULT_FONT_ID,
        .color1_255 = CFG_DEFAULT_COLOR1,
        .color2_255 = CFG_DEFAULT_COLOR2,
        .color3_255 = CFG_DEFAULT_COLOR3,
        .color4_255 = CFG_DEFAULT_COLOR4,
        .color5_255 = CFG_DEFAULT_COLOR5,
        .color6_255 = CFG_DEFAULT_COLOR6,
        .color7_255 = CFG_DEFAULT_COLOR7,
        .thumbRadius = CFG_DEFAULT_THUMBRADIUS,
        .gameArtWidth = CFG_DEFAULT_GAMEARTWIDTH,

        .showClock = CFG_DEFAULT_SHOWCLOCK,
        .clock24h = CFG_DEFAULT_CLOCK24H,
        .showBatteryPercent = CFG_DEFAULT_SHOWBATTERYPERCENT,
        .showMenuAnimations = CFG_DEFAULT_SHOWMENUANIMATIONS,
        .showMenuTransitions = CFG_DEFAULT_SHOWMENUTRANSITIONS,
        .showRecents = CFG_DEFAULT_SHOWRECENTS,
        .showGameArt = CFG_DEFAULT_SHOWGAMEART,
        .gameSwitcherScaling = CFG_DEFAULT_GAMESWITCHERSCALING,
        .defaultView = CFG_DEFAULT_VIEW,
        .showQuickSwitcherUi = CFG_DEFAULT_SHOWQUICKWITCHERUI,

        .muteLeds = CFG_DEFAULT_MUTELEDS,

        .screenTimeoutSecs = CFG_DEFAULT_SCREENTIMEOUTSECS,
        .suspendTimeoutSecs = CFG_DEFAULT_SUSPENDTIMEOUTSECS,

        .haptics = CFG_DEFAULT_HAPTICS,
        .romsUseFolderBackground = CFG_DEFAULT_ROMSUSEFOLDERBACKGROUND,
        .saveFormat = CFG_DEFAULT_SAVEFORMAT,
        .stateFormat = CFG_DEFAULT_STATEFORMAT,

        .wifi = CFG_DEFAULT_WIFI,
        .wifiDiagnostics = CFG_DEFAULT_WIFI_DIAG,
    };
    
    // 新增：设置默认语言
    strncpy(defaults.language, CFG_DEFAULT_LANGUAGE, sizeof(defaults.language) -1);
    defaults.language[sizeof(defaults.language) -1] = '\0';


    *cfg = defaults;
}

void CFG_init(FontLoad_callback_t cb, ColorSet_callback_t ccb)
{
    CFG_defaults(&settings);
    settings.onFontChange = cb;
    settings.onColorSet = ccb;
    bool fontLoaded = false;

    char settingsPath[MAX_PATH];
    sprintf(settingsPath, "%s/minuisettings.txt", SHARED_USERDATA_PATH);
    FILE *file = fopen(settingsPath, "r");
    if (file == NULL)
    {
        printf("[CFG] Unable to open settings file, loading defaults\n");
    }
    else
    {
        char line[256];
        while (fgets(line, sizeof(line), file))
        {
            int temp_value;
            uint32_t temp_color;
            if (sscanf(line, "font=%i", &temp_value) == 1)
            {
                CFG_setFontId(temp_value);
                fontLoaded = true;
                continue;
            }
            if (sscanf(line, "color1=%x", &temp_color) == 1)
            {
                char hexColor[7];
                snprintf(hexColor, sizeof(hexColor), "%06x", temp_color);
                CFG_setColor(1, HexToUint32_unmapped(hexColor));
                continue;
            }
            if (sscanf(line, "color2=%x", &temp_color) == 1)
            {
                CFG_setColor(2, temp_color);
                continue;
            }
            if (sscanf(line, "color3=%x", &temp_color) == 1)
            {
                CFG_setColor(3, temp_color);
                continue;
            }
            if (sscanf(line, "color4=%x", &temp_color) == 1)
            {
                CFG_setColor(4, temp_color);
                continue;
            }
            if (sscanf(line, "color5=%x", &temp_color) == 1)
            {
                CFG_setColor(5, temp_color);
                continue;
            }
            if (sscanf(line, "color6=%x", &temp_color) == 1)
            {
                CFG_setColor(6, temp_color);
                continue;
            }
            if (sscanf(line, "color7=%x", &temp_color) == 1)
            {
                CFG_setColor(7, temp_color);
                continue;
            }
            if (sscanf(line, "radius=%i", &temp_value) == 1)
            {
                CFG_setThumbnailRadius(temp_value);
                continue;
            }
            if (sscanf(line, "showclock=%i", &temp_value) == 1)
            {
                CFG_setShowClock((bool)temp_value);
                continue;
            }
            if (sscanf(line, "clock24h=%i", &temp_value) == 1)
            {
                CFG_setClock24H((bool)temp_value);
                continue;
            }
            if (sscanf(line, "batteryperc=%i", &temp_value) == 1)
            {
                CFG_setShowBatteryPercent((bool)temp_value);
                continue;
            }
            if (sscanf(line, "menuanim=%i", &temp_value) == 1)
            {
                CFG_setMenuAnimations((bool)temp_value);
                continue;
            }
            if (sscanf(line, "menutransitions=%i", &temp_value) == 1)
            {
                CFG_setMenuTransitions((bool)temp_value);
                continue;
            }
            if (sscanf(line, "recents=%i", &temp_value) == 1)
            {
                CFG_setShowRecents((bool)temp_value);
                continue;
            }
            if (sscanf(line, "gameart=%i", &temp_value) == 1)
            {
                CFG_setShowGameArt((bool)temp_value);
                continue;
            }
            if (sscanf(line, "screentimeout=%i", &temp_value) == 1)
            {
                CFG_setScreenTimeoutSecs(temp_value);
                continue;
            }
            if (sscanf(line, "suspendTimeout=%i", &temp_value) == 1)
            {
                CFG_setSuspendTimeoutSecs(temp_value);
                continue;
            }
            if (sscanf(line, "switcherscale=%i", &temp_value) == 1)
            {
                CFG_setGameSwitcherScaling(temp_value);
                continue;
            }
            if (sscanf(line, "haptics=%i", &temp_value) == 1)
            {
                CFG_setHaptics((bool)temp_value);
                continue;
            }
            if (sscanf(line, "romfolderbg=%i", &temp_value) == 1)
            {
                CFG_setRomsUseFolderBackground((bool)temp_value);
                continue;
            }
            if (sscanf(line, "saveFormat=%i", &temp_value) == 1)
            {
                CFG_setSaveFormat(temp_value);
                continue;
            }
            if (sscanf(line, "stateFormat=%i", &temp_value) == 1)
            {
                CFG_setStateFormat(temp_value);
                continue;
            }
            if (sscanf(line, "muteLeds=%i", &temp_value) == 1)
            {
                CFG_setMuteLEDs(temp_value);
                continue;
            }
            if (sscanf(line, "artWidth=%i", &temp_value) == 1)
            {
                CFG_setGameArtWidth((double)temp_value / 100.0);
                continue;
            }
            if (sscanf(line, "wifi=%i", &temp_value) == 1)
            {
                CFG_setWifi((bool)temp_value);
                continue;
            }
            if (sscanf(line, "defaultView=%i", &temp_value) == 1)
            {
                CFG_setDefaultView(temp_value);
                continue;
            }
            if (sscanf(line, "quickSwitcherUi=%i", &temp_value) == 1)
            {
                CFG_setShowQuickswitcherUI(temp_value);
                continue;
            }
            if (sscanf(line, "wifiDiagnostics=%i", &temp_value) == 1)
            {
                CFG_setWifiDiagnostics(temp_value);
                continue;
            }
            // 新增：读取语言设置
            char temp_lang[8];
            if (sscanf(line, "language=%7s", temp_lang) == 1) {
                strncpy(settings.language, temp_lang, sizeof(settings.language) - 1);
                settings.language[sizeof(settings.language) - 1] = '\0';
                continue;
            }
        }
        fclose(file);
    }
    // 新增：在加载配置后初始化语言模块
    Lang_Init(settings.language);

    // load gfx related stuff until we drop the indirection
    CFG_setColor(1, CFG_getColor(1));
    CFG_setColor(2, CFG_getColor(2));
    CFG_setColor(3, CFG_getColor(3));
    CFG_setColor(4, CFG_getColor(4));
    CFG_setColor(5, CFG_getColor(5));
    CFG_setColor(6, CFG_getColor(6));
    CFG_setColor(7, CFG_getColor(7));
    // avoid reloading the font if not neccessary
    if (!fontLoaded)
        CFG_setFontId(CFG_getFontId());
}
// 新增函数
const char* CFG_getLanguage(void) {
    return settings.language;
}

// 新增函数
void CFG_setLanguage(const char* lang) {
    if (lang && strlen(lang) < sizeof(settings.language)) {
        strncpy(settings.language, lang, sizeof(settings.language));
        settings.language[sizeof(settings.language)-1] = '\0';
        // 立即应用语言更改
        Lang_Init(settings.language);
    }
}
int CFG_getFontId(void)
{
    return settings.font;
}

void CFG_setFontId(int id)
{
    // 将 clamp 的上限从 2 改为 3 (或更高，取决于你未来可能添加的字体数量)
    // 或者直接改为你的新字体最大ID，这里是 2。
    settings.font = clamp(id, 0, 2);

    char *fontPath;
    if (settings.font == 1)
        fontPath = RES_PATH "/font1.ttf";
    else if (settings.font == 2) // 添加这个 else if 分支
        fontPath = RES_PATH "/font3.ttf";
    else // 原来的 font2.ttf 逻辑变为 else
        fontPath = RES_PATH "/font2.ttf";

    if(settings.onFontChange)
        settings.onFontChange(fontPath);
}

uint32_t CFG_getColor(int color_id)
{
    switch (color_id)
    {
    case 1:
        return settings.color1_255;
    case 2:
        return settings.color2_255;
    case 3:
        return settings.color3_255;
    case 4:
        return settings.color4_255;
    case 5:
        return settings.color5_255;
    case 6:
        return settings.color6_255;
    case 7:
        return settings.color7_255;
    default:
        return 0;
    }
}

void CFG_setColor(int color_id, uint32_t color)
{
    switch (color_id)
    {
    case 1:
        settings.color1_255 = color;
        THEME_COLOR1_255 = settings.color1_255;
        break;
    case 2:
        settings.color2_255 = color;
        THEME_COLOR2_255 = settings.color2_255;
        break;
    case 3:
        settings.color3_255 = color;
        THEME_COLOR3_255 = settings.color3_255;
        break;
    case 4:
        settings.color4_255 = color;
        THEME_COLOR4_255 = settings.color4_255;
        break;
    case 5:
        settings.color5_255 = color;
        THEME_COLOR5_255 = settings.color5_255;
        break;
    case 6:
        settings.color6_255 = color;
        THEME_COLOR6_255 = settings.color6_255;
        break;
    case 7:
        settings.color7_255 = color;
        THEME_COLOR7_255 = settings.color7_255;
        break;
    default:
        break;
    }

    if(settings.onColorSet)
        settings.onColorSet();
}

uint32_t CFG_getScreenTimeoutSecs(void)
{
    return settings.screenTimeoutSecs;
}

void CFG_setScreenTimeoutSecs(uint32_t secs)
{
    settings.screenTimeoutSecs = secs;
}

uint32_t CFG_getSuspendTimeoutSecs(void)
{
    return settings.suspendTimeoutSecs;
}

void CFG_setSuspendTimeoutSecs(uint32_t secs)
{
    settings.suspendTimeoutSecs = secs;
}

bool CFG_getShowClock(void)
{
    return settings.showClock;
}

void CFG_setShowClock(bool show)
{
    settings.showClock = show;
}

bool CFG_getClock24H(void)
{
    return settings.clock24h;
}

void CFG_setClock24H(bool is24)
{
    settings.clock24h = is24;
}

bool CFG_getShowBatteryPercent(void)
{
    return settings.showBatteryPercent;
}

void CFG_setShowBatteryPercent(bool show)
{
    settings.showBatteryPercent = show;
}

bool CFG_getMenuAnimations(void)
{
    return settings.showMenuAnimations;
}

void CFG_setMenuAnimations(bool show)
{
    settings.showMenuAnimations = show;
}

bool CFG_getMenuTransitions(void)
{
    return settings.showMenuTransitions;
}

void CFG_setMenuTransitions(bool show)
{
    settings.showMenuTransitions = show;
}

int CFG_getThumbnailRadius(void)
{
    return settings.thumbRadius;
}

void CFG_setThumbnailRadius(int radius)
{
    settings.thumbRadius = clamp(radius, 0, 24);
}

bool CFG_getShowRecents(void)
{
    return settings.showRecents;
}

void CFG_setShowRecents(bool show)
{
    settings.showRecents = show;
}

bool CFG_getShowGameArt(void)
{
    return settings.showGameArt;
}

void CFG_setShowGameArt(bool show)
{
    settings.showGameArt = show;
}

bool CFG_getRomsUseFolderBackground(void)
{
    return settings.romsUseFolderBackground;
}

void CFG_setRomsUseFolderBackground(bool folder)
{
    settings.romsUseFolderBackground = folder;
}

int CFG_getGameSwitcherScaling(void)
{
    return settings.gameSwitcherScaling;
}

void CFG_setGameSwitcherScaling(int enumValue)
{
    settings.gameSwitcherScaling = clamp(enumValue, 0, GFX_SCALE_NUM_OPTIONS);
}

bool CFG_getHaptics(void)
{
    return settings.haptics;
}

void CFG_setHaptics(bool enable)
{
    settings.haptics = enable;
}

int CFG_getSaveFormat(void)
{
    return settings.saveFormat;
}

void CFG_setSaveFormat(int f)
{
    settings.saveFormat = f;
}

int CFG_getStateFormat(void)
{
    return settings.stateFormat;
}

void CFG_setStateFormat(int f)
{
    settings.stateFormat = f;
}

bool CFG_getMuteLEDs(void)
{
    return settings.muteLeds;
}

void CFG_setMuteLEDs(bool on)
{
    settings.muteLeds = on;
}

double CFG_getGameArtWidth(void)
{
    return settings.gameArtWidth;
}

void CFG_setGameArtWidth(double zeroToOne)
{
    settings.gameArtWidth = clampd(zeroToOne, 0.0, 1.0);
}

bool CFG_getWifi(void)
{
    return settings.wifi;
}

void CFG_setWifi(bool on)
{
    settings.wifi = on;
}

int CFG_getDefaultView(void)
{
    return settings.defaultView;
}

void CFG_setDefaultView(int view)
{
    settings.defaultView = view;
}

bool CFG_getShowQuickswitcherUI(void)
{
    return settings.showQuickSwitcherUi;
}

void CFG_setShowQuickswitcherUI(bool on)
{
    settings.showQuickSwitcherUi = on;
}

bool CFG_getWifiDiagnostics(void)
{
    return settings.wifiDiagnostics;
}

void CFG_setWifiDiagnostics(bool on)
{
    settings.wifiDiagnostics = on;
}

void CFG_get(const char *key, char *value)
{
    if (strcmp(key, "font") == 0)
    {
        sprintf(value, "%i", CFG_getFontId());
    }
    else if (strcmp(key, "color1") == 0)
    {
        sprintf(value, "\"0x%06X\"", CFG_getColor(1));
    }
    else if (strcmp(key, "color2") == 0)
    {
        sprintf(value, "\"0x%06X\"", CFG_getColor(2));
    }
    else if (strcmp(key, "color3") == 0)
    {
        sprintf(value, "\"0x%06X\"", CFG_getColor(3));
    }
    else if (strcmp(key, "color4") == 0)
    {
        sprintf(value, "\"0x%06X\"", CFG_getColor(4));
    }
    else if (strcmp(key, "color5") == 0)
    {
        sprintf(value, "\"0x%06X\"", CFG_getColor(5));
    }
    else if (strcmp(key, "color6") == 0)
    {
        sprintf(value, "\"0x%06X\"", CFG_getColor(6));
    }
    else if (strcmp(key, "color7") == 0)
    {
        sprintf(value, "\"0x%06X\"", CFG_getColor(7));
    }
    else if (strcmp(key, "radius") == 0)
    {
        sprintf(value, "%i", CFG_getThumbnailRadius());
    }
    else if (strcmp(key, "showclock") == 0)
    {
        sprintf(value, "%i", CFG_getShowClock());
    }
    else if (strcmp(key, "clock24h") == 0)
    {
        sprintf(value, "%i", CFG_getClock24H());
    }
    else if (strcmp(key, "batteryperc") == 0)
    {
        sprintf(value, "%i", CFG_getShowBatteryPercent());
    }
    else if (strcmp(key, "menuanim") == 0)
    {
        sprintf(value, "%i", CFG_getMenuAnimations());
    }
    else if (strcmp(key, "menutransitions") == 0)
    {
        sprintf(value, "%i", CFG_getMenuTransitions());
    }
    else if (strcmp(key, "recents") == 0)
    {
        sprintf(value, "%i", CFG_getShowRecents());
    }
    else if (strcmp(key, "gameart") == 0)
    {
        sprintf(value, "%i", CFG_getShowGameArt());
    }
    else if (strcmp(key, "screentimeout") == 0)
    {
        sprintf(value, "%i", CFG_getScreenTimeoutSecs());
    }
    else if (strcmp(key, "suspendTimeout") == 0)
    {
        sprintf(value, "%i", CFG_getSuspendTimeoutSecs());
    }
    else if (strcmp(key, "switcherscale") == 0)
    {
        sprintf(value, "%i", CFG_getGameSwitcherScaling());
    }
    else if (strcmp(key, "romfolderbg") == 0)
    {
        sprintf(value, "%i", CFG_getRomsUseFolderBackground());
    }
    else if (strcmp(key, "saveFormat") == 0)
    {
        sprintf(value, "%i", CFG_getSaveFormat());
    }
    else if (strcmp(key, "stateFormat") == 0)
    {
        sprintf(value, "%i", CFG_getStateFormat());
    }
    else if (strcmp(key, "muteLeds") == 0)
    {
        sprintf(value, "%i", CFG_getMuteLEDs());
    }
    else if (strcmp(key, "artWidth") == 0)
    {
        sprintf(value, "%i", (int)(CFG_getGameArtWidth()) * 100);
    }
    else if (strcmp(key, "wifi") == 0)
    {
        sprintf(value, "%i", (int)(CFG_getWifi()));
    }
    else if (strcmp(key, "defaultView") == 0)
    {
        sprintf(value, "%i", (int)(CFG_getDefaultView()));
    }
    else if (strcmp(key, "quickSwitcherUi") == 0)
    {
        sprintf(value, "%i", (int)(CFG_getShowQuickswitcherUI()));
    }
    else if (strcmp(key, "wifiDiagnostics") == 0)
    {
        sprintf(value, "%i", (int)(CFG_getWifiDiagnostics()));
    }

    // meta, not a real setting
    else if (strcmp(key, "fontpath") == 0)
    {
        if (CFG_getFontId() == 1)
            sprintf(value, "\"%s\"", RES_PATH "/font1.ttf");
        else
            sprintf(value, "\"%s\"", RES_PATH "/font2.ttf");
    }

    else {
        sprintf(value, "");
    }
}

void CFG_sync(void)
{
    // write to file
    char settingsPath[MAX_PATH];
    sprintf(settingsPath, "%s/minuisettings.txt", getenv("SHARED_USERDATA_PATH"));
    FILE *file = fopen(settingsPath, "w");
    if (file == NULL)
    {
        printf("[CFG] Unable to open settings file, cant write\n");
        return;
    }

    fprintf(file, "font=%i\n", settings.font);
    fprintf(file, "color1=0x%06X\n", settings.color1_255);
    fprintf(file, "color2=0x%06X\n", settings.color2_255);
    fprintf(file, "color3=0x%06X\n", settings.color3_255);
    fprintf(file, "color4=0x%06X\n", settings.color4_255);
    fprintf(file, "color5=0x%06X\n", settings.color5_255);
    fprintf(file, "color6=0x%06X\n", settings.color6_255);
    fprintf(file, "color7=0x%06X\n", settings.color7_255);
    fprintf(file, "radius=%i\n", settings.thumbRadius);
    fprintf(file, "showclock=%i\n", settings.showClock);
    fprintf(file, "clock24h=%i\n", settings.clock24h);
    fprintf(file, "batteryperc=%i\n", settings.showBatteryPercent);
    fprintf(file, "menuanim=%i\n", settings.showMenuAnimations);
    fprintf(file, "menutransitions=%i\n", settings.showMenuTransitions);
    fprintf(file, "recents=%i\n", settings.showRecents);
    fprintf(file, "gameart=%i\n", settings.showGameArt);
    fprintf(file, "screentimeout=%i\n", settings.screenTimeoutSecs);
    fprintf(file, "suspendTimeout=%i\n", settings.suspendTimeoutSecs);
    fprintf(file, "switcherscale=%i\n", settings.gameSwitcherScaling);
    fprintf(file, "haptics=%i\n", settings.haptics);
    fprintf(file, "romfolderbg=%i\n", settings.romsUseFolderBackground);
    fprintf(file, "saveFormat=%i\n", settings.saveFormat);
    fprintf(file, "stateFormat=%i\n", settings.stateFormat);
    fprintf(file, "muteLeds=%i\n", settings.muteLeds);
    fprintf(file, "artWidth=%i\n", (int)(settings.gameArtWidth * 100));
    fprintf(file, "wifi=%i\n", settings.wifi);
    fprintf(file, "defaultView=%i\n", settings.defaultView);
    fprintf(file, "quickSwitcherUi=%i\n", settings.showQuickSwitcherUi);
    fprintf(file, "wifiDiagnostics=%i\n", settings.wifiDiagnostics);
    fprintf(file, "language=%s\n", settings.language);
    fclose(file);
}

void CFG_print(void)
{
    printf("{\n");
    printf("\t\"font\": %i,\n", settings.font);
    printf("\t\"color1\": \"0x%06X\",\n", settings.color1_255);
    printf("\t\"color2\": \"0x%06X\",\n", settings.color2_255);
    printf("\t\"color3\": \"0x%06X\",\n", settings.color3_255);
    printf("\t\"color4\": \"0x%06X\",\n", settings.color4_255);
    printf("\t\"color5\": \"0x%06X\",\n", settings.color5_255);
    printf("\t\"color6\": \"0x%06X\",\n", settings.color6_255);
    printf("\t\"color7\": \"0x%06X\",\n", settings.color7_255);
    printf("\t\"radius\": %i,\n", settings.thumbRadius);
    printf("\t\"showclock\": %i,\n", settings.showClock);
    printf("\t\"clock24h\": %i,\n", settings.clock24h);
    printf("\t\"batteryperc\": %i,\n", settings.showBatteryPercent);
    printf("\t\"menuanim\": %i,\n", settings.showMenuAnimations);
    printf("\t\"menutransitions\": %i,\n", settings.showMenuTransitions);
    printf("\t\"recents\": %i,\n", settings.showRecents);
    printf("\t\"gameart\": %i,\n", settings.showGameArt);
    printf("\t\"screentimeout\": %i,\n", settings.screenTimeoutSecs);
    printf("\t\"suspendTimeout\": %i,\n", settings.suspendTimeoutSecs);
    printf("\t\"switcherscale\": %i,\n", settings.gameSwitcherScaling);
    printf("\t\"haptics\": %i,\n", settings.haptics);
    printf("\t\"romfolderbg\": %i,\n", settings.romsUseFolderBackground);
    printf("\t\"saveFormat\": %i,\n", settings.saveFormat);
    printf("\t\"stateFormat\": %i,\n", settings.stateFormat);
    printf("\t\"muteLeds\": %i,\n", settings.muteLeds);
    printf("\t\"artWidth\": %i,\n", (int)(settings.gameArtWidth * 100));
    printf("\t\"wifi\": %i,\n", settings.wifi);
    printf("\t\"defaultView\": %i,\n", settings.defaultView);
    printf("\t\"quickSwitcherUi\": %i,\n", settings.showQuickSwitcherUi);
    printf("\t\"wifiDiagnostics\": %i,\n", settings.wifiDiagnostics);

    // meta, not a real setting
    if (settings.font == 1)
        printf("\t\"fontpath\": \"%s\"\n", RES_PATH "/font1.ttf");
    else
        printf("\t\"fontpath\": \"%s\"\n", RES_PATH "/font2.ttf");

    printf("}\n");
}

void CFG_quit(void)
{
    CFG_sync();
}