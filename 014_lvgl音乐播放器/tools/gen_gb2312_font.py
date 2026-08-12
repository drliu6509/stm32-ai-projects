# -*- coding: utf-8 -*-
"""
生成 GB2312 16x16 点阵中文字库 (SYSTEM.FNT) 及 unicode 映射表
- 覆盖 GB2312 全部 6763 个汉字
- 字模按 unicode 码点升序连续存放, 每字 32 字节 (16行 x 16bit, 行内 MSB 在前)
- 位图格式与 LVGL bpp=1 (lv_font_get_glyph_bitmap) 完全兼容
- 同时生成 fontlib_unicode_map.h (6763 个 unicode 码点表, 用于二分查找)
"""
import struct
from PIL import Image, ImageDraw, ImageFont

FONT_PATH = "C:/Windows/Fonts/simsun.ttc"
SIZE = 16
OUT_FNT = "f:/stm32探索者ai项目/014_lvgl音乐播放器/SD_FILES/SYSTEM.FNT"
OUT_MAP = "f:/stm32探索者ai项目/014_lvgl音乐播放器/HARDWARE/FONT/fontlib_unicode_map.h"

def gb2312_chars():
    """遍历 GB2312 区位码, 返回 (unicode, char) 列表"""
    items = []
    for qh in range(0xB0, 0xF8):          # 区 16~87
        for ql in range(0xA1, 0xFF):      # 位 1~94
            try:
                ch = bytes([qh, ql]).decode("gb2312")
                items.append((ord(ch), ch))
            except UnicodeDecodeError:
                pass
    return items

def main():
    items = gb2312_chars()
    items.sort(key=lambda x: x[0])        # unicode 升序
    print("total chars:", len(items))

    font = ImageFont.truetype(FONT_PATH, SIZE, index=0)
    data = bytearray()
    unicode_list = []

    for code, ch in items:
        img = Image.new("L", (SIZE, SIZE), 255)
        d = ImageDraw.Draw(img)
        # 先测文本包围盒, 再平移使字形在16x16内垂直/水平居中
        bbox = d.textbbox((0, 0), ch, font=font)   # (l, t, r, b)
        w = bbox[2] - bbox[0]
        h = bbox[3] - bbox[1]
        x0 = (SIZE - w) // 2 - bbox[0]
        y0 = (SIZE - h) // 2 - bbox[1]
        d.text((x0, y0), ch, font=font, fill=0)
        # 提取字模
        glyph = bytearray(SIZE * 2)
        for y in range(SIZE):
            for x in range(SIZE):
                if img.getpixel((x, y)) < 128:
                    bi = y * 2 + x // 8
                    bit = 7 - (x % 8)
                    glyph[bi] |= 1 << bit
        data += glyph
        unicode_list.append(code)

    assert len(data) == len(items) * 32, "size mismatch"
    open(OUT_FNT, "wb").write(data)
    print("FNT written:", OUT_FNT, len(data), "bytes")

    # 生成映射表头文件
    lines = ["/* 自动生成, 请勿手工修改 */",
             "#ifndef __FONTLIB_UNICODE_MAP_H__",
             "#define __FONTLIB_UNICODE_MAP_H__",
             "/* GB2312 6763 个汉字 unicode 码点表(升序), 与 SYSTEM.FNT 字模顺序一一对应 */",
             "#define FONTLIB_CHAR_NUM %d" % len(items),
             "static const unsigned short fontlib_unicode_tab[FONTLIB_CHAR_NUM] = {"]
    row = []
    for i, u in enumerate(unicode_list):
        row.append("0x%04X" % u)
        if len(row) == 12:
            lines.append("    " + ", ".join(row) + ",")
            row = []
    if row:
        lines.append("    " + ", ".join(row) + ",")
    lines.append("};")
    lines.append("#endif")
    open(OUT_MAP, "w", encoding="utf-8").write("\n".join(lines) + "\n")
    print("MAP written:", OUT_MAP)

if __name__ == "__main__":
    main()
