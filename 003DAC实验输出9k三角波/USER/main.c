#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "dac.h"
/************************************************
 ALIENTEK探索者STM32F407开发板 实验
 DAC实验-TIM6触发+DMA输出三角波
 技术支持:www.openedv.com
************************************************/

int main(void)
{
    u16 n=0;
    HAL_Init();                     	//初始化HAL库    
    Stm32_Clock_Init(336,8,2,7);    	//设置时钟,168Mhz
	delay_init(168);                	//初始化延时函数
	uart_init(115200);              	//初始化USART
	LED_Init();							//初始化LED	
    DAC1_Init();                    	//初始化DAC1(TIM6触发+DMA输出9.4kHz三角波)
	
    while(1)
	{
		if(++n>=500){n=0;LED0=!LED0;}	//LED翻转,指示程序运行(DMA自动输出波形,无需CPU干预)
		delay_ms(10);
	}
}
