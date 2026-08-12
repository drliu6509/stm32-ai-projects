#include "audioplay.h"
#include "ff.h"
#include "malloc.h"
#include "usart.h"
#include "wm8978.h"
#include "i2s.h"
#include "delay.h"
#include "exfuns.h"
#include "string.h"
#include "mp3play.h"
#include "wavplay.h"
#include "mp3ui.h"
#include "music_player.h"
#include "usbh_usr.h"
//////////////////////////////////////////////////////////////////////////////////
//	本文件仅供学习使用，未经允许，不得用于任何用途
//ALIENTEK STM32F407探索者开发板
//APP-音乐播放器 应用代码
//版权: 正点原子@ALIENTEK
//论坛: www.openedv.com
//修改日期:2026/8/11
//版本:V1.0
//版权所有，盗版必究
//Copyright(C) 正点原子@ALIENTEK 2014-2024
//All rights reserved
//////////////////////////////////////////////////////////////////////////////////
//说明
//本文件为音乐播放调度层(LVGL版):
//1, 根据music_files曲目表从audiodev.curindex开始播放, 支持MP3/WAV按类型分发
//2, MP3播放前解析ID3内嵌封面(APIC)并显示, WAV不显示封面
//3, 上一曲/下一曲/返回列表由mp3ui动作标志驱动; 返回列表时music_exit_play=1退出播放
//4, 播放期间解码循环通过mp3ui_play_ctrl()每10ms服务一次LVGL, 优先保证解码流畅
//////////////////////////////////////////////////////////////////////////////////

__audiodev audiodev;

//初始化音频播放
void audio_start(void)
{
	audiodev.status|=1<<1;		//正在播放
	audiodev.status|=1<<0;		//播放状态
	I2S_Play_Start();
}
//停止音频播放
void audio_stop(void)
{
	audiodev.status&=~(1<<0);	//清除播放状态
	audiodev.status&=~(1<<1);	//清除正在播放
	I2S_Play_Stop();
}

//得到path路径下, 目录下的有效文件总数(MP3/WAV)
//path:路径
//返回值:有效文件个数
u16 audio_get_tnum(u8 *path)
{
	u8 res;
	u16 rval=0;
 	DIR tdir;	 		//临时目录
	FILINFO tfileinfo;	//临时文件信息
	u8 *fn;
    res=f_opendir(&tdir,(const TCHAR*)path); //打开目录
  	tfileinfo.lfsize=_MAX_LFN*2+1;						//文件名字符串最大长度
	tfileinfo.lfname=mymalloc(SRAMEX,tfileinfo.lfsize);	//为文件名字符串分配内存
	if(res==FR_OK&&tfileinfo.lfname!=NULL)
	{
		while(1)//查询总的音乐文件数
		{
	        res=f_readdir(&tdir,&tfileinfo);       		//读取目录下的一个文件
	        if(res!=FR_OK||tfileinfo.fname[0]==0)break;	//错误/到达末尾,退出
     		fn=(u8*)(*tfileinfo.lfname?tfileinfo.lfname:tfileinfo.fname);
			res=f_typetell(fn);
			if((res&0XF0)==0X40)//取出高4位,说明是音频文件
			{
				rval++;//有效文件数加1
			}
		}
	}
	myfree(SRAMEX,tfileinfo.lfname);
	return rval;
}

//音乐播放(阻塞播放)
//根据music_files曲目表从audiodev.curindex开始播放, 支持MP3/WAV分发
//返回列表时退出(music_exit_play=1)
void audio_play(void)
{
	u16 curindex;			//当前曲目索引
	u16 err_cnt;			//连续失败计数(U盘异常时避免无限循环)
	u8 res;					//解码返回码
	u8 action;				//用户动作
	u8 *pname;				//完整路径缓冲

	WM8978_ADDA_Cfg(1,0);	//开启DAC
	WM8978_Input_Cfg(0,0,0);//关闭输入通道
	WM8978_Output_Cfg(1,0);	//开启DAC输出

	pname=mymalloc(SRAMEX,MUSIC_NAME_LEN+8);	//分配完整路径缓冲
	if(pname==NULL)return;						//分配失败
	audiodev.status=0;							//清空播放状态
	audiodev.seeksec=0XFFFFFFFF;				//无跳转请求
	curindex=audiodev.curindex;					//起始播放索引(列表点击时设置)
	err_cnt=0;									//清空失败计数
	if(curindex>=music_file_num)curindex=0;
	while(1)
	{
		if(music_exit_play)break;				//返回列表请求
		if(!USBH_UDISK_Status())break;			//U盘被拔出,停止播放
		if(music_file_num==0)break;				//无曲目
		//组装完整路径"2:/xxx.mp3"(用GBK原名, FATFS按CP936打开)
		strcpy((char*)pname,"2:/");
		strcat((char*)pname,(const char*)music_files[curindex].gbkname);
		audiodev.curindex=curindex;				//记录当前曲目
		audiodev.mfilenum=music_file_num;		//记录曲目总数
		mp3ui_show_song(music_files[curindex].name,curindex+1,music_file_num);//显示歌曲信息
		mp3ui_set_play(1);						//显示播放状态
		mp3ui_set_action(MP3UI_ACTION_NONE);	//清除动作标志
		audiodev.totsec=0;						//清零总时间
		audiodev.cursec=0;						//清零当前时间
		audiodev.bitrate=0;						//清零码率
		audiodev.seeksec=0XFFFFFFFF;			//无跳转请求
		mp3ui_time_refresh();					//刷新时间显示
		//MP3文件解析并显示内嵌封面(ID3 APIC), WAV无封面
		if(music_files[curindex].type==T_MP3)
		{
			if(!music_player_show_cover(pname))mp3ui_clear_cover();
		}
		else
		{
			mp3ui_clear_cover();
		}
		//按文件类型分发解码
		if(music_files[curindex].type==T_MP3)res=mp3_play_song(pname);
		else res=wav_play_song(pname);
		if(res&0X80)//解码/打开失败, 自动跳转到下一首
		{
			printf("play error:%d\r\n",res);
			curindex++;
			if(curindex>=music_file_num)curindex=0;
			err_cnt++;
			if(err_cnt>=music_file_num)break;	//整轮全部失败, 判定U盘异常, 停止播放
		}
		else//正常结束或用户操作
		{
			err_cnt=0;							//成功播放, 清除失败计数
			action=mp3ui_get_action();			//获取动作标志
			if(action==MP3UI_ACTION_PREV)		//上一曲
			{
				if(curindex)curindex--;
				else curindex=music_file_num-1;
			}
			else if(action==MP3UI_ACTION_NEXT)	//下一曲
			{
				curindex++;
				if(curindex>=music_file_num)curindex=0;
			}
			else								//自然播放结束, 顺序播放下一首
			{
				curindex++;
				if(curindex>=music_file_num)curindex=0;
			}
		}
	}
	myfree(SRAMEX,pname);						//释放内存
	audio_stop();								//停止音频输出
}
