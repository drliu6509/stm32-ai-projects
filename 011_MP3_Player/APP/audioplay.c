#include "audioplay.h"
#include "ff.h"
#include "malloc.h"
#include "usart.h"
#include "wm8978.h"
#include "i2s.h"
#include "led.h"
#include "lcd.h"
#include "delay.h"
#include "key.h"
#include "exfuns.h"
#include "string.h"
#include "mp3play.h"
#include "mp3ui.h"
#include "usbh_usr.h"
//////////////////////////////////////////////////////////////////////////////////	 
//本软件只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32F407探索者开发板
//APP-音乐播放器 应用代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//修改日期:2026/8/9
//版本：V1.0
//版权所有，盗版必究
//Copyright(C) 正点原子@ALIENTEK 2014-2024
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 	 
//说明
//本文件为裸机(无uCOS)版本的音频播放控制层,
//参考实验59(综合测试实验)audioplay.c裁剪而来,去除了uCOS/GUI依赖.
//播放流程:扫描U盘("2:")根目录的MP3文件→建立索引表→顺序播放
//播放过程中由mp3ui_play_ctrl()(在mp3play.c中调用)处理触摸控制.
////////////////////////////////////////////////////////////////////////////////// 	 

extern USBH_HOST USB_Host;			//USB主机结构体(定义在main.c)
extern USB_OTG_CORE_HANDLE USB_OTG_Core;	//USB OTG内核结构体(定义在main.c)
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

//得到path路径下,目标文件的总个数
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
  	tfileinfo.lfsize=_MAX_LFN*2+1;						//文件名字最大长度
	tfileinfo.lfname=mymalloc(SRAMIN,tfileinfo.lfsize);	//为文件名字分配内存
	if(res==FR_OK&&tfileinfo.lfname!=NULL)
	{
		while(1)//查询总的有效文件数
		{
	        res=f_readdir(&tdir,&tfileinfo);       		//读取目录下的一个文件
	        if(res!=FR_OK||tfileinfo.fname[0]==0)break;	//错误/到末尾了,退出		  
     		fn=(u8*)(*tfileinfo.lfname?tfileinfo.lfname:tfileinfo.fname);			 
			res=f_typetell(fn);	
			if((res&0XF0)==0X40)//取高四位,说明是音乐类文件	
			{
				rval++;//有效文件数加1
			}	    
		}  
	} 
	myfree(SRAMIN,tfileinfo.lfname);
	return rval;
}

//音乐播放(触摸屏控制)
//扫描U盘根目录下的MP3文件并顺序播放
void audio_play(void)
{
	u8 res;
 	DIR audiodir;	 		//目录
	FILINFO audiofileinfo;	//文件信息
	u8 *fn;   				//保存文件名
	u8 *pname;				//带路径的文件名
	u16 totmp3num; 			//音乐文件总数
	u16 curindex;			//当前播放索引
	u8 action;				//触摸动作
 	u16 temp;
	u16 *mp3indextbl;		//音乐文件索引表
	
	WM8978_ADDA_Cfg(1,0);	//开启DAC
	WM8978_Input_Cfg(0,0,0);//关闭输入通道
	WM8978_Output_Cfg(1,0);	//开启DAC输出   
	
 	while(f_opendir(&audiodir,"2:/"))//查找U盘根目录
 	{	    
		mp3ui_show_msg((u8*)"No USB Disk!");	//提示
		USBH_Process(&USB_OTG_Core,&USB_Host);				//处理USB主机事件
		delay_ms(1);				  
	} 									  
	totmp3num=audio_get_tnum("2:/"); //得到有效文件数
  	while(totmp3num==NULL)//文件总数为0		
 	{	    
		mp3ui_show_msg((u8*)"No MP3 Files!");	//提示
		USBH_Process(&USB_OTG_Core,&USB_Host);				//处理USB主机事件
		delay_ms(1);				  
	}										   
  	audiofileinfo.lfsize=_MAX_LFN*2+1;						//文件名字最大长度
	audiofileinfo.lfname=mymalloc(SRAMIN,audiofileinfo.lfsize);	//为文件名字分配内存
 	pname=mymalloc(SRAMIN,audiofileinfo.lfsize+8);			//为带路径的文件名分配内存
 	mp3indextbl=mymalloc(SRAMIN,2*totmp3num);				//分配2*totmp3num个字节的内存,用于存储文件索引
 	while(audiofileinfo.lfname==NULL||pname==NULL||mp3indextbl==NULL)//内存分配失败
 	{	    
		mp3ui_show_msg((u8*)"Memory Error!");	//提示
		USBH_Process(&USB_OTG_Core,&USB_Host);				//处理USB主机事件
		delay_ms(1);				  
	}  	 
 	//记录索引
    res=f_opendir(&audiodir,"2:/"); //打开目录
	if(res==FR_OK)
	{
		curindex=0;//当前索引为0
		while(1)//全部查询一遍
		{
			temp=audiodir.index;								//记录当前index
	        res=f_readdir(&audiodir,&audiofileinfo);       		//读取目录下的一个文件
	        if(res!=FR_OK||audiofileinfo.fname[0]==0)break;	//错误/到末尾了,退出		  
     		fn=(u8*)(*audiofileinfo.lfname?audiofileinfo.lfname:audiofileinfo.fname);			 
			res=f_typetell(fn);	
			if((res&0XF0)==0X40)//取高四位,说明是音乐类文件	
			{
				mp3indextbl[curindex]=temp;//记录索引
				curindex++;
			}	    
		} 
	}   
   	curindex=0;											//从0开始播放
   	res=f_opendir(&audiodir,(const TCHAR*)"2:/"); 		//打开目录
	while(1)//打开成功
	{	
		dir_sdi(&audiodir,mp3indextbl[curindex]);			//改变当前目录索引	   
        res=f_readdir(&audiodir,&audiofileinfo);       		//读取目录下的一个文件
        if(res!=FR_OK||audiofileinfo.fname[0]==0)break;	//错误/到末尾了,退出
     	fn=(u8*)(*audiofileinfo.lfname?audiofileinfo.lfname:audiofileinfo.fname);			 
		strcpy((char*)pname,"2:/");						//复制路径(目录)
		strcat((char*)pname,(const char*)fn);  			//将文件名接在路径后面
		audiodev.curindex=curindex;						//记录当前索引
		audiodev.mfilenum=totmp3num;					//记录文件总数
 		mp3ui_show_song(fn,curindex+1,totmp3num);		//显示歌曲信息 
		mp3ui_set_play(1);								//显示播放状态
		mp3ui_set_action(MP3UI_ACTION_NONE);			//清除触摸动作
		audiodev.totsec=0;								//清除总时间
		audiodev.cursec=0;								//清除当前时间
		audiodev.bitrate=0;								//清除码率
 		mp3ui_time_refresh();							//刷新时间显示
		res=mp3_play_song(pname); 			 			//播放MP3文件
		if(res&0X80)//播放出错,自动跳转到下一首
		{
			printf("play error:%d\r\n",res);
			curindex++;		   	
			if(curindex>=totmp3num)curindex=0;//到末尾了,自动从头开始
		}else//正常返回(播放完毕或用户切歌)
		{
			action=mp3ui_get_action();					//获取触摸动作
			if(action==MP3UI_ACTION_PREV)				//上一曲
			{
				if(curindex)curindex--;
				else curindex=totmp3num-1;
	 		}else if(action==MP3UI_ACTION_NEXT)			//下一曲
			{
				curindex++;		   	
				if(curindex>=totmp3num)curindex=0;
	 		}else										//播放完毕,顺序播放下一首
			{
				curindex++;		   	
				if(curindex>=totmp3num)curindex=0;
	 		}
		}
	} 											  
	myfree(SRAMIN,audiofileinfo.lfname);	//释放内存			    
	myfree(SRAMIN,pname);					//释放内存			    
	myfree(SRAMIN,mp3indextbl);				//释放内存	 
	audio_stop();							//停止音频输出 
}
