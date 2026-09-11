# N合一强化版ReDashboard，基于RP2040芯片实现的开源"EZ Dashboard"
简体中文 | [English](./README.md)

<table>
  <tr>
    <td><img src="./images/rp2040_post.png" width="200" alt="Image 1"></td>
    <td><img src="./images/rp2040_temp.png" width="200" alt="Image 2"></td>
  </tr>
  <tr>
    <td><img src="./images/rp2040_volt.png" width="200" alt="Image 3"></td>
    <td><img src="./images/rp2040_tach.png" width="200" alt="Image 4"></td>
  </tr>
</table>

### 硬件支持说明:
**支持以下接头开机自检跑码**
* JDASH1
* JDP1
* JBD1
* 任何其他UART 115200-8N1的接头

**在微星主板上，接入"JDASH1"可实现:**
* 显示并记录开机自检代码(Port80/81(8bit))
* 显示风扇转速
* 显示系统温度
* 显示系统电压

**在微星主板上，接入"JDP1"可实现:**
* 显示并记录开机自检代码(Port80(32bit),Port81-83(8bit))

**在其他主板上，接入任何以UART 115200-8N1发送代码的主板可实现:**
* 显示并记录开机自检代码(Port80(8bit))

**所有主板额外接入JBAT，JFP1可控制:**
* BIOS重置，开关机

### 固件功能说明:
代码回溯:\
支持P80代码缓存至芯片内存，板上按键可以上下滚动记录

代码输出:\
接入USB-C是一个USB-CDC UART装置\
支持P80代码解码后输出\
后续SDK支持电压/温度/风扇转速记录

温度/电压/转速传感器:\
支持基于主板型号，自定义标签\
支持温度C / F 转换

OLED页面控制:\
支持关闭没用的页面，例如不显示电压页\
支持配置N秒后返回主页\
支持温度/电压/转速有重大变更时，自动点屏显示\
支持自检代码更新自动显示

OLED保护功能:\
像素偏移\
自动暗屏\
自动隐藏页面标题\
自动关屏

### 按键说明:
两边按键短按一下-&gt; 切换输入源(JDASH/UART)\
两边按键长按3秒 -&gt; 切换按键模式(翻页模式/电源键模式)\
两边按键长按5秒 -&gt; 锁定JDASH/UART输入，锁定按键(无功能)，长按解锁\
两边按键长按10秒 -&gt; 重置BIOS (NMOS接通JBAT1)

单边按键连按5下 -&gt; 翻页(切换显示页面)\
P80页面下，单边按键长按 -&gt; 上/下滚动翻阅P80纪录

### BOM + 生产说明:
**想要直接复刻的请注意，由于设计是外部汇入LCEDA的**\
**我并不确认使用此项目，汇出Gerber实际生产能不能用，请自行确认原理和PCB布线/铺铜**\
**使用Autodesk Eagle的档案生成Gerber理论没有问题**

只需约15块钱的复刻成本:\
6元购入0.96寸，4线I2C OLED模块\
5元购入RP2040模块，拆下RP2040, 晶振, SPI NOR\
4元购入其他电容,电阻,LDO,其他小料\
0元白嫖的2层PCB(感谢咖喱创)

**注意:**\
这里不建议复刻RP2040版的ReDashboard\
因为之后还有使用PY32作为主控的版本，芯片成本大幅降至5毛钱 + 基本不需要外围电路\
程序已写好，并通过测试，之后有空会更新\
<br>

### 固件, 固件原码说明:
固件可以使用预编译的firmware.uf2档，但这里建议自行更改配置后编译使用
<br>

### 公开使用授权说明:
本项目，硬件/固件均使用GPL(v3或新版)授权\
本人不对项目的功能和安全作任何保证\
你可以自由使用，贩售，基于本项目的产品
```
/*
 * This file is part of the "ReDashboard_V2" distribution.
 *
 * Copyright (C) 2026 @himko9 <me@himko.dev>
 * Github: https://github.com/himko9/ReDashboard_V2
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
 ```