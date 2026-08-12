#ifndef __MUSIC_PLAYER_H
#define __MUSIC_PLAYER_H
#include "sys.h"
#include "lvgl.h"
//////////////////////////////////////////////////////////////////////////////////
//	本文件仅供学习使用，未经允许，不得用于任何用途
//ALIENTEK STM32F407探索者开发板
//APP-基于LVGL的音乐播放器 文件列表/播放调度
//版权: 正点原子@ALIENTEK
//论坛: www.openedv.com
//修改日期:2026/8/11
//版本:V1.0
//版权所有，盗版必究
//Copyright(C) 正点原子@ALIENTEK 2014-2024
//All rights reserved
//////////////////////////////////////////////////////////////////////////////////
//说明
//本文件负责:
//1, 创建音乐列表界面(扫描U盘根目录的MP3/WAV文件)
//2, 播放调度: 点击列表项->切换播放界面->调用audio_play()阻塞播放
//3, 封面显示: 解析MP3内嵌ID3 APIC封面(JPEG), 用TJpgDec解码为RGB565显示
//4, 返回列表: 播放界面"返回"按钮退出解码循环, 回到列表
//为了优先保证音乐解码, 界面不使用动画, 播放期间LVGL由mp3ui_play_ctrl每10ms服务一次.
//////////////////////////////////////////////////////////////////////////////////

//曲目数量上限
#define MUSIC_MAX_FILES   200
//文件名最大长度(含扩展名)
#define MUSIC_NAME_LEN    160

//曲目信息结构体
typedef struct
{
    u8  name[MUSIC_NAME_LEN];   //显示文件名(UTF-8,供LVGL显示)
    u8  gbkname[MUSIC_NAME_LEN];//原始文件名(GBK,f_open打开用)
    u8  type;                   //文件类型(T_MP3/T_WAV,见exfuns.h)
} music_file_t;

//曲目表(SRAMEX动态分配)
extern music_file_t *music_files;
//曲目总数
extern u16 music_file_num;
//请求退出播放标志(播放界面返回按钮置1,audio_play退出循环)
extern u8  music_exit_play;

void music_player_init(void);            //初始化音乐播放器(创建列表/播放界面)
void music_player_loop(void);            //播放调度状态机(主循环调用)
void music_player_scan(void);            //扫描U盘根目录音乐文件,刷新列表
void music_player_list_refresh(void);    //刷新列表界面
void music_player_play_index(u16 index); //播放指定索引曲目(切换到播放界面)
void music_player_show_list(void);       //加载并显示列表界面
u8   music_player_show_cover(u8 *fname); //显示MP3内嵌封面(1成功,0无封面)
#endif
