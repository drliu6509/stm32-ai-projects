#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "lcd.h"
#include "24cxx.h"
#include "app_eeprom.h"
/************************************************
 ALIENTEK STM32F407探索者开发板 实验24
 IIC实验-HAL库函数版(EEPROM测试应用库)
 功能: 自动检测EEPROM芯片容量(AT24C01~AT24C512),
       全片读写测试(先备份原数据, 测试完成后恢复),
       并实时显示测试进度
 支持: www.openedv.com
************************************************/

/* 检测到的芯片容量(最后一个有效地址, 如AT24C02为255) */
u16 g_eep_cap;
/* 容量检测失败原因(供LCD显示) */
u8 *g_detect_err = (u8 *)"";

/* 1:使用2字节地址寻址(容量>AT24C16); 0:设备地址寻址 */
static u8 g_eep_2byte;
/* 全片数据备份缓冲 */
static u8 g_eep_bak[EEP_MAX_SIZE];

/* 按指定寻址模式读一个字节
 * mode:0,单字节地址(<=AT24C16); 1,双字节地址(>AT24C16)
 * addr:字节地址
 * 返回值:读到的数据(通信失败返回0XFF) */
static u8 EEP_RW_Read(u8 mode, u16 addr)
{
    u8 dat = 0XFF;

    IIC_Start();
    if(mode)                            /* 2字节地址模式 */
    {
        IIC_Send_Byte(0XA0);
        if(IIC_Wait_Ack()) return 0XFF;
        IIC_Send_Byte(addr >> 8);
        if(IIC_Wait_Ack()) return 0XFF;
    }
    else                                /* 设备地址寻址模式 */
    {
        IIC_Send_Byte(0XA0 + ((addr / 256) << 1));
        if(IIC_Wait_Ack()) return 0XFF;
    }
    IIC_Send_Byte(addr & 0XFF);
    if(IIC_Wait_Ack()) return 0XFF;
    IIC_Start();
    if(mode)
    {
        IIC_Send_Byte(0XA1);
        if(IIC_Wait_Ack()) return 0XFF;
    }
    else
    {
        IIC_Send_Byte(0XA1 + ((addr / 256) << 1));
        if(IIC_Wait_Ack()) return 0XFF;
    }
    dat = IIC_Read_Byte(0);
    IIC_Stop();
    return dat;
}

/* 按指定寻址模式写一个字节
 * mode:0,单字节地址(<=AT24C16); 1,双字节地址(>AT24C16)
 * addr:字节地址; dat:要写入的数据
 * 返回值:0,写入成功; 1,写入失败(无应答) */
static u8 EEP_RW_Write(u8 mode, u16 addr, u8 dat)
{
    u8 ret;

    IIC_Start();
    if(mode)                            /* 2字节地址模式 */
    {
        IIC_Send_Byte(0XA0);
        if(IIC_Wait_Ack()) return 1;
        IIC_Send_Byte(addr >> 8);
        if(IIC_Wait_Ack()) return 1;
    }
    else                                /* 设备地址寻址模式 */
    {
        IIC_Send_Byte(0XA0 + ((addr / 256) << 1));
        if(IIC_Wait_Ack()) return 1;
    }
    IIC_Send_Byte(addr & 0XFF);
    if(IIC_Wait_Ack()) return 1;
    IIC_Send_Byte(dat);
    ret = IIC_Wait_Ack();
    IIC_Stop();
    delay_ms(10);                       /* 等待写周期结束 */
    return ret;
}

/* 按当前寻址模式读一个字节 */
static u8 EEP_ReadByte(u16 addr) { return EEP_RW_Read(g_eep_2byte, addr); }

/* 获取当前芯片的页大小(字节)
 * 页写时不能跨页, 按页大小对齐写入 */
static u16 EEP_PageSize(void)
{
    if(g_eep_cap <= AT24C02) return 8;      /* 24C01/02: 8字节页 */
    if(g_eep_cap <= AT24C16) return 16;     /* 24C04/08/16: 16字节页 */
    if(g_eep_cap <= AT24C64) return 32;     /* 24C32/64: 32字节页 */
    if(g_eep_cap <= AT24C256) return 64;    /* 24C128/256: 64字节页 */
    return 128;                             /* 24C512: 128字节页 */
}

/* 按指定寻址模式连续写一页数据(不跨页)
 * mode:0,单字节地址; 1,双字节地址
 * addr:起始地址; buf:数据缓冲; len:字节数(不得超过页边界)
 * 一次IIC事务写入, 写周期仅需等待一次 */
static void EEP_WritePage(u8 mode, u16 addr, u8 *buf, u16 len)
{
    IIC_Start();
    if(mode)
    {
        IIC_Send_Byte(0XA0);
        IIC_Wait_Ack();
        IIC_Send_Byte(addr >> 8);
        IIC_Wait_Ack();
    }
    else
    {
        IIC_Send_Byte(0XA0 + ((addr / 256) << 1));
        IIC_Wait_Ack();
    }
    IIC_Send_Byte(addr & 0XFF);
    IIC_Wait_Ack();
    while(len--)
    {
        IIC_Send_Byte(*buf++);
        IIC_Wait_Ack();
    }
    IIC_Stop();
    delay_ms(10);                           /* 等待写周期结束 */
}

/* 显示进度百分比
 * x,y:显示坐标; cur:当前值; total:总数 */
static void EEP_ShowProgress(u16 x, u16 y, u32 cur, u32 total)
{
    LCD_ShowxNum(x, y, cur * 100 / total, 3, 16, 0X80);
}

/* 检测EEPROM芯片容量
 * 原理: 依次在128/256/512/1024/2048/4096...边界地址写测试数据,
 *       若回绕(写边界地址却改变了地址0)则说明到达芯片容量边界;
 *       边界地址无应答时, 再用2字节地址模式区分小芯片与大芯片
 * 返回值:0,检测失败(未检测到芯片); 1,检测成功 */
u8 EEP_DetectCap(void)
{
    u8 o0, o1, ob, cur0;
    u8 wret, rdat;
    u32 b;
    u16 cap = 0;        /* 暂定容量 */
    u8 bigcheck = 0;    /* 需要做大芯片检测标志 */

    g_eep_2byte = 0;    /* 先按单字节地址模式检测 */

    /* 芯片存在性测试: 读写地址0 */
    o0 = EEP_RW_Read(0, 0);                 /* 保存地址0原值 */
    wret = EEP_RW_Write(0, 0, 0XAA);        /* 写入测试值, 返回0=有应答 */
    rdat = EEP_RW_Read(0, 0);               /* 读回校验 */
    printf("[EEP] Exist: wr=%d rd=%02X\r\n", wret, rdat);
    if(wret)                                /* 写无应答 -> 芯片不存在 */
    {
        EEP_RW_Write(0, 0, o0);
        g_detect_err = (u8 *)"Err: NO ACK!";    /* 0XA0无应答 */
        printf("[EEP] NO ACK at 0XA0, chip absent?\r\n");
        return 0;
    }
    if(rdat != 0XAA)                        /* 读回不正确 -> 芯片写保护或不存在 */
    {
        EEP_RW_Write(0, 0, o0);
        g_detect_err = (u8 *)"Err: WP/HW!";     /* 写入不生效 */
        printf("[EEP] Write verify fail, WP or bad chip!\r\n");
        return 0;
    }

    /* 边界128检测: 区分AT24C01与更大容量芯片 */
    ob = EEP_RW_Read(0, 128);
    EEP_RW_Write(0, 0, 0XAA);
    wret = EEP_RW_Write(0, 128, 0X55);
    rdat = EEP_RW_Read(0, 0);
    printf("[EEP] B128: wr=%d rd0=%02X\r\n", wret, rdat);
    if(wret == 0 && rdat == 0X55) cap = AT24C01;
    if(cap == 0) EEP_RW_Write(0, 128, ob);  /* 未回绕, 恢复边界原值 */

    /* 边界256检测: 区分AT24C02与更大容量芯片 */
    if(cap == 0)
    {
        ob = EEP_RW_Read(0, 256);
        EEP_RW_Write(0, 0, 0XAA);
        wret = EEP_RW_Write(0, 256, 0X55);
        rdat = EEP_RW_Read(0, 0);
        printf("[EEP] B256: wr=%d rd0=%02X\r\n", wret, rdat);
        if(wret) { cap = AT24C02; bigcheck = 1; }      /* 无应答: 02或大芯片 */
        else if(rdat == 0X55) cap = AT24C02;           /* 写256回绕 -> 02 */
        else EEP_RW_Write(0, 256, ob);                 /* 未回绕, 恢复, 继续 */
    }
    /* 边界512检测: 区分AT24C04与更大容量芯片 */
    if(cap == 0)
    {
        ob = EEP_RW_Read(0, 512);
        EEP_RW_Write(0, 0, 0XAA);
        wret = EEP_RW_Write(0, 512, 0X55);
        rdat = EEP_RW_Read(0, 0);
        printf("[EEP] B512: wr=%d rd0=%02X\r\n", wret, rdat);
        if(wret) { cap = AT24C04; bigcheck = 1; }
        else if(rdat == 0X55) cap = AT24C04;
        else EEP_RW_Write(0, 512, ob);
    }
    /* 边界1024检测: 区分AT24C08与更大容量芯片 */
    if(cap == 0)
    {
        ob = EEP_RW_Read(0, 1024);
        EEP_RW_Write(0, 0, 0XAA);
        wret = EEP_RW_Write(0, 1024, 0X55);
        rdat = EEP_RW_Read(0, 0);
        printf("[EEP] B1024: wr=%d rd0=%02X\r\n", wret, rdat);
        if(wret) { cap = AT24C08; bigcheck = 1; }
        else if(rdat == 0X55) cap = AT24C08;
        else EEP_RW_Write(0, 1024, ob);
    }
    /* 边界2048检测: 区分AT24C16与更大容量芯片 */
    if(cap == 0)
    {
        ob = EEP_RW_Read(0, 2048);
        EEP_RW_Write(0, 0, 0XAA);
        wret = EEP_RW_Write(0, 2048, 0X55);
        rdat = EEP_RW_Read(0, 0);
        printf("[EEP] B2048: wr=%d rd0=%02X\r\n", wret, rdat);
        if(wret) { cap = AT24C16; bigcheck = 1; }     /* 无应答: 16或大芯片 */
        else if(rdat == 0X55) cap = AT24C16;
        else EEP_RW_Write(0, 2048, ob);
    }

    /* 大芯片检测: 用2字节地址模式写地址1作为探针
     * 注意: 1字节地址的小芯片(<=24C16)会把2字节地址中的低字节(0x01)
     *       当作数据写入地址0, 从而破坏地址0;
     *       大芯片只写地址1, 地址0不变 -> 据此区分 */
    if(bigcheck)
    {
        cur0 = EEP_RW_Read(0, 0);           /* 保存当前地址0值(应仍为0XAA) */
        o1 = EEP_RW_Read(1, 1);             /* 2字节读地址1: 大芯片取真值; 小芯片会顺带破坏地址0 */
        EEP_RW_Write(1, 1, 0X77);           /* 2字节模式写地址1(探针) */
        printf("[EEP] BigChk: wr1=0x77\r\n");
        if(EEP_RW_Read(0, 0) == cur0)       /* 地址0未被破坏 -> 真正的大芯片 */
        {
            EEP_RW_Write(1, 1, o1);         /* 恢复地址1原值 */
            g_eep_2byte = 1;                /* 切换为2字节地址模式 */
            cap = 0;
            /* 大芯片容量检测: 边界4096/8192/16384/32768回绕测试
             * b用u32避免溢出; 若32768仍不回绕, 则为64KB(24C512) */
            for(b = 4096; b <= 32768; b <<= 1)
            {
                ob = EEP_RW_Read(1, b);
                EEP_RW_Write(1, 0, 0XAA);
                wret = EEP_RW_Write(1, b, 0X55);
                rdat = EEP_RW_Read(1, 0);
                printf("[EEP] Big B%d: wr=%d rd0=%02X\r\n", b, wret, rdat);
                if(wret == 0)
                {
                    if(rdat == 0X55) { cap = b - 1; break; }    /* 回绕 -> 容量为b */
                    EEP_RW_Write(1, b, ob);                     /* 未回绕, 恢复 */
                }
                else break;
            }
            if(cap == 0) cap = AT24C512;    /* 32768处仍不回绕 -> 64KB大芯片 */
        }
        else
        {
            /* 地址0被破坏 -> 小芯片(<=24C16), 恢复被写坏的地址0/1 */
            EEP_RW_Write(0, 0, cur0);
            EEP_RW_Write(0, 1, o1);
        }
    }

    EEP_RW_Write(g_eep_2byte, 0, o0);       /* 恢复地址0原值 */
    g_eep_cap = cap;
    g_detect_err = (u8 *)"";                /* 检测成功, 清除错误信息 */
    printf("[EEP] Detect OK, cap=%d\r\n", g_eep_cap);
    return 1;
}

/* 根据容量返回芯片型号名称 */
u8 *EEP_CapName(u16 cap)
{
    if(cap <= AT24C01) return (u8 *)"24C01";
    if(cap <= AT24C02) return (u8 *)"24C02";
    if(cap <= AT24C04) return (u8 *)"24C04";
    if(cap <= AT24C08) return (u8 *)"24C08";
    if(cap <= AT24C16) return (u8 *)"24C16";
    if(cap <= AT24C32) return (u8 *)"24C32";
    if(cap <= AT24C64) return (u8 *)"24C64";
    if(cap <= AT24C128) return (u8 *)"24C128";
    if(cap <= AT24C256) return (u8 *)"24C256";
    return (u8 *)"24C512";
}

/* 全片读写测试
 * 过程: 备份原数据到RAM -> 全片写入测试数据 -> 读回校验 -> 恢复原数据
 * 写入/恢复按页写入以加快速度(24C512为64KB, 逐字节写需约20分钟)
 * 返回值:0,测试通过; 1,测试失败 */
u8 EEP_FullTest(void)
{
    u32 size = g_eep_cap + 1;   /* 注意不能用u16: 24C512的65536会溢出为0 */
    u32 i;
    u16 pgsz;
    u8 page[128];               /* 页写缓冲(最大128字节) */
    u8 err = 0;

    LCD_Fill(0, 145, 239, 319, WHITE);      /* 清空结果显示区 */

    /* 阶段1: 备份原数据到RAM */
    POINT_COLOR = BLUE;
    LCD_ShowString(20, 150, 90, 16, 16, (u8 *)"Save Data:");
    for(i = 0; i < size; i++)
    {
        g_eep_bak[i] = EEP_ReadByte(i);
        if((i & 0XFF) == 0) EEP_ShowProgress(112, 150, i, size);
    }
    EEP_ShowProgress(112, 150, size, size);

    /* 阶段2: 全片写入测试数据(按页写入, 加快速度) */
    pgsz = EEP_PageSize();
    LCD_ShowString(20, 172, 90, 16, 16, (u8 *)"Write Test:");
    for(i = 0; i < size; i += pgsz)
    {
        u16 j, n = pgsz;
        if(i + n > size) n = size - i;      /* 最后一页可能不满 */
        for(j = 0; j < n; j++) page[j] = (u8)(0X55 ^ ((i + j) & 0XFF));
        EEP_WritePage(g_eep_2byte, (u16)i, page, n);
        EEP_ShowProgress(112, 172, i, size);
    }
    EEP_ShowProgress(112, 172, size, size);

    /* 阶段3: 读回校验 */
    LCD_ShowString(20, 194, 90, 16, 16, (u8 *)"Verify Test:");
    for(i = 0; i < size; i++)
    {
        if(EEP_ReadByte(i) != (u8)(0X55 ^ (i & 0XFF)))
        {
            err = 1;                        /* 校验出错, 记录失败地址 */
            break;
        }
        if((i & 0XFF) == 0) EEP_ShowProgress(112, 194, i, size);
    }
    if(err == 0) EEP_ShowProgress(112, 194, size, size);
    else EEP_ShowProgress(112, 194, i, size);

    /* 阶段4: 恢复原数据(按页写入) */
    LCD_ShowString(20, 216, 90, 16, 16, (u8 *)"Restore:");
    for(i = 0; i < size; i += pgsz)
    {
        u16 j, n = pgsz;
        if(i + n > size) n = size - i;
        for(j = 0; j < n; j++) page[j] = g_eep_bak[i + j];
        EEP_WritePage(g_eep_2byte, (u16)i, page, n);
        EEP_ShowProgress(112, 216, i, size);
    }
    EEP_ShowProgress(112, 216, size, size);

    /* 显示测试结果 */
    if(err)
    {
        POINT_COLOR = RED;
        LCD_ShowString(20, 250, 100, 16, 16, (u8 *)"Result: FAIL!");
        LCD_ShowString(20, 270, 100, 16, 16, (u8 *)"Addr: ");
        LCD_ShowNum(80, 270, i, 5, 16);
    }
    else
    {
        POINT_COLOR = GREEN;
        LCD_ShowString(20, 250, 100, 16, 16, (u8 *)"Result: PASS!");
    }
    POINT_COLOR = BLUE;
    return err;
}
