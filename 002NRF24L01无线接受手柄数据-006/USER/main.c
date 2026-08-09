#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "lcd.h"
#include "24l01.h"
#include "stdio.h"
#include "string.h"

/************************************************
 ALIENTEK 探索者STM32F407开发板
 NRF24L01无线接收摇杆数据实验-HAL库函数版
 技术支持：www.openedv.com
 淘宝店铺：http://eboard.taobao.com 
 关注微信公众平台微信号："正点原子"，免费获取STM32资料。
 广州市星翼电子科技有限公司  
 作者：正点原子 @ALIENTEK
 修改说明:
 1.接收Arduino发送的摇杆数据(X/Y/按键)
 2.LCD显示摇杆数据、按钮状态和连接状态
 3.DS1指示灯:连接成功常亮,断开熄灭
 4.仅显示英文字符
 5.显示无线模块运行状态
************************************************/

/* 摇杆数据包结构体(匹配Arduino发送端,3字节无填充) */
struct JoystickPacket {
    u8 x;       /* 摇杆X坐标(0~255) */
    u8 y;       /* 摇杆Y坐标(0~255) */
    u8 button;  /* 摇杆按键:1=按下,0=释放 */
};

/* ACK Payload响应包:通过ACK回复给Arduino,实现双向确认 */
struct AckResponse {
    u8 rx_x;       /* 回显:收到的X */
    u8 rx_y;       /* 回显:收到的Y */
    u8 rx_button;  /* 回显:收到的按键状态 */
    u32 tick;      /* STM32收到时刻的HAL_Tick(4字节) */
};

int main(void)
{
    u8 rx_buf[4];                       /* NRF接收缓冲区 */
    struct JoystickPacket *joy;         /* 摇杆数据指针 */
    u16 rx_ok_cnt = 0;                  /* 接收成功计数 */
    u16 rx_fail_cnt = 0;                /* 接收失败计数 */
    u32 last_rx_time = 0;               /* 上次接收时间(ms) */
    u8 connected = 0;                   /* 连接标志 */
    u8 rf_status_reg = 0;               /* NRF STATUS寄存器值 */
    char disp_buf[40];                  /* LCD显示缓冲区 */

    joy = (struct JoystickPacket *)rx_buf;

    HAL_Init();                         /* 初始化HAL库 */
    Stm32_Clock_Init(336,8,2,7);        /* 设置时钟,168Mhz */
    delay_init(168);                    /* 初始化延时函数 */
    uart_init(115200);                  /* 初始化USART */
    LED_Init();                         /* 初始化LED */
    LCD_Init();                         /* 初始化LCD */
    NRF24L01_Init();                    /* 初始化NRF24L01 SPI接口 */

    /* 显示标题信息 */
    POINT_COLOR = RED;
    LCD_ShowString(30,50,200,16,16,"Explorer STM32F4");
    LCD_ShowString(30,70,200,16,16,"NRF Joystick Receiver");
    LCD_ShowString(30,90,200,16,16,"ATOM@ALIENTEK");

    /* 检测NRF24L01是否在位 */
    POINT_COLOR = BLACK;
    while(NRF24L01_Check())
    {
        LCD_ShowString(30,110,200,16,16,"RF Module: ERROR");
        delay_ms(200);
        LCD_Fill(30,110,239,110+16,WHITE);
        delay_ms(200);
    }
    LCD_ShowString(30,110,200,16,16,"RF Module: OK");

    /* 显示模式和信息 */
    POINT_COLOR = BLUE;
    LCD_ShowString(30,130,200,16,16,"Mode: RX (Receiving)");
    POINT_COLOR = BLACK;
    LCD_ShowString(0,150,lcddev.width-1,16,16,"RF: CH115 1Mbps 0dBm PL:3B");

    /* 显示标签 */
    POINT_COLOR = BLUE;
    LCD_ShowString(0,180,lcddev.width-1,16,16,"Status: WAITING FOR DATA...");

    POINT_COLOR = BLACK;
    LCD_ShowString(0,210,lcddev.width-1,16,16,"Joystick X: ---");
    LCD_ShowString(0,230,lcddev.width-1,16,16,"Joystick Y: ---");
    LCD_ShowString(0,250,lcddev.width-1,16,16,"Button:    ---");

    POINT_COLOR = BLACK;
    LCD_ShowString(0,280,lcddev.width-1,16,16,"RF Status: WAIT");
    LCD_ShowString(0,310,lcddev.width-1,16,16,"RX OK:0  FAIL:0");

    /* 设置为NRF24L01接收模式 */
    NRF24L01_RX_Mode();

    /* 关闭DS1(LED1),等待连接 */
    LED1 = 1;

    last_rx_time = HAL_GetTick();

    while(1)
    {
        /* 尝试接收数据 */
        if(NRF24L01_RxPacket(rx_buf) == 0)
        {
            struct AckResponse ack_resp;    /* ACK回复包(必须在可执行语句前声明) */

            /* 接收到有效数据 */
            rx_ok_cnt++;
            if(!connected)connected = 1;     /* 首次收到数据,标记已连接 */
            last_rx_time = HAL_GetTick();

            /* DS1(连接指示灯)点亮 */
            LED1 = 0;

            /* 更新连接状态 */
            LCD_Fill(0,180,lcddev.width,180+16,WHITE);
            POINT_COLOR = GREEN;
            LCD_ShowString(0,180,lcddev.width-1,16,16,"Status: CONNECTED");

            /* 显示摇杆X值 */
            POINT_COLOR = BLACK;
            LCD_Fill(0,210,lcddev.width,230+16,WHITE);
            sprintf(disp_buf, "Joystick X: %3d", joy->x);
            LCD_ShowString(0,210,lcddev.width-1,16,16,(u8*)disp_buf);

            /* 显示摇杆Y值 */
            sprintf(disp_buf, "Joystick Y: %3d", joy->y);
            LCD_ShowString(0,230,lcddev.width-1,16,16,(u8*)disp_buf);

            /* 显示按键状态 */
            if(joy->button)
                LCD_ShowString(0,250,lcddev.width-1,16,16,"Button:    PRESSED");
            else
                LCD_ShowString(0,250,lcddev.width-1,16,16,"Button:    RELEASED");

            /* 读取RF STATUS寄存器显示运行状态 */
            rf_status_reg = NRF24L01_Read_Reg(STATUS);
            sprintf(disp_buf, "RF Status: OK (REG=0x%02X)", rf_status_reg);
            LCD_Fill(0,280,lcddev.width,280+16,WHITE);
            POINT_COLOR = BLUE;
            LCD_ShowString(0,280,lcddev.width-1,16,16,(u8*)disp_buf);
            POINT_COLOR = BLACK;

            /* 更新接收统计 */
            LCD_Fill(0,310,lcddev.width,310+16,WHITE);
            sprintf(disp_buf, "RX OK: %5d  FAIL: %5d", rx_ok_cnt, rx_fail_cnt);
            LCD_ShowString(0,310,lcddev.width-1,16,16,(u8*)disp_buf);

            /* 串口调试输出 */
            printf("X=%d Y=%d BTN=%d [OK]\r\n", joy->x, joy->y, joy->button);

            /* 预载ACK Payload:回显收到的数据+时间戳,附加到下次ACK回复 */
            ack_resp.rx_x = joy->x;
            ack_resp.rx_y = joy->y;
            ack_resp.rx_button = joy->button;
            ack_resp.tick = HAL_GetTick();
            NRF24L01_Write_Buf(WR_ACK_PAYLOAD, (u8*)&ack_resp, sizeof(ack_resp));
        }
        else
        {
            /* 无数据,检查是否超时断开 */
            if(connected && (HAL_GetTick() - last_rx_time > 500))
            {
                connected = 0;
                rx_fail_cnt++;          /* 仅在连接断开时计1次失败 */
                LED1 = 1;               /* DS1熄灭 */

                LCD_Fill(0,180,lcddev.width,180+16,WHITE);
                POINT_COLOR = RED;
                LCD_ShowString(0,180,lcddev.width-1,16,16,"Status: DISCONNECTED");
                POINT_COLOR = BLACK;

                LCD_Fill(0,280,lcddev.width,280+16,WHITE);
                LCD_ShowString(0,280,lcddev.width-1,16,16,"RF Status: NO SIGNAL");

                /* 更新统计 */
                LCD_Fill(0,310,lcddev.width,310+16,WHITE);
                sprintf(disp_buf, "RX OK: %5d  FAIL: %5d", rx_ok_cnt, rx_fail_cnt);
                LCD_ShowString(0,310,lcddev.width-1,16,16,(u8*)disp_buf);
            }
        }

        LED0 = !LED0;       /* DS0闪烁指示系统运行 */
        delay_ms(10);
    }
}
