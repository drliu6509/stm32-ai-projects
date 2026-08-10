#include "mp3ui.h"
#include "lcd.h"
#include "touch.h"
#include "delay.h"
#include "string.h"
#include "wm8978.h"
#include "audioplay.h"
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

//界面颜色定义
#define MP3UI_BK_COLOR     WHITE        //背景色
#define MP3UI_TXT_COLOR    BLACK        //文字颜色
#define MP3UI_BTN_COLOR    BLUE         //按钮背景色
#define MP3UI_BTN_TXTCOLOR WHITE        //按钮文字颜色
#define MP3UI_BTN_PRSCOLOR 0X2CFF       //按钮按下高亮色

//界面文字显示区域
#define MP3UI_TITLE_Y     10            //标题Y坐标
#define MP3UI_NAME_Y      40            //歌曲名Y坐标
#define MP3UI_TIME_Y      75            //时间Y坐标
#define MP3UI_INDEX_Y     105           //歌曲索引Y坐标

//进度条参数
#define MP3UI_PBAR_X     10             //进度条X坐标
#define MP3UI_PBAR_Y     150            //进度条Y坐标
#define MP3UI_PBAR_H     6              //进度条高度
#define MP3UI_PBAR_W     (lcddev.width-20)   //进度条宽度

//按钮参数
#define MP3UI_BTN_H       40            //按钮高度
#define MP3UI_BTN_GAP     8             //按钮间距
#define MP3UI_BTN_Y_OFF   10            //按钮距底部间距

//按钮编号
#define MP3UI_BTN_PREV    0             //上一曲
#define MP3UI_BTN_PLAY    1             //播放/暂停
#define MP3UI_BTN_NEXT    2             //下一曲
#define MP3UI_BTN_VOLDN   3             //音量-
#define MP3UI_BTN_VOLUP   4             //音量+
#define MP3UI_BTN_NUM     5             //按钮个数

//按钮结构体
typedef struct
{
	u16 x;                  //X坐标
	u16 y;                  //Y坐标
	u16 w;                  //宽度
	u16 h;                  //高度
	u8  *label;             //按钮文字
}_mp3ui_btn;

static _mp3ui_btn mp3ui_btns[MP3UI_BTN_NUM];    //5个按钮
static u8 mp3ui_action=MP3UI_ACTION_NONE;		//当前触摸动作
static u8 mp3ui_vol=40;							//当前音量(0~63)
static u8 mp3ui_playing=1;						//当前播放状态(1播放,0暂停)
static u8 mp3ui_presslast=0;					//上一次按下状态,用于检测按下沿
static u8 mp3ui_dragstate=0;					//是否正在拖动进度条(1拖动,0未拖动)
static u32 mp3ui_dragsec=0;						//拖动位置对应的秒数

//绘制一个按钮
//btn:按钮结构体指针
//bkcolor:按钮背景色(正常色或按下高亮色)
static void mp3ui_btn_draw(_mp3ui_btn *btn,u16 bkcolor)
{
	u16 tx;
	u8 *p;
	LCD_Fill(btn->x,btn->y,btn->x+btn->w-1,btn->y+btn->h-1,bkcolor);//填充按钮背景
	POINT_COLOR=MP3UI_BTN_TXTCOLOR;
	//逐字符透明显示按钮文字(mode=1),避免mode=0用背景色(白色)填充出矩形块
	p=btn->label;
	tx=btn->x+(btn->w-strlen((char*)btn->label)*8)/2;				//文字居中X坐标
	while(*p)
	{
		LCD_ShowChar(tx,btn->y,*p,16,1);							//mode=1,透明背景
		tx+=8;
		p++;
	}
}
//显示音量
static void mp3ui_vol_show(void)
{
	POINT_COLOR=MP3UI_TXT_COLOR;
	LCD_ShowString(lcddev.width-100,MP3UI_TITLE_Y,60,16,16,(u8*)"VOL:");	
	LCD_ShowxNum(lcddev.width-60,MP3UI_TITLE_Y,mp3ui_vol,2,16,0X80);		//显示音量
}
//设置按钮文字
//btn:按钮编号
//label:新的文字
static void mp3ui_btn_setlabel(u8 btn,u8 *label)
{
	mp3ui_btns[btn].label=label;
	mp3ui_btn_draw(&mp3ui_btns[btn],MP3UI_BTN_COLOR);//重绘按钮
}
//判断触摸点是否落在按钮内
//返回按钮编号,不在任何按钮内则返回0XFF
static u8 mp3ui_btn_check(u16 x,u16 y)
{
	u8 i;
	for(i=0;i<MP3UI_BTN_NUM;i++)
	{
		if(x>=mp3ui_btns[i].x&&x<=(mp3ui_btns[i].x+mp3ui_btns[i].w) 	
			&&y>=mp3ui_btns[i].y&&y<=(mp3ui_btns[i].y+mp3ui_btns[i].h))
		{
			return i;//在按钮i内
		}
	}
	return 0XFF;//不在任何按钮内
}
//绘制进度条
//cur:当前播放时间(秒)
//total:歌曲总时间(秒)
void mp3ui_progbar_draw(u32 cur,u32 total)
{
	static u32 lastsec=0XFFFFFFFF;
	u32 prog;
	if(total==0)//总时间为0,重置缓存,等待播放信息就绪
	{
		lastsec=0XFFFFFFFF;
		LCD_Fill(MP3UI_PBAR_X,MP3UI_PBAR_Y-2,MP3UI_PBAR_X+MP3UI_PBAR_W-1,MP3UI_PBAR_Y+MP3UI_PBAR_H+1,0XDFFF);
		return;
	}
	if(cur==lastsec)return;//无变化,不重绘
	if(cur>total)cur=total;
	lastsec=cur;
	prog=(u32)((unsigned long long)cur*MP3UI_PBAR_W/total);			//计算进度像素
	//绘制进度条背景
	LCD_Fill(MP3UI_PBAR_X,MP3UI_PBAR_Y-2,MP3UI_PBAR_X+MP3UI_PBAR_W-1,MP3UI_PBAR_Y+MP3UI_PBAR_H+1,0XDFFF);
	//绘制已播放部分
	if(prog>0)LCD_Fill(MP3UI_PBAR_X,MP3UI_PBAR_Y,MP3UI_PBAR_X+prog-1,MP3UI_PBAR_Y+MP3UI_PBAR_H-1,BLUE);
	//绘制拖动点
	if(prog>2)LCD_Fill(MP3UI_PBAR_X+prog-3,MP3UI_PBAR_Y-2,MP3UI_PBAR_X+prog+1,MP3UI_PBAR_Y+MP3UI_PBAR_H+1,RED);
}
//按下高亮显示按钮
//btn:按钮编号
static void mp3ui_btn_highlight(u8 btn)
{
	mp3ui_btn_draw(&mp3ui_btns[btn],MP3UI_BTN_PRSCOLOR);//以高亮色重绘按钮
}
//重绘所有按钮(恢复默认颜色)
static void mp3ui_btns_redraw(void)
{
	u8 i;
	for(i=0;i<MP3UI_BTN_NUM;i++)
	{
		mp3ui_btn_draw(&mp3ui_btns[i],MP3UI_BTN_COLOR);//重绘按钮
	}
}
//按钮动作处理
//btn:按钮编号
static void mp3ui_btn_action(u8 btn)
{
	switch(btn)
	{
		case MP3UI_BTN_PREV://上一曲
			mp3ui_action=MP3UI_ACTION_PREV;			//设置动作      
			audiodev.status&=~(1<<1);				//退出当前播放循环,切换歌曲
			break;
		case MP3UI_BTN_PLAY://播放/暂停
			if(mp3ui_playing)//当前正在播放,执行暂停
			{
				audiodev.status&=~(1<<0);			//暂停状态
				mp3ui_set_play(0);
			}else//当前暂停,恢复播放
			{
				audiodev.status|=1<<0;				//播放状态
				mp3ui_set_play(1);
			}
			break;
		case MP3UI_BTN_NEXT://下一曲
			mp3ui_action=MP3UI_ACTION_NEXT;			//设置动作      
			audiodev.status&=~(1<<1);				//退出当前播放循环,切换歌曲
			break;
		case MP3UI_BTN_VOLDN://音量-
			if(mp3ui_vol>0)mp3ui_vol--;				//音量递减
			WM8978_HPvol_Set(mp3ui_vol,mp3ui_vol);
			WM8978_SPKvol_Set(mp3ui_vol);
			mp3ui_vol_show();						//刷新音量显示
			break;
		case MP3UI_BTN_VOLUP://音量+
			if(mp3ui_vol<63)mp3ui_vol++;			//音量递增      
			WM8978_HPvol_Set(mp3ui_vol,mp3ui_vol);
			WM8978_SPKvol_Set(mp3ui_vol);
			mp3ui_vol_show();						//刷新音量显示
			break;
	}
}
//初始化UI,绘制主界面
void mp3ui_init(void)
{
	u16 i;
	u16 btn_w;
	u16 btn_x;
	u16 btn_y;

	LCD_Clear(MP3UI_BK_COLOR);											//清屏
	//显示标题
	POINT_COLOR=RED;
	LCD_ShowString(10,MP3UI_TITLE_Y,lcddev.width-20,16,16,(u8*)"MP3 Player");
	mp3ui_vol_show();													//显示音量
	//显示默认时间
	mp3ui_time_refresh();
	//绘制进度条背景
	LCD_Fill(MP3UI_PBAR_X,MP3UI_PBAR_Y-2,MP3UI_PBAR_X+MP3UI_PBAR_W-1,MP3UI_PBAR_Y+MP3UI_PBAR_H+1,0XDFFF);
	//计算按钮宽度和位置
	btn_w=(lcddev.width-6*MP3UI_BTN_GAP)/MP3UI_BTN_NUM;		//按钮宽度      
	btn_y=lcddev.height-MP3UI_BTN_H-MP3UI_BTN_Y_OFF;		//按钮Y坐标     
	//初始化5个按钮
	mp3ui_btns[MP3UI_BTN_PREV].label=(u8*)"Prev";			//上一曲        
	mp3ui_btns[MP3UI_BTN_PLAY].label=(u8*)"Pause";			//播放/暂停     
	mp3ui_btns[MP3UI_BTN_NEXT].label=(u8*)"Next";			//下一曲        
	mp3ui_btns[MP3UI_BTN_VOLDN].label=(u8*)"Vol-";			//音量-
	mp3ui_btns[MP3UI_BTN_VOLUP].label=(u8*)"Vol+";			//音量+
	for(i=0;i<MP3UI_BTN_NUM;i++)
	{
		btn_x=MP3UI_BTN_GAP+i*(btn_w+MP3UI_BTN_GAP);
		mp3ui_btns[i].x=btn_x;
		mp3ui_btns[i].y=btn_y;
		mp3ui_btns[i].w=btn_w;
		mp3ui_btns[i].h=MP3UI_BTN_H;
		mp3ui_btn_draw(&mp3ui_btns[i],MP3UI_BTN_COLOR);		//绘制按钮
	}
	mp3ui_presslast=0;													//清除按下状态
	mp3ui_dragstate=0;													//清除拖动状态
}
//显示歌曲名和索引
//name:歌曲文件名
//cur:当前歌曲序号(从1开始)
//total:歌曲总数
void mp3ui_show_song(u8 *name,u16 cur,u16 total)
{
	LCD_Fill(10,MP3UI_NAME_Y,lcddev.width-10,MP3UI_NAME_Y+15,MP3UI_BK_COLOR);//清除旧显示
	POINT_COLOR=MP3UI_TXT_COLOR;
	LCD_ShowString(10,MP3UI_NAME_Y,lcddev.width-20,16,16,name);		//显示歌曲名
	//显示索引
	LCD_Fill(10,MP3UI_INDEX_Y,lcddev.width-10,MP3UI_INDEX_Y+15,MP3UI_BK_COLOR);
	LCD_ShowString(10,MP3UI_INDEX_Y,100,16,16,(u8*)"Song:");
	LCD_ShowxNum(10+45,MP3UI_INDEX_Y,cur,3,16,0X80);		//当前歌曲序号  
	LCD_ShowChar(10+45+24,MP3UI_INDEX_Y,'/',16,0);
	LCD_ShowxNum(10+45+32,MP3UI_INDEX_Y,total,3,16,0X80);	//歌曲总数      
	//重置进度条显示
	mp3ui_progbar_draw(0,0);
}
//显示提示信息
//msg:提示信息字符串
void mp3ui_show_msg(u8 *msg)
{
	static u8 lastmsg[30];                                              //保存上一次提示内容
	if(strcmp((char*)lastmsg,(char*)msg)==0)return;             //提示内容相同,不重复绘制(避免高速闪烁)
	strcpy((char*)lastmsg,(char*)msg);                          //记录本次提示
	POINT_COLOR=RED;
	LCD_Fill(10,MP3UI_NAME_Y,lcddev.width-10,MP3UI_NAME_Y+15,MP3UI_BK_COLOR);
	LCD_ShowString(10,MP3UI_NAME_Y,lcddev.width-20,16,16,msg);
}
//刷新时间/码率显示
void mp3ui_time_refresh(void)
{
	static u32 last_cur=0XFFFFFFFF;
	static u32 last_tot=0XFFFFFFFF;
	static u32 last_bit=0XFFFFFFFF;
	if(audiodev.cursec!=last_cur||audiodev.totsec!=last_tot||audiodev.bitrate!=last_bit)
	{
		last_cur=audiodev.cursec;
		last_tot=audiodev.totsec;
		last_bit=audiodev.bitrate;
		POINT_COLOR=MP3UI_TXT_COLOR;
		//显示当前时间 mm:ss
		LCD_ShowxNum(10,MP3UI_TIME_Y,audiodev.cursec/60,2,16,0X80);	//分
		LCD_ShowChar(10+16,MP3UI_TIME_Y,':',16,0);
		LCD_ShowxNum(10+24,MP3UI_TIME_Y,audiodev.cursec%60,2,16,0X80);	//秒
		LCD_ShowChar(10+48,MP3UI_TIME_Y,'/',16,0);
		//显示总时间 mm:ss
		LCD_ShowxNum(10+56,MP3UI_TIME_Y,audiodev.totsec/60,2,16,0X80);  
		LCD_ShowChar(10+72,MP3UI_TIME_Y,':',16,0);
		LCD_ShowxNum(10+80,MP3UI_TIME_Y,audiodev.totsec%60,2,16,0X80);  
		//显示码率
		LCD_ShowxNum(10+130,MP3UI_TIME_Y,audiodev.bitrate/1000,4,16,0X80);
		LCD_ShowString(10+130+32,MP3UI_TIME_Y,60,16,16,(u8*)"Kbps");	
	}
}
//设置播放/暂停按钮状态
//play:1,播放状态;0,暂停状态
void mp3ui_set_play(u8 play)
{
	mp3ui_playing=play;
	if(play)
	{
		mp3ui_btn_setlabel(MP3UI_BTN_PLAY,(u8*)"Pause");		//播放中,按钮显示暂停
	}else
	{
		mp3ui_btn_setlabel(MP3UI_BTN_PLAY,(u8*)"Play");			//暂停中,按钮显示播放
	}
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
//播放过程中的触摸控制
//由mp3_play_song的播放循环中调用,实现按钮响应,进度条拖动和时间刷新
void mp3ui_play_ctrl(void)
{
	u8 press;
	u8 btn;
	u16 x;
	u16 y;
	u16 y1;
	u16 y2;
	u32 sec;

	tp_dev.scan(0);														//扫描触摸屏
	press=(tp_dev.sta&TP_PRES_DOWN)?1:0;	//是否按下
	x=tp_dev.x[0];
	y=tp_dev.y[0];
	y1=MP3UI_PBAR_Y-15;												//进度条触摸区域上边界
	y2=MP3UI_PBAR_Y+MP3UI_PBAR_H+15;							//进度条触摸区域下边界
	if(press&&!mp3ui_presslast)//按下沿(只有第一次按下时响应)
	{
		if(y>=y1&&y<=y2)//按在进度条区域,进入拖动
		{
			mp3ui_dragstate=1;									//进入拖动状态
			if(x<MP3UI_PBAR_X)x=MP3UI_PBAR_X;
			if(x>MP3UI_PBAR_X+MP3UI_PBAR_W-1)x=MP3UI_PBAR_X+MP3UI_PBAR_W-1;
			sec=(u32)((unsigned long long)(x-MP3UI_PBAR_X)*audiodev.totsec/MP3UI_PBAR_W);//计算目标时间
			mp3ui_dragsec=sec;
			mp3ui_progbar_draw(sec,audiodev.totsec);			//更新进度条显示
		}else//按在按钮区域
		{
			btn=mp3ui_btn_check(x,y);
			if(btn!=0XFF)
			{
				mp3ui_btn_highlight(btn);							//按下高亮指示
				mp3ui_btn_action(btn);								//执行按钮动作
			}
		}
	}else if(press&&mp3ui_presslast)//按住拖动中
	{
		if(mp3ui_dragstate)//正在拖动进度条
		{
			if(y>=y1&&y<=y2)
			{
				if(x<MP3UI_PBAR_X)x=MP3UI_PBAR_X;
				if(x>MP3UI_PBAR_X+MP3UI_PBAR_W-1)x=MP3UI_PBAR_X+MP3UI_PBAR_W-1;
				sec=(u32)((unsigned long long)(x-MP3UI_PBAR_X)*audiodev.totsec/MP3UI_PBAR_W);//计算目标时间
				if(sec!=mp3ui_dragsec)//位置变化才更新
				{
					mp3ui_dragsec=sec;
					mp3ui_progbar_draw(sec,audiodev.totsec);	//拖动时实时更新显示
				}
			}
		}
	}else if(!press&&mp3ui_presslast)//松开沿
	{
		if(mp3ui_dragstate)//结束拖动,执行跳转
		{
			audiodev.seeksec=mp3ui_dragsec;					//设置跳转目标时间
			audiodev.cursec=mp3ui_dragsec;					//同步显示
			mp3ui_dragstate=0;								//清除拖动状态
		}
		mp3ui_btns_redraw();										//恢复所有按钮颜色
	}
	mp3ui_presslast=press;									//记录本次按下状态
	//绘制进度条(未拖动时显示实际播放位置)
	if(!mp3ui_dragstate)mp3ui_progbar_draw(audiodev.cursec,audiodev.totsec);
	mp3ui_time_refresh();										//刷新时间/码率显示
}
