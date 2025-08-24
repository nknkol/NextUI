#ifndef UIMANAGER_H
#define UIMANAGER_H

#include "datastructures.h"
#include "sdl.h" // For SDL_Surface needed in Entry_open
#include <limits.h>

// --- Lifecycle Functions ---
void Menu_init(void);
void Menu_quit(void);

// --- Navigation Functions ---
void openDirectory(char* path, int auto_launch);
void closeDirectory(void);
Array* pathToStack(const char* path);

// --- Action Functions ---
void toggleQuick(Entry* self);
void Entry_open(SDL_Surface* screen, int* dirty, Entry* self);


#endif // UIMANAGER_H