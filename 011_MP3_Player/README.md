# 011_MP3_Player — MP3 播放器实验

基于正点原子探索者 V2（STM32F407ZGT6）开发板的 MP3 音乐播放器工程，从 U 盘读取 MP3 文件，经 **Helix MP3 软解码** + **I2S2 + WM8978** 输出音频，通过触摸屏（或按键）控制播放。

本工程参考正点原子综合测试实验（实验59）的 MP3 播放部分，**去除了 uCOS/emWin GUI 依赖**，改为自绘 UI（LCD 画图 + 触摸按钮）的裸机实现。

## 功能特性

- **MP3 解码播放**：移植 Helix 定点 MP3 解码器，支持 16 位单声道 / 双声道 MP3，支持 CBR / VBR 编码（最高 320Kbps）
- **文件来源**：通过 USB OTG FS 主机（MSC 类）读取 U 盘根目录下的 MP3 文件，FATFS 文件系统管理
- **标签解析**：支持 ID3V1 / ID3V2 标签，解析歌曲名（Title）与艺术家（Artist）并显示在 LCD 上
- **播放控制**：
  - 触摸屏按钮：上一曲 / 播放-暂停 / 下一曲 / 音量减 / 音量加
  - 顺序播放模式下自动切歌，支持播放进度与总时长显示
- **状态显示**：LCD 显示歌曲名、艺术家、当前时间 / 总时间、U 盘容量、音量等
- **软解码 + 双缓冲 DMA**：PCM 数据经 DMA 双缓冲送入 I2S，无卡顿播放

## 硬件连接（探索者 V2 / F407）

| 外设 | 引脚 |
| --- | --- |
| USB OTG FS（U 盘读取） | PA11 / PA12（D+/D-），PA15（电源控制） |
| I2S2（音频输出） | PB12 / PB13、PC3 / PC6，I2S2ext_SD = PC2 |
| WM8978（音频编解码） | I2C：PB8 / PB9 |
| 触摸屏 | GT9147 / FT5206 电容触摸（I2C） |
| LCD | FSMC 接口（探索者 V2 板载） |
| SD 卡 | SDIO 接口（本工程中挂载但默认从 U 盘播放） |
| 外部 Flash | W25Q128（SPI，挂载文件系统） |

## 目录结构

```
011_MP3_Player/
├── APP/                  # 应用层（自绘 UI + 播放控制，裸机实现）
│   ├── mp3ui.c/h         # MP3 播放界面：LCD 画图 + 触摸按钮 + 状态刷新
│   └── audioplay.c/h     # 音频播放控制：扫描 U 盘 MP3、顺序播放、切歌、暂停
├── AUDIOCODEC/           # 音频编解码层
│   └── mp3/
│       ├── helix/        # Helix MP3 定点软解码器（bitstream、huffman、imdct、polyphase 等）
│       └── mp3play.c/h   # MP3 播放驱动：帧读取、ID3 解析、PCM 填充 I2S 缓冲
├── CORE/                 # 内核文件：CMSIS 头文件、startup_stm32f40_41xxx.s
├── FATFS/                # 文件系统：FatFS 源码 + exfuns 扩展（文件类型判断、编码转换）
├── FWLIB/                # STM32F4 标准外设库（GPIO、I2S、DMA、USART 等）
├── HARDWARE/             # 板级外设驱动
│   ├── WM8978/           # WM8978 音频芯片驱动（I2C 配置 + 音量控制）
│   ├── I2S/              # I2S 播放驱动（DMA 双缓冲）
│   ├── LCD/  TOUCH/      # LCD 显示与触摸屏驱动
│   ├── SDIO/  SPI/  W25QXX/  24CXX/
│   └── LED/  KEY/  SRAM/
├── MALLOC/               # 内存管理（SRAMIN 内部内存池）
├── SYSTEM/               # 系统基础驱动：delay、sys、usart
├── USB/                  # USB 主机协议栈（OTG 驱动 + MSC/HID 类）
└── USER/                 # 用户工程：main.c、Keil 工程文件（MP3_Player.uvprojx）
```

## 实现过程

1. **系统初始化**（`USER/main.c`）
   - 配置中断分组、初始化延时、串口（115200）、LED、按键、LCD
   - 初始化 W25Q128、WM8978（设置耳机 40 / 喇叭 40 音量）
   - 初始化内部内存池（`my_mem_init`）供 FatFS 与解码器动态分配使用
   - 挂载三个文件系统卷：SD 卡 `0:`、外部 Flash `1:`、U 盘 `2:`
   - 初始化触摸屏，进入 MP3 播放界面（`mp3ui_init`），启动 USB 主机枚举

2. **文件扫描**（`APP/audioplay.c`）
   - `audio_play()` 先等待 U 盘就绪（未检测到则 LCD 提示 "No USB Disk!"）
   - 通过 `audio_get_tnum()` 遍历 U 盘根目录，统计有效 MP3 文件数并建立文件索引表
   - 无 MP3 时提示 "No MP3 Files!"，并持续轮询 USB 主机事件等待插入

3. **MP3 解码播放**（`AUDIOCODEC/mp3/mp3play.c` + `helix/`）
   - `mp3_play_song()` 打开文件，读取并解析 ID3V1 / ID3V2 标签，获取歌名、歌手与音频数据起始偏移
   - 从 `mp3_get_info()` 获取采样率、码率、总时长等信息
   - 分帧读取 MP3 数据，调用 Helix 解码器解码出 16 位 PCM 数据
   - 通过 `mp3_fill_buffer()` 将 PCM 写入 I2S 双缓冲（`i2sbuf1 / i2sbuf2`），由 DMA 中断回调（`mp3_i2s_dma_tx_callback`）交替填充，实现无中断连续播放；暂停时向缓冲区填零静音

4. **音频输出**（`HARDWARE/I2S` + `HARDWARE/WM8978`）
   - WM8978 通过 I2C（PB8/PB9）配置内部 DAC 通路，关闭输入通道，使能耳机 / 喇叭输出
   - I2S2 以 DMA 方式将 PCM 数据送入 WM8978 DAC，输出到耳机或板载喇叭

5. **交互界面**（`APP/mp3ui.c`）
   - LCD 绘制播放界面（歌曲信息、进度条、音量、操作按钮）
   - 轮询触摸屏坐标与按键，识别上一曲 / 播放-暂停 / 下一曲 / 音量减 / 音量加操作
   - 播放过程中定时刷新当前时间与进度显示

## 使用说明

1. 用 Keil MDK-ARM 打开 `USER/MP3_Player.uvprojx`，编译并下载到探索者 V2 开发板
2. 将 MP3 文件（支持 ID3 标签的歌曲名显示）放入 U 盘根目录
3. 插入 U 盘，开发板自动扫描并开始顺序播放
4. 通过触摸屏按钮控制播放 / 暂停、切歌、音量

## 注意事项

- 默认播放源为 U 盘（卷 `2:`）；SD 卡与外部 Flash 已挂载但未作为播放源
- 文件系统卷与编码转换依赖 `FATFS/exfuns`（支持 GBK/Unicode 长文件名）
- Keil 构建产物（OBJ、Listings 等）不纳入版本管理
