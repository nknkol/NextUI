#ifndef ASYNC_H
#define ASYNC_H

#include <sdl.h>
#include <stdbool.h>
#include "datastructures.h"
#include "defines.h" // For MAX_PATH

// --- Structs and Typedefs ---

typedef void (*BackgroundLoadedCallback)(SDL_Surface* surface);

typedef struct {
    char imagePath[MAX_PATH];
    BackgroundLoadedCallback callback;
    void* userData;
} LoadBackgroundTask;

typedef struct finishedTask {
	int startX;
	int targetX;
	int startY;
	int targetY;
	int targetTextY;
	int move_y;
	int move_w; 
	int move_h;
	int frames;
	int done;
	void* userData;
	char *entry_name;
	SDL_Rect dst;
} finishedTask;

typedef void (*AnimTaskCallback)(finishedTask *task);

typedef struct AnimTask {
	int startX;
	int targetX;
	int startY;
	int targetY;
	int targetTextY;
	int move_w; 
	int move_h;
	int frames;
	AnimTaskCallback callback;
	void* userData;
	char *entry_name;
	SDL_Rect dst;
} AnimTask;

// --- Global State Variables (extern) ---
// These are defined in async.c but needed by the main loop in nextui.c

extern SDL_mutex* frameMutex;
extern SDL_cond* flipCond;
extern SDL_Surface* folderbgbmp;
extern SDL_Surface* thumbbmp;
extern int needDraw;
extern int folderbgchanged;
extern int thumbchanged;
extern int animationDraw;
extern SDL_Rect pillRect;
extern SDL_Surface *globalpill;
extern SDL_Surface *globalText;
extern int pilltargetY;
extern int pilltargetTextY;
extern bool frameReady;
extern bool pillanimdone;
extern int ox;
extern int oy;
extern int had_thumb;
extern int globallpillW;

extern SDL_mutex* bgMutex;
extern SDL_mutex* thumbMutex;
extern SDL_mutex* animMutex;


// --- Public Functions ---
void initImageLoaderPool(SDL_Surface* main_screen);
void startLoadFolderBackground(const char* imagePath);
void startLoadThumb(const char* thumbpath);
void animPill(AnimTask *task);

#endif // ASYNC_H