#ifndef __MP3UI_H
#define __MP3UI_H
#include "sys.h"
#include "lvgl.h"
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
//本文件为音乐播放器播放界面接口,使用LVGL实现:
//按钮: 上一曲/播放暂停/下一曲/音量-/音量+/返回
//进度条: 支持拖动跳转播放位置
//封面: 显示MP3内嵌封面(ID3 APIC)或LVGL演示封面
//////////////////////////////////////////////////////////////////////////////////

//播放动作标志
#define MP3UI_ACTION_NONE  0    //无动作
#define MP3UI_ACTION_NEXT  1    //下一曲
#define MP3UI_ACTION_PREV  2    //上一曲

void mp3ui_init(void);                                                         //初始化UI(创建播放界面)
lv_obj_t *mp3ui_get_play_screen(void);                                        //获取播放屏幕对象
void mp3ui_set_back_cb(void (*cb)(void));                                     //注册返回列表回调
void mp3ui_show_song(u8 *name,u16 cur,u16 total);                             //显示歌曲相关信息
void mp3ui_show_msg(u8 *msg);                                                 //显示提示消息
void mp3ui_time_refresh(void);                                                //刷新时间/码率显示
void mp3ui_play_ctrl(void);                                                   //播放过程中的处理函数(在解码循环中调用)
void mp3ui_progbar_draw(u32 cur,u32 total);                                   //绘制进度条(cur:当前秒,total:总秒)
void mp3ui_set_play(u8 play);                                                 //设置播放/暂停按钮状态(1播放,0暂停)
void mp3ui_set_cover(u8 *data,u16 w,u16 h);                                   //显示封面图(RGB565数据)
void mp3ui_clear_cover(void);                                                 //清除封面图(显示占位)
void mp3ui_set_action(u8 action);                                             //设置动作标志
u8   mp3ui_get_action(void);                                                  //获取动作标志
#endif
