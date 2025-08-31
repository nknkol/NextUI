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
    quit.store(true); // 使用原子写操作
    quitCondition.notify_all(); // 唤醒睡眠中的线程
    
    if (worker.joinable())
        worker.join();
}

InputReactionHint Menu::handleInput(int &dirty, int &quit)
{
    auto ret = MenuList::handleInput(dirty, quit);
    // exchange原子地将workerDirty设为false, 并返回它之前的值
    if (workerDirty.exchange(false))
    {
        dirty = true;
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
    // 确保 items 列表和 scope.selected 都是有效的
    if (scope.selected >= 0 && scope.selected < items.size() && items[scope.selected]) {
        lastSelectedItemName = items[scope.selected]->getName();
        lastSelectedIndex = scope.selected; // <--- 添加这一行来保存当前索引
    }
}

void Menu::restoreUserSelection()
{
    bool selectionRestored = false;

    // 1. 优先尝试按名称恢复，这是最精确的方式
    if (!lastSelectedItemName.empty()) {
        for (int i = 0; i < items.size(); i++) {
            if (items[i] && items[i]->getName() == lastSelectedItemName) {
                scope.selected = i;
                selectionRestored = true;
                break;
            }
        }
    }

    // 2. 如果按名称恢复失败 (例如WiFi消失了), 则使用我们保存的索引来定位
    if (!selectionRestored) {
        // 将旧的索引限制在新列表的有效范围内
        // 这会自动选中新列表中最接近旧位置的项
        scope.selected = std::max(0, std::min(lastSelectedIndex, (int)items.size() - 1));
    }

    // 3. 最后，调整滚动视图，确保新选中的项目是可见的
    // 确保 end 不会超过新的列表边界
    scope.end = std::min(scope.start + scope.max_visible_options, (int)items.size());
    if (scope.selected >= scope.end) { // 如果选中项在当前视图下方
        scope.start = scope.selected - scope.max_visible_options + 1;
        scope.end = scope.start + scope.max_visible_options;
    } else if (scope.selected < scope.start) { // 如果选中项在当前视图上方
        scope.start = scope.selected;
        scope.end = scope.start + scope.max_visible_options;
    }
    
    // 再次确保 start 和 end 不会越界
    scope.start = std::max(0, scope.start);
    scope.end = std::min((int)items.size(), scope.end);
}

template <typename Map>
bool key_compare(Map const &lhs, Map const &rhs)
{
    return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin(),
                                                  [](auto a, auto b)
                                                  { return a.first == b.first; });
}

// void Menu::updater()
// {
//     int pollSecs = 15;
//     std::map<std::string, WIFI_network> prevScan;
//     std::string prevSsid;
    
//     while (!quit.load() && !globalQuit)
//     {

//         // TODO: pause when menu is not rendered
//         // Scan
//         if (WIFI_enabled())
//         {
//             // scan for available networks and add a menu item for each
//             WIFI_connection connection;
//             if(WIFI_connectionInfo(&connection) < 0)
//                 continue; // try again in a bit

//             // grab list and compare it to previous result
//             // only relayout the menu if changes happended
//             std::vector<WIFI_network> scanResults(SCAN_MAX_RESULTS);
//             int cnt = WIFI_scan(scanResults.data(), SCAN_MAX_RESULTS);
//             if(cnt < 0)
//                 continue; // try again in a bit

//             std::map<std::string, WIFI_network> scanSsids;
//             for (int i = 0; i < cnt; i++)
//                 scanSsids.emplace(scanResults[i].ssid, scanResults[i]);

//             // dont repopulate if any submenu is open
//             bool menuOpen = false;
//             {
//                 ReadLock r(itemLock);
//                 for(auto i : items){
//                     if(i && i->isDeferred()){
//                         menuOpen = true;
//                         break;
//                     }
//                 }
//             }

//             // something changed?
//             if (!menuOpen &&
//                 (prevSsid != std::string(connection.ssid) 
//                 || !key_compare(prevScan, scanSsids)))
//             {
//                 prevScan = scanSsids;
//                 prevSsid = connection.ssid;

//                 {
//                     WriteLock w(itemLock);
                    
//                     // 保存当前选择
//                     preserveUserSelection();
                    
//                     // 清理并删除旧的网络列表项
//                     std::vector<AbstractMenuItem*> toDelete;
//                     items.erase(std::remove_if(items.begin(), items.end(), 
//                         [&](AbstractMenuItem* item) {
//                             if (item != toggleItem && item != diagItem) {
//                                 toDelete.push_back(item);
//                                 return true;
//                             }
//                             return false;
//                         }), items.end());

//                     // 在持有锁的情况下安全删除
//                     for (auto item : toDelete) {
//                         delete item;
//                     }

//                     scope.count = items.size();
//                     layout_called = false;

//                     for (auto &[s, r] : scanSsids)
//                     {
//                         bool connected = false;
//                         bool hasCredentials = WIFI_isKnown(r.ssid, r.security);

//                         if (strcmp(connection.ssid, r.ssid) == 0)
//                             connected = true;

//                         MenuList *options;
//                         if (connected)
//                             options = new MenuList(MenuItemType::List, "Options",
//                                                 {
//                                                     new MenuItem{ListItemType::Button, "Disconnect", "Disconnect from this network.",
//                                                                     [&](AbstractMenuItem &item) -> InputReactionHint
//                                                                     { WIFI_disconnect(); workerDirty = true; return Exit; }},
//                                                     new ForgetItem(r, workerDirty)
//                                                 });
//                         else 
//                         if (hasCredentials)
//                             options = new MenuList(MenuItemType::List, "Options", { new ConnectKnownItem(r, workerDirty), new ForgetItem(r, workerDirty) });
//                         else
//                             options = new MenuList(MenuItemType::List, "Options", { new ConnectNewItem(r, workerDirty) });

//                         auto itm = new NetworkItem{r, connected, options};
//                         if(connected && !std::string(connection.ip).empty())
//                             itm->setDesc(std::string(r.bssid) + " | " + std::string(connection.ip));
//                         items.push_back(itm);
//                     }
                    
//                     scope.count = items.size();
//                     // 确保selected在有效范围内
//                     if (scope.selected >= scope.count) {
//                         scope.selected = std::max(0, scope.count - 1);
//                     }
                    
//                     workerDirty.store(true);
//                 } // 锁在这里自动释放
//             }
//             pollSecs = 2;
//         }
//         else
//         {
//             WriteLock w(itemLock);
            
//             // 保存当前选择
//             preserveUserSelection();
            
//             // 安全清理并删除旧项
//             std::vector<AbstractMenuItem*> toDelete;
//             items.erase(std::remove_if(items.begin(), items.end(),
//                 [&](AbstractMenuItem* item) {
//                     if (item != toggleItem && item != diagItem) {
//                         toDelete.push_back(item);
//                         return true;
//                     }
//                     return false;
//                 }), items.end());

//             // 在持有锁的情况下安全删除
//             for (auto item : toDelete) {
//                 delete item;
//             }
            
//             prevScan.clear();
//             scope.count = items.size(); // 应该是2
//             if (scope.selected >= scope.count) {
//                 scope.selected = std::max(0, scope.count - 1);
//             }
//             layout_called = false;
//             workerDirty.store(true);
//             pollSecs = 15;
//         } // 锁在这里自动释放

//         // reset selection scope (locks internally)
//         if (workerDirty)
//         {
//             // 不直接调用performLayout，而是手动更新scope
//             {
//                 WriteLock w(itemLock);
//                 scope.start = 0;
//                 scope.count = items.size();
//                 scope.max_visible_options = 5;
//                 scope.end = std::min(scope.count, scope.max_visible_options);
//                 scope.visible_rows = scope.end;
                
//                 // 恢复用户选择
//                 restoreUserSelection();
                
//                 layout_called = true;
//             }
//         }

//         // 使用条件变量进行可中断的等待，放在循环末尾
//         {
//             std::unique_lock<std::mutex> lock(quitMutex);
//             if (quitCondition.wait_for(lock, std::chrono::seconds(pollSecs), [this] { return quit.load(); })) { // 使用原子读操作
//                 break; // 收到退出信号
//             }
//         }
//     }
// }
// 请用这个新版本完整替换掉旧的 Menu::updater() 函数
void Menu::updater()
{
    LOG_note(LOG_REALTIME, "[WiFi Updater] Thread started.\n");
    int pollSecs = 15;
    std::map<std::string, WIFI_network> prevScan;
    std::string prevSsid;
    
    // 初始延迟，等待系统稳定
    std::this_thread::sleep_for(std::chrono::seconds(2));

    while (!quit.load() && !globalQuit)
    {
        LOG_note(LOG_REALTIME, "[WiFi Updater] Loop iteration started.\n");

        // TODO: pause when menu is not rendered
        // Scan
        LOG_note(LOG_REALTIME, "[WiFi Updater] Checking if WiFi is enabled...\n");
        if (WIFI_enabled())
        {
            LOG_note(LOG_REALTIME, "[WiFi Updater] WiFi is ON. Getting connection info...\n");
            // scan for available networks and add a menu item for each
            WIFI_connection connection;
            if(WIFI_connectionInfo(&connection) < 0) {
                LOG_note(LOG_REALTIME, "[WiFi Updater] WIFI_connectionInfo failed. Continuing loop.\n");
                continue; // try again in a bit
            }

            // grab list and compare it to previous result
            // only relayout the menu if changes happended
            std::vector<WIFI_network> scanResults(SCAN_MAX_RESULTS);
            LOG_note(LOG_REALTIME, "[WiFi Updater] Starting WIFI_scan...\n");
            int cnt = WIFI_scan(scanResults.data(), SCAN_MAX_RESULTS);
            LOG_note(LOG_REALTIME, "[WiFi Updater] WIFI_scan finished, found %d networks.\n", cnt);

            if(cnt < 0) {
                LOG_note(LOG_REALTIME, "[WiFi Updater] WIFI_scan returned error. Continuing loop.\n");
                continue; // try again in a bit
            }

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
            LOG_note(LOG_REALTIME, "[WiFi Updater] Submenu open: %s.\n", menuOpen ? "yes" : "no");

            // something changed?
            if (!menuOpen &&
                (prevSsid != std::string(connection.ssid) 
                || !key_compare(prevScan, scanSsids)))
            {
                LOG_note(LOG_REALTIME, "[WiFi Updater] Network list changed. Acquiring WriteLock to update UI items...\n");
                prevScan = scanSsids;
                prevSsid = connection.ssid;

                {
                    WriteLock w(itemLock);
                    LOG_note(LOG_REALTIME, "[WiFi Updater] WriteLock acquired. Preserving user selection...\n");
                    
                    // 保存当前选择
                    preserveUserSelection();
                    
                    LOG_note(LOG_REALTIME, "[WiFi Updater] Clearing old network items...\n");
                    // 清理并删除旧的网络列表项
                    std::vector<AbstractMenuItem*> toDelete;
                    items.erase(std::remove_if(items.begin(), items.end(), 
                        [&](AbstractMenuItem* item) {
                            if (item != toggleItem && item != diagItem) {
                                toDelete.push_back(item);
                                return true;
                            }
                            return false;
                        }), items.end());

                    LOG_note(LOG_REALTIME, "[WiFi Updater] Deleting %zu old items from memory...\n", toDelete.size());
                    // 在持有锁的情况下安全删除
                    for (auto item : toDelete) {
                        delete item;
                    }
                    LOG_note(LOG_REALTIME, "[WiFi Updater] Deletion complete. Re-populating list with %zu new items...\n", scanSsids.size());

                    scope.count = items.size();
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
                                                                    { WIFI_disconnect(); workerDirty.store(true); return Exit; }},
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
                    
                    LOG_note(LOG_REALTIME, "[WiFi Updater] List re-populated. Setting dirty flag.\n");
                    workerDirty.store(true);
                } // 锁在这里自动释放
                LOG_note(LOG_REALTIME, "[WiFi Updater] WriteLock released.\n");
            }
            pollSecs = 2;
        }
        else
        {
            LOG_note(LOG_REALTIME, "[WiFi Updater] WiFi is OFF. Clearing network list if necessary...\n");
            if(items.size() > 2) { // Only clear if there are network items present
                WriteLock w(itemLock);
                LOG_note(LOG_REALTIME, "[WiFi Updater] WriteLock acquired for clearing. Preserving selection...\n");
                
                // 保存当前选择
                preserveUserSelection();
                
                LOG_note(LOG_REALTIME, "[WiFi Updater] Clearing old network items...\n");
                // 安全清理并删除旧项
                std::vector<AbstractMenuItem*> toDelete;
                items.erase(std::remove_if(items.begin(), items.end(),
                    [&](AbstractMenuItem* item) {
                        if (item != toggleItem && item != diagItem) {
                            toDelete.push_back(item);
                            return true;
                        }
                        return false;
                    }), items.end());

                LOG_note(LOG_REALTIME, "[WiFi Updater] Deleting %zu old items from memory...\n", toDelete.size());
                // 在持有锁的情况下安全删除
                for (auto item : toDelete) {
                    delete item;
                }
                
                prevScan.clear();
                scope.count = items.size(); // 应该是2
                if (scope.selected >= scope.count) {
                    scope.selected = std::max(0, scope.count - 1);
                }
                layout_called = false;
                LOG_note(LOG_REALTIME, "[WiFi Updater] Clearing complete. Setting dirty flag.\n");
                workerDirty.store(true);
                LOG_note(LOG_REALTIME, "[WiFi Updater] WriteLock released.\n");
            }
            pollSecs = 15;
        } // 锁在这里自动释放

        // reset selection scope (locks internally)
        if (workerDirty.load())
        {
            LOG_note(LOG_REALTIME, "[WiFi Updater] Worker is dirty. Updating layout...\n");
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
            LOG_note(LOG_REALTIME, "[WiFi Updater] Layout updated.\n");
        }

        // 使用条件变量进行可中断的等待，放在循环末尾
        {
            LOG_note(LOG_REALTIME, "[WiFi Updater] Thread going to sleep for %d seconds...\n\n", pollSecs);
            std::unique_lock<std::mutex> lock(quitMutex);
            if (quitCondition.wait_for(lock, std::chrono::seconds(pollSecs), [this] { return quit.load(); })) {
                LOG_note(LOG_REALTIME, "[WiFi Updater] Quit signal received during sleep. Breaking loop.\n");
                break; // 收到退出信号
            }
        }
    }
    LOG_note(LOG_REALTIME, "[WiFi Updater] Loop finished. Thread exiting.\n");
}

ConnectKnownItem::ConnectKnownItem(WIFI_network n, std::atomic<bool>& dirty)
    : MenuItem(ListItemType::Button, "Connect", "Connect to this network.", [&](AbstractMenuItem &item) -> InputReactionHint{
        // 异步连接避免阻塞UI
        std::thread([net_copy = net, &d = dirty]() mutable {
            WIFI_connect(net_copy.ssid, net_copy.security); 
            d.store(true);
        }).detach();
        return Exit;
    }), net(n)
{}

ConnectNewItem::ConnectNewItem(WIFI_network n, std::atomic<bool>& dirty)
    : MenuItem(ListItemType::Button, "Enter WiFi passcode", "Connect to this network.", DeferToSubmenu, new KeyboardPrompt("Enter Wifi passcode", 
        [n, &dirty](AbstractMenuItem &item) -> InputReactionHint {
            std::string password = item.getName(); 
            // 异步连接避免阻塞UI
            std::thread([network = n, pwd = password, &d = dirty]() {
                WIFI_connectPass(network.ssid, network.security, pwd.c_str()); 
                d.store(true);
            }).detach();
            return Exit; 
        })), net(n)
{}

ForgetItem::ForgetItem(WIFI_network n, std::atomic<bool>& dirty)
    : MenuItem(ListItemType::Button, "Forget", "Removes credentials for this network.",
        [&, &dirty_ref = dirty](AbstractMenuItem &item) -> InputReactionHint { 
            WIFI_forget(net.ssid, net.security); 
            dirty_ref.store(true); 
            return Exit;
        }), net(n)
{}


NetworkItem::NetworkItem(WIFI_network n, bool connected, MenuList* submenu)
    : MenuItem(ListItemType::Custom, n.ssid, n.bssid, DeferToSubmenu, submenu), net(n), connected(connected)
{}

void NetworkItem::drawCustomItem(SDL_Surface *surface, const SDL_Rect &dst, const AbstractMenuItem &item, bool selected) const
{
    // --- 只绘制图标，其他所有背景和文字都交由父类处理 ---

    // 预设图标颜色
    uint32_t icon_color = selected ? THEME_COLOR6 : THEME_COLOR4_255;

    // 1. 绘制信号强度图标
    auto asset =
        net.rssi >= -60 ? ASSET_WIFI :    // 强
        net.rssi >= -70 ? ASSET_WIFI_MED  // 中
                        : ASSET_WIFI_LOW; // 弱
    SDL_Rect icon_rect = {0, 0, 12, 12};
    int icon_x = dst.x + dst.w - SCALE1(OPTION_PADDING + icon_rect.w);
    int icon_y = dst.y + (SCALE1(PILL_SIZE) - SCALE1(icon_rect.h)) / 2;
    SDL_Rect target_pos = {icon_x, icon_y};
    GFX_blitAssetColor(asset, NULL, surface, &target_pos, icon_color);

    // 记录当前图标的X坐标，为下一个图标定位
    int next_icon_x = icon_x;

    // 2. 绘制连接/锁定状态图标
    if(connected) {
        // 绘制“已连接”图标
        icon_rect = {0, 0, 12, 12};
        next_icon_x = next_icon_x - SCALE1(OPTION_PADDING + icon_rect.w);
        icon_y = dst.y + (SCALE1(PILL_SIZE) - SCALE1(icon_rect.h)) / 2;
        target_pos = {next_icon_x, icon_y};
        GFX_blitAssetColor(ASSET_CHECKCIRCLE, NULL, surface, &target_pos, icon_color);
    }
    else if(net.security != SECURITY_NONE) {
        // 绘制“锁定”图标
        icon_rect = {0, 0, 8, 11};
        next_icon_x = next_icon_x - SCALE1(OPTION_PADDING + icon_rect.w);
        icon_y = dst.y + (SCALE1(PILL_SIZE) - SCALE1(icon_rect.h)) / 2;
        target_pos = {next_icon_x, icon_y};
        GFX_blitAssetColor(ASSET_LOCK, NULL, surface, &target_pos, icon_color);
    }
}