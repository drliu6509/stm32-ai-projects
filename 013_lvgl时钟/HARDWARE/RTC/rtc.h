#ifndef __RTC_H
#define __RTC_H
#include "sys.h"
//////////////////////////////////////////////////////////////////////////////////
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32F407开发板
//RTC 驱动代码(基于正点原子实验15 RTC例程精简)
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2026/8/11
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2014-2024
//All rights reserved

//********************************************************************************
//修改说明
//V1.0 20260811
//精简自正点原子RTC例程,去掉闹钟/唤醒功能,新增RTC_Get_Time/RTC_Get_Date读取封装
//以及RTC_Get_Week星期计算函数.
//////////////////////////////////////////////////////////////////////////////////

u8 My_RTC_Init(void);   //RTC初始化
ErrorStatus RTC_Set_Time(u8 hour,u8 min,u8 sec,u8 ampm);   //RTC时间设置(24小时制用RTC_H12_AM)
ErrorStatus RTC_Set_Date(u8 year,u8 month,u8 date,u8 week); //RTC日期设置(year:0~99,相对2000年)
void RTC_Get_Time(u8 *hour,u8 *min,u8 *sec,u8 *ampm);      //RTC时间读取
void RTC_Get_Date(u8 *year,u8 *month,u8 *date,u8 *week);   //RTC日期读取
u8 RTC_Get_Week(u16 year,u8 month,u8 date);                //根据年月日计算星期(蔡勒公式)

#endif
