/**
 * @file lv_port_indev.c
 * @brief LVGL input device driver porting for ALIENTEK Explorer STM32F407
 *
 * The touch screen uses the built-in tp_dev driver (capacitive touch).
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"
#include "touch.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      VARIABLES
 **********************/
static lv_indev_drv_t indev_drv;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief  Initialize the LVGL input device driver
 * @note   Must be called after lv_init() and tp_dev.init()
 */
void lv_port_indev_init(void)
{
    /* Initialize the input device driver */
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;     /* Touch pad is a pointer device */
    indev_drv.read_cb = touchpad_read;          /* Read callback */

    /* Register the input device driver */
    lv_indev_drv_register(&indev_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief  LVGL input device read callback: scan the touch screen
 * @param  indev_drv: input device driver
 * @param  data: read data (position and state)
 */
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    /* Scan the touch screen */
    tp_dev.scan(0);

    /* If the screen is touched, report the position and pressed state */
    if(tp_dev.sta & TP_PRES_DOWN) {
        data->point.x = tp_dev.x[0];
        data->point.y = tp_dev.y[0];
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else {
        data->point.x = 0;
        data->point.y = 0;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
