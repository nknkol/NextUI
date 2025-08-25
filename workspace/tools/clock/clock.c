#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <msettings.h>
#include "defines.h"
#include "api.h"
#include "utils.h"
#include "sysui.h"

enum {
	CURSOR_YEAR,
	CURSOR_MONTH,
	CURSOR_DAY,
	CURSOR_HOUR,
	CURSOR_MINUTE,
	CURSOR_SECOND,
	CURSOR_AMPM,
};

int main(int argc , char* argv[]) {
	PWR_setCPUSpeed(CPU_SPEED_MENU);
	
	SDL_Surface* screen = GFX_init(MODE_MAIN);
	PAD_init();
	PWR_init();
	InitSettings();
	
	SysUI_Init(screen, &font); 

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

	int quit = 0;
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
	int32_t am_selected = tm.tm_hour < 12;

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
			n = num / 1000;
			num -= n * 1000;
			x = blit(n, x,y);
			n = num / 100;
			num -= n * 100;
			x = blit(n, x,y);
		}
		n = num / 10;
		num -= n * 10;
		x = blit(n, x,y);
		n = num;
		x = blit(n, x,y);
		return x;
	}
	void validate(void) {
		uint32_t february_days = 28;
		if ( ((year_selected % 4 == 0) && (year_selected % 100 != 0)) || (year_selected % 400 == 0)) february_days = 29;
		if (month_selected > 12) month_selected -= 12;
		else if (month_selected < 1) month_selected += 12;
		if (year_selected > 2100) year_selected = 2100;
		else if (year_selected < 1970) year_selected = 1970;
		switch(month_selected) {
			case 2:
				if (day_selected > february_days) day_selected -= february_days;
				else if (day_selected<1) day_selected += february_days;
				break;
			case 4: case 6: case 9: case 11:
				if (day_selected > 30) day_selected -= 30;
				else if (day_selected < 1) day_selected += 30;
				break;
			default:
				if (day_selected > 31) day_selected -= 31;
				else if (day_selected < 1) day_selected += 31;
				break;
		}
		if (hour_selected > 23) hour_selected -= 24;
		else if (hour_selected < 0) hour_selected += 24;
		if (minute_selected > 59) minute_selected -= 60;
		else if (minute_selected < 0) minute_selected += 60;
		if (seconds_selected > 59) seconds_selected -= 60;
		else if (seconds_selected < 0) seconds_selected += 60;
	}

	int option_count = 7;
	int dirty = 1;
	bool sys_input_handled = false; // 新增：用于标记SysUI是否处理了输入

	while(!quit) {
		// uint32_t frame_start = SDL_GetTicks();
		
		PAD_poll();

		// 让 SysUI 首先处理系统级输入（快捷键等）
		// 如果它返回true，说明有状态更新（如浮层显示/隐藏），需要重绘
		sys_input_handled = SysUI_Update();
		if (sys_input_handled) {
			dirty = 1;
		}

		// 仅当 SysUI 没有处理任何快捷键时，才执行 clock 程序的自有按键逻辑
		if (!sys_input_handled) {
			if (PAD_justRepeated(BTN_UP)) {
				dirty = 1;
				switch(select_cursor) {
					case CURSOR_YEAR: year_selected++; break;
					case CURSOR_MONTH: month_selected++; break;
					case CURSOR_DAY: day_selected++; break;
					case CURSOR_HOUR: hour_selected++; break;
					case CURSOR_MINUTE: minute_selected++; break;
					case CURSOR_SECOND: seconds_selected++; break;
					case CURSOR_AMPM: hour_selected += 12; break;
				}
			}
			else if (PAD_justRepeated(BTN_DOWN)) {
				dirty = 1;
				switch(select_cursor) {
					case CURSOR_YEAR: year_selected--; break;
					case CURSOR_MONTH: month_selected--; break;
					case CURSOR_DAY: day_selected--; break;
					case CURSOR_HOUR: hour_selected--; break;
					case CURSOR_MINUTE: minute_selected--; break;
					case CURSOR_SECOND: seconds_selected--; break;
					case CURSOR_AMPM: hour_selected -= 12; break;
				}
			}
			else if (PAD_justRepeated(BTN_LEFT)) {
				dirty = 1;
				select_cursor--;
				if (select_cursor < 0) select_cursor += option_count;
			}
			else if (PAD_justRepeated(BTN_RIGHT)) {
				dirty = 1;
				select_cursor++;
				if (select_cursor >= option_count) select_cursor -= option_count;
			}
			else if (PAD_justPressed(BTN_A)) {
				save_changes = 1;
				quit = 1;
			}
			else if (PAD_justPressed(BTN_B)) {
				quit = 1;
			}
			else if (PAD_justPressed(BTN_SELECT)) {
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

			// --- 设置UI意图 ---
			SysUI_SetTitle("Set Date & Time");
			SysUI_SetBottomHints(
				"SELECT", show_24hour ? "12 HOUR" : "24 HOUR",
				"B", "CANCEL",
				"A", "SET"
			);

			// --- 渲染时钟的核心内容 ---
			int ox = (screen->w - (show_24hour?SCALE1(188):SCALE1(223))) / 2;
			int x = ox;
			int y = SCALE1((((FIXED_HEIGHT / FIXED_SCALE)-PILL_SIZE-DIGIT_HEIGHT)/2));
			x = blitNumber(year_selected, x,y);
			x = blit(CHAR_SLASH, x,y);
			x = blitNumber(month_selected, x,y);
			x = blit(CHAR_SLASH, x,y);
			x = blitNumber(day_selected, x,y);
			x += SCALE1(10);
			am_selected = hour_selected < 12;
			if (show_24hour) {
				 x = blitNumber(hour_selected, x,y);
			} else {
				int hour = hour_selected;
				if (hour==0) hour = 12;
				else if (hour>12) hour -= 12;
				x = blitNumber(hour, x,y);
			}
			x = blit(CHAR_COLON, x,y);
			x = blitNumber(minute_selected, x,y);
			x = blit(CHAR_COLON, x,y);
			x = blitNumber(seconds_selected, x,y);
			int ampm_w = 0;
			if (!show_24hour) {
				x += SCALE1(10);
				SDL_Surface* text = TTF_RenderUTF8_Blended(font.large, am_selected ? "AM" : "PM", COLOR_WHITE);
				ampm_w = text->w + SCALE1(2);
				SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){x,y-SCALE1(3)});
				SDL_FreeSurface(text);
			}
			x = ox;
			y += SCALE1(19);
			if (select_cursor!=CURSOR_YEAR) {
				x += SCALE1(50);
				x += (select_cursor - 1) * SCALE1(30);
			}
			blitBar(x,y, (select_cursor==CURSOR_YEAR ? SCALE1(40) : (select_cursor==CURSOR_AMPM ? ampm_w : SCALE1(20))));

			// 确保调用 SysUI_Render() 来绘制顶部/底部栏和任何活动的浮层
			SysUI_Render(); 

			GFX_flip(screen);
			dirty = 0;
		}
		else {
			GFX_sync();
		}
	}
	
	SDL_FreeSurface(digits);
	
	SysUI_Quit(); 
	QuitSettings();
	PWR_quit();
	PAD_quit();
	GFX_quit();
	
	if (save_changes) {
		PLAT_setDateTime(year_selected, month_selected, day_selected, hour_selected, minute_selected, seconds_selected);
	}
	
	return EXIT_SUCCESS;
}