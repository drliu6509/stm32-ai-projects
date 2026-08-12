/**
 * @file gbk2utf8.h
 * @brief GBK(CP936)字符串转UTF-8 (用于FATFS文件名->LVGL显示)
 */
#ifndef __GBK2UTF8_H__
#define __GBK2UTF8_H__

#include "sys.h"

/**
 * @brief GBK/ASCII字符串转UTF-8
 * @param gbk: 输入字符串(GBK编码)
 * @param utf8: 输出缓冲区(UTF-8编码, 以0结尾)
 * @param maxlen: 输出缓冲区大小(字节)
 * @retval 输出字符串长度(不含结尾0)
 */
u16 gbk_str_to_utf8(const u8 *gbk, u8 *utf8, u16 maxlen);

#endif
