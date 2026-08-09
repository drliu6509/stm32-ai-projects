#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "lcd.h"
#include "usmart.h"
#include "touch.h"
#include "app_eeprom.h"
/************************************************
 ALIENTEK STM32F407探索者开发板 实验24
 IIC实验-HAL库函数版(触摸屏启动EEPROM全片测试)
 功能: 触摸START按钮启动测试, 自动检测芯片容量,
       全片读写测试(先备份原数据, 测试完成后恢复),
       并实时显示测试进度
 说明: EEPROM测试功能封装在APP/app_eeprom.c库中
 支持: www.openedv.com
************************************************/

/* 触摸启动按钮区域 */
#define BTN_X1 55
#define BTN_X2 185
#define BTN_Y1 95
#define BTN_Y2 135

/* 判断坐标是否落在START按钮区域内 */
static u8 UI_InBtn(u16 x, u16 y)
{
    if(x >= BTN_X1 && x <= BTN_X2 && y >= BTN_Y1 && y <= BTN_Y2) return 1;
    return 0;
}

/* 绘制主界面(含容量信息与START按钮) */
static void UI_ShowIdle(void)
{
    u32 size = g_eep_cap + 1;

    LCD_Clear(WHITE);
    POINT_COLOR = RED;
    LCD_ShowString(56, 20, 130, 16, 16, (u8 *)"EEPROM FULL TEST");
    POINT_COLOR = BLUE;
    LCD_ShowString(20, 44, 60, 16, 16, (u8 *)"Chip:");
    LCD_ShowString(20, 62, 60, 16, 16, (u8 *)"Size:");
    if(g_eep_cap)
    {
        LCD_ShowString(76, 44, 100, 16, 16, EEP_CapName(g_eep_cap));
        LCD_ShowNum(76, 62, size, 5, 16);
        LCD_ShowString(132, 62, 60, 16, 16, (u8 *)"Bytes");
    }
    else
    {
        POINT_COLOR = RED;
        LCD_ShowString(76, 44, 100, 16, 16, (u8 *)"NONE!");
        LCD_ShowString(76, 62, 100, 16, 16, (u8 *)"Check EEPROM!");
        if(g_detect_err[0])             /* 显示检测失败原因 */
        {
            LCD_ShowString(20, 84, 200, 16, 16, g_detect_err);
        }
    }
    /* 绘制START按钮 */
    POINT_COLOR = BLUE;
    LCD_DrawRectangle(BTN_X1, BTN_Y1, BTN_X2, BTN_Y2);
    POINT_COLOR = RED;
    LCD_ShowString(80, BTN_Y1 + 12, 100, 16, 16, (u8 *)"START TEST");
    POINT_COLOR = BLUE;
    LCD_ShowString(20, 150, 200, 16, 16, (u8 *)"Touch START to run");
    LCD_ShowString(20, 172, 200, 16, 16, (u8 *)"the full EEPROM test.");
    LCD_ShowString(20, 194, 200, 16, 16, (u8 *)"Original data will be");
    LCD_ShowString(20, 216, 200, 16, 16, (u8 *)"restored after test.");
}

int main(void)
{
    u16 t = 0;

    HAL_Init();                     /* 初始化HAL库 */
    Stm32_Clock_Init(336, 8, 2, 7); /* 设置时钟,168Mhz */
    delay_init(168);                /* 初始化延时函数 */
    uart_init(115200);              /* 初始化USART */
    usmart_dev.init(84);            /* 初始化USMART */
    LED_Init();                     /* 初始化LED */
    LCD_Init();                     /* 初始化LCD */

    tp_dev.init();                  /* 初始化触摸屏(电阻屏会自动校准) */
    AT24CXX_Init();                 /* 确保EEPROM的IIC已初始化 */

    printf("EEPROM Full Test Start!\r\n");
    EEP_DetectCap();                /* 自动检测EEPROM芯片容量 */
    printf("EEPROM Capacity: %d\r\n", g_eep_cap);
    UI_ShowIdle();                  /* 绘制主界面 */

    while(1)
    {
        tp_dev.scan(0);             /* 扫描触摸屏 */
        if(tp_dev.sta & TP_PRES_DOWN)   /* 有触摸按下 */
        {
            if(UI_InBtn(tp_dev.x[0], tp_dev.y[0]))  /* 触摸点在START按钮内 */
            {
                while(tp_dev.sta & TP_PRES_DOWN)    /* 等待松开, 防止重复触发 */
                {
                    tp_dev.scan(0);
                    delay_ms(5);
                }
                delay_ms(20);       /* 松手消抖 */
                if(g_eep_cap)
                {
                    printf("Start EEPROM Full Test...\r\n");
                    EEP_FullTest(); /* 执行全片读写测试 */
                    printf("EEPROM Full Test Done!\r\n");
                }
                else
                {
                    LCD_ShowString(20, 250, 100, 16, 16, (u8 *)"Result: NO CHIP!");
                }
            }
        }
        t++;
        delay_ms(10);
        if(t == 20)                 /* 约200ms翻转一次 */
        {
            LED0 = !LED0;
            t = 0;
        }
    }
}
