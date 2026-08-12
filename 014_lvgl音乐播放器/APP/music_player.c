#include "music_player.h"
#include "audioplay.h"
#include "font_lib.h"
#include "mp3ui.h"
#include "mp3play.h"
#include "tjpgd.h"
#include "exfuns.h"
#include "gbk2utf8.h"
#include "malloc.h"
#include "ff.h"
#include "string.h"
#include "usbh_usr.h"
#include "usart.h"
#include "delay.h"
//////////////////////////////////////////////////////////////////////////////////
//	本文件仅供学习使用，未经允许，不得用于任何用途
//ALIENTEK STM32F407探索者开发板
//APP-基于LVGL的音乐播放器 文件列表/播放调度
//版权: 正点原子@ALIENTEK
//论坛: www.openedv.com
//修改日期:2026/8/11
//版本:V1.0
//版权所有，盗版必究
//Copyright(C) 正点原子@ALIENTEK 2014-2024
//All rights reserved
//////////////////////////////////////////////////////////////////////////////////
//说明
//1, 扫描U盘("2:")根目录的MP3/WAV文件, 存入music_files表并显示在列表界面
//2, 点击列表项->切换到播放界面->audio_play()阻塞播放
//3, 播放前解析MP3内嵌ID3 APIC封面(JPEG), 使用TJpgDec(JD_FORMAT=1)解码为
//   RGB565数据, 交给mp3ui_set_cover()显示, 无封面时显示占位图
//4, 播放界面"返回"按钮: 清解码循环位+music_exit_play=1, audio_play退出回到列表
//为了优先保证音乐解码, 界面不使用动画, 播放期间LVGL由mp3ui_play_ctrl每10ms服务一次.
//////////////////////////////////////////////////////////////////////////////////

//TJpgDec封面解码缓冲区(SRAMEX)
#define COVER_JPG_SIZE   (120*1024)   //JPEG数据缓冲(提取自ID3 APIC)
#define COVER_RGB_SIZE   (320*320*2)  //RGB565输出缓冲(封面最大320x320, 超过则TJpgDec缩小)
#define COVER_POOL_SIZE  (12*1024)    //TJpgDec工作区

//曲目表(SRAMEX)
music_file_t *music_files=NULL;
u16 music_file_num=0;
u8  music_exit_play=0;

//列表界面控件
static lv_obj_t *list_scr;      //列表屏幕
static lv_obj_t *music_list;    //列表控件
static lv_obj_t *tip_label;     //提示消息(未插U盘/无文件)

//播放状态
static u8 mp_pending=0;         //有待播放请求
static u16 mp_pending_index=0;  //待播放索引
static u8 mp_playing=0;         //正在播放

//封面解码缓冲(SRAMEX)
static u8 *cover_jpgbuf=NULL;
static u8 *cover_rgbbuf=NULL;
static u8 *cover_pool=NULL;
static u16 cover_out_w=0;
static u16 cover_out_h=0;

//JPEG数据流(供TJpgDec输入回调使用)
typedef struct
{
    u8 *data;
    u32 len;
    u32 pos;
} jpeg_stream_t;

//USB主机结构体(定义于main.c,用于扫描时驱动USBH_Process完成枚举)
extern USBH_HOST USB_Host;
extern USB_OTG_CORE_HANDLE USB_OTG_Core;

//列表项点击事件
//e:LVGL事件指针
static void music_player_list_click(lv_event_t *e)
{
    u16 index=(u16)(u32)lv_event_get_user_data(e);  //Cortex-M4指针32位,直接转换
    if(index<music_file_num)
    {
        music_player_play_index(index);
    }
}

//返回列表回调(播放界面"返回"按钮触发)
static void music_player_back_cb(void)
{
    music_exit_play=1;          //请求退出当前解码循环
    lv_scr_load(list_scr);      //切换到列表界面
}

//TJpgDec输入回调: 从内存JPEG数据读取
//jd:解码器对象
//buf:输出缓冲
//ndata:请求读取字节数
//返回:实际读取字节数
static size_t cover_input(JDEC *jd,uint8_t *buf,size_t ndata)
{
    jpeg_stream_t *st=(jpeg_stream_t*)jd->device;
    size_t n;
    if(st->pos>=st->len)return 0;               //数据读完
    n=st->len-st->pos;
    if(n>ndata)n=ndata;
    memcpy(buf,st->data+st->pos,n);
    st->pos+=n;
    return n;
}

//TJpgDec输出回调: 把MCU行写入RGB565缓冲
//jd:解码器对象
//bitmap:当前MCU行RGB数据
//rect:当前输出区域(缩放后坐标, 需带left/top偏移写入)
//返回:1,继续解码
static int cover_output(JDEC *jd,void *bitmap,JRECT *rect)
{
    u8 *src=(u8*)bitmap;
    u16 roww=rect->right-rect->left+1;
    u32 row;
    for(row=rect->top;row<=rect->bottom;row++)
    {
        memcpy(cover_rgbbuf+((u32)row*cover_out_w+rect->left)*2,src,(u32)roww*2);
        src+=(u32)roww*2;
    }
    return 1;
}

//显示MP3内嵌封面
//fname:音乐文件完整路径
//返回:1,显示成功;0,无封面/解码失败
u8 music_player_show_cover(u8 *fname)
{
    JDEC jd;
    JRESULT jr;
    jpeg_stream_t stream;
    u32 jpglen;
    u8 scale=0;
    u8 ret=0;

    if(!cover_jpgbuf||!cover_rgbbuf||!cover_pool)return 0;      //缓冲未分配
    jpglen=mp3_get_cover(fname,cover_jpgbuf,COVER_JPG_SIZE);    //提取ID3内嵌封面
    if(jpglen<10)return 0;                                      //无封面

    stream.data=cover_jpgbuf;
    stream.len=jpglen;
    stream.pos=0;
    jr=jd_prepare(&jd,cover_input,cover_pool,COVER_POOL_SIZE,(void*)&stream);
    if(jr!=JDR_OK)
    {
        printf("cover prepare fail jr=%d len=%d\r\n",(int)jr,(int)jpglen);
        return 0;
    }

    //封面解码: 不超过320x320缓冲时全尺寸解码(避免缩放错位), 超过则用TJpgDec scale缩小兜底
    cover_out_w=jd.width;
    cover_out_h=jd.height;
    scale=0;
    while((((u32)cover_out_w*cover_out_h*2)>COVER_RGB_SIZE)&&scale<3)
    {
        cover_out_w>>=1;
        cover_out_h>>=1;
        scale++;
    }
    if(cover_out_w==0)cover_out_w=1;
    if(cover_out_h==0)cover_out_h=1;
    memset(cover_rgbbuf,0,(u32)cover_out_w*cover_out_h*2);      //清零(右/下边缘可能未写入)

    jr=jd_decomp(&jd,cover_output,scale);
    printf("cover: %dx%d scale=%d jr=%d jpglen=%d\r\n",(int)cover_out_w,(int)cover_out_h,
           (int)scale,(int)jr,(int)jpglen);
    if(jr==JDR_OK)
    {
        mp3ui_set_cover(cover_rgbbuf,cover_out_w,cover_out_h);  //显示封面
        ret=1;
    }
    else
    {
        mp3ui_clear_cover();
    }
    return ret;
}

//刷新列表界面(根据music_files重建列表项)
void music_player_list_refresh(void)
{
    u16 i;
    lv_obj_clean(music_list);                           //清空旧列表
    if(music_file_num==0)
    {
        lv_list_add_text(music_list,"未发现音乐文件, 请插入U盘后重试");
        lv_obj_clear_flag(tip_label,LV_OBJ_FLAG_HIDDEN);    //显示提示
        return;
    }
    lv_obj_add_flag(tip_label,LV_OBJ_FLAG_HIDDEN);          //隐藏提示
    for(i=0;i<music_file_num;i++)
    {
        lv_obj_t *btn;
        u32 ci;
        const void *icon=(music_files[i].type==T_MP3)?LV_SYMBOL_PLAY:LV_SYMBOL_AUDIO;
        btn=lv_list_add_btn(music_list,icon,(const char*)music_files[i].name);
        lv_obj_add_event_cb(btn,music_player_list_click,LV_EVENT_CLICKED,(void*)(uintptr_t)i);
        lv_obj_set_style_anim_time(btn,0,0);                //禁止动画,优先解码
        //列表项文本label设置中文字体(lv_list_add_btn内部:child0=icon,child1=label)
        for(ci=0;ci<lv_obj_get_child_cnt(btn);ci++)
        {
            lv_obj_t *child=lv_obj_get_child(btn,ci);
            if(lv_obj_check_type(child,&lv_label_class))
            {
                lv_obj_set_style_text_font(child,font_lib_get_font(),0);
            }
        }
    }
}

//扫描U盘根目录音乐文件
//扫描"2:/"根目录下的MP3/WAV文件, 存入music_files表并刷新列表
void music_player_scan(void)
{
    DIR tdir;
    FILINFO tinfo;
    u8 *fn;
    u8 res;
    u16 cnt=0;
    u16 tries=0;

    if(music_files==NULL)return;
    music_file_num=0;
    //U盘枚举完成后AppState才进入FS_TEST, 读盘才有效; 挂载失败时重试并驱动USBH_Process
    res=f_mount(fs[2],"2:",1);
    while(res!=FR_OK&&tries<500)
    {
        USBH_Process(&USB_OTG_Core,&USB_Host);
        delay_ms(10);
        res=f_mount(fs[2],"2:",1);
        tries++;
    }
    if(res!=FR_OK)                                  //挂载失败(U盘格式不支持等)
    {
        music_player_list_refresh();
        return;
    }
    res=f_opendir(&tdir,"2:/");                             //打开根目录
    if(res!=FR_OK)
    {
        music_player_list_refresh();
        return;
    }
    tinfo.lfsize=_MAX_LFN*2+1;                              //长文件名缓冲大小
    tinfo.lfname=mymalloc(SRAMEX,tinfo.lfsize);
    if(tinfo.lfname==NULL)
    {
        f_closedir(&tdir);
        return;
    }
    while(1)
    {
        res=f_readdir(&tdir,&tinfo);                        //读取下一个条目
        if(res!=FR_OK||tinfo.fname[0]==0)break;             //错误/目录结束
        fn=(u8*)(*tinfo.lfname?tinfo.lfname:tinfo.fname);
        res=f_typetell(fn);                                 //识别文件类型
        if((res&0XF0)==0X40)                                //音频文件(MP3/WAV/APE/FLAC)
        {
            if((res&0XF)<=1&&cnt<MUSIC_MAX_FILES)           //仅支持MP3/WAV
            {
                if(strlen((char*)fn)<MUSIC_NAME_LEN)
                {
                    strcpy((char*)music_files[cnt].gbkname,(char*)fn);  //保存原始GBK名(f_open打开用)
                    gbk_str_to_utf8((u8*)fn,music_files[cnt].name,MUSIC_NAME_LEN);  //GBK转UTF-8(显示用)
                    music_files[cnt].type=res;
                    cnt++;
                }
            }
        }
    }
    myfree(SRAMEX,tinfo.lfname);
    f_closedir(&tdir);
    music_file_num=cnt;
    printf("Scan music files:%d\r\n",(int)cnt);
    music_player_list_refresh();                            //刷新列表显示
}

//播放指定索引曲目
//index:曲目索引(0~music_file_num-1)
void music_player_play_index(u16 index)
{
    if(music_file_num==0)return;
    if(index>=music_file_num)index=0;
    audiodev.curindex=index;                //记录起始播放索引
    mp_pending_index=index;                 //记录待播放索引
    mp_pending=1;                           //标记待播放
    mp3ui_clear_cover();                    //先清除旧封面
    lv_scr_load(mp3ui_get_play_screen());   //切换到播放界面
}

//加载并显示列表界面
void music_player_show_list(void)
{
    lv_scr_load(list_scr);
}

//播放调度状态机(主循环调用)
//空闲时: 检测U盘插入/拔出, 处理待播放请求
//播放时: 阻塞在audio_play()内(解码循环内部通过mp3ui_play_ctrl服务LVGL)
void music_player_loop(void)
{
    static u8 last_connect=0;
    u8 connect;

    connect=USBH_UDISK_Status();                            //检测U盘连接状态
    if(connect!=last_connect)
    {
        last_connect=connect;
        if(connect)
        {
            printf("USB Disk Insert\r\n");
            music_player_scan();                            //U盘插入,扫描音乐文件
        }
        else
        {
            printf("USB Disk Remove\r\n");
            music_file_num=0;                               //U盘拔出,清空列表
            music_player_list_refresh();
        }
    }
    if(mp_playing)return;                                   //播放中(解码循环自行服务LVGL)
    if(mp_pending)                                          //有待播放请求
    {
        if(!USBH_UDISK_Status()||music_file_num==0)         //U盘已拔出/无文件,丢弃请求
        {
            mp_pending=0;
            return;
        }
        mp_pending=0;
        mp_playing=1;
        music_exit_play=0;
        audio_play();                                       //阻塞播放(内部服务LVGL)
        mp_playing=0;
    }
}

//初始化音乐播放器
//创建播放界面(mp3ui_init)和列表界面, 分配曲目表/封面解码缓冲
void music_player_init(void)
{
    lv_obj_t *lbl;

    mp3ui_init();                                           //创建播放界面
    mp3ui_set_back_cb(music_player_back_cb);                //注册返回列表回调

    //分配曲目表与封面解码缓冲(SRAMEX)
    if(music_files==NULL)
    {
        music_files=(music_file_t*)mymalloc(SRAMEX,sizeof(music_file_t)*MUSIC_MAX_FILES);
    }
    if(cover_jpgbuf==NULL)
    {
        cover_jpgbuf=(u8*)mymalloc(SRAMEX,COVER_JPG_SIZE);
    }
    if(cover_rgbbuf==NULL)
    {
        cover_rgbbuf=(u8*)mymalloc(SRAMEX,COVER_RGB_SIZE);
    }
    if(cover_pool==NULL)
    {
        cover_pool=(u8*)mymalloc(SRAMEX,COVER_POOL_SIZE);
    }
    if(music_files==NULL||cover_jpgbuf==NULL||cover_rgbbuf==NULL||cover_pool==NULL)
    {
        printf("Music player memory alloc FAIL!\r\n");
    }

    //创建列表界面
    list_scr=lv_obj_create(NULL);
    lv_obj_set_style_bg_color(list_scr,lv_color_hex(0x0F1B2D),0);  //深蓝背景
    lv_obj_set_style_bg_opa(list_scr,LV_OPA_COVER,0);
    lv_obj_set_style_anim_time(list_scr,0,0);                       //禁止界面动画

    lbl=lv_label_create(list_scr);
    lv_obj_set_style_text_font(lbl,font_lib_get_font(),0);
    lv_obj_set_style_text_color(lbl,lv_color_hex(0xFFFFFF),0);
    lv_label_set_text(lbl,"音乐播放器 - U盘列表");
    lv_obj_align(lbl,LV_ALIGN_TOP_MID,0,12);

    tip_label=lv_label_create(list_scr);
    lv_obj_set_style_text_font(tip_label,font_lib_get_font(),0);
    lv_obj_set_style_text_color(tip_label,lv_color_hex(0xAAAAAA),0);
    lv_label_set_text(tip_label,"请插入U盘...");
    lv_obj_align(tip_label,LV_ALIGN_CENTER,0,0);

    music_list=lv_list_create(list_scr);
    lv_obj_set_size(music_list,460,620);
    lv_obj_set_pos(music_list,10,50);
    lv_obj_set_style_border_width(music_list,0,0);
    lv_obj_set_style_radius(music_list,0,0);
    lv_obj_set_style_bg_opa(music_list,LV_OPA_TRANSP,0);
    lv_obj_set_style_anim_time(music_list,0,0);                     //禁止列表动画

    music_file_num=0;
    music_player_list_refresh();                                    //初始空列表
    lv_scr_load(list_scr);                                          //默认显示列表界面
}
