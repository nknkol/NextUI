#ifndef LANG_H
#define LANG_H

/**
 * @brief 初始化语言模块。
 *
 * @param lang_code 语言代码 (例如 "en", "zh")。函数将尝试加载对应的 .ini 文件。
 */
void Lang_Init(const char* lang_code);

/**
 * @brief 关闭语言模块，释放所有已加载的字符串资源。
 */
void Lang_Shutdown(void);

/**
 * @brief 根据提供的键（Key）获取翻译后的字符串。
 *
 * @param key 字符串的唯一标识符。
 * @return const char* 返回翻译后的字符串。如果找不到翻译，则返回键本身。
 */
const char* Lang_GetString(const char* key);

/**
 * @brief Lang_GetString 的便捷宏，使代码更简洁。
 */
#define L(key) Lang_GetString(key)

#endif // LANG_H