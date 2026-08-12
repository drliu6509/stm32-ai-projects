# 014_lvgl音乐播放器 — 基于 LVGL 的音乐播放器

基于正点原子探索者 V2（STM32F407ZGT6）开发板的 **LVGL v8.3.11** 音乐播放器工程。通过 USB OTG FS 主机读取 U 盘中的 MP3 / WAV 文件，**Helix MP3 定点软解码** + **I2S2 + WM8978** 输出音频，触摸屏控制播放。界面为纯 LVGL 图形界面（列表页 + 播放页），支持 MP3 内嵌 **ID3 APIC 封面（JPEG）** 解码显示与**简体中文文件名 / 中文字符显示**。

## 功能特性

- **图形界面**：LVGL v8.3.11，双缓冲 + DMA2_Stream7 刷屏，主循环轮询 `lv_timer_handler`，触摸屏（GT9147）交互
- **文件来源**：USB OTG FS 主机（MSC 类）读取 U 盘（卷 `2:`）根目录下的 MP3 / WAV 文件，FATFS 文件系统管理
- **MP3 解码播放**：Helix 定点 MP3 解码器，支持 16 位单声道 / 双声道，CBR / VBR 编码
- **WAV 播放**：PCM 数据直通 I2S 输出
- **封面显示**：解析 ID3V2 APIC 帧提取内嵌 JPEG 封面，经 TJpgDec 解码为 RGB565 后显示；封面过大时先由 TJpgDec 缩放，再经 LVGL `lv_img_set_zoom` 适配 240x240 显示区域，无封面时显示占位图
- **中文支持**：
  - 中文文件名：FATFS 返回 GBK 编码文件名，经 GBK → UTF-8 转换后用于 LVGL 显示，原始 GBK 名保留用于 `f_open` 打开文件
  - 中文字符：内部 FLASH 16x16 点阵字库（GB2312 共 6763 个汉字），LVGL 自定义字体回调查询，无需外部字库文件
- **播放控制**：列表点击选歌、播放页上一曲 / 播放-暂停 / 下一曲 / 音量减 / 音量加 / 返回列表
- **状态显示**：播放页显示歌曲名、播放进度 / 总时长、音量、封面
- **流畅播放**：播放期间解码循环每 10ms 服务一次 LVGL，界面不使用动画以优先保证音乐解码不卡顿

## 硬件连接（探索者 V2 / F407）

| 外设 | 引脚 |
| --- | --- |
| USB OTG FS（U 盘读取） | PA11 / PA12（D+/D-），PA15（电源控制） |
| I2S2（音频输出） | PB12 / PB13、PC3 / PC6，I2S2ext_SD = PC2 |
| WM8978（音频编解码） | I2C：PB8 / PB9 |
| 触摸屏 | GT9147 电容触摸（I2C） |
| LCD | FSMC 接口（480x800 RGB565，探索者 V2 板载） |
| 外部 SRAM | FSMC Bank1 NE3，1MB（LVGL 堆 + 封面缓冲等共用） |

## 目录结构

```
014_lvgl音乐播放器/
├── APP/                    # 应用层（LVGL 界面 + 播放调度）
│   ├── music_player.c/h    # 文件列表 / 播放调度：扫描 U 盘、封面解码、列表页
│   ├── audioplay.c/h       # 音频播放控制：顺序播放、切歌、暂停、返回
│   └── mp3ui.c/h           # 播放页 UI：进度、音量、封面显示、控制按钮
├── AUDIOCODEC/             # 音频编解码层
│   ├── mp3/
│   │   ├── helix/          # Helix MP3 定点软解码器（bitstream、huffman、imdct 等）
│   │   └── mp3play.c/h     # MP3 播放驱动：帧读取、ID3 解析（含 APIC 封面）、PCM 填充
│   └── wav/wavplay.c/h     # WAV 播放驱动：PCM 直通 I2S
├── CORE/                   # 内核文件：CMSIS 头文件、startup 启动文件
├── FATFS/                  # 文件系统：FatFS 源码 + exfuns 扩展
│   └── option/cc936.c      # GBK 代码页（_CODE_PAGE=936）
├── FWLIB/                  # STM32F4 标准外设库（GPIO、I2S、DMA、FSMC 等）
├── HARDWARE/               # 板级外设驱动
│   ├── FONT/               # 中文显示支撑
│   │   ├── font_lib.c/h    # LVGL 自定义中文字体（内部 FLASH 点阵，二分查找）
│   │   ├── fontlib_bitmap.c# 16x16 点阵字库（GB2312 6763 汉字，内部 FLASH 数组）
│   │   ├── fontlib_unicode_map.h / gbk_unicode_tab.h  # Unicode 映射表
│   │   ├── gbk2utf8.c/h    # GBK → UTF-8 文件名转换
│   ├── I2S/                # I2S 播放驱动（DMA 双缓冲）
│   ├── WM8978/             # WM8978 音频芯片驱动（I2C 配置 + 音量控制）
│   ├── LCD/  TOUCH/        # LCD 显示与触摸屏驱动
│   ├── SRAM/  SDIO/        # 外部 SRAM / SDIO（未作为播放源）
│   └── LED/  KEY/  TIMER/  IIC/
├── LVGL/                   # LVGL v8.3.11 源码 + 移植
│   ├── src/                # LVGL 内核源码
│   ├── lv_conf.h           # LVGL 配置（16bit 色深、512KB 内存 @外部 SRAM）
│   └── lv_port_disp.c/h    # 显示驱动移植（双缓冲 + DMA 刷屏）
│       lv_port_indev.c/h   # 输入设备（触摸）移植
├── MALLOC/                 # 内存管理（内部 / 外部 SRAM 内存池）
├── PICTURE/                # 图片解码
│   └── tjpgd.c/h           # TJpgDec JPEG 解码器（RGB565 输出，支持缩放）
├── SYSTEM/                 # 系统基础驱动：delay、sys、usart
├── USER/                   # 用户工程：main.c、stm32f4xx.h
└── build_uvprojx.ps1       # 一键生成 Keil 工程文件的 PowerShell 脚本
```

## 实现过程

1. **系统初始化**（`USER/main.c`）
   - 配置中断分组、初始化延时、串口（115200）、LED、LCD、外部 SRAM（FSMC NE3，1MB）
   - 初始化触摸屏、WM8978（耳机 / 喇叭音量 40）
   - 初始化内部内存池（`my_mem_init`）、FatFS 扩展（`exfuns_init`）
   - TIM3 定时器（1ms 中断）驱动 LVGL tick 时钟
   - `lv_init` → `lv_port_disp_init`（双缓冲 + DMA 刷屏）→ `lv_port_indev_init`（触摸）
   - 初始化 USB 主机（MSC 类）、注册 U 盘文件系统 `f_mount(fs[2],"2:",1)`
   - 创建音乐播放器界面，进入主循环：`USBH_Process` / `music_player_loop` / `lv_timer_handler`

2. **文件扫描与列表**（`APP/music_player.c`）
   - `music_player_loop()` 轮询检测 U 盘就绪，就绪后遍历 `2:` 根目录，筛选 MP3 / WAV 文件
   - 每个文件保存两份名字：**GBK 原名**（`f_open` 打开用）与 **UTF-8 名**（LVGL 显示用）
   - 通过 `lv_list_add_btn` 将歌曲加入列表，并对列表项内 label 逐个设置中文字体，保证中文正常显示

3. **编码转换**（`HARDWARE/FONT/gbk2utf8.c`）
   - FATFS 配置 `_CODE_PAGE=936`、`_LFN_UNICODE=0`，`f_readdir` 返回 GBK 编码文件名
   - `gbk_str_to_utf8()` 识别 GBK 双字节（0x81 起）汉字 / 全角字符，经 GB2312 → Unicode 查找表转换为 UTF-8
   - 播放打开文件时使用保留的 GBK 原名，避免中文字符打开失败导致跳歌

4. **中文字体**（`HARDWARE/FONT/font_lib.c`）
   - 6763 个 GB2312 汉字的 16x16 点阵以 const 数组存入内部 FLASH（约 1.36MB），无需 SD 卡字库
   - `font_lib_get_font()` 返回 LVGL 自定义字体（line_height=19），`get_glyph_dsc` / `get_glyph_bitmap` 回调通过 `fontlib_unicode_tab` 二分查找字形

5. **封面解码**（`APP/music_player.c` + `PICTURE/tjpgd.c`）
   - `mp3_get_cover()` 解析 ID3V2 APIC 帧，提取内嵌 JPEG 数据
   - TJpgDec（`JD_FORMAT=1`，RGB565 输出）将 JPEG 解码到外部 SRAM 缓冲（320x320x2）
   - 原始封面大于 320x320 时先用 TJpgDec scale 缩小兜底，解码输出按 MCU 块写回缓冲（正确处理缩放后坐标）
   - `mp3ui_set_cover()` 用 `lv_img_set_zoom` 将封面适配 240x240 显示区域；无封面时隐藏图片、显示占位图

6. **播放**（`AUDIOCODEC/` + `APP/audioplay.c`）
   - `audio_play()` 用 GBK 原名打开文件，读取采样率 / 码率等信息
   - MP3 分帧送入 Helix 解码器输出 16 位 PCM；WAV 直接读取 PCM 数据
   - PCM 经 DMA 双缓冲送入 I2S2，由 WM8978 DAC 输出到耳机 / 喇叭
   - 播放期间解码循环每 10ms 调用一次 `lv_timer_handler` 服务 LVGL，保证界面响应与解码流畅

7. **播放页 UI**（`APP/mp3ui.c`）
   - 显示歌曲名、播放进度 / 总时长、封面、音量
   - 触摸按钮：上一曲 / 播放-暂停 / 下一曲 / 音量减 / 音量加 / 返回
   - "返回"按钮置位 `music_exit_play` 标志使 `audio_play` 退出，回到列表页

## 使用说明

1. 在 PowerShell 中运行 `build_uvprojx.ps1` 生成 Keil 工程文件，或用 Keil MDK-ARM 打开生成的 `USER/*.uvprojx`，编译并下载到探索者 V2 开发板
2. 将 MP3 / WAV 文件（支持中文文件名）放入 U 盘根目录
3. 插入 U 盘，开发板自动扫描并在列表界面显示歌曲
4. 触摸点击列表项进入播放页，通过按钮控制播放 / 暂停、切歌、音量

## 注意事项

- 默认播放源为 U 盘（卷 `2:`），需将 MP3 / WAV 放在 U 盘根目录
- FATFS 以 GBK 编码读取文件名（`_CODE_PAGE=936`），仅显示时转换为 UTF-8，打开文件必须使用 GBK 原名
- 中文字库覆盖 GB2312 简体汉字（6763 字），超出范围显示为 `?`，繁体 / 生僻字不支持
- 含中文注释的源码文件（.c/.h/.ps1）需以 UTF-8 BOM 保存，否则编译器按 GBK 解析可能出现乱码或告警
- 外部 SRAM（1MB）被 LVGL 内存池（512KB）与封面解码缓冲等共用，封面解码缓冲限制为 320x320，超大会自动缩放
- 构建产物（OBJ、Listings 等）已通过 `.gitignore` 排除，不纳入版本管理
