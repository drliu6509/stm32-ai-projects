#ifndef __DAC_H
#define __DAC_H
#include "sys.h"
//////////////////////////////////////////////////////////////////////////////////	 
// 本程序只供学习使用，未经作者许可，不得用于其它任何用途
// ALIENTEK STM32F407探索者开发板
// DAC实验(TIM6触发+DMA输出三角波)	   
// 正点原子@ALIENTEK
// 技术支持:www.openedv.com
// 创建日期:2017/4/13
// 版本:V1.0
// 版权所有,盗版必究
// Copyright(C) 广州市星翼电子科技有限公司 2014-2024
// All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 	

#define DAC_WAVE_POINT_NUM  64         //三角波每周期采样点数(点数越少频率越高,波形越粗糙)

extern DAC_HandleTypeDef DAC1_Handler;        //DAC句柄
extern DMA_HandleTypeDef DAC_DMA_Handler;     //DMA句柄
extern TIM_HandleTypeDef TIM6_Handler;        //TIM6句柄

extern u16 DAC_WAVE_VAL[DAC_WAVE_POINT_NUM];  //三角波数据表

void DAC1_Init(void);                         //初始化DAC1(TIM6触发+DMA输出)
void DAC1_Set_Vol(u16 vol);                   //设置通道1输出电压
#endif
