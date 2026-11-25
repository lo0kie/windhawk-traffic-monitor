#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""任务栏窗口的观察工具：打印层级、截图、给控件发重载消息。

全部走 ctypes 调 user32 / gdi32，不需要编译任何东西：

    python scripts/shot.py scan                       # 任务栏子窗口层级 + 屏幕信息
    python scripts/shot.py capture out.bmp            # 整个虚拟屏幕
    python scripts/shot.py capture out.bmp --window Shell_TrayWnd
    python scripts/shot.py capture docs/taskbar.png --window Shell_TrayWnd \
        --crop 560 0 1140 72
    python scripts/shot.py reload                     # 给控件投递 WM_TM_RELOAD

截图默认用 PrintWindow(PW_RENDERFULLCONTENT) 让窗口自己画一遍，这样即使桌面被
置顶窗口盖住也能拿到任务栏的真实画面；窗口取不到时退回屏幕 BitBlt。
"""

from __future__ import print_function

import argparse
import ctypes
import os
import sys
from ctypes import wintypes

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bmp  # noqa: E402

user32 = ctypes.WinDLL('user32', use_last_error=True)
gdi32 = ctypes.WinDLL('gdi32', use_last_error=True)

WM_TM_RELOAD = 0x8000 + 1          # 和 src/widget_message.h 里的 WM_TM_RELOAD 一致
WIDGET_CLASS = 'WindhawkTrafficMonitor'
PW_RENDERFULLCONTENT = 0x00000002
GWL_STYLE = -16
WS_VISIBLE = 0x10000000
WS_CHILD = 0x40000000
WS_CLIPSIBLINGS = 0x04000000
MONITORINFOF_PRIMARY = 0x00000001
DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = ctypes.c_void_p(-4)


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [('biSize', wintypes.DWORD), ('biWidth', wintypes.LONG),
                ('biHeight', wintypes.LONG), ('biPlanes', wintypes.WORD),
                ('biBitCount', wintypes.WORD), ('biCompression', wintypes.DWORD),
                ('biSizeImage', wintypes.DWORD), ('biXPelsPerMeter', wintypes.LONG),
                ('biYPelsPerMeter', wintypes.LONG), ('biClrUsed', wintypes.DWORD),
                ('biClrImportant', wintypes.DWORD)]


class BITMAPINFO(ctypes.Structure):
    _fields_ = [('bmiHeader', BITMAPINFOHEADER), ('bmiColors', wintypes.DWORD * 3)]


class MONITORINFOEXW(ctypes.Structure):
    _fields_ = [('cbSize', wintypes.DWORD), ('rcMonitor', wintypes.RECT),
                ('rcWork', wintypes.RECT), ('dwFlags', wintypes.DWORD),
                ('szDevice', ctypes.c_wchar * 32)]


MONITORENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HMONITOR, wintypes.HDC,
                                     ctypes.POINTER(wintypes.RECT), wintypes.LPARAM)

user32.FindWindowW.argtypes = (wintypes.LPCWSTR, wintypes.LPCWSTR)
user32.FindWindowW.restype = wintypes.HWND
user32.FindWindowExW.argtypes = (wintypes.HWND, wintypes.HWND, wintypes.LPCWSTR,
                                 wintypes.LPCWSTR)
user32.FindWindowExW.restype = wintypes.HWND
user32.GetClassNameW.argtypes = (wintypes.HWND, wintypes.LPWSTR, ctypes.c_int)
user32.GetWindowTextW.argtypes = (wintypes.HWND, wintypes.LPWSTR, ctypes.c_int)
user32.GetWindowRect.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.RECT))
user32.GetWindowRect.restype = wintypes.BOOL
user32.GetWindowLongPtrW.argtypes = (wintypes.HWND, ctypes.c_int)
user32.GetWindowLongPtrW.restype = ctypes.c_ssize_t
user32.GetWindowThreadProcessId.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.DWORD))
user32.GetWindowThreadProcessId.restype = wintypes.DWORD
user32.GetDpiForWindow.argtypes = (wintypes.HWND,)
user32.GetDpiForWindow.restype = wintypes.UINT
user32.PrintWindow.argtypes = (wintypes.HWND, wintypes.HDC, wintypes.UINT)
user32.PrintWindow.restype = wintypes.BOOL
user32.GetDC.argtypes = (wintypes.HWND,)
user32.GetDC.restype = wintypes.HDC
user32.ReleaseDC.argtypes = (wintypes.HWND, wintypes.HDC)
user32.PostMessageW.argtypes = (wintypes.HWND, wintypes.UINT, wintypes.WPARAM,
                                wintypes.LPARAM)
user32.PostMessageW.restype = wintypes.BOOL
user32.EnumDisplayMonitors.argtypes = (wintypes.HDC, ctypes.POINTER(wintypes.RECT),
                                       MONITORENUMPROC, wintypes.LPARAM)
user32.GetSystemMetrics.argtypes = (ctypes.c_int,)
user32.GetSystemMetrics.restype = ctypes.c_int
user32.GetMonitorInfoW.argtypes = (wintypes.HMONITOR, ctypes.POINTER(MONITORINFOEXW))
user32.GetMonitorInfoW.restype = wintypes.BOOL

gdi32.CreateCompatibleDC.argtypes = (wintypes.HDC,)
gdi32.CreateCompatibleDC.restype = wintypes.HDC
gdi32.CreateDIBSection.argtypes = (wintypes.HDC, ctypes.POINTER(BITMAPINFO),
                                   wintypes.UINT, ctypes.POINTER(ctypes.c_void_p),
                                   wintypes.HANDLE, wintypes.DWORD)
gdi32.CreateDIBSection.restype = wintypes.HBITMAP
gdi32.SelectObject.argtypes = (wintypes.HDC, wintypes.HGDIOBJ)
gdi32.SelectObject.restype = wintypes.HGDIOBJ
gdi32.BitBlt.argtypes = (wintypes.HDC, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                         ctypes.c_int, wintypes.HDC, ctypes.c_int, ctypes.c_int,
                         wintypes.DWORD)
gdi32.BitBlt.restype = wintypes.BOOL
gdi32.DeleteDC.argtypes = (wintypes.HDC,)
gdi32.DeleteObject.argtypes = (wintypes.HGDIOBJ,)


def set_dpi_aware():
    try:
        user32.SetProcessDpiAwarenessContext(
            DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)
    except AttributeError:
        pass


def class_of(hwnd):
    buf = ctypes.create_unicode_buffer(256)
    user32.GetClassNameW(hwnd, buf, 256)
    return buf.value


def text_of(hwnd):
    buf = ctypes.create_unicode_buffer(256)
    user32.GetWindowTextW(hwnd, buf, 256)
    return buf.value


def rect_of(hwnd):
    rc = wintypes.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rc))
    return rc


def pid_of(hwnd):
    pid = wintypes.DWORD()
    user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
    return pid.value


def find_widgets():
    """控件挂在 Shell_TrayWnd 下面，所以从任务栏按类名枚举子窗口。

    已安装的 mod 和自己起的预览程序会同时存在两个窗口，调用方可以用 PID 区分。
    """
    found = []
    taskbar = user32.FindWindowW('Shell_TrayWnd', None)
    if taskbar:
        child = None
        while True:
            child = user32.FindWindowExW(taskbar, child, WIDGET_CLASS, None)
            if not child:
                break
            found.append(child)
    if not found:
        top = user32.FindWindowW(WIDGET_CLASS, None)
        if top:
            found.append(top)
    return found


def pick_widgets(pid):
    widgets = find_widgets()
    if pid:
        widgets = [w for w in widgets if pid_of(w) == pid]
    return widgets


def describe(hwnd):
    rc = rect_of(hwnd)
    return '0x%X pid=%-6d %dx%d @(%d,%d)' % (hwnd or 0, pid_of(hwnd),
                                             rc.right - rc.left, rc.bottom - rc.top,
                                             rc.left, rc.top)


# ---------------------------------------------------------------------------
# scan
# ---------------------------------------------------------------------------
def dump_children(parent, depth, max_depth=4):
    child = None
    while True:
        child = user32.FindWindowExW(parent, child, None, None)
        if not child:
            break
        rc, prc = rect_of(child), rect_of(parent)
        style = user32.GetWindowLongPtrW(child, GWL_STYLE) & 0xFFFFFFFF
        pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(child, ctypes.byref(pid))
        title = text_of(child)
        print('%s|- %-48s pid=%-6d rel=(%4d,%3d %4dx%3d) style=0x%08X %s%s %s%s' % (
            '  ' * depth, class_of(child), pid.value,
            rc.left - prc.left, rc.top - prc.top, rc.right - rc.left, rc.bottom - rc.top,
            style,
            'VISIBLE' if style & WS_VISIBLE else 'hidden ',
            ' child' if style & WS_CHILD else ' popup',
            'clipSib' if style & WS_CLIPSIBLINGS else '',
            title))
        if depth < max_depth:
            dump_children(child, depth + 1, max_depth)


def cmd_scan(args):
    print('=== 顶层任务栏窗口 ===')
    for name in ('Shell_TrayWnd', 'Shell_SecondaryTrayWnd'):
        hwnd = None
        while True:
            hwnd = user32.FindWindowExW(None, hwnd, name, None)
            if not hwnd:
                break
            rc = rect_of(hwnd)
            print('%s hwnd=0x%X rect=(%d,%d %dx%d) dpi=%d' % (
                name, hwnd or 0, rc.left, rc.top, rc.right - rc.left,
                rc.bottom - rc.top, user32.GetDpiForWindow(hwnd)))
            dump_children(hwnd, 1)

    print('\n=== 屏幕 ===')

    def on_monitor(hmon, hdc, lprc, data):
        mi = MONITORINFOEXW()
        mi.cbSize = ctypes.sizeof(MONITORINFOEXW)
        user32.GetMonitorInfoW(hmon, ctypes.byref(mi))
        rc = mi.rcMonitor
        print('%s primary=%d rc=(%d,%d %dx%d)' % (
            mi.szDevice, 1 if mi.dwFlags & MONITORINFOF_PRIMARY else 0,
            rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top))
        return True

    user32.EnumDisplayMonitors(None, None, MONITORENUMPROC(on_monitor), 0)


# ---------------------------------------------------------------------------
# capture
# ---------------------------------------------------------------------------
def grab(hwnd, rect, use_print=True):
    x, y, width, height = rect
    screen = user32.GetDC(None)
    mem = gdi32.CreateCompatibleDC(screen)
    info = BITMAPINFO()
    info.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    info.bmiHeader.biWidth = width
    info.bmiHeader.biHeight = -height          # 负值 = 自上而下，省得翻转
    info.bmiHeader.biPlanes = 1
    info.bmiHeader.biBitCount = 32
    info.bmiHeader.biCompression = 0           # BI_RGB

    bits = ctypes.c_void_p()
    bitmap = gdi32.CreateDIBSection(mem, ctypes.byref(info), 0, ctypes.byref(bits),
                                    None, 0)
    old = gdi32.SelectObject(mem, bitmap)

    # 控件是分层窗口，不吃 WM_PRINT，PrintWindow 会返回成功却画出全空，
    # 所以只在打印出真实内容时才算数，否则退回屏幕 BitBlt。
    printed = False
    raw = None
    if hwnd and use_print and user32.PrintWindow(hwnd, mem, PW_RENDERFULLCONTENT):
        gdi32.GdiFlush()
        raw = ctypes.string_at(bits, width * height * 4)
        if width * height and raw.count(raw[:4]) != width * height:
            printed = True

    if not printed:
        gdi32.BitBlt(mem, 0, 0, width, height, screen, x, y, 0x00CC0020)  # SRCCOPY
        gdi32.GdiFlush()
        raw = ctypes.string_at(bits, width * height * 4)

    rows = []
    for row in range(height):
        line = bytearray(raw[row * width * 4:(row + 1) * width * 4])
        line[0::4], line[2::4] = line[2::4], line[0::4]      # BGRA -> RGBA
        rows.append(line)

    gdi32.SelectObject(mem, old)
    gdi32.DeleteObject(bitmap)
    gdi32.DeleteDC(mem)
    user32.ReleaseDC(None, screen)
    return rows, printed


def cmd_capture(args):
    hwnd = None
    if args.widget:
        widgets = pick_widgets(args.pid)
        if not widgets:
            print('未找到控件窗口', file=sys.stderr)
            return 1
        if len(widgets) > 1:
            print('有 %d 个控件窗口，用 --pid 指定一个：' % len(widgets), file=sys.stderr)
            for w in widgets:
                print('  ' + describe(w), file=sys.stderr)
            return 1
        hwnd = widgets[0]
        print('控件 %s' % describe(hwnd))
    elif args.window:
        hwnd = user32.FindWindowW(args.window, None)
        if not hwnd:
            print('找不到窗口类 %s' % args.window, file=sys.stderr)
            return 1

    if hwnd:
        rc = rect_of(hwnd)
        rect = (rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top)
    elif args.rect:
        rect = tuple(args.rect)
    else:
        rect = (0, 0, user32.GetSystemMetrics(78), user32.GetSystemMetrics(79))

    width, height = rect[2], rect[3]
    rows, printed = grab(hwnd, rect, use_print=not args.widget)
    source = 'PrintWindow' if printed else 'BitBlt'

    if args.crop:
        rows = bmp.crop(width, height, rows, tuple(args.crop))
        width, height = args.crop[2], args.crop[3]
    if args.scale > 1:
        width, height, rows = bmp.scale_rows(width, height, rows, args.scale)

    bmp.save(args.out, width, height, bmp.flatten(rows))
    print('%s 取图 rect=(%d,%d %dx%d) -> %s (%dx%d)' % (
        source, rect[0], rect[1], rect[2], rect[3], args.out, width, height))
    return 0


# ---------------------------------------------------------------------------
# reload
# ---------------------------------------------------------------------------
def cmd_reload(args):
    widgets = pick_widgets(args.pid)
    if not widgets:
        print('未找到控件窗口（mod 没装，或者预览程序没在跑）', file=sys.stderr)
        return 1
    for hwnd in widgets:
        print('通知 %s' % describe(hwnd))
        user32.PostMessageW(hwnd, WM_TM_RELOAD, 0, 0)
    return 0


def main():
    set_dpi_aware()

    parser = argparse.ArgumentParser(description='任务栏窗口观察工具')
    sub = parser.add_subparsers(dest='cmd')

    p = sub.add_parser('scan', help='打印任务栏子窗口层级与屏幕信息')
    p.set_defaults(func=cmd_scan)

    p = sub.add_parser('capture', help='截图（窗口 / 控件 / 区域 / 全屏）')
    p.add_argument('out', help='输出文件，扩展名决定 bmp 还是 png')
    p.add_argument('--window', help='窗口类名，例如 Shell_TrayWnd')
    p.add_argument('--widget', action='store_true', help='截本 mod 的控件窗口')
    p.add_argument('--pid', type=int, help='同时有多个控件窗口时按 PID 选')
    p.add_argument('--rect', type=int, nargs=4, metavar=('X', 'Y', 'W', 'H'))
    p.add_argument('--crop', type=int, nargs=4, metavar=('X', 'Y', 'W', 'H'),
                   help='在取到的画面里再裁一块')
    p.add_argument('--scale', type=int, default=1, help='整数倍放大')
    p.set_defaults(func=cmd_capture)

    p = sub.add_parser('reload', help='给控件投递 WM_TM_RELOAD')
    p.add_argument('--pid', type=int, help='同时有多个控件窗口时按 PID 选')
    p.set_defaults(func=cmd_reload)

    args = parser.parse_args()
    if not getattr(args, 'func', None):
        parser.print_help()
        return 1
    return args.func(args)


if __name__ == '__main__':
    sys.exit(main())
