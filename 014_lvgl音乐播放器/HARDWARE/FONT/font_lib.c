/**
 * @file font_lib.c
 * @brief LVGL 中文字库字体实现
 *
 * 字库方案: GB2312 6763 个汉字的 16x16 点阵字模编译进内部 FLASH
 * (fontlib_bitmap.c, const 数组), 运行时直接查表显示, 不依赖SD卡/文件系统.
 *
 * 字模格式: 每字 32 字节 (16 行 x 2 字节, 行内 MSB 在前),
 * 顺序与 fontlib_unicode_map.h 的 unicode 码点表(升序)一一对应,
 * 与 LVGL bpp=1 位图格式完全一致.
 *
 * 实现方式参考 lv_freetype: 自定义 lv_font_t 的 get_glyph_dsc /
 * get_glyph_bitmap 回调, 非汉字返回 false 交给 fallback(montserrat_14) 显示.
 */
#include "font_lib.h"
#include "fontlib_unicode_map.h"

extern const u8 fontlib_bitmap_tab[FONTLIB_CHAR_NUM * 32];	//字模数据(内部FLASH)

/**
 * @brief 在unicode映射表(升序)中二分查找汉字, 返回字模索引
 * @param letter: 汉字unicode码点(0x4E00~0x9FFF)
 * @retval 命中返回索引(0~6762), 未命中返回0XFFFF
 */
static u16 font_lib_lookup(u16 letter)
{
	u16 lo = 0, hi = FONTLIB_CHAR_NUM;
	while(lo < hi)
	{
		u16 mid = (lo + hi) >> 1;
		if(fontlib_unicode_tab[mid] == letter) return mid;		//命中
		else if(fontlib_unicode_tab[mid] < letter) lo = mid + 1;
		else hi = mid;
	}
	return 0XFFFF;												//未命中
}

/**
 * @brief 获取字形描述回调: 查unicode映射表, 命中则返回16x16字形
 * @retval true:命中, 字形信息填入dsc; false:未命中, 交给fallback
 */
static bool font_lib_get_glyph_dsc(const lv_font_t *font, lv_font_glyph_dsc_t *dsc,
                                   uint32_t letter, uint32_t letter_next)
{
	u16 idx;

	if(letter < 0x4E00 || letter > 0x9FFF) return false;	//非CJK统一汉字,交给fallback

	idx = font_lib_lookup((u16)letter);
	if(idx == 0XFFFF) return false;							//字库中无此字,交给fallback

	dsc->adv_w = 16;					//全角宽度
	dsc->box_w = 16;
	dsc->box_h = 16;
	dsc->ofs_x = 0;
	dsc->ofs_y = 0;
	dsc->bpp = 1;						//单色点阵,与LVGL bpp=1兼容
	dsc->is_placeholder = 0;
	return true;
}

/**
 * @brief 获取字形位图回调: 返回32字节16x16点阵(直接指向内部FLASH)
 */
static const uint8_t *font_lib_get_glyph_bitmap(const lv_font_t *font, uint32_t letter)
{
	u16 idx;

	if(letter < 0x4E00 || letter > 0x9FFF) return NULL;

	idx = font_lib_lookup((u16)letter);
	if(idx == 0XFFFF) return NULL;
	return (const uint8_t *)&fontlib_bitmap_tab[idx * 32];
}

/**
 * @brief 获取中文点阵字体, fallback为montserrat_14(ASCII/数字/LVGL符号)
 */
const lv_font_t *font_lib_get_font(void)
{
	static lv_font_t font_cn = {0};

	font_cn.get_glyph_dsc = font_lib_get_glyph_dsc;
	font_cn.get_glyph_bitmap = font_lib_get_glyph_bitmap;
	font_cn.line_height = 19;		//与正点原子lv_font_simsun_16_cjk一致
	font_cn.base_line = 3;			//基线距行底3px(16x16汉字底边落在基线上)
	font_cn.subpx = LV_FONT_SUBPX_NONE;
	font_cn.fallback = &lv_font_montserrat_14;		//ASCII/数字/LVGL符号
	font_cn.user_data = NULL;
	return &font_cn;
}
