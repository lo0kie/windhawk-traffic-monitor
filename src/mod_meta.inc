// ==WindhawkMod==
// @id              windhawk-traffic-monitor
// @name            Windhawk Traffic Monitor
// @name:zh-CN      任务栏网速监控
// @description     TrafficMonitor's taskbar window as a Windhawk mod: shows network speed / CPU / memory on the taskbar via a format string, single or double row
// @description:zh-CN TrafficMonitor 的任务栏窗口：用格式串在任务栏上显示网速 / CPU / 内存等，支持单排和双排
// @version         1.0.2
// @author          lo0kie
// @github          https://github.com/lo0kie/windhawk-traffic-monitor
// @license         GPL-3.0
// @include         windhawk.exe
// @compilerOptions -lgdi32 -ldwrite -lpdh -liphlpapi -lcomctl32 -lshell32 -ladvapi32 -lole32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Windhawk Traffic Monitor

A Windhawk port of [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor)'s
taskbar window. Only the taskbar window, no desktop floating window, skins, plugins or
temperature monitoring.

This is a **tool mod**: it does not inject into `explorer.exe` or hook any functions.
It runs in Windhawk's dedicated process and floats its own layered window above the
taskbar to show the data.

Source code, issues and releases:
<https://github.com/lo0kie/windhawk-traffic-monitor>

## Features

- Upload / download / total network speed (the most active adapter), CPU, memory, GPU, disk activity
- Display is fully customizable via a **format string**, one row or two rows (top / bottom)
- Font, size, weight, text color, background color / opacity, alignment; changes apply instantly
- Text drawn with DirectWrite grayscale antialiasing (8-bit coverage), crisp at small sizes
- Follows the system dark / light theme
- Hover for detailed info; single, middle and right click can each be bound to an action
- Follows the taskbar: hides when the taskbar auto-hides or goes fullscreen

## Format string

Insert values with `${name}`, everything else is printed verbatim:

| Placeholder | Meaning |
| --- | --- |
| `${network_up}` | Upload speed |
| `${network_down}` | Download speed |
| `${network_total}` | Total speed (up + down) |
| `${cpu_usage}` | CPU usage |
| `${memory_usage}` | Memory usage (percentage) |
| `${memory_used}` | Used memory |
| `${memory_available}` | Available memory |
| `${memory_total}` | Total memory |
| `${gpu_usage}` | GPU usage |
| `${disk_usage}` | Disk activity |

Example: `↑: ${network_up}    ↓: ${network_down}    CPU: ${cpu_usage}`.
The window width follows the actual text width.

## Notes

- Taskbar window only: no desktop floating window, skins, plugins, usage bars, temperature or traffic history.
- The window sits to the left of the notification area, and moves to the left of the Start
  button when space is tight; or pin it with "Position" and "Horizontal offset".
- Only horizontal taskbars are supported (Windows 11 does not support vertical taskbars).
- Since this is a floating window in a separate process (not a taskbar child window), the
  right edge of the taskbar icon area is estimated; `auto` uses the Start button to
  estimate it symmetrically (conservative) and moves to the left of the Start button if
  there is not enough room.

## License

This mod is a derivative of [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor)
(GPL-3.0) and is likewise released under **GPL-3.0**. The full license text is in the
accompanying `LICENSE` file, or at <https://www.gnu.org/licenses/gpl-3.0.html>.

Copyright: this mod © lo0kie. Layout, number formatting rules, and network/usage
collection logic are ported from TrafficMonitor © zhongyang219 (GPL-3.0).
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- SingleRow: "↑: ${network_up}    ↓: ${network_down}"
  $name: Single-row format
  $name:zh-CN: 单排格式
  $description: Line shown in single-row mode; ${name} is replaced by a value, other characters are output verbatim. Placeholders - ${network_up} upload, ${network_down} download, ${network_total} total, ${cpu_usage} CPU, ${memory_usage} memory, ${memory_used} used, ${memory_available} available, ${memory_total} total memory, ${gpu_usage} GPU, ${disk_usage} disk.
  $description:zh-CN: 单排模式显示的整行文本；${名称} 替换为对应数值，其余字符原样输出。占位符：${network_up} 上传、${network_down} 下载、${network_total} 总速率、${cpu_usage} CPU、${memory_usage} 内存、${memory_used} 已用、${memory_available} 可用、${memory_total} 总量、${gpu_usage} GPU、${disk_usage} 磁盘。
- Arrange: horizontal
  $name: Arrangement
  $name:zh-CN: 排列方式
  $description: Single row shows one line; two rows split into top and bottom, taking less width.
  $description:zh-CN: 单排仅显示一行；双排分为上下两行，占用宽度更小。
  $options:
  - horizontal: Single row
  - vertical: Two rows
  $options:zh-CN:
  - horizontal: 单排
  - vertical: 双排
- TopRow: "↑: ${network_up}"
  $name: Top-row format (two rows)
  $name:zh-CN: 上排格式（双排）
  $description: Upper line in two-row mode; same syntax as the single-row format.
  $description:zh-CN: 双排时的上方一行，语法同“单排格式”。
- BottomRow: "↓: ${network_down}"
  $name: Bottom-row format (two rows)
  $name:zh-CN: 下排格式（双排）
  $description: Lower line in two-row mode; same syntax as the single-row format.
  $description:zh-CN: 双排时的下方一行，语法同“单排格式”。
- FontSize: 14
  $name: Font size (pixels)
  $name:zh-CN: 字号（像素）
  $description: Text height in pixels.
  $description:zh-CN: 文字高度，单位为像素。
- FontFace: Microsoft YaHei UI
  $name: Font
  $name:zh-CN: 字体
  $description: Font family used for the taskbar text.
  $description:zh-CN: 任务栏文字所用字体。
- FontWeight: normal
  $name: Font weight
  $name:zh-CN: 字重
  $description: Effective only if the font provides that face; otherwise the closest face is used.
  $description:zh-CN: 仅当字体提供对应字面时生效，否则使用最接近的字面。
  $options:
  - thin: Thin (100)
  - extralight: Extra Light (200)
  - light: Light (300)
  - normal: Regular (400)
  - medium: Medium (500)
  - semibold: Semibold (600)
  - bold: Bold (700)
  - extrabold: Extra Bold (800)
  - black: Black (900)
  $options:zh-CN:
  - thin: 极细（100）
  - extralight: 超细（200）
  - light: 细体（300）
  - normal: 常规（400）
  - medium: 中等（500）
  - semibold: 半粗（600）
  - bold: 粗体（700）
  - extrabold: 特粗（800）
  - black: 超粗（900）
- SpeedUnit: auto
  $name: Speed unit
  $name:zh-CN: 网速单位
  $options:
  - auto: Auto (KB/s, MB/s)
  - kb: Fixed KB/s
  - mb: Fixed MB/s
  - auto-bits: Auto (Kb/s, Mb/s, bits)
  - kb-bits: Fixed Kb/s (bits)
  - mb-bits: Fixed Mb/s (bits)
  $options:zh-CN:
  - auto: 自动（KB/s、MB/s）
  - kb: 固定 KB/s
  - mb: 固定 MB/s
  - auto-bits: 自动（Kb/s、Mb/s，比特）
  - kb-bits: 固定 Kb/s（比特）
  - mb-bits: 固定 Mb/s（比特）
- HideUnit: false
  $name: Hide units
  $name:zh-CN: 隐藏单位
  $description: Hide units such as KB/s, GB and %.
  $description:zh-CN: 不显示 KB/s、GB、% 等单位。
- Theme: auto
  $name: Theme
  $name:zh-CN: 主题
  $description: Auto follows the system dark / light mode.
  $description:zh-CN: 自动：跟随系统深色 / 浅色模式。
  $options:
  - auto: Follow system
  - dark: Dark
  - light: Light
  $options:zh-CN:
  - auto: 跟随系统
  - dark: 深色
  - light: 浅色
- TextColor: ""
  $name: Text color
  $name:zh-CN: 文字颜色
  $description: Format "#RRGGBB"; empty follows the theme.
  $description:zh-CN: 格式为“#RRGGBB”，留空时跟随主题。
- BackColor: ""
  $name: Background color
  $name:zh-CN: 背景颜色
  $description: Format "#RRGGBB"; empty draws no background.
  $description:zh-CN: 格式为“#RRGGBB”，留空时不绘制背景。
- BackOpacity: 200
  $name: Background opacity
  $name:zh-CN: 背景不透明度
  $description: Range 0-255; requires a background color.
  $description:zh-CN: 范围 0-255，需先设置背景颜色。
- Align: left
  $name: Alignment
  $name:zh-CN: 行对齐
  $description: Alignment when the two rows differ in width.
  $description:zh-CN: 两行宽度不同时的对齐方式。
  $options:
  - left: Left
  - center: Center
  - right: Right
  $options:zh-CN:
  - left: 左对齐
  - center: 居中
  - right: 右对齐
- SeparateUnit: true
  $name: Space between value and unit
  $name:zh-CN: 数值与单位间空格
- Position: auto
  $name: Position
  $name:zh-CN: 位置
  $description: "Auto: prefers the left of the notification area; falls back to the left of the Start button."
  $description:zh-CN: 自动：优先置于通知区域左侧，空间不足时移至开始按钮左侧。
  $options:
  - auto: Auto
  - right: Right of taskbar (left of notification area)
  - left: Left of taskbar (left of Start button)
  - far-left: Far left of taskbar
  $options:zh-CN:
  - auto: 自动
  - right: 任务栏右侧（通知区域左侧）
  - left: 任务栏左侧（开始按钮左侧）
  - far-left: 任务栏最左侧
- OffsetX: 0
  $name: Horizontal offset
  $name:zh-CN: 水平偏移
  $description: Pixels; positive moves right.
  $description:zh-CN: 单位为像素，正值右移。
- OffsetY: 0
  $name: Vertical offset
  $name:zh-CN: 垂直偏移
  $description: Pixels; positive moves down.
  $description:zh-CN: 单位为像素，正值下移。
- RowGap: 0
  $name: Row gap (two rows)
  $name:zh-CN: 双排行间距
  $description: Gap between the two rows in pixels; may be negative.
  $description:zh-CN: 双排时上下两行的间距（像素），可为负值。
- UpdateInterval: 1000
  $name: Refresh interval (ms)
  $name:zh-CN: 刷新间隔（毫秒）
  $description: Minimum 200.
  $description:zh-CN: 最小值为 200。
- ShowTooltip: true
  $name: Show tooltip on hover
  $name:zh-CN: 悬停时显示提示
- SingleClick: none
  $name: Single-click action
  $name:zh-CN: 单击动作
  $description: A highlight appears on hover when an action is set.
  $description:zh-CN: 设置动作后，悬停时显示高亮。
  $options:
  - none: None
  - taskmgr: Open Task Manager
  - show-desktop: Show desktop
  $options:zh-CN:
  - none: 无
  - taskmgr: 打开任务管理器
  - show-desktop: 显示桌面
- MiddleClick: none
  $name: Middle-click action
  $name:zh-CN: 中键动作
  $options:
  - none: None
  - taskmgr: Open Task Manager
  - show-desktop: Show desktop
  $options:zh-CN:
  - none: 无
  - taskmgr: 打开任务管理器
  - show-desktop: 显示桌面
- RightClick: none
  $name: Right-click action
  $name:zh-CN: 右键动作
  $description: Right-click is handled by this mod; the taskbar menu does not appear.
  $description:zh-CN: 右键由本 mod 接管，任务栏菜单不再弹出。
  $options:
  - none: None
  - taskmgr: Open Task Manager
  - show-desktop: Show desktop
  $options:zh-CN:
  - none: 无
  - taskmgr: 打开任务管理器
  - show-desktop: 显示桌面
*/
// ==/WindhawkModSettings==
