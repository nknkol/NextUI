#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "sdl.h"
#include "api.h"

#if __cplusplus
extern "C" {
#endif

/**
 * @brief 显示一个与系统UI融合的虚拟键盘，并等待用户输入。
 *
 * @param screen 指向主屏幕 SDL_Surface 的指针。
 * @param fonts 指向已加载 GFX_Fonts 结构体的指针。
 * @param title 显示在键盘顶部的标题。
 * @param initial_text 输入框的初始文本 (可以为 NULL)。
 *
 * @return 返回一个由 malloc 分配的字符串，包含用户的最终输入。
 * 如果用户取消操作，则返回 NULL。
 * 调用者在使用完返回的字符串后，有责任调用 free() 来释放内存。
 */
char* ShowKeyboard(SDL_Surface* screen, GFX_Fonts* fonts, const char* title, const char* initial_text);

#if __cplusplus
}
#endif

#endif // KEYBOARD_H

