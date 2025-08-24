#ifndef GLOBALS_H
#define GLOBALS_H

#include "datastructures.h"

// --- UI State Globals ---
extern Directory* top;
extern Array* stack;           // DirectoryArray
extern Array* recents;         // RecentArray
extern Array* quick;           // EntryArray for Quick Menu
extern Array* quickActions;    // EntryArray for Quick Toggles

// --- System State Globals ---
extern int quit;
extern int simple_mode;

// --- Launcher/Resume State Globals ---
extern int can_resume;
extern int should_resume;
extern int has_preview;
extern char slot_path[256];
extern char preview_path[256];
extern int startgame;
extern char* recent_alias; // yiiikes

// --- Navigation/Restore State Globals ---
extern int restore_depth;
extern int restore_relative;
extern int restore_selected;
extern int restore_start;
extern int restore_end;

#endif // GLOBALS_H