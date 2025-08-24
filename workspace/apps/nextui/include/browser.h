#ifndef BROWSER_H
#define BROWSER_H

#include "datastructures.h"
#include "defines.h" 
#include "globals.h"

// This global is defined in nextui.c and used by functions in browser.c
#define MAX_RECENTS 24 // a multiple of all menu rows

// Function Prototypes
int hasEmu(char* emu_name);
int hasCue(char* dir_path, char* cue_path);
int hasM3u(char* rom_path, char* m3u_path);
int hasRecents(void);
int hasCollections(void);
int hasRoms(char* dir_name);
int hasTools(void);
Entry* entryFromPakName(char* pak_name);
Directory* Directory_new(char* path, int selected);
void Directory_index(Directory* self);
int getFirstDisc(char* m3u_path, char* disc_path);
void getUniqueName(Entry* entry, char* out_name);
int isConsoleDir(char* path);
Entry* entryFromRecent(Recent* recent);

#endif // BROWSER_H
