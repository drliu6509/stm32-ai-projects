#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "key.h"
#include "lcd.h"
#include "usmart.h"
#include "24l01.h"

/************************************************
 ALIENTEK 探索者STM32F407开发板 实验33
 NRF24L01无线通信实验-HAL库函数版
 技术支持：www.openedv.com
 淘宝店铺：http://eboard.taobao.com 
 关注微信公众平台微信号："正点原子"，免费获取STM32资料。
 广州市星翼电子科技有限公司  
 作者：正点原子 @ALIENTEK
 修改说明:
 1.开机默认进入发送模式,无需按键选择
 2.发送失败后自动软件重试(最多3次)
 3.LCD显示运行状态、发送内容和收发统计
 4.显示NRF24L01发送参数信息
************************************************/

int main(void)
{
    u8 tmp_buf[33];
    u8 tx_char = ' ';       /* 发送起始字符,从空格开始 */
    u16 tx_ok_cnt = 0;      /* 发送成功计数 */
    u16 tx_fail_cnt = 0;    /* 发送失败计数 */
    u16 t = 0;              /* 延时计数 */
    u8 i, retry;
    u8 sta;

    HAL_Init();                     /* 初始化HAL库 */
    Stm32_Clock_Init(336,8,2,7);    /* 设置时钟,168Mhz */
    delay_init(168);                /* 初始化延时函数 */
    uart_init(115200);              /* 初始化USART */
    usmart_dev.init(84);            /* 初始化USMART */
    LED_Init();                     /* 初始化LED */
    KEY_Init();                     /* 初始化KEY */
    LCD_Init();                     /* 初始化LCD */
    NRF24L01_Init();                /* 初始化NRF24L01 */

    POINT_COLOR=RED;
    LCD_ShowString(30,50,200,16,16,"Explorer STM32F4");
    LCD_ShowString(30,70,200,16,16,"NRF24L01 TEST");
    LCD_ShowString(30,90,200,16,16,"ATOM@ALIENTEK");
    LCD_ShowString(30,110,200,16,16,"2017/4/15");

    /* 检测NRF24L01是否在位 */
    while(NRF24L01_Check())
    {
        LCD_ShowString(30,130,200,16,16,"NRF24L01 Error");
        delay_ms(200);
        LCD_Fill(30,130,239,130+16,WHITE);
        delay_ms(200);
    }
    LCD_ShowString(30,130,200,16,16,"NRF24L01 OK");

    /* 开机默认进入发送模式,显示模式信息 */
    POINT_COLOR=BLUE;
    LCD_ShowString(30,150,200,16,16,"NRF24L01 TX_Mode(Default)");

    /* 显示NRF24L01发送参数 */
    POINT_COLOR=BLACK;
    LCD_ShowString(0,170,lcddev.width-1,16,16,"Para:CH=40 2Mbps 0dBm AutoACK");
    LCD_ShowString(0,190,lcddev.width-1,16,16,"Retry:10(500us) PLoad:32Byte");
    LCD_ShowString(0,210,lcddev.width-1,16,16,"Addr:34 43 10 10 01(5B)");

    /* 初始化发送数据:32字节重复字符 */
    for(i=0;i<32;i++)tmp_buf[i]=tx_char;
    tmp_buf[32]=0;

    NRF24L01_TX_Mode();  /* 设置为发送模式 */

    while(1)
    {
        /* 发送数据包 */
        sta = NRF24L01_TxPacket(tmp_buf);

        if(sta == TX_OK)
        {
            tx_ok_cnt++;
            /* 显示发送成功 */
            LCD_Fill(0,230,lcddev.width,230+16,WHITE);
            POINT_COLOR=BLUE;
            LCD_ShowString(30,230,200,16,16,"Status:SEND OK !");
            LCD_ShowString(30,250,239,32,16,"Content:");
            POINT_COLOR=BLACK;
            LCD_ShowString(0,270,lcddev.width-1,32,16,tmp_buf);
        }
        else
        {
            tx_fail_cnt++;
            /* 显示发送失败 */
            LCD_Fill(0,230,lcddev.width,230+16,WHITE);
            POINT_COLOR=RED;
            LCD_ShowString(30,230,200,16,16,"Status:SEND FAIL");
            POINT_COLOR=BLACK;

            /* 软件重试:最多重试3次 */
            for(retry=0;retry<3;retry++)
            {
                delay_ms(200);
                sta = NRF24L01_TxPacket(tmp_buf);
                if(sta == TX_OK)
                {
                    tx_ok_cnt++;
                    LCD_Fill(0,230,lcddev.width,270+16,WHITE);
                    POINT_COLOR=BLUE;
                    LCD_ShowString(30,230,200,16,16,"Status:SEND OK !");
                    LCD_ShowString(30,250,239,32,16,"Content:");
                    POINT_COLOR=BLACK;
                    LCD_ShowString(0,270,lcddev.width-1,32,16,tmp_buf);
                    break;
                }
                tx_fail_cnt++;
            }
        }

        /* 顶部状态栏:显示发送字符和收发统计 */
        LCD_Fill(0,0,lcddev.width,15,WHITE);
        POINT_COLOR=BLACK;
        LCD_ShowString(0,0,lcddev.width-1,16,16,"TX:    OK:     FAIL:     ");
        LCD_ShowNum(24,0,tx_char,3,16);
        LCD_ShowNum(80,0,tx_ok_cnt,5,16);
        LCD_ShowNum(152,0,tx_fail_cnt,5,16);

        /* 准备下一次发送数据:32字节依次递增 */
        tx_char++;
        if(tx_char>'~')tx_char=' ';
        for(i=0;i<32;i++)
        {
            if(tx_char+i>'~')
                tmp_buf[i]=tx_char+i-('~'-' '+1);
            else
                tmp_buf[i]=tx_char+i;
        }
        tmp_buf[32]=0;

        LED0=!LED0;
        delay_ms(1500);
    }
}

