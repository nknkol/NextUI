#include "wifimenu.hpp"
#include "keyboardprompt.hpp"

#include <unordered_set>
#include <map>

#include <mutex>
#include <shared_mutex>
#include <condition_variable>
typedef std::shared_mutex Lock;
typedef std::unique_lock<Lock> WriteLock;
typedef std::shared_lock<Lock> ReadLock;

using namespace Wifi;
using namespace std::placeholders;

Menu::Menu(const int &globalQuit) : MenuList(MenuItemType::Fixed, "Network", {}), globalQuit(globalQuit)
{
    toggleItem = new MenuItem(ListItemType::Generic, "WiFi", "Enable/disable WiFi", {false, true}, {"Off", "On"},
                              std::bind(&Menu::getWifToggleState, this),
                              std::bind(&Menu::setWifiToggleState, this, std::placeholders::_1),
                              std::bind(&Menu::resetWifiToggleState, this));
    diagItem = new MenuItem(ListItemType::Generic, "WiFi diagnostics", "Enable/disable WiFi logging", {false, true}, {"Off", "On"},
                              std::bind(&Menu::getWifDiagnosticsState, this),
                              std::bind(&Menu::setWifiDiagnosticsState, this, std::placeholders::_1),
                              std::bind(&Menu::resetWifiDiagnosticsState, this));
    items.push_back(toggleItem);
    items.push_back(diagItem);

    // best effort layout based on the platform defines, user should really call performLayout manually
    MenuList::performLayout((SDL_Rect){0, 0, FIXED_WIDTH, FIXED_HEIGHT});
    layout_called = false;

    worker = std::thread{&Menu::updater, this};
}

Menu::~Menu()
{
    {
        std::unique_lock<std::mutex> lock(quitMutex);
        quit = true;
    }
    quitCondition.notify_all(); // 唤醒睡眠中的线程
    
    if (worker.joinable())
        worker.join();
}

InputReactionHint Menu::handleInput(int &dirty, int &quit)
{
    auto ret = MenuList::handleInput(dirty, quit);
    if (workerDirty)
    {
        dirty = true;
        workerDirty = false; // handled
        //LOG_info("collected workerDirty\n");
    }
    return ret;
}

std::any Menu::getWifToggleState() const
{
    return WIFI_enabled();
}

void Menu::setWifiToggleState(const std::any &on)
{
    // 使用异步方式避免主线程阻塞
    std::thread([on]() {
        WIFI_enable(std::any_cast<bool>(on));
    }).detach();
}

void Menu::resetWifiToggleState()
{
    //
}

std::any Menu::getWifDiagnosticsState() const
{
    return WIFI_diagnosticsEnabled();
}

void Menu::setWifiDiagnosticsState(const std::any &on)
{
    WIFI_diagnosticsEnable(std::any_cast<bool>(on));
}

void Menu::resetWifiDiagnosticsState()
{
    //
}

// 保持用户选择的辅助函数
void Menu::preserveUserSelection()
{
    if (scope.selected >= 0 && scope.selected < items.size() && items[scope.selected]) {
        lastSelectedItemName = items[scope.selected]->getName();
    }
}

void Menu::restoreUserSelection()
{
    if (!lastSelectedItemName.empty()) {
        for (int i = 0; i < items.size(); i++) {
            if (items[i] && items[i]->getName() == lastSelectedItemName) {
                scope.selected = i;
                // 调整显示范围
                if (scope.selected < scope.start) {
                    scope.start = scope.selected;
                    scope.end = std::min(scope.start + scope.max_visible_options, scope.count);
                } else if (scope.selected >= scope.end) {
                    scope.end = scope.selected + 1;
                    scope.start = std::max(0, scope.end - scope.max_visible_options);
                }
                break;
            }
        }
    }
}

template <typename Map>
bool key_compare(Map const &lhs, Map const &rhs)
{
    return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin(),
                                                  [](auto a, auto b)
                                                  { return a.first == b.first; });
}

void Menu::updater()
{
    int pollSecs = 15;
    std::map<std::string, WIFI_network> prevScan;
    std::string prevSsid;
    
    while (!quit && !globalQuit)
    {

        // TODO: pause when menu is not rendered
        // Scan
        if (WIFI_enabled())
        {
            // scan for available networks and add a menu item for each
            WIFI_connection connection;
            if(WIFI_connectionInfo(&connection) < 0)
                continue; // try again in a bit

            // grab list and compare it to previous result
            // only relayout the menu if changes happended
            std::vector<WIFI_network> scanResults(SCAN_MAX_RESULTS);
            int cnt = WIFI_scan(scanResults.data(), SCAN_MAX_RESULTS);
            if(cnt < 0)
                continue; // try again in a bit

            std::map<std::string, WIFI_network> scanSsids;
            for (int i = 0; i < cnt; i++)
                scanSsids.emplace(scanResults[i].ssid, scanResults[i]);

            // dont repopulate if any submenu is open
            bool menuOpen = false;
            {
                ReadLock r(itemLock);
                for(auto i : items){
                    if(i && i->isDeferred()){
                        menuOpen = true;
                        break;
                    }
                }
            }

            // something changed?
            if (!menuOpen &&
                (prevSsid != std::string(connection.ssid) 
                || !key_compare(prevScan, scanSsids)))
            {
                prevScan = scanSsids;
                prevSsid = connection.ssid;

                {
                    WriteLock w(itemLock);
                    
                    // 保存当前选择
                    preserveUserSelection();
                    
                    // 安全地清理旧items
                    std::vector<AbstractMenuItem*> oldItems;
                    for (auto item : items) {
                        if (item != toggleItem && item != diagItem) {
                            oldItems.push_back(item);
                        }
                    }
                    
                    items.clear();
                    items.push_back(toggleItem);
                    items.push_back(diagItem);
                    
                    // 在锁外删除旧items以避免长时间持锁
                    w.unlock();
                    for (auto item : oldItems) {
                        delete item;
                    }
                    w.lock();

                    scope.count = 2; // 先设置基础项目数量
                    layout_called = false;

                    for (auto &[s, r] : scanSsids)
                    {
                        bool connected = false;
                        bool hasCredentials = WIFI_isKnown(r.ssid, r.security);

                        if (strcmp(connection.ssid, r.ssid) == 0)
                            connected = true;

                        MenuList *options;
                        if (connected)
                            options = new MenuList(MenuItemType::List, "Options",
                                                   {
                                                       new MenuItem{ListItemType::Button, "Disconnect", "Disconnect from this network.",
                                                                    [&](AbstractMenuItem &item) -> InputReactionHint
                                                                    { WIFI_disconnect(); workerDirty = true; return Exit; }},
                                                       new ForgetItem(r, workerDirty)
                                                   });
                        else 
                        if (hasCredentials)
                            options = new MenuList(MenuItemType::List, "Options", { new ConnectKnownItem(r, workerDirty), new ForgetItem(r, workerDirty) });
                        else
                            options = new MenuList(MenuItemType::List, "Options", { new ConnectNewItem(r, workerDirty) });

                        auto itm = new NetworkItem{r, connected, options};
                        if(connected && !std::string(connection.ip).empty())
                            itm->setDesc(std::string(r.bssid) + " | " + std::string(connection.ip));
                        items.push_back(itm);
                    }
                    
                    scope.count = items.size();
                    // 确保selected在有效范围内
                    if (scope.selected >= scope.count) {
                        scope.selected = std::max(0, scope.count - 1);
                    }
                    
                    workerDirty = true;
                }
            }
            pollSecs = 2;
        }
        else
        {
            WriteLock w(itemLock);
            
            // 保存当前选择
            preserveUserSelection();
            
            // 安全清理
            std::vector<AbstractMenuItem*> oldItems;
            for (auto item : items) {
                if (item != toggleItem && item != diagItem) {
                    oldItems.push_back(item);
                }
            }
            
            items.clear();
            items.push_back(toggleItem);
            items.push_back(diagItem);
            
            // 在锁外删除
            w.unlock();
            for (auto item : oldItems) {
                delete item;
            }
            w.lock();
            
            prevScan.clear();
            scope.count = 2;
            if (scope.selected >= scope.count) {
                scope.selected = std::max(0, scope.count - 1);
            }
            layout_called = false;
            workerDirty = true;
            pollSecs = 15;
        }

        // reset selection scope (locks internally)
        if (workerDirty)
        {
            // 不直接调用performLayout，而是手动更新scope
            {
                WriteLock w(itemLock);
                scope.start = 0;
                scope.count = items.size();
                scope.max_visible_options = 5;
                scope.end = std::min(scope.count, scope.max_visible_options);
                scope.visible_rows = scope.end;
                
                // 恢复用户选择
                restoreUserSelection();
                
                layout_called = true;
            }
        }

        // 使用条件变量进行可中断的等待，放在循环末尾
        {
            std::unique_lock<std::mutex> lock(quitMutex);
            if (quitCondition.wait_for(lock, std::chrono::seconds(pollSecs), [this] { return quit; })) {
                break; // 收到退出信号
            }
        }
    }
}

ConnectKnownItem::ConnectKnownItem(WIFI_network n, bool& dirty)
    : MenuItem(ListItemType::Button, "Connect", "Connect to this network.", [&](AbstractMenuItem &item) -> InputReactionHint{
        // 异步连接避免阻塞UI
        std::thread([this, &dirty]() {
            WIFI_connect(net.ssid, net.security); 
            dirty = true;
        }).detach();
        return Exit;
    }), net(n)
{}

ConnectNewItem::ConnectNewItem(WIFI_network n, bool& dirty)
    : MenuItem(ListItemType::Button, "Enter WiFi passcode", "Connect to this network.", DeferToSubmenu, new KeyboardPrompt("Enter Wifi passcode", 
        [&](AbstractMenuItem &item) -> InputReactionHint {
            // 异步连接避免阻塞UI
            std::thread([this, &dirty, &item]() {
                WIFI_connectPass(net.ssid, net.security, item.getName().c_str()); 
                dirty = true;
            }).detach();
            return Exit; 
        })), net(n)
{}

ForgetItem::ForgetItem(WIFI_network n, bool& dirty)
    : MenuItem(ListItemType::Button, "Forget", "Removes credentials for this network.",
        [&](AbstractMenuItem &item) -> InputReactionHint { 
            WIFI_forget(net.ssid, net.security); 
            dirty = true; 
            return Exit;
        }), net(n)
{}


NetworkItem::NetworkItem(WIFI_network n, bool connected, MenuList* submenu)
    : MenuItem(ListItemType::Custom, n.ssid, n.bssid, DeferToSubmenu, submenu), net(n), connected(connected)
{}

void NetworkItem::drawCustomItem(SDL_Surface *surface, const SDL_Rect &dst, const AbstractMenuItem &item, bool selected) const
{
    SDL_Color text_color = uintToColour(THEME_COLOR4_255);
    SDL_Surface *text = TTF_RenderUTF8_Blended(font.tiny, item.getLabel().c_str(), COLOR_WHITE); // always white

    // hack - this should be correlated to max_width
    int mw = dst.w;

    if (selected)
    {
        // gray pill
        GFX_blitPillLightCPP(ASSET_BUTTON, surface, {dst.x, dst.y, mw, SCALE1(BUTTON_SIZE)});
    }

    // wifi icon
    auto asset =
        net.rssi >= -60 ? ASSET_WIFI :    // anything above 61
        net.rssi >= -70 ? ASSET_WIFI_MED  // -61 and below
                        : ASSET_WIFI_LOW; // -71 and below
    SDL_Rect rect = {0, 0, 12, 12};
    int ix = dst.x + dst.w - SCALE1(OPTION_PADDING + rect.w);
    int y = dst.y + SCALE1(BUTTON_SIZE - rect.h) / 2;
    SDL_Rect tgt{ix, y};
    GFX_blitAssetColor(asset, NULL, surface, &tgt, THEME_COLOR6);

    // connected
    if(connected) {
        SDL_Rect rect = {0, 0, 12, 12};
        ix = ix - SCALE1(OPTION_PADDING + rect.w);
        int y = dst.y + SCALE1(BUTTON_SIZE - rect.h) / 2;
        SDL_Rect tgt{ix, y};
        GFX_blitAssetColor(ASSET_CHECKCIRCLE, NULL, surface, &tgt, THEME_COLOR6);
    }
    // encrypted
    else if(net.security != SECURITY_NONE) {
        SDL_Rect rect = {0, 0, 8, 11};
        ix = ix - SCALE1(OPTION_PADDING + rect.w + 2);
        int y = dst.y + SCALE1(BUTTON_SIZE - rect.h) / 2;
        SDL_Rect tgt{ix, y};
        GFX_blitAssetColor(ASSET_LOCK, NULL, surface, &tgt, THEME_COLOR6);
    }

    if (selected)
    {
        // white pill
        int w = 0;
        TTF_SizeUTF8(font.small, item.getName().c_str(), &w, NULL);
        w += SCALE1(OPTION_PADDING * 2);
        GFX_blitPillDarkCPP(ASSET_BUTTON, surface, {dst.x, dst.y, w, SCALE1(BUTTON_SIZE)});
        text_color = uintToColour(THEME_COLOR5_255);
    }

    text = TTF_RenderUTF8_Blended(font.small, item.getName().c_str(), text_color);
    SDL_BlitSurfaceCPP(text, {}, surface, {dst.x + SCALE1(OPTION_PADDING), dst.y + SCALE1(1)});
    SDL_FreeSurface(text);
}