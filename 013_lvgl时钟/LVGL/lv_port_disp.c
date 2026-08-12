/**
 * @file lv_port_disp.c
 * @brief LVGL显示驱动移植(正点原子探索者STM32F407)
 *
 * 显示屏: ATK-MD0430 (480x800 RGB565), 通过FSMC接口连接,
 * 控制器: NT5510 (NV3041A).
 *
 * 双缓冲 + DMA异步刷屏优化:
 * - LVGL渲染到片内SRAM缓冲buf1的同时, DMA2在后台把buf2的数据搬送到LCD,
 *   渲染与刷屏流水线并行, 隐藏FSMC写入时间, 提升动画帧率.
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"
#include "lcd.h"

/*********************
 *      DEFINES
 *********************/
#define LVGL_DISP_HOR_RES    480
#define LVGL_DISP_VER_RES    800

/* 每个缓冲大小: 480*60像素(1/13屏), 双缓冲放片内SRAM, 访问速度快 */
#define LVGL_DISP_BUF_LINES  60

/* 用于向LCD(FSMC)搬送像素数据的DMA: DMA2 Stream7, 内存到内存(M2M)模式 */
#define LCD_DMA_STREAM       DMA2_Stream7
#define LCD_DMA_TC_FLAG      DMA_FLAG_TCIF7
#define LCD_DMA_TC_IT        DMA_IT_TCIF7
#define LCD_DMA_IRQn         DMA2_Stream7_IRQn

/**********************
 *      VARIABLES
 **********************/
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[LVGL_DISP_HOR_RES * LVGL_DISP_BUF_LINES];  /* 渲染缓冲1 */
static lv_color_t buf2[LVGL_DISP_HOR_RES * LVGL_DISP_BUF_LINES];  /* 渲染缓冲2 */
static lv_disp_drv_t disp_drv;
static volatile u8 dma_transferring = 0;   /* 1=上一次DMA传输未完成 */

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);
static void lcd_dma_init(void);
static void lcd_dma_start(u32 src, u32 pixel_count);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief  初始化LVGL显示驱动(双缓冲+DMA异步刷屏)
 * @note   必须在 lv_init() 和 LCD_Init() 之后调用
 */
void lv_port_disp_init(void)
{
    /* 初始化双缓冲: buf1/buf2 交替渲染, 配合异步刷屏流水线 */
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LVGL_DISP_HOR_RES * LVGL_DISP_BUF_LINES);

    /* 初始化LCD刷屏DMA */
    lcd_dma_init();

    /* 初始化显示驱动 */
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LVGL_DISP_HOR_RES;       /* 水平分辨率 */
    disp_drv.ver_res = LVGL_DISP_VER_RES;       /* 垂直分辨率 */
    disp_drv.flush_cb = disp_flush;             /* 刷屏回调 */
    disp_drv.draw_buf = &draw_buf;

    /* 注册显示驱动 */
    lv_disp_drv_register(&disp_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief  初始化LCD刷屏DMA (内存到内存模式)
 * @note   FSMC总线接口无DMA请求线, 故使用DMA的M2M模式,
 *         软件触发将片内缓冲数据搬运到FSMC映射的LCD数据寄存器地址
 */
static void lcd_dma_init(void)
{
    DMA_InitTypeDef DMA_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);    /* 使能DMA2时钟 */

    DMA_DeInit(LCD_DMA_STREAM);
    DMA_InitStructure.DMA_Channel = DMA_Channel_0;                          /* M2M模式下通道号无关 */
    DMA_InitStructure.DMA_PeripheralBaseAddr = 0;                           /* M2M模式: PAR为源地址, 每次刷屏时设置 */
    DMA_InitStructure.DMA_Memory0BaseAddr = (u32)(&LCD->LCD_RAM);           /* M2M模式: M0AR为目标地址(LCD数据寄存器,固定) */
    DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToMemory;                     /* 内存到内存 */
    DMA_InitStructure.DMA_BufferSize = 0;                                   /* 传输量, 每次刷屏时设置 */
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Enable;         /* 源地址递增 */
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Disable;                /* 目标地址固定 */
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;  /* RGB565=16bit */
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;                           /* 单次传输 */
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(LCD_DMA_STREAM, &DMA_InitStructure);

    DMA_ClearFlag(LCD_DMA_STREAM, LCD_DMA_TC_FLAG);
    DMA_ITConfig(LCD_DMA_STREAM, DMA_IT_TC, ENABLE);                        /* 使能传输完成中断 */

    NVIC_InitStructure.NVIC_IRQChannel = LCD_DMA_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;               /* 抢占优先级2 */
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;                      /* 子优先级2 */
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/**
 * @brief  启动一次LCD DMA传输
 * @param  src: 源数据地址(片内渲染缓冲)
 * @param  pixel_count: 传输的像素个数(16bit/像素)
 */
static void lcd_dma_start(u32 src, u32 pixel_count)
{
    DMA_Cmd(LCD_DMA_STREAM, DISABLE);
    DMA_SetCurrDataCounter(LCD_DMA_STREAM, pixel_count);
    LCD_DMA_STREAM->PAR = src;      /* M2M模式下PAR为源地址, 标准库无独立设置函数, 直接写寄存器 */
    DMA_Cmd(LCD_DMA_STREAM, ENABLE);
}

/**
 * @brief  LVGL刷屏回调: 设置LCD窗口后启动DMA异步搬送
 * @param  disp_drv: 显示驱动
 * @param  area: 需要刷新的区域
 * @param  color_p: 像素数据缓冲(RGB565, 按行排列)
 */
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    u32 w, h;

    /* 等待上一次DMA传输完成(同一DMA通道只能串行传输) */
    while(dma_transferring);

    w = area->x2 - area->x1 + 1;    /* 区域宽度 */
    h = area->y2 - area->y1 + 1;    /* 区域高度 */

    /* 设置LCD窗口并准备写GRAM, 之后GRAM地址按窗口自动递增 */
    LCD_Set_Window(area->x1, area->y1, w, h);
    LCD_WriteRAM_Prepare();

    /* 启动DMA后台搬送, 不在此处调用flush_ready,
     * LVGL可立即渲染下一区域到另一缓冲(双缓冲流水线),
     * 由DMA传输完成中断中调用lv_disp_flush_ready()通知LVGL */
    lcd_dma_start((u32)color_p, w * h);
    dma_transferring = 1;
}

/**
 * @brief  DMA2 Stream7 传输完成中断服务函数
 * @note   在中断中通知LVGL本次刷屏完成, 对应缓冲可被重新使用
 */
void DMA2_Stream7_IRQHandler(void)
{
    if(DMA_GetITStatus(LCD_DMA_STREAM, LCD_DMA_TC_IT))
    {
        DMA_ClearITPendingBit(LCD_DMA_STREAM, LCD_DMA_TC_IT);
        dma_transferring = 0;
        lv_disp_flush_ready(&disp_drv);
    }
}
