#include "wavplay.h" 
#include "audioplay.h"
#include "usart.h" 
#include "delay.h" 
#include "malloc.h"
#include "ff.h"
#include "i2s.h"
#include "wm8978.h"
#include "mp3ui.h"
#include "string.h"
//////////////////////////////////////////////////////////////////////////////////	 
//	本文件仅供学习使用，未经允许，不得用于任何用途
//ALIENTEK STM32F407探索者开发板
//WAV 解码播放器	   
//版权: 正点原子@ALIENTEK
//论坛: www.openedv.com
//创建日期:2014/6/29
//版本:V1.0
//版权所有，盗版必究
//Copyright(C) 正点原子@ALIENTEK 2014-2024
//All rights reserved				
//********************************************************************************
//V1.0 说明
//1,支持16位/24位WAV文件播放
//2,最高可以支持到192K/24bit的WAV格式. 
//修改说明(LVGL版):
//1,播放循环适配LVGL: 通过mp3ui_play_ctrl()服务LVGL事件/时间刷新
//2,支持进度条拖动跳转(audiodev.seeksec)
//3,支持暂停/上一曲/下一曲/返回列表(audiodev.status位操作)
////////////////////////////////////////////////////////////////////////////////// 	

__wavctrl wavctrl;		//WAV控制结构体
vu8 wavtransferend=0;	//i2s传输完成标志
vu8 wavwitchbuf=0;		//i2sbufx指示标志

//WAV解码初始化
//fname:文件路径+文件名
//wavx:wav 信息存放结构体指针
//返回值:0,成功;1,打开文件失败;2,非WAV文件;3,DATA块未找到.
u8 wav_decode_init(u8* fname,__wavctrl* wavx)
{
	FIL*ftemp;
	u8 *buf; 
	u32 br=0;
	u8 res=0;
	
	ChunkRIFF *riff;
	ChunkFMT *fmt;
	ChunkFACT *fact;
	ChunkDATA *data;
	ftemp=(FIL*)mymalloc(SRAMEX,sizeof(FIL));
	buf=mymalloc(SRAMEX,512);
	if(ftemp&&buf)	//内存分配成功
	{
		res=f_open(ftemp,(TCHAR*)fname,FA_READ);//打开文件
		if(res==FR_OK)
		{
			f_read(ftemp,buf,512,&br);	//读取512字节文件头
			riff=(ChunkRIFF *)buf;		//获取RIFF块
			if(riff->Format==0X45564157)//是WAV文件
			{
				fmt=(ChunkFMT *)(buf+12);	//获取FMT块 
				fact=(ChunkFACT *)(buf+12+8+fmt->ChunkSize);//获取FACT块
				if(fact->ChunkID==0X74636166||fact->ChunkID==0X5453494C)wavx->datastart=12+8+fmt->ChunkSize+8+fact->ChunkSize;//有fact/LIST块时的情况(未处理)
				else wavx->datastart=12+8+fmt->ChunkSize;  
				data=(ChunkDATA *)(buf+wavx->datastart);	//获取DATA块
				if(data->ChunkID==0X61746164)//解析成功!
				{
					wavx->audioformat=fmt->AudioFormat;		//音频格式
					wavx->nchannels=fmt->NumOfChannels;		//通道数
					wavx->samplerate=fmt->SampleRate;		//采样率
					wavx->bitrate=fmt->ByteRate*8;			//得到位速率
					wavx->blockalign=fmt->BlockAlign;		//块对齐
					wavx->bps=fmt->BitsPerSample;			//位数,16/24/32位
					
					wavx->datasize=data->ChunkSize;			//数据块大小
					wavx->datastart=wavx->datastart+8;		//数据开始的地方. 
				}else res=3;//data块未找到.
			}else res=2;//非wav文件
		}else res=1;//打开文件失败
		f_close(ftemp);
	}
	if(ftemp)myfree(SRAMEX,ftemp);//释放内存
	if(buf)myfree(SRAMEX,buf); 
	return res;
}

//填充buf
//buf:填充数组
//size:填充个数
//bits:位数(16/24)
//返回值:读到的数据个数
u32 wav_buffill(u8 *buf,u16 size,u8 bits)
{
	u16 readlen=0;
	u32 bread;
	u16 i;
	u8 *p;
	if(bits==24)//24bit音频,需要扩展一个
	{
		readlen=(size/4)*3;							//此处要读取的字节数
		f_read(audiodev.file,audiodev.tbuf,readlen,(UINT*)&bread);	//读取数据
		p=audiodev.tbuf;
		for(i=0;i<size;)
		{
			buf[i++]=p[1];
			buf[i]=p[2]; 
			i+=2;
			buf[i++]=p[0];
			p+=3;
		} 
		bread=(bread*4)/3;		//修正的大小.
	}else 
	{
		f_read(audiodev.file,buf,size,(UINT*)&bread);//16bit音频,直接读取数据  
		if(bread<size)//读到的数据不足,补0
		{
			for(i=bread;i<size;i++)buf[i]=0; 
		}
	}
	return bread;
}  
//WAV播放时,I2S DMA中断回调函数
void wav_i2s_dma_tx_callback(void) 
{   
	u16 i;
	if(DMA1_Stream4->CR&(1<<19))
	{
		wavwitchbuf=0;
		if((audiodev.status&0X01)==0)
		{
			for(i=0;i<WAV_I2S_TX_DMA_BUFSIZE;i++)//暂停
			{
				audiodev.i2sbuf1[i]=0;//填0
			}
		}
	}else 
	{
		wavwitchbuf=1;
		if((audiodev.status&0X01)==0)
		{
			for(i=0;i<WAV_I2S_TX_DMA_BUFSIZE;i++)//暂停
			{
				audiodev.i2sbuf2[i]=0;//填0
			}
		}
	}
	wavtransferend=1;
} 
//得到当前播放时间
//fx:文件指针
//wavx:wav播放控制结构体
void wav_get_curtime(FIL*fx,__wavctrl *wavx)
{
	long long fpos;  	
 	wavx->totsec=wavx->datasize/(wavx->bitrate/8);	//计算总时长(单位:秒) 
	fpos=fx->fptr-wavx->datastart; 					//得到当前文件播放到的位置 
	wavx->cursec=fpos*wavx->totsec/wavx->datasize;	//当前播放到第几秒?
}
//播放某个WAV文件
//fname:wav文件路径.
//返回值:
//AP_OK(0):正常播放结束
//AP_NEXT(1):用户操作停止(上一曲/下一曲/返回列表)
//AP_ERR(0X80):错误
u8 wav_play_song(u8* fname)
{
	u8 res;  
	u32 fillnum;
	audiodev.file=(FIL*)mymalloc(SRAMEX,sizeof(FIL));
	audiodev.i2sbuf1=mymalloc(SRAMEX,WAV_I2S_TX_DMA_BUFSIZE);
	audiodev.i2sbuf2=mymalloc(SRAMEX,WAV_I2S_TX_DMA_BUFSIZE);
	audiodev.tbuf=mymalloc(SRAMEX,WAV_I2S_TX_DMA_BUFSIZE);
	if(audiodev.file&&audiodev.i2sbuf1&&audiodev.i2sbuf2&&audiodev.tbuf)
	{ 
		res=wav_decode_init(fname,&wavctrl);//得到文件的信息
		if(res==0)//解析文件成功
		{
			if(wavctrl.bps==16)
			{
				WM8978_I2S_Cfg(2,0);	//飞利浦标准,16位数据长度
				I2S2_Init(I2S_Standard_Phillips,I2S_Mode_MasterTx,I2S_CPOL_Low,I2S_DataFormat_16bextended);	//飞利浦标准,主机发送,时钟低电平有效,16位扩展帧格式
			}else if(wavctrl.bps==24)
			{
				WM8978_I2S_Cfg(2,2);	//飞利浦标准,24位数据长度
				I2S2_Init(I2S_Standard_Phillips,I2S_Mode_MasterTx,I2S_CPOL_Low,I2S_DataFormat_24b);	//飞利浦标准,主机发送,时钟低电平有效,24位格式
			}
			I2S2_SampleRate_Set(wavctrl.samplerate);//设置采样率
			I2S2_TX_DMA_Init(audiodev.i2sbuf1,audiodev.i2sbuf2,WAV_I2S_TX_DMA_BUFSIZE/2); //初始化TX DMA
			i2s_tx_callback=wav_i2s_dma_tx_callback;			//回调函数指向wav_i2s_dma_callback
			audio_stop();
			res=f_open(audiodev.file,(TCHAR*)fname,FA_READ);	//打开文件
			if(res==0)
			{
				f_lseek(audiodev.file, wavctrl.datastart);		//跳过文件头
				fillnum=wav_buffill(audiodev.i2sbuf1,WAV_I2S_TX_DMA_BUFSIZE,wavctrl.bps);
				fillnum=wav_buffill(audiodev.i2sbuf2,WAV_I2S_TX_DMA_BUFSIZE,wavctrl.bps);
				audio_start();  
				while(1)
				{ 
					while(wavtransferend==0);//等待wav传输完成; 
					wavtransferend=0;
					if(fillnum!=WAV_I2S_TX_DMA_BUFSIZE)//播放结束?
					{
						res=AP_OK;
						break;
					} 
					if(wavwitchbuf)fillnum=wav_buffill(audiodev.i2sbuf2,WAV_I2S_TX_DMA_BUFSIZE,wavctrl.bps);//填充buf2
					else fillnum=wav_buffill(audiodev.i2sbuf1,WAV_I2S_TX_DMA_BUFSIZE,wavctrl.bps);//填充buf1
					//服务LVGL(触摸/时间刷新),每10ms调用一次lv_timer_handler,优先保证解码
					mp3ui_play_ctrl();
					//暂停处理:暂停时DMA回调自动填零输出静音,此处仅服务LVGL等待恢复
					if((audiodev.status&0X01)==0)
					{
						while((audiodev.status&0X01)==0)
						{
							mp3ui_play_ctrl();
							delay_ms(5);
							if((audiodev.status&(1<<1))==0)//被停止(切歌/返回列表)
							{
								res=AP_NEXT;
								break;
							}
						}
						if(res==AP_NEXT)break;
					}
					//进度条拖动跳转处理
					if(audiodev.seeksec!=0XFFFFFFFF)
					{
						u32 seekpos;
						if(wavctrl.totsec<=1)seekpos=wavctrl.datastart;
						else
						{
							if(audiodev.seeksec>=wavctrl.totsec)audiodev.seeksec=wavctrl.totsec-1;
							seekpos=wavctrl.datastart+(u32)((unsigned long long)audiodev.seeksec*(wavctrl.bitrate/8));
						}
						I2S_Play_Stop();								//停止DMA
						f_lseek(audiodev.file,seekpos);					//重新定位文件指针
						I2S2_TX_DMA_Init(audiodev.i2sbuf1,audiodev.i2sbuf2,WAV_I2S_TX_DMA_BUFSIZE/2);	//重新初始化DMA
						i2s_tx_callback=wav_i2s_dma_tx_callback;		//回调函数重新指向
						memset(audiodev.i2sbuf1,0,WAV_I2S_TX_DMA_BUFSIZE);
						memset(audiodev.i2sbuf2,0,WAV_I2S_TX_DMA_BUFSIZE);
						fillnum=wav_buffill(audiodev.i2sbuf1,WAV_I2S_TX_DMA_BUFSIZE,wavctrl.bps);
						fillnum=wav_buffill(audiodev.i2sbuf2,WAV_I2S_TX_DMA_BUFSIZE,wavctrl.bps);
						wavtransferend=0;
						audiodev.cursec=audiodev.seeksec;				//同步显示时间
						audiodev.seeksec=0XFFFFFFFF;					//清除跳转请求
						I2S_Play_Start();								//重新开始播放
						continue;
					}
					//更新时间信息(供UI显示)
					wav_get_curtime(audiodev.file,&wavctrl);
					audiodev.totsec=wavctrl.totsec;
					audiodev.cursec=wavctrl.cursec;
					audiodev.bitrate=wavctrl.bitrate;
					audiodev.samplerate=wavctrl.samplerate;
					audiodev.bps=wavctrl.bps;
					if((audiodev.status&(1<<1))==0)//用户操作停止(上一曲/下一曲/返回列表)
					{
						res=AP_NEXT;
						break;
					}
				}
				audio_stop(); 
			}else res=AP_ERR; 
		}else res=AP_ERR;
	}else res=AP_ERR; 
	myfree(SRAMEX,audiodev.tbuf);	//释放内存
	myfree(SRAMEX,audiodev.i2sbuf1);//释放内存
	myfree(SRAMEX,audiodev.i2sbuf2);//释放内存
	myfree(SRAMEX,audiodev.file);	//释放内存 
	return res;
} 
