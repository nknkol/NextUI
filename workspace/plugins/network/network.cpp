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
#include "defines.h"
}

// C++ 菜单系统和WiFi菜单
#include "wifimenu.hpp"

// --- 插件状态变量 ---

static SDL_Surface* screen;
static int quit_plugin;
static Wifi::Menu* networkMenu;

// --- 插件生命周期函数 ---

static int plugin_init(void* main_screen) {
    screen = (SDL_Surface*)main_screen;
    quit_plugin = 0;

    // 初始化 SysUI
    SysUI_Init(screen, &font);
    SysUI_SetTitle(L("Network"));
    SysUI_SetBottomHints("B", "BACK", "A", "OPTIONS", NULL, NULL);
    SysUI_SetFullscreen(false); // 我们需要SysUI绘制的顶栏和底栏

    // 创建WiFi菜单实例
    // Wifi::Menu 的构造函数需要一个引用来监控退出信号，以便能安全地停止其内部的工作线程
    // 我们将插件自己的 quit_plugin 变量传递给它
    networkMenu = new Wifi::Menu(quit_plugin);

    // 计算菜单的绘制区域，为SysUI的顶栏和底栏留出空间
    SDL_Rect listRect = {
        SCALE1(PADDING), 
        SCALE1(PADDING + PILL_SIZE), // Y 坐标向下移动一个顶栏的高度
        screen->w - SCALE1(PADDING * 2), 
        screen->h - SCALE1(PADDING * 2 + PILL_SIZE * 2) // 高度减去顶栏和底栏的高度
    };
    networkMenu->performLayout(listRect);

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
            // MenuList::handleInput 能够处理BTN_B并设置quit_plugin=1
            networkMenu->handleInput(dirty, quit_plugin);

            // B键备用退出机制
            if (PAD_justPressed(BTN_B)) {
                quit_plugin = 1;
            }
        }

        if (dirty) {
            GFX_clear(screen);
            
            // 绘制菜单
            networkMenu->draw(screen, listRect);
            
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
    // 确保在删除 networkMenu 之前，它的工作线程已经收到退出信号并可以正常退出
    quit_plugin = 1;

    delete networkMenu;
    networkMenu = nullptr;

    // 保存所有可能更改的设置
    CFG_sync();

    SysUI_Quit();
}

// --- 插件导出 ---

extern "C" {
    static NextUI_Plugin network_plugin = {
        .name = "Network",
        .display_path = SDCARD_PATH "/Tools/Settings", // 与Appearance插件放在同一目录下
        .init = plugin_init,
        .run = plugin_run,
        .quit = plugin_quit,
    };

    NextUI_Plugin* GetPlugin(void) {
        return &network_plugin;
    }
}