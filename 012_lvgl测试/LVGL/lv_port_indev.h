/**
 * @file lv_port_indev.h
 * @brief LVGL input device driver porting for ALIENTEK Explorer STM32F407
 */

#ifndef LV_PORT_INDEV_H
#define LV_PORT_INDEV_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lvgl.h"

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void lv_port_indev_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_PORT_INDEV_H*/
