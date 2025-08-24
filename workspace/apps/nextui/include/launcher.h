#ifndef LAUNCHER_H
#define LAUNCHER_H

#include "datastructures.h"
#include <sdl.h>

// --- Launcher Functions ---
void queueNext(char* cmd);
void openPak(char* path);
void openRom(char* path, char* last);

// --- Game State & Resume ---
void readyResumePath(char* rom_path, int type);
void readyResume(Entry* entry);
int autoResume(void);

// --- Recents & Last Played ---
void addRecent(char* path, char* alias);
void saveRecents(void);
void saveLast(char* path);
void loadLast(void);

// --- Graphics Transition ---
void GFX_showLauncherTransition(SDL_Surface* screen, Entry* entry);

#endif // LAUNCHER_H