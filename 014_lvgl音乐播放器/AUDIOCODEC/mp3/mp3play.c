#include "mp3play.h"
#include "audioplay.h"
#include "mp3ui.h"
#include "sys.h"
#include "delay.h"
#include "malloc.h"
#include "usart.h"
#include "ff.h"
#include "string.h"
#include "i2s.h"
#include "wm8978.h"
#include "key.h" 
#include "led.h" 
//////////////////////////////////////////////////////////////////////////////////	 
//��������ֲ��helix MP3�����?
//ALIENTEK STM32F407������
//MP3 �������?   
//����ԭ��@ALIENTEK
//������̳:www.openedv.com
//��������:2014/6/29
//�汾��V1.0
//********************************************************************************
//V1.0 ˵��
//1,֧��16λ������/������MP3�Ľ���
//2,֧��CBR/VBR��ʽMP3����
//3,֧��ID3V1��ID3V2��ǩ����
//4,֧�����б�����(MP3�����?20Kbps)����
////////////////////////////////////////////////////////////////////////////////// 	
 
__mp3ctrl * mp3ctrl;	//mp3���ƽṹ�� 
vu8 mp3transferend=0;	//i2s������ɱ��?
vu8 mp3witchbuf=0;		//i2sbufxָʾ��־

//MP3 DMA���ͻص�����
void mp3_i2s_dma_tx_callback(void) 
{    
	u16 i;
	if(DMA1_Stream4->CR&(1<<19))
	{
		mp3witchbuf=0;
		if((audiodev.status&0X01)==0)//��ͣ��,���?
		{
			for(i=0;i<2304*2;i++)audiodev.i2sbuf1[i]=0;
		}
	}else 
	{
		mp3witchbuf=1;
		if((audiodev.status&0X01)==0)//��ͣ��,���?
		{
			for(i=0;i<2304*2;i++)audiodev.i2sbuf2[i]=0;
		}
	} 
	mp3transferend=1;
} 
//���PCM���ݵ�DAC
//buf:PCM�����׵�ַ
//size:pcm������(16λΪ��λ)
//nch:������(1,������,2������)
void mp3_fill_buffer(u16* buf,u16 size,u8 nch)
{
	u16 i; 
	u16 *p;
	while(mp3transferend==0)//�ȴ��������?
	{
		delay_ms(1);
	};
	mp3transferend=0;
	if(mp3witchbuf==0)
	{
		p=(u16*)audiodev.i2sbuf1;
	}else 
	{
		p=(u16*)audiodev.i2sbuf2;
	}
	if(nch==2)for(i=0;i<size;i++)p[i]=buf[i];
	else	//������
	{
		for(i=0;i<size;i++)
		{
			p[2*i]=buf[i];
			p[2*i+1]=buf[i];
		}
	}
} 

//����ID3V1 
//buf:�������ݻ�����(��С�̶���128�ֽ�)
//pctrl:MP3������
//����ֵ:0,��ȡ����
//    ����,��ȡʧ��
u8 mp3_id3v1_decode(u8* buf,__mp3ctrl *pctrl)
{
	ID3V1_Tag *id3v1tag;
	id3v1tag=(ID3V1_Tag*)buf;
	if (strncmp("TAG",(char*)id3v1tag->id,3)==0)//��MP3 ID3V1 TAG
	{
		if(id3v1tag->title[0])strncpy((char*)pctrl->title,(char*)id3v1tag->title,30);
		if(id3v1tag->artist[0])strncpy((char*)pctrl->artist,(char*)id3v1tag->artist,30); 
	}else return 1;
	return 0;
}
//����ID3V2 
//buf:�������ݻ�����
//size:���ݴ�С
//pctrl:MP3������
//����ֵ:0,��ȡ����
//    ����,��ȡʧ��
u8 mp3_id3v2_decode(u8* buf,u32 size,__mp3ctrl *pctrl)
{
	ID3V2_TagHead *taghead;
	ID3V23_FrameHead *framehead; 
	u32 t;
	u32 tagsize;	//tag��С
	u32 frame_size;	//֡��С 
	taghead=(ID3V2_TagHead*)buf; 
	if(strncmp("ID3",(const char*)taghead->id,3)==0)//����ID3?
	{
		tagsize=((u32)taghead->size[0]<<21)|((u32)taghead->size[1]<<14)|((u16)taghead->size[2]<<7)|taghead->size[3];//�õ�tag ��С
		pctrl->datastart=tagsize;		//�õ�mp3���ݿ�ʼ��ƫ����
		if(tagsize>size)tagsize=size;	//tagsize��������bufsize��ʱ��,ֻ��������size��С������
		if(taghead->mversion<3)
		{
			printf("not supported mversion!\r\n");
			return 1;
		}
		t=10;
		while(t<tagsize)
		{
			framehead=(ID3V23_FrameHead*)(buf+t);
			frame_size=((u32)framehead->size[0]<<24)|((u32)framehead->size[1]<<16)|((u32)framehead->size[2]<<8)|framehead->size[3];//�õ�֡��С
 			if (strncmp("TT2",(char*)framehead->id,3)==0||strncmp("TIT2",(char*)framehead->id,4)==0)//�ҵ���������֡,��֧��unicode��ʽ!!
			{
				strncpy((char*)pctrl->title,(char*)(buf+t+sizeof(ID3V23_FrameHead)+1),AUDIO_MIN(frame_size-1,MP3_TITSIZE_MAX-1));
			}
 			if (strncmp("TP1",(char*)framehead->id,3)==0||strncmp("TPE1",(char*)framehead->id,4)==0)//�ҵ�����������֡
			{
				strncpy((char*)pctrl->artist,(char*)(buf+t+sizeof(ID3V23_FrameHead)+1),AUDIO_MIN(frame_size-1,MP3_ARTSIZE_MAX-1));
			}
			t+=frame_size+sizeof(ID3V23_FrameHead);
		} 
	}else pctrl->datastart=0;//������ID3,mp3�����Ǵ�0��ʼ
	return 0;
} 

//��ȡMP3������Ϣ
//pname:MP3�ļ�·��
//pctrl:MP3������Ϣ�ṹ�� 
//����ֵ:0,�ɹ�
//    ����,ʧ��
u8 mp3_get_info(u8 *pname,__mp3ctrl* pctrl)
{
    HMP3Decoder decoder;
    MP3FrameInfo frame_info;
	MP3_FrameXing* fxing;
	MP3_FrameVBRI* fvbri;
	FIL*fmp3;
	u8 *buf;
	u32 br;
	u8 res;
	int offset=0;
	u32 p;
	short samples_per_frame;	//һ֡�Ĳ�������
	u32 totframes;				//��֡��
	
	fmp3=mymalloc(SRAMEX,sizeof(FIL)); 
	buf=mymalloc(SRAMEX,5*1024);		//����5K�ڴ� 
	if(fmp3&&buf)//�ڴ�����ɹ�?
	{ 		
		f_open(fmp3,(const TCHAR*)pname,FA_READ);//���ļ�
		res=f_read(fmp3,(char*)buf,5*1024,&br);
		if(res==0)//��ȡ�ļ��ɹ�,��ʼ����ID3V2/ID3V1�Լ���ȡMP3��Ϣ
		{  
			mp3_id3v2_decode(buf,br,pctrl);	//����ID3V2����
			f_lseek(fmp3,fmp3->fsize-128);	//ƫ�Ƶ�����128��λ��
			f_read(fmp3,(char*)buf,128,&br);//��ȡ128�ֽ�
			mp3_id3v1_decode(buf,pctrl);	//����ID3V1����  
			decoder=MP3InitDecoder(); 		//MP3���������ڴ�
			f_lseek(fmp3,pctrl->datastart);	//ƫ�Ƶ����ݿ�ʼ�ĵط�
			f_read(fmp3,(char*)buf,5*1024,&br);	//��ȡ5K�ֽ�mp3����
 			offset=MP3FindSyncWord(buf,br);	//����֡ͬ����Ϣ
			if(offset>=0&&MP3GetNextFrameInfo(decoder,&frame_info,&buf[offset])==0)//�ҵ�֡ͬ����Ϣ��,����һ����Ϣ��ȡ����	
			{ 
				p=offset+4+32;
				fvbri=(MP3_FrameVBRI*)(buf+p);
				if(strncmp("VBRI",(char*)fvbri->id,4)==0)//����VBRI֡(VBR��ʽ)
				{
					if (frame_info.version==MPEG1)samples_per_frame=1152;//MPEG1,layer3ÿ֡����������1152
					else samples_per_frame=576;//MPEG2/MPEG2.5,layer3ÿ֡����������576 
 					totframes=((u32)fvbri->frames[0]<<24)|((u32)fvbri->frames[1]<<16)|((u16)fvbri->frames[2]<<8)|fvbri->frames[3];//�õ���֡��
					pctrl->totsec=totframes*samples_per_frame/frame_info.samprate;//�õ��ļ��ܳ���
				}else	//����VBRI֡,�����ǲ���Xing֡(VBR��ʽ)
				{  
					if (frame_info.version==MPEG1)	//MPEG1 
					{
						p=frame_info.nChans==2?32:17;
						samples_per_frame = 1152;	//MPEG1,layer3ÿ֡����������1152
					}else
					{
						p=frame_info.nChans==2?17:9;
						samples_per_frame=576;		//MPEG2/MPEG2.5,layer3ÿ֡����������576
					}
					p+=offset+4;
					fxing=(MP3_FrameXing*)(buf+p);
					if(strncmp("Xing",(char*)fxing->id,4)==0||strncmp("Info",(char*)fxing->id,4)==0)//��Xng֡
					{
						if(fxing->flags[3]&0X01)//������frame�ֶ�
						{
							totframes=((u32)fxing->frames[0]<<24)|((u32)fxing->frames[1]<<16)|((u16)fxing->frames[2]<<8)|fxing->frames[3];//�õ���֡��
							pctrl->totsec=totframes*samples_per_frame/frame_info.samprate;//�õ��ļ��ܳ���
						}else	//��������frames�ֶ�
						{
							pctrl->totsec=fmp3->fsize/(frame_info.bitrate/8);
						} 
					}else 		//CBR��ʽ,ֱ�Ӽ����ܲ���ʱ��
					{
						pctrl->totsec=fmp3->fsize/(frame_info.bitrate/8);
					}
				} 
				pctrl->bitrate=frame_info.bitrate;			//�õ���ǰ֡������
				mp3ctrl->samplerate=frame_info.samprate; 	//�õ�������. 
				if(frame_info.nChans==2)mp3ctrl->outsamples=frame_info.outputSamps; //���PCM��������С 
				else mp3ctrl->outsamples=frame_info.outputSamps*2; //���PCM��������С,���ڵ�����MP3,ֱ��*2,����Ϊ˫�������?
			}else res=0XFE;//δ�ҵ�ͬ��֡	
			MP3FreeDecoder(decoder);//�ͷ��ڴ�		
		} 
		f_close(fmp3);
	}else res=0XFF;
	myfree(SRAMEX,fmp3);
	myfree(SRAMEX,buf);	
	return res;	
}  
//�õ���ǰ����ʱ��
//fx:�ļ�ָ��
//mp3x:mp3���ſ�����
void mp3_get_curtime(FIL*fx,__mp3ctrl *mp3x)
{
	u32 fpos=0;  	 
	if(fx->fptr>mp3x->datastart)fpos=fx->fptr-mp3x->datastart;	//�õ���ǰ�ļ����ŵ��ĵط� 
	mp3x->cursec=fpos*mp3x->totsec/(fx->fsize-mp3x->datastart);	//��ǰ���ŵ��ڶ�������?	
}
//mp3�ļ�������˺���?
//pos:��Ҫ��λ�����ļ�λ��
//����ֵ:��ǰ�ļ�λ��(����λ��Ľ��)
u32 mp3_file_seek(u32 pos)
{
	if(pos>audiodev.file->fsize)
	{
		pos=audiodev.file->fsize;
	}
	f_lseek(audiodev.file,pos);
	return audiodev.file->fptr;
}
//����һ��MP3����
//fname:MP3�ļ�·��.
//����ֵ:0,�����������?
//[b7]:0,����״̬;1,����״̬
//[b6:0]:b7=0ʱ,��ʾ������ 
//       b7=1ʱ,��ʾ�д���(���ﲻ�ж��������?0X80~0XFF,�����Ǵ���)
u8 mp3_play_song(u8* fname)
{ 
	HMP3Decoder mp3decoder=NULL;
	MP3FrameInfo mp3frameinfo;
	u8 res;
	u8* buffer;		//����buffer  
	u8* readptr;	//MP3�����ָ��?
	int offset=0;	//ƫ����
	int outofdata=0;//�������ݷ�Χ
	int bytesleft=0;//buffer��ʣ�����Ч����?
	u32 br=0; 
	int err=0;  
	
 	mp3ctrl=mymalloc(SRAMEX,sizeof(__mp3ctrl)); 
	buffer=mymalloc(SRAMEX,MP3_FILE_BUF_SZ); 	//�������buf��С
	audiodev.file=(FIL*)mymalloc(SRAMEX,sizeof(FIL));
	audiodev.i2sbuf1=mymalloc(SRAMEX,2304*2);
	audiodev.i2sbuf2=mymalloc(SRAMEX,2304*2);
	audiodev.tbuf=mymalloc(SRAMEX,2304*2);
	audiodev.file_seek=mp3_file_seek;
	audiodev.seeksec=0XFFFFFFFF;//clear progress seek request
	
	if(!mp3ctrl||!buffer||!audiodev.file||!audiodev.i2sbuf1||!audiodev.i2sbuf2||!audiodev.tbuf)//�ڴ�����ʧ��
	{
		myfree(SRAMEX,mp3ctrl);
		myfree(SRAMEX,buffer);
		myfree(SRAMEX,audiodev.file);
		myfree(SRAMEX,audiodev.i2sbuf1);
		myfree(SRAMEX,audiodev.i2sbuf2);
		myfree(SRAMEX,audiodev.tbuf); 
		return AP_ERR;	//����
	} 
	memset(audiodev.i2sbuf1,0,2304*2);	//�������� 
	memset(audiodev.i2sbuf2,0,2304*2);	//�������� 
	memset(mp3ctrl,0,sizeof(__mp3ctrl));//�������� 
	res=mp3_get_info(fname,mp3ctrl);  
	if(res==0)
	{ 
		printf("     title:%s\r\n",mp3ctrl->title); 
		printf("    artist:%s\r\n",mp3ctrl->artist); 
		printf("   bitrate:%dbps\r\n",mp3ctrl->bitrate);	
		printf("samplerate:%d\r\n", mp3ctrl->samplerate);	
		printf("  totalsec:%d\r\n",mp3ctrl->totsec); 
		
		WM8978_I2S_Cfg(2,0);	//�����ֱ�׼,16λ���ݳ���
	I2S2_Init(I2S_Standard_Phillips,I2S_Mode_MasterTx,I2S_CPOL_Low,I2S_DataFormat_16bextended);	//�����ֱ�׼,��������,ʱ�ӵ͵�ƽ��Ч,16λ��չ֡����
	 
		I2S2_SampleRate_Set(mp3ctrl->samplerate);		//���ò����� 
		I2S2_TX_DMA_Init(audiodev.i2sbuf1,audiodev.i2sbuf2,mp3ctrl->outsamples);//����TX DMA
		i2s_tx_callback=mp3_i2s_dma_tx_callback;		//�ص�����ָ��mp3_i2s_dma_tx_callback
		mp3decoder=MP3InitDecoder(); 					//MP3���������ڴ�
		res=f_open(audiodev.file,(char*)fname,FA_READ);	//���ļ�
	}
	if(res==0&&mp3decoder!=0)//���ļ��ɹ�
	{ 
		f_lseek(audiodev.file,mp3ctrl->datastart);	//�����ļ�ͷ��tag��Ϣ
		audio_start();								//��ʼ���� 
		while(res==0)
		{
			if(audiodev.seeksec!=0XFFFFFFFF)   //process progress seek request
			{
			u32 seekpos;
			if(mp3ctrl->totsec<=1)seekpos=mp3ctrl->datastart;
			else
			{
			if(audiodev.seeksec>=mp3ctrl->totsec)audiodev.seeksec=mp3ctrl->totsec-1;
			seekpos=mp3ctrl->datastart+(u32)((unsigned long long)audiodev.seeksec*(audiodev.file->fsize-mp3ctrl->datastart)/mp3ctrl->totsec);
			
			}
			f_lseek(audiodev.file,seekpos);
			MP3FreeDecoder(mp3decoder);
			mp3decoder=MP3InitDecoder();
			bytesleft=0;
			memset(audiodev.i2sbuf1,0,2304*2);
			memset(audiodev.i2sbuf2,0,2304*2);
			audiodev.cursec=audiodev.seeksec;
			audiodev.seeksec=0XFFFFFFFF;
			}
			readptr=buffer;	//MP3��ָ��ָ��buffer
			offset=0;		//ƫ����Ϊ0
			outofdata=0;	//��������
			bytesleft=0;	
			res=f_read(audiodev.file,buffer,MP3_FILE_BUF_SZ,&br);//һ�ζ�ȡMP3_FILE_BUF_SZ�ֽ�
			if(res)//�����ݳ�����
			{
				res=AP_ERR;
				break;
			}
			if(br==0)		//����Ϊ0,˵�����������?
			{
				res=AP_OK;	//�������?
				break;
			}
			bytesleft+=br;	//buffer�����ж�����ЧMP3����?
			err=0;			
			while(!outofdata)//û�г��������쳣(���ɷ��ҵ�֡ͬ���ַ�)
			{
				offset=MP3FindSyncWord(readptr,bytesleft);//��readptrλ��,��ʼ����ͬ���ַ�
				if(offset<0)	//û���ҵ�ͬ���ַ�,����֡����ѭ��
				{ 
					outofdata=1;//û�ҵ�֡ͬ���ַ�
				}else	//�ҵ�ͬ���ַ���
				{
					readptr+=offset;		//MP3��ָ��ƫ�Ƶ�ͬ���ַ���.
					bytesleft-=offset;		//buffer�������Ч���ݸ���?�����ȥƫ����?
					err=MP3Decode(mp3decoder,&readptr,&bytesleft,(short*)audiodev.tbuf,0);//����һ֡MP3����
					if(err!=0)
					{
						printf("decode error:%d\r\n",err);
						break;
					}else
					{
						MP3GetLastFrameInfo(mp3decoder,&mp3frameinfo);	//�õ��ոս����MP3֡��Ϣ
						if(mp3ctrl->bitrate!=mp3frameinfo.bitrate)		//��������
						{
							mp3ctrl->bitrate=mp3frameinfo.bitrate; 
						}
						mp3_fill_buffer((u16*)audiodev.tbuf,mp3frameinfo.outputSamps,mp3frameinfo.nChans);//���pcm����
					}
					if(bytesleft<MAINBUF_SIZE*2)//����������С��2��MAINBUF_SIZE��ʱ��,���벹���µ����ݽ���.
					{ 
						memmove(buffer,readptr,bytesleft);//�ƶ�readptr��ָ������ݵ�buffer����,��������СΪ:bytesleft
						f_read(audiodev.file,buffer+bytesleft,MP3_FILE_BUF_SZ-bytesleft,&br);//�������µ�����
						if(br<MP3_FILE_BUF_SZ-bytesleft)
						{
							memset(buffer+bytesleft+br,0,MP3_FILE_BUF_SZ-bytesleft-br); 
						}
						bytesleft=MP3_FILE_BUF_SZ;
						readptr=buffer; 
					} 	
 					while(audiodev.status&(1<<1))//����������
					{			 
						delay_ms(1);
						mp3_get_curtime(audiodev.file,mp3ctrl); 
						audiodev.totsec=mp3ctrl->totsec;	//��������
						audiodev.cursec=mp3ctrl->cursec;
						audiodev.bitrate=mp3ctrl->bitrate;
						audiodev.samplerate=mp3ctrl->samplerate;
						audiodev.bps=16;
						mp3ui_play_ctrl();	//touch control and time refresh
						if(audiodev.seeksec!=0XFFFFFFFF)
						{
						    outofdata=1;
						    break;
						}
 						if(audiodev.status&0X01)break;//û�а�����ͣ 
					}
					if((audiodev.status&(1<<1))==0)//�����������?�������?
					{  
						res=AP_NEXT;//�������ϼ�ѭ��
						outofdata=1;//������һ��ѭ��
						break;
					}  
				}					
			}  
		}
		audio_stop();//�ر���Ƶ���?
	}else res=AP_ERR;//����
	f_close(audiodev.file);
	MP3FreeDecoder(mp3decoder);		//�ͷ��ڴ�	
	myfree(SRAMEX,mp3ctrl);
	myfree(SRAMEX,buffer);
	myfree(SRAMEX,audiodev.file);
	myfree(SRAMEX,audiodev.i2sbuf1);
	myfree(SRAMEX,audiodev.i2sbuf2);
	myfree(SRAMEX,audiodev.tbuf);
	return res;
}

//Extract embedded album cover (ID3v2 APIC frame, usually JPEG data)
//fname: MP3 file path
//outbuf: output buffer for the picture data
//maxsize: output buffer size
//return: picture data length, 0 = no cover found
u32 mp3_get_cover(u8 *fname,u8 *outbuf,u32 maxsize)
{
	FIL *f;
	u8 *buf;
	u32 br;
	u32 tagsize;
	u8 version;
	u32 jpeglen=0;
	f=mymalloc(SRAMEX,sizeof(FIL));
	buf=mymalloc(SRAMEX,512);
	if(f&&buf)
	{
		if(f_open(f,(const TCHAR*)fname,FA_READ)==FR_OK)
		{
			f_read(f,buf,512,&br);
			if(br>=10&&strncmp("ID3",(char*)buf,3)==0)			//has ID3v2 tag
			{
				version=((ID3V2_TagHead*)buf)->mversion;		//major version
				if(version>=3)									//v2.3 / v2.4
				{
					tagsize=((u32)((ID3V2_TagHead*)buf)->size[0]<<21)|
							((u32)((ID3V2_TagHead*)buf)->size[1]<<14)|
							((u16)((ID3V2_TagHead*)buf)->size[2]<<7)|
							((ID3V2_TagHead*)buf)->size[3];		//synchsafe tag size
					if(tagsize>0&&tagsize<=128*1024)			//tag size limited
					{
						u8 *tagbuf;
						u32 rdlen=0;
						tagbuf=mymalloc(SRAMEX,tagsize);		//alloc temp tag buffer
						if(tagbuf)
						{
							f_lseek(f,10);						//skip tag header
							while(rdlen<tagsize)				//read whole tag
							{
								f_read(f,tagbuf+rdlen,tagsize-rdlen,&br);
								if(br==0)break;
								rdlen+=br;
							}
							if(rdlen>=tagsize)
							{
								u32 t=0;
								while(t+10<=tagsize)			//scan frames
								{
									ID3V23_FrameHead *fh=(ID3V23_FrameHead*)(tagbuf+t);
									u32 fs;
									if(version>=4)				//v2.4: synchsafe size
									{
										fs=((u32)fh->size[0]<<21)|((u32)fh->size[1]<<14)|
										   ((u16)fh->size[2]<<7)|fh->size[3];
									}
									else						//v2.3: big endian size
									{
										fs=((u32)fh->size[0]<<24)|((u32)fh->size[1]<<16)|
										   ((u16)fh->size[2]<<8)|fh->size[3];
									}
									if(strncmp("APIC",(char*)fh->id,4)==0&&fs>=8)	//APIC frame found
									{
										u8 enc=tagbuf[t+10];	//text encoding byte
										u32 p=t+11;				//MIME string start
										u32 img_start=0,img_len=0;
										u32 frame_end=t+10+fs;
										while(p<frame_end&&tagbuf[p]!=0)p++;//skip MIME string
										if(p<frame_end)p++;		//skip null terminator
										if(p<frame_end)p++;		//skip picture type byte
										if(enc==1||enc==2)		//UTF-16: double null terminator
										{
											while(p+1<frame_end)
											{
												if(tagbuf[p]==0&&tagbuf[p+1]==0)break;
												p++;
											}
											if(p+1<frame_end)p+=2;
										}
										else					//ISO-8859-1/UTF-8: single null
										{
											while(p<frame_end&&tagbuf[p]!=0)p++;
											if(p<frame_end)p++;
										}
										img_start=p;			//picture data start
										img_len=frame_end-p;
										if(img_len>maxsize)img_len=maxsize;
										if(img_len>=4&&tagbuf[img_start]==0xFF&&tagbuf[img_start+1]==0xD8)
										{						//JPEG picture data
											memcpy(outbuf,tagbuf+img_start,img_len);
											jpeglen=img_len;
										}
										break;					//done, exit scan
									}
									t+=10+fs;
									if(fs==0)break;
								}
							}
							myfree(SRAMEX,tagbuf);
						}
					}
				}
			}
			f_close(f);
		}
	}
	myfree(SRAMEX,buf);
	myfree(SRAMEX,f);
	return jpeglen;
}





















