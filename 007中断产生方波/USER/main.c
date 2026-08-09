#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "timer.h"

/* TIM14 PWM 寄存器直操作版，完全绕开 HAL 库 */
void TIM14_PWM_Reg_Test(u32 arr,u32 psc)
{
    /* 1. 使能时钟 */
    RCC->APB1ENR |= RCC_APB1ENR_TIM14EN;   /* 使能TIM14时钟 */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOFEN;   /* 使能GPIOF时钟 */

    /* 2. 配置PF9为AF9(TIM14_CH1) */
    GPIOF->MODER   &= ~(3UL << 18);        /* 清PF9模式位 */
    GPIOF->MODER   |=  (2UL << 18);        /* PF9=AF复用模式 */
    GPIOF->AFR[1]  &= ~(0xFUL << 4);       /* 清AFRH中PF9(第9脚，AFR[1]低4位) */
    GPIOF->AFR[1]  |=  (9UL << 4);         /* PF9=AF9=TIM14_CH1 */
    GPIOF->OSPEEDR |=  (3UL << 18);        /* 高速 */
    GPIOF->PUPDR   &= ~(3UL << 18);        /* 无上下拉 */

    /* 3. 配置TIM14 */
    TIM14->PSC  = psc;                     /* 预分频 */
    TIM14->ARR  = arr;                     /* 自动重装值 */
    TIM14->CCR1 = arr/2;                   /* 比较值，50%占空比 */
    TIM14->CCMR1 = (1UL<<3) | (6UL<<4);    /* CC1S=00输出,OC1PE=1,OC1M=110(PWM1) */
    TIM14->CCER |= (1UL<<0);               /* CC1E=1 使能通道1输出 */
    TIM14->EGR  |= (1UL<<0);               /* UG=1 立即更新寄存器 */
    TIM14->CR1  |= (1UL<<0);               /* CEN=1 启动计数器 */
}

int main(void)
{
    u32 cnt1,cnt2;
    HAL_Init();                   	//初始化HAL库    
    Stm32_Clock_Init(336,8,2,7);  	//设置时钟,168Mhz
	delay_init(168);               	//初始化延时函数
	uart_init(115200);             	//初始化USART
	LED_Init();						//初始化LED	
    /* 10MHz无法被84MHz整除(84/10=8.4)，取最接近的10.5MHz：84M/((7+1)*(0+1)) = 10.5MHz */
    TIM14_PWM_Reg_Test(7,0);
    printf("=== TIM14 REG PWM test ===\r\n");
    printf("APB1ENR=0x%08X\r\n",RCC->APB1ENR);
    printf("CR1=0x%08X CCMR1=0x%08X CCER=0x%08X\r\n",TIM14->CR1,TIM14->CCMR1,TIM14->CCER);
    printf("ARR=0x%04X PSC=0x%04X CCR1=0x%04X\r\n",TIM14->ARR,TIM14->PSC,TIM14->CCR1);
    printf("GPIOF MODER=0x%08X AFRH=0x%08X\r\n",GPIOF->MODER,GPIOF->AFR[1]);
    while(1)
    {
        cnt1=TIM14->CNT;
        cnt2=TIM14->CNT;
        printf("CNT1=0x%04X CNT2=0x%04X\r\n",(u16)cnt1,(u16)cnt2);
        LED1=!LED1;                 //LED1闪烁，指示程序运行
        delay_ms(500);
    }
}
