#include "menu.hpp"

extern "C"
{
#include "defines.h"
#include "api.h"
#include "utils.h"
}

#include <mutex>
#include <shared_mutex>
typedef std::shared_mutex Lock;
typedef std::unique_lock< Lock >  WriteLock;
typedef std::shared_lock< Lock >  ReadLock;

///////////////////////////////////////////////////////////

MenuItem::MenuItem(ListItemType type, const std::string &name, const std::string &desc,
                   const std::vector<std::any> &values, const std::vector<std::string> &labels,
                   ValueGetCallback on_get, ValueSetCallback on_set, ValueResetCallback on_reset, 
                   MenuListCallback on_confirm, MenuList *submenu)
    : AbstractMenuItem(type, name, desc, on_get, on_set, on_reset, on_confirm, submenu), 
    values(values), labels(labels)
{
    initSelection();
}

MenuItem::MenuItem(ListItemType type, const std::string &name, const std::string &desc, const std::vector<std::any> &values,
                   ValueGetCallback on_get, ValueSetCallback on_set, ValueResetCallback on_reset,
                   MenuListCallback on_confirm, MenuList *submenu)
    : AbstractMenuItem(type, name, desc, on_get, on_set, on_reset, on_confirm, submenu), 
    values(values)
{
    generateDefaultLabels();
    initSelection();
}

MenuItem::MenuItem(ListItemType type, const std::string &name, const std::string &desc, 
                   int min, int max, const std::string suffix,
                   ValueGetCallback on_get, ValueSetCallback on_set, ValueResetCallback on_reset,
                   MenuListCallback on_confirm, MenuList *submenu)
    : AbstractMenuItem(type, name, desc, on_get, on_set, on_reset, on_confirm, submenu)
{
    const int step = 1; // until we need it
    const int num = (max - min) / step + 1;
    for (int i = 0; i < num; i++)
        values.push_back(min + i * step);

    generateDefaultLabels(suffix);
    initSelection();
    assert(valueIdx >= 0);
}

MenuItem::MenuItem(ListItemType type, const std::string &name, const std::string &desc,
    MenuListCallback on_confirm, MenuList *submenu)
    : MenuItem(type, name, desc, 0,0, "", nullptr, nullptr, nullptr, on_confirm, submenu)
{}

void MenuItem::generateDefaultLabels(const std::string& suffix)
{
    labels.clear();
    for (auto v : values)
    {
        if (v.type() == typeid(std::string))
            labels.push_back(std::any_cast<std::string>(v) + suffix);
        else if (v.type() == typeid(float))
            labels.push_back(std::to_string(std::any_cast<float>(v)) + suffix);
        else if (v.type() == typeid(int))
            labels.push_back(std::to_string(std::any_cast<int>(v)) + suffix);
        else if (v.type() == typeid(uint32_t))
            labels.push_back(std::to_string(std::any_cast<uint32_t>(v)) + suffix);
        else if (v.type() == typeid(bool))
            labels.push_back((std::any_cast<bool>(v) ? "On" : "Off") + suffix);
        else
            assert(false); // needs more string conversion
    }
}

void MenuItem::initSelection()
{
    int oldValueIdx = valueIdx;
    valueIdx = -1;
    if (!values.empty())
    {
        valueIdx = 0;
        if (on_get)
        {
            const auto initialVal = on_get();
            try
            {
                for (int i = 0; i < values.size(); i++)
                {
                    const auto &v = values[i];
                    if (v.type() != initialVal.type())
                        LOG_error("type mismatch: %s vs. %s", v.type().name(), initialVal.type().name());

                    assert(v.type() == initialVal.type());

                    if (v.type() == typeid(float))
                    {
                        if (std::any_cast<float>(initialVal) == std::any_cast<float>(v))
                        {
                            valueIdx = i;
                            break;
                        }
                    }
                    else if (v.type() == typeid(int))
                    {
                        if (std::any_cast<int>(initialVal) == std::any_cast<int>(v))
                        {
                            valueIdx = i;
                            break;
                        }
                    }
                    else if (v.type() == typeid(unsigned int))
                    {
                        if (std::any_cast<unsigned int>(initialVal) == std::any_cast<unsigned int>(v))
                        {
                            valueIdx = i;
                            break;
                        }
                    }
                    else if (v.type() == typeid(uint32_t))
                    {
                        if (std::any_cast<uint32_t>(initialVal) == std::any_cast<uint32_t>(v))
                        {
                            valueIdx = i;
                            break;
                        }
                    }
                    else if (v.type() == typeid(bool))
                    {
                        if (std::any_cast<bool>(initialVal) == std::any_cast<bool>(v))
                        {
                            valueIdx = i;
                            break;
                        }
                    }
                    else if (v.type() == typeid(std::string))
                    {
                        if (std::any_cast<std::string>(initialVal) == std::any_cast<std::string>(v))
                        {
                            valueIdx = i;
                            break;
                        }
                    }
                    else if (v.type() == typeid(std::basic_string<char>))
                    {
                        if (std::any_cast<std::basic_string<char>>(initialVal) == std::any_cast<std::basic_string<char>>(v))
                        {
                            valueIdx = i;
                            break;
                        }
                    }
                    else
                    {
                        LOG_warn("Cant initialize selection for %s from unknown type %s\n", this->getLabel(), v.type().name());
                    }
                }
            }
            catch (...)
            {
                LOG_warn("Bad any cast for %s\n", this->getLabel());
            }
        }
        assert(valueIdx >= 0);
    }
}

InputReactionHint MenuItem::handleInput(int &dirty)
{
    InputReactionHint hint = Unhandled;

    if (deferred)
    {
        assert(submenu);
        int subMenuJustClosed = 0;
        hint = submenu->handleInput(dirty, subMenuJustClosed);
        if (subMenuJustClosed) {
            defer(false);
            dirty = 1;
        }
        return hint;
    }

    if (PAD_justRepeated(BTN_LEFT))
    {
        hint = NoOp;
        if (prevValue())
        {
            if (on_set)
                on_set(getValue());
            dirty = 1;
        }
    }
    else if (PAD_justRepeated(BTN_RIGHT))
    {
        hint = NoOp;
        if (nextValue())
        {
            if (on_set)
                on_set(getValue());
            dirty = 1;
        }
    }
    if (PAD_justRepeated(BTN_L1))
    {
        hint = NoOp;
        if (prev(10))
        {
            if (on_set)
                on_set(getValue());
            dirty = 1;
        }
    }
    else if (PAD_justRepeated(BTN_R1))
    {
        hint = NoOp;
        if (next(10))
        {
            if (on_set)
                on_set(getValue());
            dirty = 1;
        }
    }
    else if (PAD_justPressed(BTN_A))
    {
        hint = NoOp;
        if (on_confirm)
            hint = on_confirm(*this);
        dirty = 1;
    }

    return hint;
}

bool MenuItem::next(int n)
{
    if (valueIdx < 0)
        return false;
    valueIdx = (valueIdx + n) % values.size();
    return true;
}

bool MenuItem::prev(int n)
{
    if (valueIdx < 0)
        return false;
    valueIdx = (valueIdx + values.size() - n) % values.size();
    return true;
}

///////////////////////////////////////////////////////////

MenuList::MenuList(MenuItemType type, const std::string &descp, std::vector<AbstractMenuItem*> items, MenuListCallback on_change, MenuListCallback on_confirm)
    : type(type), desc(descp), items(items), on_change(on_change), on_confirm(on_confirm)
{
    performLayout((SDL_Rect){0, 0, FIXED_WIDTH, FIXED_HEIGHT});
    layout_called = false;
}

MenuList::~MenuList()
{
    WriteLock w(itemLock);
    for (auto item : items)
        delete item;
    items.clear();
}

void MenuList::performLayout(const SDL_Rect &dst)
{
    ReadLock r(itemLock);
    scope.start = 0;
    scope.selected = 0;
    scope.count = items.size();
    
    scope.max_visible_options = 5;

    scope.end = std::min(scope.count, scope.max_visible_options);
    scope.visible_rows = scope.end;

    for (auto itm : items)
        if (itm->getSubMenu())
            itm->getSubMenu()->performLayout(dst);

    layout_called = true;
}

bool MenuList::selectNext()
{
    scope.selected++;
    if (scope.selected >= scope.count)
    {
        scope.selected = 0;
        scope.start = 0;
        scope.end = scope.visible_rows;
    }
    else if (scope.selected >= scope.end)
    {
        scope.start++;
        scope.end++;
    }

    return true;
}

bool MenuList::selectPrev()
{
    scope.selected--;
    if (scope.selected < 0)
    {
        scope.selected = scope.count - 1;
        scope.start = std::max(0, scope.count - scope.max_visible_options);
        scope.end = scope.count;
    }
    else if (scope.selected < scope.start)
    {
        scope.start--;
        scope.end--;
    }

    return true;
}

InputReactionHint MenuList::handleInput(int &dirty, int &quit)
{
    ReadLock r(itemLock);
    InputReactionHint handled = items.at(scope.selected)->handleInput(dirty);
    if(handled == ResetAllItems) {
        resetAllItems();
        dirty = 1;
        return NoOp;
    }
    else if (handled == Exit) {
        quit = 1;
        return NoOp;
    }
    else if (handled != Unhandled)
        return handled;

    if (PAD_justRepeated(BTN_UP))
    {
        if (scope.selected == 0 && !PAD_justPressed(BTN_UP))
            return NoOp;
        if (selectPrev())
            dirty = 1;
        return NoOp;
    }
    else if (PAD_justRepeated(BTN_DOWN))
    {
        if (scope.selected == scope.count - 1 && !PAD_justPressed(BTN_DOWN))
            return NoOp;
        if (selectNext())
            dirty = 1;
        return NoOp;
    }
    else if (on_change)
    {
    }
    else if (on_confirm && PAD_justPressed(BTN_A))
    {
    }
    else if (PAD_justPressed(BTN_B))
    {
        quit = 1;
        return NoOp;
    }

    return Unhandled;
}

SDL_Rect MenuList::itemSizeHint(const AbstractMenuItem &item)
{
    if (type == MenuItemType::Fixed)
    {
        return {0, 0, 0, SCALE1(PILL_SIZE)};
    }
    else if (type == MenuItemType::List)
    {
        int w = 0;
        TTF_SizeUTF8(font.small, item.getName().c_str(), &w, NULL);
        w += SCALE1(OPTION_PADDING * 2);
        return {0, 0, w, SCALE1(PILL_SIZE)};
    }
    else if (type == MenuItemType::Input || type == MenuItemType::Var)
    {
        int w = 0;
        int lw = 0;
        int rw = 0;
        TTF_SizeUTF8(font.small, item.getName().c_str(), &lw, NULL);
        int mrw = 0;
        if (!mrw || type != MenuItemType::Input)
        {
            for (int j = 0; item.getValues().size() > j && !item.getLabels()[j].empty(); j++)
            {
                TTF_SizeUTF8(font.tiny, item.getLabels()[j].c_str(), &rw, NULL);
                if (lw + rw > w)
                    w = lw + rw;
                if (rw > mrw)
                    mrw = rw;
            }
        }
        else
        {
            w = lw + mrw;
        }
        w += SCALE1(OPTION_PADDING * 4);
        return {0, 0, w, SCALE1(PILL_SIZE)};
    }
    else if (type == MenuItemType::Main)
    {
        int w = 0;
        TTF_SizeUTF8(font.large, item.getName().c_str(), &w, NULL);
        w += SCALE1(BUTTON_PADDING * 2);
        return {0, 0, w, SCALE1(PILL_SIZE)};
    }
    else
    {
        assert(false);
    }
}

void MenuList::draw(SDL_Surface *surface, const SDL_Rect &dst)
{
    assert(layout_called);
    ReadLock r(itemLock);

    auto cur = !items.empty() ? items.at(scope.selected) : nullptr;
    if (cur && cur->isDeferred())
    {
        assert(cur->getSubMenu());
        cur->getSubMenu()->draw(surface, dst);
    }
    else
    {
        // 计算列表的居中位置
        int list_h = scope.visible_rows * SCALE1(PILL_SIZE);
        int centered_y = dst.y + (dst.h - list_h) / 2;
        SDL_Rect content_dst = {dst.x, centered_y, dst.w, list_h};

        switch (type)
        {
        case MenuItemType::List:
            drawList(surface, content_dst);
            break;
        case MenuItemType::Fixed:
            drawFixed(surface, content_dst);
            break;
        case MenuItemType::Var:
        case MenuItemType::Input:
            drawInput(surface, content_dst);
            break;
        case MenuItemType::Main:
            drawMain(surface, content_dst);
            break;
        case MenuItemType::Custom:
            drawCustom(surface, dst);
            return;
        default:
            assert(false && "Unknown list type");
        }

        if (cur && cur->getDesc().length() > 0)
        {
            int w, h;
            const auto description = cur->getDesc();
            GFX_sizeText(font.tiny, description.c_str(), SCALE1(FONT_SMALL), &w, &h);
            GFX_blitTextCPP(font.tiny, description.c_str(), SCALE1(FONT_SMALL), uintToColour(THEME_COLOR4_255), surface, {(dst.x + dst.w - w) / 2, dst.y + dst.h - h, w, h});
        }
    }
}

void MenuList::drawList(SDL_Surface *surface, const SDL_Rect &dst)
{
    if (max_width == 0)
    {
        int mw = 0;
        for (auto item : items)
        {
            auto hintRect = itemSizeHint(*item);
            if (hintRect.w > mw)
                mw = hintRect.w;
        }
        max_width = std::min(mw, dst.w);
    }

    SDL_Rect rect = dst;
    rect = dx(rect, (rect.w - max_width) / 2);

    int selected_row = scope.selected - scope.start;
    for (int i = scope.start, j = 0; i < scope.end; i++, j++)
    {
        auto pos = dy(rect, SCALE1(j * PILL_SIZE));
        drawListItem(surface, pos, *items[i], j == selected_row);
    }
}

void MenuList::drawListItem(SDL_Surface *surface, const SDL_Rect &dst, const AbstractMenuItem &item, bool selected)
{
    SDL_Color text_color = uintToColour(THEME_COLOR4_255);
    SDL_Surface *text;

    if (selected)
    {
        int w = 0;
        TTF_SizeUTF8(font.small, item.getName().c_str(), &w, NULL);
        w += SCALE1(OPTION_PADDING * 2);

        GFX_blitPillDarkCPP(ASSET_WHITE_PILL, surface, {dst.x, dst.y, w, SCALE1(PILL_SIZE)});
        text_color = uintToColour(THEME_COLOR5_255);
    }
    text = TTF_RenderUTF8_Blended(font.small, item.getName().c_str(), text_color);
    SDL_BlitSurfaceCPP(text, {}, surface, {dst.x + SCALE1(OPTION_PADDING), dst.y + SCALE1(1)});
    SDL_FreeSurface(text);
}

void MenuList::drawFixed(SDL_Surface *surface, const SDL_Rect &dst)
{
    int mw = dst.w;
    max_width = mw;

    SDL_Rect rect = dst;

    int selected_row = scope.selected - scope.start;
    for (int i = scope.start, j = 0; i < scope.end; i++, j++)
    {
        auto pos = dy(rect, SCALE1(j * PILL_SIZE));
        drawFixedItem(surface, pos, *items[i], j == selected_row);
    }
}

namespace
{
    static inline void rgb_unpack(uint32_t col, int *r, int *g, int *b)
    {
        *r = (col >> 16) & 0xff;
        *g = (col >> 8) & 0xff;
        *b = col & 0xff;
    }

    static inline uint32_t mapUint(SDL_Surface *surface, uint32_t col)
    {
        int r, g, b;
        rgb_unpack(col, &r, &g, &b);
        return SDL_MapRGB(surface->format, r, g, b);
    }
}

void MenuList::drawFixedItem(SDL_Surface *surface, const SDL_Rect &dst, const AbstractMenuItem &item, bool selected)
{
    SDL_Color text_color = uintToColour(THEME_COLOR4_255);
    SDL_Color text_color_value = uintToColour(THEME_COLOR4_255);
    SDL_Surface *text;

    int mw = dst.w;

    if (selected)
    {
        GFX_blitPillLightCPP(ASSET_WHITE_PILL, surface, {dst.x, dst.y, mw, SCALE1(PILL_SIZE)});
    }

    if (item.getValue().has_value())
    {
        text = TTF_RenderUTF8_Blended(font.large, item.getLabel().c_str(), text_color_value);

        if (item.getType() == ListItemType::Color)
        {
            uint32_t color = mapUint(surface, std::any_cast<uint32_t>(item.getValue()));
            SDL_Rect rect = {
                dst.x + dst.w - SCALE1(OPTION_PADDING + FONT_LARGE),
                dst.y + SCALE1(PILL_SIZE - FONT_LARGE) / 2,
                SCALE1(FONT_LARGE), SCALE1(FONT_LARGE)};
            SDL_FillRect(surface, &rect, RGB_WHITE);
            rect = dy(dx(rect, 1), 1);
            rect.h -= 1;
            rect.w -= 1;
            SDL_FillRect(surface, &rect, color);
#define COLOR_PADDING 4
            SDL_BlitSurfaceCPP(text, {}, surface, {dst.x + mw - text->w - SCALE1(OPTION_PADDING + COLOR_PADDING + FONT_LARGE), dst.y + SCALE1(3)});
        }
        else if(item.getType() == ListItemType::Button) {
        }
        else if(item.getType() == ListItemType::Custom) {
            item.drawCustomItem(surface, dst, item, selected);
        }
        else
            SDL_BlitSurfaceCPP(text, {}, surface, {dst.x + mw - text->w - SCALE1(OPTION_PADDING), dst.y + SCALE1(3)});
        SDL_FreeSurface(text);
    }

    if (selected)
    {
        int w = 0;
        TTF_SizeUTF8(font.large, item.getName().c_str(), &w, NULL);
        w += SCALE1(OPTION_PADDING * 2);
        GFX_blitPillDarkCPP(ASSET_WHITE_PILL, surface, {dst.x, dst.y, w, SCALE1(PILL_SIZE)});
        text_color = uintToColour(THEME_COLOR5_255);
    }

    text = TTF_RenderUTF8_Blended(font.large, item.getName().c_str(), text_color);
    SDL_BlitSurfaceCPP(text, {}, surface, {dst.x + SCALE1(OPTION_PADDING), dst.y + SCALE1(3)});
    SDL_FreeSurface(text);
}

void MenuList::drawInput(SDL_Surface *surface, const SDL_Rect &dst)
{
    if (max_width == 0)
    {
        int mw = 0;
        for (auto item : items)
        {
            auto hintRect = itemSizeHint(*item);
            if (hintRect.w > mw)
                mw = hintRect.w;
        }
        max_width = std::min(mw, dst.w);
    }

    SDL_Rect rect = dst;
    rect = dx(rect, (rect.w - max_width) / 2);

    int selected_row = scope.selected - scope.start;
    for (int i = scope.start, j = 0; i < scope.end; i++, j++)
    {
        auto pos = dy(rect, SCALE1(j * PILL_SIZE));
        pos.w = max_width;
        drawInputItem(surface, pos, *items[i], j == selected_row);
    }
}

void MenuList::drawInputItem(SDL_Surface *surface, const SDL_Rect &dst, const AbstractMenuItem &item, bool selected)
{
    SDL_Color text_color = COLOR_WHITE;
    SDL_Surface *text;

    int mw = dst.w;

    if (selected)
    {
        GFX_blitPillLightCPP(ASSET_WHITE_PILL, surface, {dst.x, dst.y, mw, SCALE1(PILL_SIZE)});

        int w = 0;
        TTF_SizeUTF8(font.small, item.getName().c_str(), &w, NULL);
        w += SCALE1(OPTION_PADDING * 2);
        GFX_blitPillDarkCPP(ASSET_WHITE_PILL, surface, {dst.x, dst.y, w, SCALE1(PILL_SIZE)});
        text_color = COLOR_BLACK;
    }
    text = TTF_RenderUTF8_Blended(font.small, item.getName().c_str(), text_color);
    SDL_BlitSurfaceCPP(text, {}, surface, {dst.x + SCALE1(OPTION_PADDING), dst.y + SCALE1(1)});
    SDL_FreeSurface(text);

    if (selected)
    {
    }
    else if (item.getValue().has_value())
    {
        text = TTF_RenderUTF8_Blended(font.tiny, item.getLabel().c_str(), COLOR_WHITE);
        SDL_BlitSurfaceCPP(text, {}, surface, {dst.x + mw - text->w - SCALE1(OPTION_PADDING), dst.y + SCALE1(1)});
        SDL_FreeSurface(text);
    }
}

void MenuList::drawMain(SDL_Surface *surface, const SDL_Rect &dst)
{
    if (scope.count > 0)
    {
        int selected_row = scope.selected - scope.start;
        for (int i = scope.start, j = 0; i < scope.end; i++, j++)
        {
            auto pos = dy(dst, SCALE1(j * PILL_SIZE));
            pos.h = SCALE1(PILL_SIZE);

            drawMainItem(surface, pos, *items[i], j == selected_row);
        }
    }
    else
    {
        GFX_blitMessageCPP(font.large, "Empty folder", surface, dst);
    }
}

void MenuList::drawMainItem(SDL_Surface *surface, const SDL_Rect &dst, const AbstractMenuItem &item, bool selected)
{
    SDL_Color text_color = COLOR_WHITE;
    SDL_Surface *text;

    const bool unique = false;

    char truncated[256];
    int text_width = GFX_truncateText(font.large, item.getName().c_str(), truncated, dst.w, SCALE1(BUTTON_PADDING * 2));
    int max_width = std::min(dst.w, text_width);

    if (selected)
    {
        GFX_blitPillDarkCPP(ASSET_WHITE_PILL, surface, {dst.x, dst.y, max_width, dst.h});
        text_color = COLOR_BLACK;
    }
    else if (unique)
    {
    }
    text = TTF_RenderUTF8_Blended(font.large, truncated, text_color);
    SDL_BlitSurfaceCPP(text, {}, surface, {dst.x + SCALE1(BUTTON_PADDING), dst.y + SCALE1(3)});
    SDL_FreeSurface(text);
}

void MenuList::resetAllItems()
{
    ReadLock r(itemLock);
    for(auto item : items) {
        if(item->on_reset) {
            item->on_reset();
            item->initSelection();
        }
    }
}