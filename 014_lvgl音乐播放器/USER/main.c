#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "lcd.h"
#include "sram.h"
#include "touch.h"
#include "timer.h"
#include "malloc.h"
#include "exfuns.h"
#include "wm8978.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "usbh_usr.h"
#include "music_player.h"
#include "font_lib.h"
//////////////////////////////////////////////////////////////////////////////////
//ALIENTEK探索者STM32F407开发板  基于LVGL的音乐播放器(014)
//版权: 正点原子@ALIENTEK
//论坛: www.openedv.com
//修改日期:2026/8/11
//版本:V1.0
//版权所有，盗版必究
//Copyright(C) 正点原子@ALIENTEK 2014-2024
//All rights reserved
//////////////////////////////////////////////////////////////////////////////////
//说明
//1, 扫描U盘("2:")根目录下的MP3/WAV文件, 显示在LVGL列表界面
//2, 点击列表项进入播放界面, 支持MP3(Helix软解)/WAV(PCM直通)播放, WM8978 DAC输出
//3, MP3文件解析ID3内嵌封面(APIC JPEG), 经TJpgDec解码后显示
//4, 为优先保证音乐解码, 界面不使用动画, 播放期间解码循环每10ms服务一次LVGL
//5, TIM3定时器(1ms)驱动LVGL tick时钟; DMA1_Stream4驱动I2S双缓冲; DMA2_Stream7驱动刷屏
//////////////////////////////////////////////////////////////////////////////////

//USB主机结构体(USB驱动使用)
USBH_HOST  USB_Host;
//USB OTG内核结构体
USB_OTG_CORE_HANDLE  USB_OTG_Core;

//USB用户应用(枚举完成后由USBH_USR_MSC_Application回调)
//本工程由music_player_loop检测U盘插入并完成扫描, 此处直接返回0保持FS_TEST状态
//返回值:0,继续
u8 USH_User_App(void)
{
	return 0;
}

//主函数
int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);	//设置系统中断优先级分组2
	delay_init(168);								//初始化延时函数
	uart_init(115200);								//初始化串口波特率为115200

	printf("\r\n==== ATK-Explorer F407 LVGL Music Player Start ====\r\n");

	LED_Init();			//初始化LED
	LCD_Init();			//LCD初始化
	printf("LCD_Init OK, ID=0x%x\r\n", lcddev.id);

	FSMC_SRAM_Init();	//外部SRAM初始化(FSMC Bank1 NE3, 1MB, 供LVGL堆+SRAMEX池)
	printf("FSMC_SRAM_Init OK\r\n");

	tp_dev.init();		//触摸屏初始化
	printf("tp_dev.init OK\r\n");

	WM8978_Init();		//初始化WM8978音频编解码器
	WM8978_HPvol_Set(40,40);	//设置耳机音量
	WM8978_SPKvol_Set(40);		//设置喇叭音量
	printf("WM8978_Init OK\r\n");

	my_mem_init(SRAMIN);	//初始化内存池(内部/外部/CCM)
	printf("my_mem_init OK\r\n");
	exfuns_init();			//为fatfs相关变量分配内存
	printf("exfuns_init OK\r\n");

	TIM3_Int_Init(999,83);	//TIM3定时器初始化,1ms中断,驱动LVGL tick时钟
	printf("TIM3 tick init OK\r\n");

	lv_init();					//LVGL内核初始化
	printf("lv_init OK\r\n");
	lv_port_disp_init();		//LVGL显示驱动初始化(双缓冲+DMA刷屏)
	printf("lv_port_disp_init OK\r\n");
	lv_port_indev_init();		//LVGL输入设备(触摸屏)初始化
	printf("lv_port_indev_init OK\r\n");

	//初始化USB主机(MSC类), 用于读取U盘
	USBH_Init(&USB_OTG_Core,USB_OTG_FS_CORE_ID,&USB_Host,&USBH_MSC_cb,&USR_Callbacks);
	f_mount(fs[2],"2:",1);		//注册U盘文件系统(实际挂载在检测到U盘时进行)
	printf("USBH_Init OK\r\n");

	music_player_init();		//创建音乐播放器界面(列表页/播放页), 扫描由loop完成
	printf("music_player_init OK\r\n");

	while(1)
	{
		USBH_Process(&USB_OTG_Core, &USB_Host);		//处理USB主机事件(枚举U盘)
		music_player_loop();						//音乐播放调度(检测U盘/扫描/播放)
		lv_timer_handler();							//LVGL处理器(刷新显示/处理事件)
		delay_ms(5);
	}
}
