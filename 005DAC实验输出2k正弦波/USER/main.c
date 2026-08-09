#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "dac.h"
/************************************************
 ALIENTEK探索者STM32F407开发板 实验
 DAC实验-用TIM6触发+DMA输出2K正弦波
 技术支持:www.openedv.com
************************************************/

int main(void)
{
    HAL_Init();                     	//初始化HAL库    
    Stm32_Clock_Init(336,8,2,7);    	//设置时钟,168Mhz
	delay_init(168);                	//初始化延时函数
	uart_init(115200);              	//初始化USART
    DAC1_Init();                    	//初始化DAC1(TIM6触发+DMA输出2kHz正弦波)
	
    while(1)
	{
		//DMA循环模式自动输出正弦波,无需CPU干预
	}
}
