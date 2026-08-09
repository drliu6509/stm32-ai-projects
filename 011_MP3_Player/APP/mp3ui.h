#ifndef __MP3UI_H
#define __MP3UI_H
#include "sys.h"
//////////////////////////////////////////////////////////////////////////////////	 
//本软件只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32F407探索者开发板
//APP-MP3播放器触摸屏界面	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//修改日期:2026/8/9
//版本：V1.0
//版权所有，盗版必究
//Copyright(C) 正点原子@ALIENTEK 2014-2024
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 	 
//说明
//本文件为MP3播放器的自绘触摸屏界面,不依赖GUI库.
//使用LCD基础绘图函数+触摸检测实现按钮控制.
//按钮:上一曲/播放暂停/下一曲/音量-/音量+
////////////////////////////////////////////////////////////////////////////////// 	 

//触摸动作定义
#define MP3UI_ACTION_NONE  0	//无动作
#define MP3UI_ACTION_NEXT  1	//下一曲
#define MP3UI_ACTION_PREV  2	//上一曲

void mp3ui_init(void);								//初始化UI(绘制主界面)
void mp3ui_show_song(u8 *name,u16 cur,u16 total);	//显示歌曲名和索引
void mp3ui_show_msg(u8 *msg);						//显示提示信息
void mp3ui_time_refresh(void);						//刷新时间/码率显示
void mp3ui_play_ctrl(void);							//播放过程中的触摸控制(由mp3_play_song循环调用)
void mp3ui_set_play(u8 play);						//设置播放/暂停按钮状态(1播放,0暂停)
void mp3ui_set_action(u8 action);					//设置动作标志
u8   mp3ui_get_action(void);						//获取动作标志
#endif
