#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>
#include <stdbool.h>

// portability, deprecated
extern uint32_t THEME_COLOR1_255;
extern uint32_t THEME_COLOR2_255;
extern uint32_t THEME_COLOR3_255;
extern uint32_t THEME_COLOR4_255;
extern uint32_t THEME_COLOR5_255;
extern uint32_t THEME_COLOR6_255;
extern uint32_t THEME_COLOR7_255;

// Read-only interface for minui.c usage
// Read/Write interface for settings.cpp usage

typedef int (*FontLoad_callback_t)(const char* path);
typedef int (*ColorSet_callback_t)(void);

enum
{
	SAVE_FORMAT_SAV,
	SAVE_FORMAT_SRM,
	SAVE_FORMAT_GEN,
	SAVE_FORMAT_SRM_UNCOMPRESSED
};

enum
{
	STATE_FORMAT_SAV,
	STATE_FORMAT_SRM,
	STATE_FORMAT_SRM_UNCOMRESSED
};

enum {
	// actual views
	SCREEN_GAMELIST,
	SCREEN_GAMESWITCHER,
	SCREEN_QUICKMENU,
	// meta
	SCREEN_GAME,
	SCREEN_OFF
};

typedef struct
{
	// Theme
	int font;
	uint32_t color1_255; // not screen mapped
	uint32_t color2_255; // not screen mapped
	uint32_t color3_255; // not screen mapped
	uint32_t color4_255; // not screen mapped
	uint32_t color5_255; // not screen mapped
	uint32_t color6_255; // not screen mapped
	uint32_t color7_255; // not screen mapped
	int thumbRadius;
	int gameSwitcherScaling; // enum
	double gameArtWidth;	 // [0,1] -> 0-100% of screen width

	// font loading/unloading callback
    FontLoad_callback_t onFontChange;

    // color update callback
    ColorSet_callback_t onColorSet;

    // UI
	bool showClock;
	bool clock24h;
	bool showBatteryPercent;
	bool showMenuAnimations;
	bool showMenuTransitions;
	bool showRecents;
	bool showGameArt;
	bool romsUseFolderBackground;
	bool showQuickSwitcherUi;
	int defaultView;

	// Mute switch
	bool muteLeds;

	// Power
	uint32_t screenTimeoutSecs;
	uint32_t suspendTimeoutSecs;

	// Emulator
	int saveFormat;
	int stateFormat;

	// Haptic
	bool haptics;

	// Network
	bool wifi;
	bool wifiDiagnostics;
	// ADDED: Language setting
	char language[8];
} NextUISettings;

#define CFG_DEFAULT_FONT_ID 1  // Next
#define CFG_DEFAULT_COLOR1 0xffffffU
#define CFG_DEFAULT_COLOR2 0x9b2257U
#define CFG_DEFAULT_COLOR3 0x1e2329U
#define CFG_DEFAULT_COLOR4 0xffffffU
#define CFG_DEFAULT_COLOR5 0x000000U
#define CFG_DEFAULT_COLOR6 0xffffffU
#define CFG_DEFAULT_COLOR7 0x000000U
#define CFG_DEFAULT_THUMBRADIUS 20 // unscaled!
#define CFG_DEFAULT_SHOWCLOCK false
#define CFG_DEFAULT_CLOCK24H true
#define CFG_DEFAULT_SHOWBATTERYPERCENT false
#define CFG_DEFAULT_SHOWMENUANIMATIONS true
#define CFG_DEFAULT_SHOWMENUTRANSITIONS true
#define CFG_DEFAULT_SHOWRECENTS true
#define CFG_DEFAULT_SHOWGAMEART true
#define CFG_DEFAULT_GAMESWITCHERSCALING GFX_SCALE_FULLSCREEN
#define CFG_DEFAULT_SCREENTIMEOUTSECS 60
#define CFG_DEFAULT_SUSPENDTIMEOUTSECS 30
#define CFG_DEFAULT_HAPTICS false
#define CFG_DEFAULT_ROMSUSEFOLDERBACKGROUND true
#define CFG_DEFAULT_SAVEFORMAT SAVE_FORMAT_SAV
#define CFG_DEFAULT_STATEFORMAT STATE_FORMAT_SAV
#define CFG_DEFAULT_MUTELEDS false
#define CFG_DEFAULT_GAMEARTWIDTH 0.45
#define CFG_DEFAULT_WIFI false
#define CFG_DEFAULT_VIEW SCREEN_GAMELIST
#define CFG_DEFAULT_SHOWQUICKWITCHERUI true
#define CFG_DEFAULT_WIFI_DIAG false
// ADDED: Default language
#define CFG_DEFAULT_LANGUAGE "en"
void CFG_init(FontLoad_callback_t fontCallback, ColorSet_callback_t ccb);
void CFG_print(void);
void CFG_get(const char *key, char * value);
// void CFG_defaults(NextUISettings*);
//  The font id to use as the UI font.
//  0 - Default MinUI font
//  1 - Default NextUI font (default)
int CFG_getFontId(void);
void CFG_setFontId(int fontid);
// The colors to use for the UI. These are 0xRRGGBB values.
// 0 - Color1 (primary hint/asset colour)
// 1 - Color2 (accent colour)
// 2 - Color3 (secondary accent colour
// 3 - Background Color (unused)
uint32_t CFG_getColor(int id);
void CFG_setColor(int id, uint32_t color);
// Time in secs before the device enters screen-off mode.
uint32_t CFG_getScreenTimeoutSecs(void);
void CFG_setScreenTimeoutSecs(uint32_t secs);
// Time in secs before the device enters suspend mode (aka deep sleep).
uint32_t CFG_getSuspendTimeoutSecs(void);
void CFG_setSuspendTimeoutSecs(uint32_t secs);
// Show/hide clock in the status pill.
bool CFG_getShowClock(void);
void CFG_setShowClock(bool show);
// Sets the time format to 12/24hrs.
bool CFG_getClock24H(void);
void CFG_setClock24H(bool);
// Show/hide battery percentage in the status pill.
bool CFG_getShowBatteryPercent(void);
void CFG_setShowBatteryPercent(bool show);
// Show/hide menu animations in main menu.
bool CFG_getMenuAnimations(void);
void CFG_setMenuAnimations(bool show);
// Show/hide menu transitions between screens in main menu.
bool CFG_getMenuTransitions(void);
void CFG_setMenuTransitions(bool show);
// Set thumbnail rounding radius.
int CFG_getThumbnailRadius(void);
void CFG_setThumbnailRadius(int radius);
// Show/hide recently played in the main menu.
bool CFG_getShowRecents(void);
void CFG_setShowRecents(bool show);
// Show/hide game art in the main menu.
bool CFG_getShowGameArt(void);
void CFG_setShowGameArt(bool show);
// Use folder background or default background for roms
bool CFG_getRomsUseFolderBackground(void);
void CFG_setRomsUseFolderBackground(bool);
// The scaling algorithm used for the game switcher preview image.
int CFG_getGameSwitcherScaling(void);
void CFG_setGameSwitcherScaling(int enumValue);
// Enable/disable haptics.
bool CFG_getHaptics(void);
void CFG_setHaptics(bool enable);
// Save format to use for libretro cores
// 0 - .sav
// 1 - .srm (compressed rzip)
int CFG_getSaveFormat(void);
void CFG_setSaveFormat(int);
// Save state format to use for libretro cores
// 0 - .st0
// 1 - .state.0 (compressed rzip)
int CFG_getStateFormat(void);
void CFG_setStateFormat(int);
// Enable/disable mute also shutting off LEDs.
bool CFG_getMuteLEDs(void);
void CFG_setMuteLEDs(bool);
// Set game art width percentage.
double CFG_getGameArtWidth(void);
void CFG_setGameArtWidth(double zeroToOne);
// WiFi on/off (if available)
bool CFG_getWifi(void);
void CFG_setWifi(bool on);
// Default view on boot
int CFG_getDefaultView(void);
void CFG_setDefaultView(int view);
// Quick switcher UI painting on/off
bool CFG_getShowQuickswitcherUI(void);
void CFG_setShowQuickswitcherUI(bool on);
// WiFi diagnostic logging on/off
bool CFG_getWifiDiagnostics(void);
void CFG_setWifiDiagnostics(bool on);

void CFG_sync(void);
void CFG_quit(void);

// ADDED: Function prototypes for language setting
const char* CFG_getLanguage(void);
void CFG_setLanguage(const char* lang);

void CFG_sync(void);
void CFG_quit(void);

#endif
