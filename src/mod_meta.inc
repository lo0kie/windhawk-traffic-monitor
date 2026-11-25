// ==WindhawkMod==
// @id              windhawk-traffic-monitor
// @name            Windhawk Traffic Monitor
// @name:zh-CN      任务栏网速监控
// @description     TrafficMonitor's taskbar window as a Windhawk mod: shows network speed / CPU / memory on the taskbar via a format string, single or double row
// @description:zh-CN TrafficMonitor 的任务栏窗口：用格式串在任务栏上显示网速 / CPU / 内存等，支持单排和双排
// @version         1.0.0
// @author          lo0kie
// @github          lo0kie
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

## Features

- Upload / download / total network speed (the most active adapter), CPU, memory, GPU, disk activity
- Display is fully customizable via a **format string**, one row or two rows (top / bottom)
- Font, size, weight, text color, background color / opacity, alignment; changes apply instantly
- Text drawn with DirectWrite grayscale antialiasing (8-bit coverage), crisp at small sizes
- Follows the system dark / light theme
- Hover for detailed info, double-click to open Task Manager
- Follows the taskbar: hides when the taskbar auto-hides or goes fullscreen

## Format string

Insert values with `${name}`, everything else is printed verbatim:

| Placeholder | Meaning |
| --- | --- |
| `${up}` | Upload speed |
| `${down}` | Download speed |
| `${total}` | Total speed (up + down) |
| `${cpu}` | CPU usage |
| `${mem}` | Memory (per the "Memory display" setting) |
| `${memused}` | Used memory |
| `${memtotal}` | Total memory |
| `${gpu}` | GPU usage |
| `${disk}` | Disk activity |

Example: `↑: ${up}    ↓: ${down}    CPU: ${cpu}`.
The window width reserves space for the largest possible value, so it does not jitter.

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
- SingleRow: "↑: ${up}    ↓: ${down}"
  $name: Single-row format
  $name:zh-CN: 单排格式
  $description: >-
    The full line shown in single-row mode. Insert values with ${name}, other characters
    are printed verbatim. Available: ${up} upload, ${down} download, ${total} total speed,
    ${cpu} CPU, ${mem} memory, ${memused} used memory, ${memtotal} total memory, ${gpu} GPU,
    ${disk} disk activity. Example: "↑: ${up}   ↓: ${down}   CPU: ${cpu}"
  $description:zh-CN: >-
    单排时显示的整行文本。用 ${名字} 插入数值，其它字符原样显示。
    可用：${up} 上传速度、${down} 下载速度、${total} 总网速、${cpu} CPU 使用率、
    ${mem} 内存、${memused} 已用内存、${memtotal} 总内存、${gpu} GPU、${disk} 磁盘活动。
    例："↑: ${up}   ↓: ${down}   CPU: ${cpu}"
- Arrange: horizontal
  $name: Arrangement
  $name:zh-CN: 排列方式
  $description: >-
    Single row: only the "Single-row format" line. Two rows: top / bottom each show one
    line, narrower width, use this when the taskbar is crowded.
  $description:zh-CN: >-
    单排：只显示“单排格式”这一行。双排：上排 / 下排各显示一行，宽度更窄，
    任务栏比较挤的时候用这个。
  $options:
  - horizontal: Single row
  - vertical: Two rows
  $options:zh-CN:
  - horizontal: 单排
  - vertical: 双排
- TopRow: "↑: ${up}"
  $name: Top-row format (two rows)
  $name:zh-CN: 上排格式（双排）
  $description: The top line in two-row mode, same syntax as the single-row format.
  $description:zh-CN: 双排时上面一行，语法同“单排格式”。
- BottomRow: "↓: ${down}"
  $name: Bottom-row format (two rows)
  $name:zh-CN: 下排格式（双排）
  $description: The bottom line in two-row mode, same syntax as the single-row format.
  $description:zh-CN: 双排时下面一行，语法同“单排格式”。
- FontSize: 14
  $name: Font size (pixels)
  $name:zh-CN: 字号（像素）
  $description: Text height in pixels.
  $description:zh-CN: 文字高度，单位是像素。
- FontFace: Microsoft YaHei UI
  $name: Font
  $name:zh-CN: 字体
  $description: Font family name used for the taskbar text.
  $description:zh-CN: 任务栏文字使用的字体名称。
- FontWeight: normal
  $name: Font weight
  $name:zh-CN: 字重
  $description: >-
    Only visible if the font ships the matching face (Microsoft YaHei UI only has light /
    regular / bold); missing weights fall back to the closest available one.
  $description:zh-CN: >-
    字体本身有对应字面才看得出区别（Microsoft YaHei UI 只有细体 / 常规 / 粗体），
    没有的字重会由系统挑最接近的一个。
  $options:
  - light: Light (300)
  - normal: Regular (400)
  - medium: Medium (500)
  - semibold: Semibold (600)
  - bold: Bold (700)
  - black: Black (900)
  $options:zh-CN:
  - light: 细体（300）
  - normal: 常规（400）
  - medium: 中等（500）
  - semibold: 半粗（600）
  - bold: 粗体（700）
  - black: 特粗（900）
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
  $description: "All values show numbers only: KB/s, GB, % are all hidden."
  $description:zh-CN: 所有数值都只留数字：KB/s、GB、% 全部不显示。
- MemoryDisplay: percent
  $name: Memory display
  $name:zh-CN: 内存显示方式
  $options:
  - percent: Used percentage
  - used: Used capacity
  - available: Available capacity
  $options:zh-CN:
  - percent: 已用百分比
  - used: 已用容量
  - available: 可用容量
- Theme: auto
  $name: Theme
  $name:zh-CN: 主题
  $description: Auto follows the system dark / light mode for the default text color.
  $description:zh-CN: 自动时跟随系统深色 / 浅色模式决定默认文字颜色。
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
  $description: Like "#FFFFFF"; empty uses the theme default (white on dark, black on light).
  $description:zh-CN: 形如 "#FFFFFF"，留空则按主题自动（深色白色、浅色黑色）。
- BackColor: ""
  $name: Background color
  $name:zh-CN: 背景颜色
  $description: Like "#000000"; empty draws no background (fully transparent, text over the taskbar).
  $description:zh-CN: 形如 "#000000"，留空则不画背景（完全透明，文字直接压在任务栏上）。
- BackOpacity: 200
  $name: Background opacity
  $name:zh-CN: 背景不透明度
  $description: 0-255, only applies when a background color is set.
  $description:zh-CN: 0-255，仅在设置了背景颜色时生效。
- Align: left
  $name: Alignment
  $name:zh-CN: 行对齐
  $description: How text aligns within the window when the two rows differ in width.
  $description:zh-CN: 两行宽度不一样时，文字在窗口内的对齐方式。
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
  $name:zh-CN: 数值和单位之间加空格
- Position: auto
  $name: Position
  $name:zh-CN: 位置
  $description: "Auto: prefers the left of the notification area, moves to the left of the Start button when space is tight."
  $description:zh-CN: 自动：优先贴在通知区域左边，空间不够时移到开始按钮左边，避免盖住任务按钮。
  $options:
  - auto: Auto
  - right: Right of taskbar (left of notification area)
  - left: Left of taskbar (left of Start button)
  - far-left: Far left of taskbar
  $options:zh-CN:
  - auto: 自动
  - right: 任务栏右侧（通知区域左边）
  - left: 任务栏左侧（开始按钮左边）
  - far-left: 状态栏最左侧
- OffsetX: 0
  $name: Horizontal offset
  $name:zh-CN: 水平偏移
  $description: Pixels, positive moves right.
  $description:zh-CN: 像素，正数向右。
- OffsetY: 0
  $name: Vertical offset
  $name:zh-CN: 垂直偏移
  $description: Pixels, positive moves down.
  $description:zh-CN: 像素，正数向下。
- RowGap: 0
  $name: Gap between the two rows
  $name:zh-CN: 双排上下留白
  $description: Gap between the two rows (pixels); positive spreads them, negative pulls them closer (can be negative).
  $description:zh-CN: 双排时上下两行之间的留白（像素），正数拉开、负数拉近，可为负。
- UpdateInterval: 1000
  $name: Refresh interval (ms)
  $name:zh-CN: 刷新间隔（毫秒）
  $description: Minimum 200.
  $description:zh-CN: 最小 200。
- ShowTooltip: true
  $name: Show tooltip on hover
  $name:zh-CN: 显示鼠标提示
- SingleClick: none
  $name: Single-click action
  $name:zh-CN: 单击动作
  $description: Hovering shows a highlight when a mouse button action is set.
  $description:zh-CN: 设置鼠标按键动作后，鼠标悬停会显示高亮反馈。
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
  $description: Right-click no longer opens the taskbar's own menu.
  $description:zh-CN: 右键不再弹出任务栏自身的菜单。
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
