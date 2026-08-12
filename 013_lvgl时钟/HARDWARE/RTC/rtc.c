#include "rtc.h"
#include "delay.h"
#include "usart.h"
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

//RTC时间设置
//hour,min,sec:小时,分钟,秒钟
//ampm:@RTC_AM_PM_Definitions :RTC_H12_AM/RTC_H12_PM
//返回值:SUCEE(1),成功
//       ERROR(0),进入初始化模式失败
ErrorStatus RTC_Set_Time(u8 hour,u8 min,u8 sec,u8 ampm)
{
	RTC_TimeTypeDef RTC_TimeTypeInitStructure;

	RTC_TimeTypeInitStructure.RTC_Hours=hour;
	RTC_TimeTypeInitStructure.RTC_Minutes=min;
	RTC_TimeTypeInitStructure.RTC_Seconds=sec;
	RTC_TimeTypeInitStructure.RTC_H12=ampm;

	return RTC_SetTime(RTC_Format_BIN,&RTC_TimeTypeInitStructure);
}

//RTC日期设置
//year,month,date:年(0~99,相对2000年),月(1~12),日(1~31)
//week:星期(1~7,0,非法!)
//返回值:SUCEE(1),成功
//       ERROR(0),进入初始化模式失败
ErrorStatus RTC_Set_Date(u8 year,u8 month,u8 date,u8 week)
{
	RTC_DateTypeDef RTC_DateTypeInitStructure;
	RTC_DateTypeInitStructure.RTC_Date=date;
	RTC_DateTypeInitStructure.RTC_Month=month;
	RTC_DateTypeInitStructure.RTC_WeekDay=week;
	RTC_DateTypeInitStructure.RTC_Year=year;
	return RTC_SetDate(RTC_Format_BIN,&RTC_DateTypeInitStructure);
}

//RTC时间读取
//hour,min,sec:存放小时,分钟,秒钟
//ampm:存放上午/下午(RTC_H12_AM/RTC_H12_PM)
void RTC_Get_Time(u8 *hour,u8 *min,u8 *sec,u8 *ampm)
{
	RTC_TimeTypeDef time;

	RTC_GetTime(RTC_Format_BIN,&time);
	if(hour) *hour=time.RTC_Hours;
	if(min)  *min=time.RTC_Minutes;
	if(sec)  *sec=time.RTC_Seconds;
	if(ampm) *ampm=time.RTC_H12;
}

//RTC日期读取
//year,month,date:存放年(0~99,相对2000),月,日
//week:存放星期(1~7,1=周一...7=周日)
void RTC_Get_Date(u8 *year,u8 *month,u8 *date,u8 *week)
{
	RTC_DateTypeDef d;

	RTC_GetDate(RTC_Format_BIN,&d);
	if(year)  *year=d.RTC_Year;
	if(month) *month=d.RTC_Month;
	if(date)  *date=d.RTC_Date;
	if(week)  *week=d.RTC_WeekDay;
}

//根据年月日计算星期(蔡勒公式)
//year:实际年份(如2026),month:月(1~12),date:日(1~31)
//返回值:星期(1~7,1=周一...7=周日)
u8 RTC_Get_Week(u16 year,u8 month,u8 date)
{
	u16 y=year;
	u8 m=month;
	u8 w;

	if(m<3)					//1月2月按上一年的13月14月计算
	{
		m+=12;
		y--;
	}
	w=(date+2*m+3*(m+1)/5+y+y/4-y/100+y/400+1)%7;	//0=星期日
	if(w==0)w=7;			//转换为ST格式:1=周一...7=周日
	return w;
}

//RTC初始化
//返回值:0,初始化成功;
//       1,LSE开启失败;
//       2,进入初始化模式失败;
u8 My_RTC_Init(void)
{
	RTC_InitTypeDef RTC_InitStructure;
	u16 retry=0X1FFF;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);//使能PWR时钟
	PWR_BackupAccessCmd(ENABLE);	//使能后备寄存器访问

	if(RTC_ReadBackupRegister(RTC_BKP_DR0)!=0x5050)		//是否第一次配置?
	{
		RCC_LSEConfig(RCC_LSE_ON);//LSE 开启
		while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)	//检查指定的RCC标志位设置与否,等待低速晶振就绪
		{
			retry--;
			delay_ms(10);
		}
		if(retry==0)return 1;		//LSE 开启失败.

		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);		//设置RTC时钟(RTCCLK),选择LSE作为RTC时钟
		RCC_RTCCLKCmd(ENABLE);	//使能RTC时钟

		RTC_InitStructure.RTC_AsynchPrediv = 0x7F;//RTC异步分频系数(1~0X7F)
		RTC_InitStructure.RTC_SynchPrediv  = 0xFF;//RTC同步分频系数(0~7FFF)
		RTC_InitStructure.RTC_HourFormat   = RTC_HourFormat_24;//RTC设置为,24小时格式
		RTC_Init(&RTC_InitStructure);

		RTC_Set_Time(0,0,0,RTC_H12_AM);			//设置时间 00:00:00
		RTC_Set_Date(26,1,1,RTC_Get_Week(2026,1,1));	//设置日期 2026-01-01

		RTC_WriteBackupRegister(RTC_BKP_DR0,0x5050);	//标记已经初始化过了
	}

	return 0;
}
