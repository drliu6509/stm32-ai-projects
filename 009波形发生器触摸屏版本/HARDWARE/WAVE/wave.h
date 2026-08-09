#ifndef __WAVE_H
#define __WAVE_H	 
#include "sys.h"	     			    
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32F407开发板
//波形发生器 驱动代码	   
//作者：正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建时间:2026/8/7
//版本：V6.2
//版权所有，盗版必究
//Copyright(C) 广州市星翼电子科技有限公司 2014-2024
//All rights reserved 
////////////////////////////////////////////////////////////////////////////////// 	

//波形类型定义
#define WAVE_SINE		0	//正弦波
#define WAVE_TRIANGLE	1	//三角波
#define WAVE_SQUARE		2	//方波(占空比可调)
#define WAVE_TRAP		3	//梯形波
#define WAVE_SAWTOOTH	4	//锯齿波
#define WAVE_REV_SAW	5	//反向锯齿波
#define WAVE_PULSE		6	//单次脉冲

//DAC波形发生器(多波形)驱动函数
void wave_init(void);			//波形发生器初始化(默认三角波,1KHz,停止输出)
void wave_start(void);			//开始输出波形
void wave_stop(void);			//停止输出波形(单脉冲模式下取消脉冲并回到空闲电平)
void wave_select(u8 type);		//选择波形类型(WAVE_SINE~WAVE_PULSE)
void wave_set_freq(u16 freq);	//设置输出频率,单位Hz,范围1000~4000
void wave_set_duty(u8 duty);	//设置方波占空比,单位%,范围10~90
void wave_pulse_set_width(u16 us);	//设置单脉冲宽度,单位us,范围1~100
void wave_pulse_set_polarity(u8 pol);//设置单脉冲极性(0正脉冲/1负脉冲)
void wave_pulse_trigger(void);	//触发一次单脉冲
u8 wave_pulse_busy(void);		//查询单脉冲是否输出中(1输出中/0空闲)
#endif
