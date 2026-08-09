#ifndef __APP_EEPROM_H
#define __APP_EEPROM_H
#include "sys.h"
#include "24cxx.h"
/************************************************
 ALIENTEK STM32F407探索者开发板 实验24
 IIC实验-HAL库函数版(EEPROM测试应用库)
 功能: 自动检测EEPROM芯片容量(AT24C01~AT24C512),
       全片读写测试(先备份原数据, 测试完成后恢复),
       并实时显示测试进度
 支持: www.openedv.com
************************************************/

/* 全片测试最大容量(AT24C512为64KB) */
#define EEP_MAX_SIZE 65536

extern u16 g_eep_cap;   /* 检测到的芯片容量(最后一个有效地址, 如AT24C02为255) */
extern u8 *g_detect_err;/* 容量检测失败原因(供LCD显示) */

/* 检测EEPROM芯片容量
 * 返回值:0,检测失败(未检测到芯片); 1,检测成功 */
u8 EEP_DetectCap(void);

/* 全片读写测试
 * 过程: 备份原数据到RAM -> 全片写入测试数据 -> 读回校验 -> 恢复原数据
 * 返回值:0,测试通过; 1,测试失败 */
u8 EEP_FullTest(void);

/* 根据容量返回芯片型号名称 */
u8 *EEP_CapName(u16 cap);

#endif
