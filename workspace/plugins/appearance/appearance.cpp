#include <vector>
#include <string>
#include <any>

// 核心库头文件
extern "C" {
#include "api.h"
#include "config.h"
#include "lang.h"
#include "sysui.h"
#include "plugin.h"
#include "defines.h" // 引入这个头文件以使用 SDCARD_PATH
}

// C++ 菜单系统
#include "menu.hpp"

// --- 从 settings.cpp 迁移过来的数据 ---

static const std::vector<std::any> colors = {
    0x000022U, 0x000044U, 0x000066U, 0x000088U, 0x0000AAU, 0x0000CCU, 0x1e2329U, 0x3366FFU, 0x4D7AFFU, 0x6699FFU, 0x80B3FFU, 0x99CCFFU, 0xB3D9FFU,
    0x002222U, 0x004444U, 0x006666U, 0x008888U, 0x00AAAAU, 0x00CCCCU, 0x33FFFFU, 0x4DFFFFU, 0x66FFFFU, 0x80FFFFU, 0x99FFFFU, 0xB3FFFFU,
    0x002200U, 0x004400U, 0x006600U, 0x008800U, 0x00AA00U, 0x00CC00U, 0x33FF33U, 0x4DFF4DU, 0x66FF66U, 0x80FF80U, 0x99FF99U, 0xB3FFB3U,
    0x220022U, 0x440044U, 0x660066U, 0x880088U, 0x9B2257U, 0xAA00AAU, 0xCC00CCU, 0xFF33FFU, 0xFF4DFFU, 0xFF66FFU, 0xFF80FFU, 0xFF99FFU, 0xFFB3FFU,
    0x110022U, 0x220044U, 0x330066U, 0x440088U, 0x5500AAU, 0x6600CCU, 0x8833FFU, 0x994DFFU, 0xAA66FFU, 0xBB80FFU, 0xCC99FFU, 0xDDB3FFU,
    0x220000U, 0x440000U, 0x660000U, 0x880000U, 0xAA0000U, 0xCC0000U, 0xFF3333U, 0xFF4D4DU, 0xFF6666U, 0xFF8080U, 0xFF9999U, 0xFFB3B3U,
    0x222200U, 0x444400U, 0x666600U, 0x888800U, 0xAAAA00U, 0xCCCC00U, 0xFFFF33U, 0xFFFF4DU, 0xFFFF66U, 0xFFFF80U, 0xFFFF99U, 0xFFFFB3U,
    0x221100U, 0x442200U, 0x663300U, 0x884400U, 0xAA5500U, 0xCC6600U, 0xFF8833U, 0xFF994DU, 0xFFAA66U, 0xFFBB80U, 0xFFCC99U, 0xFFDDB3U,
    0x000000U, 0x141414U, 0x282828U, 0x3C3C3CU, 0x505050U, 0x646464U, 0x8C8C8CU, 0xA0A0A0U, 0xB4B4B4U, 0xC8C8C8U, 0xDCDCDCU, 0xFFFFFFU
};

static const std::vector<std::string> color_strings = {
    "0x000022", "0x000044", "0x000066", "0x000088", "0x0000AA", "0x0000CC", "0x1E2329", "0x3366FF", "0x4D7AFF", "0x6699FF", "0x80B3FF", "0x99CCFF", "0xB3D9FF",
    "0x002222", "0x004444", "0x006666", "0x008888", "0x00AAAA", "0x00CCCC", "0x33FFFF", "0x4DFFFF", "0x66FFFF", "0x80FFFF", "0x99FFFF", "0xB3FFFF",
    "0x002200", "0x004400", "0x006600", "0x008800", "0x00AA00", "0x00CC00", "0x33FF33", "0x4DFF4D", "0x66FF66", "0x80FF80", "0x99FF99", "0xB3FFB3",
    "0x220022", "0x440044", "0x660066", "0x880088", "0x9B2257", "0xAA00AA", "0xCC00CC", "0xFF33FF", "0xFF4DFF", "0xFF66FF", "0xFF80FF", "0xFF99FF", "0xFFB3FF",
    "0x110022", "0x220044", "0x330066", "0x440088", "0x5500AA", "0x6600CC", "0x8833FF", "0x994DFF", "0xAA66FF", "0xBB80FF", "0xCC99FF", "0xDDB3FF",
    "0x220000", "0x440000", "0x660000", "0x880000", "0xAA0000", "0xCC0000", "0xFF3333", "0xFF4D4D", "0xFF6666", "0xFF8080", "0xFF9999", "0xFFB3B3",
    "0x222200", "0x444400", "0x66600", "0x888800", "0xAAAA00", "0xCCCC00", "0xFFFF33", "0xFFFF4D", "0xFFFF66", "0xFFFF80", "0xFFFF99", "0xFFFFB3",
    "0x221100", "0x442200", "0x663300", "0x884400", "0xAA5500", "0xCC6600", "0xFF8833", "0xFF994D", "0xFFAA66", "0xFFBB80", "0xFFCC99", "0xFFDDB3",
    "0x000000", "0x141414", "0x282828", "0x3C3C3C", "0x505050", "0x646464", "0x8C8C8C", "0xA0A0A0", "0xB4B4B4", "0xC8C8C8", "0xDCDCDC", "0xFFFFFF"
};

static const std::vector<std::string> font_names = {"OG", "Next", "NotoSans"};
static const std::vector<std::string> on_off = {"Off", "On"};

// --- 插件状态变量 ---

static SDL_Surface* screen;
static int quit_plugin;
static MenuList* appearanceMenu;

// --- 插件生命周期函数 ---

static int plugin_init(void* main_screen) {
    screen = (SDL_Surface*)main_screen;
    quit_plugin = 0;

    // 初始化 SysUI
    SysUI_Init(screen, &font);
    SysUI_SetTitle(L("Appearance"));
    SysUI_SetBottomHints("B", "BACK", "A", "OKAY", "Y", "RESET");
    SysUI_SetFullscreen(false); // 我们需要SysUI绘制的顶栏和底栏

    // 创建外观菜单 (逻辑从 settings.cpp 复制)
    appearanceMenu = new MenuList(MenuItemType::Fixed, "Appearance",
    {
        new MenuItem{ListItemType::Generic, L("Font"), L("The font to render all UI text."), {0, 1, 2}, font_names, 
            []() -> std::any{ return CFG_getFontId(); },
            [](const std::any &value){ CFG_setFontId(std::any_cast<int>(value)); },
            []() { CFG_setFontId(CFG_DEFAULT_FONT_ID);}},
        new MenuItem{ListItemType::Color, L("Main Color"), L("The color used to render main UI elements."), colors, color_strings, 
            []() -> std::any{ return CFG_getColor(1); }, 
            [](const std::any &value){ CFG_setColor(1, std::any_cast<uint32_t>(value)); },
            []() { CFG_setColor(1, CFG_DEFAULT_COLOR1);}},
        new MenuItem{ListItemType::Color, L("Primary Accent Color"), L("The color used to highlight important things in the user interface."), colors, color_strings, 
            []() -> std::any{ return CFG_getColor(2); }, 
            [](const std::any &value){ CFG_setColor(2, std::any_cast<uint32_t>(value)); },
            []() { CFG_setColor(2, CFG_DEFAULT_COLOR2);}},
        new MenuItem{ListItemType::Color, L("Secondary Accent Color"), L("A secondary highlight color."), colors, color_strings, 
            []() -> std::any{ return CFG_getColor(3); }, 
            [](const std::any &value){ CFG_setColor(3, std::any_cast<uint32_t>(value)); },
            []() { CFG_setColor(3, CFG_DEFAULT_COLOR3);}},
        new MenuItem{ListItemType::Color, L("Hint info Color"), L("Color for button hints and info"), colors, color_strings, 
            []() -> std::any{ return CFG_getColor(6); }, 
            [](const std::any &value){ CFG_setColor(6, std::any_cast<uint32_t>(value)); },
            []() { CFG_setColor(6, CFG_DEFAULT_COLOR6);}},
        new MenuItem{ListItemType::Color, L("List Text"), L("List text color"), colors, color_strings, 
            []() -> std::any{ return CFG_getColor(4); }, 
            [](const std::any &value){ CFG_setColor(4, std::any_cast<uint32_t>(value)); },
            []() { CFG_setColor(4, CFG_DEFAULT_COLOR4);}},
        new MenuItem{ListItemType::Color, L("List Text Selected"), L("List selected text color"), colors, color_strings, 
            []() -> std::any { return CFG_getColor(5); }, 
            [](const std::any &value) { CFG_setColor(5, std::any_cast<uint32_t>(value)); },
            []() { CFG_setColor(5, CFG_DEFAULT_COLOR5);}},
        new MenuItem{ListItemType::Generic, L("Show battery percentage"), L("Show battery level as percent in the status pill"), {false, true}, on_off, 
            []() -> std::any { return CFG_getShowBatteryPercent(); },
            [](const std::any &value) { CFG_setShowBatteryPercent(std::any_cast<bool>(value)); },
            []() { CFG_setShowBatteryPercent(CFG_DEFAULT_SHOWBATTERYPERCENT);}},
        new MenuItem{ListItemType::Generic, L("Show menu animations"), L("Enable or disable menu animations"), {false, true}, on_off, 
            []() -> std::any{ return CFG_getMenuAnimations(); },
            [](const std::any &value) { CFG_setMenuAnimations(std::any_cast<bool>(value)); },
            []() { CFG_setMenuAnimations(CFG_DEFAULT_SHOWMENUANIMATIONS);}},
        new MenuItem{ListItemType::Generic, L("Show menu transitions"), L("Enable or disable animated transitions"), {false, true}, on_off, 
            []() -> std::any{ return CFG_getMenuTransitions(); },
            [](const std::any &value) { CFG_setMenuTransitions(std::any_cast<bool>(value)); },
            []() { CFG_setMenuTransitions(CFG_DEFAULT_SHOWMENUTRANSITIONS);}},
        new MenuItem{ListItemType::Generic, L("Game art corner radius"), L("Set the radius for the rounded corners of game art"), 0, 24, "px",
            []() -> std::any{ return CFG_getThumbnailRadius(); }, 
            [](const std::any &value) { CFG_setThumbnailRadius(std::any_cast<int>(value)); },
            []() { CFG_setThumbnailRadius(CFG_DEFAULT_THUMBRADIUS);}},
        new MenuItem{ListItemType::Generic, L("Game art width"), L("Set the percentage of screen width used for game art.\nUI elements might overrule this to avoid clipping."), 
            5, 100, "%",
            []() -> std::any{ return (int)(CFG_getGameArtWidth() * 100); }, 
            [](const std::any &value) { CFG_setGameArtWidth((double)std::any_cast<int>(value) / 100.0); },
            []() { CFG_setGameArtWidth(CFG_DEFAULT_GAMEARTWIDTH);}},
        new MenuItem{ListItemType::Generic, L("Show recents"), L("Show \"Recently Played\" menu entry.\nThis also disables Game Switcher."), {false, true}, on_off, 
            []() -> std::any { return CFG_getShowRecents(); },
            [](const std::any &value) { CFG_setShowRecents(std::any_cast<bool>(value)); },
            []() { CFG_setShowRecents(CFG_DEFAULT_SHOWRECENTS);}},
        new MenuItem{ListItemType::Generic, L("Show game art"), L("Show game artwork in the main menu"), {false, true}, on_off, []() -> std::any
            { return CFG_getShowGameArt(); },
            [](const std::any &value) { CFG_setShowGameArt(std::any_cast<bool>(value)); },
            []() { CFG_setShowGameArt(CFG_DEFAULT_SHOWGAMEART);}},
        new MenuItem{ListItemType::Generic, L("Use folder background for ROMs"), L("If enabled, used the emulator background image. Otherwise uses the default."), {false, true}, on_off, []() -> std::any
            { return CFG_getRomsUseFolderBackground(); },
            [](const std::any &value) { CFG_setRomsUseFolderBackground(std::any_cast<bool>(value)); },
            []() { CFG_setRomsUseFolderBackground(CFG_DEFAULT_ROMSUSEFOLDERBACKGROUND);}},
        new MenuItem{ListItemType::Generic, L("Show Quickswitcher UI"), L("Show/hide Quickswitcher UI elements.\nWhen hidden, will only draw background images."), {false, true}, on_off, 
            []() -> std::any{ return CFG_getShowQuickswitcherUI(); },
            [](const std::any &value){ CFG_setShowQuickswitcherUI(std::any_cast<bool>(value)); },
            []() { CFG_setShowQuickswitcherUI(CFG_DEFAULT_SHOWQUICKWITCHERUI);}},
        new MenuItem{ListItemType::Button, L("Reset to defaults"), L("Resets all options in this menu to their default values."), ResetCurrentMenu},
    });

    // 计算菜单的绘制区域，为SysUI的顶栏和底栏留出空间
    SDL_Rect listRect = {
        SCALE1(PADDING), 
        SCALE1(PADDING + PILL_SIZE), // Y 坐标向下移动一个顶栏的高度
        screen->w - SCALE1(PADDING * 2), 
        screen->h - SCALE1(PADDING * 2 + PILL_SIZE * 2) // 高度减去顶栏和底栏的高度
    };
    appearanceMenu->performLayout(listRect);

    return 0;
}

static int plugin_run() {
    int dirty = 1;
    bool input_handled_by_sysui = false;

    // 计算菜单的绘制区域
    SDL_Rect listRect = {
        SCALE1(PADDING), 
        SCALE1(PADDING + PILL_SIZE), 
        screen->w - SCALE1(PADDING * 2), 
        screen->h - SCALE1(PADDING * 2 + PILL_SIZE * 2)
    };

    while(!quit_plugin) {
        PAD_poll();
        
        // SysUI_Update 会处理硬件浮层（亮度、音量等）的输入和显示逻辑
        // 它会返回true，如果它处理了输入（例如调整亮度），我们可以跳过本轮的菜单输入处理
        input_handled_by_sysui = SysUI_Update();
        if (input_handled_by_sysui) {
            dirty = 1;
        }

        if (!input_handled_by_sysui) {
            // 将输入传递给菜单处理
            appearanceMenu->handleInput(dirty, quit_plugin);

            // B键退出
            if (PAD_justPressed(BTN_B)) {
                quit_plugin = 1;
            }

            // Y键重置
            if (PAD_justPressed(BTN_Y)) {
                appearanceMenu->resetAllItems();
                dirty = 1;
            }
        }

        if (dirty) {
            GFX_clear(screen);
            
            // 绘制菜单
            appearanceMenu->draw(screen, listRect);
            
            // 绘制SysUI（顶栏和底栏）
            SysUI_Render();

            GFX_flip(screen);
            dirty = 0;
        } else {
            GFX_sync();
        }
    }

    return 0;
}

static void plugin_quit(void) {
    delete appearanceMenu;
    appearanceMenu = nullptr;

    // 保存所有更改
    CFG_sync();

    SysUI_Quit();
}

// --- 插件导出 ---

extern "C" {
    static NextUI_Plugin appearance_plugin = {
        .name = "Appearance",
        .display_path = SDCARD_PATH "/Tools/Settings",
        .init = plugin_init,
        .run = plugin_run,
        .quit = plugin_quit,
    };

    NextUI_Plugin* GetPlugin(void) {
        return &appearance_plugin;
    }
}