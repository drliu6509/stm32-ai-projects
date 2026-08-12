#ifndef __AUDIOPLAY_H
#define __AUDIOPLAY_H
#include "sys.h"
#include "ff.h"
//////////////////////////////////////////////////////////////////////////////////
//本软件只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32F407探索者开发板
//APP-音乐播放器 应用
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//修改日期:2026/8/9
//版本：V1.0
//版权所有，盗版必究
//Copyright(C) 正点原子@ALIENTEK 2014-2024
//All rights reserved                                                           

//////////////////////////////////////////////////////////////////////////////////
//********************************************************************************
//说明
//本文件为裸机(无uCOS)版本的音频播放控制层,
//参考实验59(综合测试实验)audioplay.h裁剪而来,去除了uCOS/GUI依赖.
//支持从U盘("2:")读取并播放MP3文件.
//////////////////////////////////////////////////////////////////////////////////

//播放结果枚举
typedef enum {
	AP_OK=0X00,								//播放正常完成
	AP_NEXT,								//要求切换歌曲
	AP_ERR=0X80,					//播放出错(没有找到歌曲,或者解码错误等) 
}APRESULT;

//音乐播放控制结构体
typedef __packed struct
{
	//2个I2S音频BUF
	u8 *i2sbuf1;
	u8 *i2sbuf2;
	u8 *tbuf;					//临时buf
	FIL *file;					//音频文件指针
	u32(*file_seek)(u32);//文件快速定位函数

	vu8 status;					//bit0:0,暂停状态;1,播放状态
											//bit1:0,停止播放;1,正在播放	
											//bit2~3:音量
											//bit6:0,无动作;1,请求停止播放(退出播放循环)

	u8 mode;					//播放模式
											//0,顺序播放;1,随机播放;2,单曲循环

	u8 *path;					//当前文件所在路径
	u8 *name;					//当前播放的MP3文件名字
	u16 namelen;		//name所占的内存

	u32 totsec ;			//歌曲总时间,单位:秒
	u32 cursec ;			//当前播放时间
	u32 seeksec;			//进度跳转目标时间(秒),0XFFFFFFFF表示无跳转请求
	u32 bitrate;			//比特率(位/秒)
	u32 samplerate;		//采样率
	u16 bps;				//位数,比如16bit,24bit,32bit

	u16 curindex;			//当前播放的音频文件索引
	u16 mfilenum;			//音频文件总数目
	u16 *mfindextbl;	//音频文件索引表

}__audiodev;
extern __audiodev audiodev;	//音乐播放控制结构体

//取2个数值中的较小值.
#ifndef AUDIO_MIN
#define AUDIO_MIN(x,y)	((x)<(y)? (x):(y))
#endif

void audio_start(void);
void audio_stop(void);
u16 audio_get_tnum(u8 *path);
void audio_play(void);
#endif
