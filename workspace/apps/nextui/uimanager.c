#include "uimanager.h"
#include "globals.h"
#include "browser.h"
#include "api.h"
#include "config.h"
#include "defines.h"
#include "launcher.h"

// --- Forward declarations for static functions ---
static Array* getQuickEntries(void);
static Array* getQuickToggles(void);
static void QuickMenu_init(void);
static void QuickMenu_quit(void);
static bool isDirectSubdirectory(const Directory* parent, const char* child_path);

// --- Launcher function prototypes (will be moved later) ---
void openRom(char* path, char* last);
void openPak(char* path);
void saveLast(char* path);
void loadLast(void);

// --- Quick Menu Logic ---

static Array* getQuickEntries(void) {
	Array* entries = Array_new();

	// We assume Menu_init was already called and populated this
	if (CFG_getShowRecents() && recents->count)
		Array_push(entries, Entry_newNamed(FAUX_RECENT_PATH, ENTRY_DIR, "Recents"));

	if (hasCollections())
		Array_push(entries, Entry_new(COLLECTIONS_PATH, ENTRY_DIR));

	// Not sure we need this, its just a button press away (B)
	Array_push(entries, Entry_newNamed(ROMS_PATH, ENTRY_DIR, "Games"));

	// Add tools if applicable
    if (hasTools() && !simple_mode) {
		char tools_path[256];
		// 调整：snprintf(tools_path, sizeof(tools_path), "%s/Tools/%s", SDCARD_PATH, PLATFORM);
		snprintf(tools_path, sizeof(tools_path), "%s/Tools", SDCARD_PATH);
        Array_push(entries, Entry_new(tools_path, ENTRY_DIR));
    }

	return entries;
}

static Array* getQuickToggles(void) {
	Array *entries = Array_new();

	Entry *settings = entryFromPakName("Settings");
	if (settings)
		Array_push(entries, settings);
	
	Entry *store = entryFromPakName("Pak Store");
	if (store)
		Array_push(entries, store);

	// quick actions
	if(WIFI_supported())
		Array_push(entries, Entry_new("Wifi", ENTRY_DIP));
	if(PLAT_supportsDeepSleep() && !simple_mode)
		Array_push(entries, Entry_new("Sleep", ENTRY_DIP));
	Array_push(entries, Entry_new("Reboot", ENTRY_DIP));
	Array_push(entries, Entry_new("Poweroff", ENTRY_DIP));

	return entries;
}

static void QuickMenu_init(void) {
	quick = getQuickEntries();
	quickActions = getQuickToggles();
}
static void QuickMenu_quit(void) {
	EntryArray_free(quick);
	EntryArray_free(quickActions);
}

// --- Lifecycle Management ---

void Menu_init(void) {
	stack = Array_new();
	recents = Array_new();
	PLUGINS_init(); // <-- 修改为调用公共函数

	openDirectory(SDCARD_PATH, 0);
	loadLast();

	QuickMenu_init();
}
void Menu_quit(void) {
	RecentArray_free(recents);
	DirectoryArray_free(stack);
	PLUGINS_quit(); // <-- 修改为调用公共函数

	QuickMenu_quit();
}

// --- Navigation ---

static bool isDirectSubdirectory(const Directory* parent, const char* child_path) {
    const char* parent_path = parent->path;

    size_t parent_len = strlen(parent_path);
    size_t child_len = strlen(child_path);

    // Child must be longer than parent to be a subdirectory
    if (child_len <= parent_len || strncmp(child_path, parent_path, parent_len) != 0) {
        return false;
    }

    // Next char after parent path must be '/'
    if (child_path[parent_len] != '/') return false;

    // Walk through the child path after parent, skipping PLATFORM segments
    const char* cursor = child_path + parent_len + 1; // skip the slash

    int levels = 0;
    while (*cursor) {
        const char* next = strchr(cursor, '/');
        size_t segment_len = next ? (size_t)(next - cursor) : strlen(cursor);

        if (segment_len == 0) break;

        // Copy segment into a buffer to compare
        char segment[PATH_MAX];
        if (segment_len >= PATH_MAX) return false;
        strncpy(segment, cursor, segment_len);
        segment[segment_len] = '\0';

        // Count level only if it's not PLATFORM
        if (strcmp(segment, PLATFORM) != 0 && strcmp(segment, "Roms") != 0) {
            levels++;
        }

        if (!next) break;
        cursor = next + 1;
    }

    return (levels == 1);  // exactly one meaningful level deeper
}

Array* pathToStack(const char* path) {
	Array* array = Array_new();

	if (!path || strlen(path) == 0) return array;

	if (!prefixMatch(SDCARD_PATH, path)) return array;

	// Always include root directory
	Directory* root_dir = Directory_new(SDCARD_PATH, 0);
	root_dir->start = 0;
	root_dir->end = (root_dir->entries->count < MAIN_ROW_COUNT) ? root_dir->entries->count : MAIN_ROW_COUNT;
	Array_push(array, root_dir);

	if (exactMatch(path, SDCARD_PATH)) return array;

	char temp_path[PATH_MAX];
	strcpy(temp_path, SDCARD_PATH);
	size_t current_len = strlen(SDCARD_PATH);

	const char* cursor = path + current_len;
	if (*cursor == '/') cursor++;

	while (*cursor) {
		const char* next = strchr(cursor, '/');
		size_t segment_len = next ? (size_t)(next - cursor) : strlen(cursor);
		if (segment_len == 0 || segment_len >= PATH_MAX) break;

		char segment[PATH_MAX];
		strncpy(segment, cursor, segment_len);
		segment[segment_len] = '\0';

		// Append '/' if needed
		if (temp_path[current_len - 1] != '/') {
			if (current_len + 1 >= PATH_MAX) break;
			temp_path[current_len++] = '/';
			temp_path[current_len] = '\0';
		}

		// Append segment
		if (current_len + segment_len >= PATH_MAX) break;
		strcat(temp_path, segment);
		current_len += segment_len;

		if (strcmp(segment, PLATFORM) == 0) {
			// Merge with previous directory
			if (array->count > 0) {
				// Remove the previous directory
				Directory* last = (Directory*)array->items[array->count - 1];
				Array_pop(array);
				Directory_free(last); // assuming you have a Directory_free

				// Replace with updated one using combined path
				Directory* merged = Directory_new(temp_path, 0);
				merged->start = 0;
				merged->end = (merged->entries->count < MAIN_ROW_COUNT) ? merged->entries->count : MAIN_ROW_COUNT;
				Array_push(array, merged);
			}
		} else {
			Directory* dir = Directory_new(temp_path, 0);
			dir->start = 0;
			dir->end = (dir->entries->count < MAIN_ROW_COUNT) ? dir->entries->count : MAIN_ROW_COUNT;
			Array_push(array, dir);
		}

		if (!next) break;
		cursor = next + 1;
	}

	return array;
}

void openDirectory(char* path, int auto_launch) {
	char auto_path[256];
	if (hasCue(path, auto_path) && auto_launch) {
		openRom(auto_path, path);
		return;
	}

	char m3u_path[256];
	strcpy(m3u_path, auto_path);
	char* tmp = strrchr(m3u_path, '.') + 1; // extension
	strcpy(tmp, "m3u"); // replace with m3u
	if (exists(m3u_path) && auto_launch) {
		auto_path[0] = '\0';
		if (getFirstDisc(m3u_path, auto_path)) {
			openRom(auto_path, path);
			return;
		}
		// TODO: doesn't handle empty m3u files
	}

	// If this is the exact same directory for some reason, just return.
	if(top && strcmp(top->path, path) == 0)
		return;

	// If this path is a direct subdirectory of top, push it on top of the stack
	// If it isnt, we need to recreate the stack to keep navigation consistent
	if(!top || isDirectSubdirectory(top, path)) {
		int selected = 0;
		int start = 0;
		int end = 0;
		if (top && top->entries->count>0) {
			if (restore_depth==stack->count && top->selected==restore_relative) {
				selected = restore_selected;
				start = restore_start;
				end = restore_end;
			}
		}

		top = Directory_new(path, selected);
		top->start = start;
		top->end = end ? end : ((top->entries->count<MAIN_ROW_COUNT) ? top->entries->count : MAIN_ROW_COUNT);
	
		Array_push(stack, top);
	}
	else {
		// construct a fresh stack by walking upwards until SDCARD_ROOT
		DirectoryArray_free(stack);

		stack = pathToStack(path);
		top = stack->items[stack->count - 1];
	}
}

void closeDirectory(void) {
	restore_selected = top->selected;
	restore_start = top->start;
	restore_end = top->end;
	DirectoryArray_pop(stack);
	restore_depth = stack->count;
	top = stack->items[stack->count-1];
	restore_relative = top->selected;
}

// --- Action Implementations ---
void toggleQuick(Entry* self)
{
	if(!self)
		return;
	if(!strcmp(self->name, "Wifi")) {
		WIFI_enable(!WIFI_enabled());
	}
	else if(!strcmp(self->name, "Sleep")) {
		PWR_sleep();
	}
	else if(!strcmp(self->name, "Reboot")) {
		PWR_powerOff(1);
	}
	else if(!strcmp(self->name, "Poweroff")) {
		PWR_powerOff(0);
	}
}

void Entry_open(SDL_Surface* screen, int* dirty, Entry* self) {
	recent_alias = self->name;  // yiiikes
	if (self->type==ENTRY_ROM || self->type==ENTRY_PAK) {
		startgame = 1;
		GFX_showLauncherTransition(screen, self); // 传递 screen
		if (self->type==ENTRY_ROM) {
			char *last = NULL;
			if (prefixMatch(COLLECTIONS_PATH, top->path)) {
				char* tmp;
				char filename[256];
				
				tmp = strrchr(self->path, '/');
				if (tmp) strcpy(filename, tmp+1);
				
				char last_path[256];
				sprintf(last_path, "%s/%s", top->path, filename);
				last = last_path;
			}
			openRom(self->path, last);
		}
		else { // ENTRY_PAK
			openPak(self->path);
		}
	}
	else if (self->type==ENTRY_PLUGIN) { 
		NextUI_Plugin* plugin = PLUGIN_load(self->path);
		if (plugin) {
			GFX_clearLayers(LAYER_ALL);
			// GFX_clear(screen); // 传递 screen
			// GFX_flip(screen);

			if (plugin->init(screen) == 0) {
				plugin->run();
			}
			plugin->quit();

			*dirty = 1; // 通过指针修改 dirty
			PAD_reset();
		}
	}
	else if (self->type==ENTRY_DIR) {
		openDirectory(self->path, 1);
	}
	else if (self->type==ENTRY_DIP) {
		toggleQuick(self);
	}
}