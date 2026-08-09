#include "sys.h"
#include "delay.h"  
#include "usart.h"   
#include "led.h"
#include "lcd.h"
#include "key.h"  
#include "malloc.h" 
#include "w25qxx.h"    
#include "sdio_sdcard.h"
#include "ff.h"  
#include "exfuns.h"    
#include "wm8978.h"
#include "touch.h"
#include "usbh_usr.h" 
#include "mp3ui.h"
#include "audioplay.h"
//////////////////////////////////////////////////////////////////////////////////	 
//				触摸屏MP3播放器实验
//ALIENTEK 探索者STM32F407开发板
//支持：www.openedv.com
//淘宝：http://eboard.taobao.com
//广州市星翼电子科技有限公司    
//作者：正点原子 @ALIENTEK
//修改时间:2026/8/9
//版本：V1.0
//********************************************************************************
//说明
//本实验参考综合测试实验(实验59)的MP3解码部分,采用裸机(无uCOS)方式实现.
//播放来自U盘("2:")根目录的MP3文件,通过触摸屏控制:
//Prev(上一曲)/Pause-Play(播放暂停)/Next(下一曲)/Vol-(音量-)/Vol+(音量+)
//********************************************************************************
//接线说明
//USB(OTG_FS):PA11/PA12(数据),PA15(电源控制)
//I2S2(音频):PB12/13,PC3/PC6,I2S2ext_SD=PC2
//WM8978(控制):I2C接口PB8/PB9
//触摸屏:GT9147/FT5206电容屏或电阻屏
////////////////////////////////////////////////////////////////////////////////// 	

//USB主机结构体
USBH_HOST  USB_Host;
//USB OTG内核结构体
USB_OTG_CORE_HANDLE  USB_OTG_Core;

//USB用户应用
//检测U盘连接及文件系统状态,显示U盘容量(仅检测一次)
//返回值:0,成功
u8 USH_User_App(void)
{ 
	static u8 checkflag=0;			//检测标志,避免重复检测
	u32 total,free;					//U盘总容量/空闲容量(KB)
	if(checkflag==0)				//尚未检测
	{ 
		if(exf_getfree("2:",&total,&free)==0)//U盘文件系统就绪
		{
			POINT_COLOR=BLUE;
			LCD_ShowString(10,60,lcddev.width-20,16,16,(u8*)"USB:");	
			LCD_ShowNum(10+40,60,total>>10,5,16);	//显示U盘总容量 MB
			LCD_ShowString(10+90,60,60,16,16,(u8*)"MB");		
			checkflag=1;			//检测完成
		}
	} 
	return 0;
}

int main(void)
{        
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置系统中断优先级分组2
	delay_init(168);								//初始化延时函数
	uart_init(115200);								//初始化串口波特率为115200
	LED_Init();										//初始化LED 
	KEY_Init();										//初始化按键
	LCD_Init();										//LCD初始化
	W25QXX_Init();									//初始化W25Q128
	WM8978_Init();									//初始化WM8978
	WM8978_HPvol_Set(40,40);						//设置耳机音量(与UI初始音量一致)
	WM8978_SPKvol_Set(40);							//设置喇叭音量
	my_mem_init(SRAMIN);							//初始化内部内存池
	exfuns_init();									//为fatfs相关变量申请内存
	f_mount(fs[0],"0:",1);							//挂载SD卡
	f_mount(fs[1],"1:",1);							//挂载外部Flash
	TP_Init();										//触摸屏初始化
	mp3ui_init();									//绘制MP3播放器界面
	printf("MP3 Player Start!\r\n");
	//初始化USB主机(MSC类)
	USBH_Init(&USB_OTG_Core,USB_OTG_FS_CORE_ID,&USB_Host,&USBH_MSC_cb,&USR_Callbacks);
	f_mount(fs[2],"2:",1);
	while(1)
	{
		USBH_Process(&USB_OTG_Core, &USB_Host);		//处理USB主机事件
		audio_play();								//MP3播放(内部等待U盘就绪)
	}	
}
