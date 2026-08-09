#include "dac.h"
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

DAC_HandleTypeDef DAC1_Handler;       //DAC句柄
DMA_HandleTypeDef DAC_DMA_Handler;    //DMA句柄
TIM_HandleTypeDef TIM6_Handler;       //TIM6句柄

u16 DAC_WAVE_VAL[DAC_WAVE_POINT_NUM]; //三角波数据表

//生成三角波数据表
//上升段:0~4095,下降段:4095~0,共DAC_WAVE_POINT_NUM个点
static void DAC1_Tri_Table(void)
{
    u16 i;
    u16 half=DAC_WAVE_POINT_NUM/2;
    for(i=0;i<half;i++)
        DAC_WAVE_VAL[i]=(u16)(4095.0f*i/(half-1));                  //上升段:0~4095
    for(;i<DAC_WAVE_POINT_NUM;i++)
        DAC_WAVE_VAL[i]=(u16)(4095.0f*(DAC_WAVE_POINT_NUM-1-i)/(half-1));//下降段:4095~0
}

//初始化DAC1:TIM6触发+DMA输出三角波
//DMA循环模式自动将三角波表传输到DAC,无需CPU干预
void DAC1_Init(void)
{
    DAC_ChannelConfTypeDef DACCH1_Config;
    TIM_MasterConfigTypeDef sMasterConfig;
    
    DAC1_Tri_Table();                    //生成三角波数据表
    
    DAC1_Handler.Instance=DAC;
    HAL_DAC_Init(&DAC1_Handler);                 //初始化DAC
    
    //配置DAC通道1:使用TIM6触发(更新事件)
    DACCH1_Config.DAC_Trigger=DAC_TRIGGER_T6_TRGO;             //TIM6触发
    DACCH1_Config.DAC_OutputBuffer=DAC_OUTPUTBUFFER_DISABLE;   //DAC1输出缓冲关闭
    HAL_DAC_ConfigChannel(&DAC1_Handler,&DACCH1_Config,DAC_CHANNEL_1);//DAC通道1配置
    
    //配置TIM6:产生600kHz触发脉冲
    //三角波频率=触发频率/点数=600kHz/64≈9.4kHz
    //调整频率:PSC和ARR满足 触发频率=84MHz/((PSC+1)*(ARR+1))
    TIM6_Handler.Instance=TIM6;
    TIM6_Handler.Init.Prescaler=0;                  //分频系数:84MHz/(0+1)=84MHz
    TIM6_Handler.Init.Period=139;                   //自动重装值:84MHz/140=600kHz
    TIM6_Handler.Init.CounterMode=TIM_COUNTERMODE_UP;//向上计数模式
    TIM6_Handler.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_Base_Init(&TIM6_Handler);
    sMasterConfig.MasterOutputTrigger=TIM_TRGO_UPDATE;//TRGO输出更新事件,触发DAC
    sMasterConfig.MasterSlaveMode=TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&TIM6_Handler,&sMasterConfig);
    
    //配置DMA1_Stream5通道7(DAC1通道1专用DMA通道)
    DAC_DMA_Handler.Instance=DMA1_Stream5;
    DAC_DMA_Handler.Init.Channel=DMA_CHANNEL_7;
    DAC_DMA_Handler.Init.Direction=DMA_MEMORY_TO_PERIPH;//内存->外设
    DAC_DMA_Handler.Init.PeriphInc=DMA_PINC_DISABLE;    //外设地址不变
    DAC_DMA_Handler.Init.MemInc=DMA_MINC_ENABLE;        //内存地址递增
    DAC_DMA_Handler.Init.PeriphDataAlignment=DMA_PDATAALIGN_HALFWORD;//外设数据宽度:半字
    DAC_DMA_Handler.Init.MemDataAlignment=DMA_MDATAALIGN_HALFWORD;   //内存数据宽度:半字
    DAC_DMA_Handler.Init.Mode=DMA_CIRCULAR;         //循环模式,传输完自动重装
    DAC_DMA_Handler.Init.Priority=DMA_PRIORITY_HIGH;
    DAC_DMA_Handler.Init.FIFOMode=DMA_FIFOMODE_DISABLE;               //关闭FIFO,直接模式
    DAC_DMA_Handler.Init.FIFOThreshold=DMA_FIFO_THRESHOLD_FULL;
    DAC_DMA_Handler.Init.MemBurst=DMA_MBURST_SINGLE;                  //内存突发:单次
    DAC_DMA_Handler.Init.PeriphBurst=DMA_PBURST_SINGLE;               //外设突发:单次
    HAL_DMA_DeInit(&DAC_DMA_Handler);
    HAL_DMA_Init(&DAC_DMA_Handler);
    __HAL_LINKDMA(&DAC1_Handler,DMA_Handle1,DAC_DMA_Handler);//将DMA与DAC联系起来
    
    DMA1->LIFCR=0x000001F0;                 //清除DMA1_Stream5所有中断标志(bit4~8),避免残留标志影响
    //启动顺序参考HAL_DAC_Start_DMA():DMAEN1->DMA->使能DAC->启动TIM6
    DAC->CR |= DAC_CR_DMAEN1;               //使能DAC1通道1的DMA请求
    HAL_DMA_Start(&DAC_DMA_Handler,(u32)DAC_WAVE_VAL,(u32)&DAC->DHR12R1,DAC_WAVE_POINT_NUM);//启动DMA传输
    HAL_DAC_Start(&DAC1_Handler,DAC_CHANNEL_1);     //使能DAC通道1
    HAL_TIM_Base_Start(&TIM6_Handler);              //启动TIM6,产生触发脉冲
}

//DAC底层初始化(时钟、引脚),会被HAL_DAC_Init()调用
//hdac:DAC句柄
void HAL_DAC_MspInit(DAC_HandleTypeDef* hdac)
{      
    GPIO_InitTypeDef GPIO_Initure;
    __HAL_RCC_DAC_CLK_ENABLE();             //使能DAC时钟
    __HAL_RCC_GPIOA_CLK_ENABLE();           //使能GPIOA时钟
    __HAL_RCC_DMA1_CLK_ENABLE();            //使能DMA1时钟
	
    GPIO_Initure.Pin=GPIO_PIN_4;            //PA4
    GPIO_Initure.Mode=GPIO_MODE_ANALOG;     //模拟
    GPIO_Initure.Pull=GPIO_NOPULL;          //不带上下拉
    HAL_GPIO_Init(GPIOA,&GPIO_Initure);
}

//TIM6底层初始化(时钟),会被HAL_TIM_Base_Init()调用
//htim:TIM句柄
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* htim)
{
    __HAL_RCC_TIM6_CLK_ENABLE();            //使能TIM6时钟
}

//设置通道1输出电压
//vol:0~3300,对应0~3.3V
void DAC1_Set_Vol(u16 vol)
{
	double temp=vol;
	temp/=1000;
	temp=temp*4096/3.3;
    HAL_DAC_SetValue(&DAC1_Handler,DAC_CHANNEL_1,DAC_ALIGN_12B_R,temp);//12位右对齐数据格式设置DAC值
}
