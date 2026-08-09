#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "key.h"
#include "lcd.h"
#include "usmart.h"
#include "24l01.h"
#include "mpu6050.h"
#include "inv_mpu.h"
#include "inv_mpu_dmp_motion_driver.h"
#include "math.h"
#include "stdio.h"
#include "string.h"

/************************************************
 ALIENTEK 探索者STM32F407开发板
 NRF24L01无线发送MPU6050角度实验-HAL库函数版
 技术支持：www.openedv.com
 淘宝店铺：http://eboard.taobao.com 
 关注微信公众平台微信号："正点原子"，免费获取STM32资料。
 广州市星翼电子科技有限公司  
 作者：正点原子 @ALIENTEK
 修改说明:
 1.开机默认进入发送模式,无需按键选择
 2.读取MPU6050 DMP解算的俯仰角(Pitch)和横滚角(Roll)
 3.以ASCII码形式通过NRF24L01发送角度数据(32字节)
 4.发送失败后自动软件重试(最多3次)
 5.LCD显示角度值、发送内容和发送状态
************************************************/

int main(void)
{
    u8 tmp_buf[33];             /* NRF发送缓冲区:32字节+结束符 */
    char disp_buf[33];          /* LCD显示缓冲区 */
    u16 tx_ok_cnt = 0;          /* 发送成功计数 */
    u16 tx_fail_cnt = 0;        /* 发送失败计数 */
    u16 loop_cnt = 0;           /* 主循环计数器,用于限速 */
    u8 i, retry;
    u8 sta;
    float pitch, roll, yaw;     /* DMP解算的欧拉角(度,mpu_dmp_get_data已转为度) */
    int len;                    /* 格式化字符串长度 */
    
    HAL_Init();                     /* 初始化HAL库 */
    Stm32_Clock_Init(336,8,2,7);    /* 设置时钟,168Mhz */
    delay_init(168);                /* 初始化延时函数 */
    uart_init(115200);              /* 初始化USART */
    usmart_dev.init(84);            /* 初始化USMART */
    LED_Init();                     /* 初始化LED */
    KEY_Init();                     /* 初始化KEY */
    LCD_Init();                     /* 初始化LCD */
    NRF24L01_Init();                /* 初始化NRF24L01 SPI接口 */
    
    POINT_COLOR=RED;
    LCD_ShowString(30,50,200,16,16,"Explorer STM32F4");
    LCD_ShowString(30,70,200,16,16,"NRF+MPU6050 TX Test");
    LCD_ShowString(30,90,200,16,16,"ATOM@ALIENTEK");
    
    /* 检测NRF24L01是否在位 */
    while(NRF24L01_Check())
    {
        LCD_ShowString(30,110,200,16,16,"NRF24L01 Error");
        delay_ms(200);
        LCD_Fill(30,110,239,110+16,WHITE);
        delay_ms(200);
    }
    LCD_ShowString(30,110,200,16,16,"NRF24L01 OK");
    
    /* 初始化MPU6050(软件IIC) */
    while(MPU_Init())
    {
        LCD_ShowString(30,130,200,16,16,"MPU6050 Error");
        delay_ms(200);
        LCD_Fill(30,130,239,130+16,WHITE);
        delay_ms(200);
    }
    LCD_ShowString(30,130,200,16,16,"MPU6050 OK");
    
    /* 初始化MPU6050 DMP姿态解算 */
    while(mpu_dmp_init())
    {
        LCD_ShowString(30,150,200,16,16,"DMP Init Error");
        delay_ms(200);
        LCD_Fill(30,150,239,150+16,WHITE);
        delay_ms(200);
    }
    LCD_ShowString(30,150,200,16,16,"DMP Init OK");
    
    /* 默认进入发送模式并显示参数 */
    POINT_COLOR=BLUE;
    LCD_ShowString(30,170,200,16,16,"NRF TX_Mode(Default)");
    POINT_COLOR=BLACK;
    LCD_ShowString(0,190,lcddev.width-1,16,16,"NRF:CH40 2Mbps 0dBm PLoad:32B");
    
    /* 设置为NRF24L01发送模式 */
    NRF24L01_TX_Mode();
    
    while(1)
    {
        /* 轮询读取MPU6050 DMP输出的欧拉角(已是度,无需转换) */
        if(mpu_dmp_get_data(&pitch, &roll, &yaw) == 0)
        {
            loop_cnt++;
            /* 降低发送频率:每50次成功读取(约500ms)更新一次LCD并发送 */
            if(loop_cnt >= 50)
            {
                /* 以ASCII格式组装发送数据: "P:+xx.x R:+xx.x", 填充至32字节 */
                len = sprintf((char*)tmp_buf, "P:%+.1f R:%+.1f", pitch, roll);
                for(i=len; i<32; i++)tmp_buf[i]=' ';   /* 剩余字节填充空格 */
                tmp_buf[32]=0;                           /* 字符串结束符 */

                /* LCD显示当前角度值(已为度,直接显示) */
                LCD_Fill(0,210,lcddev.width,210+32,WHITE);
                POINT_COLOR=BLUE;
                LCD_ShowString(0,210,lcddev.width-1,16,16,"Pitch(deg) Roll(deg)  Yaw(deg)");
                POINT_COLOR=BLACK;
                sprintf(disp_buf, " %+6.1f     %+6.1f    %+6.1f", pitch, roll, yaw);
                LCD_ShowString(0,230,lcddev.width-1,16,16,(u8*)disp_buf);

                /* LCD显示将要发送的ASCII内容 */
                LCD_ShowString(0,250,lcddev.width-1,16,16,"TX Data:");
                POINT_COLOR=BLUE;
                LCD_ShowString(0,270,lcddev.width-1,16,16,tmp_buf);
                POINT_COLOR=BLACK;

                /* 通过NRF24L01发送数据 */
                sta = NRF24L01_TxPacket(tmp_buf);

                if(sta == TX_OK)
                {
                    tx_ok_cnt++;
                    LCD_Fill(0,290,lcddev.width,290+16,WHITE);
                    POINT_COLOR=BLUE;
                    LCD_ShowString(0,290,lcddev.width-1,16,16,"TX Status: SEND OK !");
                }
                else
                {
                    tx_fail_cnt++;
                    /* 硬件发送失败,启动软件重试 */
                    LCD_Fill(0,290,lcddev.width,290+16,WHITE);
                    POINT_COLOR=RED;
                    LCD_ShowString(0,290,lcddev.width-1,16,16,"TX Status: FAIL..Retry");
                    POINT_COLOR=BLACK;

                    for(retry=0;retry<3;retry++)
                    {
                        delay_ms(150);
                        sta = NRF24L01_TxPacket(tmp_buf);
                        if(sta == TX_OK)
                        {
                            tx_ok_cnt++;
                            LCD_Fill(0,290,lcddev.width,290+16,WHITE);
                            POINT_COLOR=BLUE;
                            LCD_ShowString(0,290,lcddev.width-1,16,16,"TX Status: SEND OK !");
                            break;
                        }
                        tx_fail_cnt++;
                    }
                }

                /* 顶部状态栏:显示收发统计 */
                LCD_Fill(0,0,lcddev.width,15,WHITE);
                POINT_COLOR=BLACK;
                LCD_ShowString(0,0,lcddev.width-1,16,16,"OK:       FAIL:        ");
                LCD_ShowNum(24,0,tx_ok_cnt,5,16);
                LCD_ShowNum(104,0,tx_fail_cnt,5,16);

                loop_cnt = 0;
                LED0=!LED0;          /* LED闪烁指示运行 */
            }
        }
        delay_ms(5);    /* 小延迟:让DMP FIFO有充足时间填充,确保读取成功 */
    }
}

