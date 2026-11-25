# Windhawk Traffic Monitor

将 [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor) 的任务栏窗口移植为 Windhawk mod。

本 mod 为工具 mod，在 Windhawk 的独立进程中运行，不注入 `explorer.exe`，不 hook 系统函数。通过自有分层窗口在任务栏上显示网速、CPU、内存等数据；不安装独立程序，不添加开机启动项，不占用通知区域图标。

![效果](docs/taskbar.png)

双排效果：

![双排](docs/two-rows.png)

## 功能

- 显示上传速率、下载速率、总速率（取当前流量最大的网卡）、CPU、内存、已用内存、总内存、GPU 与磁盘活动
- 显示内容由格式串定义，`${up}` 等占位符替换为数值，其余字符原样输出
- 支持单排与双排布局，双排的上、下两行分别配置
- 支持字体、字号、字重、文字颜色、背景色与不透明度、行对齐，修改后立即生效
- 跟随系统深色 / 浅色主题
- 悬停显示详细提示；单击、中键、右键可分别绑定动作（打开任务管理器、显示桌面）
- 中英文双语，设置面板与鼠标提示按系统 UI 语言切换（英文默认，含简体中文本地化）
- 未设置背景时采用分层窗口逐像素 alpha 合成，文字直接叠加于任务栏
- 文字使用 DirectWrite 灰度抗锯齿渲染（256 级覆盖率），并对覆盖率进行 S 形锐化以收窄过渡带；DirectWrite 不可用时回退 GDI

## 格式串

以 `${名称}` 插入数值，其余字符原样显示。未识别的名称会原样保留，便于发现拼写错误。

| 占位符        | 含义                                   |
| ------------- | -------------------------------------- |
| `${up}`       | 上传速率                               |
| `${down}`     | 下载速率                               |
| `${total}`    | 总速率（上传与下载之和）               |
| `${cpu}`      | CPU 使用率                             |
| `${mem}`      | 内存，按“内存显示方式”输出百分比或容量 |
| `${memused}`  | 已用内存容量                           |
| `${memtotal}` | 总内存容量                             |
| `${gpu}`      | GPU 使用率                             |
| `${disk}`     | 磁盘活动                               |

示例：

```text
单排：      ↑: ${up}    ↓: ${down}    CPU: ${cpu}
双排上行：  ↑: ${up}    ↓: ${down}
双排下行：  CPU: ${cpu}    内存: ${memused} / ${memtotal}
```

窗口宽度随实际文字宽度变化。

## 安装

1. 安装 [Windhawk](https://windhawk.net/)（1.5 及以上）。
2. 打开 Windhawk，进入 Mods，点击右上角 `⋯`，选择“创建新 Mod”，输入 id `windhawk-traffic-monitor`，按提示完成创建。
3. 删除编辑器中的模板内容，粘贴 `dist/windhawk-traffic-monitor.wh.cpp` 的全部内容（亦可从 Release 页面下载）。
4. 点击“编译 Mod”，编译无误后点击“安装”。

安装后任务栏应立即显示数据。若未显示，可在 Windhawk 中先禁用再启用本 mod。

## 设置

在 Windhawk 的 Mods 列表中打开本 mod 的“设置”。

| 设置                              | 说明                                                                                                |
| --------------------------------- | --------------------------------------------------------------------------------------------------- |
| 排列方式 `Arrange`                | `horizontal` 单排（默认）；`vertical` 双排                                                           |
| 单排格式 `SingleRow`              | 单排时显示的整行文本，默认 `↑: ${up}    ↓: ${down}`                                                 |
| 上排格式 `TopRow`                 | 双排时的上面一行                                                                                    |
| 下排格式 `BottomRow`              | 双排时的下面一行                                                                                    |
| 行对齐 `Align`                    | `left` / `center` / `right`                                                                         |
| 字体 / 字号                       | 默认 `Microsoft YaHei UI`，字号单位为像素（默认 14），不随 DPI 缩放                                 |
| 字重 `FontWeight`                 | 300 / 400 / 500 / 600 / 700 / 900；字体需提供对应字面，细体会自动改用 `Microsoft YaHei UI Light` 字族 |
| 主题                              | `auto` 跟随系统，或强制 `dark` / `light`                                                            |
| 文字颜色                          | `#RRGGBB`，留空则按主题自动取色                                                                     |
| 背景颜色 / 背景不透明度           | 留空表示不绘制背景；如需底色可填 `#202020` 等                                                       |
| 网速单位 / 数值与单位间加空格     | `auto` / `kb` / `mb` / `auto-bits` / `kb-bits` / `mb-bits`；比特单位按 ×8 换算                       |
| 隐藏单位                          | 作用于所有数值：`KB/s`、`GB`、`%` 均不显示，仅保留数字                                              |
| 内存显示方式                      | 已用百分比 / 已用容量 / 可用容量（影响 `${mem}`）                                                   |
| 位置                              | `auto` 优先贴通知区域左侧；可强制 `right` / `left` / `far-left`（任务栏最左侧）                      |
| 水平 / 垂直偏移                   | 像素，正数向右 / 向下                                                                               |
| 双排上下留白 `RowGap`             | 双排两行之间的留白（像素），正数拉开、负数拉近                                                       |
| 刷新间隔                          | 毫秒，最小 200                                                                                      |
| 显示鼠标提示 / 单击·中键·右键动作 | 三个按键分别绑定 `none` / `taskmgr`（打开任务管理器）/ `show-desktop`（显示桌面）                   |

## 与 TrafficMonitor 的差异

本 mod 仅实现任务栏窗口，未包含以下功能：

- 桌面悬浮窗口、皮肤、字体阴影、透明色键
- 占用条与占用图
- 插件系统、流量历史统计、今日流量
- 网络连接选择：固定统计当前流量最大的网卡，虚拟网卡与 Hyper-V 不会重复计数
- 硬件温度（依赖 OpenHardwareMonitor DLL）、CPU 频率

采集与实现方式：

- 网速：`GetIfTable`，32 位计数器按模取增量以处理回绕；多网卡时取本次增量最大者
- CPU：优先 PDH `\Processor Information(_Total)\% Processor Utility`，失败时回退 `GetSystemTimes`
- 内存：`GlobalMemoryStatusEx`
- GPU：PDH `\GPU Engine(*)\Utilization Percentage`，按引擎类型分组求和后取最大值
- 磁盘：PDH `\PhysicalDisk(_Total)\% Idle Time`，取 `100 - 空闲`
- 数值格式化规则对照 `Common.cpp` 的 `DataSizeToString` 移植
- 窗口高度与排布对照 `CTaskBarDlg::CalculateWindowSize`
- 文字渲染改用 DirectWrite：GDI 灰度抗锯齿仅 16 级覆盖率，小字号下边缘存在明显台阶

## 已知限制

- 位置为估算值。Windows 11 任务栏图标由 XAML 绘制，系统未暴露图标区域右边界；`auto` 模式依据开始按钮位置估算，优先避免覆盖图标。需要固定位置时可设为 `right` 或 `left`。
- 仅处理主屏的 `Shell_TrayWnd`，副屏任务栏不显示。
- 仅支持左右方向的任务栏。
- 任务按钮较多且强制 `right` 时，窗口可能覆盖最右侧按钮。
- 字号上限为 32 像素，窗口高度不超过任务栏；字号过大时文字会被裁切。
- 文字无法达到与任务栏原生文字相同的锐利度：原生文字使用 ClearType 次像素渲染，需要已知背景色；本 mod 为透明分层窗口，仅能使用灰度抗锯齿。

## 开发

源码按功能拆分于 `src/`，由 `scripts/pack.py` 合并为 Windhawk 所需的单文件。编译使用 Windhawk 自带的 clang，无需安装 Visual Studio。

```bash
python scripts/build.py all       # 合并 + 编译校验 + 预览
python scripts/build.py pack      # 生成 dist/windhawk-traffic-monitor.wh.cpp
python scripts/build.py mod       # 直接编译 src/，做链接检查
python scripts/build.py preview   # 编译独立预览程序
python scripts/build.py clean     # 清理 .temp/
```

文件说明：

```text
src/mod.wh.cpp          合并入口
src/mod_meta.inc        元数据、Readme 与设置声明
src/preamble.h          系统与标准库头、日志宏
src/util.h              字符串与颜色工具
src/settings.h          设置结构与读取
src/format.h            数值格式化
src/snapshot.h          单次采样数据
src/net.h               网速采集
src/sysinfo.h           CPU / GPU / 磁盘采集
src/widget*.h           任务栏窗口（布局、定位、渲染、消息等）
src/textrender.h        DirectWrite 文字渲染
src/thread.h            任务栏窗口线程
src/mod_entry.h         Windhawk 入口
scripts/pack.py         分片合并
scripts/build.py        构建入口
scripts/shot.py         任务栏检查与截图工具
scripts/bmp.py          BMP 图像处理工具
```

`.temp/` 存放编译与调试产物，执行 `python scripts/build.py clean` 可清除。

## 许可

本 mod 为 [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor) 的衍生作品，以 GPL-3.0 发布。许可证全文见随附 `LICENSE` 文件，亦内嵌于发布产物的注释中，或参见 <https://www.gnu.org/licenses/gpl-3.0.html>。

- 本 mod 版权 © lo0kie
- 界面布局、数值格式化规则、网速与占用率采集逻辑参考并移植自 TrafficMonitor，版权 © zhongyang219
- README 中提及 `taskbar-clock-customization`（m417z）仅作实现对比，不涉及代码再分发

相对 TrafficMonitor 的主要修改：改为 Windhawk 工具 mod、仅保留任务栏窗口、文字渲染改用 DirectWrite、移除皮肤 / 插件 / 温度 / 流量统计等模块。
