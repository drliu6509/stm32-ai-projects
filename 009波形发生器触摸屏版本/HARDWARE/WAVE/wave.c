#include "wave.h"
#include "math.h"
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
//实现说明：
//采用TIM7触发+DMA循环搬运的方式产生连续波形,支持六种连续波形,频率全部可调。
//1,正弦波  2,三角波  3,方波(占空比10%~90%可调)  4,梯形波
//5,锯齿波  6,反向锯齿波
//所有波形统一每周期采样200个点。
//DAC更新频率=输出频率*200,最高4KHz*200=800KHz,不超过DAC 1Msps上限。
//TIM7时钟84MHz,ARR=84MHz/(freq*200)-1。
//DMA采用循环模式,无需CPU干预,频率稳定无中断抖动。
//单次脉冲:进入脉冲模式时关闭DAC触发(TEN=0),手动写DHR立即生效;
//使用TIM6(1MHz时基)中断定时脉宽,支持脉宽1~100us和极性可调。
//DAC输出缓冲关闭,输出0~3.3V全摆幅,但输出阻抗较高(约15K)。
//DAC1输出引脚:PA4。

#define WAVE_POINT_NUM	200		//波形每周期采样点数(所有波形统一)
#define PI				3.14159265f	//圆周率

static u16 wave_table[WAVE_POINT_NUM];	//波形12位数据查找表(0~4095)

static u8 wave_type=WAVE_TRIANGLE;	//当前波形类型,默认三角波
static u8 wave_duty=50;				//方波占空比(10%~90%),默认50%
static u16 pulse_width_us=10;		//单脉冲宽度(1~100us),默认10us
static u8 pulse_polarity=0;			//单脉冲极性:0,正脉冲;1,负脉冲
static u8 pulse_busy=0;				//单脉冲输出中标志

//生成正弦波查找表
static void wave_sine_gen(void)
{
	u16 i;
	for(i=0;i<WAVE_POINT_NUM;i++)
	{
		//4095*(1+sin)/2,幅值0~4095
		wave_table[i]=(u16)(4095.0f*(1.0f+sinf(2.0f*PI*i/WAVE_POINT_NUM))/2.0f);
	}
}

//生成三角波查找表(上升100点,下降100点)
static void wave_tri_gen(void)
{
	u16 i;
	u16 half=WAVE_POINT_NUM/2;
	for(i=0;i<half;i++)wave_table[i]=(u16)((u32)i*4095/half);				//上升沿
	for(i=half;i<WAVE_POINT_NUM;i++)wave_table[i]=(u16)((u32)(WAVE_POINT_NUM-i)*4095/half);	//下降沿
}

//生成方波查找表
//duty:占空比,单位%,范围10~90
static void wave_square_gen(u8 duty)
{
	u16 i;
	u16 high_num=(u16)((u32)WAVE_POINT_NUM*duty/100);	//高电平对应的点数
	for(i=0;i<WAVE_POINT_NUM;i++)
	{
		if(i<high_num)wave_table[i]=4095;				//高电平
		else wave_table[i]=0;							//低电平
	}
}

//生成梯形波查找表(上升1/4,高平1/4,下降1/4,低平1/4)
static void wave_trap_gen(void)
{
	u16 i;
	u16 rise=WAVE_POINT_NUM/4;	//上升段点数
	u16 high=WAVE_POINT_NUM/4;	//高电平段点数
	u16 fall=WAVE_POINT_NUM/4;	//下降段点数
	for(i=0;i<rise;i++)wave_table[i]=(u16)((u32)i*4095/rise);							//上升沿
	for(i=rise;i<rise+high;i++)wave_table[i]=4095;										//高电平
	for(i=rise+high;i<rise+high+fall;i++)wave_table[i]=(u16)((u32)(rise+high+fall-i)*4095/fall);	//下降沿
	for(i=rise+high+fall;i<WAVE_POINT_NUM;i++)wave_table[i]=0;							//低电平
}

//生成锯齿波查找表(线性上升到顶后瞬间回到0)
static void wave_saw_gen(void)
{
	u16 i;
	for(i=0;i<WAVE_POINT_NUM;i++)wave_table[i]=(u16)((u32)i*4095/(WAVE_POINT_NUM-1));	//线性上升
}

//生成反向锯齿波查找表(线性下降到0后瞬间跳回顶)
static void wave_rev_saw_gen(void)
{
	u16 i;
	for(i=0;i<WAVE_POINT_NUM;i++)wave_table[i]=(u16)((u32)(WAVE_POINT_NUM-1-i)*4095/(WAVE_POINT_NUM-1));	//线性下降
}

//按当前波形类型生成查找表
static void wave_table_gen(void)
{
	switch(wave_type)
	{
		case WAVE_SINE:wave_sine_gen();break;			//正弦波
		case WAVE_SQUARE:wave_square_gen(wave_duty);break;	//方波
		case WAVE_TRAP:wave_trap_gen();break;			//梯形波
		case WAVE_SAWTOOTH:wave_saw_gen();break;		//锯齿波
		case WAVE_REV_SAW:wave_rev_saw_gen();break;		//反向锯齿波
		default:wave_tri_gen();break;					//三角波(默认)
	}
}

//DAC触发源配置
//NewState:ENABLE,TIM7触发(连续波形DMA搬运用);DISABLE,无触发(单脉冲手动输出用)
static void DAC_TriggerConfig(FunctionalState NewState)
{
	DAC_InitTypeDef DAC_InitType;
	DAC_InitType.DAC_Trigger=(NewState==ENABLE)?DAC_Trigger_T7_TRGO:DAC_Trigger_None;	//选择触发源
	DAC_InitType.DAC_WaveGeneration=DAC_WaveGeneration_None;	//不使用内置波形发生器
	DAC_InitType.DAC_LFSRUnmask_TriangleAmplitude=DAC_LFSRUnmask_Bit0;
	DAC_InitType.DAC_OutputBuffer=DAC_OutputBuffer_Disable;		//关闭输出缓冲
	DAC_Init(DAC_Channel_1,&DAC_InitType);						//重新配置DAC
}

//TIM7+DAC+DMA初始化
static void TIM7_DAC_DMA_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	DAC_InitTypeDef DAC_InitType;
	DMA_InitTypeDef DMA_InitStructure;
	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE);	//使能GPIOA时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC,ENABLE);		//使能DAC时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM7,ENABLE);		//使能TIM7时钟
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1,ENABLE);		//使能DMA1时钟
	
	//PA4配置为模拟输入,作为DAC1输出引脚
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AN;				//模拟输入
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_DOWN;			//下拉
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	
	//DAC通道1初始化,TIM7触发,DMA自动搬运波形数据
	DAC_InitType.DAC_Trigger=DAC_Trigger_T7_TRGO;			//TIM7更新事件触发DAC
	DAC_InitType.DAC_WaveGeneration=DAC_WaveGeneration_None;//不使用内置波形发生器
	DAC_InitType.DAC_LFSRUnmask_TriangleAmplitude=DAC_LFSRUnmask_Bit0;
	DAC_InitType.DAC_OutputBuffer=DAC_OutputBuffer_Disable;	//关闭输出缓冲,输出0~3.3V全摆幅
	DAC_Init(DAC_Channel_1,&DAC_InitType);					//初始化DAC通道1
	DAC_Cmd(DAC_Channel_1,ENABLE);							//使能DAC通道1
	DAC_DMACmd(DAC_Channel_1,ENABLE);						//使能DAC1的DMA请求
	
	//TIM7初始化,默认配置1KHz(触发频率200KHz)
	TIM_TimeBaseInitStructure.TIM_Period=419;				//84MHz/(419+1)=200KHz,配合200点输出1KHz
	TIM_TimeBaseInitStructure.TIM_Prescaler=0;				//预分频
	TIM_TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up;//向上计数模式
	TIM_TimeBaseInit(TIM7,&TIM_TimeBaseInitStructure);		//初始化TIM7
	TIM_ARRPreloadConfig(TIM7,ENABLE);						//ARR预装载使能,频率切换平滑无毛刺
	TIM_SelectOutputTrigger(TIM7,TIM_TRGOSource_Update);	//选择TIM7更新事件作为触发输出
	TIM_Cmd(TIM7,DISABLE);									//默认关闭,由wave_start开启
	
	//DMA1 Stream5 Channel7:存储器(波形表)→外设(DAC1数据寄存器),循环模式
	DMA_InitStructure.DMA_Channel=DMA_Channel_7;						//DMA1 Stream5 Channel7对应DAC1
	DMA_InitStructure.DMA_PeripheralBaseAddr=(u32)&DAC->DHR12R1;		//外设地址:DAC1数据保持寄存器
	DMA_InitStructure.DMA_Memory0BaseAddr=(u32)wave_table;				//存储器地址:波形查找表
	DMA_InitStructure.DMA_DIR=DMA_DIR_MemoryToPeripheral;				//存储器→外设
	DMA_InitStructure.DMA_BufferSize=WAVE_POINT_NUM;					//传输数据个数
	DMA_InitStructure.DMA_PeripheralInc=DMA_PeripheralInc_Disable;		//外设地址不递增
	DMA_InitStructure.DMA_MemoryInc=DMA_MemoryInc_Enable;				//存储器地址递增
	DMA_InitStructure.DMA_PeripheralDataSize=DMA_PeripheralDataSize_HalfWord;	//外设数据宽度16位
	DMA_InitStructure.DMA_MemoryDataSize=DMA_MemoryDataSize_HalfWord;			//存储器数据宽度16位
	DMA_InitStructure.DMA_Mode=DMA_Mode_Circular;						//循环传输,连续输出波形
	DMA_InitStructure.DMA_Priority=DMA_Priority_High;					//高优先级
	DMA_InitStructure.DMA_FIFOMode=DMA_FIFOMode_Disable;				//不使用FIFO
	DMA_InitStructure.DMA_FIFOThreshold=DMA_FIFOThreshold_HalfFull;
	DMA_InitStructure.DMA_MemoryBurst=DMA_MemoryBurst_Single;			//单次突发
	DMA_InitStructure.DMA_PeripheralBurst=DMA_PeripheralBurst_Single;
	DMA_Init(DMA1_Stream5,&DMA_InitStructure);							//初始化DMA1 Stream5
	DMA_Cmd(DMA1_Stream5,DISABLE);										//默认关闭,由wave_start开启
}

//TIM6初始化:时基1MHz,用于单脉冲宽度定时
static void TIM6_Init(void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6,ENABLE);	//使能TIM6时钟
	TIM_TimeBaseInitStructure.TIM_Period=99;			//默认100us脉宽(1MHz时基)
	TIM_TimeBaseInitStructure.TIM_Prescaler=83;			//84MHz/84=1MHz
	TIM_TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up;//向上计数
	TIM_TimeBaseInit(TIM6,&TIM_TimeBaseInitStructure);	//初始化TIM6
	TIM_Cmd(TIM6,DISABLE);								//默认关闭
	
	NVIC_InitStructure.NVIC_IRQChannel=TIM6_DAC_IRQn;			//TIM6中断(与DAC共用中断号)
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=2;		//抢占优先级2
	NVIC_InitStructure.NVIC_IRQChannelSubPriority=3;			//子优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

//设置波形输出频率
//freq:输出频率,单位Hz,范围1000~4000
void wave_set_freq(u16 freq)
{
	u16 arr;
	arr=(u16)(84000000UL/((u32)freq*WAVE_POINT_NUM)-1);	//计算定时器重装载值,触发频率=freq*200
	TIM_SetAutoreload(TIM7,arr);						//更新ARR,下一个更新事件生效,改变DAC触发频率
}

//选择波形类型
//type:波形类型(WAVE_SINE~WAVE_PULSE)
void wave_select(u8 type)
{
	if(type>WAVE_PULSE)return;	//类型非法直接返回
	wave_type=type;
	if(type==WAVE_PULSE)	//进入单脉冲模式
	{
		DAC_TriggerConfig(DISABLE);	//关闭DAC触发,手动写DHR立即生效
		//输出空闲电平
		if(pulse_polarity==0)DAC_SetChannel1Data(DAC_Align_12b_R,0);	//正脉冲空闲低电平
		else DAC_SetChannel1Data(DAC_Align_12b_R,4095);				//负脉冲空闲高电平
	}
	else	//连续波形模式
	{
		wave_table_gen();			//重新生成连续波形查找表
		DAC_TriggerConfig(ENABLE);	//恢复TIM7触发(DMA搬运波形数据用)
	}
}

//设置方波占空比
//duty:占空比,单位%,范围10~90
void wave_set_duty(u8 duty)
{
	if(duty<10)duty=10;			//下限10%
	if(duty>90)duty=90;			//上限90%
	wave_duty=duty;
	if(wave_type==WAVE_SQUARE)wave_square_gen(wave_duty);	//仅方波需要重新生成查找表
}

//设置单脉冲宽度
//us:脉冲宽度,单位us,范围1~100
void wave_pulse_set_width(u16 us)
{
	if(us<1)us=1;				//下限1us
	if(us>100)us=100;			//上限100us
	pulse_width_us=us;
}

//设置单脉冲极性
//pol:0,正脉冲(空闲低,脉冲高);1,负脉冲(空闲高,脉冲低)
void wave_pulse_set_polarity(u8 pol)
{
	pulse_polarity=pol?1:0;
	if(wave_type==WAVE_PULSE)	//单脉冲模式下立即更新空闲电平
	{
		if(pulse_polarity==0)DAC_SetChannel1Data(DAC_Align_12b_R,0);
		else DAC_SetChannel1Data(DAC_Align_12b_R,4095);
	}
}

//触发一次单脉冲
void wave_pulse_trigger(void)
{
	u16 arr;
	if(pulse_busy)return;		//脉冲输出中,忽略重复触发
	pulse_busy=1;
	//输出脉冲有效电平
	if(pulse_polarity==0)DAC_SetChannel1Data(DAC_Align_12b_R,4095);	//正脉冲输出高电平
	else DAC_SetChannel1Data(DAC_Align_12b_R,0);						//负脉冲输出低电平
	//启动TIM6定时脉宽
	arr=(u16)(pulse_width_us-1);			//1MHz时基,脉宽=us个计数
	TIM_SetAutoreload(TIM6,arr);
	TIM_SetCounter(TIM6,0);
	TIM_ITConfig(TIM6,TIM_IT_Update,ENABLE);	//使能更新中断
	TIM_Cmd(TIM6,ENABLE);						//启动TIM6
}

//查询单脉冲是否输出中
//返回值:1,输出中;0,空闲
u8 wave_pulse_busy(void)
{
	return pulse_busy;
}

//TIM6中断服务函数:单脉冲定时结束,回到空闲电平
void TIM6_DAC_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM6,TIM_IT_Update)!=RESET)
	{
		TIM_ClearITPendingBit(TIM6,TIM_IT_Update);
		TIM_Cmd(TIM6,DISABLE);					//关闭TIM6
		TIM_ITConfig(TIM6,TIM_IT_Update,DISABLE);	//关闭更新中断
		if(pulse_polarity==0)DAC_SetChannel1Data(DAC_Align_12b_R,0);	//正脉冲回到空闲低电平
		else DAC_SetChannel1Data(DAC_Align_12b_R,4095);				//负脉冲回到空闲高电平
		pulse_busy=0;
	}
}

//波形发生器初始化
//1,生成默认三角波查找表
//2,初始化TIM7+DAC+DMA
//3,初始化TIM6(单脉冲定时)
//4,默认输出频率1KHz,停止输出
void wave_init(void)
{
	wave_table_gen();			//生成默认(三角波)查找表
	TIM7_DAC_DMA_Init();		//TIM7+DAC+DMA初始化
	TIM6_Init();				//TIM6初始化(单脉冲宽度定时)
	wave_set_freq(1000);		//默认输出频率1KHz
	wave_stop();				//默认停止输出
}

//开始输出波形
void wave_start(void)
{
	DMA_Cmd(DMA1_Stream5,ENABLE);	//使能DMA传输,准备搬运波形数据
	TIM_Cmd(TIM7,ENABLE);			//使能TIM7,开始触发DAC输出波形
}

//停止输出波形(强制输出0V)
void wave_stop(void)
{
	u8 pulse_mode=0;				//记录当前是否为单脉冲模式
	TIM_Cmd(TIM7,DISABLE);			//关闭TIM7触发
	DMA_Cmd(DMA1_Stream5,DISABLE);	//关闭DMA传输
	//取消正在输出的单脉冲
	if(pulse_busy)
	{
		TIM_Cmd(TIM6,DISABLE);						//关闭TIM6
		TIM_ITConfig(TIM6,TIM_IT_Update,DISABLE);	//关闭更新中断
		TIM_ClearITPendingBit(TIM6,TIM_IT_Update);	//清除中断标志
		pulse_busy=0;
	}
	pulse_mode=(wave_type==WAVE_PULSE)?1:0;	//记录当前模式
	DAC_TriggerConfig(DISABLE);				//关闭DAC触发,手动写DHR立即生效
	DAC_SetChannel1Data(DAC_Align_12b_R,0);	//强制输出0V
	if(!pulse_mode)DAC_TriggerConfig(ENABLE);	//连续波形模式恢复TIM7触发,供下次wave_start使用
}
