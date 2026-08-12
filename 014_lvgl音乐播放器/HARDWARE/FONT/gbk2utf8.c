/**
 * @file gbk2utf8.c
 * @brief GBK(CP936)字符串转UTF-8
 *
 * 背景: FATFS配置为 _LFN_UNICODE=0 + _CODE_PAGE=936,
 * f_readdir 返回的中文文件名是 GBK 编码, 而 LVGL 要求 UTF-8.
 * 本模块通过 gbk_unicode_tab.h 查表, 把 GBK/ASCII 混合字符串转换为 UTF-8.
 */
#include "gbk2utf8.h"
#include "gbk_unicode_tab.h"

/**
 * @brief GBK双字节码(汉字/全角符号区)转Unicode
 * @param hi: 高字节(区, 0xA1~0xF7 为GB2312区; GBK扩展区0x81~0xA0/0xF8~0xFE未建表)
 * @param lo: 低字节(位, 0xA1~0xFE)
 * @retval Unicode码点; 0表示无效码/未建表
 */
static u16 gbk_to_unicode(u8 hi, u8 lo)
{
	if(hi<0xA1||hi>0xF7||lo<0xA1||lo>0xFE)return 0;
	return gbk_unicode_tab[(u16)((hi-0xA1)*GBK_TAB_W+(lo-0xA1))];
}

/**
 * @brief GBK/ASCII字符串转UTF-8
 * @param gbk: 输入字符串(GBK编码, 含中文/全角符号)
 * @param utf8: 输出缓冲区(UTF-8编码, 以0结尾)
 * @param maxlen: 输出缓冲区大小(字节)
 * @retval 输出字符串长度(不含结尾0)
 */
u16 gbk_str_to_utf8(const u8 *gbk, u8 *utf8, u16 maxlen)
{
	u16 o=0;

	while(*gbk&&o<maxlen-3)
	{
		u8 c=*gbk++;
		if(c<0x80)							//ASCII直接复制
		{
			utf8[o++]=c;
		}
		else if(c>=0x81&&*gbk)					//GBK双字节(0x81~0xFE, 汉字/全角/扩展区)
		{
			u16 uni=gbk_to_unicode(c,*gbk++);
			if(uni==0)utf8[o++]='?';		//无效码,用?代替
			else if(uni<0x800)				//2字节UTF-8
			{
				utf8[o++]=(u8)(0xC0|(uni>>6));
				utf8[o++]=(u8)(0x80|(uni&0x3F));
			}
			else							//3字节UTF-8
			{
				utf8[o++]=(u8)(0xE0|(uni>>12));
				utf8[o++]=(u8)(0x80|((uni>>6)&0x3F));
				utf8[o++]=(u8)(0x80|(uni&0x3F));
			}
		}
		else utf8[o++]='?';					//无法识别的字节
	}
	utf8[o]=0;
	return o;
}
