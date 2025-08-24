#include <stdbool.h>
#include <unistd.h>
#include "async.h"
#include "api.h"
#include "config.h"

// --- Private Structs ---
typedef struct TaskNode {
    LoadBackgroundTask* task;
    struct TaskNode* next;
} TaskNode;
typedef struct AnimTaskNode {
	AnimTask* task;
    struct AnimTaskNode* next;
} AnimTaskNode;

// --- Module-internal Global Variables ---
static TaskNode* taskBGQueueHead = NULL;
static TaskNode* taskBGQueueTail = NULL;
static TaskNode* taskThumbQueueHead = NULL;
static TaskNode* taskThumbQueueTail = NULL;
static AnimTaskNode* animTaskQueueHead = NULL;
static AnimTaskNode* animTtaskQueueTail = NULL;
static SDL_mutex* bgqueueMutex = NULL;
static SDL_mutex* thumbqueueMutex = NULL;
static SDL_mutex* animqueueMutex = NULL;
static SDL_cond* bgqueueCond = NULL;
static SDL_cond* thumbqueueCond = NULL;
static SDL_cond* animqueueCond = NULL;

SDL_mutex* bgMutex = NULL;
SDL_mutex* thumbMutex = NULL;
SDL_mutex* animMutex = NULL;
static SDL_Surface* screen = NULL; // Must be assigned via initImageLoaderPool

static int currentBGQueueSize = 0;
static int currentThumbQueueSize = 0;
static int currentAnimQueueSize = 0;
#define MAX_QUEUE_SIZE 1

// --- Global Variable Definitions (for extern) ---
SDL_mutex* frameMutex = NULL;
SDL_cond* flipCond = NULL;
SDL_Surface* folderbgbmp = NULL;
SDL_Surface* thumbbmp = NULL;
int needDraw = 0;
int folderbgchanged = 0;
int thumbchanged = 0;
int animationDraw = 1;
SDL_Rect pillRect;
SDL_Surface *globalpill = NULL;
SDL_Surface *globalText = NULL;
int pilltargetY = 0;
int pilltargetTextY = 0;
bool frameReady = true;
bool pillanimdone = false;
int ox = 0;
int oy = 0;
int had_thumb = 0;
int globallpillW = 0;


// --- Private Functions ---
static void enqueueBGTask(LoadBackgroundTask* task) {
	SDL_LockMutex(bgqueueMutex);
    TaskNode* node = (TaskNode*)malloc(sizeof(TaskNode));
    node->task = task;
    node->next = NULL;

    // If queue is full, drop the oldest task (head)
    if (currentBGQueueSize >= MAX_QUEUE_SIZE) {
        TaskNode* oldNode = taskBGQueueHead;
        if (oldNode) {
            taskBGQueueHead = oldNode->next;
            if (!taskBGQueueHead) {
                taskBGQueueTail = NULL;
            }
            if (oldNode->task) {
                free(oldNode->task);  // Only if task was malloc'd
            }
            free(oldNode);
            currentBGQueueSize--;
        }
    }

    // Enqueue the new task
    if (taskBGQueueTail) {
        taskBGQueueTail->next = node;
        taskBGQueueTail = node;
    } else {
        taskBGQueueHead = taskBGQueueTail = node;
    }
 
    currentBGQueueSize++;
    SDL_CondSignal(bgqueueCond);
    SDL_UnlockMutex(bgqueueMutex);
}

static void enqueueThumbTask(LoadBackgroundTask* task) {
	SDL_LockMutex(thumbqueueMutex);
    TaskNode* node = (TaskNode*)malloc(sizeof(TaskNode));
    node->task = task;
    node->next = NULL;

    // If queue is full, drop the oldest task (head)
    if (currentThumbQueueSize >= MAX_QUEUE_SIZE) {
        TaskNode* oldNode = taskThumbQueueHead;
        if (oldNode) {
            taskThumbQueueHead = oldNode->next;
            if (!taskThumbQueueHead) {
                taskThumbQueueTail = NULL;
            }
            if (oldNode->task) {
                free(oldNode->task);  // Only if task was malloc'd
            }
            free(oldNode);
            currentThumbQueueSize--;
        }
    }

    // Enqueue the new task
    if (taskThumbQueueTail) {
        taskThumbQueueTail->next = node;
        taskThumbQueueTail = node;
    } else {
        taskThumbQueueHead = taskThumbQueueTail = node;
    }
 
    currentThumbQueueSize++;
    SDL_CondSignal(thumbqueueCond);
    SDL_UnlockMutex(thumbqueueMutex);
}

static void enqueueanmimtask(AnimTask* task) {
    AnimTaskNode* node = (AnimTaskNode*)malloc(sizeof(AnimTaskNode));
    node->task = task;
    node->next = NULL;
	
    SDL_LockMutex(animqueueMutex);
	pillanimdone = false;
    // If queue is full, drop the oldest task (head)
    if (currentAnimQueueSize >= 1) {
        AnimTaskNode* oldNode = animTaskQueueHead;
        if (oldNode) {
            animTaskQueueHead = oldNode->next;
            if (!animTaskQueueHead) {
                animTtaskQueueTail = NULL;
            }
            if (oldNode->task) {
                free(oldNode->task);  // Only if task was malloc'd
            }
            free(oldNode);
            currentAnimQueueSize--;
        }
    }

    // Enqueue the new task
    if (animTtaskQueueTail) {
        animTtaskQueueTail->next = node;
        animTtaskQueueTail = node;
    } else {
        animTaskQueueHead = animTtaskQueueTail = node;
    }

    currentAnimQueueSize++;
    SDL_CondSignal(animqueueCond);
    SDL_UnlockMutex(animqueueMutex);
}


// --- Worker Threads ---

static int BGLoadWorker(void* unused) {
    while (true) {
        SDL_LockMutex(bgqueueMutex);
        while (!taskBGQueueHead) {
        	SDL_CondWait(bgqueueCond, bgqueueMutex);
        }
        TaskNode* node = taskBGQueueHead;
        taskBGQueueHead = node->next;
        if (!taskBGQueueHead) taskBGQueueTail = NULL;
        SDL_UnlockMutex(bgqueueMutex);
		// give processor lil space in between queue items for other shit
		//SDL_Delay(100);
        LoadBackgroundTask* task = node->task;
        free(node);

        SDL_Surface* result = NULL;
        if (access(task->imagePath, F_OK) == 0) {
            SDL_Surface* image = IMG_Load(task->imagePath);
            if (image) {
                SDL_Surface* imageRGBA = SDL_ConvertSurfaceFormat(image, SDL_PIXELFORMAT_RGBA8888, 0);
                SDL_FreeSurface(image);
                result = imageRGBA;
            }
        }

        if (task->callback) {
			task->callback(result);
		}
        free(task);
		SDL_LockMutex(bgqueueMutex);
		if (!taskBGQueueHead) taskBGQueueTail = NULL;
		currentBGQueueSize--;  // <-- add this
		SDL_UnlockMutex(bgqueueMutex);
    }
    return 0;
}

static int ThumbLoadWorker(void* unused) {
    while (true) {
        SDL_LockMutex(thumbqueueMutex);
        while (!taskThumbQueueHead) {
        	SDL_CondWait(thumbqueueCond, thumbqueueMutex);
        }
        TaskNode* node = taskThumbQueueHead;
        taskThumbQueueHead = node->next;
        if (!taskThumbQueueHead) taskThumbQueueTail = NULL;
        SDL_UnlockMutex(thumbqueueMutex);
		// give processor lil space in between queue items for other shit
		//SDL_Delay(100);
        LoadBackgroundTask* task = node->task;
        free(node);

        SDL_Surface* result = NULL;
        if (access(task->imagePath, F_OK) == 0) {
            SDL_Surface* image = IMG_Load(task->imagePath);
            if (image) {
                SDL_Surface* imageRGBA = SDL_ConvertSurfaceFormat(image, SDL_PIXELFORMAT_RGBA8888, 0);
                SDL_FreeSurface(image);
                result = imageRGBA;
            }
        }

        if (task->callback) {
			task->callback(result);
		}
        free(task);
		SDL_LockMutex(thumbqueueMutex);
		if (!taskThumbQueueHead) taskThumbQueueTail = NULL;
		currentThumbQueueSize--;  // <-- add this
		SDL_UnlockMutex(thumbqueueMutex);
    }
    return 0;
}

static int animWorker(void* unused) {
	  while (true) {
 		SDL_LockMutex(animqueueMutex);
        while (!animTaskQueueHead) {
            SDL_CondWait(animqueueCond, animqueueMutex);
        }
        AnimTaskNode* node = animTaskQueueHead;
        animTaskQueueHead = node->next;
        if (!animTaskQueueHead) animTtaskQueueTail = NULL;
		SDL_UnlockMutex(animqueueMutex);

        AnimTask* task = node->task;
		finishedTask* finaltask = (finishedTask*)malloc(sizeof(finishedTask));
		int total_frames = task->frames;
		// This somehow leads to the pill not rendering correctly when wrapping the list (last element to first, or reverse).
		// TODO: Figure out why this is here. Ideally we shouldnt refer to specific platforms in here, but the commit message doesnt
		// help all that much and comparing magic numbers also isnt that descriptive on its own.
		if(strcmp("Desktop", PLAT_getModel()) != 0) {
			if(task->targetY > task->startY + SCALE1(PILL_SIZE) || task->targetY < task->startY - SCALE1(PILL_SIZE)) {
				total_frames = 0;
			}
		}
			
		for (int frame = 0; frame <= total_frames; frame++) {
			float t = (float)frame / total_frames;
			if (t > 1.0f) t = 1.0f;

			int current_x = task->startX + (int)((task->targetX - task->startX) * t);
			int current_y = task->startY + (int)(( task->targetY -  task->startY) * t);
			
			SDL_Rect moveDst = { current_x, current_y, task->move_w, task->move_h };
			finaltask->dst = moveDst;
			finaltask->entry_name = task->entry_name;
			finaltask->move_w = task->move_w;
			finaltask->move_h = task->move_h;
			finaltask->targetY = task->targetY;
			finaltask->targetTextY = task->targetTextY;
			finaltask->move_y = SCALE1(PADDING + task->targetY+4);
			finaltask->done = 0;
			if(frame >= total_frames) finaltask->done=1;
			task->callback(finaltask);
			SDL_LockMutex(frameMutex);
			while (!frameReady) {
				SDL_CondWait(flipCond, frameMutex);
			}
			frameReady = false;
			SDL_UnlockMutex(frameMutex);
			
		}
		SDL_LockMutex(animqueueMutex);
		if (!animTaskQueueHead) animTtaskQueueTail = NULL;
		currentAnimQueueSize--;  // <-- add this
		SDL_UnlockMutex(animqueueMutex);
	
		SDL_LockMutex(animMutex);
		pillanimdone = true;
		free(finaltask);
		SDL_UnlockMutex(animMutex);
	}
}


// --- Callbacks and Public Functions ---

void onBackgroundLoaded(SDL_Surface* surface) {
	SDL_LockMutex(bgMutex);
	folderbgchanged = 1;
	if (folderbgbmp) SDL_FreeSurface(folderbgbmp);
    if (!surface) {
		folderbgbmp = NULL;
		SDL_UnlockMutex(bgMutex);
		return;
	}
    folderbgbmp = surface;
	needDraw = 1;
	SDL_UnlockMutex(bgMutex);
}

// void startLoadFolderBackground(const char* imagePath, BackgroundLoadedCallback callback, void* userData) {
//     LoadBackgroundTask* task = malloc(sizeof(LoadBackgroundTask));
//     if (!task) return;

//  	snprintf(task->imagePath, sizeof(task->imagePath), "%s", imagePath);
//     task->callback = callback;
//     task->userData = userData;
//     enqueueBGTask(task);
// }
void startLoadFolderBackground(const char* imagePath) { // 修改签名
    LoadBackgroundTask* task = malloc(sizeof(LoadBackgroundTask));
    if (!task) return;
 	snprintf(task->imagePath, sizeof(task->imagePath), "%s", imagePath);
    task->callback = onBackgroundLoaded; // 直接使用内部回调
    task->userData = NULL;
    enqueueBGTask(task);
}

void onThumbLoaded(SDL_Surface* surface) {
	SDL_LockMutex(thumbMutex);
	thumbchanged = 1;
	if (thumbbmp) SDL_FreeSurface(thumbbmp);
    if (!surface) {
		thumbbmp = NULL;
		SDL_UnlockMutex(thumbMutex);
		return;
	}
  
		
    thumbbmp = surface;
	int img_w = thumbbmp->w;
	int img_h = thumbbmp->h;
	double aspect_ratio = (double)img_h / img_w;
	int max_w = (int)(screen->w * CFG_getGameArtWidth()); 
	int max_h = (int)(screen->h * 0.6);  
	int new_w = max_w;
	int new_h = (int)(new_w * aspect_ratio); 
	
	if (new_h > max_h) {
		new_h = max_h;
		new_w = (int)(new_h / aspect_ratio);
	}
	GFX_ApplyRoundedCorners_RGBA8888(
		thumbbmp,
		&(SDL_Rect){0, 0, thumbbmp->w, thumbbmp->h},
		SCALE1((float)CFG_getThumbnailRadius() * ((float)img_w / (float)new_w))
	);
	needDraw = 1;
	SDL_UnlockMutex(thumbMutex);
}

// void startLoadThumb(const char* thumbpath, BackgroundLoadedCallback callback, void* userData) {
//     LoadBackgroundTask* task = malloc(sizeof(LoadBackgroundTask));
//     if (!task) return;

//     snprintf(task->imagePath, sizeof(task->imagePath), "%s", thumbpath);
//     task->callback = callback;
//     task->userData = userData;
//     enqueueThumbTask(task);
// }
void startLoadThumb(const char* thumbpath) { // 修改签名
    LoadBackgroundTask* task = malloc(sizeof(LoadBackgroundTask));
    if (!task) return;
    snprintf(task->imagePath, sizeof(task->imagePath), "%s", thumbpath);
    task->callback = onThumbLoaded; // 直接使用内部回调
    task->userData = NULL;
    enqueueThumbTask(task);
}

static void animcallback(finishedTask *task) {
	SDL_LockMutex(animMutex);
	pillRect = task->dst; 
	pilltargetY = +screen->w; // move offscreen
	if(task->done) {
		pilltargetY = task->targetY;
		pilltargetTextY = task->targetTextY;
		SDL_Color text_color = uintToColour(THEME_COLOR5_255);
		SDL_Surface *tmp = TTF_RenderUTF8_Blended(font.large, task->entry_name, text_color);

		SDL_Surface *converted = SDL_ConvertSurfaceFormat(tmp, SDL_PIXELFORMAT_RGBA8888, 0);
		SDL_FreeSurface(tmp); // tmp no longer needed

		SDL_Rect crop_rect = { 0, 0, task->move_w - SCALE1(BUTTON_PADDING * 2), converted->h };
		SDL_Surface *cropped = SDL_CreateRGBSurfaceWithFormat(
			0, crop_rect.w, crop_rect.h, 32, SDL_PIXELFORMAT_RGBA8888
		);
		if (!cropped) {
			SDL_FreeSurface(converted);
		}

		SDL_SetSurfaceBlendMode(converted, SDL_BLENDMODE_NONE); 
		SDL_BlitSurface(converted, &crop_rect, cropped, NULL);
		SDL_FreeSurface(converted);

		globalText = cropped;
	}
	needDraw = 1;
	SDL_UnlockMutex(animMutex);
	animationDraw = 1;
}

void animPill(AnimTask *task) {
	task->callback = animcallback;
	enqueueanmimtask(task);
}

void initImageLoaderPool(SDL_Surface* main_screen) {
    screen = main_screen; // Store the screen surface reference

    thumbqueueMutex = SDL_CreateMutex();
    bgqueueMutex = SDL_CreateMutex();
    animqueueMutex = SDL_CreateMutex();
    bgqueueCond = SDL_CreateCond();
    thumbqueueCond = SDL_CreateCond();
    animqueueCond = SDL_CreateCond();
	bgMutex = SDL_CreateMutex();
	thumbMutex = SDL_CreateMutex();
	animMutex = SDL_CreateMutex();
	frameMutex = SDL_CreateMutex();
	flipCond = SDL_CreateCond();

    SDL_CreateThread(BGLoadWorker, "BGLoadWorker", NULL);
    SDL_CreateThread(ThumbLoadWorker, "ThumbLoadWorker", NULL);
	SDL_CreateThread(animWorker, "animWorker", NULL);
}
// void initImageLoaderPool() {
//     thumbqueueMutex = SDL_CreateMutex();
//     bgqueueMutex = SDL_CreateMutex();
//     bgqueueCond = SDL_CreateCond();
//     thumbqueueCond = SDL_CreateCond();
// 	bgMutex = SDL_CreateMutex();
// 	thumbMutex = SDL_CreateMutex();
// 	animMutex = SDL_CreateMutex();
// 	animqueueMutex = SDL_CreateMutex();
// 	animqueueCond = SDL_CreateCond();
// 	frameMutex = SDL_CreateMutex();
// 	flipCond = SDL_CreateCond();

//     SDL_CreateThread(BGLoadWorker, "BGLoadWorker", NULL);
//     SDL_CreateThread(ThumbLoadWorker, "ThumbLoadWorker", NULL);
// 	SDL_CreateThread(animWorker, "animWorker", NULL);
// }