#include "clock.h"
#include "rtc.h"
#include <stdio.h>
#include <string.h>
//////////////////////////////////////////////////////////////////////////////////
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32F407开发板
//LVGL 时钟界面
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2026/8/11
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2014-2024
//All rights reserved

//********************************************************************************
//功能说明
//1,表盘实时显示当前时间(时针/分针/秒针),表盘中央显示数字时间
//2,表盘下方显示当前日期(YYYY-MM-DD 星期)
//3,点击右下角SET按钮弹出设置界面,可设置时间(时/分/秒)和日期(年/月/日)
//////////////////////////////////////////////////////////////////////////////////

/* 表盘几何参数 */
#define FACE_X       50      /* 表盘在屏幕上的X坐标 */
#define FACE_Y       60      /* 表盘在屏幕上的Y坐标 */
#define FACE_SIZE    380     /* 表盘尺寸(宽=高) */
#define CX           190     /* 表盘中心X(相对表盘对象) */
#define CY           190     /* 表盘中心Y(相对表盘对象) */
#define TICK_OUT     180     /* 主刻度外半径 */
#define TICK_IN      170     /* 主刻度内半径 */
#define HOUR_LEN     110     /* 时针长度 */
#define MIN_LEN      145     /* 分针长度 */
#define SEC_LEN      160     /* 秒针长度 */

static lv_obj_t * hour_hand;    /* 时针 */
static lv_obj_t * min_hand;     /* 分针 */
static lv_obj_t * sec_hand;     /* 秒针 */
static lv_obj_t * time_label;   /* 表盘中央数字时间 */
static lv_obj_t * date_label;   /* 日期显示 */

static lv_point_t hand_pts[3][2];   /* 指针端点(0=时针,1=分针,2=秒针) */

static lv_obj_t * set_scr;          /* 设置界面根对象 */
static lv_obj_t * roll_h, * roll_m, * roll_s;   /* 时/分/秒 roller */
static lv_obj_t * roll_y, * roll_mo, * roll_d;  /* 年/月/日 roller */

static const char * week_str[8] = {"", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

static void clock_refresh_cb(lv_timer_t * t);   /* 每秒刷新回调 */
static void create_ticks(lv_obj_t * parent);    /* 创建表盘刻度 */
static void create_clock_numbers(lv_obj_t * parent);   /* 创建12/3/6/9数字 */
static void set_btn_event_cb(lv_event_t * e);   /* 设置按钮事件 */
static void ok_btn_event_cb(lv_event_t * e);    /* 确定按钮事件 */
static void cancel_btn_event_cb(lv_event_t * e);/* 取消按钮事件 */
static void create_setting_screen(void);        /* 创建时间设置界面 */
static lv_obj_t * clock_roller_create(lv_obj_t * parent, int from, int to); /* 创建roller */

//更新指针位置
//idx:0=时针,1=分针,2=秒针
//angle:角度(0~360,0度指向12点)
//len:指针长度
static void update_hand(u8 idx, lv_obj_t * hand, int16_t angle, lv_coord_t len)
{
	hand_pts[idx][0].x = CX;
	hand_pts[idx][0].y = CY;
	hand_pts[idx][1].x = CX + (int32_t)len * lv_trigo_sin(angle) / 32767;
	hand_pts[idx][1].y = CY - (int32_t)len * lv_trigo_cos(angle) / 32767;
	lv_line_set_points(hand, hand_pts[idx], 2);
}

//每秒刷新回调:读取RTC,更新指针和日期显示
static void clock_refresh_cb(lv_timer_t * t)
{
	u8 hour, min, sec, year, month, day, week;

	RTC_Get_Time(&hour, &min, &sec, NULL);
	RTC_Get_Date(&year, &month, &day, &week);

	/* 更新三根指针:时针每小时30度,分针每分钟6度,秒针每秒钟6度 */
	update_hand(0, hour_hand, (hour % 12) * 30 + min / 2, HOUR_LEN);
	update_hand(1, min_hand, min * 6 + sec / 10, MIN_LEN);
	update_hand(2, sec_hand, sec * 6, SEC_LEN);

	/* 更新表盘中央数字时间 */
	lv_label_set_text_fmt(time_label, "%02d:%02d:%02d", hour, min, sec);

	/* 更新日期 */
	lv_label_set_text_fmt(date_label, "20%02d-%02d-%02d %s", year, month, day, week_str[week % 8]);
}

//创建表盘刻度(12个主刻度,3/6/9/12方向刻度加粗加长)
static void create_ticks(lv_obj_t * parent)
{
	static lv_point_t tick_pts[12][2];
	int i;

	for(i = 0; i < 12; i++)
	{
		int16_t a = i * 30;
		lv_coord_t tin = (i % 3 == 0) ? 158 : TICK_IN;	/* 整点刻度加长 */

		tick_pts[i][0].x = CX + (int32_t)TICK_OUT * lv_trigo_sin(a) / 32767;
		tick_pts[i][0].y = CY - (int32_t)TICK_OUT * lv_trigo_cos(a) / 32767;
		tick_pts[i][1].x = CX + (int32_t)tin * lv_trigo_sin(a) / 32767;
		tick_pts[i][1].y = CY - (int32_t)tin * lv_trigo_cos(a) / 32767;

		lv_obj_t * line = lv_line_create(parent);
		lv_line_set_points(line, tick_pts[i], 2);
		lv_obj_set_style_line_width(line, (i % 3 == 0) ? 5 : 2, 0);
		lv_obj_set_style_line_color(line, lv_color_white(), 0);
		lv_obj_set_style_line_rounded(line, true, 0);
	}
}

//创建表盘数字12/3/6/9
static void create_clock_numbers(lv_obj_t * parent)
{
	lv_obj_t * n;

	n = lv_label_create(parent);
	lv_label_set_text(n, "12");
	lv_obj_set_style_text_font(n, &lv_font_montserrat_20, 0);
	lv_obj_set_style_text_color(n, lv_color_white(), 0);
	lv_obj_align(n, LV_ALIGN_TOP_MID, 0, 60);		/* 12下移靠近中心,避免压住顶部刻度 */

	n = lv_label_create(parent);
	lv_label_set_text(n, "3");
	lv_obj_set_style_text_font(n, &lv_font_montserrat_20, 0);
	lv_obj_set_style_text_color(n, lv_color_white(), 0);
	lv_obj_align(n, LV_ALIGN_RIGHT_MID, -60, 0);	/* 3左移靠近中心,避免压住右侧刻度 */

	n = lv_label_create(parent);
	lv_label_set_text(n, "6");
	lv_obj_set_style_text_font(n, &lv_font_montserrat_20, 0);
	lv_obj_set_style_text_color(n, lv_color_white(), 0);
	lv_obj_align(n, LV_ALIGN_BOTTOM_MID, 0, -60);	/* 6上移靠近中心,避免压住底部刻度 */

	n = lv_label_create(parent);
	lv_label_set_text(n, "9");
	lv_obj_set_style_text_font(n, &lv_font_montserrat_20, 0);
	lv_obj_set_style_text_color(n, lv_color_white(), 0);
	lv_obj_align(n, LV_ALIGN_LEFT_MID, 60, 0);		/* 9右移靠近中心,避免压住左侧刻度 */
}

//创建roller选择器
//from,to:选项范围,选项以两位数字显示(年份如2026不受两位限制)
//返回值:roller对象
static lv_obj_t * clock_roller_create(lv_obj_t * parent, int from, int to)
{
	char buf[512] = {0};
	char tmp[8];
	int i;
	lv_obj_t * roller;

	for(i = from; i <= to; i++)
	{
		sprintf(tmp, "%02d\n", i);
		strcat(buf, tmp);
	}
	buf[strlen(buf) - 1] = '\0';	/* 去掉最后一个换行符 */

	roller = lv_roller_create(parent);
	lv_roller_set_options(roller, buf, LV_ROLLER_MODE_NORMAL);
	lv_roller_set_visible_row_count(roller, 3);
	lv_obj_set_width(roller, 130);
	lv_obj_set_scroll_dir(roller, LV_DIR_TOP | LV_DIR_BOTTOM);	/* 仅允许上下滚动,关闭左右滑动 */
	return roller;
}

//确定按钮:把设置的值写入RTC并关闭设置界面
static void ok_btn_event_cb(lv_event_t * e)
{
	u8 hour, min, sec, month, day;
	u16 year;

	if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;

	hour  = lv_roller_get_selected(roll_h);		/* 时 */
	min   = lv_roller_get_selected(roll_m);		/* 分 */
	sec   = lv_roller_get_selected(roll_s);		/* 秒 */
	year  = 2020 + lv_roller_get_selected(roll_y);	/* 年 */
	month = 1 + lv_roller_get_selected(roll_mo);	/* 月 */
	day   = 1 + lv_roller_get_selected(roll_d);		/* 日 */

	RTC_Set_Time(hour, min, sec, RTC_H12_AM);
	RTC_Set_Date(year - 2000, month, day, RTC_Get_Week(year, month, day));

	lv_obj_del(set_scr);
	set_scr = NULL;
	clock_refresh_cb(NULL);		/* 立即刷新主界面 */
}

//取消按钮:关闭设置界面,不保存
static void cancel_btn_event_cb(lv_event_t * e)
{
	if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;

	lv_obj_del(set_scr);
	set_scr = NULL;
}

//创建时间日期设置界面(半透明覆盖层)
static void create_setting_screen(void)
{
	u8 hour, min, sec, year, month, day, week;
	lv_obj_t * panel;
	lv_obj_t * title;
	lv_obj_t * ok_btn, * cancel_btn;
	lv_obj_t * ok_l, * cancel_l;

	RTC_Get_Time(&hour, &min, &sec, NULL);
	RTC_Get_Date(&year, &month, &day, &week);

	/* 半透明覆盖层,拦截主界面点击 */
	set_scr = lv_obj_create(lv_layer_top());
	lv_obj_remove_style_all(set_scr);
	lv_obj_set_size(set_scr, 480, 800);
	lv_obj_set_pos(set_scr, 0, 0);
	lv_obj_clear_flag(set_scr, LV_OBJ_FLAG_SCROLLABLE);	/* 关闭覆盖层滚动 */
	lv_obj_set_style_bg_opa(set_scr, LV_OPA_70, 0);
	lv_obj_set_style_bg_color(set_scr, lv_color_hex(0x000000), 0);

	/* 设置面板 */
	panel = lv_obj_create(set_scr);
	lv_obj_set_size(panel, 440, 660);
	lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
	lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);		/* 关闭面板滚动 */

	title = lv_label_create(panel);
	lv_label_set_text(title, "SET TIME AND DATE");
	lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

	/* 时间行: 时/分/秒 */
	roll_h = clock_roller_create(panel, 0, 23);
	roll_m = clock_roller_create(panel, 0, 59);
	roll_s = clock_roller_create(panel, 0, 59);
	lv_roller_set_selected(roll_h, hour, LV_ANIM_OFF);
	lv_roller_set_selected(roll_m, min, LV_ANIM_OFF);
	lv_roller_set_selected(roll_s, sec, LV_ANIM_OFF);
	lv_obj_set_pos(roll_h, 15, 80);
	lv_obj_set_pos(roll_m, 155, 80);
	lv_obj_set_pos(roll_s, 295, 80);

	/* 日期行: 年/月/日 */
	roll_y  = clock_roller_create(panel, 2020, 2039);
	roll_mo = clock_roller_create(panel, 1, 12);
	roll_d  = clock_roller_create(panel, 1, 31);
	lv_roller_set_selected(roll_y, 2000 + year - 2020, LV_ANIM_OFF);
	lv_roller_set_selected(roll_mo, month - 1, LV_ANIM_OFF);
	lv_roller_set_selected(roll_d, day - 1, LV_ANIM_OFF);
	lv_obj_set_pos(roll_y, 15, 280);
	lv_obj_set_pos(roll_mo, 155, 280);
	lv_obj_set_pos(roll_d, 295, 280);

	/* 确定/取消按钮 */
	ok_btn = lv_btn_create(panel);
	lv_obj_set_size(ok_btn, 150, 50);
	lv_obj_align(ok_btn, LV_ALIGN_BOTTOM_LEFT, 30, -30);
	lv_obj_add_event_cb(ok_btn, ok_btn_event_cb, LV_EVENT_ALL, NULL);
	ok_l = lv_label_create(ok_btn);
	lv_label_set_text(ok_l, "OK");
	lv_obj_center(ok_l);

	cancel_btn = lv_btn_create(panel);
	lv_obj_set_size(cancel_btn, 150, 50);
	lv_obj_align(cancel_btn, LV_ALIGN_BOTTOM_RIGHT, -30, -30);
	lv_obj_add_event_cb(cancel_btn, cancel_btn_event_cb, LV_EVENT_ALL, NULL);
	cancel_l = lv_label_create(cancel_btn);
	lv_label_set_text(cancel_l, "CANCEL");
	lv_obj_center(cancel_l);
}

//设置按钮:弹出时间设置界面
static void set_btn_event_cb(lv_event_t * e)
{
	if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
	if(set_scr == NULL) create_setting_screen();	/* 防止重复弹出 */
}

//时钟主界面初始化:创建表盘/指针/日期/设置按钮,并启动每秒刷新
void clock_ui_init(void)
{
	lv_obj_t * scr = lv_scr_act();
	lv_obj_t * face;
	lv_obj_t * dot;
	lv_obj_t * set_btn, * set_l;

	/* 屏幕背景 */
	lv_obj_set_style_bg_color(scr, lv_color_hex(0x0f1420), 0);

	/* 表盘 */
	face = lv_obj_create(scr);
	lv_obj_remove_style_all(face);
	lv_obj_set_size(face, FACE_SIZE, FACE_SIZE);
	lv_obj_set_pos(face, FACE_X, FACE_Y);
	lv_obj_clear_flag(face, LV_OBJ_FLAG_SCROLLABLE);	/* 关闭表盘滚动,防止拖动移位 */
	lv_obj_set_style_bg_color(face, lv_color_hex(0x1b2a4a), 0);
	lv_obj_set_style_radius(face, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_border_color(face, lv_color_hex(0x2e86de), 0);
	lv_obj_set_style_border_width(face, 5, 0);
	lv_obj_set_style_pad_all(face, 0, 0);

	create_ticks(face);				/* 表盘刻度 */
	create_clock_numbers(face);		/* 12/3/6/9数字 */

	/* 时针 */
	hour_hand = lv_line_create(face);
	lv_obj_set_style_line_width(hour_hand, 7, 0);
	lv_obj_set_style_line_color(hour_hand, lv_color_white(), 0);
	lv_obj_set_style_line_rounded(hour_hand, true, 0);

	/* 分针 */
	min_hand = lv_line_create(face);
	lv_obj_set_style_line_width(min_hand, 4, 0);
	lv_obj_set_style_line_color(min_hand, lv_color_white(), 0);
	lv_obj_set_style_line_rounded(min_hand, true, 0);

	/* 秒针 */
	sec_hand = lv_line_create(face);
	lv_obj_set_style_line_width(sec_hand, 2, 0);
	lv_obj_set_style_line_color(sec_hand, lv_color_hex(0xff4757), 0);
	lv_obj_set_style_line_rounded(sec_hand, true, 0);

	/* 中心圆点 */
	dot = lv_obj_create(face);
	lv_obj_remove_style_all(dot);
	lv_obj_set_size(dot, 16, 16);
	lv_obj_set_style_bg_color(dot, lv_color_hex(0xff4757), 0);
	lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
	lv_obj_align(dot, LV_ALIGN_CENTER, 0, 0);

	/* 表盘下方数字时间(独立显示,不与表盘重叠) */
	time_label = lv_label_create(scr);
	lv_obj_set_style_text_font(time_label, &lv_font_montserrat_28, 0);
	lv_obj_set_style_text_color(time_label, lv_color_white(), 0);
	lv_obj_set_width(time_label, 480);
	lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_pos(time_label, 0, 452);

	/* 表盘下方日期显示 */
	date_label = lv_label_create(scr);
	lv_obj_set_style_text_font(date_label, &lv_font_montserrat_20, 0);
	lv_obj_set_style_text_color(date_label, lv_color_hex(0x74b9ff), 0);
	lv_obj_set_pos(date_label, 0, 510);
	lv_obj_set_width(date_label, 480);
	lv_obj_set_style_text_align(date_label, LV_TEXT_ALIGN_CENTER, 0);

	/* 右下角设置按钮 */
	set_btn = lv_btn_create(scr);
	lv_obj_set_size(set_btn, 100, 50);
	lv_obj_set_pos(set_btn, 355, 720);
	lv_obj_add_event_cb(set_btn, set_btn_event_cb, LV_EVENT_ALL, NULL);
	set_l = lv_label_create(set_btn);
	lv_label_set_text(set_l, "SET");
	lv_obj_center(set_l);

	/* 每秒刷新定时器 */
	lv_timer_create(clock_refresh_cb, 1000, NULL);
	clock_refresh_cb(NULL);		/* 立即刷新一次 */
}
