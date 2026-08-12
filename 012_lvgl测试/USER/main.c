#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "lcd.h"
#include "touch.h"
#include "timer.h"
#include "sram.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "demos/lv_demos.h"
//////////////////////////////////////////////////////////////////////////////////
//ALIENTEK探索者STM32F407开发板  LVGL官方Widgets例程
//使用TIM3定时器(1ms)驱动LVGL tick时钟,运行官方lv_demo_widgets例程
//串口调试信息: 115200bps, 可定位初始化卡死位置
//////////////////////////////////////////////////////////////////////////////////

//外部SRAM读写自检函数(测试SRAM末尾64字节区域,避免占用LVGL堆空间)
//返回值: 0=自检失败, 1=自检通过
static u8 sram_test(void)
{
	u32 i;
	u8 temp;
	for(i = 0; i < 64; i++)
	{
		fsmc_sram_test_write(0x100000 - 64 + i, (u8)i);	//写入递增数据
	}
	for(i = 0; i < 64; i++)
	{
		temp = fsmc_sram_test_read(0x100000 - 64 + i);	//读回验证
		if(temp != (u8)i) return 0;						//数据不一致则失败
	}
	return 1;
}

//主函数
int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);	//设置系统中断优先级分组2
	delay_init(168);								//初始化延时函数
	uart_init(115200);								//初始化串口波特率为115200

	printf("\r\n==== ATK-Explorer F407 LVGL Demo Start ====\r\n");

	LED_Init();			//初始化LED
	LCD_Init();			//LCD初始化
	printf("LCD_Init OK, ID=0x%x\r\n", lcddev.id);

	FSMC_SRAM_Init();	//外部SRAM初始化(FSMC Bank1 NE3, 1MB)
	printf("FSMC_SRAM_Init OK\r\n");
	if(sram_test() == 0)	//外部SRAM读写自检
	{
		printf("External SRAM test FAIL\r\n");
	}
	else
	{
		printf("External SRAM test PASS\r\n");
	}

	tp_dev.init();		//触摸屏初始化
	printf("tp_dev.init OK\r\n");

	TIM3_Int_Init(999,83);	//TIM3定时器初始化,1ms中断,驱动LVGL tick时钟
	printf("TIM3 tick init OK\r\n");

	lv_init();					//LVGL内核初始化
	printf("lv_init OK\r\n");

	lv_port_disp_init();		//LVGL显示驱动初始化
	printf("lv_port_disp_init OK\r\n");

	lv_port_indev_init();		//LVGL输入设备(触摸屏)初始化
	printf("lv_port_indev_init OK\r\n");

	lv_demo_widgets();			//运行LVGL官方Widgets例程
	printf("lv_demo_widgets OK\r\n");

	while(1)
	{
		lv_timer_handler();		//LVGL处理器(刷新显示/处理事件等)
		delay_ms(5);
	}
}
