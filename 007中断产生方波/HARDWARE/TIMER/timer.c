#include "timer.h"
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32F407开发板
//定时器PWM输出实验	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2017/4/7
//版本：V1.0
//版权所有，盗版必究
//Copyright(C) 广州市星翼电子科技有限公司 2014-2024
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 	

TIM_HandleTypeDef TIM14_Handler;      //定时器14句柄

//TIM14 PWM输出初始化
//PWM输出初始化
//arr：自动重装值
//psc：时钟预分频数
//TIM14挂载在APB1总线上，定时器时钟为84Mhz
//PWM输出频率计算方法：Fpwm=84M/((arr+1)*(psc+1)) (单位:Hz)
//PF9(LED0)配置为TIM14_CH1通道PWM方式，输出占空比50%的方波
void TIM14_PWM_Init(u32 arr,u32 psc)
{  
    TIM_OC_InitTypeDef TIM14_OC_Handler;
    
    TIM14_Handler.Instance=TIM14;                       //定时器14
    TIM14_Handler.Init.Prescaler=psc;                   //定时器分频
    TIM14_Handler.Init.CounterMode=TIM_COUNTERMODE_UP;  //向上计数模式
    TIM14_Handler.Init.Period=arr;                      //自动重装载值
    TIM14_Handler.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;//时钟分频因子
    HAL_TIM_PWM_Init(&TIM14_Handler);                   //初始化PWM
    
    TIM14_OC_Handler.OCMode=TIM_OCMODE_PWM1;            //模式选择PWM1
    TIM14_OC_Handler.Pulse=arr/2;                       //设置比较值，此值确定占空比(50%)
    TIM14_OC_Handler.OCPolarity=TIM_OCPOLARITY_HIGH;    //输出比较极性为高
    HAL_TIM_PWM_ConfigChannel(&TIM14_Handler,&TIM14_OC_Handler,TIM_CHANNEL_1);//配置TIM14通道1
    
    HAL_TIM_PWM_Start(&TIM14_Handler,TIM_CHANNEL_1);    //开启PWM通道1
}

//定时器底层驱动，时钟使能，引脚配置
//此函数会被HAL_TIM_PWM_Init()调用
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    GPIO_InitTypeDef GPIO_Initure;
    __HAL_RCC_TIM14_CLK_ENABLE();            //使能定时器14时钟
    __HAL_RCC_GPIOF_CLK_ENABLE();            //使能GPIOF时钟
    
    GPIO_Initure.Pin=GPIO_PIN_9;             //PF9
    GPIO_Initure.Mode=GPIO_MODE_AF_PP;       //复用推挽输出
    GPIO_Initure.Pull=GPIO_PULLUP;           //上拉
    GPIO_Initure.Speed=GPIO_SPEED_HIGH;      //高速
    GPIO_Initure.Alternate=GPIO_AF9_TIM14;   //PF9配置为TIM14_CH1
    HAL_GPIO_Init(GPIOF,&GPIO_Initure);
}
