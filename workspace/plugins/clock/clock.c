#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <msettings.h>

#include "defines.h"
#include "api.h"
#include "utils.h"
#include "plugin.h"
#include "sysui.h"

static SDL_Surface* screen;
static int quit_plugin;

enum {
	CURSOR_YEAR,
	CURSOR_MONTH,
	CURSOR_DAY,
	CURSOR_HOUR,
	CURSOR_MINUTE,
	CURSOR_SECOND,
	CURSOR_AMPM,
};

static int plugin_init(void* main_screen) {
    screen = (SDL_Surface*)main_screen;
    quit_plugin = 0;
    
    SysUI_Init(screen, &font);
    SysUI_SetTitle("Date and time");
    SysUI_SetBottomHints("SELECT", "24H", "A", "SET", "B", "BACK");
    SysUI_SetFullscreen(false);

    PWR_init();

    return 0;
}

static int plugin_run() {
	SDL_Surface* digits = SDL_CreateRGBSurface(SDL_SWSURFACE, SCALE2(120,16), FIXED_DEPTH,RGBA_MASK_AUTO);
	SDL_FillRect(digits, NULL, RGB_BLACK);
	
	SDL_Surface* digit;
	char* chars[] = { "0","1","2","3","4","5","6","7","8","9","/",":", NULL };
	char* c;
	int i = 0;

#define DIGIT_WIDTH 10
#define DIGIT_HEIGHT 16
#define CHAR_SLASH 10
#define CHAR_COLON 11

	while ((c = chars[i])) {
		digit = TTF_RenderUTF8_Blended(font.large, c, COLOR_WHITE);
		int y = i==CHAR_COLON ? SCALE1(-1.5) : 0;
		SDL_BlitSurface(digit, NULL, digits, &(SDL_Rect){ (i * SCALE1(DIGIT_WIDTH)) + (SCALE1(DIGIT_WIDTH) - digit->w)/2, y + (SCALE1(DIGIT_HEIGHT) - digit->h)/2 });
		SDL_FreeSurface(digit);
		i += 1;
	}
	if (PLAT_isOnline()) { 
		GFX_clear(screen);
		SDL_Rect msg_rect = {0, 0, screen->w, screen->h}; 
		GFX_blitMessage(font.large, "Syncing network time...", screen, &msg_rect);
		GFX_flip(screen);
		system("sntp -sS pool.ntp.org");
		SDL_Delay(500); 
	}
	int save_changes = 0;
	int select_cursor = 0;
	int show_24hour = exists(USERDATA_PATH "/show_24hour");
	
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	
	int32_t day_selected = tm.tm_mday;
	int32_t month_selected = tm.tm_mon + 1;
	uint32_t year_selected = tm.tm_year + 1900;
	int32_t hour_selected = tm.tm_hour;
	int32_t minute_selected = tm.tm_min;
	int32_t seconds_selected = tm.tm_sec;
	
	int blit(int i, int x, int y) {
		SDL_BlitSurface(digits, &(SDL_Rect){i*SCALE1(10),0,SCALE2(10,16)}, screen, &(SDL_Rect){x,y});
		return x + SCALE1(10);
	}
	void blitBar(int x, int y, int w) {
		GFX_blitPill(ASSET_UNDERLINE, screen, &(SDL_Rect){x,y,w});
	}
	int blitNumber(int num, int x, int y) {
		int n;
		if (num > 999) {
			n = num / 1000; num -= n * 1000; x = blit(n, x,y);
			n = num / 100; num -= n * 100; x = blit(n, x,y);
		}
		n = num / 10; num -= n * 10; x = blit(n, x,y);
		n = num; x = blit(n, x,y);
		return x;
	}
	void validate(void) {
		uint32_t february_days = 28;
		if ( ((year_selected % 4 == 0) && (year_selected % 100 != 0)) || (year_selected % 400 == 0)) february_days = 29;
		if (month_selected > 12) month_selected -= 12; else if (month_selected < 1) month_selected += 12;
		if (year_selected > 2100) year_selected = 2100; else if (year_selected < 1970) year_selected = 1970;
		switch(month_selected) {
			case 2: if (day_selected > february_days) day_selected -= february_days; else if (day_selected<1) day_selected += february_days; break;
			case 4: case 6: case 9: case 11: if (day_selected > 30) day_selected -= 30; else if (day_selected < 1) day_selected += 30; break;
			default: if (day_selected > 31) day_selected -= 31; else if (day_selected < 1) day_selected += 31; break;
		}
		if (hour_selected > 23) hour_selected -= 24; else if (hour_selected < 0) hour_selected += 24;
		if (minute_selected > 59) minute_selected -= 60; else if (minute_selected < 0) minute_selected += 60;
		if (seconds_selected > 59) seconds_selected -= 60; else if (seconds_selected < 0) seconds_selected += 60;
	}
	
	int option_count = 7;
	int dirty = 1;
	bool input_handled_by_sysui = false; 
    
    int show_setting_dummy = 0; 
    
	while(!quit_plugin) {
		uint32_t now = SDL_GetTicks();
		PAD_poll();
        

        input_handled_by_sysui = SysUI_Update();
        if (input_handled_by_sysui) {
			dirty = 1; 
		}

        PWR_update(&dirty, &show_setting_dummy, NULL, NULL);

		if (!input_handled_by_sysui) {
			if (PAD_justRepeated(BTN_UP)) { dirty = 1; switch(select_cursor) { case CURSOR_YEAR: year_selected++; break; case CURSOR_MONTH: month_selected++; break; case CURSOR_DAY: day_selected++; break; case CURSOR_HOUR: hour_selected++; break; case CURSOR_MINUTE: minute_selected++; break; case CURSOR_SECOND: seconds_selected++; break; case CURSOR_AMPM: hour_selected += 12; break; } }
			else if (PAD_justRepeated(BTN_DOWN)) { dirty = 1; switch(select_cursor) { case CURSOR_YEAR: year_selected--; break; case CURSOR_MONTH: month_selected--; break; case CURSOR_DAY: day_selected--; break; case CURSOR_HOUR: hour_selected--; break; case CURSOR_MINUTE: minute_selected--; break; case CURSOR_SECOND: seconds_selected--; break; case CURSOR_AMPM: hour_selected -= 12; break; } }
			else if (PAD_justRepeated(BTN_LEFT)) { dirty = 1; select_cursor--; if (select_cursor < 0) select_cursor += option_count; }
			else if (PAD_justRepeated(BTN_RIGHT)) { dirty = 1; select_cursor++; if (select_cursor >= option_count) select_cursor -= option_count; }
			else if (PAD_justPressed(BTN_A)) { save_changes = 1; quit_plugin = 1; }
			else if (PAD_justPressed(BTN_B)) { quit_plugin = 1; }
			else if (PAD_tappedSelect(now)) {
				dirty = 1;
				show_24hour = !show_24hour;
				option_count = (show_24hour ? CURSOR_SECOND : CURSOR_AMPM) + 1;
				if (select_cursor >= option_count) select_cursor -= option_count;
				if (show_24hour) system("touch " USERDATA_PATH "/show_24hour");
				else system("rm " USERDATA_PATH "/show_24hour");
			}
		}
		
        if (dirty) {
            validate();
            GFX_clear(screen);
            
            const char* hour_mode_hint = show_24hour ? "12H" : "24H";
            SysUI_SetBottomHints("SELECT", hour_mode_hint, "A", "SET", "B", "BACK");

            int ox = (screen->w - (show_24hour?SCALE1(188):SCALE1(223))) / 2;
            int x = ox;

            int content_total_height = SCALE1(DIGIT_HEIGHT + 22);
            int y = (screen->h - content_total_height) / 2;
            
            x = blitNumber(year_selected, x,y); x = blit(CHAR_SLASH, x,y); x = blitNumber(month_selected, x,y); x = blit(CHAR_SLASH, x,y); x = blitNumber(day_selected, x,y);
            x += SCALE1(10);
            
            int am_selected = hour_selected < 12;
            if (show_24hour) { x = blitNumber(hour_selected, x,y); }
            else { int hour = hour_selected; if (hour==0) hour = 12; else if (hour>12) hour -= 12; x = blitNumber(hour, x,y); }
            x = blit(CHAR_COLON, x,y); x = blitNumber(minute_selected, x,y); x = blit(CHAR_COLON, x,y); x = blitNumber(seconds_selected, x,y);
            
            int ampm_w = 0;
            if (!show_24hour) {
                x += SCALE1(10);
                SDL_Surface* text = TTF_RenderUTF8_Blended(font.large, am_selected ? "AM" : "PM", COLOR_WHITE);
                ampm_w = text->w + SCALE1(2);
                SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){x,y-SCALE1(3)});
                SDL_FreeSurface(text);
            }
        
            x = ox; y += SCALE1(19);
            if (select_cursor!=CURSOR_YEAR) { x += SCALE1(50); x += (select_cursor - 1) * SCALE1(30); }
            blitBar(x,y, (select_cursor==CURSOR_YEAR ? SCALE1(40) : (select_cursor==CURSOR_AMPM ? ampm_w : SCALE1(20))));
        
            SysUI_Render();

            GFX_flip(screen);
            dirty = 0;
        } else GFX_sync();
	}
	
	SDL_FreeSurface(digits);
	if (save_changes) PLAT_setDateTime(year_selected, month_selected, day_selected, hour_selected, minute_selected, seconds_selected);
    return 0;
}

static void plugin_quit(void) {
    PWR_quit();
    SysUI_Quit();
}

static NextUI_Plugin clock_plugin = {
    .name = "Clock",
    .init = plugin_init,
    .run = plugin_run,
    .quit = plugin_quit,
};

NextUI_Plugin* GetPlugin(void) {
    return &clock_plugin;
}