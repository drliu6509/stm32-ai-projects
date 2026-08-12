$ErrorActionPreference = 'Stop'

# Build 012_lvgl_test.uvprojx from DAC_WAVE.uvprojx template
$root = $PSScriptRoot
$srcPath = Join-Path $root 'USER\DAC_WAVE.uvprojx'
$dstPath = Join-Path $root 'USER\012_lvgl测试.uvprojx'

$utf8 = New-Object System.Text.UTF8Encoding($false)
$content = [System.IO.File]::ReadAllText($srcPath, $utf8)

# Replace target name / output name
$content = $content.Replace('DAC_WAVE', '012_lvgl测试')

# OutputName must be ASCII (keil build command line issue with chinese)
$content = $content.Replace('<OutputName>012_lvgl测试</OutputName>', '<OutputName>012_lvgl_test</OutputName>')

# ARMCC C99 mode (needed by LVGL)
$content = $content.Replace('<uC99>0</uC99>', '<uC99>1</uC99>')

# Keep batch via files for diagnostics
$content = $content.Replace('<CreateBatchFile>0</CreateBatchFile>', '<CreateBatchFile>1</CreateBatchFile>')

# Replace Define (add LV_CONF_INCLUDE_SIMPLE)
$content = $content.Replace('<Define>STM32F40_41xxx,USE_STDPERIPH_DRIVER</Define>', '<Define>STM32F40_41xxx,USE_STDPERIPH_DRIVER,LV_CONF_INCLUDE_SIMPLE</Define>')

# Replace IncludePath
$oldInc = '<IncludePath>..\CORE;..\SYSTEM\delay;..\SYSTEM\sys;..\SYSTEM\usart;..\USER;..\HARDWARE\LED;..\HARDWARE\LCD;..\HARDWARE\KEY;..\HARDWARE\IIC;..\HARDWARE\24CXX;..\FWLIB\inc;..\HARDWARE\TOUCH;..\HARDWARE\DAC;..\HARDWARE\WAVE</IncludePath>'
$newInc = '<IncludePath>..\CORE;..\SYSTEM\delay;..\SYSTEM\sys;..\SYSTEM\usart;..\USER;..\HARDWARE\LED;..\HARDWARE\LCD;..\HARDWARE\KEY;..\HARDWARE\IIC;..\HARDWARE\TIMER;..\FWLIB\inc;..\HARDWARE\TOUCH;..\LVGL;..\LVGL\src;..\LVGL\demos;..\LVGL\demos\widgets</IncludePath>'
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

# ---- HARDWARE group ----
$hwFiles = @(
    @('led.c', '1', '..\HARDWARE\LED\led.c'),
    @('lcd.c', '1', '..\HARDWARE\LCD\lcd.c'),
    @('key.c', '1', '..\HARDWARE\KEY\key.c'),
    @('myiic.c', '1', '..\HARDWARE\IIC\myiic.c'),
    @('ctiic.c', '1', '..\HARDWARE\TOUCH\ctiic.c'),
    @('gt9147.c', '1', '..\HARDWARE\TOUCH\gt9147.c'),
    @('ott2001a.c', '1', '..\HARDWARE\TOUCH\ott2001a.c'),
    @('ft5206.c', '1', '..\HARDWARE\TOUCH\ft5206.c'),
    @('touch.c', '1', '..\HARDWARE\TOUCH\touch.c'),
    @('timer.c', '1', '..\HARDWARE\TIMER\timer.c')
)

# ---- SYSTEM group ----
$sysFiles = @(
    @('delay.c', '1', '..\SYSTEM\delay\delay.c'),
    @('sys.c', '1', '..\SYSTEM\sys\sys.c'),
    @('usart.c', '1', '..\SYSTEM\usart\usart.c')
)

# ---- CORE group ----
# NOTE: single-element nested array would be unwrapped by PowerShell,
# use the ',' operator to keep it as an array.
$coreFiles = @(
    ,@('startup_stm32f40_41xxx.s', '2', '..\CORE\startup_stm32f40_41xxx.s')
)

# ---- FWLIB group ----
$fwlFiles = @(
    @('misc.c', '1', '..\FWLIB\src\misc.c'),
    @('stm32f4xx_gpio.c', '1', '..\FWLIB\src\stm32f4xx_gpio.c'),
    @('stm32f4xx_fsmc.c', '1', '..\FWLIB\src\stm32f4xx_fsmc.c'),
    @('stm32f4xx_rcc.c', '1', '..\FWLIB\src\stm32f4xx_rcc.c'),
    @('stm32f4xx_syscfg.c', '1', '..\FWLIB\src\stm32f4xx_syscfg.c'),
    @('stm32f4xx_usart.c', '1', '..\FWLIB\src\stm32f4xx_usart.c'),
    @('stm32f4xx_tim.c', '1', '..\FWLIB\src\stm32f4xx_tim.c'),
    @('stm32f4xx_dma.c', '1', '..\FWLIB\src\stm32f4xx_dma.c')
)

# ---- Collect LVGL sources ----
$lvglFiles = New-Object System.Collections.ArrayList

# core
Get-ChildItem "$root\LVGL\src\core\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\core\$($_.Name)"))
}
# draw (root level only)
Get-ChildItem "$root\LVGL\src\draw\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\draw\$($_.Name)"))
}
# draw/sw
Get-ChildItem "$root\LVGL\src\draw\sw\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\draw\sw\$($_.Name)"))
}
# extra
Get-ChildItem "$root\LVGL\src\extra\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\extra\$($_.Name)"))
}
# extra/layouts
Get-ChildItem "$root\LVGL\src\extra\layouts\flex\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\extra\layouts\flex\$($_.Name)"))
}
Get-ChildItem "$root\LVGL\src\extra\layouts\grid\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\extra\layouts\grid\$($_.Name)"))
}
# extra/themes
Get-ChildItem "$root\LVGL\src\extra\themes\default\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\extra\themes\default\$($_.Name)"))
}
Get-ChildItem "$root\LVGL\src\extra\themes\basic\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\extra\themes\basic\$($_.Name)"))
}
Get-ChildItem "$root\LVGL\src\extra\themes\mono\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\extra\themes\mono\$($_.Name)"))
}
# extra/widgets (recursive)
Get-ChildItem "$root\LVGL\src\extra\widgets\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\extra\widgets\$($_.Name)"))
}
Get-ChildItem "$root\LVGL\src\extra\widgets" -Directory | ForEach-Object {
    Get-ChildItem "$($_.FullName)\*.c" | ForEach-Object {
        [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\extra\widgets\$($_.Directory.Name)\$($_.Name)"))
    }
}
# font (needed ones)
$fontNames = @('lv_font.c', 'lv_font_fmt_txt.c', 'lv_font_montserrat_12.c', 'lv_font_montserrat_14.c', 'lv_font_montserrat_16.c', 'lv_font_montserrat_18.c', 'lv_font_montserrat_20.c')
foreach ($fn in $fontNames) {
    [void]$lvglFiles.Add(@($fn, '1', "..\LVGL\src\font\$fn"))
}
# hal
Get-ChildItem "$root\LVGL\src\hal\*.c" | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\hal\$($_.Name)"))
}
# misc (exclude lv_templ.c)
Get-ChildItem "$root\LVGL\src\misc\*.c" | Where-Object { $_.Name -ne 'lv_templ.c' } | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\misc\$($_.Name)"))
}
# widgets (exclude lv_objx_templ.c)
Get-ChildItem "$root\LVGL\src\widgets\*.c" | Where-Object { $_.Name -ne 'lv_objx_templ.c' } | ForEach-Object {
    [void]$lvglFiles.Add(@($_.Name, '1', "..\LVGL\src\widgets\$($_.Name)"))
}

# ---- LVGL port files ----
$lvglFiles.Add(@('lv_port_disp.c', '1', '..\LVGL\lv_port_disp.c')) | Out-Null
$lvglFiles.Add(@('lv_port_indev.c', '1', '..\LVGL\lv_port_indev.c')) | Out-Null

# ---- Demo group ----
$demoFiles = @(
    @('lv_demo_widgets.c', '1', '..\LVGL\demos\widgets\lv_demo_widgets.c'),
    @('img_lvgl_logo.c', '1', '..\LVGL\demos\widgets\assets\img_lvgl_logo.c'),
    @('img_demo_widgets_avatar.c', '1', '..\LVGL\demos\widgets\assets\img_demo_widgets_avatar.c'),
    @('img_clothes.c', '1', '..\LVGL\demos\widgets\assets\img_clothes.c')
)

$groupsSb = New-Object System.Text.StringBuilder
[void]$groupsSb.AppendLine("      <Groups>")
[void]$groupsSb.Append((GroupXml 'USER' $userFiles))
[void]$groupsSb.Append((GroupXml 'HARDWARE' $hwFiles))
[void]$groupsSb.Append((GroupXml 'SYSTEM' $sysFiles))
[void]$groupsSb.Append((GroupXml 'CORE' $coreFiles))
[void]$groupsSb.Append((GroupXml 'FWLIB' $fwlFiles))
[void]$groupsSb.Append((GroupXml 'LVGL' $lvglFiles))
[void]$groupsSb.Append((GroupXml 'LVGL_DEMO' $demoFiles))
[void]$groupsSb.AppendLine("      </Groups>")

$startTag = '      <Groups>'
$endTag = '      </Groups>'
$startIdx = $content.IndexOf($startTag)
$endIdx = $content.IndexOf($endTag) + $endTag.Length
$content = $content.Substring(0, $startIdx) + $groupsSb.ToString() + $content.Substring($endIdx)

[System.IO.File]::WriteAllText($dstPath, $content, $utf8)
Write-Output "Written: $dstPath"
Write-Output ('LVGL files count: ' + $lvglFiles.Count)
