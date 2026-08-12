$ErrorActionPreference = 'Stop'

# Build 014_music_player.uvprojx from 013_lvgl时钟.uvprojx template
$root = $PSScriptRoot
$srcPath = Join-Path $root 'USER\013_lvgl时钟.uvprojx'
$dstPath = Join-Path $root 'USER\014_music_player.uvprojx'

$utf8 = New-Object System.Text.UTF8Encoding($false)
$content = [System.IO.File]::ReadAllText($srcPath, $utf8)

# Replace target name / output name
$content = $content.Replace('013_lvgl时钟', '014_lvgl音乐播放器')
$content = $content.Replace('<OutputName>013_lvgl_clock</OutputName>', '<OutputName>014_music_player</OutputName>')

# ARMCC C99 mode (needed by LVGL)
$content = $content.Replace('<uC99>0</uC99>', '<uC99>1</uC99>')

# Keep batch via files for diagnostics
$content = $content.Replace('<CreateBatchFile>0</CreateBatchFile>', '<CreateBatchFile>1</CreateBatchFile>')

# Replace Define (add LV_CONF_INCLUDE_SIMPLE + USE_USB_OTG_FS)
$content = $content.Replace('<Define>STM32F40_41xxx,USE_STDPERIPH_DRIVER,LV_CONF_INCLUDE_SIMPLE</Define>', '<Define>STM32F40_41xxx,USE_STDPERIPH_DRIVER,LV_CONF_INCLUDE_SIMPLE,USE_USB_OTG_FS</Define>')

# Replace IncludePath
$oldInc = '<IncludePath>..\CORE;..\SYSTEM\delay;..\SYSTEM\sys;..\SYSTEM\usart;..\USER;..\HARDWARE\LED;..\HARDWARE\LCD;..\HARDWARE\KEY;..\HARDWARE\IIC;..\HARDWARE\TIMER;..\HARDWARE\SRAM;..\HARDWARE\RTC;..\HARDWARE\CLOCK;..\FWLIB\inc;..\HARDWARE\TOUCH;..\LVGL;..\LVGL\src;..\LVGL\demos;..\LVGL\demos\widgets</IncludePath>'
$newInc = '<IncludePath>..\CORE;..\SYSTEM\delay;..\SYSTEM\sys;..\SYSTEM\usart;..\USER;..\HARDWARE\LED;..\HARDWARE\LCD;..\HARDWARE\KEY;..\HARDWARE\I2S;..\HARDWARE\WM8978;..\HARDWARE\IIC;..\HARDWARE\TOUCH;..\HARDWARE\TIMER;..\HARDWARE\SRAM;..\HARDWARE\SDIO;..\HARDWARE\FONT;..\MALLOC;..\FATFS\exfuns;..\FATFS\src;..\FWLIB\inc;..\APP;..\AUDIOCODEC\mp3;..\AUDIOCODEC\mp3\helix;..\AUDIOCODEC\wav;..\PICTURE;..\USB\USB_APP;..\USB\STM32_USB_OTG_Driver\inc;..\USB\STM32_USB_HOST_Library\Core\inc;..\USB\STM32_USB_HOST_Library\Class\MSC\inc;..\LVGL;..\LVGL\src</IncludePath>'
if ($content.Contains($oldInc)) {
    $content = $content.Replace($oldInc, $newInc)
    Write-Output 'IncludePath replaced'
} else {
    Write-Output 'WARN: IncludePath pattern not found'
}

function FileXml($name, $type, $path) {
    return "            <File>`r`n              <FileName>$name</FileName>`r`n              <FileType>$type</FileType>`r`n              <FilePath>$path</FilePath>`r`n            </File>"
}

function GroupXml($groupName, $files) {
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.AppendLine("        <Group>")
    [void]$sb.AppendLine("          <GroupName>$groupName</GroupName>")
    [void]$sb.AppendLine("          <Files>")
    foreach ($f in $files) {
        [void]$sb.AppendLine((FileXml $f[0] $f[1] $f[2]))
    }
    [void]$sb.AppendLine("          </Files>")
    [void]$sb.AppendLine("        </Group>")
    return $sb.ToString()
}

# ---- USER group ----
$userFiles = @(
    @('main.c', '1', '.\main.c'),
    @('stm32f4xx_it.c', '1', '.\stm32f4xx_it.c'),
    @('system_stm32f4xx.c', '1', '.\system_stm32f4xx.c')
)

# ---- APP group ----
$appFiles = @(
    @('audioplay.c', '1', '..\APP\audioplay.c'),
    @('mp3ui.c', '1', '..\APP\mp3ui.c'),
    @('music_player.c', '1', '..\APP\music_player.c')
)

# ---- AUDIOCODEC group (MP3 helix + WAV) ----
$audioFiles = @(
    @('mp3play.c', '1', '..\AUDIOCODEC\mp3\mp3play.c'),
    @('wavplay.c', '1', '..\AUDIOCODEC\wav\wavplay.c'),
    @('bitstream.c', '1', '..\AUDIOCODEC\mp3\helix\bitstream.c'),
    @('buffers.c', '1', '..\AUDIOCODEC\mp3\helix\buffers.c'),
    @('dct32.c', '1', '..\AUDIOCODEC\mp3\helix\dct32.c'),
    @('dequant.c', '1', '..\AUDIOCODEC\mp3\helix\dequant.c'),
    @('dqchan.c', '1', '..\AUDIOCODEC\mp3\helix\dqchan.c'),
    @('huffman.c', '1', '..\AUDIOCODEC\mp3\helix\huffman.c'),
    @('hufftabs.c', '1', '..\AUDIOCODEC\mp3\helix\hufftabs.c'),
    @('imdct.c', '1', '..\AUDIOCODEC\mp3\helix\imdct.c'),
    @('mp3dec.c', '1', '..\AUDIOCODEC\mp3\helix\mp3dec.c'),
    @('mp3tabs.c', '1', '..\AUDIOCODEC\mp3\helix\mp3tabs.c'),
    @('scalfact.c', '1', '..\AUDIOCODEC\mp3\helix\scalfact.c'),
    @('stproc.c', '1', '..\AUDIOCODEC\mp3\helix\stproc.c'),
    @('subband.c', '1', '..\AUDIOCODEC\mp3\helix\subband.c'),
    @('trigtabs.c', '1', '..\AUDIOCODEC\mp3\helix\trigtabs.c'),
    @('asmmisc_rvds.s', '2', '..\AUDIOCODEC\mp3\helix\arm\asmmisc_rvds.s'),
    @('asmpoly_thumb2_rvds.s', '2', '..\AUDIOCODEC\mp3\helix\arm\asmpoly_thumb2_rvds.s')
)

# ---- HARDWARE group ----
$hwFiles = @(
    @('led.c', '1', '..\HARDWARE\LED\led.c'),
    @('lcd.c', '1', '..\HARDWARE\LCD\lcd.c'),
    @('key.c', '1', '..\HARDWARE\KEY\key.c'),
    @('i2s.c', '1', '..\HARDWARE\I2S\i2s.c'),
    @('wm8978.c', '1', '..\HARDWARE\WM8978\wm8978.c'),
    @('myiic.c', '1', '..\HARDWARE\IIC\myiic.c'),
    @('ctiic.c', '1', '..\HARDWARE\TOUCH\ctiic.c'),
    @('gt9147.c', '1', '..\HARDWARE\TOUCH\gt9147.c'),
    @('ott2001a.c', '1', '..\HARDWARE\TOUCH\ott2001a.c'),
    @('ft5206.c', '1', '..\HARDWARE\TOUCH\ft5206.c'),
    @('touch.c', '1', '..\HARDWARE\TOUCH\touch.c'),
    @('timer.c', '1', '..\HARDWARE\TIMER\timer.c'),
    @('sram.c', '1', '..\HARDWARE\SRAM\sram.c'),
    @('sdio_sdcard.c', '1', '..\HARDWARE\SDIO\sdio_sdcard.c'),
    @('font_lib.c', '1', '..\HARDWARE\FONT\font_lib.c'),
    @('fontlib_bitmap.c', '1', '..\HARDWARE\FONT\fontlib_bitmap.c'),
    @('gbk2utf8.c', '1', '..\HARDWARE\FONT\gbk2utf8.c')
)

# ---- SYSTEM group ----
$sysFiles = @(
    @('delay.c', '1', '..\SYSTEM\delay\delay.c'),
    @('sys.c', '1', '..\SYSTEM\sys\sys.c'),
    @('usart.c', '1', '..\SYSTEM\usart\usart.c')
)

# ---- CORE group ----
$coreFiles = @(
    ,@('startup_stm32f40_41xxx.s', '2', '..\CORE\startup_stm32f40_41xxx.s')
)

# ---- FWLIB group ----
$fwlFiles = @(
    @('misc.c', '1', '..\FWLIB\src\misc.c'),
    @('stm32f4xx_dma.c', '1', '..\FWLIB\src\stm32f4xx_dma.c'),
    @('stm32f4xx_fsmc.c', '1', '..\FWLIB\src\stm32f4xx_fsmc.c'),
    @('stm32f4xx_gpio.c', '1', '..\FWLIB\src\stm32f4xx_gpio.c'),
    @('stm32f4xx_rcc.c', '1', '..\FWLIB\src\stm32f4xx_rcc.c'),
    @('stm32f4xx_spi.c', '1', '..\FWLIB\src\stm32f4xx_spi.c'),
    @('stm32f4xx_sdio.c', '1', '..\FWLIB\src\stm32f4xx_sdio.c'),
    @('stm32f4xx_syscfg.c', '1', '..\FWLIB\src\stm32f4xx_syscfg.c'),
    @('stm32f4xx_tim.c', '1', '..\FWLIB\src\stm32f4xx_tim.c'),
    @('stm32f4xx_usart.c', '1', '..\FWLIB\src\stm32f4xx_usart.c')
)

# ---- MALLOC group ----
$mallocFiles = @(
    ,@('malloc.c', '1', '..\MALLOC\malloc.c')
)

# ---- FATFS group ----
$fatfsFiles = @(
    @('diskio.c', '1', '..\FATFS\src\diskio.c'),
    @('ff.c', '1', '..\FATFS\src\ff.c'),
    @('exfuns.c', '1', '..\FATFS\exfuns\exfuns.c'),
    @('cc936.c', '1', '..\FATFS\src\option\cc936.c')
)

# ---- PICTURE group (TJpgDec) ----
$picFiles = @(
    ,@('tjpgd.c', '1', '..\PICTURE\tjpgd.c')
)

# ---- USB_OTG group ----
$usbotgFiles = @(
    @('usb_core.c', '1', '..\USB\STM32_USB_OTG_Driver\src\usb_core.c'),
    @('usb_hcd.c', '1', '..\USB\STM32_USB_OTG_Driver\src\usb_hcd.c'),
    @('usb_hcd_int.c', '1', '..\USB\STM32_USB_OTG_Driver\src\usb_hcd_int.c')
)

# ---- USB_HOST group ----
$usbhostFiles = @(
    @('usbh_core.c', '1', '..\USB\STM32_USB_HOST_Library\Core\src\usbh_core.c'),
    @('usbh_hcs.c', '1', '..\USB\STM32_USB_HOST_Library\Core\src\usbh_hcs.c'),
    @('usbh_ioreq.c', '1', '..\USB\STM32_USB_HOST_Library\Core\src\usbh_ioreq.c'),
    @('usbh_stdreq.c', '1', '..\USB\STM32_USB_HOST_Library\Core\src\usbh_stdreq.c'),
    @('usbh_msc_bot.c', '1', '..\USB\STM32_USB_HOST_Library\Class\MSC\src\usbh_msc_bot.c'),
    @('usbh_msc_core.c', '1', '..\USB\STM32_USB_HOST_Library\Class\MSC\src\usbh_msc_core.c'),
    @('usbh_msc_scsi.c', '1', '..\USB\STM32_USB_HOST_Library\Class\MSC\src\usbh_msc_scsi.c')
)

# ---- USB_APP group ----
$usbappFiles = @(
    @('usb_bsp.c', '1', '..\USB\USB_APP\usb_bsp.c'),
    @('usbh_usr.c', '1', '..\USB\USB_APP\usbh_usr.c')
)

# ---- Collect LVGL sources (mirror 013, no demos) ----
$lvglFiles = New-Object System.Collections.ArrayList

Get-ChildItem "$root\LVGL\src\core\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\core\$($_.Name)"))
}
Get-ChildItem "$root\LVGL\src\draw\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\draw\$($_.Name)"))
}
Get-ChildItem "$root\LVGL\src\draw\sw\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\draw\sw\$($_.Name)"))
}
Get-ChildItem "$root\LVGL\src\extra\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\extra\$($_.Name)"))
}
Get-ChildItem "$root\LVGL\src\extra\layouts\flex\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\extra\layouts\flex\$($_.Name)"))
}
Get-ChildItem "$root\LVGL\src\extra\layouts\grid\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\extra\layouts\grid\$($_.Name)"))
}
Get-ChildItem "$root\LVGL\src\extra\themes\default\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\extra\themes\default\$($_.Name)"))
}
Get-ChildItem "$root\LVGL\src\extra\themes\basic\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\extra\themes\basic\$($_.Name)"))
}
Get-ChildItem "$root\LVGL\src\extra\themes\mono\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\extra\themes\mono\$($_.Name)"))
}
Get-ChildItem "$root\LVGL\src\extra\widgets\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\extra\widgets\$($_.Name)"))
}
Get-ChildItem "$root\LVGL\src\extra\widgets" -Directory | ForEach-Object {
    Get-ChildItem "$($_.FullName)\*.c" | ForEach-Object {
        [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\extra\widgets\$($_.Directory.Name)\$($_.Name)"))
    }
}
# font (montserrat needed by widgets; Chinese handled by font_lib from SD card)
$fontNames = @('lv_font.c', 'lv_font_fmt_txt.c', 'lv_font_montserrat_12.c', 'lv_font_montserrat_14.c', 'lv_font_montserrat_16.c', 'lv_font_montserrat_20.c')
foreach ($fn in $fontNames) {
    [void]$lvglFiles.Add(@($fn, '1', "..\LVGL\src\font\$fn"))
}
Get-ChildItem "$root\LVGL\src\hal\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\hal\$($_.Name)"))
}
Get-ChildItem "$root\LVGL\src\misc\*.c" | Where-Object { $_.Name -ne 'lv_templ.c' } | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\misc\$($_.Name)"))
}
Get-ChildItem "$root\LVGL\src\widgets\*.c" | Where-Object { $_.Name -ne 'lv_objx_templ.c' } | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\widgets\$($_.Name)"))
}

# LVGL port files
$lvglFiles.Add(@('lv_port_disp.c', '1', '..\LVGL\lv_port_disp.c')) | Out-Null
$lvglFiles.Add(@('lv_port_indev.c', '1', '..\LVGL\lv_port_indev.c')) | Out-Null

$groupsSb = New-Object System.Text.StringBuilder
[void]$groupsSb.AppendLine("      <Groups>")
[void]$groupsSb.Append((GroupXml 'USER' $userFiles))
[void]$groupsSb.Append((GroupXml 'APP' $appFiles))
[void]$groupsSb.Append((GroupXml 'AUDIOCODEC' $audioFiles))
[void]$groupsSb.Append((GroupXml 'HARDWARE' $hwFiles))
[void]$groupsSb.Append((GroupXml 'SYSTEM' $sysFiles))
[void]$groupsSb.Append((GroupXml 'CORE' $coreFiles))
[void]$groupsSb.Append((GroupXml 'FWLIB' $fwlFiles))
[void]$groupsSb.Append((GroupXml 'MALLOC' $mallocFiles))
[void]$groupsSb.Append((GroupXml 'FATFS' $fatfsFiles))
[void]$groupsSb.Append((GroupXml 'PICTURE' $picFiles))
[void]$groupsSb.Append((GroupXml 'USB_OTG' $usbotgFiles))
[void]$groupsSb.Append((GroupXml 'USB_HOST' $usbhostFiles))
[void]$groupsSb.Append((GroupXml 'USB_APP' $usbappFiles))
[void]$groupsSb.Append((GroupXml 'LVGL' $lvglFiles))
[void]$groupsSb.AppendLine("      </Groups>")

$startTag = '      <Groups>'
$endTag = '      </Groups>'
$startIdx = $content.IndexOf($startTag)
$endIdx = $content.IndexOf($endTag) + $endTag.Length
$content = $content.Substring(0, $startIdx) + $groupsSb.ToString() + $content.Substring($endIdx)

[System.IO.File]::WriteAllText($dstPath, $content, $utf8)
Write-Output "Written: $dstPath"
Write-Output ('LVGL files count: ' + $lvglFiles.Count)
