/*-----------------------------------------------------------------------*/
/* Low level disk I/O module for FatFs          (C)ChaN, 2013            */
/*-----------------------------------------------------------------------*/
/* 014_LVGL音乐播放器专用版本                                              */
/* 本工程支持两个存储介质:                                                */
/*   0号盘: SD卡(SDIO接口), 用于存放中文字库SYSTEM.FNT                    */
/*   2号盘: U盘(USB Host MSC), 用于存放音乐文件                           */
/* 已去掉外部FLASH(W25QXX)代码路径.                                       */
/*-----------------------------------------------------------------------*/

#include "diskio.h"		/* FatFs lower layer API */
#include "malloc.h"
#include "usbh_usr.h"
#include "sdio_sdcard.h"

#define SD_DISK   0	//SD卡,盘号为0
#define USB_DISK  2	//U盘,盘号为2

//初始化磁盘
DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber (0..) */
)
{
	if(pdrv==SD_DISK)
	{
		if(SD_Init()==SD_OK)return 0;		//SD卡初始化成功
		else return STA_NOINIT;				//SD卡初始化失败
	}
	else if(pdrv==USB_DISK)
	{
		if(USBH_UDISK_Status())return 0;	//U盘连接成功,返回就绪
		else return STA_NOINIT;				//U盘未连接
	}
	return STA_NOINIT;
}

//获取磁盘状态
DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber (0..) */
)
{
	return 0;
}

//读扇区
DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address (LBA) */
	UINT count		/* Number of sectors to read (1..128) */
)
{
	u8 res=0;
	if(!count)return RES_PARERR;	//count不能等于0,否则返回参数错误
	if(pdrv==SD_DISK)
	{
		res=SD_ReadDisk(buff,sector,count);
	}
	else if(pdrv==USB_DISK)
	{
		res=USBH_UDISK_Read(buff,sector,count);
	}
	else res=1;
	if(res==0x00)return RES_OK;
	else return RES_ERROR;
}

//写扇区
DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber (0..) */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address (LBA) */
	UINT count			/* Number of sectors to write (1..128) */
)
{
	u8 res=0;
	if(!count)return RES_PARERR;	//count不能等于0,否则返回参数错误
	if(pdrv==SD_DISK)
	{
		res=SD_WriteDisk((u8*)buff,sector,count);
	}
	else if(pdrv==USB_DISK)
	{
		res=USBH_UDISK_Write((u8*)buff,sector,count);
	}
	else res=1;
	if(res==0x00)return RES_OK;
	else return RES_ERROR;
}

//其他控制命令
DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res;
	if(pdrv==SD_DISK)
	{
		switch(cmd)
		{
			case CTRL_SYNC:
				res=RES_OK;
				break;
			case GET_SECTOR_SIZE:
				*(WORD*)buff=512;
				res=RES_OK;
				break;
			case GET_BLOCK_SIZE:
				*(WORD*)buff=512;
				res=RES_OK;
				break;
			case GET_SECTOR_COUNT:
				*(DWORD*)buff=SDCardInfo.CardCapacity/512;	//SD卡总扇区数
				res=RES_OK;
				break;
			default:
				res=RES_PARERR;
				break;
		}
	}
	else if(pdrv==USB_DISK)
	{
		switch(cmd)
		{
			case CTRL_SYNC:
				res=RES_OK;
				break;
			case GET_SECTOR_SIZE:
				*(WORD*)buff=512;
				res=RES_OK;
				break;
			case GET_BLOCK_SIZE:
				*(WORD*)buff=512;
				res=RES_OK;
				break;
			case GET_SECTOR_COUNT:
				*(DWORD*)buff=USBH_MSC_Param.MSCapacity;
				res=RES_OK;
				break;
			default:
				res=RES_PARERR;
				break;
		}
	}
	else res=RES_ERROR;	//不支持的磁盘
	return res;
}

//获取当前时间(FATFS文件时间戳,本工程不关注)
DWORD get_fattime (void)
{
	return 0;
}

//动态分配内存(FATFS长文件名等使用)
void *ff_memalloc (UINT size)
{
	return (void*)mymalloc(SRAMIN,size);
}

//释放内存
void ff_memfree (void* mf)
{
	myfree(SRAMIN,mf);
}
