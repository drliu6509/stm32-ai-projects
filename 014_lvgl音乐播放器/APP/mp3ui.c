#include "mp3ui.h"
#include "lvgl.h"
#include "font_lib.h"
#include "audioplay.h"
#include "wm8978.h"
#include "string.h"
#include "stdio.h"
//////////////////////////////////////////////////////////////////////////////////
//	本文件仅供学习使用，未经允许，不得用于任何用途
//ALIENTEK STM32F407探索者开发板
//APP-MP3音乐播放器 播放界面(LVGL版)
//版权: 正点原子@ALIENTEK
//论坛: www.openedv.com
//修改日期:2026/8/11
//版本:V1.0
//版权所有，盗版必究
//Copyright(C) 正点原子@ALIENTEK 2014-2024
//All rights reserved
//////////////////////////////////////////////////////////////////////////////////
//说明
//使用LVGL实现音乐播放界面:
//1, 显示封面图(LVGL演示页/ID3内嵌封面)
//2, 显示歌名/序号/时间/码率
//3, 进度条支持拖动跳转
//4, 按钮: 上一曲/播放暂停/下一曲/音量-/音量+/返回
//为了优先保证音乐解码, 界面不使用动画, 播放期间每10ms才服务一次LVGL.
//////////////////////////////////////////////////////////////////////////////////

//播放界面控件
static lv_obj_t *play_scr;			//播放屏幕
static lv_obj_t *back_btn;			//返回按钮
static lv_obj_t *cover_img;			//封面图
static lv_obj_t *cover_ph;			//封面占位图
static lv_obj_t *title_label;		//歌名/消息
static lv_obj_t *info_label;		//序号
static lv_obj_t *time_label;		//时间/码率
static lv_obj_t *prog_slider;		//进度条
static lv_obj_t *prev_btn;			//上一曲
static lv_obj_t *play_btn;			//播放/暂停
static lv_obj_t *play_btn_label;	//播放按钮文字
static lv_obj_t *next_btn;			//下一曲
static lv_obj_t *voldn_btn;			//音量-
static lv_obj_t *volup_btn;			//音量+
static lv_obj_t *vol_label;			//音量显示

//状态变量
static u8 mp3ui_action=MP3UI_ACTION_NONE;			//当前动作标志
static u8 mp3ui_vol=40;								//当前音量(0~63)
static u8 mp3ui_playing=1;							//当前播放状态(1播放,0暂停)
static u8 mp3ui_dragging=0;							//是否正在拖动进度条(1拖动,0未拖动)
static u32 mp3ui_last_cur=0XFFFFFFFF;				//上次显示的当前时间
static u32 mp3ui_last_tot=0XFFFFFFFF;				//上次显示的总时间
static u32 mp3ui_last_bit=0XFFFFFFFF;				//上次显示的码率

//封面图像描述符(数据指针指向外部解码缓冲,由music_player提供)
static lv_img_dsc_t cover_dsc;

//返回按钮回调(由music_player注册,用于返回列表页)
static void (*mp3ui_back_cb)(void)=NULL;

//更新时间显示
//sec:当前时间(秒)
//total:总时间(秒)
//bit:码率(位/秒)
static void mp3ui_time_label_update(u32 sec,u32 total,u32 bit)
{
	u8 buf[48];
	if(total>9999)total=9999;	//防止过长显示溢出
	sprintf((char*)buf,"%02d:%02d / %02d:%02d  %dkbps",(int)(sec/60),(int)(sec%60),
			(int)(total/60),(int)(total%60),(int)(bit/1000));
	lv_label_set_text(time_label,(const char*)buf);
}

//显示音量
static void mp3ui_vol_show(void)
{
	u8 buf[24];
	sprintf((char*)buf,"音量: %d",(int)mp3ui_vol);
	lv_label_set_text(vol_label,(const char*)buf);
}

//界面刷新(仅当数值变化时更新,减少重绘开销)
static void mp3ui_ui_refresh(void)
{
	if(audiodev.cursec!=mp3ui_last_cur||audiodev.totsec!=mp3ui_last_tot||audiodev.bitrate!=mp3ui_last_bit)
	{
		mp3ui_last_cur=audiodev.cursec;
		mp3ui_last_tot=audiodev.totsec;
		mp3ui_last_bit=audiodev.bitrate;
		mp3ui_time_label_update(audiodev.cursec,audiodev.totsec,audiodev.bitrate);
		//未拖动进度条时,才更新进度条位置
		if(!mp3ui_dragging&&audiodev.totsec)
		{
			u32 val=(u32)((unsigned long long)audiodev.cursec*1000/audiodev.totsec);
			lv_slider_set_value(prog_slider,val,LV_ANIM_OFF);
		}
	}
}

//按钮事件处理
//e:LVGL事件指针
static void mp3ui_evt_handler(lv_event_t *e)
{
	lv_event_code_t code=lv_event_get_code(e);
	if(code==LV_EVENT_CLICKED)
	{
		lv_obj_t *obj=lv_event_get_target(e);
		if(obj==back_btn)							//返回列表
		{
			audiodev.status&=~(1<<1);				//退出当前解码循环
			if(mp3ui_back_cb)mp3ui_back_cb();		//通知music_player返回列表
		}
		else if(obj==prev_btn)						//上一曲
		{
			mp3ui_action=MP3UI_ACTION_PREV;			//设置动作
			audiodev.status&=~(1<<1);				//退出当前解码循环,切换歌曲
		}
		else if(obj==next_btn)						//下一曲
		{
			mp3ui_action=MP3UI_ACTION_NEXT;			//设置动作
			audiodev.status&=~(1<<1);				//退出当前解码循环,切换歌曲
		}
		else if(obj==play_btn)						//播放/暂停
		{
			if(audiodev.status&0X01)				//当前播放,执行暂停
			{
				audiodev.status&=~(1<<0);			//暂停状态
				mp3ui_set_play(0);
			}
			else									//当前暂停,恢复播放
			{
				audiodev.status|=1<<0;				//播放状态
				mp3ui_set_play(1);
			}
		}
		else if(obj==voldn_btn)						//音量-
		{
			if(mp3ui_vol>0)mp3ui_vol--;				//音量递减
			WM8978_HPvol_Set(mp3ui_vol,mp3ui_vol);
			WM8978_SPKvol_Set(mp3ui_vol);
			mp3ui_vol_show();						//刷新音量显示
		}
		else if(obj==volup_btn)						//音量+
		{
			if(mp3ui_vol<63)mp3ui_vol++;			//音量递增
			WM8978_HPvol_Set(mp3ui_vol,mp3ui_vol);
			WM8978_SPKvol_Set(mp3ui_vol);
			mp3ui_vol_show();						//刷新音量显示
		}
	}
}

//进度条事件处理(支持拖动跳转)
//e:LVGL事件指针
static void mp3ui_slider_evt(lv_event_t *e)
{
	lv_event_code_t code=lv_event_get_code(e);
	if(code==LV_EVENT_PRESSED)						//开始拖动
	{
		mp3ui_dragging=1;
	}
	else if(code==LV_EVENT_PRESSING)				//拖动中,实时显示目标时间
	{
		if(audiodev.totsec)
		{
			u32 val=lv_slider_get_value(prog_slider);
			u32 sec=(u32)((unsigned long long)val*audiodev.totsec/1000);
			mp3ui_time_label_update(sec,audiodev.totsec,audiodev.bitrate);
		}
	}
	else if(code==LV_EVENT_RELEASED)				//松开,执行跳转
	{
		if(audiodev.totsec)
		{
			u32 val=lv_slider_get_value(prog_slider);
			audiodev.seeksec=(u32)((unsigned long long)val*audiodev.totsec/1000);//设置跳转目标时间
		}
		mp3ui_dragging=0;
	}
}

//创建一个按钮
//parent:父对象
//x,y,w,h:位置和大小
//label:按钮文字
static lv_obj_t *mp3ui_btn_create(lv_obj_t *parent,u16 x,u16 y,u16 w,u16 h,const char *label)
{
	lv_obj_t *btn=lv_btn_create(parent);
	lv_obj_set_pos(btn,x,y);
	lv_obj_set_size(btn,w,h);
	lv_obj_add_event_cb(btn,mp3ui_evt_handler,LV_EVENT_CLICKED,NULL);
	lv_obj_set_style_anim_time(btn,0,0);				//禁止按下动画,优先解码
	lv_obj_t *lbl=lv_label_create(btn);
	lv_obj_set_style_text_font(lbl,font_lib_get_font(),0);
	lv_label_set_text(lbl,label);
	lv_obj_center(lbl);
	return btn;
}

//初始化UI,创建播放界面(不加载,由music_player在播放时加载)
void mp3ui_init(void)
{
	lv_obj_t *lbl;
	play_scr=lv_obj_create(NULL);
	lv_obj_set_style_bg_color(play_scr,lv_color_hex(0x16213E),0);	//深蓝背景
	lv_obj_set_style_bg_opa(play_scr,LV_OPA_COVER,0);
	lv_obj_set_style_anim_time(play_scr,0,0);						//禁止界面动画

	//顶部: 返回按钮 + 音量显示
	back_btn=mp3ui_btn_create(play_scr,10,10,70,44,"返回");
	vol_label=lv_label_create(play_scr);
	lv_obj_set_style_text_font(vol_label,font_lib_get_font(),0);
	lv_obj_set_style_text_color(vol_label,lv_color_hex(0xCCCCCC),0);
	lv_obj_set_pos(vol_label,340,20);
	mp3ui_vol_show();

	//封面图(默认隐藏,无封面时显示占位)
	cover_img=lv_img_create(play_scr);
	lv_obj_set_pos(cover_img,120,56);
	lv_obj_set_size(cover_img,240,240);
	lv_obj_add_flag(cover_img,LV_OBJ_FLAG_HIDDEN);
	cover_ph=lv_obj_create(play_scr);
	lv_obj_set_pos(cover_ph,120,56);
	lv_obj_set_size(cover_ph,240,240);
	lv_obj_set_style_bg_color(cover_ph,lv_color_hex(0x2A3A66),0);
	lv_obj_set_style_bg_opa(cover_ph,LV_OPA_COVER,0);
	lv_obj_set_style_radius(cover_ph,8,0);
	lbl=lv_label_create(cover_ph);
	lv_obj_set_style_text_font(lbl,font_lib_get_font(),0);
	lv_obj_set_style_text_color(lbl,lv_color_hex(0x888888),0);
	lv_label_set_text(lbl,"无封面");
	lv_obj_center(lbl);

	//歌名/序号
	title_label=lv_label_create(play_scr);
	lv_obj_set_style_text_font(title_label,font_lib_get_font(),0);
	lv_obj_set_style_text_color(title_label,lv_color_hex(0xFFFFFF),0);
	lv_obj_set_width(title_label,460);
	lv_obj_set_style_text_align(title_label,LV_TEXT_ALIGN_CENTER,0);
	lv_label_set_long_mode(title_label,LV_LABEL_LONG_WRAP);			//超长自动换行
	lv_obj_set_pos(title_label,10,312);
	lv_label_set_text(title_label,"音乐播放器");

	info_label=lv_label_create(play_scr);
	lv_obj_set_style_text_font(info_label,font_lib_get_font(),0);
	lv_obj_set_style_text_color(info_label,lv_color_hex(0xAAAAAA),0);
	lv_obj_set_width(info_label,460);
	lv_obj_set_style_text_align(info_label,LV_TEXT_ALIGN_CENTER,0);
	lv_obj_set_pos(info_label,10,352);
	lv_label_set_text(info_label,"");

	//时间显示
	time_label=lv_label_create(play_scr);
	lv_obj_set_style_text_font(time_label,font_lib_get_font(),0);
	lv_obj_set_style_text_color(time_label,lv_color_hex(0xCCCCCC),0);
	lv_obj_set_width(time_label,460);
	lv_obj_set_style_text_align(time_label,LV_TEXT_ALIGN_CENTER,0);
	lv_obj_set_pos(time_label,10,390);
	lv_label_set_text(time_label,"00:00 / 00:00  0kbps");

	//进度条(0~1000,点击/拖动跳转)
	prog_slider=lv_slider_create(play_scr);
	lv_obj_set_pos(prog_slider,30,424);
	lv_obj_set_size(prog_slider,420,22);
	lv_slider_set_range(prog_slider,0,1000);
	lv_slider_set_value(prog_slider,0,LV_ANIM_OFF);
	lv_obj_add_event_cb(prog_slider,mp3ui_slider_evt,LV_EVENT_PRESSED,NULL);
	lv_obj_add_event_cb(prog_slider,mp3ui_slider_evt,LV_EVENT_PRESSING,NULL);
	lv_obj_add_event_cb(prog_slider,mp3ui_slider_evt,LV_EVENT_RELEASED,NULL);
	lv_obj_set_style_anim_time(prog_slider,0,0);					//禁止拖动动画

	//控制按钮行1: 上一曲/播放暂停/下一曲
	prev_btn=mp3ui_btn_create(play_scr,40,470,110,52,"上一曲");
	play_btn=mp3ui_btn_create(play_scr,185,470,110,52,"暂停");
	play_btn_label=lv_obj_get_child(play_btn,0);
	next_btn=mp3ui_btn_create(play_scr,330,470,110,52,"下一曲");
	//控制按钮行2: 音量-/音量+
	voldn_btn=mp3ui_btn_create(play_scr,130,540,100,52,"音量-");
	volup_btn=mp3ui_btn_create(play_scr,250,540,100,52,"音量+");

	mp3ui_action=MP3UI_ACTION_NONE;
	mp3ui_dragging=0;
	mp3ui_last_cur=0XFFFFFFFF;
	mp3ui_last_tot=0XFFFFFFFF;
	mp3ui_last_bit=0XFFFFFFFF;
}

//获取播放屏幕对象
lv_obj_t *mp3ui_get_play_screen(void)
{
	return play_scr;
}

//注册返回按钮回调
//cb:回调函数(返回列表页)
void mp3ui_set_back_cb(void (*cb)(void))
{
	mp3ui_back_cb=cb;
}

//显示歌曲相关信息
//name:歌曲文件名
//cur:当前曲目序号(从1开始)
//total:曲目总数
void mp3ui_show_song(u8 *name,u16 cur,u16 total)
{
	u8 buf[32];
	lv_label_set_text(title_label,(const char*)name);				//显示歌名
	sprintf((char*)buf,"第 %d/%d 首",(int)cur,(int)total);			//显示序号
	lv_label_set_text(info_label,(const char*)buf);
	mp3ui_progbar_draw(0,0);										//重置进度显示
}

//显示提示消息
//msg:提示消息字符串
void mp3ui_show_msg(u8 *msg)
{
	lv_label_set_text(title_label,(const char*)msg);
}

//刷新时间/码率显示
void mp3ui_time_refresh(void)
{
	mp3ui_ui_refresh();
}

//绘制进度条
//cur:当前播放时间(秒)
//total:总时间(秒)
void mp3ui_progbar_draw(u32 cur,u32 total)
{
	if(total==0)													//总时间为0,尚未加载,等待信息
	{
		mp3ui_last_cur=0XFFFFFFFF;
		mp3ui_last_tot=0XFFFFFFFF;
		lv_slider_set_value(prog_slider,0,LV_ANIM_OFF);
		return;
	}
	if(cur>total)cur=total;
	if(!mp3ui_dragging)												//拖动中不更新进度条位置
	{
		u32 val=(u32)((unsigned long long)cur*1000/total);
		lv_slider_set_value(prog_slider,val,LV_ANIM_OFF);
	}
	mp3ui_time_label_update(cur,total,audiodev.bitrate);
}

//设置播放/暂停按钮状态
//play:1,播放状态;0,暂停状态
void mp3ui_set_play(u8 play)
{
	mp3ui_playing=play;
	if(play)
	{
		lv_label_set_text(play_btn_label,"暂停");						//播放中,按钮显示暂停
	}
	else
	{
		lv_label_set_text(play_btn_label,"播放");						//暂停中,按钮显示播放
	}
}

//显示封面图
//data:RGB565解码数据缓冲(由music_player分配,播放期间保持有效)
//w,h:封面宽高
void mp3ui_set_cover(u8 *data,u16 w,u16 h)
{
	u16 zoom;
	if(data==NULL)return;
	cover_dsc.header.always_zero=0;
	cover_dsc.header.w=w;
	cover_dsc.header.h=h;
	cover_dsc.header.cf=LV_IMG_CF_TRUE_COLOR;
	cover_dsc.data_size=(u32)w*h*2;
	cover_dsc.data=data;
	lv_img_set_src(cover_img,&cover_dsc);
	lv_obj_clear_flag(cover_img,LV_OBJ_FLAG_HIDDEN);			//显示封面图
	lv_obj_add_flag(cover_ph,LV_OBJ_FLAG_HIDDEN);				//隐藏占位图
	//封面适配240x240显示区域: 超过则按比例缩小(LVGL缩放,可靠), 不足则1:1显示
	zoom=256;
	if(w>240||h>240)
	{
		u16 m=(w>h)?w:h;										//长边
		zoom=(u16)((u32)240*256/m);								//缩放到240以内
		if(zoom<64)zoom=64;										//最小缩到1/4
	}
	lv_img_set_zoom(cover_img,zoom);
}

//清除封面图(显示占位图)
void mp3ui_clear_cover(void)
{
	lv_img_set_src(cover_img,NULL);
	lv_obj_add_flag(cover_img,LV_OBJ_FLAG_HIDDEN);					//隐藏封面图
	lv_obj_clear_flag(cover_ph,LV_OBJ_FLAG_HIDDEN);					//显示占位图
}

//设置动作标志
void mp3ui_set_action(u8 action)
{
	mp3ui_action=action;
}

//获取动作标志
u8 mp3ui_get_action(void)
{
	return mp3ui_action;
}

//播放过程中的处理函数
//在mp3_play_song/wav_play_song的解码循环中调用,实现LVGL事件服务
//为了优先保证音乐解码, 每10ms才调用一次lv_timer_handler
void mp3ui_play_ctrl(void)
{
	static u32 serv_tick=0;
	u32 now=lv_tick_get();
	mp3ui_ui_refresh();												//按需刷新时间/进度
	if(now-serv_tick>=10)											//每10ms服务一次LVGL
	{
		serv_tick=now;
		lv_timer_handler();
	}
}
