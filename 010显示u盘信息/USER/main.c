#include "sys.h"
#include "delay.h"  
#include "usart.h"   
#include "led.h"
#include "lcd.h"
#include "key.h"  
#include "usmart.h"  
#include "sram.h"   
#include "malloc.h" 
#include "w25qxx.h"    
#include "sdio_sdcard.h"
#include "ff.h"  
#include "exfuns.h"    
#include "fontupd.h"
#include "text.h"	
#include "piclib.h"	 
#include "usbh_usr.h" 

//ALIENTEK 探索者STM32F407开发板
//010 U盘信息显示实验-库函数版本
//功能:显示U盘插入/拔出状态,并读取U盘基本信息(总容量/剩余容量/已用容量)
//支持中文显示(需要W25QXX中已有字库)
//技术支持:www.openedv.com

USBH_HOST  USB_Host;
USB_OTG_CORE_HANDLE  USB_OTG_Core;

//显示U盘基本信息
//total:U盘总容量(单位:KB)
//free:U盘剩余容量(单位:KB)
void usb_disk_info_show(u32 total,u32 free)
{
	u32 used=total-free;		//计算出已用容量
	POINT_COLOR=BLUE;	   		
	Show_Str(30,95,200,16,"文件系统:FATFS OK!",16,0);	
	Show_Str(30,115,200,16,"总容量:",16,0);	 
	Show_Str(30,135,200,16,"剩余容量:",16,0); 	    
	Show_Str(30,155,200,16,"已用容量:",16,0); 	    
	LCD_ShowNum(102,115,total>>10,6,16);	//显示U盘总容量(MB)
	LCD_ShowNum(102,135,free>>10,6,16);	//显示U盘剩余容量(MB)
	LCD_ShowNum(102,155,used>>10,6,16);	//显示U盘已用容量(MB)
      LCD_ShowString(150,115,80,16,16,(u8*)"MB");  //显示U盘总容量单位(MB)
      LCD_ShowString(150,135,80,16,16,(u8*)"MB");  //显示U盘剩余容量单位(MB)
      LCD_ShowString(150,155,80,16,16,(u8*)"MB");  //显示U盘已用容量单位(MB)
}

//用户应用函数,U盘枚举完成后,由USBH主机库调用
//返回值:0,正常
//       1,有错误
u8 USH_User_App(void)
{ 
	u32 total,free;
	u8 res=0;
	res=exf_getfree("2:",&total,&free);	//读取U盘总容量和剩余容量
	if(res==0)
	{
		usb_disk_info_show(total,free);	//显示U盘基本信息
	}else
	{
		POINT_COLOR=RED;	   
		Show_Str(30,95,200,16,"文件系统读取失败!",16,0);
	}

	return res;
} 


//TIM2初始化,用于U盘读写速度测试计时
//计数频率1MHz,即1个计数对应1us
void tim2_init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);     //使能TIM2时钟
    TIM_TimeBaseStructure.TIM_Period=0XFFFFFFFF;            //自动重装载值,32位计数器
    TIM_TimeBaseStructure.TIM_Prescaler=83;                 //预分频系数,计数频率1MHz
    TIM_TimeBaseStructure.TIM_ClockDivision=TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode=TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2,&TIM_TimeBaseStructure);          //初始化TIM2
    TIM_Cmd(TIM2,ENABLE);                                   //使能TIM2
}

//U盘读写速度测试
//在U盘根目录创建2MB测试文件,分别测试写入速度和读取速度
//按下KEY1键触发本函数
void usb_speed_test(void)
{
    u8 *speedbuf;
    UINT bw,br;
    u32 size;
    u32 t1,t2;
    u32 wspeed,rspeed;
    FRESULT res;

    if(!HCD_IsDeviceConnected(&USB_OTG_Core))//U盘未插入
    {
        POINT_COLOR=RED;
        Show_Str(30,260,200,16,"U盘未插入,无法测试!",16,0);
        return;
    }
    LCD_Fill(30,260,239,306,WHITE);     //清除速度测试显示区域
    speedbuf=mymalloc(SRAMIN,4096);     //申请4KB测试缓冲区
    if(speedbuf==NULL)
    {
        POINT_COLOR=RED;
        Show_Str(30,260,200,16,"内存申请失败!",16,0);
        return;
    }
    mymemset(speedbuf,0X5A,4096);       //填充测试数据
    POINT_COLOR=BLUE;
    Show_Str(30,260,200,16,"正在写入测试...",16,0);
    res=f_open(file,"2:SPEED.TST",FA_WRITE|FA_CREATE_ALWAYS);//创建测试文件
    wspeed=0;
    if(res==FR_OK)
    {
        size=0;
        TIM_SetCounter(TIM2,0);         //清零计数器,开始计时
        while(size<2*1024*1024)         //写入2MB测试数据
        {
            res=f_write(file,speedbuf,4096,&bw);
            if(res!=FR_OK||bw==0)break;
            size+=bw;
        }
        t1=TIM_GetCounter(TIM2);        //读取计数值
        f_close(file);                  //关闭文件
        if(t1<1)t1=1;
        wspeed=(u32)((float)size/1024*1000000/t1);//计算写入速度(KB/s)
    }

    Show_Str(30,260,200,16,"正在读取测试...",16,0);
    res=f_open(file,"2:SPEED.TST",FA_READ);//打开测试文件
    rspeed=0;
    if(res==FR_OK)
    {
        size=0;
        TIM_SetCounter(TIM2,0);         //清零计数器,开始计时
        while(size<2*1024*1024)         //读取2MB测试数据
        {
            res=f_read(file,speedbuf,4096,&br);
            if(res!=FR_OK||br==0)break;
            size+=br;
        }
        t2=TIM_GetCounter(TIM2);        //读取计数值
        f_close(file);                  //关闭文件
        if(t2<1)t2=1;
        rspeed=(u32)((float)size/1024*1000000/t2);//计算读取速度(KB/s)
    }
    f_unlink("2:SPEED.TST");    //删除测试文件
    myfree(SRAMIN,speedbuf);    //释放测试缓冲区

    LCD_Fill(30,260,239,306,WHITE);     //清除进度提示
    POINT_COLOR=BLUE;
    Show_Str(30,260,200,16,"写入速度:",16,0);
    LCD_ShowNum(110,260,wspeed,6,16);   //显示写入速度(KB/s)
    LCD_ShowString(166,260,80,16,16,(u8*)"KB/s");
    Show_Str(30,280,200,16,"读取速度:",16,0);
    LCD_ShowNum(110,280,rspeed,6,16);   //显示读取速度(KB/s)
    LCD_ShowString(166,280,80,16,16,(u8*)"KB/s");
}

int main(void)
{        
  u8 t,key;
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置系统中断优先级分组2
	delay_init(168);  //初始化延时函数
	uart_init(115200);		//初始化串口波特率为115200
	LED_Init();				//初始化LED连接的硬件接口
	KEY_Init();				//按键
  	LCD_Init();				//初始化LCD 
	W25QXX_Init();			//SPI FLASH初始化
	usmart_dev.init(84); 	//初始化USMART	 
	my_mem_init(SRAMIN);	//初始化内部内存池	
 	exfuns_init();			//为fatfs相关变量申请内存 
	piclib_init();			//初始化画图
  	f_mount(fs[0],"0:",1); 	//挂载SD卡  
  	f_mount(fs[1],"1:",1); 	//挂载外部FLASH  
  	f_mount(fs[2],"2:",1); 	//挂载U盘
	POINT_COLOR=RED;      
 	while(font_init()) 				//检查字库
	{	    
		LCD_ShowString(60,50,200,16,16,"Font Error!");
		delay_ms(200);				  
		LCD_Fill(60,50,240,66,WHITE);//清除显示	     
		delay_ms(200);				  
	}
	Show_Str(30,30,200,16,"U盘信息显示实验",16,0);					    	 
      LCD_Fill(30,60,239,76,WHITE);  //清除状态行残留
	Show_Str(30,60,200,16,"请插入U盘...",16,0);			 		
	//初始化USB主机
    tim2_init();    //初始化TIM2,用于速度测试计时
  	USBH_Init(&USB_OTG_Core,USB_OTG_FS_CORE_ID,&USB_Host,&USBH_MSC_cb,&USR_Callbacks);  
	while(1)
	{
		USBH_Process(&USB_OTG_Core, &USB_Host);
        key=KEY_Scan(0);    //扫描按键
        if(key==KEY1_PRES)  //KEY1按下
        {
            usb_speed_test();   //执行U盘读写速度测试
        }
		delay_ms(1);
		t++;
		if(t==200)
		{
			LED0=!LED0;
			t=0;
		}
	}	
}
