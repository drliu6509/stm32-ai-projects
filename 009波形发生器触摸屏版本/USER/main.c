#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "lcd.h"
#include "touch.h"
#include "wave.h"
//ALIENTEK 探索者STM32F407开发板  测试版本
//触摸屏控制DAC波形发生器
//通过触摸屏选择波形类型(正弦/三角/方波/梯形/锯齿/反向锯齿/单次脉冲)
//连续波形:拖动滑块调整输出频率(1KHz~4KHz)和方波占空比(10%~90%)
//单次脉冲:拖动滑块调整脉宽(1~100us)和极性(正脉冲/负脉冲),按PULSE触发一次
//DAC1输出引脚:PA4

//控件尺寸定义
#define KEY_W		160		//START/STOP按键宽度
#define KEY_H		60		//START/STOP按键高度
#define SEL_BTN_W	100		//波形选择按钮宽度
#define SEL_BTN_H	40		//波形选择按钮高度
#define SEL_GAP		10		//波形选择按钮间隔
#define SEL_COL		4		//波形选择按钮每行个数
#define SLIDER_W	200		//滑块宽度
#define SLIDER_H	16		//滑块高度

//全局控件坐标(根据屏幕分辨率动态计算)
u16 key_x0;		//START/STOP按键水平起点
u16 key_y1;		//START按键垂直起点
u16 key_y2;		//STOP按键垂直起点
u16 sel_x0;		//波形选择按钮水平起点
u16 sel_y1;		//波形选择按钮第一行垂直起点
u16 sel_y2;		//波形选择按钮第二行垂直起点
u16 slider_x0;	//滑块水平起点
u16 slider_y0;	//频率/脉宽滑块垂直起点
u16 duty_y0;	//占空比/极性滑块垂直起点

u16 freq_hz=1000;		//连续波形输出频率(1000~4000Hz)
u8  duty=50;			//方波占空比(10%~90%)
u16 pulse_width_us=10;	//单脉冲宽度(1~100us)
u8  pulse_polarity=0;	//单脉冲极性(0正/1负)

//画一个矩形按键
//x0,y0:按键左上角坐标
//w,h:按键尺寸
//color:按键背景色
//str:按键显示文字
void draw_key_btn(u16 x0,u16 y0,u16 w,u16 h,u16 color,u8 *str)
{
	LCD_Fill(x0,y0,x0+w-1,y0+h-1,color);	//填充按键背景色
	LCD_DrawRectangle(x0,y0,x0+w-1,y0+h-1);	//画按键边框
	POINT_COLOR=WHITE;						//文字设为白色
	BACK_COLOR=color;						//字符背景色设为按键颜色
	LCD_ShowString(x0+(w-40)/2,y0+(h-16)/2,50,16,16,str);	//居中显示文字
	BACK_COLOR=WHITE;						//恢复默认背景色
}

//画一个波形选择按钮(两行布局,每行4个)
//idx:按钮序号(0~6,第一行0~3,第二行4~6,序号7为空格不画)
//sel:是否选中(选中为蓝底白字,未选中为灰底黑字)
//str:按钮显示文字
void draw_sel_btn(u8 idx,u8 sel,u8 *str)
{
	u8 row=idx/SEL_COL;						//所在行(0或1)
	u8 col=idx%SEL_COL;						//所在列(0~3)
	u16 y=(row==0)?sel_y1:sel_y2;			//行起始坐标
	u16 x=sel_x0+col*(SEL_BTN_W+SEL_GAP);	//列起始坐标
	if(sel)	//选中状态
	{
		LCD_Fill(x,y,x+SEL_BTN_W-1,y+SEL_BTN_H-1,BLUE);	//蓝色背景
		LCD_DrawRectangle(x,y,x+SEL_BTN_W-1,y+SEL_BTN_H-1);
		POINT_COLOR=WHITE;					//白色文字
		BACK_COLOR=BLUE;
	}
	else	//未选中状态
	{
		LCD_Fill(x,y,x+SEL_BTN_W-1,y+SEL_BTN_H-1,LGRAY);	//灰色背景
		LCD_DrawRectangle(x,y,x+SEL_BTN_W-1,y+SEL_BTN_H-1);
		POINT_COLOR=BLACK;					//黑色文字
		BACK_COLOR=LGRAY;
	}
	LCD_ShowString(x+(SEL_BTN_W-24)/2,y+(SEL_BTN_H-16)/2,40,16,16,str);	//居中显示文字
	BACK_COLOR=WHITE;						//恢复默认背景色
}

//重绘所有波形选择按钮
void draw_sel_all(u8 wtype)
{
	draw_sel_btn(0,wtype==WAVE_SINE,"SIN");		//正弦波
	draw_sel_btn(1,wtype==WAVE_TRIANGLE,"TRI");	//三角波
	draw_sel_btn(2,wtype==WAVE_SQUARE,"SQR");	//方波
	draw_sel_btn(3,wtype==WAVE_TRAP,"TRA");		//梯形波
	draw_sel_btn(4,wtype==WAVE_SAWTOOTH,"SAW");	//锯齿波
	draw_sel_btn(5,wtype==WAVE_REV_SAW,"RVS");	//反向锯齿波
	draw_sel_btn(6,wtype==WAVE_PULSE,"PUL");	//单次脉冲
}

//重绘START按键文字(单脉冲模式显示PULSE,连续波形显示START)
void draw_start_btn(u8 wtype)
{
	if(wtype==WAVE_PULSE)draw_key_btn(key_x0,key_y1,KEY_W,KEY_H,GREEN,"PULSE");	//单脉冲模式:触发按键
	else draw_key_btn(key_x0,key_y1,KEY_W,KEY_H,GREEN,"START");					//连续波形:启动按键
}

//显示波形输出状态
//state:0,停止;1,输出中
void show_wave_state(u8 state)
{
	POINT_COLOR=BLUE;
	if(state)LCD_ShowString(key_x0,lcddev.height-40,100,16,16,"State:ON ");	//显示输出状态
	else LCD_ShowString(key_x0,lcddev.height-40,100,16,16,"State:OFF");		//显示停止状态
}

//显示当前频率
//freq:频率(1000~4000Hz),显示格式:FREQ:X.XKHz
void show_freq(u16 freq)
{
	POINT_COLOR=BLUE;
	LCD_ShowString(slider_x0,160,60,16,16,"FREQ:");			//频率标签
	LCD_ShowNum(slider_x0+42,160,freq/1000,1,16);			//整数部分
	LCD_ShowChar(slider_x0+50,160,'.',16,0);				//小数点
	LCD_ShowNum(slider_x0+58,160,(freq%1000)/100,1,16);		//小数部分
	LCD_ShowString(slider_x0+66,160,40,16,16,"KHz");		//单位
}

//绘制频率滑块
//freq:当前频率(1000~4000Hz)
void draw_freq_slider(u16 freq)
{
	u16 pos;
	pos=(u16)((u32)(freq-1000)*(SLIDER_W-4)/3000);			//根据频率计算滑块位置
	LCD_Fill(slider_x0,slider_y0,slider_x0+pos,slider_y0+SLIDER_H-1,BLUE);	//填充已选区域
	LCD_Fill(slider_x0+pos+1,slider_y0,slider_x0+SLIDER_W-1,slider_y0+SLIDER_H-1,GRAY);	//填充未选区域
	LCD_DrawRectangle(slider_x0,slider_y0,slider_x0+SLIDER_W-1,slider_y0+SLIDER_H-1);	//画边框
}

//根据触摸X坐标设置频率(1KHz~4KHz,100Hz步进)
//touch_x:触摸点X坐标
void set_freq_by_slider(u16 touch_x)
{
	u16 steps;
	u16 new_freq;
	steps=(u16)((u32)(touch_x-slider_x0)*30/(SLIDER_W-1));	//映射到30个档位(1K~4K)
	if(steps>30)steps=30;
	new_freq=1000+steps*100;								//转换为实际频率
	if(new_freq!=freq_hz)									//频率有变化才刷新
	{
		freq_hz=new_freq;
		wave_set_freq(freq_hz);		//更新波形输出频率
		draw_freq_slider(freq_hz);	//刷新滑块显示
		show_freq(freq_hz);			//刷新频率显示
	}
}

//显示单脉冲宽度
//us:脉宽(1~100us),显示格式:WIDTH:xxxus
void show_width(u16 us)
{
	POINT_COLOR=BLUE;
	LCD_ShowString(slider_x0,160,60,16,16,"WIDTH:");		//脉宽标签
	LCD_ShowNum(slider_x0+48,160,us,3,16);					//脉宽数值
	LCD_ShowString(slider_x0+72,160,40,16,16,"us");			//单位
}

//绘制脉宽滑块
//us:当前脉宽(1~100us)
void draw_width_slider(u16 us)
{
	u16 pos;
	pos=(u16)((u32)(us-1)*(SLIDER_W-4)/99);				//根据脉宽计算滑块位置
	LCD_Fill(slider_x0,slider_y0,slider_x0+pos,slider_y0+SLIDER_H-1,BLUE);	//填充已选区域
	LCD_Fill(slider_x0+pos+1,slider_y0,slider_x0+SLIDER_W-1,slider_y0+SLIDER_H-1,GRAY);	//填充未选区域
	LCD_DrawRectangle(slider_x0,slider_y0,slider_x0+SLIDER_W-1,slider_y0+SLIDER_H-1);	//画边框
}

//根据触摸X坐标设置单脉冲宽度(1~100us)
//touch_x:触摸点X坐标
void set_width_by_slider(u16 touch_x)
{
	u16 new_us;
	new_us=1+(u16)((u32)(touch_x-slider_x0)*99/(SLIDER_W-1));	//映射到脉宽
	if(new_us>100)new_us=100;
	if(new_us!=pulse_width_us)							//脉宽有变化才刷新
	{
		pulse_width_us=new_us;
		wave_pulse_set_width(new_us);	//更新单脉冲宽度
		draw_width_slider(new_us);		//刷新滑块显示
		show_width(new_us);				//刷新脉宽显示
	}
}

//显示当前方波占空比
//duty:占空比(10%~90%),显示格式:DUTY:xx%
void show_duty(u8 duty)
{
	POINT_COLOR=BLUE;
	LCD_ShowString(slider_x0,220,60,16,16,"DUTY:");		//占空比标签
	LCD_ShowNum(slider_x0+42,220,duty,2,16);			//占空比数值
	LCD_ShowString(slider_x0+60,220,20,16,16,"%");		//百分号
}

//绘制占空比滑块
//duty:当前占空比(10%~90%)
void draw_duty_slider(u8 duty)
{
	u16 pos;
	pos=(u16)((u32)(duty-10)*(SLIDER_W-4)/80);			//10%~90%映射到滑块位置
	LCD_Fill(slider_x0,duty_y0,slider_x0+pos,duty_y0+SLIDER_H-1,BLUE);		//填充已选区域
	LCD_Fill(slider_x0+pos+1,duty_y0,slider_x0+SLIDER_W-1,duty_y0+SLIDER_H-1,GRAY);	//填充未选区域
	LCD_DrawRectangle(slider_x0,duty_y0,slider_x0+SLIDER_W-1,duty_y0+SLIDER_H-1);	//画边框
}

//根据触摸X坐标设置方波占空比(10%~90%)
//touch_x:触摸点X坐标
void set_duty_by_slider(u16 touch_x)
{
	u8 new_duty;
	new_duty=10+(u8)((u32)(touch_x-slider_x0)*80/(SLIDER_W-1));	//映射到占空比
	if(new_duty>90)new_duty=90;
	if(new_duty!=duty)										//占空比有变化才刷新
	{
		duty=new_duty;
		wave_set_duty(duty);	//更新方波占空比
		draw_duty_slider(duty);	//刷新滑块显示
		show_duty(duty);		//刷新占空比显示
	}
}

//显示单脉冲极性
//pol:0,正脉冲;1,负脉冲,显示格式:POL:POS/NEG
void show_polarity(u8 pol)
{
	POINT_COLOR=BLUE;
	LCD_ShowString(slider_x0,220,60,16,16,"POL:");		//极性标签
	if(pol==0)LCD_ShowString(slider_x0+40,220,40,16,16,"POS");	//正脉冲
	else LCD_ShowString(slider_x0+40,220,40,16,16,"NEG");		//负脉冲
}

//绘制极性滑块(左半为POS,右半为NEG)
//pol:当前极性(0正/1负)
void draw_polarity_slider(u8 pol)
{
	u16 mid=slider_x0+(SLIDER_W-4)/2;					//滑块中点
	if(pol==0)	//正脉冲:左半高亮
	{
		LCD_Fill(slider_x0,duty_y0,mid,duty_y0+SLIDER_H-1,BLUE);
		LCD_Fill(mid+1,duty_y0,slider_x0+SLIDER_W-1,duty_y0+SLIDER_H-1,GRAY);
	}
	else		//负脉冲:右半高亮
	{
		LCD_Fill(slider_x0,duty_y0,mid,duty_y0+SLIDER_H-1,GRAY);
		LCD_Fill(mid+1,duty_y0,slider_x0+SLIDER_W-1,duty_y0+SLIDER_H-1,BLUE);
	}
	LCD_DrawRectangle(slider_x0,duty_y0,slider_x0+SLIDER_W-1,duty_y0+SLIDER_H-1);	//画边框
}

//根据触摸X坐标设置单脉冲极性(左半POS/右半NEG)
//touch_x:触摸点X坐标
void set_polarity_by_slider(u16 touch_x)
{
	u8 new_pol;
	new_pol=(touch_x<slider_x0+SLIDER_W/2)?0:1;	//判断左半还是右半
	if(new_pol!=pulse_polarity)					//极性有变化才刷新
	{
		pulse_polarity=new_pol;
		wave_pulse_set_polarity(new_pol);	//更新单脉冲极性
		draw_polarity_slider(new_pol);		//刷新滑块显示
		show_polarity(new_pol);				//刷新极性显示
	}
}

//按当前波形类型刷新滑块显示
//wtype:当前波形类型
void refresh_sliders(u8 wtype)
{
	//先清除标签区旧内容,避免切换模式后残留字符(如DUTY的%与POL的POS叠加)
	LCD_Fill(slider_x0,160,slider_x0+SLIDER_W-1,175,WHITE);	//清除频率/脉宽标签区
	LCD_Fill(slider_x0,220,slider_x0+SLIDER_W-1,235,WHITE);	//清除占空比/极性标签区
	if(wtype==WAVE_PULSE)	//单脉冲模式:显示脉宽和极性
	{
		show_width(pulse_width_us);			//显示脉宽
		draw_width_slider(pulse_width_us);	//绘制脉宽滑块
		show_polarity(pulse_polarity);		//显示极性
		draw_polarity_slider(pulse_polarity);//绘制极性滑块
	}
	else	//连续波形:显示频率和占空比
	{
		show_freq(freq_hz);			//显示当前频率
		draw_freq_slider(freq_hz);	//绘制频率滑块
		show_duty(duty);			//显示当前占空比
		draw_duty_slider(duty);		//绘制占空比滑块
	}
}

int main(void)
{
	u8 wave_state=0;	//输出状态:0,停止;1,输出中(单脉冲模式为脉冲输出中)
	u8 wtype=WAVE_TRIANGLE;	//当前波形类型,默认三角波
	u8 t=0;				//LED闪烁计数值
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);	//设置系统中断优先级分组2
	delay_init(168);								//初始化延时函数
	uart_init(115200);								//初始化串口波特率为115200
	
	LED_Init();			//初始化LED
	LCD_Init();			//LCD初始化
	tp_dev.init();		//触摸屏初始化
	wave_init();		//波形发生器初始化(默认三角波,1KHz,停止输出)
	
	LCD_Clear(WHITE);	//清屏
	//根据屏幕分辨率计算控件坐标,保证居中显示
	key_x0=(lcddev.width-KEY_W)/2;
	key_y1=(lcddev.height*3)/5;
	key_y2=key_y1+65;
	sel_x0=(lcddev.width-(SEL_BTN_W*SEL_COL+SEL_GAP*(SEL_COL-1)))/2;
	sel_y1=50;
	sel_y2=sel_y1+SEL_BTN_H+SEL_GAP;
	slider_x0=(lcddev.width-SLIDER_W)/2;
	slider_y0=183;
	duty_y0=243;
	
	//显示标题
	POINT_COLOR=RED;
	LCD_ShowString((lcddev.width-140)/2,20,140,16,16,"DAC WAVE TEST");	//标题
	//波形选择按钮
	draw_sel_all(wtype);
	//频率/脉宽显示与滑块
	refresh_sliders(wtype);
	//画START/STOP按键
	draw_start_btn(wtype);
	draw_key_btn(key_x0,key_y2,KEY_W,KEY_H,RED,"STOP");
	show_wave_state(0);		//初始化显示停止状态
	
	while(1)
	{
		tp_dev.scan(0);		//扫描触摸屏
		if(tp_dev.sta&TP_PRES_DOWN)		//有触摸按下
		{
			if(tp_dev.x[0]<lcddev.width&&tp_dev.y[0]<lcddev.height)
			{
				//波形选择按钮区域(两行,每行4个)
				if((tp_dev.y[0]>=sel_y1&&tp_dev.y[0]<=sel_y1+SEL_BTN_H-1)||
				   (tp_dev.y[0]>=sel_y2&&tp_dev.y[0]<=sel_y2+SEL_BTN_H-1))
				{
					u8 i;
					u8 row=(tp_dev.y[0]>=sel_y2)?1:0;	//判断所在行
					for(i=0;i<SEL_COL;i++)
					{
						u16 bx=sel_x0+i*(SEL_BTN_W+SEL_GAP);
						if(tp_dev.x[0]>=bx&&tp_dev.x[0]<=bx+SEL_BTN_W-1)
						{
							u8 type=row*SEL_COL+i;		//计算波形类型序号
							if(type<=WAVE_PULSE&&type!=wtype)	//跳过空格,且切换不同的波形
							{
								//从连续波形切到单脉冲模式时,先停止连续波形输出
								if(type==WAVE_PULSE&&wave_state==1)
								{
									wave_stop();
									wave_state=0;
								}
								//从单脉冲模式切走时,取消正在输出的脉冲
								if(wtype==WAVE_PULSE&&wave_pulse_busy())
								{
									wave_stop();
									wave_state=0;
								}
								wtype=type;
								wave_select(type);		//选择波形
								draw_sel_all(wtype);	//刷新选择按钮
								refresh_sliders(wtype);	//刷新滑块(频率/脉宽,占空比/极性)
								draw_start_btn(wtype);	//刷新START按键文字(START/PULSE)
								show_wave_state(wave_state);
							}
							break;
						}
					}
				}
				//频率/脉宽滑块区域(上下扩展20像素便于操作,拖动即可调节)
				else if(tp_dev.y[0]>=slider_y0-20&&tp_dev.y[0]<=slider_y0+SLIDER_H-1+20&&
						tp_dev.x[0]>=slider_x0&&tp_dev.x[0]<=slider_x0+SLIDER_W-1)
				{
					if(wtype==WAVE_PULSE)set_width_by_slider(tp_dev.x[0]);	//单脉冲模式:设置脉宽
					else set_freq_by_slider(tp_dev.x[0]);					//连续波形:设置频率
				}
				//占空比/极性滑块区域
				else if(tp_dev.y[0]>=duty_y0-20&&tp_dev.y[0]<=duty_y0+SLIDER_H-1+20&&
						tp_dev.x[0]>=slider_x0&&tp_dev.x[0]<=slider_x0+SLIDER_W-1)
				{
					if(wtype==WAVE_PULSE)set_polarity_by_slider(tp_dev.x[0]);	//单脉冲模式:设置极性
					else set_duty_by_slider(tp_dev.x[0]);						//连续波形:设置占空比
				}
				//START按键区域(单脉冲模式为PULSE触发按键)
				else if(tp_dev.x[0]>=key_x0&&tp_dev.x[0]<=key_x0+KEY_W-1&&
						tp_dev.y[0]>=key_y1&&tp_dev.y[0]<=key_y1+KEY_H-1)
				{
					if(wtype==WAVE_PULSE)
					{
						wave_pulse_trigger();	//触发一次单脉冲
					}
					else if(wave_state==0)		//波形未输出时按下,防止重复触发
					{
						wave_start();	//开始输出波形
						wave_state=1;
						show_wave_state(1);	//刷新状态显示
					}
				}
				//STOP按键区域
				else if(tp_dev.x[0]>=key_x0&&tp_dev.x[0]<=key_x0+KEY_W-1&&
						tp_dev.y[0]>=key_y2&&tp_dev.y[0]<=key_y2+KEY_H-1)
				{
					if(wtype==WAVE_PULSE)
					{
						wave_stop();	//单脉冲模式:取消脉冲并强制输出0V
						wave_state=0;
						show_wave_state(0);
					}
					else if(wave_state==1)		//波形输出中按下才停止
					{
						wave_stop();	//停止输出波形(强制输出0V)
						wave_state=0;
						show_wave_state(0);	//刷新状态显示
					}
				}
			}
		}
		//单脉冲模式:根据脉冲输出状态刷新显示
		if(wtype==WAVE_PULSE)
		{
			u8 busy=wave_pulse_busy();
			if(busy!=wave_state)
			{
				wave_state=busy;
				show_wave_state(busy);	//刷新状态显示(ON表示脉冲输出中)
			}
		}
		t++;
		if(t%20==0)LED0=!LED0;	//约100ms翻转一次,指示系统运行
		delay_ms(5);
	}
}
