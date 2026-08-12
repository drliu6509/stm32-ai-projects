/**
 * @file font_lib.h
 * @brief LVGL 中文字库字体 (字模编译进内部FLASH, 无需SD卡)
 *
 * 字模数据在 fontlib_bitmap.c 中 (const 数组, GB2312 6763 汉字 x 32 字节),
 * 编译进内部 FLASH, 开机即用, 不依赖 SD 卡/文件系统.
 * 通过自定义 lv_font_t 回调让 LVGL 显示任意汉字.
 * ASCII 字符/数字/LVGL符号 由 fallback (montserrat_14) 提供.
 */
#ifndef __FONT_LIB_H__
#define __FONT_LIB_H__

#include "lvgl.h"
#include "sys.h"

/**
 * @brief 获取中文点阵字体(16x16), fallback为montserrat_14
 */
const lv_font_t *font_lib_get_font(void);

#endif
