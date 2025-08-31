#pragma once

#include "menu.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic> // 1. 包含了 <atomic> 头文件

namespace Wifi
{
    class Menu : public MenuList
    {
        const int &globalQuit;
        // wifi on/off
        MenuItem *toggleItem;
        // diagnostics on/off
        MenuItem *diagItem;

        std::thread worker;
        std::atomic<bool> quit{false}; // 2. 将类型从 bool 改为 std::atomic<bool>
        std::atomic<bool> workerDirty{false}; // 3. 将类型从 bool 改为 std::atomic<bool>
        
        // 用于可中断的睡眠和安全退出
        std::mutex quitMutex;
        std::condition_variable quitCondition;
        
        // 保持用户选择的状态
        std::string lastSelectedItemName;
        int lastSelectedIndex = 0;

    public:
        Menu(const int &globalQuit);
        ~Menu();

        InputReactionHint handleInput(int &dirty, int &quit) override;

    private:
        std::any getWifToggleState() const;
        void setWifiToggleState(const std::any &on);
        void resetWifiToggleState();

        std::any getWifDiagnosticsState() const;
        void setWifiDiagnosticsState(const std::any &on);
        void resetWifiDiagnosticsState();

        void updater();
        
        // 保持和恢复用户选择的辅助函数
        void preserveUserSelection();
        void restoreUserSelection();
    };

    class NetworkItem : public MenuItem
    {
        WIFI_network net;
        bool connected;

    public:
        NetworkItem(WIFI_network n, bool connected, MenuList *submenu);

        void drawCustomItem(SDL_Surface *surface, const SDL_Rect &dst, const AbstractMenuItem &item, bool selected) const override;
    };

    class ConnectKnownItem : public MenuItem
    {
        WIFI_network net;

    public:
        // 4. 将构造函数参数从 bool& 改为 std::atomic<bool>&
        ConnectKnownItem(WIFI_network n, std::atomic<bool>& dirty);
    };

    class ConnectNewItem : public MenuItem
    {
        WIFI_network net;

    public:
        // 5. 将构造函数参数从 bool& 改为 std::atomic<bool>&
        ConnectNewItem(WIFI_network n, std::atomic<bool>& dirty);
    };

    class ForgetItem : public MenuItem
    {
        WIFI_network net;

    public:
        // 6. 将构造函数参数从 bool& 改为 std::atomic<bool>&
        ForgetItem(WIFI_network n, std::atomic<bool>& dirty);
    };
}