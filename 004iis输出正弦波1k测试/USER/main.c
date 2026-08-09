#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "wm8978.h"
#include "i2s.h"
#include "sine.h"
/************************************************
 ALIENTEK探索者STM32F407开发板 
 IIS输出500Hz正弦波实验-HAL库函数版本
 技术支持：www.openedv.com
 淘宝店铺：http://eboard.taobao.com 
 关注微信公众号平台微信号："正点原子"，免费获取STM32资料。
 广州市星翼电子科技有限公司  
 作者：正点原子 @ALIENTEK
************************************************/

int main(void)
{
    HAL_Init();                   	//初始化HAL库    
    Stm32_Clock_Init(336,8,2,7);  	//设置时钟,168Mhz
	delay_init(168);              	//初始化延时函数
	uart_init(115200);            	//初始化USART1
	LED_Init();                   	//初始化LED	
	sine_tbl_init();              	//生成500Hz正弦波数据表
	
	WM8978_Init();                	//初始化WM8978
	WM8978_ADDA_Cfg(1,0);         	//开启DAC
	WM8978_Input_Cfg(0,0,0);      	//关闭输入通道
	WM8978_Output_Cfg(1,0);       	//开启DAC输出
	WM8978_I2S_Cfg(2,0);          	//I2S标准,16位数据
	WM8978_HPvol_Set(40,40);      	//设置耳机音量
	WM8978_SPKvol_Set(0);         	//关闭喇叭输出,仅使用耳机
	
	I2S2_Init(I2S_STANDARD_PHILIPS,I2S_MODE_MASTER_TX,I2S_CPOL_LOW,I2S_DATAFORMAT_16B_EXTENDED);//I2S2初始化:标准模式,主机发送,16位扩展帧
	I2S2_SampleRate_Set(SINE_SAMPLE_RATE); 		//设置采样率48KHz
	I2S2_TX_DMA_Init((u8*)sine_tbl,(u8*)sine_tbl,SINE_TABLE_SIZE);//初始化I2S2 TX DMA,双缓冲指向同一数据表
	i2s_tx_callback=sine_i2s_dma_callback;  	//设置DMA传输完成回调函数
	I2S_Play_Start();              	//开始播放500Hz正弦波
	
	printf("500Hz正弦波播放中...\r\n");
	while(1)
	{
		LED0=!LED0;
		delay_ms(200);            	//延时200ms,LED闪烁指示运行状态
	}
}
