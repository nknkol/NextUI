#include <unistd.h>
#include <libgen.h>
#include "launcher.h"
#include "globals.h"
#include "defines.h"
#include "utils.h"
#include "api.h"
#include "browser.h"
#include "uimanager.h"

// based on https://stackoverflow.com/a/31775567/145965
static int replaceString(char *line, const char *search, const char *replace) {
   char *sp; // start of pattern
   if ((sp = strstr(line, search)) == NULL) {
      return 0;
   }
   int count = 1;
   int sLen = strlen(search);
   int rLen = strlen(replace);
   if (sLen > rLen) {
      // move from right to left
      char *src = sp + sLen;
      char *dst = sp + rLen;
      while((*dst = *src) != '\0') { dst++; src++; }
   } else if (sLen < rLen) {
      // move from left to right
      int tLen = strlen(sp) - sLen;
      char *stop = sp + rLen;
      char *src = sp + sLen + tLen;
      char *dst = sp + rLen + tLen;
      while(dst >= stop) { *dst = *src; dst--; src--; }
   }
   memcpy(sp, replace, rLen);
   count += replaceString(sp + rLen, search, replace);
   return count;
}
static char* escapeSingleQuotes(char* str) {
	// why not call replaceString directly?
	// call points require the modified string be returned
	// but replaceString is recursive and depends on its
	// own return value (but does it need to?)
	replaceString(str, "'", "'\\''");
	return str;
}

void queueNext(char* cmd) {
	LOG_info("cmd: %s\n", cmd);
	putFile("/tmp/next", cmd);
	quit = 1;
}

void saveRecents(void) {
	FILE* file = fopen(RECENT_PATH, "w");
	if (file) {
		for (int i=0; i<recents->count; i++) {
			Recent* recent = recents->items[i];
			fputs(recent->path, file);
			if (recent->alias) {
				fputs("\t", file);
				fputs(recent->alias, file);
			}
			putc('\n', file);
		}
		fclose(file);
	}
}

void addRecent(char* path, char* alias) {
	path += strlen(SDCARD_PATH); // makes paths platform agnostic
	int id = RecentArray_indexOf(recents, path);
	if (id==-1) { // add
		while (recents->count>=MAX_RECENTS) {
			Recent_free(Array_pop(recents));
		}
		Array_unshift(recents, Recent_new(path, alias));
	}
	else if (id>0) { // bump to top
		for (int i=id; i>0; i--) {
			void* tmp = recents->items[i-1];
			recents->items[i-1] = recents->items[i];
			recents->items[i] = tmp;
		}
	}
	saveRecents();
}


void readyResumePath(char* rom_path, int type) {
	char* tmp;
	can_resume = 0;
	has_preview = 0;
	char path[256];
	strcpy(path, rom_path);
	
	if (!prefixMatch(ROMS_PATH, path)) return;
	
	char auto_path[256];
	if (type==ENTRY_DIR) {
		if (!hasCue(path, auto_path)) { // no cue?
			tmp = strrchr(auto_path, '.') + 1; // extension
			strcpy(tmp, "m3u"); // replace with m3u
			if (!exists(auto_path)) return; // no m3u
		}
		strcpy(path, auto_path); // cue or m3u if one exists
	}
	
	if (!suffixMatch(".m3u", path)) {
		char m3u_path[256];
		if (hasM3u(path, m3u_path)) {
			// change path to m3u path
			strcpy(path, m3u_path);
		}
	}
	
	char emu_name[256];
	getEmuName(path, emu_name);
	
	char rom_file[256];
	tmp = strrchr(path, '/') + 1;
	strcpy(rom_file, tmp);
	
	sprintf(slot_path, "%s/.minui/%s/%s.txt", SHARED_USERDATA_PATH, emu_name, rom_file); // /.userdata/.minui/<EMU>/<romname>.ext.txt
	sprintf(preview_path, "%s/.minui/%s/%s.0.bmp", SHARED_USERDATA_PATH, emu_name, rom_file); // /.userdata/.minui/<EMU>/<romname>.ext.0.bmp
	
	can_resume = exists(slot_path);
	has_preview = exists(preview_path);

}

void readyResume(Entry* entry) {
	readyResumePath(entry->path, entry->type);
}

int autoResume(void) {
	// NOTE: bypasses recents

	if (!exists(AUTO_RESUME_PATH)) return 0;
	
	char path[256];
	getFile(AUTO_RESUME_PATH, path, 256);
	unlink(AUTO_RESUME_PATH);
	sync();
	
	// make sure rom still exists
	char sd_path[256];
	sprintf(sd_path, "%s%s", SDCARD_PATH, path);
	if (!exists(sd_path)) return 0;
	
	// make sure emu still exists
	char emu_name[256];
	getEmuName(sd_path, emu_name);
	
	char emu_path[256];
	getEmuPath(emu_name, emu_path);
	
	if (!exists(emu_path)) return 0;
	
	// putFile(LAST_PATH, FAUX_RECENT_PATH); // saveLast() will crash here because top is NULL

	char act[256];
	sprintf(act, "gametimectl.elf start '%s'", escapeSingleQuotes(sd_path));
	system(act);
	
	char cmd[256];
	// dont escape sd_path again because it was already escaped for gametimectl and function modifies input str aswell
	sprintf(cmd, "'%s' '%s'", escapeSingleQuotes(emu_path), sd_path);
	putInt(RESUME_SLOT_PATH, AUTO_RESUME_SLOT);
	queueNext(cmd);
	return 1;
}

void openPak(char* path) {
	// NOTE: escapeSingleQuotes() modifies the passed string 
	// so we need to save the path before we call that
	// if (prefixMatch(ROMS_PATH, path)) {
	// 	addRecent(path);
	// }
	saveLast(path);
	
	char cmd[256];
	sprintf(cmd, "'%s/launch.sh'", escapeSingleQuotes(path));
	queueNext(cmd);
}

void openRom(char* path, char* last) {
	LOG_info("openRom(%s,%s)\n", path, last);
	
	char sd_path[256];
	strcpy(sd_path, path);
	
	char m3u_path[256];
	int has_m3u = hasM3u(sd_path, m3u_path);
	
	char recent_path[256];
	strcpy(recent_path, has_m3u ? m3u_path : sd_path);
	
	if (has_m3u && suffixMatch(".m3u", sd_path)) {
		getFirstDisc(m3u_path, sd_path);
	}

	char emu_name[256];
	getEmuName(sd_path, emu_name);

	if (should_resume) {
		char slot[16];
		getFile(slot_path, slot, 16);
		putFile(RESUME_SLOT_PATH, slot);
		should_resume = 0;

		if (has_m3u) {
			char rom_file[256];
			strcpy(rom_file, strrchr(m3u_path, '/') + 1);
			
			// get disc for state
			char disc_path_path[256];
			sprintf(disc_path_path, "%s/.minui/%s/%s.%s.txt", SHARED_USERDATA_PATH, emu_name, rom_file, slot); // /.userdata/arm-480/.minui/<EMU>/<romname>.ext.0.txt

			if (exists(disc_path_path)) {
				// switch to disc path
				char disc_path[256];
				getFile(disc_path_path, disc_path, 256);
				if (disc_path[0]=='/') strcpy(sd_path, disc_path); // absolute
				else { // relative
					strcpy(sd_path, m3u_path);
					char* tmp = strrchr(sd_path, '/') + 1;
					strcpy(tmp, disc_path);
				}
			}
		}
	}
	else putInt(RESUME_SLOT_PATH,8); // resume hidden default state
	
	char emu_path[256];
	getEmuPath(emu_name, emu_path);
	
	// NOTE: escapeSingleQuotes() modifies the passed string 
	// so we need to save the path before we call that
	addRecent(recent_path, recent_alias); // yiiikes
	saveLast(last==NULL ? sd_path : last);
	char act[256];
	sprintf(act, "gametimectl.elf start '%s'", escapeSingleQuotes(sd_path));
	system(act);
	char cmd[256];
	// dont escape sd_path again because it was already escaped for gametimectl and function modifies input str aswell
	sprintf(cmd, "'%s' '%s'", escapeSingleQuotes(emu_path), sd_path);
	queueNext(cmd);
}

void saveLast(char* path) {
	// special case for recently played
	if (exactMatch(top->path, FAUX_RECENT_PATH)) {
		// NOTE: that we don't have to save the file because
		// your most recently played game will always be at
		// the top which is also the default selection
		path = FAUX_RECENT_PATH;
	}
	putFile(LAST_PATH, path);
}

void loadLast(void) { // call after loading root directory
	if (!exists(LAST_PATH)) return;

	char last_path[256];
	getFile(LAST_PATH, last_path, 256);
	
	char full_path[256];
	strcpy(full_path, last_path);
	
	char* tmp;
	char filename[256];
	tmp = strrchr(last_path, '/');
	if (tmp) strcpy(filename, tmp);
	
	Array* last = Array_new();
	while (!exactMatch(last_path, SDCARD_PATH)) {
		Array_push(last, strdup(last_path));
		
		char* slash = strrchr(last_path, '/');
		last_path[(slash-last_path)] = '\0';
	}
	
	while (last->count>0) {
		char* path = Array_pop(last);
		if (!exactMatch(path, ROMS_PATH)) { // romsDir is effectively root as far as restoring state after a game
			char collated_path[256];
			collated_path[0] = '\0';
			if (suffixMatch(")", path) && isConsoleDir(path)) {
				strcpy(collated_path, path);
				tmp = strrchr(collated_path, '(');
				if (tmp) tmp[1] = '\0'; // 1 because we want to keep the opening parenthesis to avoid collating "Game Boy Color" and "Game Boy Advance" into "Game Boy"
			}
			
			for (int i=0; i<top->entries->count; i++) {
				Entry* entry = top->entries->items[i];
			
				// NOTE: strlen() is required for collated_path, '\0' wasn't reading as NULL for some reason
				if (exactMatch(entry->path, path) || (strlen(collated_path) && prefixMatch(collated_path, entry->path)) || (prefixMatch(COLLECTIONS_PATH, full_path) && suffixMatch(filename, entry->path))) {
					top->selected = i;
					if (i>=top->end) {
						top->start = i;
						top->end = top->start + MAIN_ROW_COUNT;
						if (top->end>top->entries->count) {
							top->end = top->entries->count;
							top->start = top->end - MAIN_ROW_COUNT;
						}
					}
					if (last->count==0 && !exactMatch(entry->path, FAUX_RECENT_PATH) && !(!exactMatch(entry->path, COLLECTIONS_PATH) && prefixMatch(COLLECTIONS_PATH, entry->path))) break; // don't show contents of auto-launch dirs
				
					if (entry->type==ENTRY_DIR) {
						openDirectory(entry->path, 0);
						break;
					}
				}
			}
		}
		free(path); // we took ownership when we popped it
	}
	
	StringArray_free(last);

	if (top->selected >= 0 && top->selected < top->entries->count) {
		Entry *selected_entry = top->entries->items[top->selected];
		readyResume(selected_entry);
	}
}

void GFX_showLauncherTransition(SDL_Surface* screen, Entry* entry) {
	GFX_clearLayers(LAYER_ALL);
    GFX_clear(screen);

    char icon_path[MAX_PATH];
    char* filename = strrchr(entry->path, '/') + 1;
    
    // 创建一个副本以安全地修改
    char filename_copy[256];
    strncpy(filename_copy, filename, sizeof(filename_copy) - 1);
    filename_copy[sizeof(filename_copy) - 1] = '\0';

    char* dot = strrchr(filename_copy, '.');
    if (dot) *dot = '\0'; // a.pak -> a

    char dirname_copy[MAX_PATH];
    strncpy(dirname_copy, entry->path, sizeof(dirname_copy) - 1);
    dirname_copy[sizeof(dirname_copy) - 1] = '\0';

    sprintf(icon_path, "%s/.media/%s.png", dirname(dirname_copy), filename_copy);

    SDL_Surface* icon = NULL;
    if (exists(icon_path)) {
        icon = IMG_Load(icon_path);
    }

    if (icon) {
        int x = (screen->w - icon->w) / 2;
        int y = (screen->h - icon->h) / 2;
        SDL_BlitSurface(icon, NULL, screen, &(SDL_Rect){x,y});
        SDL_FreeSurface(icon);
    }

    char loading_text[256];
    sprintf(loading_text, "Loading %s...", entry->name);
    GFX_blitMessage(font.small, loading_text, screen, &(SDL_Rect){0, screen->h - SCALE1(PILL_SIZE * 2), screen->w, SCALE1(PILL_SIZE)});

    GFX_flip(screen);
    SDL_Delay(500); // 短暂显示
}