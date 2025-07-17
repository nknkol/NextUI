#include "lang.h"
#include "defines.h" // 需要这个来获取 SDCARD_PATH
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LANG_STRINGS 512
#define MAX_LINE_LENGTH 256

// 用于存储单个键值对
typedef struct {
    char* key;
    char* value;
} LangEntry;

// 全局变量来存储所有加载的字符串
static LangEntry g_lang_entries[MAX_LANG_STRINGS];
static int g_lang_entry_count = 0;

// 内部辅助函数：移除字符串首尾的空白字符
static void trim_whitespace(char* str) {
    if (!str) return;
    char* start = str;
    while (isspace((unsigned char)*start)) {
        start++;
    }

    char* end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) {
        end--;
    }
    
    end[1] = '\0';
    
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

void Lang_Shutdown(void) {
    for (int i = 0; i < g_lang_entry_count; i++) {
        free(g_lang_entries[i].key);
        free(g_lang_entries[i].value);
    }
    g_lang_entry_count = 0;
}

void Lang_Init(const char* lang_code) {
    // 先清理旧的语言数据
    Lang_Shutdown();

    char file_path[MAX_PATH];
    
    // <<< 修改后的行
    // 使用 SYSTEM_PATH 宏来构建新的语言文件路径
    snprintf(file_path, sizeof(file_path), "%s/lang/%s.ini", SYSTEM_PATH, lang_code);

    FILE* file = fopen(file_path, "r");
    if (!file) {
        fprintf(stderr, "Warning: Could not open language file: %s.\n", file_path);
        // 在实际应用中，这里可以尝试加载一个默认语言，例如 "en"
        // if (strcmp(lang_code, "en") != 0) Lang_Init("en");
        return;
    }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file) && g_lang_entry_count < MAX_LANG_STRINGS) {
        // 跳过注释和空行
        if (line[0] == '#' || line[0] == '\n' || line[0] == '[') continue;

        char* separator = strchr(line, '=');
        if (separator) {
            *separator = '\0'; // 分割键和值
            char* key = line;
            char* value = separator + 1;

            // 清理可能存在的回车换行符
            value[strcspn(value, "\r\n")] = 0;

            // 清理首尾空白
            trim_whitespace(key);
            trim_whitespace(value);
            
            if(strlen(key) > 0) {
                g_lang_entries[g_lang_entry_count].key = strdup(key);
                g_lang_entries[g_lang_entry_count].value = strdup(value);
                g_lang_entry_count++;
            }
        }
    }

    fclose(file);
}

const char* Lang_GetString(const char* key) {
    if (!key) return "";
    // 线性搜索键 (对于数百个字符串来说性能足够)
    for (int i = 0; i < g_lang_entry_count; i++) {
        if (strcmp(g_lang_entries[i].key, key) == 0) {
            return g_lang_entries[i].value;
        }
    }
    // 如果找不到，返回键本身，这样方便调试
    return key;
}