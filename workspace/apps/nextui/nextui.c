#include <stdio.h>
#include <stdlib.h>
#include <msettings.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <sys/resource.h>
#include <pthread.h>
#include <assert.h>
#include <limits.h>

#include "defines.h"
#include "api.h"
#include "utils.h"
#include "config.h"
#include "lang.h"
#include "plugin.h"
#include "datastructures.h"
#include "browser.h"
#include "globals.h"
#include "uimanager.h"
#include "launcher.h"
#include "async.h" 
///////////////////////////////////////

char* recent_alias = NULL;

///////////////////////////////////////

Directory* top;
Array* stack; // DirectoryArray
Array* recents; // RecentArray
// static Array* plugins; // 新增：用于存放所有已加载的插件条目
Array* quick; // EntryArray
Array* quickActions; // EntryArray

int quit = 0;
int can_resume = 0;
int should_resume = 0; // set to 1 on BTN_RESUME but only if can_resume==1
int has_preview = 0;
int simple_mode = 0;
static int switcher_selected = 0;
char slot_path[256];
char preview_path[256];
static int animationdirection = 0;

int restore_depth = -1;
int restore_relative = -1;
int restore_selected = 0;
int restore_start = 0;
int restore_end = 0;
int startgame = 0;

static int dirty = 1;
static int remember_selection = 0;

enum {
	ANIM_NONE = 0,
	SLIDE_LEFT = 1,
	SLIDE_RIGHT = 2,
};

SDL_Rect pillRect;
SDL_Surface *globalpill;
SDL_Surface *globalText;
SDL_Surface* screen = NULL;
///////////////////////////////////////

int main (int argc, char *argv[]) {
	// LOG_info("time from launch to:\n");
	// unsigned long main_begin = SDL_GetTicks();
	// unsigned long first_draw = 0;
	
	if (autoResume()) return 0; // nothing to do
	
	simple_mode = exists(SIMPLE_MODE_PATH);

	LOG_info("NextUI\n");
	InitSettings();
	
	screen = GFX_init(MODE_MAIN);
	// LOG_info("- graphics init: %lu\n", SDL_GetTicks() - main_begin);
	static char folderBgPath[1024] = "";
	SDL_Surface *blackBG = NULL;

	blackBG = SDL_CreateRGBSurfaceWithFormat(0, screen->w, screen->h, 32, SDL_PIXELFORMAT_RGBA8888);
	SDL_FillRect(blackBG, NULL, SDL_MapRGBA(screen->format, 0, 0, 0, 255));

	PAD_init();
	// LOG_info("- input init: %lu\n", SDL_GetTicks() - main_begin);
	VIB_init();
	WIFI_init();
	PWR_init();
	if (!HAS_POWER_BUTTON && !simple_mode) PWR_disableSleep();
	// LOG_info("- power init: %lu\n", SDL_GetTicks() - main_begin);
	
	// start my threaded image loader :D
	initImageLoaderPool(screen);
	Menu_init();
	int qm_row = 0;
	int qm_col = 0;
	int qm_slot = 0;
	int qm_shift = 0;
	int qm_slots = QUICK_SWITCHER_COUNT > quick->count ? quick->count : QUICK_SWITCHER_COUNT;
	// LOG_info("- menu init: %lu\n", SDL_GetTicks() - main_begin);

	int lastScreen = SCREEN_OFF;
	int currentScreen = CFG_getDefaultView();

	if(exists(GAME_SWITCHER_PERSIST_PATH)) {
		// consider this "consumed", dont bring up the switcher next time we regularly exit a game
		unlink(GAME_SWITCHER_PERSIST_PATH);
		currentScreen = SCREEN_GAMESWITCHER;
	}

	// add a nice fade into the game switcher
	if(currentScreen == SCREEN_GAMESWITCHER)
		lastScreen = SCREEN_GAME;

	// make sure we have no running games logged as active anymore (we might be launching back into the UI here)
	system("gametimectl.elf stop_all");
	
	GFX_setVsync(VSYNC_STRICT);

	PAD_reset();
	GFX_clearLayers(LAYER_ALL);
	GFX_clear(screen);

	int show_setting = 0; // 1=brightness,2=volume
	int was_online = PLAT_isOnline();

	pthread_t cpucheckthread;
    pthread_create(&cpucheckthread, NULL, PLAT_cpu_monitor, NULL);

	int selected_row = top->selected - top->start;
	float targetY;
	float previousY;
	int is_scrolling = 0;



	// SDL_Surface * blackBG = SDL_CreateRGBSurfaceWithFormat(0,screen->w,screen->h,32,SDL_PIXELFORMAT_RGBA8888);
	// SDL_FillRect(blackBG,NULL,SDL_MapRGBA(screen->format,0,0,0,255));

	// SDL_LockMutex(animMutex);
	// globalpill = SDL_CreateRGBSurfaceWithFormat(SDL_SWSURFACE, screen->w, SCALE1(PILL_SIZE), FIXED_DEPTH, SDL_PIXELFORMAT_RGBA8888);
	// globalText = SDL_CreateRGBSurfaceWithFormat(SDL_SWSURFACE, screen->w, SCALE1(PILL_SIZE), FIXED_DEPTH, SDL_PIXELFORMAT_RGBA8888);
	// static int globallpillW = 0;
	// SDL_UnlockMutex(animMutex);

	//LOG_info("Start time time %ims\n",SDL_GetTicks());
	while (!quit) {
		GFX_startFrame();
		unsigned long now = SDL_GetTicks();
		
		PAD_poll();
			
		int selected = top->selected;
		int total = top->entries->count;
		
		PWR_update(&dirty, &show_setting, NULL, NULL);
		
		int is_online = PLAT_isOnline();
		if (was_online!=is_online) dirty = 1;
		was_online = is_online;
		int gsanimdir = ANIM_NONE;
		
		if (currentScreen == SCREEN_QUICKMENU) {
			int qm_total = qm_row == 0 ? quick->count : quickActions->count;

			if (PAD_justPressed(BTN_B) || PAD_tappedMenu(now)) {
				currentScreen = SCREEN_GAMELIST;
				folderbgchanged = 1; // The background painting code is a clusterfuck, just force a repaint here
				dirty = 1;
			}
			else if (PAD_justReleased(BTN_A)) {
				Entry *selected = qm_row == 0 ? quick->items[qm_col] : quickActions->items[qm_col];
				if(selected->type != ENTRY_DIP) {
					currentScreen = SCREEN_GAMELIST;
					total = top->entries->count;
					// prevent restoring list state, game list screen currently isnt our nav origin
					top->selected = 0;
					top->start = 0;
					top->end = top->start + MAIN_ROW_COUNT;
					restore_depth = -1;
					restore_relative = -1;
					restore_selected = 0;
					restore_start = 0;
					restore_end = 0;
				}
				Entry_open(screen, &dirty, selected); // 修改调用
				dirty = 1;
			}
			else if (PAD_justPressed(BTN_RIGHT)) {
				if(qm_row == 0 && qm_total > qm_slots) {
					qm_col++;
					if(qm_col >= qm_total) {
						qm_col = 0;
						qm_shift = 0;
						qm_slot = 0;
					}
					else {
						qm_slot++;
						if(qm_slot >= qm_slots) {
							qm_slot = qm_slots - 1;
							qm_shift++;
						}
					}
				}
				else {
					qm_col += 1;
					if(qm_col >= qm_total) {
						qm_col = 0;
					}
				}
				dirty = 1;
			}
			else if (PAD_justPressed(BTN_LEFT)) {
				if(qm_row == 0  && qm_total > qm_slots) {
					qm_col -= 1;
					if(qm_col < 0) {
						qm_col = qm_total - 1;
						qm_shift = qm_total - qm_slots;
						qm_slot = qm_slots - 1;
					}
					else {
						qm_slot--;
						if(qm_slot < 0) {
							qm_slot = 0;
							qm_shift--;
						}
					}
				}
				else {
					qm_col -= 1;
					if(qm_col < 0) {
						qm_col = qm_total - 1;
					}
				}
				dirty = 1;
			}
			else if(PAD_justPressed(BTN_DOWN)) {
				if(qm_row == 0) {
					qm_row = 1;
					qm_col = 0;
					dirty = 1;
				}
			}
			else if(PAD_justPressed(BTN_UP)) {
				if(qm_row == 1) {
					qm_row = 0;
					qm_col = qm_slot + qm_shift;
					dirty = 1;
				}
			}
		}
		else if(currentScreen == SCREEN_GAMESWITCHER) {
			if (PAD_justPressed(BTN_B) || PAD_tappedSelect(now)) {
				currentScreen = SCREEN_GAMELIST;
				switcher_selected = 0;
				dirty = 1;
				folderbgchanged = 1; // The background painting code is a clusterfuck, just force a repaint here
			}
			else if (recents->count > 0 && PAD_justReleased(BTN_A)) {
				// this will drop us back into game switcher after leaving the game
				putFile(GAME_SWITCHER_PERSIST_PATH, "unused");
				startgame = 1;
				Entry *selectedEntry = entryFromRecent(recents->items[switcher_selected]);
				should_resume = can_resume;
				Entry_open(screen, &dirty, selectedEntry);
				dirty = 1;
				Entry_free(selectedEntry);
			}
			else if (recents->count > 0 && PAD_justReleased(BTN_Y)) {
				// remove
				Recent* recentEntry = recents->items[switcher_selected--];
				Array_remove(recents, recentEntry);
				Recent_free(recentEntry);
				saveRecents();
				if(switcher_selected < 0)
					switcher_selected = recents->count - 1; // wrap
				dirty = 1;
			}
			else if (PAD_justPressed(BTN_RIGHT)) {
				switcher_selected++;
				if(switcher_selected == recents->count)
					switcher_selected = 0; // wrap
				dirty = 1;
				gsanimdir = SLIDE_LEFT;
			}
			else if (PAD_justPressed(BTN_LEFT)) {
				switcher_selected--;
				if(switcher_selected < 0)
					switcher_selected = recents->count - 1; // wrap
				dirty = 1;
				gsanimdir = SLIDE_RIGHT;
			}
		}
		else {
			if (PAD_tappedMenu(now)) {
				currentScreen = SCREEN_QUICKMENU;
				qm_col = 0;
				qm_row = 0;
				qm_shift = 0;
				qm_slot = 0;
				dirty = 1;
				folderbgchanged = 1; // The background painting code is a clusterfuck, just force a repaint here
				if (!HAS_POWER_BUTTON && !simple_mode) PWR_enableSleep();
			}
			else if (PAD_tappedSelect(now)) {
				currentScreen = SCREEN_GAMESWITCHER;
				switcher_selected = 0; 
				dirty = 1;
			}
			else if (total>0) {
				if (PAD_justRepeated(BTN_UP)) {
					if (selected==0 && !PAD_justPressed(BTN_UP)) {
						// stop at top
					}
					else {
						selected -= 1;
						if (selected<0) {
							selected = total-1;
							int start = total - MAIN_ROW_COUNT;
							top->start = (start<0) ? 0 : start;
							top->end = total; 
						}
						else if (selected<top->start) {
							top->start -= 1;
							top->end -= 1;
						}
					}
				}
				else if (PAD_justRepeated(BTN_DOWN)) {
					if (selected==total-1 && !PAD_justPressed(BTN_DOWN)) {
						// stop at bottom
					}
					else {
						selected += 1;
						if (selected>=total) {
							selected = 0;
							top->start = 0;
							top->end = (total<MAIN_ROW_COUNT) ? total : MAIN_ROW_COUNT;
						}
						else if (selected>=top->end) {
							top->start += 1;
							top->end += 1;
						}
					}
				}
				if (PAD_justRepeated(BTN_LEFT)) {
					selected -= MAIN_ROW_COUNT;
					if (selected<0) {
						selected = 0;
						top->start = 0;
						top->end = (total<MAIN_ROW_COUNT) ? total : MAIN_ROW_COUNT;
					}
					else if (selected<top->start) {
						top->start -= MAIN_ROW_COUNT;
						if (top->start<0) top->start = 0;
						top->end = top->start + MAIN_ROW_COUNT;
					}
				}
				else if (PAD_justRepeated(BTN_RIGHT)) {
					selected += MAIN_ROW_COUNT;
					if (selected>=total) {
						selected = total-1;
						int start = total - MAIN_ROW_COUNT;
						top->start = (start<0) ? 0 : start;
						top->end = total;
					}
					else if (selected>=top->end) {
						top->end += MAIN_ROW_COUNT;
						if (top->end>total) top->end = total;
						top->start = top->end - MAIN_ROW_COUNT;
					}
				}
			}
		
			if (PAD_justRepeated(BTN_L1) && !PAD_isPressed(BTN_R1) && !PWR_ignoreSettingInput(BTN_L1, show_setting)) { // previous alpha
				Entry* entry = top->entries->items[selected];
				int i = entry->alpha-1;
				if (i>=0) {
					selected = top->alphas->items[i];
					if (total>MAIN_ROW_COUNT) {
						top->start = selected;
						top->end = top->start + MAIN_ROW_COUNT;
						if (top->end>total) top->end = total;
						top->start = top->end - MAIN_ROW_COUNT;
					}
				}
			}
			else if (PAD_justRepeated(BTN_R1) && !PAD_isPressed(BTN_L1) && !PWR_ignoreSettingInput(BTN_R1, show_setting)) { // next alpha
				Entry* entry = top->entries->items[selected];
				int i = entry->alpha+1;
				if (i<top->alphas->count) {
					selected = top->alphas->items[i];
					if (total>MAIN_ROW_COUNT) {
						top->start = selected;
						top->end = top->start + MAIN_ROW_COUNT;
						if (top->end>total) top->end = total;
						top->start = top->end - MAIN_ROW_COUNT;
					}
				}
			}
	
			if (selected!=top->selected) {
				top->selected = selected;
				dirty = 1;
			}
	
			if (dirty && total>0) readyResume(top->entries->items[top->selected]);

			if (total>0 && can_resume && PAD_justReleased(BTN_RESUME)) {
				should_resume = 1;
				Entry_open(screen, &dirty, top->entries->items[top->selected]); // 修改调用
				if (quit) continue;
				dirty = 1;
			}
			else if (total>0 && PAD_justPressed(BTN_A)) {
				animationdirection = SLIDE_LEFT;
				Entry_open(screen, &dirty, top->entries->items[top->selected]); // 修改调用
				if (quit) continue;
				total = top->entries->count;
				dirty = 1;
				
				if (total>0) readyResume(top->entries->items[top->selected]);
			}
			else if (PAD_justPressed(BTN_B) && stack->count>1) {
				closeDirectory();
				animationdirection = SLIDE_RIGHT;
				total = top->entries->count;
				dirty = 1;
				
				if (total>0) readyResume(top->entries->items[top->selected]);
			}
		}
		
		if(dirty) {
			SDL_Surface *tmpOldScreen = NULL;
			SDL_Surface * switcherSur = NULL;
			// NOTE:22 This causes slowdown when CFG_getMenuTransitions is set to false because animationdirection turns > 0 somewhere but is never set back to 0 and so this code runs on every action, will fix later
			if(animationdirection != ANIM_NONE || (lastScreen==SCREEN_GAMELIST && currentScreen == SCREEN_GAMESWITCHER)) {
				if(tmpOldScreen) SDL_FreeSurface(tmpOldScreen);
				tmpOldScreen = GFX_captureRendererToSurface();
				SDL_SetSurfaceBlendMode(tmpOldScreen,SDL_BLENDMODE_BLEND);
			}

			// clear only background layer on start
			if(lastScreen==SCREEN_GAME || lastScreen==SCREEN_OFF) {
				GFX_clearLayers(LAYER_ALL);
			}
			else {	
				GFX_clearLayers(LAYER_TRANSITION);
				if(lastScreen!=SCREEN_GAMELIST)	
					GFX_clearLayers(LAYER_THUMBNAIL);
				GFX_clearLayers(LAYER_SCROLLTEXT);
				GFX_clearLayers(LAYER_IDK2);
			}
			GFX_clear(screen);

			// --- 关键修改点 1: 必须先调用 GFX_blitHardwareGroup 来声明和初始化 ow ---
			int ow = GFX_blitHardwareGroup(screen, show_setting);
			
			// --- 关键修改点 2: 然后再使用 ow 来绘制标题栏 ---
			if (currentScreen == SCREEN_GAMELIST) {
				int title_max_width = screen->w - SCALE1(PADDING * 2) - ow;
				char display_name_title[256];
				int text_width = GFX_truncateText(font.large, L("gamelist_title_system"), display_name_title, title_max_width, SCALE1(BUTTON_PADDING*2));
				title_max_width = MIN(title_max_width, text_width);

				SDL_Surface* text_title;
				SDL_Color textColor = uintToColour(THEME_COLOR6_255);
				text_title = TTF_RenderUTF8_Blended(font.large, display_name_title, textColor);
				GFX_blitPillLight(ASSET_WHITE_PILL, screen, &(SDL_Rect){
					SCALE1(PADDING),
					SCALE1(PADDING),
					title_max_width,
					SCALE1(PILL_SIZE)
				});
				SDL_BlitSurface(text_title, &(SDL_Rect){
					0,
					0,
					title_max_width-SCALE1(BUTTON_PADDING*2),
					text_title->h
				}, screen, &(SDL_Rect){
					SCALE1(PADDING+BUTTON_PADDING),
					SCALE1(PADDING+4)
				});
				SDL_FreeSurface(text_title);
			}

			// --- 原有逻辑开始 ---
			if (currentScreen == SCREEN_QUICKMENU) {
				if(lastScreen != SCREEN_QUICKMENU) {
					GFX_clearLayers(LAYER_BACKGROUND);
					GFX_clearLayers(LAYER_THUMBNAIL);
				}

				Entry *current = qm_row == 0 ? quick->items[qm_col] : quickActions->items[qm_col];
				char newBgPath[MAX_PATH];
				char fallbackBgPath[MAX_PATH];
				sprintf(newBgPath, SDCARD_PATH "/.media/quick_%s%s.png", current->name, 
					!strcmp(current->name,"Wifi") && CFG_getWifi() ? "_off" : ""); // wifi or wifi_off, based on state
				sprintf(fallbackBgPath, SDCARD_PATH "/.media/quick.png");
				
				// background
				if(!exists(newBgPath))
					strncpy(newBgPath, fallbackBgPath, sizeof(newBgPath) - 1);

				if(strcmp(newBgPath, folderBgPath) != 0) {
					strncpy(folderBgPath, newBgPath, sizeof(folderBgPath) - 1);
					// startLoadFolderBackground(newBgPath, onBackgroundLoaded, NULL);
					startLoadFolderBackground(newBgPath);
				}
				
				// buttons (duped and trimmed from below)
				if (show_setting && !GetHDMI()) GFX_blitHardwareHints(screen, show_setting);
				else GFX_blitButtonGroup((char*[]){ BTN_SLEEP==BTN_POWER?"POWER":"MENU","SLEEP",  NULL }, 0, screen, 0);
				
				GFX_blitButtonGroup((char*[]){ "B","BACK", "A","OPEN", NULL }, 1, screen, 1);

				if(CFG_getShowQuickswitcherUI()) {
					#define MENU_ITEM_SIZE 72 // item size, top line
					#define MENU_MARGIN_Y 32 // space between main UI elements and quick menu
					#define MENU_MARGIN_X 40 // space between main UI elements and quick menu
					#define MENU_ITEM_MARGIN 18 // space between items, top line
					#define MENU_TOGGLE_MARGIN 8 // space between items, bottom line
					#define MENU_LINE_MARGIN 8 // space between top and bottom line

					// this is flexible, not sure I like it at smaller scales than 3 though
					//int item_size = screen->h - SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN + // top pill area
					//	MENU_MARGIN_Y + MENU_LINE_MARGIN + PILL_SIZE + MENU_MARGIN_Y + // our own area
					//	BUTTON_MARGIN + PILL_SIZE + PADDING); // bottom pill area

					int item_space_y = screen->h - SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN + // top pill area
						MENU_MARGIN_Y + MENU_LINE_MARGIN + PILL_SIZE + MENU_MARGIN_Y + // our own area
						BUTTON_MARGIN + PILL_SIZE + PADDING);
					int item_size = SCALE1(MENU_ITEM_SIZE);
					int item_extra_y = item_space_y - item_size;
					int item_space_x = screen->w - SCALE1(PADDING + MENU_MARGIN_X + MENU_MARGIN_X + PADDING);
					// extra left margin for the first item in order to properly center all of them in the 
					// available space
					int item_inset_x = (item_space_x - SCALE1(qm_slots * MENU_ITEM_SIZE + (qm_slots - 1) * MENU_ITEM_MARGIN)) / 2;

					// primary
					ox = SCALE1(PADDING + MENU_MARGIN_X) + item_inset_x;
					oy = SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN + MENU_MARGIN_Y) + item_extra_y / 2;
					// just to keep selection visible.
					// every display should be able to fit three items, we shift horizontally to accomodate.
					ox -= qm_shift * (item_size + SCALE1(MENU_ITEM_MARGIN));
					for (int c = 0; c < quick->count; c++)
					{
						SDL_Rect item_rect = {ox, oy, item_size, item_size};
						Entry *item = quick->items[c];

						SDL_Color text_color = uintToColour(THEME_COLOR4_255);
						uint32_t item_color = THEME_COLOR3;
						uint32_t icon_color = THEME_COLOR4;

						if(qm_row == 0 && qm_col == c) {
							text_color = uintToColour(THEME_COLOR5_255);
							item_color = THEME_COLOR1;
							icon_color = THEME_COLOR5;
						}
						
						GFX_blitRectColor(ASSET_STATE_BG, screen, &item_rect, item_color);

						char icon_path[MAX_PATH];
						sprintf(icon_path, SDCARD_PATH "/.system/res/%s@%ix.png", item->name, FIXED_SCALE);
						SDL_Surface* bmp = IMG_Load(icon_path);
						if(bmp) {
							SDL_Surface* converted = SDL_ConvertSurfaceFormat(bmp, SDL_PIXELFORMAT_RGBA8888, 0);
							if (converted) {
								SDL_FreeSurface(bmp); 
								bmp = converted; 
							}
						}
						if(bmp) {
							// Calculate the position to center the source surface
							int x = (item_rect.w - bmp->w) / 2;
							int y = (item_rect.h - SCALE1(FONT_TINY + BUTTON_MARGIN) - bmp->h) / 2;
							SDL_Rect destRect = { ox+x, oy+y, 0, 0 };  // width/height not required
							//SDL_BlitSurface(bmp, NULL, screen, &destRect);

							GFX_blitSurfaceColor(bmp, NULL, screen, &destRect, icon_color);
						}

						int w, h;
						GFX_sizeText(font.tiny, item->name, SCALE1(FONT_TINY), &w, &h);
						SDL_Rect text_rect = {item_rect.x + (item_size - w) / 2, item_rect.y + item_size - h - SCALE1(BUTTON_MARGIN), w, h};
						GFX_blitText(font.tiny, item->name, SCALE1(FONT_TINY), text_color, screen, &text_rect);

						ox += item_rect.w + SCALE1(MENU_ITEM_MARGIN);
					}

					// secondary
					ox = SCALE1(PADDING + MENU_MARGIN_X);
					ox += (screen->w - SCALE1(PADDING + MENU_MARGIN_X + MENU_MARGIN_X + PADDING) - SCALE1(quickActions->count * PILL_SIZE) - SCALE1((quickActions->count - 1) * MENU_TOGGLE_MARGIN))/2;
					oy = SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN + MENU_MARGIN_Y + MENU_LINE_MARGIN) + item_size + item_extra_y / 2;
					for (int c = 0; c < quickActions->count; c++) {
						SDL_Rect item_rect = {ox, oy, SCALE1(PILL_SIZE), SCALE1(PILL_SIZE)};
						Entry *item = quickActions->items[c];

						SDL_Color text_color = uintToColour(THEME_COLOR4_255);
						uint32_t item_color = THEME_COLOR3;
						uint32_t icon_color = THEME_COLOR4;

						if(qm_row == 1 && qm_col == c) {
							text_color = uintToColour(THEME_COLOR5_255);
							item_color = THEME_COLOR1;
							icon_color = THEME_COLOR5;
						}

						GFX_blitPillColor(ASSET_WHITE_PILL, screen, &item_rect, item_color, RGB_WHITE);

						int asset = ASSET_WIFI;
						if (!strcmp(item->name,"Wifi"))
							asset = CFG_getWifi() ? ASSET_WIFI_OFF : ASSET_WIFI;
						else if (!strcmp(item->name,"Sleep"))
							asset = ASSET_SUSPEND;
						else if (!strcmp(item->name,"Reboot"))
							asset = ASSET_RESTART;
						else if (!strcmp(item->name,"Poweroff"))
							asset = ASSET_POWEROFF;
						else if (!strcmp(item->name,"Settings"))
							asset = ASSET_SETTINGS;
						else if (!strcmp(item->name,"Pak Store"))
							asset = ASSET_STORE;

						SDL_Rect rect;
						GFX_assetRect(asset, &rect);
						int x = item_rect.x;
						int y = item_rect.y;
						x += (SCALE1(PILL_SIZE) - rect.w) / 2;
						y += (SCALE1(PILL_SIZE) - rect.h) / 2;
						
						GFX_blitAssetColor(asset, NULL, screen, &(SDL_Rect){x,y}, icon_color);
						
						ox += item_rect.w + SCALE1(MENU_TOGGLE_MARGIN);
					}
				}
				lastScreen = SCREEN_QUICKMENU;
			}
			else if(startgame) {
				pilltargetY = +screen->w;
				animationdirection = ANIM_NONE;
				SDL_Surface *tmpsur = GFX_captureRendererToSurface();
				GFX_clearLayers(LAYER_ALL);
				GFX_clear(screen);
				GFX_flipHidden();

				if(lastScreen==SCREEN_GAMESWITCHER) {
					GFX_animateSurfaceOpacityAndScale(tmpsur,screen->w/2,screen->h/2,screen->w,screen->h,screen->w*4,screen->h*4,255,0,CFG_getMenuTransitions() ? 150:20,LAYER_BACKGROUND);
				} else {
					GFX_animateSurfaceOpacity(tmpsur,0,0,screen->w,screen->h,255,0,CFG_getMenuTransitions() ? 150:20,LAYER_BACKGROUND);
				}
				SDL_FreeSurface(tmpsur);
			}
			else if(currentScreen == SCREEN_GAMESWITCHER) {
				GFX_clearLayers(LAYER_ALL);
				ox = 0;
				oy = 0;
				
				// For all recents with resumable state (i.e. has savegame), show game switcher carousel
				if(recents->count > 0) {
					Entry *selectedEntry = entryFromRecent(recents->items[switcher_selected]);
					readyResume(selectedEntry);
					// title pill
					{
						int max_width = screen->w - SCALE1(PADDING * 2) - ow;
						
						char display_name[256];
						int text_width = GFX_truncateText(font.large, selectedEntry->name, display_name, max_width, SCALE1(BUTTON_PADDING*2));
						max_width = MIN(max_width, text_width);

						SDL_Surface* text;
						SDL_Color textColor = uintToColour(THEME_COLOR6_255);
						text = TTF_RenderUTF8_Blended(font.large, display_name, textColor);
						GFX_blitPillLight(ASSET_WHITE_PILL, screen, &(SDL_Rect){
							SCALE1(PADDING),
							SCALE1(PADDING),
							max_width,
							SCALE1(PILL_SIZE)
						});
						SDL_BlitSurface(text, &(SDL_Rect){
							0,
							0,
							max_width-SCALE1(BUTTON_PADDING*2),
							text->h
						}, screen, &(SDL_Rect){
							SCALE1(PADDING+BUTTON_PADDING),
							SCALE1(PADDING+4)
						});
						SDL_FreeSurface(text);
					}

					if(can_resume) GFX_blitButtonGroup((char*[]){ "B","BACK",  NULL }, 0, screen, 0);
					else GFX_blitButtonGroup((char*[]){ BTN_SLEEP==BTN_POWER?"POWER":"MENU","SLEEP",  NULL }, 0, screen, 0);

					GFX_blitButtonGroup((char*[]){ "Y", "REMOVE", "A","RESUME", NULL }, 1, screen, 1);

					if(has_preview) {
						// lotta memory churn here
					
						SDL_Surface* bmp = IMG_Load(preview_path);
						SDL_Surface* raw_preview = SDL_ConvertSurfaceFormat(bmp, SDL_PIXELFORMAT_RGBA8888, 0);
						if (raw_preview) {
							SDL_FreeSurface(bmp); 
							bmp = raw_preview; 
						}
						if(bmp) {
							int aw = screen->w;
							int ah = screen->h;
							int ax = 0;
							int ay = 0;
						
							float aspectRatio = (float)bmp->w / (float)bmp->h;
							float screenRatio = (float)screen->w / (float)screen->h;
					
							if (screenRatio > aspectRatio) {
								aw = (int)(screen->h * aspectRatio);
								ah = screen->h;
							} else {
								aw = screen->w;
								ah = (int)(screen->w / aspectRatio);
							}
							ax = (screen->w - aw) / 2;
							ay = (screen->h - ah) / 2;
						
							if(lastScreen == SCREEN_GAME) {
								// need to flip once so streaming_texture1 is updated
								GFX_flipHidden();
								GFX_animateSurfaceOpacityAndScale(bmp,screen->w/2,screen->h/2,screen->w*4,screen->h*4,aw,ah,0,255,CFG_getMenuTransitions() ? 150:20,LAYER_ALL);
							} else if(lastScreen == SCREEN_GAMELIST) { 
								
								GFX_drawOnLayer(blackBG,0,0,screen->w,screen->h,1.0f,0,LAYER_BACKGROUND);
								GFX_drawOnLayer(bmp,ax,ay,aw, ah,1.0f,0,LAYER_BACKGROUND);
								GFX_flipHidden();
								SDL_Surface *tmpNewScreen = GFX_captureRendererToSurface();
								GFX_clearLayers(LAYER_ALL);
								folderbgchanged=1;
								GFX_drawOnLayer(tmpOldScreen,0,0,screen->w, screen->h,1.0f,0,LAYER_ALL);
								GFX_animateSurface(tmpNewScreen,0,0-screen->h,0,0,screen->w,screen->h,CFG_getMenuTransitions() ? 100:20,255,255,LAYER_BACKGROUND);
								SDL_FreeSurface(tmpNewScreen);
								
							} else if(lastScreen == SCREEN_GAMESWITCHER) {
								GFX_flipHidden();
								GFX_drawOnLayer(blackBG,0,0,screen->w, screen->h,1.0f,0,LAYER_BACKGROUND);
								if(gsanimdir == SLIDE_LEFT) 
									GFX_animateSurface(bmp,ax+screen->w,ay,ax,ay,aw,ah,CFG_getMenuTransitions() ? 80:20,0,255,LAYER_ALL);
								else if(gsanimdir == SLIDE_RIGHT)
									GFX_animateSurface(bmp,ax-screen->w,ay,ax,ay,aw,ah,CFG_getMenuTransitions() ? 80:20,0,255,LAYER_ALL);
								
								GFX_drawOnLayer(bmp,ax,ay,aw,ah,1.0f,0,LAYER_BACKGROUND);
							} else if(lastScreen == SCREEN_QUICKMENU) {
								GFX_flipHidden();
								GFX_drawOnLayer(blackBG,0,0,screen->w, screen->h,1.0f,0,LAYER_BACKGROUND);								
								GFX_drawOnLayer(bmp,ax,ay,aw,ah,1.0f,0,LAYER_BACKGROUND);
							}
							SDL_FreeSurface(bmp);  // Free after rendering
						}
					}
					else {
						SDL_Rect preview_rect = {ox,oy,screen->w,screen->h};
						SDL_Surface * tmpsur = SDL_CreateRGBSurfaceWithFormat(0,screen->w,screen->h,32,SDL_PIXELFORMAT_RGBA8888);
						SDL_FillRect(tmpsur, &preview_rect, SDL_MapRGBA(screen->format,0,0,0,255));
						if(lastScreen == SCREEN_GAME) {
							GFX_animateSurfaceOpacityAndScale(tmpsur,screen->w/2,screen->h/2,screen->w*4,screen->h*4,screen->w,screen->h,255,0,CFG_getMenuTransitions() ? 150:20,LAYER_BACKGROUND);
						} else if(lastScreen == SCREEN_GAMELIST) { 
							GFX_animateSurface(tmpsur,0,0-screen->h,0,0,screen->w,screen->h,CFG_getMenuTransitions() ? 100:20,255,255,LAYER_ALL);
						} else if(lastScreen == SCREEN_GAMESWITCHER) {
							GFX_flipHidden();
							if(gsanimdir == SLIDE_LEFT) 
								GFX_animateSurface(tmpsur,0+screen->w,0,0,0,screen->w,screen->h,CFG_getMenuTransitions() ? 80:20,0,255,LAYER_ALL);
							else if(gsanimdir == SLIDE_RIGHT)
								GFX_animateSurface(tmpsur,0-screen->w,0,0,0,screen->w,screen->h,CFG_getMenuTransitions() ? 80:20,0,255,LAYER_ALL);
						}
						SDL_FreeSurface(tmpsur);
						GFX_blitMessage(font.large, "No Preview", screen, &preview_rect);
					}
					Entry_free(selectedEntry);
				}
				else {
					SDL_Rect preview_rect = {ox,oy,screen->w,screen->h};
					SDL_FillRect(screen, &preview_rect, 0);
					GFX_blitMessage(font.large, "No Recents", screen, &preview_rect);
					GFX_blitButtonGroup((char*[]){ "B","BACK", NULL }, 1, screen, 1);
				}
				
				GFX_flipHidden();

				if(switcherSur) SDL_FreeSurface(switcherSur);
				switcherSur = GFX_captureRendererToSurface();
				lastScreen = SCREEN_GAMESWITCHER;
			}
			else { // if currentscreen == SCREEN_GAMELIST
				// background and game art file path stuff
				Entry* entry = top->entries->items[top->selected];
				assert(entry);
				char tmp_path[MAX_PATH];
				strncpy(tmp_path, entry->path, sizeof(tmp_path) - 1);
				tmp_path[sizeof(tmp_path) - 1] = '\0';
			
				char* res_name = strrchr(tmp_path, '/');
				if (res_name) res_name++;

				char path_copy[1024];
				strncpy(path_copy, entry->path, sizeof(path_copy) - 1);
				path_copy[sizeof(path_copy) - 1] = '\0';
		
				char* rompath = dirname(path_copy);
			
				char res_copy[1024];
				strncpy(res_copy, res_name, sizeof(res_copy) - 1);
				res_copy[sizeof(res_copy) - 1] = '\0';
		
				char* dot = strrchr(res_copy, '.');
				if (dot) *dot = '\0'; 

				static int lastType = -1;
		
				if(((entry->type == ENTRY_DIR || entry->type == ENTRY_ROM) && CFG_getRomsUseFolderBackground())) {
					char *newBg = entry->type == ENTRY_DIR ? entry->path:rompath;
					if((strcmp(newBg, folderBgPath) != 0 || lastType != entry->type) && sizeof(folderBgPath) != 1) {
						lastType = entry->type;
						char tmppath[512];
						strncpy(folderBgPath, newBg, sizeof(folderBgPath) - 1);
						if (entry->type == ENTRY_DIR)
							snprintf(tmppath, sizeof(tmppath), "%s/.media/bg.png", folderBgPath);
						else if (entry->type == ENTRY_ROM)
							snprintf(tmppath, sizeof(tmppath), "%s/.media/bglist.png", folderBgPath);
						if(!exists(tmppath)) {
							snprintf(tmppath, sizeof(tmppath), SDCARD_PATH "/bg.png", folderBgPath);
						}
						// startLoadFolderBackground(tmppath, onBackgroundLoaded, NULL);
						startLoadFolderBackground(tmppath);
					}
				} 
				else if(strcmp(SDCARD_PATH "/bg.png", folderBgPath) != 0) {
					strncpy(folderBgPath, SDCARD_PATH "/bg.png", sizeof(folderBgPath) - 1);
					// startLoadFolderBackground(SDCARD_PATH "/bg.png", onBackgroundLoaded, NULL);
					startLoadFolderBackground(SDCARD_PATH "/bg.png");
				}
				// load game thumbnails
				if (total > 0) {
					if(CFG_getShowGameArt()) {
						char thumbpath[1024];
						snprintf(thumbpath, sizeof(thumbpath), "%s/.media/%s.png", rompath, res_copy);
						had_thumb = 0;
						// startLoadThumb(thumbpath, onThumbLoaded, NULL);
						startLoadThumb(thumbpath);
						int max_w = (int)(screen->w - (screen->w * CFG_getGameArtWidth())); 
						int max_h = (int)(screen->h * 0.6);  
						int new_w = max_w;
						int new_h = max_h; 
						had_thumb = 1;
						if(exists(thumbpath))
							ox = (int)(max_w) - SCALE1(BUTTON_MARGIN*5);
						else
							ox = screen->w;
					}
				}

				// buttons
				if (show_setting && !GetHDMI()) GFX_blitHardwareHints(screen, show_setting);
				else if (can_resume) GFX_blitButtonGroup((char*[]){ "X","RESUME",  NULL }, 0, screen, 0);
				else GFX_blitButtonGroup((char*[]){ 
					BTN_SLEEP==BTN_POWER?"POWER":"MENU",
					BTN_SLEEP==BTN_POWER||simple_mode?"SLEEP":"INFO",  
					NULL }, 0, screen, 0);
			
				if (total==0) {
					if (stack->count>1) {
						GFX_blitButtonGroup((char*[]){ "B","BACK",  NULL }, 0, screen, 1);
					}
				}
				else {
					if (stack->count>1) {
						GFX_blitButtonGroup((char*[]){ "B","BACK", "A","OPEN", NULL }, 1, screen, 1);
					}
					else {
						GFX_blitButtonGroup((char*[]){ "A","OPEN", NULL }, 0, screen, 1);
					}
				}

				// list
				if (total > 0) {
					selected_row = top->selected - top->start;
					previousY = (remember_selection) * PILL_SIZE;
					targetY = selected_row * PILL_SIZE;
					// --- 最终修正版：在对称的安全区域内进行垂直居中 ---
					int safe_area_top = SCALE1(PADDING + PILL_SIZE); // 标题栏的底部
					int safe_area_bottom = screen->h - SCALE1(PADDING + PILL_SIZE); // 底部按钮栏的顶部
					int available_height = safe_area_bottom - safe_area_top;
					int list_block_height = MAIN_ROW_COUNT * SCALE1(PILL_SIZE);
					int list_oy = safe_area_top + ((available_height - list_block_height) / 2);
					
					for (int i = top->start, j = 0; i < top->end; i++, j++) {
						Entry* entry = top->entries->items[i];
						char* entry_name = entry->name;
						char* entry_unique = entry->unique;
						int available_width = (had_thumb ? ox + SCALE1(BUTTON_MARGIN) : screen->w - SCALE1(BUTTON_MARGIN)) - SCALE1(PADDING * 2);
						if (i == top->start && !(had_thumb)) available_width -= ow;
						trimSortingMeta(&entry_name);

						if (entry_unique) // Only render if a unique name exists
							trimSortingMeta(&entry_unique);
						
						char display_name[256];
						int text_width = GFX_getTextWidth(font.large, entry_unique ? entry_unique : entry_name,display_name, available_width, SCALE1(BUTTON_PADDING * 2));
						
						int max_width = MIN(available_width, text_width);
					
						SDL_Color text_color = uintToColour(THEME_COLOR4_255);
						int notext = 0;
						if(selected_row == remember_selection && j == selected_row && (selected_row+1 >= (top->end-top->start) || selected_row == 0 || selected_row == remember_selection)) {
							text_color = uintToColour(THEME_COLOR5_255);
							notext=1;
						}
						SDL_Surface* text = TTF_RenderUTF8_Blended(font.large, entry_name, text_color);
						SDL_Surface* text_unique = TTF_RenderUTF8_Blended(font.large, display_name, COLOR_DARK_TEXT);
						if (j == selected_row) {
							is_scrolling = GFX_resetScrollText(font.large,display_name, max_width - SCALE1(BUTTON_PADDING*2));
							SDL_LockMutex(animMutex);
							if(globalpill) SDL_FreeSurface(globalpill);
							globalpill = SDL_CreateRGBSurfaceWithFormat(SDL_SWSURFACE, max_width, SCALE1(PILL_SIZE), FIXED_DEPTH, SDL_PIXELFORMAT_RGBA8888);
							GFX_blitPillDark(ASSET_WHITE_PILL, globalpill, &(SDL_Rect){0,0, max_width, SCALE1(PILL_SIZE)});
							globallpillW =  max_width;
							SDL_UnlockMutex(animMutex);
							AnimTask* task = malloc(sizeof(AnimTask));
							task->startX = SCALE1(BUTTON_MARGIN);
							task->startY = list_oy + SCALE1(previousY); // 使用偏移
							task->targetX = SCALE1(BUTTON_MARGIN);
							task->targetY = list_oy + SCALE1(targetY); // 使用偏移
							task->targetTextY = list_oy + SCALE1(targetY+4); // 使用偏移
							pilltargetTextY = +screen->w;
							task->move_w = max_width;
							task->move_h = SCALE1(PILL_SIZE);
							task->frames = CFG_getMenuAnimations() ? 3:0;
							task->entry_name = notext ? " ":entry_name;
							animPill(task);
						} 
						SDL_Rect text_rect = { 0, 0, max_width - SCALE1(BUTTON_PADDING*2), text->h };
						SDL_Rect dest_rect = { SCALE1(BUTTON_MARGIN + BUTTON_PADDING), list_oy + SCALE1((j * PILL_SIZE)+4) }; // 使用偏移
				
						SDL_BlitSurface(text_unique, &text_rect, screen, &dest_rect);
						SDL_BlitSurface(text, &text_rect, screen, &dest_rect);
						SDL_FreeSurface(text_unique); // Free after use
						SDL_FreeSurface(text); // Free after use
					}
					if(lastScreen==SCREEN_GAMESWITCHER) {
						if(switcherSur) {
							// update cpu surface here first
							GFX_clearLayers(LAYER_ALL);
							folderbgchanged=1;
							
							GFX_flipHidden();
							GFX_animateSurface(switcherSur,0,0,0,0-screen->h,screen->w,screen->h,CFG_getMenuTransitions() ? 100:20,255,255,LAYER_BACKGROUND);
							animationdirection = ANIM_NONE;
						}
					}
					if(lastScreen==SCREEN_OFF) {
						GFX_animateSurfaceOpacity(blackBG,0,0,screen->w,screen->h,255,0,CFG_getMenuTransitions() ? 200:20,LAYER_THUMBNAIL);
					}
		
					remember_selection = selected_row;
				}
				else {
					// TODO: for some reason screen's dimensions end up being 0x0 in GFX_blitMessage...
					GFX_blitMessage(font.large, "Empty folder", screen, &(SDL_Rect){0,0,screen->w,screen->h}); //, NULL);
				}
				
				lastScreen = SCREEN_GAMELIST;
			}

			if(animationdirection != ANIM_NONE) {
				if(CFG_getMenuTransitions()) {
					GFX_clearLayers(LAYER_BACKGROUND);
					folderbgchanged = 1;
					GFX_clearLayers(LAYER_TRANSITION);
					GFX_flipHidden();
					SDL_Surface *tmpNewScreen = GFX_captureRendererToSurface();
					SDL_SetSurfaceBlendMode(tmpNewScreen,SDL_BLENDMODE_BLEND);
					GFX_clearLayers(LAYER_THUMBNAIL);
					if(animationdirection == SLIDE_LEFT) GFX_animateAndFadeSurface(tmpOldScreen,0,0,0-FIXED_WIDTH,0,FIXED_WIDTH,FIXED_HEIGHT,200,tmpNewScreen,1,0,FIXED_WIDTH,FIXED_HEIGHT,0,255,LAYER_THUMBNAIL);
					if(animationdirection == SLIDE_RIGHT) GFX_animateAndFadeSurface(tmpOldScreen,0,0,0+FIXED_WIDTH,0,FIXED_WIDTH,FIXED_HEIGHT,200,tmpNewScreen,1,0,FIXED_WIDTH,FIXED_HEIGHT,0,255,LAYER_THUMBNAIL);
					GFX_clearLayers(LAYER_THUMBNAIL);
					SDL_FreeSurface(tmpNewScreen);
				}
				// animation done
				animationdirection = ANIM_NONE;
			}

			if(lastScreen == SCREEN_QUICKMENU) {
				SDL_LockMutex(bgMutex);
				if(folderbgchanged) {
					if(folderbgbmp)
						GFX_drawOnLayer(folderbgbmp,0, 0, screen->w, screen->h,1.0f,0,LAYER_BACKGROUND);
					else
						GFX_clearLayers(LAYER_BACKGROUND);
					folderbgchanged = 0;
				}
				SDL_UnlockMutex(bgMutex);
			}
			else if(lastScreen == SCREEN_GAMELIST) {
				SDL_LockMutex(bgMutex);
				if(folderbgchanged) {
					if(folderbgbmp)
						GFX_drawOnLayer(folderbgbmp,0, 0, screen->w, screen->h,1.0f,0,LAYER_BACKGROUND);
					else
						GFX_clearLayers(LAYER_BACKGROUND);
					folderbgchanged = 0;
				}
				SDL_UnlockMutex(bgMutex);
				SDL_LockMutex(thumbMutex);
				if(thumbbmp && thumbchanged) {
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

					int target_x = screen->w-(new_w + SCALE1(BUTTON_MARGIN*3));
					int target_y = (int)(screen->h * 0.50);
					int center_y = target_y - (new_h / 2); // FIX: use new_h instead of thumbbmp->h
					GFX_clearLayers(LAYER_THUMBNAIL);
					GFX_drawOnLayer(thumbbmp,target_x,center_y,new_w,new_h,1.0f,0,LAYER_THUMBNAIL);
				} else if(thumbchanged) {
					GFX_clearLayers(LAYER_THUMBNAIL);
				}
				SDL_UnlockMutex(thumbMutex);

				GFX_clearLayers(LAYER_TRANSITION);
				GFX_clearLayers(LAYER_SCROLLTEXT);
				
				SDL_LockMutex(animMutex);
				GFX_drawOnLayer(globalpill, pillRect.x, pillRect.y, globallpillW, globalpill->h, 1.0f, 0, LAYER_TRANSITION);
				// GFX_drawOnLayer(globalText, SCALE1(PADDING+BUTTON_PADDING), pilltargetTextY, globalText->w, globalText->h, 1.0f, 0, LAYER_SCROLLTEXT);
				SDL_UnlockMutex(animMutex);
			}
			if(!startgame) // dont flip if game gonna start
				GFX_flip(screen);

			dirty = 0;
		} else if(animationDraw || folderbgchanged || thumbchanged || is_scrolling) {
			// honestly this whole thing is here only for the scrolling text, I set it now to run this at 30fps which is enough for scrolling text, should move this to seperate animation function eventually
			Uint32 now = SDL_GetTicks();
			Uint32 frame_start = now;
			static char cached_display_name[256] = "";
			SDL_LockMutex(bgMutex);
			if(folderbgchanged) {
				if(folderbgbmp)
					GFX_drawOnLayer(folderbgbmp,0, 0, screen->w, screen->h,1.0f,0,LAYER_BACKGROUND);
				else 
					GFX_clearLayers(LAYER_BACKGROUND);
				folderbgchanged = 0;
			}
			SDL_UnlockMutex(bgMutex);
			SDL_LockMutex(thumbMutex);
			if(thumbbmp && thumbchanged) {
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
	
				int target_x = screen->w-(new_w + SCALE1(BUTTON_MARGIN*3));
				int target_y = (int)(screen->h * 0.50);
				int center_y = target_y - (new_h / 2); // FIX: use new_h instead of thumbbmp->h
				GFX_clearLayers(LAYER_THUMBNAIL);
				GFX_drawOnLayer(thumbbmp,target_x,center_y,new_w,new_h,1.0f,0,LAYER_THUMBNAIL);
				thumbchanged = 0;
			} else if(thumbchanged) {
				GFX_clearLayers(LAYER_THUMBNAIL);
				thumbchanged = 0;
			}
			SDL_UnlockMutex(thumbMutex);
			SDL_LockMutex(animMutex);
			if(animationDraw) {
				GFX_clearLayers(LAYER_TRANSITION);
				GFX_drawOnLayer(globalpill, pillRect.x, pillRect.y, globallpillW, globalpill->h, 1.0f, 0, LAYER_TRANSITION);
				animationDraw = 0;
			}
			SDL_UnlockMutex(animMutex);
			if (currentScreen != SCREEN_GAMESWITCHER && currentScreen != SCREEN_QUICKMENU) {
				// if(is_scrolling && pillanimdone && currentAnimQueueSize < 1) {
				if(is_scrolling && pillanimdone) {
					int ow = GFX_blitHardwareGroup(screen, show_setting);
					Entry* entry = top->entries->items[top->selected];
					trimSortingMeta(&entry->name);
					char* entry_text = entry->name;
					if (entry->unique) {
						trimSortingMeta(&entry->unique);
						entry_text = entry->unique;
					}

					int available_width = (had_thumb ? ox + SCALE1(BUTTON_MARGIN) : screen->w - SCALE1(BUTTON_MARGIN)) - SCALE1(PADDING * 2);
					if (top->selected == top->start && !had_thumb) available_width -= ow;

					SDL_Color text_color = uintToColour(THEME_COLOR5_255);

					int text_width = GFX_getTextWidth(font.large, entry_text, cached_display_name, available_width, SCALE1(BUTTON_PADDING * 2));
					int max_width = MIN(available_width, text_width);
					
					// --- 关键修改点 ---
					int safe_area_top = SCALE1(PADDING + PILL_SIZE);
					int safe_area_bottom = screen->h - SCALE1(PADDING + PILL_SIZE);
					int available_height = safe_area_bottom - safe_area_top;
					int list_block_height = MAIN_ROW_COUNT * SCALE1(PILL_SIZE);
					int list_oy = safe_area_top + ((available_height - list_block_height) / 2);

					GFX_clearLayers(LAYER_SCROLLTEXT);
					GFX_scrollTextTexture(
						font.large,
						entry_text,
						SCALE1(BUTTON_MARGIN + BUTTON_PADDING), list_oy + SCALE1((remember_selection * PILL_SIZE) + 4),
						max_width - SCALE1(BUTTON_PADDING * 2),
						0,
						text_color,
						1,
						17  // 要背景
					);
				} 
				else {
					GFX_clearLayers(LAYER_TRANSITION);
					GFX_clearLayers(LAYER_SCROLLTEXT);
					SDL_LockMutex(animMutex);
					GFX_drawOnLayer(globalpill, pillRect.x, pillRect.y, globallpillW, globalpill->h, 1.0f, 0, LAYER_TRANSITION);
					GFX_drawOnLayer(globalText, SCALE1(BUTTON_MARGIN + BUTTON_PADDING),pilltargetTextY, globalText->w, globalText->h, 1.0f, 0, LAYER_SCROLLTEXT);
					SDL_UnlockMutex(animMutex);
					PLAT_GPU_Flip();
				} 
			}
			else {
				SDL_Delay(100); // why are we running long delays on the render thread, wtf?
			}
			dirty = 0;
		} 
		else {
			// want to draw only if needed
			// SDL_LockMutex(bgqueueMutex);
			// SDL_LockMutex(thumbqueueMutex);
			// SDL_LockMutex(animqueueMutex);
			if(needDraw) {
				PLAT_GPU_Flip();
				needDraw = 0;
			} else {
				// TODO: Why 17? Seems like an odd choice for 60fps, it almost guarantees we miss at least one frame.
				// This should either be 16(.66666667) or make proper use of SDL_Ticks to only wait for the next render pass.
				SDL_Delay(17); 
			}
			// GFX_sync_fixed_rate(60.0);
			// SDL_UnlockMutex(animqueueMutex);
			// SDL_UnlockMutex(thumbqueueMutex);
			// SDL_UnlockMutex(bgqueueMutex);
		}
		GFX_sync_fixed_rate(60.0);
		SDL_LockMutex(frameMutex);
		frameReady = true;
		SDL_CondSignal(flipCond);
		SDL_UnlockMutex(frameMutex);

		// animation does not carry over between loops, this should only ever be set by
		// input handling and directly consumed by the following render pass
		assert(animationdirection == ANIM_NONE);

		// handle HDMI change
		static int had_hdmi = -1;
		int has_hdmi = GetHDMI();
		if (had_hdmi==-1) had_hdmi = has_hdmi;
		if (has_hdmi!=had_hdmi) {
			had_hdmi = has_hdmi;

			Entry* entry = top->entries->items[top->selected];
			LOG_info("restarting after HDMI change... (%s)\n", entry->path);
			saveLast(entry->path); // NOTE: doesn't work in Recents (by design)
			sleep(4);
			quit = 1;
		}
	}
	// if(blackBG)	SDL_FreeSurface(blackBG);
	// if (folderbgbmp) SDL_FreeSurface(folderbgbmp);
	// if (thumbbmp) SDL_FreeSurface(thumbbmp);
	if(blackBG) SDL_FreeSurface(blackBG);
	Menu_quit();
	PWR_quit();
	PAD_quit();
	GFX_quit();
	QuitSettings();
}
